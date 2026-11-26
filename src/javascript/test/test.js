'use strict';
// node --test test/     (SHMs are created in a fresh directory under os.tmpdir())
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawn } = require('node:child_process');
const dao = require('..');

const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'daojs-'));
const shmPath = (n) => path.join(dir, `${n}.im.shm`);
test.after(() => fs.rmSync(dir, { recursive: true, force: true }));

test('create, open, read back', () => {
    const w = dao.create(shmPath('rw'), [3, 4], 'float32');
    assert.deepStrictEqual(w.shape, [3, 4]);
    assert.strictEqual(w.dtype, 'float32');
    assert.strictEqual(w.size, 12);
    const r = dao.open(shmPath('rw'));
    const c0 = r.counter;
    const frame = Float32Array.from({ length: 12 }, (_, i) => i * 0.5);
    w.setData(frame);
    assert.strictEqual(r.counter, c0 + 1n);
    assert.deepStrictEqual(r.getData(), frame);
    w.setData(Array.from({ length: 12 }, () => 7));   // plain array, converted
    assert.ok(r.getData().every((v) => v === 7));
    r.close(); w.close();
    assert.ok(r.closed);
    assert.throws(() => r.getData(), /closed/);
});

test('every dtype round-trips', () => {
    for (const [name, d] of Object.entries(dao.DTYPES)) {
        const n = 6, len = d.complex ? 2 * n : n;
        const big = d.array === BigUint64Array || d.array === BigInt64Array;
        const data = d.array.from({ length: len }, (_, i) => (big ? BigInt(i + 1) : i + 1));
        const w = dao.create(shmPath(`t_${name}`), [2, 3], name, { data });
        const r = dao.open(shmPath(`t_${name}`));
        assert.strictEqual(r.dtype, name);
        const got = r.getData();
        assert.strictEqual(got.constructor, d.array, name);
        assert.deepStrictEqual(got, data, name);
        r.close(); w.close();
    }
});

test('setData checks type and length', () => {
    const w = dao.create(shmPath('chk'), [4], 'uint16');
    assert.throws(() => w._h.setData(new Float32Array(4)), /wrong TypedArray type/);
    assert.throws(() => w.setData(new Uint16Array(3)), /3 values given/);
    assert.throws(() => w._h.setData([1, 2, 3, 4]), /TypedArray/);
    w.close();
});

test('bad arguments', () => {
    assert.throws(() => dao.open(shmPath('does_not_exist')), /cannot open/);
    assert.throws(() => dao.create(shmPath('x'), [2], 'float16'), /unknown dtype/);
    assert.throws(() => dao.create(shmPath('x'), [1, 2, 3, 4]), /1 to 3 sizes/);
    assert.throws(() => dao.create(shmPath('x'), [0, 2]), /positive/);
});

test('1-D: create [n] makes a 1-D SHM, which open() reads back', () => {
    const v = dao.create(shmPath('vec'), [5], 'float32', { data: [1, 2, 3, 4, 5] });
    assert.deepStrictEqual(v.shape, [5]);
    const r = dao.open(shmPath('vec'));
    assert.deepStrictEqual(r.shape, [5]);
    assert.deepStrictEqual(Array.from(r.getData()), [1, 2, 3, 4, 5]);
    r.close();
    v.close();
});

test('wait: a frame wakes it, without blocking the event loop', async () => {
    const w = dao.create(shmPath('wait'), [8], 'int32');
    const r = dao.open(shmPath('wait'));
    let ticks = 0;
    const timer = setInterval(() => ticks++, 5);
    setTimeout(() => w.setData(Int32Array.from([1, 2, 3, 4, 5, 6, 7, 8])), 150);
    const t0 = Date.now();
    const frame = await r.next(2000);
    clearInterval(timer);
    assert.deepStrictEqual(frame, Int32Array.from([1, 2, 3, 4, 5, 6, 7, 8]));
    assert.ok(Date.now() - t0 >= 100, 'woke on the write, not before');
    assert.ok(ticks >= 10, `event loop kept running (${ticks} ticks)`);
    r.close(); w.close();
});

test('wait: timeout resolves false; one wait per handle', async () => {
    const r = dao.create(shmPath('tmo'), [2], 'float64');
    const p = r.wait(200);
    assert.throws(() => r._h.wait(100, () => {}), /already pending/);
    assert.strictEqual(await p, false);
    assert.throws(() => r._h.wait(0, () => {}), /positive timeout/);
    r.close();
});

test('close during a wait is deferred until it returns', async () => {
    const r = dao.create(shmPath('cls'), [2], 'float32');
    const p = r.wait(300);
    r.close();
    assert.ok(r.closed);
    assert.strictEqual(await p, false);
    assert.throws(() => r.getData(), /closed/);
});

test('FIFO: getData returns the newest segment', () => {
    const w = dao.create(shmPath('fifo'), [4], 'float32', { depth: 3 });
    assert.strictEqual(w.depth, 3);
    const r = dao.open(shmPath('fifo'));
    for (let k = 1; k <= 5; k++) w.setData([k, k, k, k]);
    assert.deepStrictEqual(Array.from(r.getData()), [5, 5, 5, 5]);
    r.close(); w.close();
});

test('frames from another process', async () => {
    const name = shmPath('xproc');
    const w = dao.create(name, [16, 16], 'uint16');
    const r = dao.open(name);
    const child = spawn(process.execPath, ['-e', `
        const dao = require(${JSON.stringify(path.join(__dirname, '..'))});
        const w = dao.open(${JSON.stringify(name)});
        let k = 0;
        const t = setInterval(() => {
            w.setData(new Uint16Array(256).fill(++k));
            if (k === 20) { clearInterval(t); w.close(); }
        }, 10);`]);
    const seen = [];
    for await (const f of r.frames(2000)) {
        seen.push(f[0]);
        if (f[0] === 20) break;
    }
    await new Promise((res) => child.on('exit', res));
    assert.strictEqual(seen[seen.length - 1], 20);
    assert.ok(seen.length >= 10, `received ${seen.length} frames`);
    r.close(); w.close();
});
