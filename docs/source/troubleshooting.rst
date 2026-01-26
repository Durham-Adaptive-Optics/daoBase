Troubleshooting 
==================

This section covers common issues and their solutions when working with the DAO library.

Threading Issues
----------------

Thread Hangs on Join()
~~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: Your application hangs when calling ``thread.Join()``, requiring a force quit.

**Cause**: Prior to version 0.0.2, the thread's outer loop only checked ``m_running`` flag, causing the thread to loop back to wait on a condition variable even after ``Stop()`` was called.

**Solution**: 

1. **Update to version 0.0.2 or later** - The threading system has been fixed to properly check both ``m_running`` and ``m_stop`` flags.

2. If you cannot upgrade, ensure your ``RestartableThread()`` implementation doesn't block indefinitely:

.. code-block:: cpp

    void RestartableThread() override
    {
        // Use timeouts instead of indefinite waits
        if (isRunning()) {
            // Process with timeout
            processDataWithTimeout(100);  // 100ms timeout
        }
    }

3. Always call ``Stop()`` before ``Join()``:

.. code-block:: cpp

    thread.Stop();   // Signal thread to stop
    usleep(10000);   // Brief delay to allow thread to process stop signal
    thread.Join();   // Wait for completion

**Fixed in**: Version 0.0.2 (January 2026)

Thread Not Responding to Stop Signal
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: Thread continues running even after ``Stop()`` is called.

**Cause**: The thread's ``RestartableThread()`` method may be blocking in a long operation.

**Solution**: 

1. Check thread state regularly in long operations:

.. code-block:: cpp

    void RestartableThread() override
    {
        while (isRunning()) {
            // Break long operations into chunks
            for (int i = 0; i < 1000 && isRunning(); i++) {
                processItem(i);
            }
        }
    }

2. Use timeouts on blocking operations:

.. code-block:: cpp

    void RestartableThread() override
    {
        // Instead of: sem_wait(semaphore);
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1;  // 1 second timeout
        sem_timedwait(semaphore, &timeout);
    }

Shared Memory Issues
--------------------

"No Semaphore Slots Available" Error
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: Registration fails with "No semaphores available - all 10 slots are in use".

**Cause**: All 10 semaphore slots are occupied, possibly by dead processes that didn't unregister.

**Solution**:

1. **Update to version 0.0.2 or later** - Automatic cleanup of stale semaphores is now performed before each registration.

2. For older versions, manually clean up:

.. code-block:: python

    # Python
    writer = shm("/tmp/example.im.shm", data)
    cleaned = writer.cleanup_stale_semaphores()
    print(f"Cleaned {cleaned} stale entries")

.. code-block:: cpp

    // C++
    Dao::Shm<float> writer(shmPath, shape, data);
    int cleaned = writer.cleanup_stale_semaphores();
    std::cout << "Cleaned " << cleaned << " stale entries" << std::endl;

3. Verify processes using semaphores:

.. code-block:: python

    readers = writer.get_registered_readers()
    for r in readers:
        print(f"Sem {r['index']}: PID {r['reader_pid']} - {r['reader_name']}")

**Fixed in**: Version 0.0.2 (January 2026) - Automatic cleanup added

"Semaphore Slot Stolen" Error
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: Reader registration fails or validation fails with "Semaphore slot stolen by another process".

**Cause**: Two processes tried to register with the same preferred semaphore, or a process crashed and another took its slot.

**Solution**:

1. Use automatic allocation instead of preferred semaphores:

.. code-block:: python

    # Let system choose semaphore
    sem = reader.register_reader("my_reader")
    # Don't specify preferred_sem unless necessary

2. Validate registration periodically:

.. code-block:: python

    if not reader.validate_registration():
        # Re-register if stolen
        sem = reader.register_reader("my_reader")
        
3. For diagnostic purposes, check who owns each semaphore:

.. code-block:: python

    readers = shm_obj.get_registered_readers()
    for entry in readers:
        print(f"Slot {entry['index']}: PID {entry['reader_pid']}")

Cannot Re-register Reader
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: Attempting to call ``register_reader()`` twice fails with error about already being registered.

**Cause**: Prior to version 0.0.2, the system didn't allow re-registration.

**Solution**:

1. **Update to version 0.0.2 or later** - Re-registration is now automatic. The system unregisters before re-registering.

2. For older versions, manually unregister first:

.. code-block:: python

    reader.unregister_reader()
    sem = reader.register_reader("new_name")

.. code-block:: cpp

    reader.unregister_reader();
    int sem = reader.register_reader("new_name");

**Fixed in**: Version 0.0.2 (January 2026)

Shared Memory Synchronization Issues
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: Reader gets stale data or misses updates.

**Cause**: Not using proper synchronization mechanism.

**Solution**:

