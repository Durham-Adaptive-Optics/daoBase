import socket
from datetime import datetime
import logging
from logging.handlers import TimedRotatingFileHandler
import threading
import os,sys

# this is the network logger. Maybe we can encapsulate this better. 
import zmq
import daoLogging_pb2
import time

logging.TRACE = 5
logging.addLevelName(logging.TRACE, "TRACE")

PROCESS_NAME=__name__

class daoLog:
    def __init__(self, name=__name__, filepath=".dao/logs", addr='127.0.0.1:4115', level=logging.TRACE):
        self.name = name
        if name is None:
            self.logger = logging.getLogger(__name__)
        else:
            self.logger = logging.getLogger(self.name)
            global PROCESS_NAME
            PROCESS_NAME = self.name
        
        self.logger.setLevel(level)

        formatter = logging.Formatter('[%(asctime)s] [%(machine)s] - %(name)s [%(levelname)s] : %(message)s (%(filename)s:%(lineno)d)')
        formatter.converter = time.gmtime
        # formatter.formatTime = lambda record, datefmt=None: time.strftime("%Y-%m-%dT%H:%M:%S.000Z", datefmt)

        handler = logging.StreamHandler(sys.stdout)
        handler.setLevel(level)
        handler.setFormatter(formatter)
        self.logger.addHandler(handler)

        if self.name is not __name__:
            if filepath == ".dao/logs":
                filename = os.path.expanduser("~") + "/"  + filepath + "/" + name + '.logs'
            else:
                filename = filepath + "/" + name + '.logs'
            print(filename)
            formatter = logging.Formatter('[%(asctime)s] [%(machine)s] - %(name)s [%(levelname)s] : %(message)s')
            handler = TimedRotatingFileHandler(filename, when='midnight', interval=1, backupCount=7)
            handler.setFormatter(formatter)
            handler.setLevel(level)
            self.logger.addHandler(handler)

            handler = daoLogZmqHandler(f"tcp://{addr}")
            handler.setLevel(level)
            handler.setFormatter(formatter)
            self.logger.addHandler(handler)

        self.logger.addFilter(MachineFilter())

        logging.Logger.trace = self.trace

    def trace(self, msg, *args, **kwargs):
        """
        Log a message at the TRACE level.
        
        :param msg: The message to log
        """
        if self.logger.isEnabledFor(logging.TRACE):
            self.logger._log(logging.TRACE, msg, args, **kwargs)

class MachineFilter(logging.Filter):
    def __init__(self, machine=None):
        if machine is None:
            self.hostname = socket.gethostname()
        else:
            self.hostname = machine

    def filter(self, record):
        record.machine = self.hostname
        return True

class daoLogZmqHandler(logging.Handler):
    def __init__(self, addr):
        """
        Initialize the ZeroMQ handler with the specified parameters.
        
        :param addr: Address to send the logs
        """
        super().__init__()      
        # Create the ZeroMQ context and socket
        self.socketString = addr
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PUB)
        self.socket.connect(self.socketString)

    def emit(self, record):
        """
        Send the log message over the ZeroMQ socket.
        
        :param record: The log record to send
        """
        log_msg = daoLogging_pb2.LogMessage()
        log_msg.level = record.levelno
        log_msg.log_message = record.msg
        log_msg.time_stamp = str(record.created)
        log_msg.component_name = record.name
        log_msg.machine = record.machine
        sendString = log_msg.SerializeToString()
        self.socket.send(sendString)

class ColoredFormatter(logging.Formatter):
    """
    Custom formatter class to add color output to log messages
    """
    def __init__(self, msg, use_color = True):
        """
        Initialize the formatter with a message format and a flag to indicate whether to use color output
        """
        logging.Formatter.__init__(self, msg)
        self.use_color = use_color

    def format(self, record):
        """
        Override the format method to add color codes to the level name of the log message
        """
        if self.use_color:
            color = "\033[0;0m"  # default color
            if record.levelname == "ERROR":
                color = "\033[91m"
            elif record.levelname == "CRITICAL":
                color = "\033[31m"
            elif record.levelname == "FATAL":
                color = "\033[31m"
            elif record.levelname == "WARNING":
                color = "\033[93m"
            elif record.levelname == "INFO":
                color = "\033[97m"
            elif record.levelname == "DEBUG":
                color = "\033[92m"
            elif record.levelname == "TRACE":
                color = "\033[94m"
            record.levelname = color + record.levelname + "\033[0m"
        return logging.Formatter.format(self, record)
    def formatTime(self, record, datefmt=None):
        """
        Override the formatTime method to change the asctime format
        """
        ct = self.converter(record.created)
        if datefmt:
            s = time.strftime(datefmt, ct)
        else:
            t = time.strftime("%Y-%m-%d %H:%M:%S", ct)
            s = "%s.%03d" % (t, record.msecs)
        return s

