# daoBase
basic tools for dao: shm, logging, error,...

# prerequiries
waf needs to be installed and in the path: see https://waf.io/book/

# environment
```
export DAOROOT=$HOME/DAOROOT
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DAOROOT/lib:$DAOROOT/lib64
export PATH=$PATH:$HOME/DAOROOT/bin
```

!! BE SURE THE DAOINTROOT exists. In the example
```
mkdir $HOME/DAOROOT
```

# build
waf configure --prefix=$DAOROOT
waf
wad install
