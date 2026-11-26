'use strict';
// dao: Node.js binding of the daoBase SHM API.
//
//   const dao = require('dao');
//   const cam = dao.open('/tmp/cred2.im.shm');
//   const img = cam.getData();            // Uint16Array, cam.shape = [rows, cols]
//   if (await cam.wait(1000)) ...         // next frame, false on timeout
//   dao.open('/tmp/cred2Fps.im.shm').setData([400]);
//   const s = dao.create('/tmp/test.im.shm', [4, 4], 'float32');

const native = require('./build/Release/dao.node');

// dao atype (dao.h _DATATYPE_*) <-> name and TypedArray
const DTYPES = {
    uint8:      { atype: 1,  array: Uint8Array },
    int8:       { atype: 2,  array: Int8Array },
    uint16:     { atype: 3,  array: Uint16Array },
    int16:      { atype: 4,  array: Int16Array },
    uint32:     { atype: 5,  array: Uint32Array },
    int32:      { atype: 6,  array: Int32Array },
    uint64:     { atype: 7,  array: BigUint64Array },
    int64:      { atype: 8,  array: BigInt64Array },
    float32:    { atype: 9,  array: Float32Array },
    float64:    { atype: 10, array: Float64Array },
    complex64:  { atype: 11, array: Float32Array, complex: true },   // re, im interleaved
    complex128: { atype: 12, array: Float64Array, complex: true },
};
const BY_ATYPE = {};
for (const [name, d] of Object.entries(DTYPES)) BY_ATYPE[d.atype] = { name, ...d };

class Shm {
    constructor(handle) { this._h = handle; }

    /** SHM file name, as opened. */
    get name() { return this._h.name; }
    /** Sizes, same order as numpy's shape in Python (rows, cols for an image). */
    get shape() { return this._h.shape; }
    /** Element type name ('uint16', 'float32', ...). */
    get dtype() { return BY_ATYPE[this._h.atype].name; }
    /** dao.h atype code. */
    get atype() { return this._h.atype; }
    /** FIFO depth (1 for a plain SHM). */
    get depth() { return this._h.depth; }
    /** Frame counter (cnt0), a BigInt. */
    get counter() { return this._h.counter; }
    get closed() { return this._h.closed; }
    /** Number of elements in one frame. */
    get size() { return this.shape.reduce((a, b) => a * b, 1); }

    /** A copy of the newest frame, as a TypedArray (flat, row-major). */
    getData() { return this._h.getData(); }

    /** Writes a frame: a TypedArray of the SHM's type, or an array of numbers. */
    setData(data) {
        const d = BY_ATYPE[this._h.atype];
        if (!ArrayBuffer.isView(data) || data.constructor !== d.array) {
            data = d.array === BigUint64Array || d.array === BigInt64Array
                ? d.array.from(data, BigInt) : d.array.from(data);
        }
        this._h.setData(data);
    }

    /**
     * Waits for the next frame, on this handle's own semaphore (claimed at the first
     * wait), off the event loop. Resolves true when a frame was posted, false on
     * timeout. One wait at a time per Shm: open another Shm for a second reader.
     */
    wait(timeoutMs = 1000) {
        return new Promise((resolve, reject) => {
            this._h.wait(timeoutMs, (err, posted) => (err ? reject(err) : resolve(posted)));
        });
    }

    /** Waits for the next frame and returns it (null on timeout). */
    async next(timeoutMs = 1000) {
        return (await this.wait(timeoutMs)) ? this.getData() : null;
    }

    /** Async iterator over new frames: for await (const f of shm.frames()) ... */
    async *frames(timeoutMs = 1000) {
        while (!this.closed) {
            if (await this.wait(timeoutMs)) yield this.getData();
        }
    }

    /** Closes the SHM (deferred until a pending wait returns). */
    close() { this._h.close(); }
}

/** Opens an existing SHM. */
function open(name) {
    return new Shm(new native.Shm(name));
}

/**
 * Creates (or overwrites) a SHM.
 * @param name   file name, e.g. '/tmp/test.im.shm'
 * @param shape  1 to 3 sizes, e.g. [rows, cols]
 * @param dtype  'uint16', 'float32', ... (default 'float32')
 * @param opts   { depth: FIFO depth (default 1), data: initial frame }
 */
function create(name, shape, dtype = 'float32', opts = {}) {
    const d = DTYPES[dtype];
    if (!d) throw new Error(`dao: unknown dtype ${dtype} (${Object.keys(DTYPES).join(', ')})`);
    const shm = new Shm(new native.Shm(name, shape, d.atype, opts.depth || 1));
    if (opts.data !== undefined) shm.setData(opts.data);
    return shm;
}

module.exports = { open, create, Shm, DTYPES };
