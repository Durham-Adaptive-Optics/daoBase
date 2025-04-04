DAO Thread System
===============

Overview
--------

The DAO Thread system provides a robust and flexible framework for creating and managing threads in C++ applications. It is designed to support real-time applications with features such as core affinity, NUMA awareness, and inter-thread communication.

The thread system consists of several layers:

1. **ThreadIfce**: Interface defining the thread operations
2. **ThreadBase**: Base implementation providing core thread functionality
3. **Thread**: User-facing class that combines the interface and base implementation
4. **ThreadTable**: Container for managing multiple threads
5. **SignalTable**: Inter-thread communication mechanism

Class Hierarchy
--------------

.. code-block:: none

    ThreadIfce (Interface)
        |
        |      ThreadBase
        |          |
        +----------+
                |
                +--> Thread

Thread Interface
--------------

The ``ThreadIfce`` class defines the standard interface for all thread operations:

.. code-block:: cpp

    class ThreadIfce
    {
    public:
        virtual ~ThreadIfce(){};
        
        virtual void Start()    = 0;
        virtual void Stop()     = 0;    
        virtual void Exit()     = 0;
        virtual void Spawn()    = 0;
        virtual void Join()     = 0;
        virtual void Kill(int signal) = 0;    
      
        // signaling
        virtual void Signal(int index) = 0;
        virtual void SignalWaitSpin(int index) = 0;
        virtual void SignalWaitSleep(int index, int usleep) = 0;
    };

Thread Base Class
----------------

The ``ThreadBase`` class provides the core implementation of thread functionality:

Construction
~~~~~~~~~~~

.. code-block:: cpp

    ThreadBase(std::string thread_name, Log::Logger& logger, int core=-1, int thread_number=-1, bool rt_enabled=true)

Parameters:

- **thread_name**: Name for the thread (shown in system tools)
- **logger**: Logger instance for thread logging
- **core**: CPU core to pin this thread to (-1 for no specific core)
- **thread_number**: ID for thread when multiple threads of same type exist
- **rt_enabled**: Whether to use real-time scheduling when possible

Thread Lifecycle
~~~~~~~~~~~~~~~

Threads follow a specific lifecycle:

1. **Construction**: Thread object is created but not started
2. **Spawn**: Thread is spawned but waits for a start signal
3. **Start**: Thread begins execution
4. **Stop**: Thread stops execution but remains ready to restart
5. **Exit**: Thread exits completely
6. **Join**: Wait for thread to finish

Thread Lifecycle Methods
~~~~~~~~~~~~~~~~~~~~~~~

- **Spawn()**: Creates the thread and calls OnceOnSpawn()
- **Start()**: Signals the thread to begin execution and calls OnceOnStart()
- **Stop()**: Signals the thread to stop execution and calls OnceOnStop()
- **Exit()**: Signals the thread to exit completely and calls OnceOnExit()
- **Join()**: Waits for the thread to finish execution
- **Kill(int signal)**: Forcibly terminates the thread

Extension Points
~~~~~~~~~~~~~~

ThreadBase provides several virtual methods that can be overridden:

- **OnceOnSpawn()**: Called once when the thread is spawned
- **OnceOnStart()**: Called once when the thread is started
- **OnceOnStop()**: Called once when the thread is stopped
- **OnceOnExit()**: Called once when the thread exits
- **Body()**: The main thread function (must be implemented by derived classes)

Thread Class
-----------

The ``Thread`` class combines ThreadBase and ThreadIfce, providing a complete implementation:

.. code-block:: cpp

    class Thread: public ThreadBase, public ThreadIfce
    {
    public:
        Thread(std::string name, Log::Logger& logger, int core=-1, int thread_number=-1, bool rt_enabled=true);
        
        // Implements ThreadIfce methods by delegating to ThreadBase
        // ...
        
    protected:
        virtual void RestartableThread() = 0;  // Must be implemented by derived classes
    };

Creating a Custom Thread
----------------------

To create a custom thread, inherit from the Thread class and implement RestartableThread():

