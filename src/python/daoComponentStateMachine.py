from statemachine import StateMachine, State

import logging
import daoLog

class daoComponentStateMachine(StateMachine):
    def __init__(self, name):
        super().__init__()
        self.name = name
        self.log = logging.getLogger(self.name)

    log = logging.getLogger()
    "dao Base statemachine"
    Off = State('Off', initial=True)
    Standby = State('Standby')
    Idle = State('Idle')
    Running = State('Running')
    Error = State('Error')


    Init = Off.to(Standby)
    Stop = Standby.to(Off)

    Enable = Standby.to(Idle)
    Disable = Idle.to(Standby)

    Run = Idle.to(Running)
    StopRun = Running.to(Idle)

    Err = Running.to(Error)
    Err2 = Idle.to(Error)
    Recover = Error.to(Idle)

    # all these functions can be overwritten
    def on_Init(self):
        self.log.trace('Init')

    def on_Stop(self):
        self.log.trace('Stop')

    def on_Enable(self):
        self.log.trace('Enable')

    def on_Disable(self):
        self.log.trace('Disable')

    def on_Run(self):
        self.log.trace('Run')

    def on_StopRun(self):
        self.log.trace('StopRun')
    
    def on_Err(self):
        self.log.trace('Err')

    def on_Err2(self):
        self.log.trace('Err2')
    
    def on_Recover(self):
        self.log.trace('Recover')

# Entry exit function
    def on_enter_Off(self):
        self.log.trace('enter Off!')

    def on_exit_Off(self):
        self.log.trace('exit Off!')

    def on_enter_Standby(self):
        self.log.trace('enter Standby!')
        
    def on_exit_Standby(self):
        self.log.trace('exit Standby!')

    def on_enter_Idle(self):
        self.log.trace('enter Idle!')
        
    def on_exit_Idle(self):
        self.log.trace('exit Idle!')

    def on_enter_Running(self):
        self.log.trace('enter Running!')
        
    def on_exit_Running(self):
        self.log.trace('exit Running!')

    def on_enter_Error(self):
        self.log.trace('enter Error!')
        
    def on_exit_Error(self):
        self.log.trace('exit Error!')

if __name__=="__main__":
    name = "test"
    log = daoLog.daoLog(name)
    stm = daoComponentStateMachine(name)
    
    stm.Init()
    stm.Enable()
    stm.Run()
    stm.StopRun()
    stm.Disable()
    stm.Stop()

