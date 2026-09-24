#!/usr/bin/env python3

'''
Read/Write access to SHM. Tjhe old interface is still available as shmOld object if needed
'''
import os, sys, struct
import numpy as np
# import astropy.io.fits as pf
import time
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

logFile = "/tmp/daolog.txt"
if sys.platform == "linux" or sys.platform == "linux2":
    daoLib = ctypes.CDLL('libdao.so')
elif sys.platform == "darwin":
    daoroot = os.getenv('DAOROOT')
    daoLib = ctypes.CDLL(daoroot +'/lib/libdao.dylib')
elif sys.platform == "win32":
    # TODO - Add proper path
    daoLibPath = os.path.join(os.getenv('DAOROOT'), 'lib', 'dao-0.dll')
    daoLib = ctypes.WinDLL(daoLibPath)
    logFile = "daolog.txt"
else:
    raise Exception("Unsupported platform")

# Add function prototype for daoSetLogLevel
daoLib.daoSetLogLevel.argtypes = [ctypes.c_int]
daoLib.daoSetLogLevel.restype = None

# Add function prototype for daoGetLogLevel
daoLib.daoGetLogLevel.argtypes = []
daoLib.daoGetLogLevel.restype = ctypes.c_int

def setLogLevel(logLevel):
    """Set the log level for the DAO library.
    
    Args:
        logLevel (int): The log level to set (0-4)
    """
    daoLib.daoSetLogLevel(logLevel)

def getLogLevel():
    """Get the current log level for the DAO library.
    
    Returns:
        int: The current log level (0-4)
    """
    return daoLib.daoGetLogLevel()

logging.TRACE = 5
logging.addLevelName(logging.TRACE, "TRACE")


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

def make_timespec(seconds_float):
    sec = int(seconds_float)
    nsec = int((seconds_float - sec) * 1e9)
    return timespec(tv_sec=sec, tv_nsec=nsec)

def make_timespec_from_now(timeout_seconds):
    """
    Returns an absolute timespec 'now + timeout_seconds' for use with sem_timedwait.
    Works with CLOCK_REALTIME behavior.
    """
    now = time.time()
    future = now + timeout_seconds

    tv_sec = int(future)
    tv_nsec = int((future - tv_sec) * 1e9)

    return timespec(tv_sec=tv_sec, tv_nsec=tv_nsec)
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
if sys.platform == "darwin":
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
            ("lastNbArray", ctypes.c_uint64 * 2024),
            ("semCounter", ctypes.c_uint32 * 10),
            ("semLogCounter", ctypes.c_uint32),
            ("fifo_size", ctypes.c_uint32),
            ("fifo_last_written", ctypes.c_uint32),
            # GPU payload (daoShmCreateGpu), see dao.h
            ("gpu_magic", ctypes.c_uint32),
            ("gpu_device", ctypes.c_int32),
            ("gpu_uuid", ctypes.c_uint8 * 16),
            ("gpu_flags", ctypes.c_uint32),
            ("gpu_size", ctypes.c_uint64),
            ("gpu_id", ctypes.c_uint64)
        ]
else:
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
            ("lastNbArray", ctypes.c_uint64 * 2024),
            ("fifo_size", ctypes.c_uint32),
            ("fifo_last_written", ctypes.c_uint32),
            # GPU payload (daoShmCreateGpu), see dao.h
            ("gpu_magic", ctypes.c_uint32),
            ("gpu_device", ctypes.c_int32),
            ("gpu_uuid", ctypes.c_uint8 * 16),
            ("gpu_flags", ctypes.c_uint32),
            ("gpu_size", ctypes.c_uint64),
            ("gpu_id", ctypes.c_uint64)
        ]
    

if sys.platform == "win32":
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
            ('shmfm', ctypes.POINTER(ctypes.c_void_p)),
            ('fifo_last_read', ctypes.c_uint32),
            ('fifo_last_read_cnt0', ctypes.c_uint64),
            # GPU SHM: device pointer of the payload, private GPU state
            ('d_array', ctypes.c_void_p),
            ('gpu', ctypes.c_void_p)
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
            ('semWritePID', ctypes.POINTER(ctypes.c_int32)),
            ('fifo_last_read', ctypes.c_uint32),
            ('fifo_last_read_cnt0', ctypes.c_uint64),
            # GPU SHM: device pointer of the payload, private GPU state
            ('d_array', ctypes.c_void_p),
            ('gpu', ctypes.c_void_p)
        ]