class ZmqSubLogger(threading.Thread):
    """
    ZmqSubLogger is a class that runs the ZeroMQ subscriber logger in a separate thread.
    """
    def __init__(self, level=logging.TRACE, host='127.0.0.1', port = 5559):
        """
        Initialize the thread and the flag to turn on/off the display of messages
        """
        self.level = level
        self.host = host
        self.port = port
        threading.Thread.__init__(self)
        self.display_messages = True
        self.msgCnt = 0
        
    def run(self):
        """
        The run method contains the code for the ZeroMQ self.subscriber logger
        """
        # Create a ZeroMQ context
        self.context = zmq.Context()

        # Create a ZeroMQ subscriber socket
        self.subscriber = self.context.socket(zmq.SUB)
        self.subscriber.connect(f'tcp://{self.host}:{self.port}')
        self.subscriber.setsockopt(zmq.SUBSCRIBE, b'')

        # Create a logger
        self.logger = logging.getLogger("zmq_sub")
        
        # Create a console handler
        self.ch = logging.StreamHandler()
        #self.ch.setLevel(logging.INFO)

        # Create a formatter from the color one define previously
        #self.formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
        self.formatter = ColoredFormatter("%(asctime)s - %(name)s - [%(levelname)s] - %(message)s")

        # Add the self.formatter to the console handler
        self.ch.setFormatter(self.formatter)

        # Add the console handler to the self.logger
        self.logger.addHandler(self.ch)
        self.logger.setLevel(logging.TRACE)

        print(f'Thread started, listening on : tcp://{self.host}:{self.port}')
        while True:
            # Receive message
            self.serialized_message = self.subscriber.recv()
            self.msgCnt = self.msgCnt+1
            # Deserialize message
            self.message = daoLogging_pb2.LogMessage()
            self.message.ParseFromString(self.serialized_message)
            timestamp_float = float(self.message.time_stamp)
            timestamp_datetime = datetime.fromtimestamp(timestamp_float)
            readable_string = timestamp_datetime.strftime('%Y-%m-%d %H:%M:%S.%f')
            stringMsg = f'[{readable_string}] - {self.message.component_name} [{self.message.level}] : {self.message.log_message}'
            # Check if display_messages flag is set
            if self.display_messages:
                # Log the self.message
                if self.message.level == logging.TRACE:
                    self.logger.log(logging.TRACE, stringMsg)
                elif self.message.level == logging.DEBUG:
                    self.logger.log(logging.DEBUG, stringMsg)
                elif self.message.level == logging.INFO:
                    self.logger.log(logging.INFO, stringMsg)
                elif self.message.level == logging.WARNING:
                    self.logger.log(logging.WARNING, stringMsg)
                elif self.message.level == logging.ERROR:
                    self.logger.log(logging.ERROR, stringMsg)
                elif self.message.level == logging.FATAL:
                    self.logger.log(logging.FATAL, stringMsg)

if __name__=="__main__":

    logger = daoLog("test")
    print(f"PROCCESS_NAME: {PROCESS_NAME}")
    log = logging.getLogger(PROCESS_NAME)

    log.trace("This is a trace message")
    log.debug("This is a debug message")
    log.info("This is a info message")
    log.warning("This is a warning message")
    log.error("This is a error message ")
    log.critical("This is a critical message")

    # # Create an instance of the ZmqSubLogger
    # zmq_sub_logger = ZmqSubLogger()
    # # Start the thread
    # zmq_sub_logger.start()