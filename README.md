# daoBase
basic tools for dao

# Prerequiries
## Linux package
Package for Redhat based distribution... to be adapted for other Linux distrib
```
yum install openssl-devel protobuf-devel gsl-devel numactl-devel glibc-devel ncurses-devel redis gtest-devel
```
for ubuntu distribution:

```
sudo apt install -y libtool pkg-config build-essential autoconf automake python3 python-is-python3 libssl-dev libncurses5-dev libncursesw5-dev redis libgtest-dev libgsl-dev libzmq3-dev
```

## Waf
waf needs to be installed and in the path: see https://waf.io/book/.  

## Python
a python install is needed

```
pip install posix_ipc zmq protobuf astropy python-statemachine statemachine redis sphinx
```

# Environment
```
export DAOROOT=$HOME/DAOROOT
export DAODATA=$HOME/DAODATA
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DAOROOT/lib:$DAOROOT/lib64
export PATH=$PATH:$HOME/DAOROOT/bin
export PYTHONPATH=$DAOROOT/python

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


# UNIT tests.

The build include unit tests under tests. These can be built and run by uncommenting out the following lines of code in the wscript:

	bld.recurse('test')
	from waflib.Tools import
	waf_unit_testbld.add_post_fun(waf_unit_test.summary)
