#!/usr/bin/env python3
import zmq
from threading import Thread, Event
from

class daoProxy:
    def __init__(self, xsubPort=5558, xpubPort=5559, capPort=5560, logfile='/tmp/daoLogs.txt'):
        self.xpubPort = xpubPort
        self.xsubPort = xsubPort
        self.capPort  = capPort

        self.capThread = Thread(target=self.capture)
        self.capThread_event = Event()
        # Create a context
        self.context = zmq.Context()

        # Create an XPUB socket for the server to receive messages from clients
        self.xpub_socket = self.context.socket(zmq.XPUB)
        self.xpub_socket.bind(f"tcp://*:{xpubPort}")

        # Create an XSUB socket for the clients to send messages to the server
        self.xsub_socket = self.context.socket(zmq.XSUB)
        self.xsub_socket.bind(f"tcp://*:{xsubPort}")

        # Create an XSUB socket for the clients to send messages to the server
        self.capPub_socket = self.context.socket(zmq.XPUB)
        self.capPub_socket.bind(f"tcp://*:{capPort}")

        # Create an XSUB socket for the clients to send messages to the server
        self.capSub_socket = self.context.socket(zmq.SUB)
        self.capSub_socket.connect(f"tcp://*:{capPort}")
        self.capSub_socket.subscribe('')

        # open logfile and append

        print(f'Starting a proxy server port {self.xsubPort} to {self.xpubPort}')
        # Create a proxy to forward messages between the XPUB and XSUB sockets
        zmq.proxy(self.xsub_socket, self.xpub_socket)

    def capture(self):
        running = True:
        while running:
            message = self.capSub_socket.recv()



if __name__=="__main__":
    proxy = daoProxy()