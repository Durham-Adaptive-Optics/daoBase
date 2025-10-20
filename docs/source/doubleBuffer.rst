Double Buffer System
====================

Overview
--------

The DAO Double Buffer system provides a thread-safe mechanism for sharing data between threads with minimal locking. It implements a classic double buffering pattern where one buffer is used for reading while the other is used for writing, with the ability to swap buffers atomically.

This pattern is particularly useful in real-time applications where consistent access to data is required without blocking writers or readers.

.. image:: _static/double_buffer.png
   :width: 500px
   :alt: Double Buffer Concept
   :align: center

Key Features
------------

- Thread-safe access to shared data
- Zero-copy data transfer
- NUMA-aware memory allocation
- Support for multiple data types
- Buffer swapping with frame synchronization
- Low-latency access to current data

DoubleBuffer Class
------------------

The ``DoubleBuffer`` class is a template class that can store any data type:

.. code-block:: cpp

    template <class T> class DoubleBuffer {
        // ...
    };

Construction
~~~~~~~~~~~~~
The constructor initializes the double buffer with a specified number of elements and an optional initial value. It can also allocate memory on a specific NUMA node. The double buffer is templated, allowing it to store any data type. The constructor allocates memory for two buffers of the specified size and initializes them with the given fill value.

The constructor signature is as follows:

.. code-block:: cpp

    DoubleBuffer(size_t numberOfElements, int alloc_now_node = -1, T fillvalue = 0)

Parameters:

- **numberOfElements**: Number of elements the buffer will store
- **alloc_now_node**: NUMA node to allocate memory on (-1 for default allocation)
- **fillvalue**: Initial value for all elements in both buffers (default is 0)

Buffer Management
~~~~~~~~~~~~~~~~~

The DoubleBuffer class maintains two internal buffers and tracks which one is currently active:

- **Active Buffer**: Used for reading, represents current state
- **Passive Buffer**: Used for writing, represents future state

Buffer States
~~~~~~~~~~~~~

- **Active Index**: Index of the currently active buffer (0 or 1)
- **Dirty Flag**: Indicates if the passive buffer has been modified
- **Target Frame**: Optional frame number for synchronized swapping

Core Methods
------------
The DoubleBuffer class provides several core methods for managing the buffers:

Buffer Access
~~~~~~~~~~~~~

- **Active()**: Get pointer to the active buffer for reading
- **Passive()**: Get pointer to the passive buffer for writing
- **SetActiveBuffer(int index)**: Manually set which buffer is active
- **SwapBuffers()**: Switch the active and passive buffers

Data Operations
~~~~~~~~~~~~~~~

- **CopyIn(T* data, uint64_t frame = 0)**: Copy data into the passive buffer
- **CopyAndSwap(T* data)**: Copy data into passive buffer and immediately swap

The CopyAndSwap is designed to be overloaded incase data manipulation is required on loading the data into the double buffer.

NUMA Integration
~~~~~~~~~~~~~~~~

- **AllocOnNode()**: Allocate buffer memory on a specific NUMA node
- **GetNode()**: Get the NUMA node where buffers are allocated

Frame Synchronization
~~~~~~~~~~~~~~~~~~~~~

- **Active(uint64_t &frame)**: Get active buffer, swap if target frame is reached
- **SetDirty()**: Mark the passive buffer as modified
- **GetDirty()**: Check if the passive buffer is modified

Usage Patterns
--------------

Basic Usage
~~~~~~~~~~~

.. code-block:: cpp

    // Create a double buffer for 1024 float values
    Dao::DoubleBuffer<float> buffer(1024);
    
    // Get pointer to active buffer for reading
    float* data = buffer.Active();
    
    // Process data
    processData(data, 1024);
    
    // Later, update data
    float* newData = generateNewData();
    buffer.CopyAndSwap(newData);

NUMA-Aware Usage
~~~~~~~~~~~~~~~~

.. code-block:: cpp

    // Create a double buffer on NUMA node 0 with initial value 0.0
    Dao::DoubleBuffer<float> buffer(1024, 0, 0.0);
    
    // Verify NUMA node
    int node = buffer.GetNode();  // Should return 0

Frame Synchronized Updates
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    // Update buffer with new data and a frame number
    buffer.CopyIn(newData, frameCounter);
    
    // Later, access buffer with frame check
    uint64_t currentFrame = frameCounter;
    float* data = buffer.Active(currentFrame);  // Will swap if frameCounter >= target

Low-Level Buffer Management
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    // Manually manage buffers
    buffer.SetActiveBuffer(0);  // Set buffer 0 as active
    
    // Get passive buffer for writing
    float* writeBuffer = buffer.Passive();
    
    // Modify write buffer directly
    for (int i = 0; i < 1024; i++) {
        writeBuffer[i] = i * 0.1f;
    }
    
    // Mark as dirty and swap
    buffer.SetDirty();
    buffer.SwapBuffers();

Best Practices
--------------

1. **Memory Management**: Be aware of buffer allocation and deallocation, especially with NUMA
2. **Thread Safety**: The double buffer itself is not thread-safe; external synchronization is required for multithreaded access
3. **Buffer Size**: Choose appropriate buffer size to balance memory usage and performance
4. **Frame Synchronization**: Use frame numbers for synchronized updates in frame-based applications
5. **Error Handling**: Check for allocation failures when creating double buffers

Integration with Component System
---------------------------------

The DoubleBuffer class is designed to work seamlessly with the DAO Component system:

.. code-block:: cpp

    // In component constructor
    m_dataBuffer = new DoubleBuffer<float>(1024, m_node);
    
    // In component update thread
    void ComponentUpdateThread::RestartableThread()
    {
        // Check for new data
        if (m_shm->get_counter() > m_counter)
        {
            // Copy new data to passive buffer and swap
            m_dataBuffer->CopyAndSwap(m_shm->get_frame());
            m_counter = m_shm->get_counter();
        }
    }
    
    // In processing logic
    float* currentData = m_dataBuffer->Active();
    processData(currentData);

Example: Real-Time Data Processing
----------------------------------

.. code-block:: cpp

    #include <daoDoubleBuffer.hpp>
    
    class DataProcessor
    {
    public:
        DataProcessor(int bufferSize)
        : m_buffer(bufferSize)
        {
            // Initialize
        }
        
        void updateData(float* newData)
        {
            // Copy new data and swap buffers
            m_buffer.CopyAndSwap(newData);
        }
        
        void processData()
        {
            // Get current data without blocking
            float* data = m_buffer.Active();
            
            // Process data without worrying about concurrent updates
            for (size_t i = 0; i < m_buffer.Size(); i++)
            {
                // Process each element
                result[i] = processElement(data[i]);
            }
        }
        
    private:
        Dao::DoubleBuffer<float> m_buffer;
    };