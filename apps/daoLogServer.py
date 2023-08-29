#! /usr/bin/env python
import zmq
import signal
import sys
import configparser
import os
import daoLogging_pb2
import threading
import logging 
from logging.handlers import TimedRotatingFileHandler

class daoLogServer:
    def __init__(self):
        print("daoLogServer starting...")
        d = os.getenv("DAODATA")
        print(f'DAODATA = {d}') 
        self.filename = os.getenv("DAODATA") + "/config/daoLogServer.conf"
        print(f"Opening file: {self.filename}")
        self.config = self.read_config(self.filename)

        #get config
        self.log_file  = self.config["General"]["log_file"]
        self.recv_port = self.config['General']['port']
        self.send_ip   = 'localhost'
        self.send_port = 4116

        if self.config.has_option('General', 'send_ip'):
            self.send_ip = self.config['General']['send_ip']

        if self.config.has_option('General', 'send_port'):
            self.send_port = int(self.config['General']['send_port'])
        
        if f"tcp://*:{self.recv_port}" == f"tcp://{self.send_ip}:{self.send_port}":
            self.send_port+=1

        self.recv    = f"tcp://*:{self.recv_port}"
        self.send    = f"tcp://{self.send_ip}:{self.send_port}"
        self.capture = f"tcp://127.0.0.1:{self.send_port+1}"

        # Configure logging
        backups = int(self.config["General"]["backups"])
        interval = self.config["General"]["interval"]
        when = self.config["General"]["when"]

        self.log_formatter = logging.Formatter('%(message)s')

        # destination_dir = os.path.join(os.path.expanduser("~"), self.filename)
        self.log_handler = TimedRotatingFileHandler(self.log_file, when=when, interval=1, backupCount=backups)
        self.log_handler.setFormatter(self.log_formatter)
        self.logger = logging.getLogger('my_logger')
        self.logger.setLevel(logging.DEBUG)
        self.logger.addHandler(self.log_handler)


        print(f"Receive on: {self.recv}")
        print(f"Send on:    {self.send}")
        print(f"Capture on: {self.capture}")
        print(f"log file:   {self.log_file}")

        self.context = zmq.Context()

        # receive port
        self.recv_socket = self.context.socket(zmq.SUB)
        self.recv_socket.bind(self.recv)
        self.recv_socket.setsockopt_string(zmq.SUBSCRIBE, '')

        #send port
        self.send_socket = self.context.socket(zmq.PUB)
        self.send_socket.connect(self.send)

        #capture
        self.capture_socket = self.context.socket(zmq.PUB)
        self.capture_socket.bind(self.capture)


        self.capture_socket2 = self.context.socket(zmq.SUB)
        self.capture_socket2.connect(self.capture)
        self.capture_socket2.setsockopt_string(zmq.SUBSCRIBE, '')

        self.running = True
        self.capture_thread_instance = threading.Thread(target=self.capture_thread)
        self.capture_thread_instance.start()

        # Register the signal handler for the desired signal (e.g., SIGINT or SIGTERM)
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)

        try:
            proxy = zmq.proxy(self.recv_socket, self.send_socket, self.capture_socket)
        except SystemExit:
            pass 
        finally:
                # clean up
            print("Waiting for thread to join")
            self.capture_thread_instance.join()
            print("Closing sockets")
            self.recv_socket.close()
            self.send_socket.close()
            self.capture_socket.close()
            self.capture_socket2.close()
            print("Destroying context")
            self.context.term()
        print("Exiting")


    def read_config(self, config_path):
        config = configparser.ConfigParser()
        config.read(config_path)
        return config

    def getLevelName(self, level):
        if level == daoLogging_pb2.LogMessage.NOSET:
            return "NOSET"
        elif level == daoLogging_pb2.LogMessage.TRACE:
            return "TRACE"
        elif level == daoLogging_pb2.LogMessage.DEBUG:
            return "DEBUG"
        elif level == daoLogging_pb2.LogMessage.INFO:
            return "INFO"
        elif level == daoLogging_pb2.LogMessage.WARNING:
            return "WARNING"
        elif level == daoLogging_pb2.LogMessage.ERROR:
            return "ERROR"
        elif level == daoLogging_pb2.LogMessage.CRITICAL:
            return "CRITICAL"
        else:
            return "?????"

    def format_log_message(self, log_message):
        log_format = '[%(asctime)s] [%(machine)s] - %(name)s [%(levelname)s] : %(message)s'
        
        formatted_log = log_format % {
            'asctime': log_message.time_stamp,
            'machine': log_message.machine,
            'name': log_message.component_name,
            'levelname': self.getLevelName(log_message.level),
            'message': log_message.log_message,
        }
        return formatted_log

    def capture_thread(self,):
        poller = zmq.Poller()
        poller.register(self.capture_socket2, zmq.POLLIN)
        log_msg = daoLogging_pb2.LogMessage()

        while self.running:
            socks = dict(poller.poll(1000))    
            if self.capture_socket2 in socks and socks[self.capture_socket2] == zmq.POLLIN:
                # Handle captured messages
                data = self.capture_socket2.recv()
                log_msg.ParseFromString(data)
                self.logger.info(self.format_log_message(log_msg))

    # Define a signal handler function
    def signal_handler(self,signum, frame): 
        self.running = False
        print("Received signal, exiting gracefully...")
        raise SystemExit


def main():
    server = daoLogServer()

if __name__ == "__main__":
    main()