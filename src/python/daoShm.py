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
        
    def get_data(self, check=False, reform=True, semNb=0, timeout=0):
        ''' --------------------------------------------------------------
        Reads and returns the data part of the SHM file

        Parameters:
        ----------
        - check: integer (last index) if not False, waits image update
        - reform: boolean, if True, reshapes the array in a 2-3D format
        -------------------------------------------------------------- '''
        if check == True:
            result = self.daoShmWaitForSemaphore(ctypes.byref(self.image), semNb)

        arraySize = np.ctypeslib.as_array(ctypes.cast(self.image.md.contents.size,\
                                                      ctypes.POINTER(ctypes.c_uint32)), shape=(3,))

        arrayPtr = ctypes.cast(self.image.array,\
                               ctypes.POINTER(daoType2CtypesType(self.image.md.contents.atype)))
        #data=np.ctypeslib.as_array(arrayPtr, shape=(self.image.md.contents.nelement,)).astype(daoType2NpType(self.image.md.contents.atype))
        data=np.ctypeslib.as_array(arrayPtr, shape=(arraySize[0], arraySize[1])).astype(daoType2NpType(self.image.md.contents.atype))
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

SEMAPHORE_MAXVAL = 1
IMAGE_NB_SEMAPHORE = 10

# ------------------------------------------------------
#          list of available data types
# ------------------------------------------------------
all_dtypes = [np.uint8,     np.int8,    np.uint16,    np.int16, 
              np.uint32,    np.int32,   np.uint64,    np.int64,
              np.float32,   np.float64, np.complex64, np.complex128]

# ------------------------------------------------------
# list of metadata keys for the shm structure (global)
# ------------------------------------------------------
mtkeys = ['imname', 'naxis',  'size',    'nel',   'atype',
          'crtime', 'latime', 'tvsec',   'tvnsec', 
          'shared', 'status', 'logflag', 'sem',
          'cnt0',   'cnt1',   'cnt2',
          'write',  'nbkw', 'lastPos', 'lastNb',
          'packetNb', 'packetTotal', 'lastNbArray']

# ------------------------------------------------------
#    string used to decode the binary shm structure
# ------------------------------------------------------
hdr_fmt     = '80s B 3I Q B d d q q B B B H5x Q Q Q B H I I I I 512H'
hdr_fmt_pck = '80s B 3I Q B d d q q B B B H5x Q Q Q B H I I I I 512H'           # packed style
hdr_fmt_aln = '80s B3x 3I Q B7x d d q q B B B1x H2x Q Q Q B1x H4x I I I I 512L' # aligned style




