#! /usr/bin/env python
#
# Python script to build SHM for the dao examples
#

import numpy as np
import shmlib

import shmlib
import numpy as np
shm=shmlib.shm('/tmp/demo.im.shm',np.zeros((100,100)).astype(np.float32))
clockShm=shmlib.shm('/tmp/demoClock.im.shm',np.zeros((1,1)).astype(np.uint32))

latency = shmlib.shm('/tmp/demoLatency.im.shm',\
                np.zeros((1, 1)).astype(np.float32))

latencyAvg = shmlib.shm('/tmp/demoLatencyAvg.im.shm',\
                np.zeros((1, 1)).astype(np.float32))

latencyRms = shmlib.shm('/tmp/demoLatencyRms.im.shm',\
                np.zeros((1, 1)).astype(np.float32))