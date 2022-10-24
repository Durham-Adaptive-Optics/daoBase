from sqlite3 import connect
import sys, signal
import daoLogging_pb2
import zmq
import time
import random
from datetime import datetime

if __name__=="__main__":

    # configure the zmq for Pub
    protocol= 'tcp'
    ip = '127.0.0.1'
    port="5555"

    connect_string = protocol + '://'+ ip + ':' + str(5555)
    print(f"Connecting to : {connect_string}")
    context = zmq.Context()
    socket = context.socket(zmq.PUB)

    socket.bind(connect_string)

    log_message = daoLogging_pb2.Logs()
    log = log_message.logs.add()
    i = 0
    
    log.component_name = 'Test_logger'
    log.machine = 'hrtc3'
    log.payload = "Some log message"
    log.level = log.WARNING
    try:
        while(True):
            # configure some sort log messages
            print(f"Sending message: {i}")

            log.time_stamp = datetime.now().strftime("%d-%b-%Y (%H:%M:%S.%f)")
            log.frame_number = i

            sendString = log_message.SerializeToString()
            socket.send(sendString)
            time.sleep(1)
            i+=1


    except KeyboardInterrupt:
        print("")
        print("Keyboard Interupt caught, exiting program")

    print("Starting clean up")
