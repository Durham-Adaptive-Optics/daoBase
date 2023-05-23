from daoComponentStateMachine import daoComponentStateMachine
import logging
import daoLog
from threading import Thread
from threading import Event
import time
import yaml
import daoCommand_pb2  # protobuf
import zmq
import statemachine
import os

class daoComponent(daoComponentStateMachine):
    def __init__(self, name=__name__, config=None, port=5556, goRunning=True):
        super(daoComponentStateMachine, self).__init__(self, name)
        self.name = name

        self.log = logging.getLogger(name)
        self.log.info("Created")
        if config is not None:
            self.config = config

        self.request_stop = False
        self.port=port
        self.commandThread = Thread(target = self.zmqThread)
        self.stopEvent = Event()
        self.commandThread.start()

        self.config = config
        self.procThread = Thread(target=self.processThreadContainer)
        self.procThreadStop = Event()

        self.update_thread = Thread(target=self.updateThread)
        self.update_threadStopEvent = Event()
        self.update_map_list = []

        self.variable_dictionary = {}

        if(goRunning):
            self.Init()
            self.Enable()
            self.Run()

    def stop(self):
            if self.update_thread.is_alive():
                self.update_threadStopEvent.set()
            self.stopEvent.set()
            self.commandThread.join()

    def on_Init(self):
        try:
            self.load_static_config()
        except Exception as e:
            self.log.critical(str(e))
            exit()
        return 
    
    def on_Enable(self):
        try:
            self.load_dynamic_config()
        except Exception as e:
            self.log.critical(str(e))
            exit()

        if self.update_map_list:
            self.update_thread = Thread(target=self.updateThread)
            self.update_threadStopEvent = Event()
            self.update_thread.start()
        else:
            self.log.info("Update Map list empty so update_thread not started.")

        return 

    def on_Run(self):
        self.log.trace("On_Run: starting processing thread")
        self.procThread = Thread(target=self.processThreadContainer)
        self.procThreadStop = Event()
        self.procThread.start()
        return
    
    def on_StopRun(self):
        self.log.trace("On_StopRun: ")
        self.procThreadStop.set()
        self.procThread.join()
        return

    def load_static_config(self):
        return
    
    def load_dynamic_config(self):
        return

    def bring_online(self):
        return

    # here we can allow the user to do processing nothing special
    def processThreadContainer(self):
        self.processingThread()
    

    def processingThread(self):
        pass

        # to be overloaded by user
    def user_dump():
        pass

        # must return 
        # True/False for success and
        # string
    def user_process_other(self, string):
        return True, string

    def process_COMMAND(self, message):
        command = daoCommand_pb2.CommandMessage()
        command.ParseFromString(message)
        # disentagle command.
        status = daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS')
        payload = "Message"
        if(command.function == daoCommand_pb2.CommandMessage.COMMAND.Value('EXEC')):
            [status, payload] = self.process_EXEC(command.payload)
        elif(command.function == daoCommand_pb2.CommandMessage.COMMAND.Value('SETUP')):
            status = daoCommand_pb2.ReplyMessage.RETURN.Value('FAILURE')
            payload = "Function not implimented"
        elif(command.function == daoCommand_pb2.CommandMessage.COMMAND.Value('UPDATE')):
            status = daoCommand_pb2.ReplyMessage.RETURN.Value('FAILURE')
            payload = "Function not implimented"
        elif(command.function == daoCommand_pb2.CommandMessage.COMMAND.Value('PING')):
            [status, payload] = self.process_PING()
        elif(command.function == daoCommand_pb2.CommandMessage.COMMAND.Value('STATE')):
            [status, payload] = self.process_STATE()
        elif(command.function == daoCommand_pb2.CommandMessage.COMMAND.Value('SET_LOG_LEVEL')):
            [status, payload] = self.process_SET_LOG_LEVEL(command.payload)
        elif(command.function == daoCommand_pb2.CommandMessage.COMMAND.Value('DUMP')):
            [status, payload] = self.process_DUMP()
        elif(command.function == daoCommand_pb2.CommandMessage.COMMAND.Value('OTHER')):
            [status, payload] = self.process_OTHER(command.payload)
        elif(command.function == daoCommand_pb2.CommandMessage.COMMAND.Value('QUERY')):
            [status, payload] = self.process_QUERY(command.payload)
        else:
            status = daoCommand_pb2.ReplyMessage.RETURN.Value('FAILURE')
            payload = f"Unknown command: {command.function}"

        return self.construct_reply(status, payload)

    def construct_reply(self, status, payload):
        Reply = daoCommand_pb2.ReplyMessage()
        Reply.status = status
        if(payload is not None):
            Reply.payload = payload
        return Reply.SerializeToString()

    def process_EXEC(self, stateChange):
        try:
            if (stateChange == "Init"):
                self.Init()        
            elif (stateChange == "Stop"):
                self.Stop()
            elif (stateChange == "Enable"):
                self.Enable()
            elif (stateChange == "Disable"):
                self.Disable()
            elif (stateChange == "Run"):
                self.Run()
            elif (stateChange == "Idle"):
                self.StopRun()
            elif (stateChange == "Recover"):
                self.Recover()     
            else:
                self.log.warning(f"Failed to change state unknown transition {stateChange}")
                return daoCommand_pb2.ReplyMessage.RETURN.Value('FAILURE'), f"Failed to change state unknown transition {stateChange}"  
        except statemachine.exceptions.TransitionNotAllowed:
            warning = f"transition not alloweds in current state: [{self.current_state.value} : {stateChange}]"
            self.log.warning(warning)
            return daoCommand_pb2.ReplyMessage.RETURN.Value('FAILURE'), warning

        return daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS'), None
    
    def process_PING(self):
        payload = str(os.getpid())
        status  = daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS')
        self.log.trace(f"PID: {payload}")
        return status, payload
    
    def process_STATE(self):
        payload = self.current_state_value
        status  = daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS')
        self.log.trace(f"STATE: {payload}")
        return status, payload

    def process_SET_LOG_LEVEL(self, level_string):
        if(level_string == "TRACE"):
            self.log.setLevel(level_string)
            return daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS'), None
        elif(level_string == "DEBUG"):
            self.log.setLevel(level_string)
            return daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS'), None
        elif(level_string == "INFO"):
            self.log.setLevel(level_string)
            return daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS'), None
        elif(level_string == "WARNING"):
            self.log.setLevel(level_string)
            return daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS'), None
        elif(level_string == "ERROR"):
            self.log.setLevel(level_string)
            return daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS'), None
        elif(level_string == "CRITICAL"):
            self.log.setLevel(level_string)
            return daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS'), None
        else:
            self.log.warning("Unkown log level cannot set")
            return daoCommand_pb2.ReplyMessage.RETURN.Value('FAILURE'), f"Failed to set unknown log level: {level_string}"
 
    def process_DUMP(self):
            payload = self.user_dump()
            status = daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS')
            return status, payload

    def process_OTHER(self, string):
            status, payload = self.user_process_other(string)
            if(status):
                status = daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS')
            else:
                status = daoCommand_pb2.ReplyMessage.RETURN.Value('FAILURE')
            return status, payload

    def process_QUERY(self,key):
        if key in self.variable_dictionary:
            self.log.trace(self.variable_dictionary[key])
            payload = str(self.variable_dictionary[key])
            status = daoCommand_pb2.ReplyMessage.RETURN.Value('SUCCESS')
        else:
            status = daoCommand_pb2.ReplyMessage.RETURN.Value('FAILURE')
            payload = f"Unknown variable name: {key}"
        return status, payload


    def zmqThread(self):
        self.log.trace("ZMQ Thread started")
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        self.socket.setsockopt(zmq.RCVTIMEO, 20) # will return after 200 ms
        connect = "tcp://*:%s" % self.port
        self.log.trace("Binding to: %s" % connect)
        self.socket.bind(connect)
        Running = True
        while Running == True:
            try: 
                message = self.socket.recv()

                reply = self.process_COMMAND(message)
                self.socket.send(reply)
   
            except zmq.ZMQError as e:
                if e.errno == zmq.EAGAIN:
                    if self.stopEvent.is_set():
                        self.log.trace("stop detected returning")
                        Running = False
                    pass # no message was ready (yet!)
                else:
                    self.log.error(e)
        return

    def add_update_map(self, shm_file, double_buffer):
        self.update_map_list.append([0, shm_file, double_buffer])
        

    def updateThread(self):
        counters = []
        for i in range(len(self.update_map_list)):
            self.update_map_list[i][0] = self.update_map_list[i][1].get_counter()
            self.update_map_list[i][2].write(self.update_map_list[i][1].get_data())

        running = True

        while running:
            # check each shm in list and update if new available
            for i in range(len(self.update_map_list)):
                if(self.update_map_list[i][1].get_counter() > self.update_map_list[i][0]):
                    self.log.debug(f"UpdateThread: new map available [{i}]")
                    self.update_map_list[i][0] = self.update_map_list[i][1].get_counter()
                    self.update_map_list[i][2].write(self.update_map_list[i][1].get_data())

            if(self.update_threadStopEvent.isSet()):
                running = False
        return

if __name__=="__main__":
    name = "test"
    log = daoLog.daoLog(name)
    # log.setLevel(logging.INFO)
    A = daoComponent(name=name)
    while True:
        try:
            time.sleep(1)
        except KeyboardInterrupt:
            print('interrupted!')
            A.stop()
            break
        