''' 
---------------------------------------------------------
Table taken from Python 2 documentation, section 7.3.2.2.
---------------------------------------------------------

|--------+--------------------+----------------+----------|
| Format | C Type             | Python type    | Std size |
|--------+--------------------+----------------+----------|
| x      | pad byte           | no value       |          |
| c      | char               | string (len=1) |        1 |
| b      | signed char        | integer        |        1 |
| B      | unsigned char      | integer        |        1 |
| ?      | _Bool              | bool           |        1 |
| h      | short              | integer        |        2 |
| H      | unsigned short     | integer        |        2 |
| i      | int                | integer        |        4 |
| I      | unsigned int       | integer        |        4 |
| l      | long               | integer        |        4 |
| L      | unsigned long      | integer        |        4 |
| q      | long long          | integer        |        8 |
| Q      | unsigned long long | integer        |        8 |
| f      | float              | float          |        4 |
| d      | double             | float          |        8 |
| s      | char[]             | string         |          |
| p      | char[]             | string         |          |
| P      | void *             | integer        |          |
|--------+--------------------+----------------+----------| 
'''
class shmOld:
    def __init__(self, fname=None, data=None, verbose=False, packed=False, nbkw=0, pubPort=5555, subPort=5555, subHost='localhost'):
        ''' --------------------------------------------------------------
        Constructor for a SHM (shared memory) object.

        Parameters:
        ----------
        - fname: name of the shared memory file structure
        - data: some array (1, 2 or 3D of data)
        - verbose: optional boolean

        Depending on whether the file already exists, and/or some new
        data is provided, the file will be created or overwritten.
        -------------------------------------------------------------- '''
        #self.hdr_fmt   = hdr_fmt  # in case the user is interested
        #self.c0_offset = 144      # fast-offset for counter #0
        #self.kwsz      = 113      # size of a keyword SHM data structure
        self.packed = packed
        
        if self.packed:
            self.hdr_fmt = hdr_fmt_pck # packed shm structure
            self.kwfmt0 = "16s s"      # packed keyword structure
        else:
            self.hdr_fmt = hdr_fmt_aln # aligned shm structure
            self.kwfmt0 = "16s s7x"    # aligned keyword structure

        self.ts_offset = 128
        self.c0_offset = 152        # fast-offset for counter #0 (updated later)
        self.c1_offset = 160
        self.c2_offset = 168
        self.kwsz      = 96 + struct.calcsize(self.kwfmt0) # keyword SHM size

        # --------------------------------------------------------------------
        #                dictionary containing the metadata
        # --------------------------------------------------------------------
        self.mtdata = {'imname': '',
                       'naxis' : 0,
                       'size'  : (0,0,0),
                       'nel': 0,
                       'atype': 0,
                       'crtime': 0.0,
                       'latime': 0.0, 
                       'tvsec' : 0,
                       'tvnsec': 0,
                       'shared': 0,
                       'status': 0,
                       'logflag': 0,
                       'sem': 0,
                       'cnt0'  : 0,
                       'cnt1'  : 0,
                       'cnt2': 0,
                       'write' : 0,
                       'nbkw'  : 0,
                       'lastPos': 0,
                       'lastNb' : 0,
                       'packetNb':0,
                       'packetTotal':0,
                       'lastNbArray':tuple([int(0)]*512)}

        # --------------------------------------------------------------------
        #          dictionary describing the content of a keyword
        # --------------------------------------------------------------------
        self.kwd = {'name': '', 'type': 'N', 'value': '', 'comment': ''}

        # ---------------
        if fname is None:
            print("No SHM file name provided")
            return(None)

        self.fname = fname
        # ---------------
        # Creating semaphore, x9
        singleName=self.fname.split('/')[-1].split('.')[0]
        self.semaphores = []
        for k in range(IMAGE_NB_SEMAPHORE):
            semName = '/'+singleName+'_sem'+'0'+str(k)
            #print('creating semaphore '+semName)
            self.semaphores.append(posix_ipc.Semaphore(semName, flags=posix_ipc.O_CREAT))
        print(str(k)+' semaphores created or re-used')

        # ---------------
        if ((not os.path.exists(fname)) or (data is not None)):
            print("%s will be created or overwritten" % (fname,))
            self.create(fname, data, nbkw)

        # ---------------
        else:
            print("reading from existing %s" % (fname,))
            self.fd      = os.open(fname, os.O_RDWR)
            self.stats   = os.fstat(self.fd)
            self.buf_len = self.stats.st_size
            self.buf     = mmap.mmap(self.fd, self.buf_len, mmap.MAP_SHARED)
            self.read_meta_data(verbose=verbose)
            self.select_dtype()        # identify main data-type
            self.get_data()            # read the main data
            self.create_keyword_list() # create empty list of keywords
            self.read_keywords()       # populate the keywords with data
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
            
    def create(self, fname, data, nbkw=0):
        ''' --------------------------------------------------------------
        Create a shared memory data structure

        Parameters:
        ----------
        - fname: name of the shared memory file structure
        - data: some array (1, 2 or 3D of data)
        
        Called by the constructor if the provided file-name does not
        exist: a new structure needs to be created, and will be populated
        with information based on the provided data.
        -------------------------------------------------------------- '''
        
        if data is None:
            print("No data (ndarray) provided! Nothing happens here")
            return

        # ---------------------------------------------------------
        # feed the relevant dictionary entries with available data
        # ---------------------------------------------------------
        self.npdtype          = data.dtype
        print(fname.split('/')[-1].split('.')[0])
        self.mtdata['imname'] = fname.split('/')[-1].split('.')[0]#fname.ljust(80, ' ')
        self.mtdata['naxis']  = data.ndim
        self.mtdata['size']   = data.shape
        self.mtdata['nel']    = data.size
        self.mtdata['atype']  = self.select_atype()
        self.mtdata['shared'] = 1
        self.mtdata['nbkw']   = nbkw
        self.mtdata['sem']    = IMAGE_NB_SEMAPHORE
        
        if data.ndim == 2:
            self.mtdata['size'] = self.mtdata['size'] + (0,)

        self.select_dtype()

        # ---------------------------------------------------------
        #          reconstruct a SHM metadata buffer
        # ---------------------------------------------------------
        fmts = self.hdr_fmt.split(' ')
        minibuf = ''.encode()
        for i, fmt in enumerate(fmts):
            if i == 2:# tuple size 3
                tpl = self.mtdata[mtkeys[i]]
                minibuf += struct.pack(fmt, tpl[0], tpl[1], tpl[2])
            elif i == 22:# tuple size 512
                lst = list(self.mtdata[mtkeys[i]])
                minibuf += struct.pack(fmt, *lst)# slat lst
            else:
                if isinstance(self.mtdata[mtkeys[i]],str):
                    minibuf += struct.pack(fmt, self.mtdata[mtkeys[i]].encode())
                else:
                    minibuf += struct.pack(fmt, self.mtdata[mtkeys[i]])
            if mtkeys[i] == "sem": # the mkey before "cnt0" !
                self.c0_offset = len(minibuf)
        self.im_offset = len(minibuf)

        # ---------------------------------------------------------
        #             allocate the file and mmap it
        # ---------------------------------------------------------
        kwspace = self.kwsz * nbkw                    # kword space
        fsz = self.im_offset + self.img_len + kwspace # file size
        npg = int(fsz / mmap.PAGESIZE) + 1                 # nb pages
        self.fd = os.open(fname, os.O_CREAT | os.O_TRUNC | os.O_RDWR)
        os.write(self.fd, ('\x00' * npg * mmap.PAGESIZE).encode())
        self.buf = mmap.mmap(self.fd, npg * mmap.PAGESIZE, 
                             mmap.MAP_SHARED, mmap.PROT_WRITE)

        # ---------------------------------------------------------
        #              write the information to SHM
        # ---------------------------------------------------------
        self.buf[:self.im_offset] = minibuf # the metadata
        self.set_data(data)
        self.create_keyword_list()
        self.write_keywords()
        return(0)

    def rename_img(self, newname):
        ''' --------------------------------------------------------------
        Gives the user a chance to rename the image.

        Parameter:
        ---------
        - newname: a string (< 80 char) with the name
        -------------------------------------------------------------- '''
        
        self.mtdata['imname'] = newname.ljust(80, ' ')
        self.buf[0:80]        = struct.pack('80s', self.mtdata['imname'])

    def close(self,):
        ''' --------------------------------------------------------------
        Clean close of a SHM data structure link

        Clean close of buffer, release the file descriptor.
        -------------------------------------------------------------- '''
        self.buf.close()
        os.close(self.fd)
        self.fd = 0
        return(0)

    def read_meta_data(self, verbose=True):
        ''' --------------------------------------------------------------
        Read the metadata fraction of the SHM file.
        Populate the shm object mtdata dictionary.

        Parameters:
        ----------
        - verbose: (boolean, default: True), prints its findings
        -------------------------------------------------------------- '''
        offset = 0
        fmts = self.hdr_fmt.split(' ')
        for i, fmt in enumerate(fmts):
            hlen = struct.calcsize(fmt)
            mdata_bit = struct.unpack(fmt, self.buf[offset:offset+hlen])
            if i == 2:
                self.mtdata[mtkeys[i]] = mdata_bit
            elif i == 22:
                self.mtdata[mtkeys[i]] = mdata_bit
            else:
                self.mtdata[mtkeys[i]] = mdata_bit[0]
            offset += hlen

        self.mtdata['imname'] = self.mtdata['imname'].decode().strip('\x00')
        self.im_offset = offset # offset for the image content

        if verbose:
            self.print_meta_data()

    def get_meta_data(self):
        ''' --------------------------------------------------------------
        Get the metadata fraction of the SHM file.
        Populate the shm object mtdata dictionary.

        Parameters:
        ----------
        - verbose: (boolean, default: True), prints its findings
        -------------------------------------------------------------- '''
        offset = 0
        fmts = self.hdr_fmt.split(' ')
        for i, fmt in enumerate(fmts):
            hlen = struct.calcsize(fmt)
            mdata_bit = struct.unpack(fmt, self.buf[offset:offset+hlen])
            if i == 2:
                self.mtdata[mtkeys[i]] = mdata_bit
            elif i == 22:
                self.mtdata[mtkeys[i]] = mdata_bit
            else:
                self.mtdata[mtkeys[i]] = mdata_bit[0]
            offset += hlen

        self.mtdata['imname'] = self.mtdata['imname'].decode().strip('\x00')
        self.im_offset = offset # offset for the image content

        return(self.mtdata)

    def create_keyword_list(self):
        ''' --------------------------------------------------------------
        Place-holder. The name should be sufficiently explicit.
        -------------------------------------------------------------- '''
        nbkw = self.mtdata['nbkw']     # how many keywords
        self.kwds = []                 # prepare an empty list 
        for ii in range(nbkw):         # fill with empty dictionaries
            self.kwds.append(self.kwd.copy())
            
    def read_keywords(self):
        ''' --------------------------------------------------------------
        Read all keywords from SHM file
        -------------------------------------------------------------- '''        
        for ii in range(self.mtdata['nbkw']):
            self.read_keyword(ii)

    def write_keywords(self):
        ''' --------------------------------------------------------------
        Writes all keyword data to SHM file
        -------------------------------------------------------------- '''
        for ii in range(self.mtdata['nbkw']):
            self.write_keyword(ii)

    def read_keyword(self, ii):
        ''' --------------------------------------------------------------
        Read the content of keyword of given index.

        Parameters:
        ----------
        - ii: index of the keyword to read
        -------------------------------------------------------------- '''
        kwsz = self.kwsz              # keyword SHM data structure size
        k0   = self.im_offset + self.img_len + ii * kwsz # kword offset

        # ------------------------------------------
        #             read from SHM
        # ------------------------------------------
        kname, ktype = struct.unpack('16s s', self.buf[k0:k0+17]) 

        # ------------------------------------------
        # depending on type, select parsing strategy
        # ------------------------------------------
        kwfmt = '16s 80s'
        
        if ktype == 'L':   # keyword value is int64
            kwfmt = 'q 8x 80s'
        elif ktype == 'D': # keyword value is double
            kwfmt = 'd 8x 80s'
        elif ktype == 'S': # keyword value is string
            kwfmt = '16s 80s'
        elif ktype == 'N': # keyword is unused
            kwfmt = '16s 80s'
        
        kval, kcomm = struct.unpack(kwfmt, self.buf[k0+17:k0+kwsz])

        if kwfmt == '16s 80s':
            kval = str(kval).strip('\x00')

        # ------------------------------------------
        #    fill in the dictionary of keywords
        # ------------------------------------------
        self.kwds[ii]['name']    = str(kname).strip('\x00')
        self.kwds[ii]['type']    = ktype
        self.kwds[ii]['value']   = kval
        self.kwds[ii]['comment'] = str(kcomm).strip('\x00')

    def update_keyword(self, ii, name, value, comment):
        ''' --------------------------------------------------------------
        Update keyword data in dictionary and writes it to SHM file

        Parameters:
        ----------
        - ii      : index of the keyword to write (integer)
        - name    : the new keyword name 
        -------------------------------------------------------------- '''

        if (ii >= self.mtdata['nbkw']):
            print("Keyword index %d is not allocated and cannot be written")
            return

        # ------------------------------------------
        #    update relevant keyword dictionary
        # ------------------------------------------
        try:
            self.kwds[ii]['name'] = str(name).ljust(16, ' ')
        except:
            print('Keyword name not compatible (< 16 char)')

        if isinstance(value, (long, int)):
            self.kwds[ii]['type'] = 'L'
            self.kwds[ii]['value'] = long(value)
            
        elif isinstance(value, float):
            self.kwds[ii]['type'] = 'D'
            self.kwds[ii]['value'] = np.double(value)
            
        elif isinstance(value, str):
            self.kwds[ii]['type'] = 'S'
            self.kwds[ii]['value'] = str(value)
        else:
            self.kwds[ii]['type'] = 'N'
            self.kwds[ii]['value'] = str(value)

        try:
            self.kwds[ii]['comment'] = str(comment).ljust(80, ' ')
        except:
            print('Keyword comment not compatible (< 80 char)')

        # ------------------------------------------
        #          write keyword to SHM
        # ------------------------------------------
        self.write_keyword(ii)
        
    def write_keyword(self, ii):
        ''' --------------------------------------------------------------
        Write keyword data to shared memory.

        Parameters:
        ----------
        - ii      : index of the keyword to write (integer)
        -------------------------------------------------------------- '''

        if (ii >= self.mtdata['nbkw']):
            print("Keyword index %d is not allocated and cannot be written")
            return

        kwsz = self.kwsz
        k0   = self.im_offset + self.img_len + ii * kwsz # kword offset
        
        # ------------------------------------------
        #    read the keyword dictionary
        # ------------------------------------------
        kname = bytes(self.kwds[ii]['name'], "utf-8")
        ktype = self.kwds[ii]['type']
        kval  = self.kwds[ii]['value']
        kcomm = bytes(self.kwds[ii]['comment'], "utf-8")

        if ktype == 'L':
            kwfmt = '=16s s q 8x 80s'
        elif ktype == 'D':
            kwfmt = '=16s s d 8x 80s'
        elif ktype == 'S':
            kwfmt = '=16s s 16s 80s'
        elif ktype == 'N':
            kwfmt = '=16s s 16s 80s'

        self.buf[k0:k0+kwsz] = struct.pack(kwfmt, kname, ktype, kval, kcomm) 

    def print_meta_data(self):
        ''' --------------------------------------------------------------
        Basic printout of the content of the mtdata dictionary.
        -------------------------------------------------------------- '''
        fmts = self.hdr_fmt.split(' ')
        for i, fmt in enumerate(fmts):
            print(mtkeys[i], self.mtdata[mtkeys[i]])

    def select_dtype(self):
        ''' --------------------------------------------------------------
        Based on the value of the 'atype' code used in SHM, determines
        which numpy data format to use.
        -------------------------------------------------------------- '''
        atype        = self.mtdata['atype']
        self.npdtype = all_dtypes[atype-1]
        self.img_len = self.mtdata['nel'] * self.npdtype().itemsize

    def select_atype(self):
        ''' --------------------------------------------------------------
        Based on the type of numpy data provided, sets the appropriate
        'atype' value in the metadata of the SHM file.
        -------------------------------------------------------------- '''
        for i, mydt in enumerate(all_dtypes):
            if mydt == self.npdtype:
                self.mtdata['atype'] = i+1
        return(self.mtdata['atype'])

    def get_counter(self,):
        ''' --------------------------------------------------------------
        Read the image counter from SHM
        -------------------------------------------------------------- '''
        c0   = self.c0_offset                           # counter offset
        cntr = struct.unpack('Q', self.buf[c0:c0+8])[0] # read from SHM
        self.mtdata['cnt0'] = cntr                      # update object mtdata
        return(cntr)

    def get_frame_id(self,):
        ''' --------------------------------------------------------------
        Read the image counter from SHM
        -------------------------------------------------------------- '''
        c2   = self.c2_offset                           # counter offset
        cntr = struct.unpack('Q', self.buf[c2:c2+8])[0] # read from SHM
        self.mtdata['cnt2'] = cntr                      # update object mtdata
        return(cntr)

    def increment_counter(self,):
        ''' --------------------------------------------------------------
        Increment the image counter. Called when writing new data to SHM
        -------------------------------------------------------------- '''
        c0                  = self.c0_offset         # counter offset
        cntr                = self.get_counter() + 1 # increment counter
        self.buf[c0:c0+8]   = struct.pack('Q', cntr) # update SHM file
        self.mtdata['cnt0'] = cntr                   # update object mtdata
        return(cntr)

    def set_counter(self, cntr):
        ''' --------------------------------------------------------------
        set the image counter
        -------------------------------------------------------------- '''
        c0                  = self.c0_offset         # counter offset
        self.buf[c0:c0+8]   = struct.pack('Q', cntr) # update SHM file
        self.mtdata['cnt0'] = cntr                   # update object mtdata
        return

    def set_frame_id(self, cntr):
        ''' --------------------------------------------------------------
        set the image counter
        -------------------------------------------------------------- '''
        c2                  = self.c2_offset         # counter offset
        self.buf[c2:c2+8]   = struct.pack('Q', cntr) # update SHM file
        self.mtdata['cnt2'] = cntr                   # update object mtdata
        return

    def set_timestamp(self, ):
        # Get current datetime object
        now = datetime.datetime.now()
        tv_sec = int(now.timestamp())
        tv_nsec = now.microsecond * 1000

        start = self.ts_offset
        self.buf[start:start+8] = struct.pack('q', tv_sec)
        self.buf[start+8:start+16] = struct.pack('q', tv_nsec)

        self.mtdata['tvsec']  = tv_sec
        self.mtdata['tvnsec'] = tv_nsec
        return
    
    def get_timestamp(self, ):
        start = self.ts_offset
        tv_sec  = struct.unpack('q', self.buf[start:start+8])[0] # read from SHM
        tv_nsec = struct.unpack('q', self.buf[start+8:start+16])[0] # read from SHM
        self.mtdata['tvsec']  = tv_sec
        self.mtdata['tvnsec'] = tv_nsec
        
        # now I have tv_sec and tv_nsec we convert to a datetime
        return datetime.datetime.fromtimestamp(tv_sec) + datetime.timedelta(microseconds=tv_nsec/1000)

    def get_data(self, check=False, reform=True, semNb=0):
        ''' --------------------------------------------------------------
        Reads and returns the data part of the SHM file

        Parameters:
        ----------
        - check: integer (last index) if not False, waits image update
        - reform: boolean, if True, reshapes the array in a 2-3D format
        -------------------------------------------------------------- '''
        i0 = self.im_offset                                  # image offset
        i1 = i0 + self.img_len                               # image end

        if check is not False:
            self.semaphores[semNb].acquire()
