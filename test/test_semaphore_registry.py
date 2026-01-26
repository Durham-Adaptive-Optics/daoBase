#!/usr/bin/env python3
"""
Test script for semaphore registry functionality in daoShm.py

This tests the Python wrapper for the new semaphore registry system
that prevents race conditions between multiple readers.
"""

import sys
import os
import unittest
import time
import multiprocessing as mp
from pathlib import Path

# Add the source directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src', 'python'))
from daoShm import shm

class TestSemaphoreRegistry(unittest.TestCase):
    """Test cases for semaphore registry functionality"""
    
    def setUp(self):
        """Create a unique shared memory for each test"""
        test_name = self.id().split('.')[-1]
        # Use shorter names for macOS semaphore name limits (max 31 chars with suffixes)
        short_name = test_name.replace('test_', '').replace('_', '')[:8]
        self.shm_name = f"/tmp/r{short_name}.im.shm"
        
        # Clean up any existing file
        if os.path.exists(self.shm_name):
            os.remove(self.shm_name)
    
    def tearDown(self):
        """Clean up shared memory file"""
        if os.path.exists(self.shm_name):
            os.remove(self.shm_name)
    
    def test_single_reader_registration(self):
        """Test single reader can register and unregister"""
        # Create writer
        import numpy as np
        data = np.zeros((10, 10), dtype=np.float32)
        writer = shm(self.shm_name, data, False)
        
        # Create reader
        reader = shm(self.shm_name)
        
        # Register with custom name
        sem_index = reader.register_reader("test_reader", -1)
        self.assertGreaterEqual(sem_index, 0)
        self.assertLess(sem_index, 10)  # IMAGE_NB_SEMAPHORE
        
        # Validate registration
        self.assertTrue(reader.validate_registration())
        
        # Check registry
        registry = writer.get_registered_readers()
        self.assertEqual(len(registry), 1)
        self.assertEqual(registry[0]['reader_name'], 'test_reader')
        
        # Unregister
        result = reader.unregister_reader()
        self.assertEqual(result, 0)
        self.assertFalse(reader.validate_registration())
        
        # Registry should be empty
        registry = writer.get_registered_readers()
        self.assertEqual(len(registry), 0)
        
        writer.close()
        reader.close()
    
    def test_multiple_readers(self):
        """Test multiple readers get different semaphores"""
        # Create writer
        import numpy as np
        data = np.zeros((10, 10), dtype=np.float32)
        writer = shm(self.shm_name, data, False)
        
        # Create 5 readers
        readers = []
        semaphores = []
        
        for i in range(5):
            reader = shm(self.shm_name)
            sem = reader.register_reader(f"reader_{i}", -1)
            self.assertGreaterEqual(sem, 0)
            semaphores.append(sem)
            readers.append(reader)
        
        # All semaphores should be unique
        self.assertEqual(len(set(semaphores)), 5)
        
        # Check registry
        registry = writer.get_registered_readers()
        self.assertEqual(len(registry), 5)
        
        # All should validate
        for reader in readers:
            self.assertTrue(reader.validate_registration())
        
        # Unregister all
        for reader in readers:
            reader.unregister_reader()
            reader.close()
        
        registry = writer.get_registered_readers()
        self.assertEqual(len(registry), 0)
        
        writer.close()
    
    def test_semaphore_exhaustion(self):
        """Test that registration fails when all 10 slots are used"""
        # Create writer
        import numpy as np
        data = np.zeros((10, 10), dtype=np.float32)
        writer = shm(self.shm_name, data, False)
        
        readers = []
        
        # Register 10 readers (max)
        for i in range(10):
            reader = shm(self.shm_name)
            sem = reader.register_reader(f"reader_{i}", -1)
            self.assertGreaterEqual(sem, 0)
            readers.append(reader)
        
        # Try to register 11th reader - should fail
        extra_reader = shm(self.shm_name)
        with self.assertRaises(RuntimeError) as context:
            extra_reader.register_reader("extra", -1)
        self.assertIn("No semaphores available", str(context.exception))
        
        # Unregister one reader
        readers[5].unregister_reader()
        
        # Now extra reader should succeed
        sem = extra_reader.register_reader("extra", -1)
        self.assertGreaterEqual(sem, 0)
        
        # Clean up
        for reader in readers:
            reader.unregister_reader()
            reader.close()
        extra_reader.unregister_reader()
        extra_reader.close()
        writer.close()
    
    def test_preferred_semaphore(self):
        """Test requesting a specific semaphore index"""
        import numpy as np
        data = np.zeros((10, 10), dtype=np.float32)
        writer = shm(self.shm_name, data, False)
        
        reader1 = shm(self.shm_name)
        sem1 = reader1.register_reader("reader1", 5)
        self.assertEqual(sem1, 5)
        
        # Try to get same semaphore - should get different one
        reader2 = shm(self.shm_name)
        sem2 = reader2.register_reader("reader2", 5)
        self.assertNotEqual(sem2, 5)
        
        reader1.unregister_reader()
        reader1.close()
        reader2.unregister_reader()
        reader2.close()
        writer.close()
    
    def test_force_unlock(self):
        """Test force unlocking a semaphore"""
        import numpy as np
        data = np.zeros((10, 10), dtype=np.float32)
        writer = shm(self.shm_name, data, False)
        
        reader = shm(self.shm_name)
        sem = reader.register_reader("test_reader", -1)
        self.assertGreaterEqual(sem, 0)
        
        # Force unlock from writer
        result = writer.force_unlock_semaphore(sem)
        self.assertEqual(result, 0)
        
        # Reader validation should now fail
        self.assertFalse(reader.validate_registration())
        
        # Test invalid semaphore number
        with self.assertRaises(ValueError):
            writer.force_unlock_semaphore(-1)
        with self.assertRaises(ValueError):
            writer.force_unlock_semaphore(10)
        
        reader.close()
        writer.close()
    
    def test_cleanup_stale_semaphores(self):
        """Test cleanup of stale entries"""
        import numpy as np
        data = np.zeros((10, 10), dtype=np.float32)
        writer = shm(self.shm_name, data, False)
        
        # Register and then close without unregistering
        reader = shm(self.shm_name)
        sem = reader.register_reader("temp_reader", -1)
        self.assertGreaterEqual(sem, 0)
        
        # Force unlock to simulate stale entry
        writer.force_unlock_semaphore(sem)
        
        # Note: Can't easily test actual process death without multiprocessing
        # But we can test the API exists and doesn't crash
        cleaned = writer.cleanup_stale_semaphores()
        self.assertGreaterEqual(cleaned, 0)
        
        reader.close()
        writer.close()
    
    def test_registry_info(self):
        """Test retrieving detailed registry information"""
        import numpy as np
        data = np.zeros((10, 10), dtype=np.float32)
        writer = shm(self.shm_name, data, False)
        
        names = ['alice', 'bob', 'charlie']
        readers = []
        
        for name in names:
            reader = shm(self.shm_name)
            reader.register_reader(name, -1)
            readers.append(reader)
        
        # Get registry
        registry = writer.get_registered_readers()
        self.assertEqual(len(registry), 3)
        
        # Check all names are present
        registry_names = [r['reader_name'] for r in registry]
        for name in names:
            self.assertIn(name, registry_names)
        
        # Check fields exist
        for entry in registry:
            self.assertIn('index', entry)
            self.assertIn('locked', entry)
            self.assertIn('reader_pid', entry)
            self.assertIn('reader_name', entry)
            self.assertIn('last_read_cnt0', entry)
            self.assertIn('last_read_time', entry)
            self.assertIn('read_count', entry)
            self.assertIn('timeout_count', entry)
            
            self.assertTrue(entry['locked'])
            self.assertGreater(entry['reader_pid'], 0)
        
        # Clean up
        for reader in readers:
            reader.unregister_reader()
            reader.close()
        writer.close()
    
    def test_auto_unregister_on_delete(self):
        """Test that __del__ automatically unregisters"""
        import numpy as np
        data = np.zeros((10, 10), dtype=np.float32)
        writer = shm(self.shm_name, data, False)
        
        # Create and register reader in a scope
        reader = shm(self.shm_name)
        reader.register_reader("temp", -1)
        registry = writer.get_registered_readers()
        self.assertEqual(len(registry), 1)
        
        # Delete reader - should auto-unregister via __del__
        del reader
        
        # Force garbage collection to trigger __del__
        import gc
        gc.collect()
        time.sleep(0.1)
        
        # Registry should be empty
        registry = writer.get_registered_readers()
        self.assertEqual(len(registry), 0)
        
        writer.close()


