#!/usr/bin/env python3

'''
Read/Write access to SHM. Tjhe old interface is still available as shmOld object if needed
'''

import os, sys, mmap, struct
import numpy as np
import astropy.io.fits as pf
import time
#import pdb
import posix_ipc
from threading import Thread
from threading import Event
import zmq
import datetime

#
# ctypes interface for dao
#
import ctypes
import logging
import daoLog
# Load the shared library
daoLib = ctypes.CDLL('libdao.so')

logging.TRACE = 5
logging.addLevelName(logging.TRACE, "TRACE")

logFile = "/tmp/daolog.txt"
ip = '127.0.0.1'
port = 5558
addr=f"tcp://{ip}:{port}"
logger = daoLog.daoLog(__name__, filename=logFile)
log = logging.getLogger(__name__)

def struct2Dict(structure):
    result = {}
    for field in structure._fields_:
        field_name = field[0]
        field_value = getattr(structure, field_name)
        
        if isinstance(field_value, ctypes.Array):
            # Convert array to a list
            field_value = list(field_value)
            
        result[field_name] = field_value
        
    return result

def npType2CtypesType(npArray):
    npType = npArray.dtype.type
    ctypesType = {
        np.int8: ctypes.c_int8,
        np.uint8: ctypes.c_uint8,
        np.int16: ctypes.c_int16,
        np.uint16: ctypes.c_uint16,
        np.int32: ctypes.c_int32,
        np.uint32: ctypes.c_uint32,
        np.int64: ctypes.c_int64,
        np.uint64: ctypes.c_uint64,
        np.float32: ctypes.c_float,
        np.float64: ctypes.c_double
    }.get(npType)

    return ctypesType

def npType2DaoType(npArray):
    npType = npArray.dtype.type
    daoType = {
        np.int8: 1,
        np.uint8: 2,
        np.int16: 3,
        np.uint16: 4,
        np.int32: 5,
        np.uint32: 6,
        np.int64: 7,
        np.uint64: 8,
        np.float32: 9,
        np.float64: 10
    }.get(npType)

    return daoType

def daoType2NpType(daoType):
    npType = {
        1: np.int8,
        2: np.uint8,
        3: np.int16,
        4: np.uint16,
        5: np.int32,
        6: np.uint32,
        7: np.int64,
        8: np.uint64,
        9: np.float32,
        10: np.float64
    }.get(daoType)

    return npType

def daoType2CtypesType(daoType):
    ctypesType = {
        1: ctypes.c_int8,
        2: ctypes.c_uint8,
        3: ctypes.c_int16,
        4: ctypes.c_uint16,
        5: ctypes.c_int32,
        6: ctypes.c_uint32,
        7: ctypes.c_int64,
        8: ctypes.c_uint64,
        9: ctypes.c_float,
        10: ctypes.c_double
    }.get(daoType)

    return ctypesType

# Define the struct timespec structure
class timespec(ctypes.Structure):
    _fields_ = [
        ('tv_sec', ctypes.c_long),
        ('tv_nsec', ctypes.c_long)
    ]

class TIMESPECFIXED(ctypes.Structure):
    _fields_ = [
        ('firstlong', ctypes.c_long),
        ('secondlong', ctypes.c_long)
    ]

class IMAGE_KEYWORD(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 16),
        ("type", ctypes.c_char),
        ("value", ctypes.c_double),  # Use the largest data type to accommodate all possible values
        ("cnt", ctypes.c_uint64),
        ("comment", ctypes.c_char * 80)
    ]

