#!/usr/bin/env python3

'''
Read/Write access to SHM. Tjhe old interface is still available as shmOld object if needed
'''

import os, sys, mmap, struct
isWindows = True if (sys.platform == 'win32') else False

import numpy as np
import astropy.io.fits as pf
import time
#import pdb
if not isWindows:
    import posix_ipc
from threading import Thread
from threading import Event
import zmq
import datetime
import Pyro4
import json
import marshal

# Set Pyro to use JSON serialization instead of pickle
Pyro4.config.SERIALIZER = "marshal"
Pyro4.config.SERIALIZERS_ACCEPTED = {"marshal"}

#
# ctypes interface for dao
#
import ctypes
import logging
import daoLog
# Load the shared library

if isWindows:
    # TODO - Add proper path
    daoLibPath = os.path.join(os.getenv('DAOROOT'), 'lib', 'dao-0.dll')
    daoLib = ctypes.WinDLL(daoLibPath)
else:
    daoLib = ctypes.CDLL('libdao.so')

logging.TRACE = 5
logging.addLevelName(logging.TRACE, "TRACE")

if isWindows:
    # TODO - Add proper temporary path
    logFile = "daolog.txt"
else:
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

class Complex64(ctypes.Structure):
    _fields_ = [("real", ctypes.c_float), ("imag", ctypes.c_float)]

class Complex128(ctypes.Structure):
    _fields_ = [("real", ctypes.c_double), ("imag", ctypes.c_double)]

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
        np.float64: ctypes.c_double,
        np.complex64: Complex64,
        np.complex128: Complex128
    }.get(npType)

    return ctypesType

def npType2DaoType(npArray):
    npType = npArray.dtype.type
    daoType = {
        np.uint8: 1,
        np.int8: 2,
        np.uint16: 3,
        np.int16: 4,
        np.uint32: 5,
        np.int32: 6,
        np.uint64: 7,
        np.int64: 8,
        np.float32: 9,
        np.float64: 10,
        np.complex64: 11,
        np.complex128: 12
    }.get(npType)

    return daoType

def daoType2NpType(daoType):
    npType = {
        1: np.uint8,
        2: np.int8,
        3: np.uint16,
        4: np.int16,
        5: np.uint32,
        6: np.int32,
        7: np.uint64,
        8: np.int64,
        9: np.float32,
        10: np.float64,
        11: np.complex64,
        12: np.complex128
    }.get(daoType)

    return npType

def daoType2CtypesType(daoType):
    ctypesType = {
        1: ctypes.c_uint8,
        2: ctypes.c_int8,
        3: ctypes.c_uint16,
        4: ctypes.c_int16,
        5: ctypes.c_uint32,
        6: ctypes.c_int32,
        7: ctypes.c_uint64,
        8: ctypes.c_int64,
        9: ctypes.c_float,
        10: ctypes.c_double,
        11: Complex64,
        12: Complex128
    }.get(daoType)

    return ctypesType

# Define the struct timespec structure
class timespec(ctypes.Structure):
    _fields_ = [
        ('tv_sec', ctypes.c_int64),
        ('tv_nsec', ctypes.c_int64)
    ]

class TIMESPECFIXED(ctypes.Structure):
    _fields_ = [
        ('firstlong', ctypes.c_int64),
        ('secondlong', ctypes.c_int64)
    ]

class IMAGE_KEYWORD(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 16),
        ("type", ctypes.c_char),
        ("value", ctypes.c_double),  # Use the largest data type to accommodate all possible values
        ("comment", ctypes.c_char * 80)
    ]

# Define the IMAGE_METADATA structure
class IMAGE_METADATA(ctypes.Structure):
    class ATIME(ctypes.Union):
        _fields_ = [
            ("ts", timespec),
            ("tsfixed", TIMESPECFIXED)
        ]

    _fields_ = [
        ("name", ctypes.c_char * 80),
        ("naxis", ctypes.c_uint8),
        ("size", ctypes.c_uint32 * 3),
        ("nelement", ctypes.c_uint64),
        ("atype", ctypes.c_uint8),
        ("creation_time", ctypes.c_double),
        ("last_access", ctypes.c_double),
        ("atime", ATIME),
        ("shared", ctypes.c_uint8),
        ("status", ctypes.c_uint8),
        ("logflag", ctypes.c_uint8),
        ("sem", ctypes.c_uint16),
        ("cnt0", ctypes.c_uint64),
        ("cnt1", ctypes.c_uint64),
        ("cnt2", ctypes.c_uint64),
        ("write", ctypes.c_uint8),
        ("NBkw", ctypes.c_uint16),
        ("lastPos", ctypes.c_uint32),
        ("lastNb", ctypes.c_uint32),
        ("packetNb", ctypes.c_uint32),
        ("packetTotal", ctypes.c_uint32),
        ("lastNbArray", ctypes.c_uint64 * 512)
    ]

