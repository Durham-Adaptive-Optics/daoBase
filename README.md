# daoBase
Basic tools for dao

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

# Build Steps
* mkdir build 
* cd build
* cmake -DCMAKE_BUILD_TYPE=[debug|asan|release] ../
* cmake --build ./
* cmake --install ./ --prefix [install-path]

# Documentation
Documents are built using doxygen and sphinx. To build the documents use the following command


# Build Options
The following build options are available 

## Debug: -DCMAKE_BUILD_TYPE=debug
Add debug information and disables all optimisations.

## Address Sanitization: -DCMAKE_BUILD_TYPE=asan
Mostly used for developers it is available to check address sanitisation.

## Release: -DCMAKE_BUILD_TYPE=release
Enables program optimizations (no debug information provided).

# Testing
