#!/usr/bin/env python3

import unittest
import threading
import time
from daoEvent import EventManager,EventReceiver,daoEvent_pb2

class TestEventManager(unittest.TestCase):
    def setUp(self):
        # Create an instance of the EventManager before each test
        self.event_manager = EventManager()

    def tearDown(self):
        # Delete the EventManager instance after each test
        del self.event_manager

    def test_send_event(self):
        # Create an instance of the EventReceiver
        event_receiver = EventReceiver()
        # Start a new thread to listen for events
        threading.Thread(target=event_receiver.listen_for_events).start()
        # Create an event to be sent
        event = daoEvent_pb2.Event(name='example', data='example data')
        # Send the event
        self.event_manager.send_event(event)
        # Sleep for 1 sec to wait for the event to be processed
        time.sleep(2)
        # Assert that the received event has the expected name and data
        print(event_receiver.event.name)
        print(event_receiver.event.data)
        self.assertEqual(event_receiver.event.name, 'example')
        self.assertEqual(event_receiver.event.data, 'example data')

if __name__ == '__main__':
    unittest.main()

