Installation Guide
==================

This guide provides comprehensive installation instructions for DAO (Durham Adaptive Optics) on Linux, macOS, and Windows platforms.

.. note::
   For Windows build instructions, see the dedicated :ref:`windows-installation` section below.

Prerequisites
=============

Linux (RedHat-based distributions)
-----------------------------------

For RedHat-based distributions (CentOS, RHEL, Fedora):

.. code-block:: bash

   yum install openssl-devel protobuf-devel gsl-devel numactl-devel glibc-devel ncurses-devel redis gtest-devel zeromq zeromq-devel

Linux (Ubuntu/Debian)
----------------------

For Ubuntu and Debian-based distributions:

.. code-block:: bash

   sudo apt install -y libtool pkg-config build-essential autoconf automake python3 python-is-python3 libssl-dev libncurses5-dev libncursesw5-dev redis libgtest-dev libgsl-dev libzmq3-dev protobuf-compiler numactl libnuma-dev

macOS
-----

You need Homebrew for package management on macOS:

.. code-block:: bash

   brew install pkg-config zeromq protobuf gsl

If you use Anaconda, there could be a conflict between protobuf versions, so uninstall the conda version:

.. code-block:: bash

   conda uninstall libprotobuf

Linux ARM64
-----------

ARM64 systems may encounter protobuf version mismatches. The recommended solution is to rebuild from source.

First, install Abseil:

.. code-block:: bash

   git clone https://github.com/abseil/abseil-cpp.git
   cd abseil-cpp
   mkdir build && cd build
   cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release
   make -j$(nproc)
   sudo make install

Then install Protocol Buffers:

.. code-block:: bash

   wget https://github.com/protocolbuffers/protobuf/releases/download/v25.3/protobuf-25.3.tar.gz
   tar zxvf protobuf-25.3.tar.gz
   cd protobuf-25.3
   mkdir -p cmake/build
   cd cmake/build

   cmake ../.. \
     -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
     -Dprotobuf_BUILD_TESTS=OFF \
     -Dprotobuf_ABSL_PROVIDER=package

   make -j$(nproc)
   sudo make install
   sudo ldconfig

Build System (Waf)
==================

DAO uses the Waf build system. Install it as follows:

.. code-block:: bash

   mkdir ~/bin
   cd ~/bin
   wget https://waf.io/waf-2.0.26
   ln -s waf-2.0.26 waf
   chmod u+x *

Add ``$HOME/bin`` to your PATH environment variable.

Python Environment
==================

A Python installation is required. We recommend Miniconda, but any Python distribution will work.

Linux x86_64
-------------

.. code-block:: bash

   mkdir -p ~/miniconda3
   wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ~/miniconda3/miniconda.sh
   bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
   rm -rf ~/miniconda3/miniconda.sh
   ~/miniconda3/bin/conda init bash
   ~/miniconda3/bin/conda init zsh

Linux ARM64
-----------

.. code-block:: bash

   mkdir -p ~/miniconda3
   curl -L https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-aarch64.sh -o ~/miniconda3/miniconda.sh
   bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
   rm -rf ~/miniconda3/miniconda.sh
   ~/miniconda3/bin/conda init bash
   ~/miniconda3/bin/conda init zsh

macOS ARM64
-----------

.. code-block:: bash

   mkdir -p ~/miniconda3
   curl -L https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-arm64.sh -o ~/miniconda3/miniconda.sh
   bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
   rm -rf ~/miniconda3/miniconda.sh
   ~/miniconda3/bin/conda init bash
   ~/miniconda3/bin/conda init zsh

Python Packages
================

Install the required Python packages:

.. code-block:: bash

   pip install posix_ipc zmq protobuf==3.20.0 astropy python-statemachine statemachine redis sphinx screeninfo

For macOS, check your protoc version and install the corresponding protobuf:

.. code-block:: bash

   protoc --version
   # If version is 29.3, install:
   pip install protobuf==5.29.3

Environment Variables
=====================

Linux/macOS
-----------

Add these environment variables to your shell configuration file (``.bashrc``, ``.zshrc``, etc.):

.. code-block:: bash

   export DAOROOT=$HOME/DAOROOT
   export DAODATA=$HOME/DAODATA
   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DAOROOT/lib:$DAOROOT/lib64
   export PATH=$PATH:$HOME/bin:$HOME/DAOROOT/bin
   export PYTHONPATH=$PYTHONPATH:$DAOROOT/python
   export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$DAOROOT/lib/pkgconfig

For macOS, add these additional variables:

.. code-block:: bash

   export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:/opt/homebrew/lib
   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/homebrew/lib
   export CPLUS_INCLUDE_PATH=$CPLUS_INCLUDE_PATH:/opt/homebrew/include
   export C_INCLUDE_PATH=$C_INCLUDE_PATH:/opt/homebrew/include
   export CPATH=$CPATH:/opt/homebrew/include
   export PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig

.. important::
   Ensure that the DAOROOT directory exists:
   
   .. code-block:: bash
   
      mkdir $HOME/DAOROOT

Building DAO
============

Once all prerequisites are installed and environment variables are set:

