#!/usr/bin/env python3

'''---------------------------------------------------------------------------
Read and write access to shared memory (SHM) structures used by SCExAO

- Author : Frantz Martinache
- Date   : July 12, 2017

Improved version of the original SHM structure used by SCExAO and friends.
---------------------------------------------------------------------------

Named semaphores seems to be something missing from the python API and may 
require the use of an external package.

A possibility:
http://semanchuk.com/philip/posix_ipc/

More info on semaphores:
https://www.mkssoftware.com/docs/man3/sem_open.3.asp
https://docs.python.org/2/library/threading.html#semaphore-objects
'''

import os, sys, mmap, struct
import numpy as np
import astropy.io.fits as pf
import time
#import pdb
import posix_ipc

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

class shm:
    def __init__(self, fname=None, data=None, verbose=False, packed=False, nbkw=0):
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
        for k in range(10):
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
        self.mtdata['sem']    = 10
        
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

#        self.mtdata['imname'] = self.mtdata['imname'].strip('\x00')
        self.im_offset = offset # offset for the image content

        if verbose:
            self.print_meta_data()

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

    def set_data(self, data, check_dt=False):
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
        for k in range(10):
            if self.semaphores[k].value < 10:
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

    def get_expt(self,):
        ''' --------------------------------------------------------------
        SCExAO specific: returns the exposure time (from keyword)
        -------------------------------------------------------------- '''
        ii0 = 3 # index of exposure time in keywords
        self.read_keyword(ii0)
        self.expt = self.kwds[ii0]['value']
        return self.expt

# =================================================================
# =================================================================
