# daoBase [![CI Workflow](https://github.com/Durham-Adaptive-Optics/daoBase/actions/workflows/main.yml/badge.svg)](https://github.com/Durham-Adaptive-Optics/daoBase/actions/workflows/main.yml) [![DOI](https://zenodo.org/badge/506638374.svg)](https://doi.org/10.5281/zenodo.17264151)
`daoBase` is the core library of **DAO** (Durham Adaptive Optics), a real-time control framework for adaptive optics instruments. It provides the shared-memory (`dao.shm`) and messaging primitives that every other DAO repository builds on: fixed-layout `IMAGE`/`IMAGE_METADATA` shared-memory buffers for passing frames, slopes, and commands between real-time processes with minimal latency, a ZMQ-based command/event/logging layer, and a component/state-machine framework (`daoComponent`) for structuring long-running RTC processes.

The core is written in C, with a C++ layer built on top for the component/threading/state-machine framework, and bindings for Python, Rust, Julia, and MATLAB — so a pipeline can mix languages freely while every process still talks over the same shared-memory buffers. See [`daoTools`](https://github.com/Durham-Adaptive-Optics/daoTools) for the application layer (centroiding, reconstruction, loop control, GUIs) built on top of this library.

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

# Installation

## Quick start (recommended)

For a first install, run the installer from the repo root:

```bash
./install.sh
```

It builds *everything*, end to end: detects your OS and installs the system
packages (`apt`/`dnf`/`yum`/`pacman`/`zypper`/`brew`), reuses Miniconda if you
already have it or installs it otherwise, proposes a dedicated `dao` conda env
(optional — decline it and it'll ask which existing env to use instead), gets
`waf`, asks where `DAOROOT`/`DAODATA` should live (default `/opt/dao/...`,
created with `sudo` and handed back to you if that needs root the first
time), installs the Python dependencies, and finally builds and installs
daoBase itself (`waf configure && waf && waf install`) — finishing with a
real `import daoShm` + shared-memory round-trip to confirm it actually works.

```bash
./install.sh --help
```

lists every option (skip a step, pick a different prefix, non-interactive
`--yes`, `--dry-run`, ...). It's idempotent — safe to re-run any time; it
reuses whatever it finds (existing conda, existing env, existing waf, ...)
rather than redoing work.

### Re-installing / updating after a `git pull`

If daoBase is already installed and only the source changed (no new system
dependency), you don't need the full installer again — from the repo root:

```bash
waf install
```

(or `waf configure --prefix=$DAOROOT && waf && waf install` if you changed
something `configure` cares about, e.g. enabling a newly-available optional
dependency like BLAS or CUDA). Re-running `./install.sh` in full is always
fine too — being idempotent, it will just confirm everything is already in
place; that's slower, not harmful, so use whichever you prefer.

### For developers of `install.sh`

[`test/install/`](test/install/README.md) holds a Docker-based harness that
runs `install.sh` on a disposable, fresh Ubuntu/Rocky container as a
non-root sudo user. It's there for anyone changing `install.sh` itself, to
check it still works end to end before it reaches a real machine — not
something you need as a regular user.

## Manual install (step by step)

The installer above automates everything below; this is kept as a reference
for doing it by hand, or for platform quirks (e.g. arm64 protobuf) it doesn't
cover.

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
pip install posix_ipc zmq protobuf astropy python-statemachine statemachine redis sphinx screeninfo
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