# Define the IMAGE_METADATA structure
class IMAGE_METADATA(ctypes.Structure):
    _fields_ = [
        ("version", ctypes.c_char * 32),
        ("name", ctypes.c_char * 80),
        ("naxis", ctypes.c_uint8),
        ("size", ctypes.c_uint32 * 3),
        ("nelement", ctypes.c_uint64),
        ("datatype", ctypes.c_uint8),
        ("imagetype", ctypes.c_uint64),
        ("creationtime", timespec),
        ("lastaccesstime", timespec),
        ("atime", timespec),
        ("writetime", timespec),
        ("creatorPID", ctypes.c_uint),# pid_t
        ("ownerPID", ctypes.c_uint),# pid_t
        ("shared", ctypes.c_uint8),
        ("inode", ctypes.c_ulong),# ino_t
        ("location", ctypes.c_int8),
        ("status", ctypes.c_uint8),
        ("flag", ctypes.c_uint64),
        ("logflag", ctypes.c_uint8),
        ("sem", ctypes.c_uint16),
        ("NBproctrace", ctypes.c_uint16),
        ("cnt0", ctypes.c_uint64),
        ("cnt1", ctypes.c_uint64),
        ("cnt2", ctypes.c_uint64),
        ("write", ctypes.c_uint8),
        ("NBkw", ctypes.c_uint16),
        ("CBsize", ctypes.c_uint32),
        ("CBsize", ctypes.c_uint32),
        ("CBindex", ctypes.c_uint32),
        ("CBcycle", ctypes.c_uint32),
        ("imdatamemsize", ctypes.c_uint64),
        ("cudaMemHandle", ctypes.c_char * 64),# cudaIpcMemHandle_t
        ("lastNb", ctypes.c_uint32),
        ("packetNb", ctypes.c_uint32),
        ("packetTotal", ctypes.c_uint32),
        ("lastNbArray", ctypes.c_uint64 * 512)
    ]

# Defining the STREAM_PROC_TRACE structure
class STREAM_PROC_TRACE(ctypes.Structure):
    _fields_ = [
        ("triggermode", ctypes.c_int),
        ("procwrite_PID", ctypes.c_uint),
        ("trigger_inode", ctypes.c_ulong),
        ("ts_procstart", timespec),
        ("ts_streamupdate", timespec),
        ("trigsemindex", ctypes.c_int),
        ("triggerstatus", ctypes.c_int),
        ("cnt0", ctypes.c_uint64)
    ]

# Define the SEMFILEDATA structure
class SEMFILEDATA(ctypes.Structure):
    _fields_ = [
        ("semdata", ctypes.c_byte * 32) # sem_t
    ]

# Defining the CBFRAMEMD structure
class CBFRAMEMD(ctypes.Structure):
    _fields_ = [
        ("cnt0", ctypes.c_uint64),      # 64-bit unsigned int
        ("cnt1", ctypes.c_uint64),      # 64-bit unsigned int
        ("atime", timespec),            # timespec structure for atime
        ("writetime", timespec)         # timespec structure for writetime
    ]

# Define the IMAGE structure
class IMAGE(ctypes.Structure):
    _fields_ = [
        ('name', ctypes.c_char * 80),
        ('used', ctypes.c_uint8),
        ('createcnt', ctypes.c_int64),
        ('shmfd', ctypes.c_int32),
        ('memsize', ctypes.c_uint64),
        ('semlog', ctypes.POINTER(ctypes.c_void_p)),
        ('md', ctypes.POINTER(IMAGE_METADATA)),
#        ('_pad', ctypes.c_uint64),  # Pad to align to 8-byte boundary
        ('array', ctypes.c_void_p),
        ('semptr', ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))),
        ('kw', ctypes.POINTER(IMAGE_KEYWORD)),
        ('semfile', ctypes.POINTER(SEMFILEDATA)),
        ('semReadPID', ctypes.POINTER(ctypes.c_int)),
        ('semWritePID', ctypes.POINTER(ctypes.c_int)),
        ('semctrl', ctypes.POINTER(ctypes.c_int32)),
        ('semstatus', ctypes.POINTER(ctypes.c_int32)),
        ('streamproctrace', ctypes.POINTER(STREAM_PROC_TRACE)),
        ('flagarray', ctypes.POINTER(ctypes.c_int64)),
        ('cntarray', ctypes.POINTER(ctypes.c_int64)),
        ("atimearray", ctypes.POINTER(timespec)),
        ("writetimearray", ctypes.POINTER(timespec)),
        ("CircBuff_md", ctypes.POINTER(CBFRAMEMD)),
        ('CMimdata', ctypes.c_void_p),
    ]

