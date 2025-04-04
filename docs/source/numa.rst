NUMA System
===========

Overview
--------

NUMA (Non-Uniform Memory Access) is a computer memory design used in multiprocessor systems where the memory access time depends on the memory location relative to the processor. The DAO NUMA system provides utilities to optimize performance on NUMA-based architectures by managing processor affinity and memory allocation.

.. image:: _static/numa_architecture.png
   :width: 500px
   :alt: NUMA Architecture
   :align: center

On NUMA systems, memory access times are not uniform - each processor can access its own local memory faster than non-local memory (memory local to another processor or memory shared between processors). The DAO NUMA utilities help applications optimize performance by:

1. Pinning threads to specific CPU cores
2. Allocating memory on the same NUMA node as the processing thread
3. Providing mapping between cores and NUMA nodes

Key Features
-----------

- CPU core affinity management
- NUMA-aware memory allocation
- Core-to-node mapping
- Cross-platform support (full functionality on Linux, compatibility layer on other platforms)

Namespace and Functions
---------------------

The NUMA functions are contained within the ``Dao::Numa`` namespace:

Core Affinity Functions
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    void SetProcAffinity(int core);
    int GetProcAffinity();

These functions control which CPU core a thread runs on:

- **SetProcAffinity**: Pin the current thread to a specific CPU core
- **GetProcAffinity**: Get the current CPU core the thread is running on

NUMA Node Mapping
~~~~~~~~~~~~~~

.. code-block:: cpp

    int Core2Node(int core);
    int Node2FirstCore(int node);

These functions provide mapping between CPU cores and NUMA nodes:

- **Core2Node**: Get the NUMA node that a specific core belongs to
- **Node2FirstCore**: Get the first CPU core on a specific NUMA node

NUMA Memory Allocation
~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    void* AllocOnNode(size_t size, int node);
    
    template <class T>
    T* AllocOnNode(size_t nElements, int node, T fill);
    
    void Free(void* start, size_t size);
    
    template<class T>
    void FreeT(T* start, size_t nElements);

These functions manage NUMA-aware memory allocation:

- **AllocOnNode**: Allocate memory on a specific NUMA node
- **AllocOnNode<T>**: Allocate typed memory on a specific node with fill value
- **Free**: Free memory allocated with AllocOnNode
- **FreeT<T>**: Free typed memory allocated with AllocOnNode<T>

Utility Functions
~~~~~~~~~~~~~~

.. code-block:: cpp

    int GetMaxCores();
    size_t GetMaxNode();

These functions provide system information:

- **GetMaxCores**: Get the maximum number of CPU cores on the system
- **GetMaxNode**: Get the maximum NUMA node number on the system

Platform Support
--------------

The NUMA implementation varies by platform:

- **Linux**: Full NUMA support using the ``numa.h`` and ``numaif.h`` headers
- **macOS/Windows**: Limited functionality, providing API compatibility

Usage Patterns
-------------

Basic CPU Affinity
~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <daoNuma.hpp>
    
    // Pin current thread to core 2
    Dao::Numa::SetProcAffinity(2);
    
    // Get current core
    int currentCore = Dao::Numa::GetProcAffinity();

NUMA-Aware Memory Allocation
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    // Get the current core
    int core = Dao::Numa::GetProcAffinity();
    
    // Get the NUMA node for this core
    int node = Dao::Numa::Core2Node(core);
    
    // Allocate memory on the same node as the current thread
    float* data = (float*)Dao::Numa::AllocOnNode(1024 * sizeof(float), node);
    
    // Use the memory
    for (int i = 0; i < 1024; i++) {
        data[i] = i * 0.1f;
    }
    
    // Free the memory
    Dao::Numa::Free(data, 1024 * sizeof(float));

Typed Memory Allocation with Fill Value
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    // Allocate an array of 1024 floats on node 0, initialized to 0.0f
    float* data = Dao::Numa::AllocOnNode<float>(1024, 0, 0.0f);
    
    // Free the memory
    Dao::Numa::FreeT<float>(data, 1024);

Integration with Thread System
---------------------------

The NUMA system is designed to work with the DAO Thread system:

.. code-block:: cpp

    #include <daoThread.hpp>
    #include <daoNuma.hpp>
    
    class ProcessingThread : public Dao::Thread
    {
    public:
        ProcessingThread(Dao::Log::Logger& logger, int core)
        : Thread("Processor", logger, core)
        {
            // Get NUMA node for this thread
            m_node = Dao::Numa::Core2Node(core);
            
            // Allocate memory on this NUMA node
            m_data = Dao::Numa::AllocOnNode<float>(1024, m_node, 0.0f);
        }
        
        ~ProcessingThread()
        {
            // Free NUMA memory
            Dao::Numa::FreeT<float>(m_data, 1024);
        }
        
    protected:
        void RestartableThread() override
        {
            // Process data
            processData();
        }
        
    private:
        int m_node;
        float* m_data;
    };

Best Practices
-------------

1. **Thread Placement**: Place threads performing related work on the same NUMA node
2. **Memory Allocation**: Allocate memory on the same node as the thread that will use it most
3. **Data Sharing**: Minimize data sharing between threads on different NUMA nodes
4. **Memory Access Patterns**: Be aware of memory access patterns that may cross NUMA node boundaries
5. **Core Affinity**: Use core affinity to ensure threads stay on their assigned cores

Performance Considerations
------------------------

- **Memory Bandwidth**: Each NUMA node has its own memory bandwidth
- **Cache Coherency**: Cache coherency operations across NUMA nodes can be expensive
- **Access Latency**: Remote memory access has higher latency than local memory access
- **Allocation Overhead**: NUMA-aware allocation has slightly higher overhead than standard allocation
- **Core Mapping**: Core to node mapping is system-specific

Example: Data Processing on Multiple NUMA Nodes
---------------------------------------------

.. code-block:: cpp

    #include <daoNuma.hpp>
    #include <daoThreadTable.hpp>
    
    class NumaProcessor
    {
    public:
        NumaProcessor()
        {
            // Determine system topology
            m_maxNodes = Dao::Numa::GetMaxNode();
            m_maxCores = Dao::Numa::GetMaxCores();
            
            // Create thread for each node
            for (size_t node = 0; node <= m_maxNodes; node++) {
                int core = Dao::Numa::Node2FirstCore(node);
                ProcessingThread* thread = new ProcessingThread(m_logger, core, node);
                m_threadTable.Add(thread);
                m_threads.push_back(thread);
            }
        }
        
        void run()
        {
            // Start all threads
            m_threadTable.Spawn();
            m_threadTable.Start();
            
            // Wait for completion
            m_threadTable.Join();
        }
        
    private:
        size_t m_maxNodes;
        int m_maxCores;
        Dao::Log::Logger m_logger;
        Dao::ThreadTable m_threadTable;
        std::vector<ProcessingThread*> m_threads;
    };