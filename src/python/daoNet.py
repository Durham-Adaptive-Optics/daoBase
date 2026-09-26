"""
daoNet -- dao SHMs over the network, from Python.

Every machine runs a daoShmNetd (``daoShmNet.py start``). An SHM of another machine
is then opened as a local one -- the service keeps a replica with the same name
up to date::

    import daoShm
    s = daoShm.shm('/tmp/dm1Cmd.im.shm')      # on another machine: works the same
    s.get_data(check=True)                     # waits for its next frame

This module talks to the local service (its control port, 127.0.0.1 only)::

    import daoNet
    daoNet.ls()                        # every SHM of the network: node, name, shape, type, state
    daoNet.status()                    # this service: peers, replicas, streams
    s = daoNet.open('rtc1:dm1Cmd')     # the SHM of machine rtc1 (a daoShm.shm)
    daoNet.want('dm1Cmd', keep=True)   # keep it replicated, used or not
    daoNet.release('dm1Cmd')           # stop replicating it

See daoBase docs, "Network SHMs".
"""
import builtins
import os
import shutil
import socket
import subprocess
import sys
import time

DATA_PORT = 7710
CONTROL_PORT = 7709
BEACON_PORT = 7711


class ServiceError(RuntimeError):
    """The local daoShmNetd refused a request, or does not run."""


def control_port():
    """The local service's control port: $DAO_NET_CONTROL, or 7709."""
    try:
        return int(os.environ.get('DAO_NET_CONTROL', CONTROL_PORT))
    except ValueError:
        return CONTROL_PORT


class Client:
    """A connection to the local daoShmNetd, one request each."""

    def __init__(self, port=None, timeout=10.0):
        self.port = port or control_port()
        self.timeout = timeout

    def _lines(self, request, timeout=None):
        """Send one request; the reply lines (up to 'END', or the single line)."""
        try:
            with socket.create_connection(('127.0.0.1', self.port), timeout=1.0) as s:
                s.settimeout(timeout or self.timeout)
                s.sendall((request + '\n').encode('utf-8'))
                data = b''
                while True:
                    chunk = s.recv(65536)
                    if not chunk:
                        break
                    data += chunk
        except (ConnectionRefusedError, socket.timeout, OSError) as e:
            raise ServiceError(f'no daoShmNetd on 127.0.0.1:{self.port} ({e}): daoShmNet.py start') from None
        lines = data.decode('utf-8', 'replace').splitlines()
        return [ln for ln in lines if ln != 'END']

    def _one(self, request, timeout=None):
        lines = self._lines(request, timeout)
        if not lines:
            raise ServiceError(f'no answer to {request.split()[0]}')
        if lines[0].startswith('ERR'):
            raise ServiceError(lines[0][4:])
        return lines[0][3:] if lines[0].startswith('OK') else lines[0]

    def ping(self):
        """{'node', 'domain', 'version'} of the local service (ServiceError if none)."""
        node, domain, version = self._one('PING', 2.0).split()[:3]
        return {'node': node, 'domain': domain, 'version': version}

    def ls(self):
        """Every SHM of the network: [{'node', 'name', 'shape', 'type', 'state'}], state
        'local' (this machine), 'remote', 'replica' (replicated here), ',gpu' added."""
        out = []
        for ln in self._lines('LIST'):
            f = ln.split('\t')
            if len(f) == 6 and f[0] == 'SHM':
                out.append({'node': f[1], 'name': f[2], 'shape': f[3], 'type': f[4], 'state': f[5]})
        return out

    def status(self):
        """This service: {'node', 'domain', 'port', 'control_port', 'dir', 'peers', 'replicas', 'sources'}."""
        st = {'peers': [], 'replicas': [], 'sources': []}
        for ln in self._lines('STATUS'):
            f = ln.split('\t')
            if f[0] == 'NODE' and len(f) >= 6:
                st.update(node=f[1], domain=f[2], port=int(f[3]), control_port=int(f[4]), dir=f[5])
            elif f[0] == 'PEER' and len(f) >= 7:
                st['peers'].append({'node': f[1], 'address': f[2], 'port': int(f[3]), 'shms': int(f[4]),
                                    'seen_ms_ago': int(f[5]), 'via': f[6]})
            elif f[0] == 'REPLICA' and len(f) >= 9:
                st['replicas'].append({'shm': f[1], 'path': f[2], 'link': f[3], 'frames': int(f[4]),
                                       'dropped': int(f[5]), 'rate_hz': float(f[6]), 'age_us': int(f[7]),
                                       'mode': f[8]})
            elif f[0] == 'SOURCE' and len(f) >= 4:
                st['sources'].append({'shm': f[1], 'readers': int(f[2]), 'counter': int(f[3])})
        return st

    def want(self, name, keep=False, timeout=5.0):
        """The local path of `name` (a path, a bare name, or 'node:name'), replicated
        here from its machine (or this machine's own SHM). keep: replicated even
        when nobody uses it, until release()."""
        ms = int(timeout * 1000)
        return self._one(f"{'KEEP' if keep else 'WANT'} {name} {ms}", timeout + 3.0)

    def release(self, name):
        """Stop replicating `name` here (its local replica is removed)."""
        return self._one(f'RELEASE {name}')

    def stop(self):
        """Stop the local service."""
        return self._one('QUIT')


