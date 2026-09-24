# Python bindings

`daoShm.py` is a ctypes wrapper around `libdao`'s SHM API (see
[`../../include/dao.h`](../../include/dao.h)), giving Python the same
create / open / read / write / close interface as the Julia, MATLAB and
Rust bindings in this repo.

## Setup

`waf install` (see [the top-level README](../../README.md)) copies every
`.py` file in this directory to `$DAOROOT/python`, which `dao_env.sh` adds
to `PYTHONPATH`. Once that's sourced, `import daoShm` just works.

To run against an in-tree `waf build` instead (no install), point
`LD_LIBRARY_PATH` at `build/src` and `PYTHONPATH` at this directory.

## Usage

```python
import numpy as np
from daoShm import shm

# Create a new SHM (or overwrite an existing one) sized/typed from the array.
s = shm("/tmp/test.im.shm", data=np.zeros((4, 4), dtype=np.float32))

# Attach to an existing SHM from another process.
s2 = shm("/tmp/test.im.shm")

s.set_data(np.ones((4, 4), dtype=np.float32))   # write the next frame
data = s2.get_data()                            # read the latest frame

# Block until a new frame is posted (semaphore 0 by default), then read it.
data = s2.get_data(check=True)

s.close()
s2.close()
```

`shm` also exposes `get_data_next`/`get_data_arbitrary`/`get_history` for
walking a multi-slot (FIFO) SHM's history, `get_meta_data`/`get_counter`/
`get_frame_id`/`get_timestamp` for metadata, and `reset_tail` to resync a
reader that has fallen behind. See the docstrings in `daoShm.py`.

## Example

```bash
python3 example_shm.py
```

`example_shm.py` creates a SHM, writes to it, reads it back through a
second handle, and closes both - see that file for the full walkthrough.

## Naming

`daoShm.py`'s low-level ctypes bindings call `libdao`'s primary API
(`daoShmCreate`, `daoShmOpen`, `daoShmSetData`, `daoShmGetData`, ...). The
older C names (`daoShmImageCreate`, `daoShmShm2Img`, `daoShmImage2Shm`,
`daoShmGetNewestSegment`, ...) still exist in `libdao` for other languages'
or projects' compatibility, but this module doesn't use them.
