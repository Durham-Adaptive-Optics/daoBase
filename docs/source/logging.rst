Logging
=======

Dao provides a flexible logging system that supports multiple output destinations,
including standard output, files, and network sockets, with the ability to control
output granularity based on severity levels at runtime.

Severity Levels
===============

Log messages are assigned a severity level by the programmer, indicating the
importance of each message and allowing the logging system to filter for specific
severities during output.

The following is a list of all the available severity levels:

* TRACE
* DEBUG
* INFO
* WARNING
* ERROR
* CRITICAL

Python
======

To use the Dao logging system within a Python application, import the following:

.. code-block:: python

    from daoLog import daoLog
    import logging

The example below demonstrates how to create a Dao logger and use it to output
logs to the standard output:

.. code-block:: python
    _ = daoLog("LoggingApp")

    logger = logging.getLogger("LoggingApp")
    logger.Trace("trace log")
    logger.Debug("debug log")
    logger.Info("info log")
    logger.Warning("warning log")
    logger.Error("error log")
    logger.critical("critical log")

By default, the log severity threshold is set to ``TRACE``, therefore all severities
will be included in the output. The following shows how to To adjust the logging granularity
at runtime:

.. code-block:: python
    newSeverity: str = "INFO"  # Any supported severity level
    logger = logging.getLogger("LoggingApp")
    logger.setLevel(newSeverity)

To have the logs also be directed to a file, you simply need to pass the desired
output file path when creating the logger:

.. code-block:: python
    _ = daoLog("LoggingApp", filename="logFile.log")

By default the logs will now be output to both standard output and the specified file,
however you can disable output to standard output by doing the following:

.. code-block:: python
    _ = daoLog("LoggingApp", filename="logFile.log", toScreen=False)


Each log contains several pieces of information such as the logger name, a timestamp, 
the device host name and the severity level - below shows an example output from the logger.

.. code-block:: text
    [2025-10-13 09:15:17,286] [HOST-NAME] - __main__ [TRACE] : trace log (daoLog.py:77)
    [2025-10-13 09:15:17,286] [HOST-NAME] - __main__ [DEBUG] : debug log (daoLog.py:276)
    [2025-10-13 09:15:17,286] [HOST-NAME] - __main__ [INFO] : info log (daoLog.py:277)
    [2025-10-13 09:15:17,286] [HOST-NAME] - __main__ [WARNING] : warning log (daoLog.py:278)
    [2025-10-13 09:15:17,286] [HOST-NAME] - __main__ [ERROR] : error log  (daoLog.py:279)
    [2025-10-13 09:15:17,286] [HOST-NAME] - __main__ [CRITICAL] : critical log (daoLog.py:280)

C++
===

The C++ logging system queues log messages as they are issued by the application
and routes them to the configured output destination on a dedicated background
thread, minimizing overhead for real-time applications.

To use the Dao logging system within your C++ application, include the following header:

.. code-block:: cpp
    #include <daoLog.hpp>

The following example shows how to create a Dao logger and output various logs
to standard output:

.. code-block:: cpp
    Dao::Log logger("LoggingApp", Dao::Log::DESTINATION::SCREEN);

    logger.Trace("trace log");
    logger.Debug("debug log");
    logger.Info("info log");
    logger.Warning("warning log");
    logger.Error("error log");
    logger.critical("critical log");

Note that the trace and debug logs are omitted
as the default severity threshold is the ``INFO`` level, thus filtering out
lower-severity messages. The threshold can be adjusted at runtime depending on
the desired granularity of the output, as shown below:

.. code-block:: cpp
    Dao::Log::LEVEL newSeverity = Dao::Log::LEVEL::TRACE; // Any level in Dao::Log::LEVEL
    logger.SetLevel(newSeverity);

The logger can instead direct logs to a file instead of standard output. Below shows how
to configure the logger object to achieve this.

.. code-block:: cpp
    std::string logFilePath = "logFile.log".
    Dao::Log::Logger logger("LoggingApp", Dao::Log::Logger::DESTINATION::FILE, logFilePath);

The logger is also capable of directing the logs over the network using a TCP ZeroMQ socket. 
A user created ZeroMQ application can then receive these logs on the destination device and
be processed as desired. The following shows how to configure the logger to achieve this.

.. code-block:: cpp
    int portNumber = 1234;
    std::string ipAddr = "127.0.0.1";
    Dao::Log::Logger logger("LoggingApp", Dao::Log::Logger::DESTINATION::NETWORK, ipAddr, portNumber);

The logger formats each log message upon output to the destination. Each log contains several
pieces of information such as the logger name, a timestamp, the device host name and 
the severity level - below shows an example output from the logger.

.. code-block:: text
    LoggingApp:25-10-13 10:23:28 [TRACE]    - trace log
    LoggingApp:25-10-13 10:23:28 [DEBUG]    - debug log
    LoggingApp:25-10-13 10:23:28 [INFO ]    - info log
    LoggingApp:25-10-13 10:23:28 [WARNING]  - warning log
    LoggingApp:25-10-13 10:23:28 [ERROR]    - error log
    LoggingApp:25-10-13 10:23:28 [CRITICAL] - critical log
