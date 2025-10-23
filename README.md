# daoBase [![CI Workflow](https://github.com/Durham-Adaptive-Optics/daoBase/actions/workflows/main.yml/badge.svg)](https://github.com/Durham-Adaptive-Optics/daoBase/actions/workflows/main.yml) [![DOI](https://zenodo.org/badge/506638374.svg)](https://doi.org/10.5281/zenodo.17264151)
basic tools for dao

## 📖 Documentation

The complete documentation for installation, usage, and API reference can be found here:

[**Read the Docs**](https://daobase.readthedocs.io/en/latest/)

---

## Citation

If you use this software in your research or work, please cite it using the following DOI:

**DOI:** [10.5281/zenodo.17264152](https://doi.org/10.5281/zenodo.17264152)

### BibTeX

```bibtex
@software{barr_2025_17264152,
  author       = {Barr, David and
                  Cetre, Sylvain and
                  Connolly, John and
                  Thomas Davies},
  title        = {Durham-Adaptive-Optics/daoBase: Initial Release},
  month        = oct,
  year         = 2025,
  publisher    = {Zenodo},
  version      = {v0.0.1},
  doi          = {10.5281/zenodo.17264152},
  url          = {https://doi.org/10.5281/zenodo.17264152},
  swhid        = {swh:1:dir:e28d715b004461ad5ac69d7ac13d5a4a0f8fc196
                   ;origin=https://doi.org/10.5281/zenodo.17264151;vi
                   sit=swh:1:snp:c39ecfb0c6dc79d3ad9523b5cdd88dc1a14f
                   43e2;anchor=swh:1:rel:b822be04b6dda609af461bb157fc
                   b1ec74853023;path=Durham-Adaptive-Optics-daoBase-
                   ccd63ee
                  },
}
```

---

# Basic Build
If you just want to use DAO in python, you can just use pip.

```
git clone git@github.com:Durham-Adaptive-Optics/daoBase.git
cd daoBase
pip install .
```
then in your python3 environement:
```
(base) daouser@scetre-MS-7D98:~$ ipython3
Python 3.13.5 | packaged by Anaconda, Inc. | (main, Jun 12 2025, 16:09:02) [GCC 11.2.0]
Type 'copyright', 'credits' or 'license' for more information
IPython 9.6.0 -- An enhanced Interactive Python. Type '?' for help.
Tip: You can change the editing mode of IPython to behave more like vi, or emacs.

In [1]: import numpy as np

In [2]: import dao

In [3]: a=dao.shm('/tmp/test.im.shm', np.zeros((5,5)))
[2025-10-23 08:06:03,208] [scetre-MS-7D98] - dao.daoShm [INFO] : /tmp/test.im.shm will be created or overwritten (daoShm.py:511)
2025-10-23_10:06:03:208490 [info] dao.c:daoShmImageCreate:1234: shm name = test.im.shm
2025-10-23_10:06:03:208500 [info] dao.c:daoShmImageCreate:1238: local name: test
2025-10-23_10:06:03:208502 [info] dao.c:daoShmImageCreate:1278: Creation test_semlog
2025-10-23_10:06:03:208553 [info] dao.c:daoShmImageCreate:1884: Creating Semaphores
2025-10-23_10:06:03:208555 [info] dao.c:daoImageCreateSem:1056: shm name = test.im.shm
2025-10-23_10:06:03:208556 [info] dao.c:daoImageCreateSem:1060: local name: test
2025-10-23_10:06:03:208558 [info] dao.c:daoImageCreateSem:1104: Closing semaphore
2025-10-23_10:06:03:208559 [info] dao.c:daoImageCreateSem:1111: Remove associated files
2025-10-23_10:06:03:208615 [info] dao.c:daoImageCreateSem:1119: Done
2025-10-23_10:06:03:208617 [info] dao.c:daoImageCreateSem:1132: malloc semptr 10 entries
2025-10-23_10:06:03:208647 [info] dao.c:daoShmImageCreate:1886: Semaphores created

In [4]: a.get_data()
Out[4]:
array([[0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0.],
       [0., 0., 0., 0., 0.]])
```
Depending of your python installation, you can have issue with GLIBC, if you see an error like 
```
/.../libstdc++.so.6: version `GLIBCXX_3.4.32' not found
```
in conda you can try to fix it with 
```
conda install -c conda-forge libstdcxx-ng>=12 libgcc-ng>=12
```


# Build
(for build instructions on Windows, see windows-build.md)
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

## MAC

You need homebrew for the packages.
```
brew install pkg-config zeromq protobuf gsl

```

if you use anaconda there could be a conflict between protobuf versions so uninstall using

```
conda uninstall libprotobuf
```
## Linux arm64
We encoutered several issue with the protobuf, usually related to the librptobuf vs protox version mismatch. One simple solution was to rebuild from the source 

```
git clone https://github.com/abseil/abseil-cpp.git
cd abseil-cpp
mkdir build && cd build
cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```
then protobuf
```
wget https://github.com/protocolbuffers/protobuf/releases/download/v25.3/protobuf-25.3.tar.gz
tar zxvf protobuf-25.4.tar.gz
cd protobuf-25.3-full
mkdir -p cmake/build
cd cmake/build

cmake ../.. \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -Dprotobuf_BUILD_TESTS=OFF \
  -Dprotobuf_ABSL_PROVIDER=package

make -j$(nproc)
sudo make install
sudo ldconfig
```
and 

## Waf
waf needs to be installed and in the path: see https://waf.io/book/.  
Our typical install
```
mkdir ~/bin
cd ~/bin
wget https://waf.io/waf-2.0.26
ln -s waf-2.0.26 waf
chmod u+x *
```
and add $HOME/bin in your path
## Python
a python install is needed. We usually used miniconda but any aother install will work.
here is our typical install command. For Linux x86_64
```
mkdir -p ~/miniconda3
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ~/miniconda3/miniconda.sh
bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
rm -rf ~/miniconda3/miniconda.sh
~/miniconda3/bin/conda init bash
~/miniconda3/bin/conda init zsh
```
Linux arm64
```
mkdir -p ~/miniconda3
curl -L https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-aarch64.sh -o ~/miniconda3/miniconda.sh
bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
rm -rf ~/miniconda3/miniconda.sh
~/miniconda3/bin/conda init bash
~/miniconda3/bin/conda init zsh
```
For MACOS arm64 install
```
mkdir -p ~/miniconda3
curl -L https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-arm64.sh -o ~/miniconda3/miniconda.sh
bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
rm -rf ~/miniconda3/miniconda.sh
~/miniconda3/bin/conda init bash
~/miniconda3/bin/conda init zsh
```
The following package should be installed
```
pip install posix_ipc zmq protobuf==3.20.0 astropy python-statemachine statemachine redis sphinx screeninfo
```
For MACOS, check the version of your protoc
```
protoc --version
libprotoc 29.3
```
Corresponding protobuf is 
```
pip install protobuf==5.29.3
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
For MACOS, add the following variables in your environment
```
export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:/opt/homebrew/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/homebrew/lib
 
export CPLUS_INCLUDE_PATH=$CPLUS_INCLUDE_PATH:/opt/homebrew/include
export C_INCLUDE_PATH=$C_INCLUDE_PATH:/opt/homebrew/include
export CPATH=$CPATH:/opt/homebrew/include
export PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig
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

