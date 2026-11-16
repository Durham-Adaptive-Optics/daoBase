'use strict';
// node example_web.js <shm> [port]   then open http://localhost:8090
//
// Shows a SHM live in a browser. A web page cannot open a SHM, so this Node server
// reads it (dao binding) and streams the frames as Server-Sent Events: no npm
// dependency, Node's http module only. Frames are sent at most every 50 ms.
// It listens on localhost only: it is an example, with no authentication.
const http = require('node:http');
const dao = require('.');

const name = process.argv[2];
const port = Number(process.argv[3] || 8090);
if (!name) {
    console.error('usage: node example_web.js <shm> [port]');
    process.exit(1);
}
const MIN_PERIOD_MS = 50;

const PAGE = `<!doctype html><meta charset="utf-8"><title>dao SHM</title>
<style>body{font:14px sans-serif;margin:16px;background:#111;color:#ddd}
canvas{image-rendering:pixelated;width:min(90vw,640px);border:1px solid #444}</style>
<p id="info">connecting…</p><canvas id="c"></canvas>
<script>
const T = {uint8:Uint8Array,int8:Int8Array,uint16:Uint16Array,int16:Int16Array,
           uint32:Uint32Array,int32:Int32Array,float32:Float32Array,float64:Float64Array};
const c = document.getElementById('c'), ctx = c.getContext('2d'), info = document.getElementById('info');
new EventSource('/stream').onmessage = (ev) => {
  const m = JSON.parse(ev.data);
  const bytes = Uint8Array.from(atob(m.data), (ch) => ch.charCodeAt(0));
  if (!T[m.dtype]) { info.textContent = m.dtype + ': not displayed'; return; }
  const a = new T[m.dtype](bytes.buffer);
  const h = m.shape[0], w = m.shape.length > 1 ? m.shape[1] : 1;
  let lo = Infinity, hi = -Infinity;
  for (const v of a) { if (v < lo) lo = v; if (v > hi) hi = v; }
  const img = ctx.createImageData(w, h), k = 255 / ((hi - lo) || 1);
  for (let i = 0; i < w * h; i++) {
    const g = (a[i] - lo) * k;
    img.data.set([g, g, g, 255], 4 * i);
  }
  c.width = w; c.height = h; ctx.putImageData(img, 0, 0);
  info.textContent = m.name + '  ' + m.shape.join('x') + ' ' + m.dtype +
                     '  counter ' + m.counter + '  min ' + lo + ' max ' + hi;
};
</script>`;

const clients = new Set();

async function pump() {
    let shm = null, lastSent = 0;
    for (;;) {
        try {
            if (!shm) shm = dao.open(name);
            if (!(await shm.wait(1000))) continue;
            const now = Date.now();
            if (!clients.size || now - lastSent < MIN_PERIOD_MS) continue;
            lastSent = now;
            const f = shm.getData();
            const msg = JSON.stringify({
                name: shm.name, shape: shm.shape, dtype: shm.dtype,
                counter: shm.counter.toString(),
                data: Buffer.from(f.buffer, f.byteOffset, f.byteLength).toString('base64'),
            });
            for (const res of clients) res.write(`data: ${msg}\n\n`);
        } catch (e) {
            // not there yet, or recreated by its writer (e.g. a new ROI): reopen
            console.error(String(e.message || e));
            if (shm) shm.close();
            shm = null;
            await new Promise((r) => setTimeout(r, 1000));
        }
    }
}

http.createServer((req, res) => {
    if (req.url === '/stream') {
        res.writeHead(200, { 'Content-Type': 'text/event-stream', 'Cache-Control': 'no-cache' });
        clients.add(res);
        req.on('close', () => clients.delete(res));
    } else {
        res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
        res.end(PAGE);
    }
}).listen(port, '127.0.0.1', () => console.log(`http://localhost:${port}  (${name})`));

pump();
