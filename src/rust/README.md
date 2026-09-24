# Rust bindings

This crate wraps `libdao`'s SHM API (see
[`../../include/dao.h`](../../include/dao.h)), giving Rust the same
create / open / read / write / close interface as the Python, Julia and
MATLAB bindings in this repo.

Unlike those, this one doesn't hand-transcribe the C structs: `build.rs`
runs [`bindgen`](https://docs.rs/bindgen) over `dao.h` (via `wrapper.h`) at
build time and generates the raw FFI layer (`dao::sys`) from it directly,
so it can't drift out of sync with the real struct layout. [`Shm`], in
`src/lib.rs`, is a small safe wrapper over that raw layer.

## Setup

You need `libclang` installed for `bindgen` to parse `dao.h` (e.g.
`apt install libclang-dev` on Debian/Ubuntu, `brew install llvm` on macOS).

`build.rs` links against `libdao` by searching, in order:
- `$DAOROOT/lib` and `$DAOROOT/lib64` (an installed daoBase - see
  [the top-level README](../../README.md))
- `../../build/src` (an in-tree `waf build`, no install needed)

```bash
cargo build
```

## Usage

```rust
use dao::Shm;

// Create a new SHM (or overwrite an existing one).
let mut w = Shm::create::<f32>("/tmp/test.im.shm", &[4, 4])?;

// Attach to an existing SHM from another process.
let mut r = Shm::open("/tmp/test.im.shm")?;

w.set_data(&[1.0f32; 16])?;       // write the next frame
let data: &[f32] = r.get_data()?; // read the latest frame

// Block until semaphore 0 is posted (a new frame), then read it.
r.wait_sem(0)?;
let data: &[f32] = r.get_data()?;
```

The SHM is closed automatically when a `Shm` is dropped. `get_data`'s
element type isn't checked against the SHM's own `atype` - as with the raw
C API, it's the caller's job to read back the same type that was passed to
`Shm::create`.

The raw, bindgen-generated bindings (`dao::sys`, including `sys::IMAGE`,
`sys::IMAGE_METADATA`, and every `daoShm*`/`daoSem*` C function) are also
public, for anything `Shm` doesn't cover.

## Example

```bash
cargo run
```

`src/main.rs` creates a SHM, writes to it, reads it back through a second
handle, and lets both close on drop.

## Naming

`Shm` calls `libdao`'s primary API (`daoShmCreate`, `daoShmOpen`,
`daoShmSetData`, `daoShmGetData`, ...). The older C names
(`daoShmImageCreate`, `daoShmShm2Img`, `daoShmImage2Shm`, ...) still exist
in `libdao` for other languages' or projects' compatibility - and are
still reachable via `dao::sys` - but `Shm` doesn't use them.
