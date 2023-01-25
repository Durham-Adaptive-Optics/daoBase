#!/usr/bin/env python3
import zmq
import daoLogging_pb2 as daoLogging
from datetime import datetime
import time
import random
import string

def get_random_string(length):
    # choose from all lowercase letter
    letters = string.ascii_lowercase
    result_str = ''.join(random.choice(letters) for i in range(length))
    print("Random string of length", length, "is:", result_str)
    return result_str

name = get_random_string(8)

# Create a context
context = zmq.Context()

# Connect to the server's XSUB socket
socket = context.socket(zmq.XPUB)
socket.connect("tcp://localhost:5558")

def generateMessage(msg):
    # Send a message to the server
    message = daoLogging.LogMessage()
    message.component_name = name
    message.time_stamp = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
    message.log_message = msg

    serialized_message = message.SerializeToString()
    socket.send(serialized_message)

# Close the socket
#socket.close()
