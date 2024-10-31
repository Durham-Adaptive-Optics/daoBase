# daoBase
basic tools for dao

# Prerequiries
## Linux package
Package for Redhat based distribution... to be adapted for other Linux distrib
```
yum install openssl-devel protobuf-devel gsl-devel numactl-devel glibc-devel ncurses-devel redis gtest-devel zeromq zeromq-devel
```
for ubuntu distribution:

```
sudo apt install -y libtool pkg-config build-essential autoconf automake python3 python-is-python3 libssl-dev libncurses5-dev libncursesw5-dev redis libgtest-dev libgsl-dev libzmq3-dev protobuf-compiler numactl libnuma-dev
```

## Waf
waf needs to be installed and in the path: see https://waf.io/book/.  
Our typical install
```
mkdir ~/bin
cd ~/bin
wget https://waf.io/waf-2.0.26
ln -s waf-2.0.26 waf
```
and add $HOME/bin in your path
## Python
a python install is needed. We usually used miniconda but any aother install will work.
here is our typical install command
```
mkdir -p ~/miniconda3
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ~/miniconda3/miniconda.sh
bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
rm -rf ~/miniconda3/miniconda.sh
~/miniconda3/bin/conda init bash
~/miniconda3/bin/conda init zsh
```
The following package should be installed
```
pip install posix_ipc zmq protobuf==3.20.0 astropy python-statemachine statemachine redis sphinx screeninfo
```

## PMG and ZMQ
need to have recent version of ZMQ and ensure PGM is install
```
git clone https://github.com/zeromq/libzmq.git
cd libzmq
mkdir build && cd build
cmake ..
make -j4
sudo make install
sudo ldconfig
```

# Environment
```
export DAOROOT=$HOME/DAOROOT
export DAODATA=$HOME/DAODATA
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DAOROOT/lib:$DAOROOT/lib64
export PATH=$PATH:$HOME/bin:$HOME/DAOROOT/bin
export PYTHONPATH=$PYTHONPATH:$DAOROOT/python
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$DAOROOT/lib/pkgconfig

```
!! BE SURE THE DAOROOT exists. In the example
```
mkdir $HOME/DAOROOT
```
# Build
```
waf configure --prefix=$DAOROOT
waf
waf install
```

# Documentation
Documents are built using doxygen and sphinx. To build the documents use the following command
```
waf build_docs
```

To clean the document build.
```
waf clean_docs
```

# Build Options
The following build options are available 

## Debug
Add debug flag to support debuging build and install with '-g' to support use of gdb
```
waf build --debug
```

## Sanitiser
Mostly used for developers it is available to check address sanitisation


```
waf build --santizer
```

## UNIT tests.

The build include unit tests under tests. These can be built and run by running with the following

```
waf --test
```
This will build and run the tests.

## How to use dao. The basic python example
From python console, import dao and create a shared memory of 10x10 zeros, read the content, add 1 and read the content again
```
(base) scetre@LAPTOP-53NU7121:~/dev/pyro4$ ipython
Python 3.12.3 | packaged by Anaconda, Inc. | (main, May  6 2024, 19:46:43) [GCC 11.2.0]
Type 'copyright', 'credits' or 'license' for more information
IPython 8.24.0 -- An enhanced Interactive Python. Type '?' for help.

In [1]: import dao

In [2]: import numpy as np

In [3]: shm=dao.shm('/tmp/test.im.shm', np.zeros((10,10)))
[2024-10-23 14:33:08,671] [LAPTOP-53NU7121] - daoShm [INFO] : /tmp/test.im.shm will be created or overwritten (daoShm.py:320)
2024-10-23_16:33:08:678232 [info] dao.c:daoShmImageCreate:817: shm name = test.im.shm
2024-10-23_16:33:08:678281 [info] dao.c:daoShmImageCreate:821: local name: test
2024-10-23_16:33:08:678285 [info] dao.c:daoShmImageCreate:838: Creation test_semlog
2024-10-23_16:33:08:678585 [info] dao.c:daoShmImageCreate:1396: Creating Semaphores
2024-10-23_16:33:08:678610 [info] dao.c:daoImageCreateSem:694: shm name = test.im.shm
2024-10-23_16:33:08:678613 [info] dao.c:daoImageCreateSem:698: local name: test
2024-10-23_16:33:08:678615 [info] dao.c:daoImageCreateSem:720: Closing semaphore
2024-10-23_16:33:08:678618 [info] dao.c:daoImageCreateSem:727: Remove associated files
2024-10-23_16:33:08:678762 [info] dao.c:daoImageCreateSem:735: Done
2024-10-23_16:33:08:678776 [info] dao.c:daoImageCreateSem:747: malloc semptr 10 entries
2024-10-23_16:33:08:678991 [info] dao.c:daoShmImageCreate:1398: Semaphores created

In [4]: shm.get_data()
Out[4]: 
array([[0., 0., 0., 0., 0., 0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0., 0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0., 0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0., 0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0., 0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0., 0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0., 0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0., 0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0., 0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0., 0., 0., 0., 0., 0.]])

In [5]: shm.set_data(shm.get_data()+1)

In [6]: shm.get_data()
Out[6]: 
array([[1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.]])


```
Open a second python console, this time only pass the shared memory name to connect to the existing shared memory and get the value which will match the ones you last set in the shared memory
```
(base) scetre@LAPTOP-53NU7121:~$ ipython
Python 3.12.3 | packaged by Anaconda, Inc. | (main, May  6 2024, 19:46:43) [GCC 11.2.0]
Type 'copyright', 'credits' or 'license' for more information
IPython 8.24.0 -- An enhanced Interactive Python. Type '?' for help.

In [1]: import dao

In [2]: shm=dao.shm('/tmp/test.im.shm')
[2024-10-23 14:37:10,617] [LAPTOP-53NU7121] - daoShm [INFO] : loading existing /tmp/test.im.shm  (daoShm.py:333)
2024-10-23_16:37:10:617998 [info] dao.c:daoShmShm2Img:441: shm name = test.im.shm
2024-10-23_16:37:10:618019 [info] dao.c:daoShmShm2Img:445: local name: test

In [3]: shm.get_data()
Out[3]: 
array([[1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.],
       [1., 1., 1., 1., 1., 1., 1., 1., 1., 1.]])


```

The shared memory is accessible from any python, c, julia or matlab code using dao API.

