# MATLAB bindings

`daoShm.m` is a MATLAB class wrapping `daomex.c`, a MEX file that calls
`libdao`'s SHM API (see [`../../include/dao.h`](../../include/dao.h)). It
gives MATLAB the same create / open / read / write / close interface as
the Python, Julia and Rust bindings in this repo.

Unlike the Python/Julia bindings, this one isn't built by `waf
install` - you compile `daomex.c` yourself, once, with MATLAB's `mex`.

## Setup

Build the MEX file (from this directory), pointing it at daoBase's headers
and `libdao`:

```matlab
mex daomex.c -I$DAOROOT/include -L$DAOROOT/lib -ldao
```

Or with Octave instead of MATLAB:

```bash
mkoctfile --mex -I$DAOROOT/include -L$DAOROOT/lib -ldao daomex.c
```

(substitute `$DAOROOT` for wherever daoBase was installed - see [the
top-level README](../../README.md) - or point `-I`/`-L` at an in-tree `waf
build`'s `include`/`src` instead). This produces a platform-specific
`daomex.mex*` file (e.g. `daomex.mexa64` on Linux) alongside `daomex.c`.

Add this directory to MATLAB's path (`addpath`), and make sure `libdao.so`
is on your dynamic library path (`LD_LIBRARY_PATH` on Linux) before
starting MATLAB.

## Usage

```matlab
% Create a new SHM (or overwrite an existing one) sized/typed from the array.
w = daoShm('/tmp/test.im.shm', ones(4, 4, 'single'));

% Attach to an existing SHM from another process.
r = daoShm('/tmp/test.im.shm');

w.set_data(2 * ones(4, 4, 'single'));   % write the next frame
data = r.get_data();                    % read the latest frame

% Block until semaphore 1 is posted (a new frame), then read it.
data = r.get_data(true, 1);

w.close();
r.close();
```

`w.close()`/`r.close()` must be called explicitly once a `daoShm` object is
no longer needed - MATLAB does not call it automatically when the variable
goes out of scope, and skipping it leaks the SHM's semaphores and mapped
memory for the life of the MATLAB process.

## Example

```matlab
example_basic
```

`example_basic.m` creates a SHM, writes to it, reads it back through a
second handle, and closes both.

## Naming

`daomex.c` calls `libdao`'s primary API (`daoShmCreate`, `daoShmOpen`,
`daoShmSetData`, ...). The older C names (`daoShmImageCreate`,
`daoShmShm2Img`, `daoShmImage2Shm`, ...) still exist in `libdao` for other
languages' or projects' compatibility, but this file doesn't use them.