if isWindows:
    # Define the IMAGE structure
    class IMAGE(ctypes.Structure):
    #    # Define the nested structure for the 'array' union
    #    class ArrayUnion(ctypes.Union):
    #        _fields_ = [
    #            ('UI8', ctypes.POINTER(ctypes.c_uint8)),
    #            ('SI8', ctypes.POINTER(ctypes.c_int8)),
    #            ('UI16', ctypes.POINTER(ctypes.c_uint16)),
    #            ('SI16', ctypes.POINTER(ctypes.c_int16)),
    #            ('UI32', ctypes.POINTER(ctypes.c_uint32)),
    #            ('SI32', ctypes.POINTER(ctypes.c_int32)),
    #            ('UI64', ctypes.POINTER(ctypes.c_uint64)),
    #            ('SI64', ctypes.POINTER(ctypes.c_int64)),
    #            ('F', ctypes.POINTER(ctypes.c_float)),
    #            ('D', ctypes.POINTER(ctypes.c_double)),
    #            # Add more fields for other data types if needed
    #        ]
            
        _fields_ = [
            ('name', ctypes.c_char * 80),
            ('used', ctypes.c_uint8),
            ('shmfd', ctypes.POINTER(ctypes.c_void_p)),
            ('memsize', ctypes.c_uint64),
            ('semlog', ctypes.POINTER(ctypes.c_void_p)),
            ('md', ctypes.POINTER(IMAGE_METADATA)),
    #        ('_pad', ctypes.c_uint64),  # Pad to align to 8-byte boundary
            ('array', ctypes.c_void_p),
            ('semptr', ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))),
            ('kw', ctypes.POINTER(IMAGE_KEYWORD)),
            ('semReadPID', ctypes.POINTER(ctypes.c_int32)),
            ('semWritePID', ctypes.POINTER(ctypes.c_int32)),
            ('shmfm', ctypes.POINTER(ctypes.c_void_p))
        ]
else:
    # Define the IMAGE structure
    class IMAGE(ctypes.Structure):
    #    # Define the nested structure for the 'array' union
    #    class ArrayUnion(ctypes.Union):
    #        _fields_ = [
    #            ('UI8', ctypes.POINTER(ctypes.c_uint8)),
    #            ('SI8', ctypes.POINTER(ctypes.c_int8)),
    #            ('UI16', ctypes.POINTER(ctypes.c_uint16)),
    #            ('SI16', ctypes.POINTER(ctypes.c_int16)),
    #            ('UI32', ctypes.POINTER(ctypes.c_uint32)),
    #            ('SI32', ctypes.POINTER(ctypes.c_int32)),
    #            ('UI64', ctypes.POINTER(ctypes.c_uint64)),
    #            ('SI64', ctypes.POINTER(ctypes.c_int64)),
    #            ('F', ctypes.POINTER(ctypes.c_float)),
    #            ('D', ctypes.POINTER(ctypes.c_double)),
    #            # Add more fields for other data types if needed
    #        ]
            
        _fields_ = [
            ('name', ctypes.c_char * 80),
            ('used', ctypes.c_uint8),
            ('shmfd', ctypes.c_int32),
            ('memsize', ctypes.c_uint64),
            ('semlog', ctypes.POINTER(ctypes.c_void_p)),
            ('md', ctypes.POINTER(IMAGE_METADATA)),
    #        ('_pad', ctypes.c_uint64),  # Pad to align to 8-byte boundary
            ('array', ctypes.c_void_p),
            ('semptr', ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))),
            ('kw', ctypes.POINTER(IMAGE_KEYWORD)),
            ('semReadPID', ctypes.POINTER(ctypes.c_int32)),
            ('semWritePID', ctypes.POINTER(ctypes.c_int32))
        ]

