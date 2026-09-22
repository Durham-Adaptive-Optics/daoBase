# Julia bindings

`dao.jl` wraps `libdao`'s SHM API (see
[`../../include/dao.h`](../../include/dao.h)) via `ccall`, giving Julia the
same create / open / read / write / close interface as the Python, MATLAB
and Rust bindings in this repo.

## Setup

`waf install` (see [the top-level README](../../README.md)) copies every
`.jl` file in this directory to `$DAOROOT/julia`. `libdao.so` needs to be
on your library path (`dao_env.sh` sets `LD_LIBRARY_PATH` for this).

To run against an in-tree `waf build` instead (no install), point
`LD_LIBRARY_PATH` at `build/src`.

## Usage

```julia
include(joinpath(ENV["DAOROOT"], "julia", "dao.jl"))

# Create a new SHM (or overwrite an existing one) sized/typed from the array.
w = dao.shm("/tmp/test.im.shm", ones(Float32, 4, 4))

# Attach to an existing SHM from another process.
r = dao.shm("/tmp/test.im.shm")

dao.set_data(w, fill(2.0f0, 4, 4))   # write the next frame
data = dao.get_data(r)               # read the latest frame

# Block until a new frame is posted (semaphore 0 by default), then read it.
data = dao.get_data(r, check=true)

dao.close(w)
dao.close(r)
```

`dao.get_naxis`, `dao.get_size`, `dao.get_counter` and `dao.get_metadata`
read the SHM's metadata. None of the high-level functions (`shm`,
`get_data`, `set_data`, `close`, ...) are exported from the module, so call
them as `dao.foo(...)`, as above.

## Example

```bash
julia example_basic.jl
```

`example_basic.jl` creates a SHM, writes to it, reads it back through a
second handle, and closes both. [`../../test/test_dao_julia.jl`](../../test/test_dao_julia.jl)
is a more thorough walkthrough, including waiting on a semaphore and on the
update counter.

## Known limitations

The `IMAGE`/`IMAGE_METADATA` struct definitions in `dao.jl` are hand-written
to match [`dao.h`](../../include/dao.h)'s Linux/macOS field layout; the
Windows layout (wider handle types, an extra `shmfm` field) isn't modeled.
If `dao.h` ever changes those structs, this file needs updating by hand -
unlike the Rust bindings, which generate the struct layout directly from
the header at build time.

## Naming

`dao.jl`'s low-level `ccall` wrappers call `libdao`'s primary API
(`daoShmCreate`, `daoShmOpen`, `daoShmSetData`, `daoShmGetData`, ...). The
older C names (`daoShmImageCreate`, `daoShmShm2Img`, `daoShmImage2Shm`, ...)
still exist in `libdao` for other languages' or projects' compatibility,
but this module doesn't use them.
