#!/usr/bin/env python3
"""
On any other machine: the writer's image, opened by its name as if it were local.

    daoShmNet.py start          # once, on this machine too
    python reader.py [--name netDemo] [--send 1.5 -0.5 0.25 0]

Prints, once a second: the frames read (each woken by the SHM's semaphore), the spot's
position, the frames' age (write there -> ready here; needs the machines' clocks in
sync, NTP or PTP), and the link. --send writes a command back to the writer's machine.
"""
import argparse
import time

import numpy as np

import daoNet
import daoShm


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--name', default='netDemo')
    ap.add_argument('--send', type=float, nargs=4, metavar='V', help='write this command to the writer')
    a = ap.parse_args()

    image = daoShm.shm(f'/tmp/{a.name}.im.shm')        # the other machine's: the same line as a local SHM
    kw = daoNet.keywords(image)
    print(f"/tmp/{a.name}.im.shm: from {kw.get('NET_REPLICA', 'this machine')}. Ctrl+C stops.")
    if a.send:
        command = daoShm.shm(f'/tmp/{a.name}Cmd.im.shm')
        command.set_data(np.array(a.send, np.float32))
        print(f'command sent: {a.send}')
    frames, ages, t0 = 0, [], time.monotonic()
    try:
        while True:
            img = image.get_data(check=True, semNb=1, timeout=2.0)   # wakes on each new frame
            if img is None:
                print('no frame for 2 s (writer stopped? link:',
                      'up)' if daoNet.keywords(image).get('NET_LINK', 1) else 'DOWN)')
                continue
            frames += 1
            kw = daoNet.keywords(image)
            ages.append(kw.get('NET_AGE_US', 0))
            if time.monotonic() - t0 >= 1.0:
                y, x = np.unravel_index(np.argmax(img), img.shape)
                print(f"{frames:5d} frames/s   spot ({x:3d}, {y:3d})   age median {np.median(ages):6.0f} us   "
                      f"skipped {kw.get('NET_DROPPED', 0)}   link {'up' if kw.get('NET_LINK', 1) else 'DOWN'}")
                frames, ages, t0 = 0, [], time.monotonic()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
