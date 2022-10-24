import sys, signal
import daoLogging_pb2
import zmq
import random

def getEventString():
    tmpString = ""
    return tmpString

def process_log(file, log_message):

    # TimeStamp + machine + component + level + event/error code + string
    String = f"[{log_message.logs[0].time_stamp}] - {log_message.logs[0].machine}({log_message.logs[0].component_name}) @Frame:{log_message.logs[0].frame_number} : "
    if(log_message.HasField("level") ):
        String+=f"{log_message.EVENT.Name(log_message.logs[0].event)}"
    else:
        String+=f"{log_message.logs[0].level}"
    
    String+=f" - {log_message.logs[0].payload}"

    String+="\n"
    print(String)
    file.write(String)

    return

if __name__=="__main__":
    log_path = '/tmp'
    log_file = 'hrtc.logs'
    protocal = 'tcp'
    ip = '127.0.0.1'
    port = 5555
    
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    connect_string = protocal + '://'+ ip + ':' + str(5555)
    print(f"Connecting to : {connect_string}")
    socket.connect (connect_string)


    logs = daoLogging_pb2.Logs()
    log_file = open(log_path +'/' + log_file, 'a')

    # This will hold the file comp names and the files in memory
    # log_files = {}
    # subcribe to all data:
    socket.subscribe('')
    try:
        i = 0
        tv = 0
        while(True):
            # print("Waiting to receive")
            recv_message = socket.recv()
            print(f'Received Message: {i}')
            print(f'length: {recv_message.size()}')
            # logs.ParseFromString(recv_message)
            # process_log(log_file, logs)
            i+=1

    except KeyboardInterrupt:
        print("")
        print("Keyboard Interupt caught, exiting program")

    print("Starting clean up")
    log_file.close()