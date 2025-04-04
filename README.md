# daoBase
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
here is our typical install command
```
mkdir -p ~/miniconda3
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ~/miniconda3/miniconda.sh
bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
rm -rf ~/miniconda3/miniconda.sh
~/miniconda3/bin/conda init bash
~/miniconda3/bin/conda init zsh
```
For MACOS install
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

