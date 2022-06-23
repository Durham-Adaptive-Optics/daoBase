#! /usr/bin/env python
#
# Python script to build SHM for the dao examples
#

import numpy as np
import shmlib

import shmlib
import numpy as np
shm=shmlib.shm('/tmp/demoShm.im.shm',np.zeros((100,100)).astype(np.float32))
clockShm=shmlib.shm('/tmp/demoClockShm.im.shm',np.zeros((1,1)).astype(np.uint32))