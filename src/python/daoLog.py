import socket
from datetime import datetime
import logging

# this is the network logger. Maybe we can encapsulate this better. 
import zmq
import daoLogging_pb2
import time


class daoLog(logging.Logger):
    TRACE = 5
    def __init__(self, name, level=TRACE, ip=None, port=None, machine=None, logfile=None):
        super().__init__(name)
        logging.addLevelName(self.TRACE, "TRACE")
        self.setLevel(level)
        self.propagate = True
        handler = logging.StreamHandler()
        handler.setLevel(level)
        
        self.ip = ip
        self.port = port
        self.machine = machine
        if self.machine is None:
            self.machine = socket.gethostname()

        formatter = logging.Formatter('[%(asctime)s] %(machine)s - %(name)s [%(levelname)s] : %(message)s')
        handler.setFormatter(formatter)
        handler.addFilter(ContextFilter(self.machine))
        self.addHandler(handler)

        if(self.port is None or self.ip is None or self.machine is None):
            pass
        else:
            print("d")
            zmqHandle = daoLogZmqHandler(ip=self.ip, port=self.port, machine=self.machine)
            self.addHandler(zmqHandle)
            
        if logfile is not None:
            file_handler = logging.FileHandler(file)
            file_handler.addFilter(ContextFilter(self.machine))
            file_handler.setFormatter(formatter)
            self.addHandler(file_handler)
        
        logging.Logger.trace = self.trace

    def trace(self, msg, *args, **kwargs):
        if self.isEnabledFor(self.TRACE):
            self._log(self.TRACE, msg, args, **kwargs)

class daoLogZmqHandler(logging.Handler):
    def __init__(self, ip, port, machine):
        super().__init__()
        self.ip = ip
        self.port = port
        self.machine = machine
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PUB)
        self.socket.connect(f"tcp://{self.ip}:{self.port}")

    def emit(self, record):
        log_message = daoLogging_pb2.Logs()
        log_mess = log_message.logs.add()
        log_mess.level = record.levelname
        log_mess.log_message = record.msg
        log_mess.time_stamp = str(record.created)
        log_mess.component_name = record.name
        log_mess.machine = record.machine
        self.socket.send(log_message.SerializeToString())


class ContextFilter(logging.Filter):
    def __init__(self, machine:str):
        super().__init__()
        self.machine = machine
    def filter(self, record):
        record.machine = self.machine
        return True

if __name__=="__main__":
    file = "log.txt"
    ip = '127.0.0.1'
    port = 5555
    log = daoLog(__name__, logfile=file ,port=port, ip=ip)

    log.trace("This is a trace message")
    log.debug("This is a debug message")
    log.info("This is a info message")
    log.warning("This is a warning message")
    log.error("This is a error message ")
    log.critical("This is a critical message")