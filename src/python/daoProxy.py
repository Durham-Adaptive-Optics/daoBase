#!/usr/bin/env python3
import zmq

class daoProxy:
    def __init__(self, xsubPort=5558, xpubPort=5559):
        self.xpubPort = xpubPort
        self.xsubPort = xsubPort
        # Create a context
        self.context = zmq.Context()

        # Create an XPUB socket for the server to receive messages from clients
        self.xpub_socket = self.context.socket(zmq.XPUB)
        self.xpub_socket.bind("tcp://*:"+str(xpubPort))

        # Create an XSUB socket for the clients to send messages to the server
        self.xsub_socket = self.context.socket(zmq.XSUB)
        self.xsub_socket.bind("tcp://*:"+str(xsubPort))

        print(f'Starting a proxy server port {self.xsubPort} to {self.xpubPort}')
        # Create a proxy to forward messages between the XPUB and XSUB sockets
        zmq.proxy(self.xsub_socket, self.xpub_socket)


if __name__=="__main__":
    proxy = daoProxy()