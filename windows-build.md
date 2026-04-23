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

# Automated Installation Script

An automated PowerShell script (`install-windows-deps.ps1`) is provided to simplify the installation of all prerequisites. This script automates the manual steps described above.

## Prerequisites for Automated Installation

- Windows 10 or later
- PowerShell (included with Windows)
- Administrator privileges
- Internet connection

## What the Script Installs

The script automatically downloads, builds, and configures:

1. **Microsoft Visual Studio Build Tools 2022** (via winget)
2. **ZeroMQ 4.3.5** (built from source)
3. **Protocol Buffers 3.20.0** (built from source)
4. **pkg-config-lite 0.28-1** (with automatic .pc file creation)
5. **Miniconda** (Python environment)
6. **Required Python packages** (zmq, protobuf==3.20.0, astropy, python-statemachine, statemachine, redis, sphinx, screeninfo)
7. **Waf 2.0.26** (build system)

## Running the Automated Installation

### Basic Usage (Default Locations)
```powershell
# Run PowerShell as Administrator
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
.\install-windows-deps.ps1
```

### Custom Installation Paths
```powershell
.\install-windows-deps.ps1 -InstallDir "D:\my-dao-deps" -DaoRoot "D:\dao" -DaoData "D:\dao-data"
```

## Script Parameters

- **`-InstallDir`** (default: `C:\daoBase-deps`): Where dependencies will be installed
- **`-DaoRoot`** (default: `C:\daoBase`): DAO installation directory  
- **`-DaoData`** (default: `C:\daoData`): DAO data directory

## After Automated Installation

The script automatically sets persistent environment variables (`DAOROOT`, `DAODATA`, `PATH`, `PYTHONPATH`, and `PKG_CONFIG_PATH`) for the current user. These will be available in new terminal sessions.

For immediate use, open a new PowerShell/Command Prompt window, or use the backup environment setup script that's created:

```cmd
C:\daoBase-deps\setup-dao-env.bat
```

Then build daoBase as described in the [Build](#build) section above:

```cmd
cd path\to\daoBase\source
python "C:\daoBase-deps\waf-2.0.26" configure --prefix=%DAOROOT%
python "C:\daoBase-deps\waf-2.0.26"
python "C:\daoBase-deps\waf-2.0.26" install
```

## Installation Directory Structure

After running the automated script:

```
C:\daoBase-deps\
├── downloads\          # Downloaded source archives
├── build\             # Build directories
├── zeromq\            # ZeroMQ installation
├── protobuf\          # Protocol Buffers installation
├── pkg-config-lite\   # pkg-config installation
├── pkg-config\lib\    # Auto-generated .pc files
├── miniconda3\        # Python environment
├── waf-2.0.26         # Waf build system
└── setup-dao-env.bat  # Environment setup script
```

## Troubleshooting Automated Installation

- **"Execution Policy" errors**: Run `Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser` first
- **Administrator required**: The script must be run as Administrator to install Visual Studio Build Tools
- **Download failures**: Check internet connection and firewall settings
- **Build failures**: Ensure Visual Studio Build Tools installed correctly; try running from "x64 Native Tools Command Prompt"

If the automated installation fails, you can still follow the manual installation steps described in the sections above.

## Uninstalling Dependencies

An uninstall script (`uninstall-windows-deps.ps1`) is provided to completely remove all installed dependencies and clean up environment variables.

### Basic Uninstall
```powershell
# Run PowerShell as Administrator (recommended)
.\uninstall-windows-deps.ps1
```

### Uninstall with Options
```powershell
# Keep Miniconda installation
.\uninstall-windows-deps.ps1 -KeepMiniconda

# Force removal without confirmation
.\uninstall-windows-deps.ps1 -Force

# Custom paths (if you used custom paths during installation)
.\uninstall-windows-deps.ps1 -InstallDir "D:\my-dao-deps" -DaoRoot "D:\dao" -DaoData "D:\dao-data"
```

### What Gets Removed
- All dependency installations (ZeroMQ, Protocol Buffers, pkg-config, Waf)
- DAO directories (`DAOROOT` and `DAODATA`)
- Environment variables (`DAOROOT`, `DAODATA`, and DAO-related PATH entries)
- Visual Studio Build Tools (optional, with confirmation)
- Miniconda (optional, with confirmation)
