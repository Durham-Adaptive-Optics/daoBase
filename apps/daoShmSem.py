#!/usr/bin/env python3
"""
daoShmSem.py -- which semaphores of an SHM readers wait on, and which processes.

    daoShmSem.py /tmp/dm1Cmd.im.shm [more SHMs]      # once
    daoShmSem.py -w /tmp/dm1Cmd.im.shm               # every second

Each reader waits on a semaphore of its own (dao picks it at its first wait).
PIDs are shown on Linux, for the processes this user may inspect.
"""
import argparse
import os
import time

import daoShm


def command(pid):
    try:
        with open('/proc/%d/cmdline' % pid, 'rb') as f:
            return f.read().replace(b'\0', b' ').decode(errors='replace').strip()
    except OSError:
        return ''


def show(name, shm):
    users = shm.sem_users()
    nsem = shm.image.md.contents.sem
    print('%s: %d semaphores, %d taken' % (name, nsem, len(users)))
    for s in range(nsem):
        if s not in users:
            print('  %2d  free' % s)
        elif not users[s]:
            print('  %2d  taken' % s)
        else:
            for pid in users[s]:
                print('  %2d  taken  %7d  %s' % (s, pid, command(pid)[:100]))


def main():
    parser = argparse.ArgumentParser(description='Which semaphores of an SHM readers wait on.')
    parser.add_argument('shm', nargs='+', help='SHM file(s)')
    parser.add_argument('-w', '--watch', action='store_true', help='refresh every second')
    args = parser.parse_args()
    shms = [(name, daoShm.shm(name)) for name in args.shm]   # opening does not take a semaphore
    while True:
        if args.watch:
            print('\033[H\033[J', end='')
        for name, shm in shms:
            show(name, shm)
        if not args.watch:
            break
        time.sleep(1)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        pass