.. code-block:: cpp

    class MyThread : public Dao::Thread
    {
    public:
        MyThread(std::string name, Dao::Log::Logger& logger, int core=-1)
        : Thread(name, logger, core)
        {
            // Custom initialization
        }
        
    protected:
        void RestartableThread() override
        {
            // This function is called repeatedly while the thread is running
            // Implement your thread's main logic here
            
            // For example:
            processData();
            
            // Optional sleep to control execution rate
            usleep(1000);
        }
        
        // Optionally override lifecycle hooks
        void OnceOnStart() override
        {
            Thread::OnceOnStart();  // Call base implementation
            // Additional start logic
        }
    };

Thread Table
-----------

The ``ThreadTable`` class provides a way to manage multiple threads as a group:

.. code-block:: cpp

    ThreadTable threadTable;
    
    // Add threads
    threadTable.Add(&thread1);
    threadTable.Add(&thread2);
    
    // Start all threads
    threadTable.Spawn();
    threadTable.Start();
    
    // Stop all threads
    threadTable.Stop();
    threadTable.Exit();
    threadTable.Join();

Thread Table Methods
~~~~~~~~~~~~~~~~~~

- **Add(ThreadIfce* thread)**: Adds a thread to the table
- **Start()**: Starts all threads
- **Stop()**: Stops all threads
- **Exit()**: Signals all threads to exit
- **Spawn()**: Spawns all threads
- **Join()**: Waits for all threads to finish
- **Kill(int signal)**: Forcibly terminates all threads
- **Signal(int index)**: Sends a signal to all threads

Signal Table
----------

The ``SignalTable`` class provides a mechanism for inter-thread communication:

.. code-block:: cpp

    // Thread 1
    m_signal_table->SignalSend(SIGNAL_DATA_READY);
    
    // Thread 2
    m_signal_table->SignalReceiveSpin(SIGNAL_DATA_READY);  // Blocks until signal received

Signal Table Methods
~~~~~~~~~~~~~~~~~~

- **SignalSend(int index)**: Sends a signal
- **SignalReceive(int index)**: Non-blocking check for signal
- **SignalReceiveSpin(int index)**: Blocking wait for signal (spinning)
- **SignalReceiveSleep(int index, uint64_t uSleep)**: Blocking wait with sleep intervals
- **SignalReset(int index)**: Resets a specific signal
- **SignalTableReset()**: Resets all signals

Predefined Signals
~~~~~~~~~~~~~~~~

- **SIGNAL_THREAD_READY (0)**: Indicates thread is ready
- **SIGNAL_LOOPSTART_ALL (1)**: Used to synchronize the start of multiple threads

Real-Time Considerations
----------------------

For real-time applications, the thread system offers:

- **Core Affinity**: Pin threads to specific CPU cores
- **NUMA Awareness**: Optimize memory access for Non-Uniform Memory Architecture
- **Real-time Scheduling**: Use SCHED_FIFO when running as root
- **Signal-based Synchronization**: Low-latency inter-thread communication

Best Practices
-------------

1. **Thread Creation**: Create all threads at application startup to avoid dynamic thread creation overhead
2. **Core Placement**: Pin critical threads to isolated cores
3. **Thread Priority**: Use RT threads for time-critical operations
4. **Signal Management**: Reset signals appropriately to avoid lost or stale signals
5. **Error Handling**: Implement proper exception handling in the thread functions

Usage Example
------------

.. code-block:: cpp

    #include <daoThread.hpp>
    #include <daoLog.hpp>
    
    class ProcessingThread : public Dao::Thread
    {
    public:
        ProcessingThread(Dao::Log::Logger& logger)
        : Thread("Processor", logger, 2)  // Pin to core 2
        {
            // Initialize
        }
        
    protected:
        void RestartableThread() override
        {
            // Process data
            processNextDataItem();
            
            // Signal completion
            Signal(DATA_PROCESSED);
            
            // Sleep briefly to yield CPU
            usleep(100);
        }
        
    private:
        void processNextDataItem()
        {
            // Implementation
        }
    };
    
    int main()
    {
        Dao::Log::Logger logger("ThreadApp");
        
        ProcessingThread procThread(logger);
        
        // Start thread
        procThread.Spawn();
        procThread.Start();
        
        // Wait for thread to finish
        procThread.Join();
        
        return 0;
    }