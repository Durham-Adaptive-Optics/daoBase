# daoBase for Windows

These instructions explain the process of building and installing daoBase on Windows using the Waf script.

# Prerequisites:

## Software to install

You must ensure the following software is installed prior to building daoBase.
- protoc
- ZeroMQ
- MSVC
- pkg_config

*Microsoft Visual Studio*: must be installed https://visualstudio.microsoft.com/downloads/.

*ZeroMQ*: on Windows, it must be downloaded as a source package (from the [GitHub page](https://github.com/zeromq/libzmq)) and built.

*Protocol Buffers*: We use v3.20.0 of Protocol Buffers, download the "protobuf-cpp-3.20.0" source code from [here](https://github.com/protocolbuffers/protobuf/releases/tag/v3.20.0).

*pkg-config*: In this case, any Windows-compatible build of pkg-config should work. We used pkg-config-lite 0.28-1, which can be downloaded from [SourceForge](https://sourceforge.net/projects/pkgconfiglite/files/).

## Protocol Buffers

Extract the source code archive, and open the Visual Studio "x64 Native Tools" command prompt in this directory.

Run the following commands:

```
cd cmake
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=C:\Users\nwnw17\Documents\daoExperiments\protobuf-3.20.0_install ..
nmake
```

Following this, you should have a Windows build of Protocol Buffers.

## pkg-config

Pkg-config is used by the Waf build script to identify the location of the dependencies on the system. On Windows, it is necessary to create the pkg-config files manually.

Examples for the Protocol Buffers and ZeroMQ package files are provided - substitute the prefix path here with the suitable path on your system.

Beware that the backslash character *\\* must be used twice to be recognised correctly. For example, the path *C:\\Windows\\* must instead be written as *C:\\\\Windows\\\\* for the intended effect. It may just be easier to substitute with a forward slash character */* instead.

```
prefix=(your path to Protocol Buffers, e.g. C:/protoc-3.20.0-win64)
exec_prefix=${prefix}/cmake/build
libdir=${prefix}/cmake/build
includedir=${prefix}/src

Name: protobuf
Description: Protocol Buffers
Version: 3.20.0
Cflags: -I${includedir}
Libs: -L${libdir} -llibprotobuf
```

```
prefix=(your path to ZeroMQ, e.g. C:/zeromq-4.3.5/cmake-build)
exec_prefix=${prefix}
libdir=${prefix}/lib
includedir=${prefix}/include

Name: libzmq
Description: 0MQ c++ library
Version: 4.3.5
Libs: -L${libdir} -lzmq
Libs.private: -lstdc++ 
Requires.private: 
Cflags: -I${includedir} 
```

Name these files *protobuf.pc* and *zmq.pc* respectively, and save them both in a new folder. We will use this folder later.

## Miniconda

You will need a Python installation, both to build the system and to use the DAO Python bindings. We usually use Miniconda but any other installation should work.

Download the latest "Miniconda3 Windows 64-bit" installer from the [Anaconda website](https://docs.anaconda.com/miniconda/#miniconda-latest-installer-links), and follow the instructions in the graphical installer.

Once Miniconda is installed, you can open a Miniconda terminal session using the "Anaconda Prompt (miniconda3)" shortcut placed in your Start Menu.

You should then run the below command, which will install the necessary Python packages.

```
pip install zmq protobuf==3.20.0 astropy python-statemachine statemachine redis sphinx screeninfo
```

## Waf

Download the Waf build system from the following link: https://waf.io/waf-2.0.26

Place it somewhere suitable on your system.

## Environment

At the Miniconda command prompt, enter the following commands to set up the appropriate paths. These must be set every time you want to use DAO. You may wish to create a .bat file with these commands so that you do not need to retype them in subsequent sessions.

```
set DAOROOT= (absolute path to your DAOROOT directory)
set DAODATA= (absolute path to your DAODATA directory)
set PATH=%PATH%;%DAOROOT%\bin; (absolute path to your Protocol Buffers bin folder)
set PYTHONPATH=%PYTHONPATH%;%DAOROOT%\python
set PKG_CONFIG_PATH=%PKG_CONFIG_PATH%;%DAOROOT%\lib\pkgconfig; (absolute path to your pkg-config package folder, created earlier).
```

## Build

On each of the following lines, make sure to substitute *waf* with the absolute path to your copy of Waf.

```
python waf configure --prefix=%DAOROOT%
python waf
python waf install
```

## Documentation

There is currently no way to build the documentation on Windows. This will be addressed shortly.

# Build options

## Debug

Pass the --debug option when building to enable debugging support.

## Sanitiser

Address sanitisation is not currently available on Windows.

## Unit tests

There is currently no way to build and run the unit tests on Windows. These tests must first be ported.

# Limitations:

libdaoProto is not yet compiled on Windows. This is due to a problem which should be solved soon.

Unlike DAO on Linux/Mac, you cannot create a shared memory file over a file which already exists, if said file is still in use by another process on the system.
