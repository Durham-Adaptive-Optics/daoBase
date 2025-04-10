DAO Component System
====================

Overview
--------

The DAO Component system provides a framework for building modular, state-driven applications. It implements a consistent component lifecycle model with well-defined states and transitions. Components communicate via ZeroMQ messaging and can share memory for high-performance data exchange.

.. image:: _static/component_diagram.png
   :width: 600px
   :alt: Component Architecture
   :align: center

Component Architecture
----------------------

The Component architecture consists of several layers:

1. **Component Class**: The user-facing API that implements the ComponentIfce interface
2. **ComponentBase**: Core implementation providing state machine and thread management
3. **ComponentIfce**: Interface definition for component lifecycle methods
4. **ComponentStateMachine**: State machine implementation for component lifecycle
5. **ComponentUpdateThread**: Thread for handling shared memory updates
6. **ComponentZmqThread**: Thread for handling ZeroMQ messaging

Class Hierarchy
---------------

.. code-block:: none

    ComponentIfce (Interface)
        |
        +--> StateMachine
                |
                +--> ComponentBase
                        |
                        +--> Component

Component States
----------------

Components follow a state machine with the following states:

- **Off**: Initial state, component is not initialized
- **Standby**: Component is initialized but not enabled
- **Idle**: Component is enabled but not running
- **Running**: Component is actively processing
- **Error**: Component has encountered an error

State transitions are triggered by events:

- **Init**: Transition from Off to Standby
- **Stop**: Transition from Standby to Off
- **Enable**: Transition from Standby to Idle
- **Disable**: Transition from Idle to Standby
- **Run**: Transition from Idle to Running
- **Idle**: Transition from Running to Idle
- **OnFailure**: Transition to Error state
- **Recover**: Transition from Error to Idle

.. image:: _static/state_machine.png
   :width: 500px
   :alt: Component State Machine
   :align: center

Component Class
---------------

The Component class is the main entry point for users to create component instances. It inherits from ComponentBase and implements the ComponentIfce interface.

Construction
~~~~~~~~~~~~

.. code-block:: cpp

    Component(std::string name, Dao::Log::Logger& logger, std::string ip, int port, int core=-1)

Parameters:

- **name**: Unique identifier for the component
- **logger**: Logger instance for component logging
- **ip**: IP address for ZMQ communication
- **port**: Port number for ZMQ communication
- **core**: CPU core affinity (-1 means no specific core)

Lifecycle Methods
~~~~~~~~~~~~~~~~~

Components expose the following lifecycle methods:

- **Init()**: Initialize the component
- **Stop()**: Stop and clean up the component
- **Enable()**: Enable the component's functionality
- **Disable()**: Disable the component's functionality
- **Run()**: Start active processing
- **Idle()**: Pause active processing
- **OnFailure()**: Handle error conditions
- **Recover()**: Recover from error state
- **GetStateText()**: Get the current state as a string

ComponentBase Class
-------------------

The ComponentBase class provides the core implementation for components, handling thread management and state transitions.

Key Methods
~~~~~~~~~~~

- **PostConstructor()**: Finalizes component setup after construction
- **PostEnable()**: Additional actions during Enable
- **PostDisable()**: Additional actions during Disable

Thread Management
~~~~~~~~~~~~~~~~~


ComponentBase manages two threads:

- **ZmqThread**: Handles command-response messaging
- **UpdateThread**: Handles shared memory updates

ComponentIfce Interface
-----------------------

This interface defines the required methods for component lifecycle management:

.. code-block:: cpp

    class ComponentIfce
    {
    public:
        virtual ~ComponentIfce(){};
        
        virtual void Init()     = 0;
        virtual void Stop()     = 0;
        virtual void Enable()   = 0;
        virtual void Disable()  = 0;
        virtual void Run()      = 0;
        virtual void Idle()     = 0;
        virtual void OnFailure()= 0;
        virtual void Recover()  = 0;
        virtual std::string GetStateText() = 0;
    };

State Machine
-------------

The Component State Machine is based on the StateMachine class and handles transitions between component states.

Key Features:

- Event-driven state transitions
- Entry and exit hooks for each state
- Protected virtual methods for transition customization
- Thread-safe state changes

ZeroMQ Communication
--------------------

The ComponentZmqThread handles command and control messaging using ZeroMQ's request-response pattern.

Supported Commands:

