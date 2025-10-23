import zmq
import time
import threading
import daoEvent_pb2

class EventManager:
    def __init__(self):
        # Create a new ZeroMQ context
        self.context = zmq.Context()
        # Create a new PUB socket
        self.socket = self.context.socket(zmq.PUB)
        # Bind the socket to the network
        self.socket.bind("tcp://*:5555")

    def send_event(self, event):
        print(event)
        # Serialize the event using Protocol Buffers
        event_bytes = event.SerializeToString()
        # Send the serialized event over the network
        self.socket.send(event_bytes)

class EventReceiver:
    def __init__(self):
        # Create a new ZeroMQ context
        self.context = zmq.Context()
        # Create a new SUB socket
        self.socket = self.context.socket(zmq.SUB)
        # Connect the socket to the network
        self.socket.connect("tcp://localhost:5555")
        # Subscribe to all events
        self.socket.setsockopt(zmq.SUBSCRIBE, b'')
        self.event = daoEvent_pb2.Event() 

    def listen_for_events(self):
        while True:
            # Receive an event from the network
            event_bytes = self.socket.recv()
            # Deserialize the event using Protocol Buffers
            self.event = daoEvent_pb2.Event()
            self.event.ParseFromString(event_bytes)
            # Handle the event here
            print(event)

if __name__ == '__main__':
    event_manager = EventManager()
    event_receiver = EventReceiver()
    # Start a new thread to listen for events
    threading.Thread(target=event_receiver.listen_for_events).start()

    # Send an example event
    event = daoEvent_pb2.Event(name='example', data='example data')
    event_manager.send_event(event)