#            while self.get_counter() <= check:
                #sys.stdout.write('\rcounter = %d' % (c0,))
                #sys.stdout.flush()
#                pass#time.sleep(0.001)

            #sys.stdout.write('---\n')

        data = np.fromstring(self.buf[i0:i1],dtype=self.npdtype) # read img

        if reform:
            rsz = self.mtdata['size'][:self.mtdata['naxis']]
            data = np.reshape(data, rsz)
        return(data)

    def set_data(self, data, check_dt=False, frameId=None, ts=True):
        ''' --------------------------------------------------------------
        Upload new data to the SHM file.

        Parameters:
        ----------
        - data: the array to upload to SHM
        - check_dt: boolean (default: false) recasts data

        Note:
        ----

        The check_dt is available here for comfort. For the sake of
        performance, data should be properly cast to start with, and
        this option not used!
        -------------------------------------------------------------- '''
        i0 = self.im_offset                                      # image offset
        i1 = i0 + self.img_len                                   # image end
        if check_dt is True:
            self.buf[i0:i1] = data.astype(self.npdtype()).tostring()
        else:
            try:
                self.buf[i0:i1] = data.tostring()
            except:
                print("Warning: writing wrong data-type to shared memory")
                return
        self.increment_counter()
        if frameId:
            self.set_frame_id(frameId)
        if ts:
            self.set_timestamp()
        for k in range(IMAGE_NB_SEMAPHORE):
            if self.semaphores[k].value < SEMAPHORE_MAXVAL:
                self.semaphores[k].release()

        return

    def save_as_fits(self, fitsname):
        ''' --------------------------------------------------------------
        Convenient sometimes, to be able to export the data as a fits file.
        
        Parameters:
        ----------
        - fitsname: a filename (clobber=True)
        -------------------------------------------------------------- '''
#        pf.writeto(fitsname, self.get_data(), clobber=True)
        return(0)


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
# =================================================================
# =================================================================

#class shmData:
#    def __init__(self, shmObj):
#        self.mtdata = shmObj.get_meta_data()
#        self.data   = shmObj.get_data()
#
#from json import JSONEncoder
#import json
#
## A class without JSON Serialization support
#class Class_No_JSONSerialization:
#        pass
#
## A specialised JSONEncoder that encodes Shared Memmory Object (shm)
## objects as JSON
#class shmEncoder(JSONEncoder):
#
#    def default(self, object):
#
#        if isinstance(object, shm):
#
#            return object.__dict__
#
#        else:
#
#            # call base class implementation which takes care of
#
#            # raising exceptions for unsupported types
#
#            return json.JSONEncoder.default(self, object)

    
        