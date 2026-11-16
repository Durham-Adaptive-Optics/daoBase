# JavaScript (Node.js) bindings

`dao` gives Node.js the same SHM interface as the Python, Julia, Rust and MATLAB
bindings: open, create, read, write, wait for the next frame, close. It is a
Node-API addon written in C ([`daoShm.c`](daoShm.c)) over `libdao`. It
includes [`../../include/dao.h`](../../include/dao.h), so the `IMAGE` layout always
matches the library. It uses only `node_api.h`: no npm dependency.

A web page cannot open a SHM. In a browser, the frames go through a server:
[`example_web.js`](example_web.js) is a small one built on this binding, and
daoTools' `daoShmWebViewer.py` is the full viewer.

## Build

You need Node.js ≥ 16 with its headers, `node-gyp`, a C compiler and daoBase
(`apt install nodejs node-gyp` on Debian/Ubuntu).

```bash
cd src/javascript
node-gyp rebuild          # or: npm run build
npm test                  # node --test test/
```

`binding.gyp` links `libdao` from `$DAOROOT/lib` (an installed daoBase) or from
`../../build/src` (an in-tree `waf build`), and sets both as rpath. waf does not
build or install the addon, the same as the Rust crate.

With the Debian `node-gyp` and a conda Python first in `PATH`, gyp fails with
`No package metadata was found for gyp`. Use the system Python:
`PYTHON=/usr/bin/python3 node-gyp rebuild`.

To use it from another project: `npm install /path/to/daoBase/src/javascript`, or
`require('/path/to/daoBase/src/javascript')`.

## Usage

```js
const dao = require('dao');

// open an existing SHM (from C, Python, another Node process...)
const cam = dao.open('/tmp/cred2.im.shm');
cam.shape;      // [512, 640]: same order as numpy's shape in Python
cam.dtype;      // 'uint16'
cam.counter;    // 1234n (BigInt)
const img = cam.getData();          // Uint16Array: a copy of the newest frame, row-major

// wait for frames, off the event loop
if (await cam.wait(1000)) { /* a new frame */ }      // false: timed out
const f = await cam.next(1000);                       // the frame, or null
for await (const frame of cam.frames()) { /* ... */ }

// write
dao.open('/tmp/cred2Fps.im.shm').setData([400]);      // array, or Float32Array
const s = dao.create('/tmp/test.im.shm', [4, 4], 'float32', { data: new Float32Array(16) });
s.setData(new Float32Array(16).fill(1));              // posts the semaphores, counter + 1

cam.close();    // also done when the object is garbage collected
```

| dtype | dao atype | TypedArray |
|-------|-----------|------------|
| `uint8`, `int8` | 1, 2 | `Uint8Array`, `Int8Array` |
| `uint16`, `int16` | 3, 4 | `Uint16Array`, `Int16Array` |
| `uint32`, `int32` | 5, 6 | `Uint32Array`, `Int32Array` |
| `uint64`, `int64` | 7, 8 | `BigUint64Array`, `BigInt64Array` |
| `float32`, `float64` | 9, 10 | `Float32Array`, `Float64Array` |
| `complex64`, `complex128` | 11, 12 | `Float32Array`, `Float64Array`, re/im interleaved |

`create(name, shape, dtype, { depth })` makes a FIFO SHM when `depth > 1`;
`getData()` returns its newest segment. TypeScript types are in
[`index.d.ts`](index.d.ts).

## Notes

- **Waits.** `wait()` runs `daoShmWaitSemTimeout` on this handle's own semaphore
  (`DAO_SEM_AUTO`, claimed at the first wait) in libuv's thread pool, so the event loop
  keeps running. A timeout is required: a wait with none would hold a pool thread
  forever when the writer stops (libuv has 4 by default). One wait at a time per
  handle. Another reader in the same process opens its own `Shm`.
- **`close()`** during a wait takes effect when the wait returns (at most its timeout).
  Closing gives the semaphore back.
- **Copies.** `getData()` copies the frame. When its writer recreates a SHM (for
  example a camera changing its ROI), a view would point at the old file. Reopen the
  SHM when its shape should change (see `example_web.js`).
- **1-D SHMs.** `libdao`'s `daoShmOpen` exits the process when an axis has size 0, so
  `create(name, [n])` makes an `[n, 1]` SHM, as DAO SHMs always are, and `open()`
  refuses a true 1-D SHM with an error instead of letting libdao exit.
- Tested on Linux (Node 18). macOS should build the same way but is untested. Windows
  is not done: `binding.gyp` has no Windows link line.

## Examples

```bash
node example.js                          # create a SHM, write and read back 10 frames
node example_web.js /tmp/cred2.im.shm    # live view at http://localhost:8090 (localhost only)
```
