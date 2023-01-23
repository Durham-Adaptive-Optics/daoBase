import socket
from datetime import datetime

# this is the network logger. Maybe we can encapsulate this better. 
import zmq
import daoLogging_pb2


class daoLog:
    def __init__(self, name, dst="SCREEN", level="TRACE", ip=None, port=None, filename=None):
        self.name = name
        self.hostname = socket.gethostname()
        self.dst = dst
        if(dst == "SCREEN"):
            pass
        if(dst == "FILE"):
            # open file
            self.file = open(self.filename, "a")

        if(dst == "NETWORK"):
            self.ip = ip
            self.port = port
            connect_string = f'tcp://{ip}:{port}'
            print(f"Connecting to : {connect_string}")
            self.network_context = zmq.Context()
            self.network_socket = self.network_context.socket(zmq.PUB)
            self.network_socket.connect(connect_string)
            # create a empty log_message that is configured at will
            self.network_log_message = daoLogging_pb2.Logs()
            self.network_log_mess = self.network_log_message.logs.add()
            self.network_log_mess.component_name = self.name
            self.network_log_mess.machine = self.hostname
        self.level = self.LevelStringToValue(level)


        # configure depending dst
    def log(self, string, level):
        timestamp =datetime.now()
        ts= timestamp.strftime("%d-%b-%Y (%H:%M:%S.%f)")
        tmp = f"[{self.hostname}] {ts} [{self.name}] [{level}] - {string}"
        if(self.dst == "SCREEN"):
            print(tmp)
        elif(self.dst == "FILE"):
            self.write(tmp)
        elif(self.dst == "NETWORK"):
            self.writeToNetwork(string, level, ts)


    def Trace(self, string):
        if(self.level <= 0):
            self.log(string, "TRACE")
    
    def Debug(self, string):
        if(self.level <= 1):
            self.log(string, "DEBUG")

    def Info(self, string):
        if(self.level <= 2):
            self.log(string,"INFO")

    def Warning(self, string):
        if(self.level <= 3):
            self.log(string, "WARNING")

    def Error(self, string):
        if(self.level <= 4):
            self.log(string, "ERROR")

    def Fatal(self, string):
        if(self.level <= 5):
            self.log(string, "FATAL")

    def setLogLevel(self, level):
        self.level = self.LevelStringToValue(level)
        self.Debug(f"Log level set to {self.LevelValueToString(self.level)}")

    def LevelStringToValue(self, levelString):
        if(levelString == "TRACE"):
            return 0
        elif(levelString == "DEBUG"):
            return 1
        elif(levelString == "INFO"):
            return 2
        elif(levelString == "WARNING"):
            return 3
        elif(levelString == "ERROR"):
            return 4
        elif(levelString == "FATAL"):
            return 5
    

    def LevelValueToString(self, value):
        if(value == 0):
            return "TRACE"
        elif(value == 1):
            return "DEBUG"
        elif(value == 2):
            return "INFO"
        elif(value == 3):
            return "WARNING"
        elif(value == 4):
            return "ERROR"
        elif(value == 5):
            return "FATAL"
        else:
            return "UNKNOWN LEVEL"

    def writeToNetwork(self, string, level, ts):
        self.network_log_mess.level = level
        self.network_log_mess.log_message = string
        self.network_log_mess.time_stamp = ts
        sendString = self.network_log_message.SerializeToString()
        self.network_socket.send(sendString)

if __name__=="__main__":
    logger = daoLog("test")
    logger.setLogLevel("INFO")
    logger.Trace("T")
    logger.Debug("T")
    logger.Info("T")
    logger.Warning("T")
    logger.Error("T")
    logger.Fatal("T")