- **EXEC**: Execute component lifecycle methods
- **SETUP**: Configure component parameters
- **UPDATE**: Update component data
- **PING**: Check component health
- **STATE**: Get current state information
- **SET_LOG_LEVEL**: Change logging level

Example Command Processing:

.. code-block:: cpp

    void process_EXEC(std::string Payload)
    {
        if(Payload == "Init")
            m_ifce->Init();
        else if (Payload == "Stop")
            m_ifce->Stop();
        else if (Payload == "Enable")
            m_ifce->Enable();
        // ...
    }

Shared Memory Updates
---------------------

The ComponentUpdateThread manages updates from shared memory using the DAO shared memory system.

Features:

- Supports multiple data types
- Uses double buffering for thread safety
- Provides callback mechanism for update notifications
- Tracks update counters for synchronization

Example Item Update:

.. code-block:: cpp

    template<class T>
    class ItemUpdate
    {
    public:
        ItemUpdate(ShmIfce<T> * shm, DoubleBuffer<T>* buffer, 
                  std::string name, std::function<void()> callback = nullptr);
        
        void check_update(Log::Logger& logger);
        void setCounter();
        uint64_t getCounter();
        std::string getName();
    };

Creating a Custom Component
---------------------------

To create a custom component, inherit from the Component class:

.. code-block:: cpp

    #include <daoComponent.hpp>
    
    class MyComponent : public Dao::Component
    {
    public:
        MyComponent(std::string name, Dao::Log::Logger& logger, 
                  std::string ip, int port, int core=-1)
        : Component(name, logger, ip, port, core)
        {
            // Custom initialization
        }
        
    protected:
        // Override state machine hooks
        void entry_Idle() override 
        {
            // Custom idle state entry
            Component::entry_Idle();
        }
        
        void transition_Idle_Running() override
        {
            // Custom transition logic
            Component::transition_Idle_Running();
        }
        
    private:
        // Component-specific members
    };

Usage Example
-------------

.. code-block:: cpp

    #include <daoComponent.hpp>
    #include <daoLog.hpp>
    
    int main()
    {
        // Create logger
        Dao::Log::Logger logger("MyApp");
        logger.SetLevel(Dao::Log::LEVEL::INFO);
        
        // Create component
        Dao::Component myComponent("TestComponent", logger, "127.0.0.1", 5555);
        
        // Initialize component
        myComponent.Init();
        
        // Enable component
        myComponent.Enable();
        
        // Start processing
        myComponent.Run();
        
        // Later...
        
        // Stop processing
        myComponent.Idle();
        
        // Disable component
        myComponent.Disable();
        
        // Shut down
        myComponent.Stop();
        
        return 0;
    }

Best Practices
--------------

1. **Lifecycle Management**: Always follow the proper sequence of state transitions
2. **Error Handling**: Implement robust error handling in the OnFailure() method
3. **Resource Cleanup**: Clean up resources in the Stop() method
4. **Thread Safety**: Use thread-safe data structures for sharing data between threads
5. **Core Affinity**: Set appropriate core affinity for real-time applications

Advanced Features
-----------------

Custom State Transitions
~~~~~~~~~~~~~~~~~~~~~~~~

Override the transition methods to implement custom behavior:

.. code-block:: cpp

    void transition_Idle_Running() override
    {
        // Pre-transition tasks
        prepareForRunning();
        
        // Call base implementation
        Component::transition_Idle_Running();
        
        // Post-transition tasks
        startDataProcessing();
    }

Shared Memory Integration
~~~~~~~~~~~~~~~~~~~~~~~~~

Add shared memory items to the update thread:

.. code-block:: cpp

    // In component constructor
    ShmIfce<float> * shm = new ShmIfce<float>("data.im.shm");
    DoubleBuffer<float> * buffer = new DoubleBuffer<float>(1024);
    
    // Add to update thread
    m_update_thread->add(shm, buffer, "myData", [this](){
        // Custom callback on update
        this->processNewData();
    });

ZeroMQ Command Handling
~~~~~~~~~~~~~~~~~~~~~~~


To handle custom commands, extend the ZmqThread functionality:

.. code-block:: cpp

    // Create custom ZmqThread subclass
    class MyZmqThread : public ComponentZmqThread
    {
        // Override process_message to handle custom commands
    };
    
    // Then use your custom thread in the component