class shm:
    def __init__(self, fname=None, data=None, nbkw=0, pubPort=5555, subPort=5555, subHost='localhost'):
        # int8_t daoShmInit1D(const char *name, char *prefix, uint32_t nbVal, IMAGE **image);
        self.daoShmInit1D = daoLib.daoShmInit1D
        self.daoShmInit1D.argtypes = [
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_char),
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.POINTER(IMAGE))
        ]
        self.daoShmInit1D.restype = ctypes.c_int8

        # int8_t daoShmShm2Img(const char *name, char *prefix, IMAGE *image);
        self.daoShmShm2Img = daoLib.daoShmShm2Img
        self.daoShmShm2Img.argtypes = [
            ctypes.c_char_p,
            ctypes.POINTER(IMAGE)
        ]
        self.daoShmShm2Img.restype = ctypes.c_int8

        # int8_t daoShmImage2Shm(void *procim, uint32_t nbVal, IMAGE *image);
        self.daoShmImage2Shm = daoLib.daoShmImage2Shm
        self.daoShmImage2Shm.argtypes = [
            ctypes.c_void_p,
            ctypes.c_uint32,
            ctypes.POINTER(IMAGE)
        ]
        self.daoShmImage2Shm.restype = ctypes.c_int8

        # int8_t daoShmImagePart2Shm(char *procim, uint32_t nbVal, IMAGE *image, uint32_t position,
        #                             uint16_t packetId, uint16_t packetTotal, uint64_t frameNumber);
        self.daoShmImagePart2Shm = daoLib.daoShmImagePart2Shm
        self.daoShmImagePart2Shm.argtypes = [
            ctypes.POINTER(ctypes.c_char),
            ctypes.c_uint32,
            ctypes.POINTER(IMAGE),
            ctypes.c_uint32,
            ctypes.c_uint16,
            ctypes.c_uint16,
            ctypes.c_uint64
        ]
        self.daoShmImagePart2Shm.restype = ctypes.c_int8

        # int8_t daoShmImagePart2ShmFinalize(IMAGE *image);
        self.daoShmImagePart2ShmFinalize = daoLib.daoShmImagePart2ShmFinalize
        self.daoShmImagePart2ShmFinalize.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmImagePart2ShmFinalize.restype = ctypes.c_int8

        # int8_t daoShmImageCreate(IMAGE *image, const char *name, long naxis, uint32_t *size,
        #                              uint8_t datatype, int shared, int NBkw);
        self.daoShmImageCreate = daoLib.daoShmImageCreate
        self.daoShmImageCreate.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_char_p,
            ctypes.c_long,
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.c_uint8,
            ctypes.c_int,
            ctypes.c_int
        ]
        self.daoShmImageCreate.restype = ctypes.c_int8

        # int8_t daoShmCombineShm2Shm(IMAGE **imageCude, IMAGE *image, int nbChannel, int nbVal);
        self.daoShmCombineShm2Shm = daoLib.daoShmCombineShm2Shm
        self.daoShmCombineShm2Shm.argtypes = [
            ctypes.POINTER(ctypes.POINTER(IMAGE)),
            ctypes.POINTER(IMAGE),
            ctypes.c_int,
            ctypes.c_int
        ]
        self.daoShmCombineShm2Shm.restype = ctypes.c_int8

        # uint64_t daoShmGetCounter(IMAGE *image);
        self.daoShmGetCounter = daoLib.daoShmGetCounter
        self.daoShmGetCounter.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmGetCounter.restype = ctypes.c_uint64

        self.daoShmWaitForSemaphore = daoLib.daoShmWaitForSemaphore
        self.daoShmWaitForSemaphore.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_int32
        ]
        self.daoShmWaitForSemaphore.restype = ctypes.c_int8

        self.daoShmWaitForCounter = daoLib.daoShmWaitForCounter
        self.daoShmWaitForCounter.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmWaitForCounter.restype = ctypes.c_int8

        self.image=IMAGE()
        if fname == '':
            log.error("Need at least a SHM name")
        elif data is not None:
            log.info("%s will be created or overwritten" % (fname,))
            dataSize = data.shape
            self.daoShmImageCreate(ctypes.byref(self.image), fname.encode('utf-8'), 2,\
                                   (ctypes.c_uint32 * len(dataSize))(*dataSize),\
                                   npType2DaoType(data), 1, 0)
            cData = data.ctypes.data_as(ctypes.c_void_p)
            nbVal = ctypes.c_uint32(data.size)
            # Call the daoShmImage2Shm function to feel the SHM
            result = self.daoShmImage2Shm(cData, nbVal, ctypes.byref(self.image))
        else:
            log.info("loading existing %s " % (fname))
            result = self.daoShmShm2Img(fname.encode('utf-8'), ctypes.byref(self.image))
        # Publisher
        self.pubPort = pubPort
        self.pubContext = zmq.Context()
        
        self.pubEvent = Event()
        self.pubThread = Thread(target = self.publish)
        self.pubEnable = False
        #self.pubThread.start()
        # Subscriber
        self.subPort = subPort
        self.subHost = subHost
        self.subContext = zmq.Context()
        self.subEvent = Event()
        self.subThread = Thread(target = self.subscribe)
        self.subEnable = False
        #self.subThread.start()

    def set_data(self, data):
        ''' --------------------------------------------------------------
        Upload new data to the SHM file.

        Parameters:
        ----------
        - data: the array to upload to SHM
        - check_dt: boolean (default: false) recasts data
        '''
        # Call the daoShmImage2Shm function to feel the SHM
        cData = data.ctypes.data_as(ctypes.c_void_p)
        nbVal = ctypes.c_uint32(data.size)
        result = self.daoShmImage2Shm(cData, nbVal, ctypes.byref(self.image))
        
    def get_data(self, check=False, reform=True, semNb=0, timeout=0, spin=False):
        ''' --------------------------------------------------------------
        Reads and returns the data part of the SHM file

        Parameters:
        ----------
        - check: integer (last index) if not False, waits image update
        - reform: boolean, if True, reshapes the array in a 2-3D format
        -------------------------------------------------------------- '''
        if check == True:
            if spin == True:
                result = self.daoShmWaitForCounter(ctypes.byref(self.image))
            else:
                result = self.daoShmWaitForSemaphore(ctypes.byref(self.image), semNb)

        arraySize = np.ctypeslib.as_array(ctypes.cast(self.image.md.contents.size,\
                                                      ctypes.POINTER(ctypes.c_uint32)), shape=(3,))

        arrayPtr = ctypes.cast(self.image.array,\
                               ctypes.POINTER(daoType2CtypesType(self.image.md.contents.datatype)))
        #data=np.ctypeslib.as_array(arrayPtr, shape=(self.image.md.contents.nelement,)).astype(daoType2NpType(self.image.md.contents.datatype))
        data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0], arraySize[1])).astype(daoType2NpType(self.image.md.contents.datatype))
        return data

    def get_meta_data(self):
        ''' --------------------------------------------------------------
        Get the metadata fraction of the SHM file.
        Populate the shm object mtdata dictionary.

        Parameters:
        ----------
        - verbose: (boolean, default: True), prints its findings
        -------------------------------------------------------------- '''
        md=self.image.md.contents
        self.mtdata=struct2Dict(md)

        #decode time
        # it will replace the C timestamp to something readble
        mdt=self.mtdata['atime']
        mdts=struct2Dict(mdt)
        self.mtdata['atime'] = mdts

        return self.mtdata

    def get_counter(self,):
        ''' --------------------------------------------------------------
        Read the image counter from SHM
        -------------------------------------------------------------- '''
        return self.get_meta_data()['cnt0']

    def get_frame_id(self,):
        ''' --------------------------------------------------------------
        Read the image counter from SHM
        -------------------------------------------------------------- '''
        return self.get_meta_data()['cnt2']

    def get_timestamp(self, ):
        md=self.get_meta_data()
        tv_sec = self.mtdata['atime']['tv_sec']
        tv_nsec = self.mtdata['atime']['tv_nsec']

        # now I have tv_sec and tv_nsec we convert to a datetime
        return datetime.datetime.fromtimestamp(tv_sec) + datetime.timedelta(microseconds=tv_nsec/1000)

    def publish(self):
        self.pubSocket = self.pubContext.socket(zmq.PUB)
        self.pubSocket.bind("tcp://*:%d"%(self.pubPort))
        self.pubThreadCounter = 0
        while True:
            if self.pubEnable:
                topic = 'frameData'
                self.pubSocket.send_string(topic, zmq.SNDMORE)
                self.pubSocket.send_pyobj(self.get_data())
            time.sleep(0.1)
            if self.pubEvent.is_set():
                break
    
    def subscribe(self):
        self.subSocket = self.subContext.socket(zmq.SUB)
        self.subSocket.connect("tcp://%s:%d"%(self.subHost, self.subPort))
        self.subSocket.setsockopt(zmq.SUBSCRIBE, b'frameData')
        self.subSocket.setsockopt(zmq.CONFLATE, 1)
        topic = 'frameData'
        while True:
            if self.subEnable:
                topic = self.subSocket.recv_string()
                frameData = self.subSocket.recv_pyobj()
                self.set_data(frameData)
            if self.subEvent.is_set():
                break

        