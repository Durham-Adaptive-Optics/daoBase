# daoBase
basic tools for dao: shm, logging, error,...

# Prerequiries
waf needs to be installed and in the path: see https://waf.io/book/.  
Another dependency is the posix_ipc python module. Usually the installation is as simple as 
```
pip3 install posix_ipc
pip3 install astropy
```

# Environment
```
export DAOROOT=$HOME/DAOROOT
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
