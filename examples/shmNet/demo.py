#!/usr/bin/env python3
"""
Network SHMs in one minute, on one machine.

Starts two daoShmNetd services, as two machines ("camera" and "display", each with
its own SHM directory and ports), then:

  1. on "camera": creates a 128x128 image SHM and writes a moving spot at 100 Hz;
  2. on "display": lists the network's SHMs, and opens the camera's image by its
     name -- a plain daoShm.shm(...), as if it were local -- then reads 2 s of
     frames, waking on its semaphore, with their age and the link's state;
  3. writes a command back from "display" to a "camera" SHM, and shows it arrives;
  4. stops everything and removes what it made.

    python demo.py            (daoBase installed; or from the repository:
                               PYTHONPATH=src/python LD_LIBRARY_PATH=build/src)

On two real machines there is nothing of this set-up: run `daoShmNet.py start` on
each, and open the SHM by its name (see README.md: writer.py and reader.py).
"""
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

PORT = random.randint(40000, 50000)
CAMERA = {'node': 'camera', 'dir': '/tmp/daonetDemoCamera', 'port': PORT, 'control': PORT + 1}
DISPLAY = {'node': 'display', 'dir': '/tmp/daonetDemoDisplay', 'port': PORT + 10, 'control': PORT + 11,
           'prefix': 'display_'}      # one machine: a replica may not share its source's name
DOMAIN = f'demo{os.getpid()}'


def start(me, peer):
    os.makedirs(me['dir'], exist_ok=True)
    args = ['--name', me['node'], '--domain', DOMAIN, '--dir', me['dir'], '--port', str(me['port']),
            '--control-port', str(me['control']), '--beacon-port', str(me['port'] + 5),
            '--no-multicast', '--peer', f"127.0.0.1:{peer['port']}"]
    if me.get('prefix'):
        args += ['--replica-prefix', me['prefix']]
    os.environ['DAO_NET_CONTROL'] = str(me['control'])
    return daoNet.start(args, log=os.path.join(me['dir'], 'daoShmNetd.log'))


def main():
    sys.stdout.reconfigure(line_buffering=True)          # each line as it comes
    for d in (CAMERA['dir'], DISPLAY['dir']):
        shutil.rmtree(d, ignore_errors=True)
    print('starting two services: "camera" and "display"')
    services = [start(CAMERA, DISPLAY), start(DISPLAY, CAMERA)]
    stop = threading.Event()
    try:
        # --- on "camera": an image, written at 100 Hz ---------------------------------
        image = daoShm.shm(f"{CAMERA['dir']}/wfsImage.im.shm", np.zeros((128, 128), np.float32))
        command = daoShm.shm(f"{CAMERA['dir']}/wfsCommand.im.shm", np.zeros(4, np.float32))
        yy, xx = np.mgrid[0:128, 0:128]

        def camera():
            k = 0
            while not stop.is_set():
                x, y = 64 + 40 * np.cos(k / 50), 64 + 40 * np.sin(k / 50)
                image.set_data(np.exp(-((xx - x) ** 2 + (yy - y) ** 2) / 30).astype(np.float32))
                k += 1
                time.sleep(0.01)

        threading.Thread(target=camera, daemon=True).start()

        # --- on "display": everything below is what a program on another machine does -
        os.environ['DAO_NET_CONTROL'] = str(DISPLAY['control'])
        time.sleep(2.5)                                     # the lists: exchanged every second
        print('\nthe network, seen from "display" (daoShmNet.py ls):')
        for s in daoNet.ls():
            print(f"   {s['node']:8s} {s['name']:12s} {s['shape']:8s} {s['type']:8s} {s['state']}")

        t0 = time.monotonic()
        wfs = daoShm.shm('camera:wfsImage')     # on two machines: daoShm.shm('/tmp/wfsImage.im.shm')
        print(f"\nopened camera's wfsImage on display in {1e3 * (time.monotonic() - t0):.0f} ms: "
              f"{wfs.image.name.decode()}", flush=True)
        frames, ages, t0 = 0, [], time.monotonic()
        while time.monotonic() - t0 < 2.0:
            img = wfs.get_data(check=True, semNb=1, timeout=1.0)   # wakes on each new frame
            if img is None:
                continue
            frames += 1
            ages.append(daoNet.keywords(wfs)['NET_AGE_US'])
            if frames % 50 == 0:
                y, x = np.unravel_index(np.argmax(img), img.shape)
                kw = daoNet.keywords(wfs)
                print(f"   frame {frames:3d}: spot at ({x:3d}, {y:3d})   age {kw['NET_AGE_US']:5d} us   "
                      f"link {'up' if kw['NET_LINK'] else 'DOWN'}   from {kw['NET_REPLICA']}")
        print(f"read {frames} frames in 2 s, each woken by its semaphore; "
              f"age median {np.median(ages):.0f} us (write on camera -> ready on display)")

        # --- a write from "display" reaches "camera" ------------------------------------
        cmd = daoShm.shm('camera:wfsCommand')
        cmd.set_data(np.array([1.5, -0.5, 0.25, 0.0], np.float32))
        time.sleep(0.2)
        print(f"\ndisplay wrote wfsCommand = [1.5, -0.5, 0.25, 0]; camera now has {command.get_data()}")

        print('\nthe display service (daoShmNet.py status):')
        for r in daoNet.status()['replicas']:
            print(f"   {r['shm']:20s} link {r['link']:4s} frames {r['frames']:5d}  skipped {r['dropped']:3d}  "
                  f"{r['rate_hz']:6.1f} Hz")
    finally:
        stop.set()
        time.sleep(0.1)
        for proc in services:
            proc.terminate()
            try:
                proc.wait(3)
            except subprocess.TimeoutExpired:
                proc.kill()
        for d in (CAMERA['dir'], DISPLAY['dir']):
            shutil.rmtree(d, ignore_errors=True)
    print('\ndone: services stopped, files removed')


if __name__ == '__main__':
    sys.exit(main())