.. code-block:: bash

   waf configure --prefix=$DAOROOT
   waf
   waf install

Build Options
=============

Debug Build
-----------

To build with debugging support:

.. code-block:: bash

   waf build --debug

Address Sanitizer
-----------------

For development and debugging (developers only):

.. code-block:: bash

   waf build --sanitizer

Unit Tests
----------

To build and run unit tests:

.. code-block:: bash

   waf --test

Documentation
=============

To build the documentation (requires Doxygen and Sphinx):

.. code-block:: bash

   waf build_docs

To clean the documentation build:

.. code-block:: bash

   waf clean_docs

.. _windows-installation:

Windows Installation
====================

These instructions explain how to build and install daoBase on Windows using the Waf script.

Prerequisites
-------------

You must ensure the following software is installed:

- **Microsoft Visual Studio**: Download from https://visualstudio.microsoft.com/downloads/
- **Protocol Buffers v3.20.0**: Download "protobuf-cpp-3.20.0" from https://github.com/protocolbuffers/protobuf/releases/tag/v3.20.0
- **ZeroMQ**: Download source from https://github.com/zeromq/libzmq
- **pkg-config**: Use pkg-config-lite 0.28-1 from https://sourceforge.net/projects/pkgconfiglite/files/

Building Protocol Buffers
--------------------------

1. Extract the Protocol Buffers source code
2. Open Visual Studio "x64 Native Tools" command prompt in the extracted directory
3. Run the following commands:

.. code-block:: batch

   cd cmake
   mkdir build
   cd build
   cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=C:\path\to\protobuf\install ..
   nmake

pkg-config Setup
-----------------

Create pkg-config files manually for the dependencies. 

For Protocol Buffers, create ``protobuf.pc``:

.. code-block:: ini

   prefix=C:/path/to/protobuf
   exec_prefix=${prefix}/cmake/build
   libdir=${prefix}/cmake/build
   includedir=${prefix}/src

   Name: protobuf
   Description: Protocol Buffers
   Version: 3.20.0
   Cflags: -I${includedir}
   Libs: -L${libdir} -llibprotobuf

For ZeroMQ, create ``zmq.pc``:

.. code-block:: ini

   prefix=C:/path/to/zeromq/cmake-build
   exec_prefix=${prefix}
   libdir=${prefix}/lib
   includedir=${prefix}/include

   Name: libzmq
   Description: 0MQ c++ library
   Version: 4.3.5
   Libs: -L${libdir} -lzmq
   Libs.private: -lstdc++
   Cflags: -I${includedir}

Save both files in a dedicated folder for later use.

Python Setup (Windows)
-----------------------

1. Download and install Miniconda from https://docs.anaconda.com/miniconda/
2. Open "Anaconda Prompt (miniconda3)" from the Start Menu
3. Install required packages:

.. code-block:: batch

   pip install zmq protobuf==3.20.0 astropy python-statemachine statemachine redis sphinx screeninfo

Waf Installation
-----------------

Download Waf from https://waf.io/waf-2.0.26 and place it in a suitable location.

Environment Variables (Windows)
--------------------------------

Set these variables in the Miniconda command prompt (create a .bat file for convenience):

.. code-block:: batch

   set DAOROOT=C:\path\to\your\DAOROOT
   set DAODATA=C:\path\to\your\DAODATA
   set PATH=%PATH%;%DAOROOT%\bin;C:\path\to\protobuf\bin
   set PYTHONPATH=%PYTHONPATH%;%DAOROOT%\python
   set PKG_CONFIG_PATH=%PKG_CONFIG_PATH%;%DAOROOT%\lib\pkgconfig;C:\path\to\pkg-config\folder

Building on Windows
--------------------

.. code-block:: batch

   python waf configure --prefix=%DAOROOT%
   python waf
   python waf install

Windows Limitations
-------------------

- Documentation building is not currently supported on Windows
- Address sanitization is not available
- Unit tests are not yet ported to Windows
- libdaoProto compilation is not yet supported
- Cannot overwrite shared memory files that are in use by other processes

Troubleshooting
===============

common errors:

.. code-block:: console

  daoLogging.pb.h:15:2: error: #error "This file was generated by a newer version of protoc which is"
     15 | #error "This file was generated by a newer version of protoc which is"
        |  ^~~~~
  daoLogging.pb.h:16:2: error: #error "incompatible with your Protocol Buffer headers. Please update"
     16 | #error "incompatible with your Protocol Buffer headers. Please update"
        |  ^~~~~
  daoLogging.pb.h:17:2: error: #error "your headers."
     17 | #error "your headers."
        |  ^~~~~
  In file included from daoLogging.pb.cc:4:
  daoLogging.pb.h:29:10: fatal error: google/protobuf/generated_message_tctable_decl.h: No such file or directory
     29 | #include "google/protobuf/generated_message_tctable_decl.h"
        |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  compilation terminated.

This is caused by the wrong protoc being used and typcally the one comes with anaconda.


fix by

.. code-block:: shell

  which protoc

.. code-block:: shell

  protoc --version

.. code-block:: shell

  conda uninstall protobuf

.. code-block:: shell

  rm $(which protoc)

Then install the correct protoc.
