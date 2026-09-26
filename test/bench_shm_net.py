#!/usr/bin/env python3
"""
Network SHMs: latency, throughput and CPU of daoShmNetd.

Two services on this machine (as two machines: A owns the SHMs, B replicates them,
over TCP on the loopback), for each frame size:

  latency     the writer paced so that each frame travels alone:
                service  from the source's publication to the replica's (the replica's
                         NET_AGE_US: copy, TCP, receive, publish)
                reader   to a Python reader woken by the replica's semaphore
  throughput  the writer paced, its rate doubled while fewer than 1 % of the frames
              are skipped: the highest such rate, and its MB/s (a writer flat out, with
              no gap between its writes, leaves no instant to copy a whole frame)
  CPU         of the two services while streaming at the paced rate, and idle (Linux)

On one machine this measures the software (copies, system calls, wake-ups), not a
network: across machines, add the network's own latency, and its bandwidth caps the
big frames (10 GbE: ~1.2 GB/s).

    PYTHONPATH=src/python LD_LIBRARY_PATH=build/src python test/bench_shm_net.py
    ... --sizes 1k 64k 1M 16M --samples 500 --json results.json
"""
import argparse
import json
import os
import random
import shutil
import subprocess
import sys
import threading
import time

import numpy as np

import daoNet
import daoShm

BASE = random.randint(30000, 40000)
A = {'node': 'benchA', 'dir': f'/tmp/daonetBenchA{os.getpid()}', 'port': BASE, 'control': BASE + 1}
B = {'node': 'benchB', 'dir': f'/tmp/daonetBenchB{os.getpid()}', 'port': BASE + 10, 'control': BASE + 11,
     'prefix': 'B_'}


def parse_size(text):
    units = {'k': 1024, 'K': 1024, 'm': 1024 ** 2, 'M': 1024 ** 2}
    return int(float(text[:-1]) * units[text[-1]]) if text[-1] in units else int(text)


def human(n):
    for unit, div in (('MB', 1024 ** 2), ('kB', 1024)):
        if n >= div:
            return f'{n / div:g} {unit}'
    return f'{n} B'


def start(svc, peer, extra=()):
    os.makedirs(svc['dir'], exist_ok=True)
    args = [daoNet.service_executable(), '--name', svc['node'], '--domain', f'bench{os.getpid()}',
            '--dir', svc['dir'], '--port', str(svc['port']), '--control-port', str(svc['control']),
            '--beacon-port', str(svc['port'] + 5), '--no-multicast', '--peer', f"127.0.0.1:{peer['port']}",
            '--idle', '0']
    if svc.get('prefix'):
        args += ['--replica-prefix', svc['prefix']]
    args += list(extra)
    log = open(os.path.join(svc['dir'], 'daoShmNetd.log'), 'a')
    proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
    t0 = time.monotonic()
    while not daoNet.running(svc['control']):
        if proc.poll() is not None or time.monotonic() - t0 > 5:
            sys.exit(f"daoShmNetd did not start (see {svc['dir']}/daoShmNetd.log)")
        time.sleep(0.05)
    return proc


