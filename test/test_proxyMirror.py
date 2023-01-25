import zmq
import daoLogging_pb2 as daoLogging # import the generated protobuf module

context = zmq.Context()

# Connect to the proxy server's XPUB socket
socket = context.socket(zmq.SUB)
socket.connect("tcp://localhost:5559")

# Subscribe to all messages
socket.setsockopt_string(zmq.SUBSCRIBE, "")

while True:
    # Receive message from the proxy
    serialized_message = socket.recv()
    # Deserialize message
    message = daoLogging.LogMessage()
    message.ParseFromString(serialized_message)
    print(f'{message.time_stamp} [{message.level}] - {message.component_name} - {message.log_message}')

# Close the socket
socket.close()