class shm:
    DAO_SUCCESS = 0
    DAO_ERROR = 1
    DAO_TIMEOUT = -1
    DAO_OVERWRITE = -2
    DAO_NOTREADY = -3

    def __init__(self, fname=None, data=None, nbkw=0, pubPort=5555, subPort=5555, subHost='localhost', logLevel=0, depth=1,
                 gpu=None, mirror=True):
        # gpu: CUDA device index to create a GPU SHM (payload on the GPU, see
        #      daoShmCreateGpu); only used with data. mirror: keep the /tmp host
        #      copy current, so CPU readers and tools keep working.
        # int8_t daoShmCreate1D(const char *name, uint32_t nbVal, IMAGE **image);
        self.daoShmCreate1D = daoLib.daoShmCreate1D
        self.daoShmCreate1D.argtypes = [
            ctypes.c_char_p,
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.POINTER(IMAGE))
        ]
        self.daoShmCreate1D.restype = ctypes.c_int8

        # set the log level
        setLogLevel(logLevel)

        # int8_t daoShmOpen(const char *name, IMAGE *image);
        self.daoShmOpen = daoLib.daoShmOpen
        self.daoShmOpen.argtypes = [
            ctypes.c_char_p,
            ctypes.POINTER(IMAGE)
        ]
        self.daoShmOpen.restype = ctypes.c_int8

        # int8_t daoShmSetData(IMAGE *image, void *im, uint32_t nbVal);
        self.daoShmSetData = daoLib.daoShmSetData
        self.daoShmSetData.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_void_p,
            ctypes.c_uint32
        ]
        self.daoShmSetData.restype = ctypes.c_int8

        # int8_t daoShmSetDataQuiet(IMAGE *image, void *im, uint32_t nbVal);
        self.daoShmSetDataQuiet = daoLib.daoShmSetDataQuiet
        self.daoShmSetDataQuiet.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_void_p,
            ctypes.c_uint32
        ]
        self.daoShmSetDataQuiet.restype = ctypes.c_int8

        # int8_t daoShmSetDataPart(IMAGE *image, char *im, uint32_t nbVal, uint32_t position,
        #                          uint16_t packetId, uint16_t packetTotal, uint64_t frameNumber);
        self.daoShmSetDataPart = daoLib.daoShmSetDataPart
        self.daoShmSetDataPart.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.POINTER(ctypes.c_char),
            ctypes.c_uint32,
            ctypes.c_uint32,
            ctypes.c_uint16,
            ctypes.c_uint16,
            ctypes.c_uint64
        ]
        self.daoShmSetDataPart.restype = ctypes.c_int8

        # int8_t daoShmSetDataPartFinalize(IMAGE *image);
        self.daoShmSetDataPartFinalize = daoLib.daoShmSetDataPartFinalize
        self.daoShmSetDataPartFinalize.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmSetDataPartFinalize.restype = ctypes.c_int8


        # int8_t daoShmCreateFifo(IMAGE *image, const char *name, long naxis, uint32_t *size,
        #                         uint8_t atype, int shared, int NBkw, uint32_t fifo_size);
        self.daoShmCreateFifo = daoLib.daoShmCreateFifo
        self.daoShmCreateFifo.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_char_p,
            ctypes.c_long,
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.c_uint8,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_uint32
        ]
        self.daoShmCreateFifo.restype = ctypes.c_int8

        # int8_t daoShmCreate(IMAGE *image, const char *name, long naxis, uint32_t *size,
        #                     uint8_t atype, int shared, int NBkw);
        self.daoShmCreate = daoLib.daoShmCreate
        self.daoShmCreate.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_char_p,
            ctypes.c_long,
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.c_uint8,
            ctypes.c_int,
            ctypes.c_int
        ]
        self.daoShmCreate.restype = ctypes.c_int8

        # int8_t daoShmCombine(IMAGE **imageCube, IMAGE *image, int nbChannel, int nbVal);
        self.daoShmCombine = daoLib.daoShmCombine
        self.daoShmCombine.argtypes = [
            ctypes.POINTER(ctypes.POINTER(IMAGE)),
            ctypes.POINTER(IMAGE),
            ctypes.c_int,
            ctypes.c_int
        ]
        self.daoShmCombine.restype = ctypes.c_int8

        # uint64_t daoShmGetCounter(IMAGE *image);
        self.daoShmGetCounter = daoLib.daoShmGetCounter
        self.daoShmGetCounter.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmGetCounter.restype = ctypes.c_uint64

        self.daoShmWaitSem = daoLib.daoShmWaitSem
        self.daoShmWaitSem.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_int32
        ]
        self.daoShmWaitSem.restype = ctypes.c_int8

        self.daoShmWaitSemTimeout = daoLib.daoShmWaitSemTimeout
        self.daoShmWaitSemTimeout.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_int32,
            ctypes.POINTER(timespec)
        ]
        self.daoShmWaitSemTimeout.restype = ctypes.c_int8

        self.daoShmWaitCounter = daoLib.daoShmWaitCounter
        self.daoShmWaitCounter.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmWaitCounter.restype = ctypes.c_int8

        # FIFO functions
        self.daoShmGetDataNext = daoLib.daoShmGetDataNext
        self.daoShmGetDataNext.argtypes = [ctypes.POINTER(IMAGE), ctypes.POINTER(ctypes.c_void_p),\
                                              ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint64)]
        self.daoShmGetDataNext.restype = ctypes.c_int8

        self.daoShmWaitData = daoLib.daoShmWaitData
        self.daoShmWaitData.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmWaitData.restype = ctypes.c_int8

        self.daoShmGetDataAt = daoLib.daoShmGetDataAt
        self.daoShmGetDataAt.argtypes = [ctypes.POINTER(IMAGE), ctypes.POINTER(ctypes.c_void_p), ctypes.c_uint32]
        self.daoShmGetDataAt.restype = ctypes.c_int8

        self.daoShmGetData = daoLib.daoShmGetData
        self.daoShmGetData.argtypes = [ctypes.POINTER(IMAGE), ctypes.POINTER(ctypes.c_void_p),\
                                              ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint64)]
        self.daoShmGetData.restype = ctypes.c_int8

        self.daoShmCheckOverwrite = daoLib.daoShmCheckOverwrite
        self.daoShmCheckOverwrite.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmCheckOverwrite.restype = ctypes.c_int8

        self.daoShmResetReadTail = daoLib.daoShmResetReadTail
        self.daoShmResetReadTail.argtypes = [ctypes.POINTER(IMAGE), ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint64)]
        self.daoShmResetReadTail.restype = ctypes.c_int8

        # int8_t daoShmClose(IMAGE *image);
        self.daoShmClose = daoLib.daoShmClose
        self.daoShmClose.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmClose.restype = ctypes.c_int8

        # GPU SHMs (daoGpu.c)
        self.daoShmCreateGpu = daoLib.daoShmCreateGpu
        self.daoShmCreateGpu.argtypes = [ctypes.POINTER(IMAGE), ctypes.c_char_p, ctypes.c_long,
                                         ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint8, ctypes.c_int,
                                         ctypes.c_int, ctypes.c_uint32]
        self.daoShmCreateGpu.restype = ctypes.c_int8
        self.daoShmIsGpu = daoLib.daoShmIsGpu
        self.daoShmIsGpu.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmIsGpu.restype = ctypes.c_int
        self.daoShmCommit = daoLib.daoShmCommit
        self.daoShmCommit.argtypes = [ctypes.POINTER(IMAGE), ctypes.c_void_p]
        self.daoShmCommit.restype = ctypes.c_int8
        self.daoShmCommitSync = daoLib.daoShmCommitSync
        self.daoShmCommitSync.argtypes = [ctypes.POINTER(IMAGE), ctypes.c_void_p]
        self.daoShmCommitSync.restype = ctypes.c_int8
        self.daoShmBeginWrite = daoLib.daoShmBeginWrite
        self.daoShmBeginWrite.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmBeginWrite.restype = ctypes.c_int8

        self.image=IMAGE()
        if fname == '':
            log.error("Need at least a SHM name")
        elif data is not None:
            if depth < 1:
                log.warning("Invalid depth passed (depth < 1). Forcing to 1...")
                depth = 1

            log.info("%s will be created or overwritten" % (fname,))
            dataSize = data.shape
            if gpu is not None:
                if depth != 1:
                    raise ValueError("daoShm.shm: FIFO (depth > 1) GPU SHMs are not supported")
                result = self.daoShmCreateGpu(ctypes.byref(self.image), fname.encode('utf-8'), len(dataSize),
                                              (ctypes.c_uint32 * len(dataSize))(*dataSize),
                                              npType2DaoType(data), int(gpu), nbkw,
                                              1 if mirror else 0)
                if result != self.DAO_SUCCESS:
                    raise OSError("daoShm.shm: failed to create GPU SHM '%s' on device %s" % (fname, gpu))
            else:
                self.daoShmCreateFifo(ctypes.byref(self.image), fname.encode('utf-8'), len(dataSize),\
                                (ctypes.c_uint32 * len(dataSize))(*dataSize),\
                                npType2DaoType(data), 1, 0, depth)
            if data.flags['C_CONTIGUOUS']:
                cData = data.ctypes.data_as(ctypes.c_void_p)
            else:
                cData = np.ascontiguousarray(data).ctypes.data_as(ctypes.c_void_p)
            nbVal = ctypes.c_uint32(data.size)
            # Call the daoShmImage2Shm function to feel the SHM
            result = self.daoShmSetData(ctypes.byref(self.image), cData, nbVal)
        else:
            # log.info("loading existing %s " % (fname))
            # Fail cleanly instead of letting the C layer mmap nothing and
            # segfault later on the first get_data()/set_data().
            if fname is None or not os.path.isfile(fname):
                raise FileNotFoundError(
                    "daoShm.shm: shared memory file '%s' does not exist" % (fname,))
            result = self.daoShmOpen(fname.encode('utf-8'), ctypes.byref(self.image))
            if result != self.DAO_SUCCESS:
                raise OSError(
                    "daoShm.shm: failed to load shared memory file '%s' "
                    "(dao error %d)" % (fname, result))
        # Cache the SHM element type and count once. atype/naxis/size/nelement are
        # written only at creation and never mutated, so set_data() can validate
        # incoming arrays against these cached ints for a few tens of ns instead
        # of walking the ctypes metadata struct on every call.
        self._cache_expected_type()
        # Publisher
        self.pubPort = pubPort
        self.pubContext = 0# zmq.Context()
        
        self.pubEvent = Event()
        self.pubThread = Thread(target = self.publish)
        self.pubEnable = False
        self.last_received_counter = 0  # Track last counter received from subscription
        #self.pubThread.start()
        # Subscriber
        self.subPort = subPort
        self.subHost = subHost
        self.subContext = 0 # zmq.Context()
        self.subEvent = Event()
        self.subThread = Thread(target = self.subscribe)
        self.subEnable = False
        #self.subThread.start()

    def _cache_expected_type(self):
        ''' --------------------------------------------------------------
        Cache the SHM element dtype (as np.dtype.num) and element count so
        set_data() can validate incoming arrays cheaply. Silently disables
        the check if the metadata cannot be read.
        '''
        self._expected_dtype_num = None
        self._expected_nelement = None
        try:
            md = self.image.md.contents
            npType = daoType2NpType(md.atype)
            if npType is not None:
                self._expected_dtype_num = np.dtype(npType).num
            naxis = md.naxis
            nelement = 1
            for i in range(naxis):
                nelement *= md.size[i]
            self._expected_nelement = int(nelement)
        except (ValueError, AttributeError):
            pass

    def set_data(self, data):
        ''' --------------------------------------------------------------
        Upload new data to the SHM file.

        Parameters:
        ----------
        - data: the array to upload to SHM
        '''
        # Cheap guard against a caller passing an array whose dtype or element
        # count does not match the SHM. The C layer trusts data.size as the
        # memcpy count and reads the element size from the SHM's own atype, so a
        # mismatch means silent corruption (wrong dtype) or a buffer overrun
        # (too many elements). Costs ~50 ns; the SHM type/geometry is immutable.
        if self._expected_dtype_num is not None \
                and data.dtype.num != self._expected_dtype_num:
            raise TypeError(
                "set_data: array dtype %s does not match SHM type (dao atype %d)"
                % (data.dtype, self.image.md.contents.atype))
        if self._expected_nelement is not None \
                and data.size != self._expected_nelement:
            raise ValueError(
                "set_data: array has %d elements but SHM expects %d"
                % (data.size, self._expected_nelement))

        # Call the daoShmImage2Shm function to feel the SHM
        if data.flags['C_CONTIGUOUS']:
            cData = data.ctypes.data_as(ctypes.c_void_p)
        else:
            cData = np.ascontiguousarray(data).ctypes.data_as(ctypes.c_void_p)

        nbVal = ctypes.c_uint32(data.size)
        result = self.daoShmSetData(ctypes.byref(self.image), cData, nbVal)

    def get_data_next(self, wait=False, reform=True, x=None, y=None):
        ''' --------------------------------------------------------------
        Reads and returns the next data part of the SHM file

        Parameters:
        ----------
        - wait: integer (last index) if not False, waits image update
        - reform: boolean, if True, reshapes the array in a 2-3D format
        - x, y: optional slice objects to extract only a sub-region of the
                image (e.g. x=slice(5,10), y=slice(10,15) for a 5x5 crop).
                Same convention as get_data.
        -------------------------------------------------------------- '''

        if wait == True:
            result = self.daoShmWaitData(ctypes.byref(self.image))
            if result != 0:
                log.error("Error waiting for counter")
                return None
        
        arrayPtr = ctypes.c_void_p(None)
        temp32 = ctypes.c_uint32()
        temp64 = ctypes.c_uint64()
        
        result = self.daoShmGetDataNext(ctypes.byref(self.image), ctypes.byref(arrayPtr),\
                                           ctypes.byref(temp32),  ctypes.byref(temp64))
        
        data = None
        
        if result == self.DAO_SUCCESS or result == self.DAO_OVERWRITE:
            arraySize = np.ctypeslib.as_array(ctypes.cast(self.image.md.contents.size,\
                                                        ctypes.POINTER(ctypes.c_uint32)), shape=(3,))

            arrayPtr = ctypes.cast(arrayPtr,\
                                ctypes.POINTER(daoType2CtypesType(self.image.md.contents.atype)))
            #data=np.ctypeslib.as_array(arrayPtr, shape=(self.image.md.contents.nelement,)).astype(daoType2NpType(self.image.md.contents.atype))
            if arraySize[2] == 0:
                if arraySize[1] == 0:
                    data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0],))#.astype(daoType2NpType(self.image.md.contents.atype))
                else:
                    data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0], arraySize[1]))#.astype(daoType2NpType(self.image.md.contents.atype))
            else:
                data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0], arraySize[1], arraySize[2]))#.astype(daoType2NpType(self.image.md.contents.atype))

            # Apply the requested ROI to the zero-copy view *before* the
            # complex-reconstruction/cast below, so only the sub-region ends
            # up being copied out of shared memory instead of the whole frame.
            if x is not None or y is not None:
                ySlice = y if y is not None else slice(None)
                xSlice = x if x is not None else slice(None)
                if data.ndim == 1:
                    data = data[xSlice]
                else:
                    data = data[ySlice, xSlice]

            # Check if the dtype is structured (i.e., for complex types)
            if data.dtype.fields is not None and 'real' in data.dtype.fields and 'imag' in data.dtype.fields:
                # Reconstruct complex array by combining real and imaginary parts
                data = data['real'] + 1j * data['imag']

            # Cast to the desired NumPy type (e.g., complex64, complex128, or float, or...)
            data = data.astype(daoType2NpType(self.image.md.contents.atype))

        return (result, data)
    

    def get_data_arbitrary(self, index, x=None, y=None):
        ''' --------------------------------------------------------------
        Reads and returns the requested data segment of the SHM file

        Parameters:
        ----------
        - wait: integer (last index) if not False, waits image update
        - reform: boolean, if True, reshapes the array in a 2-3D format
        - x, y: optional slice objects to extract only a sub-region of the
                image (e.g. x=slice(5,10), y=slice(10,15) for a 5x5 crop).
                Same convention as get_data.
        -------------------------------------------------------------- '''

        # First, check the requested number of items is valid
        fifo_size = self.image.md[0].fifo_size
        fifo_idx = index % fifo_size
        
        arrayPtr = ctypes.c_void_p(None)
        
        result = self.daoShmGetDataAt(ctypes.byref(self.image), ctypes.byref(arrayPtr),\
                                           ctypes.c_uint32(fifo_idx))

        # Cast our void pointer to the desired type
        arrayPtr = ctypes.cast(arrayPtr.value,\
                               ctypes.POINTER(daoType2CtypesType(self.image.md.contents.atype)))

        # Get the array size
        arraySize = np.ctypeslib.as_array(ctypes.cast(self.image.md.contents.size,\
                                                      ctypes.POINTER(ctypes.c_uint32)), shape=(3,))

        #data=np.ctypeslib.as_array(arrayPtr, shape=(self.image.md.contents.nelement,)).astype(daoType2NpType(self.image.md.contents.atype))
        if arraySize[2] == 0:
            if arraySize[1] == 0:
                data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0],))#.astype(daoType2NpType(self.image.md.contents.atype))
            else:
                data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0], arraySize[1]))#.astype(daoType2NpType(self.image.md.contents.atype))
        else:
            data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0], arraySize[1], arraySize[2]))#.astype(daoType2NpType(self.image.md.contents.atype))

        # Apply the requested ROI to the zero-copy view *before* the
        # complex-reconstruction/cast below, so only the sub-region ends up
        # being copied out of shared memory instead of the whole frame.
        if x is not None or y is not None:
            ySlice = y if y is not None else slice(None)
            xSlice = x if x is not None else slice(None)
            if data.ndim == 1:
                data = data[xSlice]
            else:
                data = data[ySlice, xSlice]

        # Check if the dtype is structured (i.e., for complex types)
        if data.dtype.fields is not None and 'real' in data.dtype.fields and 'imag' in data.dtype.fields:
            # Reconstruct complex array by combining real and imaginary parts
            data = data['real'] + 1j * data['imag']

        # Cast to the desired NumPy type (e.g., complex64, complex128, or float, or...)
        data = data.astype(daoType2NpType(self.image.md.contents.atype))

        return data


    # ------------------------------------------------------------------
    # GPU SHMs
    def is_gpu(self):
        ''' True if the payload of this SHM lives on a GPU (daoShmCreateGpu). '''
        return bool(self.daoShmIsGpu(ctypes.byref(self.image)))

    def device_ptr(self):
        ''' Device pointer (int) of the GPU payload in this process, or None. '''
        return self.image.d_array or None

    def begin_write(self):
        ''' Mark the frame as being written, before launching GPU work on
        device_ptr(); commit() clears it. Readers (get_data) then never return
        a frame that is half written. '''
        self.daoShmBeginWrite(ctypes.byref(self.image))

    def commit(self, stream=0, sync=False):
        ''' Publish data written on the GPU (e.g. by a kernel into device_ptr()):
        once the work already queued on `stream` is done, cnt0 is incremented,
        the timestamp set and the semaphores posted. `stream` is a CUDA stream
        handle (int, or a CuPy stream); 0 is the default stream.
        sync=False returns at once (published by a CUDA callback); sync=True
        waits for the stream and publishes directly, ~10-15 us sooner. '''
        handle = getattr(stream, "ptr", stream) or None
        fn = self.daoShmCommitSync if sync else self.daoShmCommit
        if fn(ctypes.byref(self.image), ctypes.c_void_p(handle)) != self.DAO_SUCCESS:
            raise OSError("daoShm.shm: commit failed")

    def get_device_array(self):
        ''' CuPy array viewing the GPU payload in place (no copy). Writes to it
        are published with commit(). Requires CuPy. '''
        import cupy
        ptr = self.device_ptr()
        if ptr is None:
            raise ValueError("daoShm.shm: not a GPU SHM, or its GPU is not accessible here")
        md = self.image.md.contents
        shape = tuple(int(n) for n in md.size[:md.naxis])
        dtype = np.dtype(daoType2NpType(md.atype))
        device = self._cupy_device(cupy, bytes(md.gpu_uuid))
        mem = cupy.cuda.UnownedMemory(ptr, int(md.nelement) * dtype.itemsize, self, device)
        return cupy.ndarray(shape, dtype, cupy.cuda.MemoryPointer(mem, 0))

    @staticmethod
    def _cupy_device(cupy, uuid):
        for i in range(cupy.cuda.runtime.getDeviceCount()):
            if bytes(cupy.cuda.runtime.getDeviceProperties(i)["uuid"]) == uuid:
                return i
        raise ValueError("daoShm.shm: the SHM's GPU is not visible to CuPy")

    def get_data(self, check=False, reform=True, semNb=0, timeout=0, spin=False, x=None, y=None):
        ''' --------------------------------------------------------------
        Reads and returns the newest data segment of the SHM file

        Parameters:
        ----------
        - check: integer (last index) if not False, waits image update
        - reform: boolean, if True, reshapes the array in a 2-3D format
        - x, y: optional slice objects to extract only a sub-region of the
                image (e.g. x=slice(5,10), y=slice(10,15) for a 5x5 crop).
                The view onto shared memory is zero-copy regardless of
                image size, so slicing it before the final dtype cast
                means only the requested sub-region is actually copied.
        -------------------------------------------------------------- '''
        if check == True:
            if spin == True:
                result = self.daoShmWaitCounter(ctypes.byref(self.image))
            else:
                if timeout == 0:
                    # On Windows, daoShmWaitForSemaphore returns DAO_TIMEOUT
                    # (=-1) after a short internal timeout so that Python
                    # regains control and can process KeyboardInterrupt.
                    # We loop here in Python so Ctrl+C works correctly.
                    result = -1
                    while result == -1:
                        result = self.daoShmWaitSem(ctypes.byref(self.image), semNb)
                else:
                    ts = make_timespec_from_now(timeout)
                    result = self.daoShmWaitSemTimeout(ctypes.byref(self.image), semNb, ctypes.byref(ts))
                    if result != 0:
                        log.error("Timeout waiting for semaphore")
                        return None

        arrayPtr = ctypes.c_void_p(None)
        seg_idx = ctypes.c_uint32(0)
        seg_cnt0 = ctypes.c_uint64(0)
        
        result = self.daoShmGetData(ctypes.byref(self.image), ctypes.byref(arrayPtr),\
                                           ctypes.byref(seg_idx), ctypes.byref(seg_cnt0))
        
        # Cast our void pointer to the desired type
        arrayPtr = ctypes.cast(arrayPtr.value,\
                               ctypes.POINTER(daoType2CtypesType(self.image.md.contents.atype)))

        # Get the array size
        arraySize = np.ctypeslib.as_array(ctypes.cast(self.image.md.contents.size,\
                                                      ctypes.POINTER(ctypes.c_uint32)), shape=(3,))

        #data=np.ctypeslib.as_array(arrayPtr, shape=(self.image.md.contents.nelement,)).astype(daoType2NpType(self.image.md.contents.atype))
        if arraySize[2] == 0:
            if arraySize[1] == 0:
                data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0],))#.astype(daoType2NpType(self.image.md.contents.atype))
            else:
                data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0], arraySize[1]))#.astype(daoType2NpType(self.image.md.contents.atype))
        else:
            data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0], arraySize[1], arraySize[2]))#.astype(daoType2NpType(self.image.md.contents.atype))

        # Apply the requested ROI to the zero-copy view *before* the
        # complex-reconstruction/cast below, so only the sub-region ends up
        # being copied out of shared memory instead of the whole frame.
        if x is not None or y is not None:
            ySlice = y if y is not None else slice(None)
            xSlice = x if x is not None else slice(None)
            if data.ndim == 1:
                data = data[xSlice]
            else:
                data = data[ySlice, xSlice]

        # Check if the dtype is structured (i.e., for complex types)
        if data.dtype.fields is not None and 'real' in data.dtype.fields and 'imag' in data.dtype.fields:
            # Reconstruct complex array by combining real and imaginary parts
            data = data['real'] + 1j * data['imag']

        # Cast to the desired NumPy type (e.g., complex64, complex128, or float, or...)
        data = data.astype(daoType2NpType(self.image.md.contents.atype))

        return data



    def get_history(self, num_items=1, check=False, semNb=0, timeout=0, spin=False, buffer=False, x=None, y=None):
        ''' --------------------------------------------------------------
        Reads and returns the last N segments written to the SHM

        Parameters:
        ----------
        - num_items: Number of items to return
        - check:  if True, waits for the next semaphore post before reading history
        - semNb:  semaphore number to wait on (used when check=True or buffer=True)
        - timeout: seconds to wait for semaphore (0 = wait indefinitely)
        - spin:   if True, use counter-based wait instead of semaphore
        - buffer: if True, waits for num_items new writes since last call before
                  returning the array (implies semaphore waiting per write)
        - x, y: optional slice objects to crop each returned frame to a
                sub-region (e.g. x=slice(5,10), y=slice(10,15) for a 5x5
                crop). Same convention as get_data - only the requested
                sub-region is copied out of shared memory per history entry.
        -------------------------------------------------------------- '''

        # First, check the requested number of items is valid
        fifo_size = self.image.md[0].fifo_size

        if (num_items < 1):
            log.error("Cannot read less than 1 item")
            return None
        elif (num_items > fifo_size):
            log.error("Cannot read more segments than exist")
            return None

        def _wait_one():
            """Wait for a single new semaphore post. Returns False on timeout."""
            if spin:
                self.daoShmWaitCounter(ctypes.byref(self.image))
            else:
                if timeout == 0:
                    result = -1
                    while result == -1:
                        result = self.daoShmWaitSem(ctypes.byref(self.image), semNb)
                else:
                    ts = make_timespec_from_now(timeout)
                    result = self.daoShmWaitSemTimeout(ctypes.byref(self.image), semNb, ctypes.byref(ts))
                    if result != 0:
                        log.error("Timeout waiting for semaphore")
                        return False
            return True

        if buffer:
            # Accumulate num_items new writes before reading
            for _ in range(num_items):
                if not _wait_one():
                    return None
        elif check:
            # Wait for the next semaphore post (next write) before reading
            if not _wait_one():
                return None

        # Now determine the size of array to reserve
        arraySize = np.ctypeslib.as_array(ctypes.cast(self.image.md.contents.size,\
                                                      ctypes.POINTER(ctypes.c_uint32)), shape=(3,))
        
        if arraySize[2] == 0:
            if arraySize[1] == 0:
                frame_shape = (arraySize[0],)
            else:
                frame_shape = (arraySize[0], arraySize[1])
        else:
            frame_shape = (arraySize[0], arraySize[1], arraySize[2])

        # Compute the cropped per-frame shape (if x/y were requested) so the
        # preallocated history buffer, and thus every per-frame copy below,
        # only ever holds the requested sub-region instead of full frames.
        ySlice = y if y is not None else slice(None)
        xSlice = x if x is not None else slice(None)
        if len(frame_shape) == 1:
            cropped_frame_shape = (len(range(*xSlice.indices(int(frame_shape[0])))),)
        else:
            cropped_frame_shape = (len(range(*ySlice.indices(int(frame_shape[0])))),
                                    len(range(*xSlice.indices(int(frame_shape[1]))))) + tuple(frame_shape[2:])

        result_shape = (num_items,) + cropped_frame_shape

        history_data = np.empty(result_shape, dtype=daoType2NpType(self.image.md.contents.atype))

        # Determine the index to read backwards from
        idx_base = self.image.md[0].fifo_last_written - num_items + 1

        element_ctype = daoType2CtypesType(self.image.md.contents.atype)

        for idx_offset in range(num_items):
            # Calculate index
            current_idx = (idx_base + idx_offset) % fifo_size

            arrayPtr = ctypes.c_void_p(None)
            self.daoShmGetDataAt(ctypes.byref(self.image), ctypes.byref(arrayPtr), ctypes.c_uint32(current_idx))
            arrayPtr = ctypes.cast(arrayPtr.value, ctypes.POINTER(element_ctype))

            current_data = np.ctypeslib.as_array(arrayPtr, shape=frame_shape)

            if x is not None or y is not None:
                if len(frame_shape) == 1:
                    current_data = current_data[xSlice]
                else:
                    current_data = current_data[ySlice, xSlice]

            if current_data.dtype.fields is not None \
                    and 'real' in current_data.dtype.fields \
                    and 'imag' in current_data.dtype.fields:
                # Reconstruct complex array by combining real and imaginary parts
                current_data = current_data['real'] + 1j * current_data['imag']

            history_data[idx_offset] = current_data 

        # Cast to the desired NumPy type (e.g., complex64, complex128, or float, or...)
        history_data = history_data.astype(daoType2NpType(self.image.md.contents.atype))

        return history_data



    def get_meta_data(self, index=None):
        ''' --------------------------------------------------------------
        Get the metadata fraction of the SHM file.
        Populate the shm object mtdata dictionary.

        Parameters:
        ----------
        - verbose: (boolean, default: True), prints its findings
        -------------------------------------------------------------- '''

        if (index == None):
            # get latest metadata from FIFO
            seg_ptr = ctypes.c_void_p(None)
            seg_idx = ctypes.c_uint32(0)
            seg_cnt0 = ctypes.c_uint64(0)

            self.daoShmGetData(ctypes.byref(self.image),\
                                        ctypes.byref(seg_ptr),\
                                        ctypes.byref(seg_idx),
                                        ctypes.byref(seg_cnt0))
        else:
            # get requested metadata from FIFO
            fifo_size = self.image.md.contents.fifo_size
            adjusted_index = index % fifo_size
            seg_idx = ctypes.c_uint32(adjusted_index)

        md = self.image.md[seg_idx.value]
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

        # Graft current FIFO values into this metadata from image->md[0]
        self.mtdata['fifo_last_written'] = self.image.md.contents.fifo_last_written
        self.mtdata['fifo_size'] = self.image.md.contents.fifo_size

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

    def reset_tail(self, ):
        ''' --------------------------------------------------------------
        Reset the reading tail for this instance of the SHM
        -------------------------------------------------------------- '''
        tail_index = ctypes.c_uint32()
        tail_timestamp = ctypes.c_uint64()
        result = self.daoShmResetReadTail(
            ctypes.byref(self.image),
            ctypes.byref(tail_index),
            ctypes.byref(tail_timestamp)
        )
        return result

    def publish(self):
        self.pubContext = zmq.Context()
        self.pubSocket = self.pubContext.socket(zmq.PUB)
        self.pubSocket.bind("tcp://*:%d" % (self.pubPort))
        self.pubThreadCounter = self.get_counter()
        while True:
            if self.pubEnable:
                current_counter = self.get_counter()
                if current_counter > self.pubThreadCounter:
                    # Only publish if this counter wasn't just received from subscription
                    if current_counter != self.last_received_counter:
                        topic = 'frameData'
                        self.pubSocket.send_string(topic, zmq.SNDMORE)
                        self.pubSocket.send_pyobj(self.get_data())
                    self.pubThreadCounter = current_counter
            if self.pubEvent.is_set():
                break
            time.sleep(0.001)
        self.pubSocket.close()
        self.pubContext.term()
    
    def subscribe(self):
        self.subContext = zmq.Context()
        self.subSocket = self.subContext.socket(zmq.SUB)
        self.subSocket.connect("tcp://%s:%d" % (self.subHost, self.subPort))
        self.subSocket.setsockopt(zmq.SUBSCRIBE, b'frameData')
        self.subSocket.setsockopt(zmq.CONFLATE, 1)
        self.subSocket.setsockopt(zmq.RCVTIMEO, 100)  # Set a 1-second timeout for receiving
        topic = 'frameData'
        while True:
            if self.subEnable:
                try:
                    topic = self.subSocket.recv_string()
                    frameData = self.subSocket.recv_pyobj()
                    # Store current counter before updating data
                    self.last_received_counter = self.get_counter() + 1  # Predict next counter
                    self.set_data(frameData)
                except zmq.Again:
                    # Timeout occurred, check if we need to exit
                    if self.subEvent.is_set():
                        break
            if self.subEvent.is_set():
                break
        self.subSocket.close()
        self.subContext.term()

    def close(self):
        ''' --------------------------------------------------------------
        Close the SHM file.

        -------------------------------------------------------------- '''
        result = self.daoShmClose(ctypes.byref(self.image))
        
    def __del__(self):
        ''' --------------------------------------------------------------
        Destructor to ensure proper resource cleanup.
        
        This method is automatically called when the object is garbage collected.
        -------------------------------------------------------------- '''
        try:
            # Only call close if the image has been used/initialized
            if hasattr(self, 'image') and self.image.used:
                self.close()
        except:
            # Suppress errors during garbage collection
            pass


