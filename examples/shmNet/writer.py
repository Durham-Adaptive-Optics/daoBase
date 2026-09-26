#!/usr/bin/env python3
"""
On the machine that owns the SHM: a 128x128 image, a moving spot written at --rate Hz,
and a 4-value command SHM (written from any machine, printed here when it changes).

    daoShmNet.py start          # once, on this machine
    python writer.py [--rate 100] [--name netDemo]

Then, on any other machine running its service: python reader.py
"""
import argparse
import time

import numpy as np

import daoShm


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--rate', type=float, default=100.0, help='frames per second')
    ap.add_argument('--name', default='netDemo', help='the SHM: /tmp/<name>.im.shm, and <name>Cmd')
    a = ap.parse_args()
    image = daoShm.shm(f'/tmp/{a.name}.im.shm', np.zeros((128, 128), np.float32))
    command = daoShm.shm(f'/tmp/{a.name}Cmd.im.shm', np.zeros(4, np.float32))
    print(f'writing /tmp/{a.name}.im.shm at {a.rate:g} Hz; /tmp/{a.name}Cmd.im.shm waits for commands. Ctrl+C stops.')
    yy, xx = np.mgrid[0:128, 0:128]
    k, last_cmd = 0, command.get_counter()
    period, nxt = 1.0 / a.rate, time.perf_counter()
    try:
        while True:
            x, y = 64 + 40 * np.cos(k / 50), 64 + 40 * np.sin(k / 50)
            image.set_data(np.exp(-((xx - x) ** 2 + (yy - y) ** 2) / 30).astype(np.float32))
            k += 1
            if command.get_counter() != last_cmd:          # written from another machine
                last_cmd = command.get_counter()
                print(f'command received: {command.get_data()}')
            nxt += period
            time.sleep(max(0.0, nxt - time.perf_counter()))
    except KeyboardInterrupt:
        print(f'\n{k} frames written')


if __name__ == '__main__':
    main()
