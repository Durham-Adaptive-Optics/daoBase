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

# Constants from dao.h
IMAGE_NB_SEMAPHORE = 10  # Number of semaphores per image

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

class SEM_REGISTRY_ENTRY(ctypes.Structure):
    """Semaphore registry entry for tracking reader ownership"""
    _fields_ = [
        ("locked", ctypes.c_uint8),
        ("reader_pid", ctypes.c_int32 if sys.platform == "win32" else ctypes.c_int32),
        ("reader_name", ctypes.c_char * 64),
        ("last_read_cnt0", ctypes.c_uint64),
        ("last_read_time", timespec),
        ("read_count", ctypes.c_uint32),
        ("timeout_count", ctypes.c_uint32)
    ]

class IMAGE_KEYWORD(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 16),
        ("type", ctypes.c_char),
        ("value", ctypes.c_double),  # Use the largest data type to accommodate all possible values
        ("comment", ctypes.c_char * 80)
    ]

# Define the IMAGE_METADATA structure
if sys.platform == "Darwin":
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
            ("lastNbArray", ctypes.c_uint64 * 512),
            ("semCounter", ctypes.c_uint32 * 10),
            ("semLogCounter", ctypes.c_uint32),
            ("sem_registry", SEM_REGISTRY_ENTRY * 10),
            ("registry_version", ctypes.c_uint64)
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
            ("lastNbArray", ctypes.c_uint64 * 512),
            ("semCounter", ctypes.c_uint32 * 10),
            ("semLogCounter", ctypes.c_uint32),
            ("sem_registry", SEM_REGISTRY_ENTRY * 10),
            ("registry_version", ctypes.c_uint64)
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
            ('registry_lock', ctypes.POINTER(ctypes.c_void_p)),
            ('my_semaphore_index', ctypes.c_int32),
            ('is_reader', ctypes.c_uint8),
            ('is_writer', ctypes.c_uint8),
            ('my_process_name', ctypes.c_char * 64),
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
            ('semWritePID', ctypes.POINTER(ctypes.c_int32)),
            ('registry_lock', ctypes.POINTER(ctypes.c_void_p)),
            ('my_semaphore_index', ctypes.c_int32),
            ('is_reader', ctypes.c_uint8),
            ('is_writer', ctypes.c_uint8),
            ('my_process_name', ctypes.c_char * 64)
        ]

