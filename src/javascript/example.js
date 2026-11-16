'use strict';
// node example.js [shm]      (default /tmp/daojs_example.im.shm)
// Creates a SHM, writes 10 frames into it from a timer, and reads them back with
// wait(): the same pattern works across processes and with Python / C writers.
const dao = require('.');

const name = process.argv[2] || '/tmp/daojs_example.im.shm';

async function main() {
    const writer = dao.create(name, [4, 4], 'float32');
    const reader = dao.open(name);
    console.log(`${reader.name}: shape ${reader.shape}, ${reader.dtype}, counter ${reader.counter}`);

    let k = 0;
    const timer = setInterval(() => writer.setData(new Float32Array(16).fill(++k)), 50);

    for (let n = 0; n < 10; n++) {
        const frame = await reader.next(1000);   // null on timeout
        console.log(`counter ${reader.counter}: frame[0] = ${frame && frame[0]}`);
    }
    clearInterval(timer);
    reader.close();
    writer.close();
}

main().catch((e) => { console.error(e); process.exit(1); });