1. Use semaphore synchronization for each reader:

.. code-block:: python

    # Register and get semaphore index
    sem = reader.register_reader("my_reader")
    
    # Use that semaphore for synchronization
    while running:
        data = reader.get_data(check=True, semNb=sem)
        process(data)

2. For multiple readers on same buffer, each should use their own semaphore:

.. code-block:: cpp

    // Reader 1
    Dao::Shm<float> reader1(shmPath);
    int sem1 = reader1.register_reader("reader1");
    float* data1 = reader1.get_frame((Dao::ShmSync)sem1);
    
    // Reader 2
    Dao::Shm<float> reader2(shmPath);
    int sem2 = reader2.register_reader("reader2");
    float* data2 = reader2.get_frame((Dao::ShmSync)sem2);

Build Issues
------------

Protobuf Version Mismatch
~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: Build fails with protobuf-related errors, especially on ARM64.

**Solution**: See the installation guide for platform-specific protobuf installation instructions.

Missing Dependencies
~~~~~~~~~~~~~~~~~~~~

**Symptom**: Build fails with missing library errors.

**Solution**: Install all required packages:

.. code-block:: bash

    # Ubuntu/Debian
    sudo apt install -y libtool pkg-config build-essential autoconf automake \
        python3 python-is-python3 libssl-dev libncurses5-dev libncursesw5-dev \
        redis libgtest-dev libgsl-dev libzmq3-dev protobuf-compiler \
        numactl libnuma-dev
    
    # macOS
    brew install pkg-config zeromq protobuf gsl

Test Failures
-------------

Running Tests
~~~~~~~~~~~~~

To run all tests:

.. code-block:: bash

    cd daoBase
    waf --test

To run specific test:

.. code-block:: bash

    ./build/test/test_semaphore_registry
    ./build/test/test_update_thread

Intermittent Test Failures
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: Tests pass sometimes and fail other times, especially threading tests.

**Cause**: Race conditions or timing issues in the tests.

**Solution**: 

1. Ensure you're running version 0.0.2 or later where threading issues have been fixed.

2. If tests still fail intermittently, check system load:

.. code-block:: bash

    # Reduce system load before running tests
    # Close unnecessary applications
    waf clean
    waf --test

Performance Issues
------------------

Slow Shared Memory Access
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: Reading from or writing to shared memory is slower than expected.

**Solution**:

1. Use proper synchronization to avoid busy-waiting:

.. code-block:: python

    # Bad: busy-wait polling
    while True:
        data = reader.get_data()
        if counter_changed:
            process(data)
    
    # Good: semaphore synchronization
    sem = reader.register_reader("reader")
    while running:
        data = reader.get_data(check=True, semNb=sem)
        process(data)

2. For real-time applications, pin threads to specific cores:

.. code-block:: cpp

    // Pin to core 2
    ProcessingThread procThread(logger, 2);

3. Enable real-time scheduling (requires root):

.. code-block:: cpp

    // rt_enabled=true in constructor
    Thread thread("RealTime", logger, core, -1, true);

Memory Leaks
~~~~~~~~~~~~

**Symptom**: Memory usage grows over time.

**Solution**:

1. Ensure proper cleanup:

.. code-block:: python

    reader = shm("/tmp/example.im.shm")
    try:
        # Use reader
        pass
    finally:
        reader.close()  # Explicit cleanup

2. Rely on automatic cleanup:

.. code-block:: cpp

    {
        Dao::Shm<float> reader(shmPath);
        // Use reader
    }  // Automatically unregisters and cleans up

Platform-Specific Issues
------------------------

macOS Issues
~~~~~~~~~~~~

**Symptom**: Semaphore operations behave differently on macOS.

**Cause**: macOS uses a different semaphore implementation.

**Solution**: The library handles platform differences automatically. Ensure you're using POSIX-compliant operations.

Windows Issues
~~~~~~~~~~~~~~

**Symptom**: Shared memory creation fails on Windows.

**Cause**: Windows requires different security attributes.

**Solution**: The library handles Windows-specific requirements automatically. Ensure you have proper permissions for creating shared memory objects.

Getting Help
------------

If you encounter issues not covered here:

1. Check the GitHub issues: https://github.com/Durham-Adaptive-Optics/daoBase/issues
2. Review the API documentation
3. Enable debug logging:

.. code-block:: python

    from daoShm import setLogLevel
    setLogLevel(4)  # 4 = DEBUG level

4. Create a minimal reproducible example and report the issue

Version History
---------------

**Version 0.0.2 (January 2026)**

- Fixed thread hanging on ``Join()`` 
- Added automatic semaphore re-registration support
- Added automatic cleanup of stale semaphore registrations
- Improved thread lifecycle management

**Version 0.0.1 (October 2025)**

- Initial release
