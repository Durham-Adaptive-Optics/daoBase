# daoBase
basic tools for dao: shm, logging, error,...

# Prerequiries
waf needs to be installed and in the path: see https://waf.io/book/

# Environment
```
export DAOROOT=$HOME/DAOROOT
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DAOROOT/lib:$DAOROOT/lib64
export PATH=$PATH:$HOME/DAOROOT/bin

```
!! BE SURE THE DAOROOT exists. In the example
```
mkdir $HOME/DAOROOT
```
# Build
```
waf configure --prefix=$DAOROOT
waf
wad install
```
# Apps
the apps folder provides basic examples.
## Building shared memory
A python code is provided to create all the shared memory used by the example
```
daoExampleBuildShm.py # TO BE IMPLEMENTED.....
```
It is possible to create just the shared memory for an example. See examples below
## staring a simple clock
The first example is a simple C code increasing a counter in the shared memory.
This software clock can be used to synchrnized different program using built-in semaphore in the DAO SHM
### create SHM
```
import shmlib
import numpy as np
clockShm=shmlib.shm('/tmp/demoClockShm.im.shm',np.zeros((1,1)).astype(np.uint32))
``` 
### run example
Now let's run the clock at 1.5kHx
```
daoClock -L demoClockShm 1500
```
## writing data in shared memory
This example is a C program writing in the shared memory at a specific rate.
The program is waiting for a new value in another shared memory. We can use the previous example and use the clock as the trigger
### create SHM
```
import shmlib
import numpy as np
shm=shmlib.shm('/tmp/demoShm.im.shm',np.zeros((100,100)).astype(np.float32))
clockShm=shmlib.shm('/tmp/demoClockShm.im.shm',np.zeros((1,1)).astype(np.uint32))
``` 
### run example
Now run the program writing at the in the share memory random values
```
daoRandWriter -L demoShm demoClockShm
```