def reader_process(shm_name, reader_id, result_queue):
    """Helper function for multiprocess tests"""
    try:
        reader = shm(shm_name)
        sem = reader.register_reader(f"process_{reader_id}", -1)
        
        # Simulate reading
        time.sleep(0.1)
        
        is_valid = reader.validate_registration()
        
        reader.unregister_reader()
        reader.close()
        
        result_queue.put((reader_id, sem, is_valid))
    except Exception as e:
        result_queue.put((reader_id, -1, False, str(e)))


class TestMultiProcessRegistry(unittest.TestCase):
    """Test cases using multiple processes"""
    
    def setUp(self):
        """Create a unique shared memory for each test"""
        test_name = self.id().split('.')[-1]
        # Use shorter names for macOS semaphore name limits (max 31 chars with suffixes)
        short_name = test_name.replace('test_', '').replace('_', '')[:8]
        self.shm_name = f"/tmp/m{short_name}.im.shm"
        
        if os.path.exists(self.shm_name):
            os.remove(self.shm_name)
    
    def tearDown(self):
        """Clean up shared memory file"""
        if os.path.exists(self.shm_name):
            os.remove(self.shm_name)
    
    def test_multiprocess_readers(self):
        """Test multiple processes registering as readers"""
        # Create writer in main process
        import numpy as np
        data = np.zeros((10, 10), dtype=np.float32)
        writer = shm(self.shm_name, data, False)
        
        # Spawn multiple reader processes
        num_readers = 5
        result_queue = mp.Queue()
        processes = []
        
        for i in range(num_readers):
            p = mp.Process(target=reader_process, args=(self.shm_name, i, result_queue))
            p.start()
            processes.append(p)
        
        # Wait for all processes
        for p in processes:
            p.join(timeout=5)
        
        # Collect results
        results = []
        while not result_queue.empty():
            results.append(result_queue.get())
        
        self.assertEqual(len(results), num_readers)
        
        # Check all got unique semaphores
        semaphores = [r[1] for r in results]
        self.assertEqual(len(set(semaphores)), num_readers)
        
        # All should have been valid
        for result in results:
            self.assertTrue(result[2], f"Reader {result[0]} validation failed")
        
        writer.close()


if __name__ == '__main__':
    # Run tests
    unittest.main(verbosity=2)