def ls():
    return Client().ls()


def status():
    return Client().status()


def want(name, keep=False, timeout=5.0):
    return Client().want(name, keep, timeout)


def release(name):
    return Client().release(name)


def open(name, timeout=5.0):
    """The SHM `name` of any machine of the network, as a daoShm.shm."""
    import daoShm
    return daoShm.shm(Client().want(name, timeout=timeout))


def keywords(shm):
    """The keywords of a daoShm.shm, {name: value}: a replica's NET_REPLICA (its
    machine), NET_LINK (1: connected), NET_AGE_US (the last frame's age), NET_DROPPED
    (frames skipped), NET_SRC_CNT, NET_SRC_TIME, and the source's own keywords.
    Read with the C layout (120 bytes each)."""
    import ctypes
    import struct
    md = shm.image.md[0]
    n = int(md.NBkw)
    if n == 0 or not shm.image.kw:
        return {}
    raw = ctypes.string_at(ctypes.cast(shm.image.kw, ctypes.c_void_p).value, 120 * n)
    out = {}
    for i in range(n):
        name, ktype, value, _comment = struct.unpack_from('16sc7x16s80s', raw, 120 * i)
        name = name.split(b'\0', 1)[0].decode('ascii', 'replace')
        ktype = ktype.decode('ascii', 'replace')
        if ktype == 'L':
            out[name] = struct.unpack_from('<q', value)[0]
        elif ktype == 'D':
            out[name] = struct.unpack_from('<d', value)[0]
        elif ktype == 'S':
            out[name] = value.split(b'\0', 1)[0].decode('utf-8', 'replace')
    return out


def running(port=None):
    """Is a daoShmNetd answering on this machine?"""
    try:
        Client(port).ping()
        return True
    except ServiceError:
        return False


def service_executable():
    """The daoShmNetd program: on the PATH, in $DAOROOT/bin, or next to this module's build."""
    exe = 'daoShmNetd.exe' if sys.platform == 'win32' else 'daoShmNetd'
    found = shutil.which(exe)
    if found:
        return found
    for base in (os.path.join(os.environ.get('DAOROOT', ''), 'bin'),
                 os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'build', 'src')):
        path = os.path.join(base, exe)
        if os.path.isfile(path):
            return os.path.abspath(path)
    raise ServiceError('daoShmNetd not found (install daoBase, or put it on the PATH)')


def start(args=(), log=None, wait=5.0):
    """Start daoShmNetd in the background with these options (daoShmNetd --help);
    its log appended to `log` (default: <temp dir>/daoShmNetd-<control port>.log).
    Returns the process; ServiceError if it does not answer within `wait` s."""
    args = list(args)
    port = control_port()
    if '--control-port' in args:
        port = int(args[args.index('--control-port') + 1])
    if running(port):
        raise ServiceError(f'a daoShmNetd already answers on 127.0.0.1:{port}')
    if log is None:
        import tempfile
        log = os.path.join(tempfile.gettempdir(), f'daoShmNetd-{port}.log')
    out = builtins.open(log, 'a')                    # this module's open() is for SHMs
    kwargs = {'stdout': out, 'stderr': subprocess.STDOUT, 'stdin': subprocess.DEVNULL}
    if sys.platform == 'win32':
        kwargs['creationflags'] = subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.DETACHED_PROCESS
    else:
        kwargs['start_new_session'] = True           # survives the terminal
    proc = subprocess.Popen([service_executable()] + args, **kwargs)
    out.close()
    t0 = time.monotonic()
    while time.monotonic() - t0 < wait:
        if running(port):
            return proc
        if proc.poll() is not None:
            break
        time.sleep(0.05)
    raise ServiceError(f'daoShmNetd did not start (see {log})')
