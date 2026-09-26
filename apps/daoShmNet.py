#!/usr/bin/env python3
"""
daoShmNet.py -- dao SHMs over the network: the local service, and what it sees.

    daoShmNet.py start [daoShmNetd options]   start this machine's service (in the background)
    daoShmNet.py stop                         stop it
    daoShmNet.py ls                           every SHM of the network, and where it is
    daoShmNet.py status                       this service: peers, replicas, streams
    daoShmNet.py want NAME [--keep]           replicate NAME here now (--keep: even unused)
    daoShmNet.py release NAME                 stop replicating NAME here
    daoShmNet.py ping                         is the service running?

Once each machine runs its service, a program opens an SHM of any machine by its
usual name ("/tmp/dm1Cmd.im.shm") -- nothing else to do. NAME may name the machine
("rtc1:dm1Cmd") when several have that SHM. The service's options (daoShmNetd --help):
--domain (only services of a domain see each other), --peer HOST (without multicast),
--allow PREFIX, --read-only, --idle SECONDS, ... See daoBase docs, "Network SHMs".
"""
import argparse
import sys

import daoNet


def _table(rows, headers):
    widths = [max(len(str(h)), *(len(str(r[i])) for r in rows)) if rows else len(h)
              for i, h in enumerate(headers)]
    print('  '.join(h.ljust(w) for h, w in zip(headers, widths)))
    for r in rows:
        print('  '.join(str(v).ljust(w) for v, w in zip(r, widths)))


def cmd_start(a, extra):
    daoNet.start(extra)
    info = daoNet.Client().ping()
    print(f"daoShmNetd running: node {info['node']}, domain {info['domain']}")


def cmd_stop(a, extra):
    daoNet.Client().stop()
    print('daoShmNetd stopped')


def cmd_ls(a, extra):
    rows = sorted((s['node'], s['name'], s['shape'], s['type'], s['state']) for s in daoNet.ls())
    if not rows:
        print('no SHM on the network (daoShmNet.py status: which machines are seen)')
        return
    _table(rows, ['MACHINE', 'SHM', 'SHAPE', 'TYPE', 'STATE'])


def cmd_status(a, extra):
    st = daoNet.status()
    print(f"this machine: {st.get('node')}  domain {st.get('domain')}  data port {st.get('port')}  "
          f"control 127.0.0.1:{st.get('control_port')}  SHMs in {st.get('dir')}")
    print()
    if st['peers']:
        _table([(p['node'], f"{p['address']}:{p['port']}", p['shms'], f"{p['seen_ms_ago']} ms", p['via'])
                for p in st['peers']], ['PEER', 'ADDRESS', 'SHMS', 'LAST SEEN', 'FOUND BY'])
    else:
        print('no other machine seen (same --domain? multicast allowed? else --peer HOST)')
    if st['replicas']:
        print()
        _table([(r['shm'], r['link'], r['frames'], r['dropped'], f"{r['rate_hz']:.1f}",
                 f"{r['age_us']} us", r['mode'], r['path']) for r in st['replicas']],
               ['REPLICA', 'LINK', 'FRAMES', 'SKIPPED', 'RATE HZ', 'AGE', 'MODE', 'PATH'])
    if st['sources']:
        print()
        _table([(s['shm'], s['readers'], s['counter']) for s in st['sources']],
               ['SERVED', 'READERS', 'COUNTER'])


def cmd_want(a, extra):
    print(daoNet.want(a.name, keep=a.keep, timeout=a.timeout))


def cmd_release(a, extra):
    print(daoNet.release(a.name))


def cmd_ping(a, extra):
    info = daoNet.Client().ping()
    print(f"daoShmNetd {info['version']} running: node {info['node']}, domain {info['domain']}")


def main(argv=None):
    ap = argparse.ArgumentParser(prog='daoShmNet.py', description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('start', help='start the service (then its options)')
    sub.add_parser('stop', help='stop the service')
    sub.add_parser('ls', help='every SHM of the network')
    sub.add_parser('status', help='peers, replicas, streams')
    w = sub.add_parser('want', help='replicate an SHM here now')
    w.add_argument('name')
    w.add_argument('--keep', action='store_true', help='keep it even when nobody uses it')
    w.add_argument('--timeout', type=float, default=5.0)
    r = sub.add_parser('release', help='stop replicating an SHM here')
    r.add_argument('name')
    sub.add_parser('ping', help='is the service running?')
    argv = sys.argv[1:] if argv is None else argv
    # 'start' passes everything after it to daoShmNetd
    extra = []
    if argv[:1] == ['start']:
        argv, extra = argv[:1], argv[1:]
    a = ap.parse_args(argv)
    try:
        {'start': cmd_start, 'stop': cmd_stop, 'ls': cmd_ls, 'status': cmd_status, 'want': cmd_want,
         'release': cmd_release, 'ping': cmd_ping}[a.cmd](a, extra)
    except daoNet.ServiceError as e:
        print(f'daoShmNet: {e}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