def serialize_numpy_to_marshal(arr):
    # Convert numpy array to a marshal serializable format
    marshal_data = {
        'data': arr.tolist(),    # Convert array data to a list
        'shape': arr.shape,      # Save the shape of the array
        'dtype': str(arr.dtype)  # Save the data type of the array (e.g., uint16)
    }
    return marshal.dumps(marshal_data)

def deserialize_numpy_from_marshal(marshal_data):
    # Load marshal string into a dictionary
    data_dict = marshal.loads(marshal_data)
    
    # Rebuild the numpy array from the list, shape, and dtype
    return np.array(data_dict['data'], dtype=data_dict['dtype']).reshape(data_dict['shape'])


@Pyro4.expose
class shm:
    def __init__(self, fname=None, data=None, nbkw=0, pubPort=5555, subPort=5555, subHost='localhost', remote=False, host='localhost', sync=True):
        self.remote = remote
        self.sync = sync
        self.host = host
        if self.remote == True:
            # Locate the Pyro nameserver and look up the object by the name "shared_memory"
            self.ns = Pyro4.locateNS(host=host)
            self.uri = self.ns.lookup(fname)
            self.proxyGet = Pyro4.Proxy(self.uri)
            self.proxySet = Pyro4.Proxy(self.uri)
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
        #                              uint8_t atype, int shared, int NBkw);
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
            ctypes.c_int32,
            ctypes.c_int32
        ]
        self.daoShmWaitForSemaphore.restype = ctypes.c_int8

        self.daoShmWaitForCounter = daoLib.daoShmWaitForCounter
        self.daoShmWaitForCounter.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmWaitForCounter.restype = ctypes.c_int8

        self.image=IMAGE()
        if self.remote:
            data = self.proxyGet.get_data(serialized=True)
            data = deserialize_numpy_from_marshal(data)
        if fname == '':
            log.error("Need at least a SHM name")
        elif data is not None:
            log.info("%s will be created or overwritten" % (fname,))
            dataSize = data.shape
            self.daoShmImageCreate(ctypes.byref(self.image), fname.encode('utf-8'), len(dataSize),\
                                   (ctypes.c_uint32 * len(dataSize))(*dataSize),\
                                   npType2DaoType(data), 1, 0)
            if data.flags['C_CONTIGUOUS']:
                cData = data.ctypes.data_as(ctypes.c_void_p)
            else:
                cData = np.ascontiguousarray(data).ctypes.data_as(ctypes.c_void_p)
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
        self.lastRemoteWriteCounter = self.get_counter()
        #self.subThread.start()
        if self.remote and self.sync:
            # Create and start the sync thread
            self.syncGetThread = Thread(target=self.syncGet)
            self.syncGetThread.daemon = True  # This ensures the thread will exit when the main program does
            self.syncGetThreadRun = True
            self.syncGetThread.start()
            self.syncGetThreadCounter = 0
            self.syncPutThread = Thread(target=self.syncPut)
            self.syncPutThread.daemon = True  # This ensures the thread will exit when the main program does
            self.syncPutThreadRun = True
            self.syncPutThread.start()
            self.syncPutThreadCounter = 0

    def syncPut(self):
        while self.syncPutThreadRun:
            data = self.get_data(check=True, semNb=9)
            if data is not None and self.get_counter() > self.lastRemoteWriteCounter:
                self.proxySet.set_data(serialize_numpy_to_marshal(data), serialized=True)
                self.lastRemoteWriteCounter = self.get_counter()
            self.syncPutThreadCounter += 1

    def syncGet(self):
        while self.syncGetThreadRun:
            data = self.proxyGet.get_data(check=True, semNb=9, serialized=True, timeout=1)
            if data is not None and self.lastRemoteWriteCounter == self.get_counter():
                data = deserialize_numpy_from_marshal(data)
                self.set_data(data, sync=False)
                self.lastRemoteWriteCounter = self.get_counter()
            self.syncGetThreadCounter += 1

    def set_data(self, data, serialized=False, sync=True):
        ''' --------------------------------------------------------------
        Upload new data to the SHM file.

        Parameters:
        ----------
        - data: the array to upload to SHM
        - check_dt: boolean (default: false) recasts data
        '''
        if self.remote and not self.sync:
            self.proxySet.set_data(serialize_numpy_to_marshal(data), serialized=True)
            return
        # if the data passed are list, convert it to numpy array
        if serialized==True:
            data = deserialize_numpy_from_marshal(data)
        # Call the daoShmImage2Shm function to feel the SHM
        if data.flags['C_CONTIGUOUS']:
            cData = data.ctypes.data_as(ctypes.c_void_p)
        else:
            cData = np.ascontiguousarray(data).ctypes.data_as(ctypes.c_void_p)

        nbVal = ctypes.c_uint32(data.size)
        result = self.daoShmImage2Shm(cData, nbVal, ctypes.byref(self.image))
        # once it is written, automatically update the remote one if sync enabled
        if self.remote and sync:
            self.proxySet.set_data(serialize_numpy_to_marshal(data), serialized=True)
        
    def get_data(self, check=False, reform=True, semNb=0, timeout=0, spin=False, serialized=False):
        ''' --------------------------------------------------------------
        Reads and returns the data part of the SHM file

        Parameters:
        ----------
        - check: integer (last index) if not False, waits image update
        - reform: boolean, if True, reshapes the array in a 2-3D format
        -------------------------------------------------------------- '''
        if self.remote and not self.sync:
            data = self.proxyGet.get_data(check=check, reform=reform, semNb=semNb, timeout=timeout, spin=spin, serialized=True)
            return deserialize_numpy_from_marshal(data)

        if check == True:
            if spin == True:
                result = self.daoShmWaitForCounter(ctypes.byref(self.image))
            else:
                result = self.daoShmWaitForSemaphore(ctypes.byref(self.image), semNb, timeout)
                if result != 0:
                    return None

        arraySize = np.ctypeslib.as_array(ctypes.cast(self.image.md.contents.size,\
                                                      ctypes.POINTER(ctypes.c_uint32)), shape=(3,))

        arrayPtr = ctypes.cast(self.image.array,\
                               ctypes.POINTER(daoType2CtypesType(self.image.md.contents.atype)))
        #data=np.ctypeslib.as_array(arrayPtr, shape=(self.image.md.contents.nelement,)).astype(daoType2NpType(self.image.md.contents.atype))
        if arraySize[2] == 0:
            if arraySize[1] == 0:
                data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0],))#.astype(daoType2NpType(self.image.md.contents.atype))
            else:
                data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0], arraySize[1]))#.astype(daoType2NpType(self.image.md.contents.atype))
        else:
            data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0], arraySize[1], arraySize[2]))#.astype(daoType2NpType(self.image.md.contents.atype))

        # Check if the dtype is structured (i.e., for complex types)
        if data.dtype.fields is not None and 'real' in data.dtype.fields and 'imag' in data.dtype.fields:
            # Reconstruct complex array by combining real and imaginary parts
            data = data['real'] + 1j * data['imag']

        # Cast to the desired NumPy type (e.g., complex64, complex128, or float, or...)
        data = data.astype(daoType2NpType(self.image.md.contents.atype))

        # if the data are asked as list, convert the numpy array
        if serialized==True:
            data = serialize_numpy_to_marshal(data)

        return data

    def get_meta_data(self):
        ''' --------------------------------------------------------------
        Get the metadata fraction of the SHM file.
        Populate the shm object mtdata dictionary.

        Parameters:
        ----------
        - verbose: (boolean, default: True), prints its findings
        -------------------------------------------------------------- '''
        #if self.remote:
        #    self.mtdata = self.proxyGet.get_meta_data()
        #else:
        md=self.image.md.contents
        self.mtdata=struct2Dict(md)
    
        #decode time
        # it will replace the C timestamp to something readble
        mdt=self.mtdata['atime']
        mdts=struct2Dict(mdt)
    
        mdt1=mdts['ts']
        mdt1s=struct2Dict(mdt1)
        mdts['ts'] = mdt1s
    
        mdt2=mdts['tsfixed']
        mdt2s=struct2Dict(mdt2)
        mdts['tsfixed'] = mdt2s
    
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
        tv_sec = self.mtdata['atime']['ts']['tv_sec']
        tv_nsec = self.mtdata['atime']['ts']['tv_nsec']

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