class shm:
    def __init__(self, fname=None, data=None, nbkw=0, pubPort=5555, subPort=5555, subHost='localhost', logLevel=1):
        # int8_t daoShmInit1D(const char *name, char *prefix, uint32_t nbVal, IMAGE **image);
        self.daoShmInit1D = daoLib.daoShmInit1D
        self.daoShmInit1D.argtypes = [
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_char),
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.POINTER(IMAGE))
        ]
        self.daoShmInit1D.restype = ctypes.c_int8

        # set the log level
        setLogLevel(logLevel)

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

        # int8_t daoShmImage2ShmQuiet(void *procim, uint32_t nbVal, IMAGE *image);
        self.daoShmImage2ShmQuiet = daoLib.daoShmImage2ShmQuiet
        self.daoShmImage2ShmQuiet.argtypes = [
            ctypes.c_void_p,
            ctypes.c_uint32,
            ctypes.POINTER(IMAGE)
        ]
        self.daoShmImage2ShmQuiet.restype = ctypes.c_int8

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
            ctypes.c_int32
        ]
        self.daoShmWaitForSemaphore.restype = ctypes.c_int8

        self.daoShmWaitForSemaphoreTimeout = daoLib.daoShmWaitForSemaphoreTimeout
        self.daoShmWaitForSemaphoreTimeout.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_int32,
            ctypes.POINTER(timespec)
        ]
        self.daoShmWaitForSemaphoreTimeout.restype = ctypes.c_int8

        self.daoShmWaitForCounter = daoLib.daoShmWaitForCounter
        self.daoShmWaitForCounter.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmWaitForCounter.restype = ctypes.c_int8
        
        # int8_t daoShmCloseShm(IMAGE *image);
        self.daoShmCloseShm = daoLib.daoShmCloseShm
        self.daoShmCloseShm.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmCloseShm.restype = ctypes.c_int8
        
        # Semaphore Registry API
        self.daoShmRegisterReader = daoLib.daoShmRegisterReader
        self.daoShmRegisterReader.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_char_p,
            ctypes.c_int32
        ]
        self.daoShmRegisterReader.restype = ctypes.c_int8
        
        self.daoShmUnregisterReader = daoLib.daoShmUnregisterReader
        self.daoShmUnregisterReader.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmUnregisterReader.restype = ctypes.c_int8
        
        self.daoShmValidateReaderRegistration = daoLib.daoShmValidateReaderRegistration
        self.daoShmValidateReaderRegistration.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmValidateReaderRegistration.restype = ctypes.c_int8
        
        self.daoShmGetRegisteredReaders = daoLib.daoShmGetRegisteredReaders
        self.daoShmGetRegisteredReaders.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.POINTER(SEM_REGISTRY_ENTRY)
        ]
        self.daoShmGetRegisteredReaders.restype = ctypes.c_int8
        
        self.daoShmForceUnlockSemaphore = daoLib.daoShmForceUnlockSemaphore
        self.daoShmForceUnlockSemaphore.argtypes = [
            ctypes.POINTER(IMAGE),
            ctypes.c_int32
        ]
        self.daoShmForceUnlockSemaphore.restype = ctypes.c_int8
        
        self.daoShmCleanupStaleSemaphores = daoLib.daoShmCleanupStaleSemaphores
        self.daoShmCleanupStaleSemaphores.argtypes = [ctypes.POINTER(IMAGE)]
        self.daoShmCleanupStaleSemaphores.restype = ctypes.c_int8

        self.image=IMAGE()
        
        # Initialize local registration tracking
        self._my_semaphore = -1
        self._registered = False
        
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
            # log.info("loading existing %s " % (fname))
            result = self.daoShmShm2Img(fname.encode('utf-8'), ctypes.byref(self.image))
            # Auto-register as reader for backward compatibility
            try:
                import os
                process_name = os.path.basename(sys.argv[0]) if sys.argv else "python_reader"
                self._my_semaphore = self.register_reader(process_name)
                # log.info(f"Auto-registered as reader with semaphore {self._my_semaphore}")
            except RuntimeError as e:
                # If registration failed due to no available semaphores, try cleanup and retry
                if "No semaphores available" in str(e):
                    log.warning(f"Initial registration failed, cleaning up stale semaphores...")
                    cleaned = self.cleanup_stale_semaphores()
                    # log.info(f"Cleaned up {cleaned} stale semaphore(s)")
                    if cleaned > 0:
                        try:
                            self._my_semaphore = self.register_reader(process_name)
                            # log.info(f"Auto-registered as reader with semaphore {self._my_semaphore} after cleanup")
                        except Exception as retry_e:
                            log.warning(f"Failed to auto-register reader after cleanup: {retry_e}")
                    else:
                        log.warning(f"Failed to auto-register reader: {e}")
                else:
                    log.warning(f"Failed to auto-register reader: {e}")
            except Exception as e:
                log.warning(f"Failed to auto-register reader: {e}")
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

    def set_data(self, data):
        ''' --------------------------------------------------------------
        Upload new data to the SHM file.

        Parameters:
        ----------
        - data: the array to upload to SHM
        '''
        # Call the daoShmImage2Shm function to feel the SHM
        if data.flags['C_CONTIGUOUS']:
            cData = data.ctypes.data_as(ctypes.c_void_p)
        else:
            cData = np.ascontiguousarray(data).ctypes.data_as(ctypes.c_void_p)

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
                if timeout == 0:
                    result = self.daoShmWaitForSemaphore(ctypes.byref(self.image), semNb)
                else:
                    ts = make_timespec_from_now(timeout)
                    result = self.daoShmWaitForSemaphoreTimeout(ctypes.byref(self.image), semNb, ctypes.byref(ts))
                    if result != 0:
                        log.error("Timeout waiting for semaphore")
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

    def register_reader(self, process_name=None, preferred_sem=-1):
        ''' --------------------------------------------------------------
        Register as a reader for this shared memory buffer.
        
        This function allocates a dedicated semaphore for this reader process,
        preventing race conditions when multiple readers access the same buffer.
        
        Parameters:
            process_name: str, optional
                Custom name for this reader process. If None, uses the actual
                process name from the OS.
            preferred_sem: int, optional
                Preferred semaphore index (0-9). If -1 or unavailable, 
                automatically finds a free semaphore.
                
        Returns:
            int: The allocated semaphore index (0-9)
            
        Raises:
            RuntimeError: If registration fails (no semaphores available,
                         or registration was stolen by another process)
        -------------------------------------------------------------- '''
        if self._registered:
            # Already registered, return current semaphore
            return self._my_semaphore
            
        # Convert process_name to bytes if provided
        proc_name_bytes = None
        if process_name is not None:
            proc_name_bytes = process_name.encode('utf-8')
            
        # Call C function
        sem_index = self.daoShmRegisterReader(
            ctypes.byref(self.image),
            proc_name_bytes,
            ctypes.c_int(preferred_sem)
        )
        
        if sem_index < 0:
            if sem_index == -3:  # DAO_NO_SEMAPHORE_AVAILABLE
                raise RuntimeError("No semaphores available - all 10 slots are in use")
            elif sem_index == -4:  # DAO_REGISTRATION_STOLEN
                raise RuntimeError("Registration was stolen by another process")
            else:
                raise RuntimeError(f"Failed to register reader: error code {sem_index}")
                
        # Update local state
        self._my_semaphore = sem_index
        self._registered = True
        return sem_index

    def unregister_reader(self):
        ''' --------------------------------------------------------------
        Unregister this reader and release its semaphore.
        
        This should be called when the reader no longer needs to access
        the shared memory, allowing other readers to use the semaphore slot.
        
        Returns:
            int: 0 on success, negative error code on failure
        -------------------------------------------------------------- '''
        if not self._registered:
            return 0  # Not registered, nothing to do
            
        result = self.daoShmUnregisterReader(ctypes.byref(self.image))
        
        if result == 0:
            self._my_semaphore = -1
            self._registered = False
            
        return result

    def validate_registration(self):
        ''' --------------------------------------------------------------
        Validate that this reader's registration is still valid.
        
        Checks that our process still owns the semaphore slot we registered for.
        This is a quick check that doesn't involve acquiring locks.
        
        Returns:
            bool: True if registration is valid, False otherwise
        -------------------------------------------------------------- '''
        if not self._registered:
            return False
            
        result = self.daoShmValidateReaderRegistration(ctypes.byref(self.image))
        
        if result != 0:
            # Registration is invalid - clear local state
            self._my_semaphore = -1
            self._registered = False
            return False
            
        return True

    def get_registered_readers(self):
        ''' --------------------------------------------------------------
        Get information about all currently registered readers.
        
        Returns a list of dictionaries, one per registered reader, containing:
        - locked: True if semaphore is in use
        - reader_pid: Process ID of the reader
        - reader_name: Name of the reader process
        - last_read_cnt0: Counter value at last read
        - last_read_time: Timestamp of last read
        - read_count: Total number of successful reads
        - timeout_count: Number of timeouts
        
        Returns:
            list: List of dictionaries with reader information
        -------------------------------------------------------------- '''
        # Allocate array for registry entries
        registry_array = (SEM_REGISTRY_ENTRY * IMAGE_NB_SEMAPHORE)()
        
        # Call C function
        result = self.daoShmGetRegisteredReaders(
            ctypes.byref(self.image),
            registry_array
        )
        
        if result < 0:
            return []
            
        # Convert to list of dicts
        readers = []
        for i in range(IMAGE_NB_SEMAPHORE):
            entry = registry_array[i]
            if entry.locked:
                readers.append({
                    'index': i,
                    'locked': bool(entry.locked),
                    'reader_pid': entry.reader_pid,
                    'reader_name': entry.reader_name.decode('utf-8', errors='ignore').rstrip('\x00'),
                    'last_read_cnt0': entry.last_read_cnt0,
                    'last_read_time': entry.last_read_time,
                    'read_count': entry.read_count,
                    'timeout_count': entry.timeout_count
                })
                
        return readers

    def force_unlock_semaphore(self, semNb):
        ''' --------------------------------------------------------------
        Force unlock a semaphore slot (administrative function).
        
        WARNING: This should only be used for administrative/debugging
        purposes. Forcefully unlocking a semaphore that is actively in use
        can cause race conditions.
        
        Parameters:
            semNb: int
                Semaphore index (0-9) to force unlock
                
        Returns:
            int: 0 on success, negative error code on failure
        -------------------------------------------------------------- '''
        if semNb < 0 or semNb >= IMAGE_NB_SEMAPHORE:
            raise ValueError(f"Invalid semaphore number {semNb}, must be 0-{IMAGE_NB_SEMAPHORE-1}")
            
        return self.daoShmForceUnlockSemaphore(
            ctypes.byref(self.image),
            ctypes.c_int(semNb)
        )

    def cleanup_stale_semaphores(self):
        ''' --------------------------------------------------------------
        Clean up semaphore registrations from dead processes.
        
        Scans the registry and removes entries for processes that are
        no longer running. This helps prevent semaphore exhaustion from
        crashed processes.
        
        Returns:
            int: Number of stale entries cleaned up, or negative error code
        -------------------------------------------------------------- '''
        return self.daoShmCleanupStaleSemaphores(ctypes.byref(self.image))

    def close(self):
        ''' --------------------------------------------------------------
        Close the SHM file.

        -------------------------------------------------------------- '''
        result = self.daoShmCloseShm(ctypes.byref(self.image))
        
    def __del__(self):
        ''' --------------------------------------------------------------
        Destructor to ensure proper resource cleanup.
        
        This method is automatically called when the object is garbage collected.
        -------------------------------------------------------------- '''
        try:
            # Unregister reader if registered
            if hasattr(self, '_registered') and self._registered:
                self.unregister_reader()
            # Only call close if the image has been used/initialized
            if hasattr(self, 'image') and self.image.used:
                self.close()
        except:
            # Suppress errors during garbage collection
            pass