def cpu_seconds(pid):
    """utime + stime of a process, s (Linux), or None."""
    try:
        with open(f'/proc/{pid}/stat') as f:
            fields = f.read().rsplit(')', 1)[1].split()
        return (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')
    except (OSError, IndexError, ValueError):
        return None


def replica_of(name, nbytes):
    src = daoShm.shm(f"{A['dir']}/{name}.im.shm", np.zeros(nbytes, np.uint8))
    t0 = time.monotonic()
    while not any(s['name'] == name for s in daoNet.ls()):
        if time.monotonic() - t0 > 6:
            sys.exit(f'{name} never listed on B')
        time.sleep(0.05)
    return src, daoShm.shm(f'benchA:{name}')


def latency(src, rep, nbytes, samples, rate):
    """(service latencies, reader latencies), us."""
    frame = np.zeros(nbytes, np.uint8)
    lat, path = [], []
    while rep.get_data(check=True, semNb=1, timeout=0.02, x=slice(0, 1)) is not None:
        pass                                               # earlier posts
    done = threading.Event()

    def reader():
        while not done.is_set():
            if rep.get_data(check=True, semNb=1, timeout=0.5, x=slice(0, 1)) is None:
                continue
            woke = time.time_ns()
            kw = daoNet.keywords(rep)
            if kw.get('NET_SRC_TIME'):
                lat.append((woke - kw['NET_SRC_TIME']) / 1000.0)
                path.append(float(kw['NET_AGE_US']))

    t = threading.Thread(target=reader, daemon=True)
    t.start()
    period = 1.0 / rate
    nxt = time.perf_counter()
    for k in range(samples):
        frame[0] = k & 0xFF
        src.set_data(frame)
        nxt += period
        pause = nxt - time.perf_counter()
        if pause > 0:
            time.sleep(pause)
    time.sleep(0.5)
    done.set()
    t.join(2)
    return np.array(path), np.array(lat)


def paced(src, rep, frame, rate, duration):
    """The writer at `rate` for `duration`: (frames/s written, frames/s delivered, skipped fraction)."""
    c_src, c_rep = src.get_counter(), rep.get_counter()
    period = 1.0 / rate
    t0 = time.perf_counter()
    nxt = t0
    while True:
        now = time.perf_counter()
        if now - t0 >= duration:
            break
        if now >= nxt:
            src.set_data(frame)
            nxt += period
        elif nxt - now > 0.002:
            time.sleep(nxt - now - 0.001)            # sleep, then spin to the instant
    elapsed = time.perf_counter() - t0
    time.sleep(0.3)
    written = src.get_counter() - c_src
    delivered = rep.get_counter() - c_rep
    return written / elapsed, delivered / elapsed, max(0.0, 1.0 - delivered / max(written, 1))


def max_rate(src, rep, nbytes, duration):
    """The highest paced rate (doubled from a start) with < 1 % frames skipped:
    (rate Hz, delivered Hz, skipped fraction), or None."""
    frame = np.zeros(nbytes, np.uint8)
    rate = 20.0 if nbytes >= 4 * 1024 ** 2 else 200.0
    best = None
    while rate <= 200000:
        written, delivered, skipped = paced(src, rep, frame, rate, duration)
        if written < 0.9 * rate:                     # the writer itself cannot go faster
            break
        if skipped > 0.01:
            break
        best = (rate, delivered, skipped)
        rate *= 2
    return best


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--sizes', nargs='+', default=['1k', '64k', '1M', '16M'])
    ap.add_argument('--samples', type=int, default=400, help='latency samples per size')
    ap.add_argument('--rate', type=float, default=200.0, help='paced rate for latency and CPU, Hz')
    ap.add_argument('--duration', type=float, default=0.5, help='each throughput step, s')
    ap.add_argument('--spin', action='store_true', help='the services in low-latency mode (daoShmNetd --spin)')
    ap.add_argument('--json', help='also write the results there')
    a = ap.parse_args()

    for d in (A['dir'], B['dir']):
        shutil.rmtree(d, ignore_errors=True)
    extra = ['--spin'] if a.spin else []
    pa, pb = start(A, B, extra), start(B, A, extra)
    os.environ['DAO_NET_CONTROL'] = str(B['control'])
    results = []
    try:
        time.sleep(1.0)
        c0 = [cpu_seconds(pa.pid), cpu_seconds(pb.pid)]
        time.sleep(5.0)
        c1 = [cpu_seconds(pa.pid), cpu_seconds(pb.pid)]
        idle = [100 * (y - x) / 5.0 if x is not None and y is not None else None for x, y in zip(c0, c1)]
        for text in a.sizes:
            nbytes = parse_size(text)
            src, rep = replica_of(f'bench{nbytes}', nbytes)
            rate = min(a.rate, 50.0) if nbytes >= 4 * 1024 ** 2 else a.rate
            samples = max(50, a.samples // 4) if nbytes >= 4 * 1024 ** 2 else a.samples
            ca = [cpu_seconds(pa.pid), cpu_seconds(pb.pid)]
            t0 = time.perf_counter()
            path, lat = latency(src, rep, nbytes, samples, rate)
            dt = time.perf_counter() - t0
            cb = [cpu_seconds(pa.pid), cpu_seconds(pb.pid)]
            cpu = [100 * (y - x) / dt if x is not None and y is not None else None for x, y in zip(ca, cb)]
            best = max_rate(src, rep, nbytes, a.duration)
            stats = lambda v: {'median': float(np.median(v)), 'p90': float(np.percentile(v, 90)),
                               'p99': float(np.percentile(v, 99)), 'max': float(v.max())} if v.size else None
            r = {'size_bytes': nbytes, 'rate_hz': rate, 'samples': int(lat.size),
                 'service_latency_us': stats(path), 'reader_latency_us': stats(lat),
                 'max_rate_hz': best[0] if best else None, 'delivered_hz': best[1] if best else None,
                 'delivered_MBps': best[1] * nbytes / 1024 ** 2 if best else None,
                 'cpu_percent_owner': cpu[0], 'cpu_percent_reader': cpu[1]}
            results.append(r)
            del rep, src
    finally:
        for p in (pa, pb):
            p.terminate()
            try:
                p.wait(3)
            except subprocess.TimeoutExpired:
                p.kill()
        for d in (A['dir'], B['dir']):
            shutil.rmtree(d, ignore_errors=True)

    pct = lambda v: '-' if v is None else f'{v:.1f} %'
    print(f'daoShmNetd{" --spin" if a.spin else ""}, two services on one machine '
          f'({os.uname().nodename if hasattr(os, "uname") else ""})')
    print(f'idle CPU: owner {pct(idle[0])}, reader {pct(idle[1])}')
    print()
    head = ['frame', 'service median / p99', 'reader median / p99', 'CPU owner / reader',
            'max rate, no skip', 'MB/s']
    nan = {'median': float('nan'), 'p99': float('nan')}
    rows = []
    for r in results:
        S, L = r['service_latency_us'] or nan, r['reader_latency_us'] or nan
        rows.append([human(r['size_bytes']), f"{S['median']:.0f} / {S['p99']:.0f} us",
                     f"{L['median']:.0f} / {L['p99']:.0f} us",
                     f"{pct(r['cpu_percent_owner'])} / {pct(r['cpu_percent_reader'])} @{r['rate_hz']:g} Hz",
                     f"{r['max_rate_hz']:g} Hz" if r['max_rate_hz'] else '-',
                     f"{r['delivered_MBps']:.0f}" if r['delivered_MBps'] else '-'])
    widths = [max(len(h), *(len(row[i]) for row in rows)) for i, h in enumerate(head)]
    print('  '.join(h.ljust(w) for h, w in zip(head, widths)))
    for row in rows:
        print('  '.join(v.ljust(w) for v, w in zip(row, widths)))
    if a.json:
        with open(a.json, 'w') as f:
            json.dump({'idle_cpu_percent': idle, 'results': results}, f, indent=1)
        print(f'\n-> {a.json}')


if __name__ == '__main__':
    main()
