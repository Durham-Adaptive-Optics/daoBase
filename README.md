# daoBase [![CI Workflow](https://github.com/Durham-Adaptive-Optics/daoBase/actions/workflows/main.yml/badge.svg)](https://github.com/Durham-Adaptive-Optics/daoBase/actions/workflows/main.yml) [![DOI](https://zenodo.org/badge/506638374.svg)](https://doi.org/10.5281/zenodo.17264151)
basic tools for dao

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

