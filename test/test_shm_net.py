"""
Network SHMs (daoShmNetd, daoNet): two services on this machine, as two machines.

Service A owns the SHMs (in its own directory); service B replicates them (its
replicas prefixed B_: on one machine, a replica with the very same name would
share its source's semaphores). They reach each other through --peer, on their
own ports and domain: nothing goes out on the network.

    PYTHONPATH=src/python LD_LIBRARY_PATH=build/src python -m pytest test/test_shm_net.py
"""
import os
import random
import shutil
import socket
import subprocess
import time

import numpy as np
import pytest

import daoNet
import daoShm

BASE = random.randint(20000, 30000)
A = {'node': 'nodeA', 'dir': f'/tmp/daonetTestA{os.getpid()}', 'port': BASE, 'control': BASE + 1}
B = {'node': 'nodeB', 'dir': f'/tmp/daonetTestB{os.getpid()}', 'port': BASE + 10, 'control': BASE + 11,
     'prefix': 'B_'}
DOMAIN = f'test{os.getpid()}'


def _start(svc, peer, extra=()):
    args = [daoNet.service_executable(), '--name', svc['node'], '--domain', DOMAIN, '--dir', svc['dir'],
            '--port', str(svc['port']), '--control-port', str(svc['control']),
            '--beacon-port', str(svc['port'] + 5), '--no-multicast', '--peer', f"127.0.0.1:{peer['port']}"]
    if svc.get('prefix'):
        args += ['--replica-prefix', svc['prefix']]
    log = open(os.path.join(svc['dir'], 'daoShmNetd.log'), 'a')          # removed with the directory
    proc = subprocess.Popen(args + list(extra), stdout=log, stderr=subprocess.STDOUT)
    t0 = time.monotonic()
    while not daoNet.running(svc['control']):
        assert proc.poll() is None, 'daoShmNetd exited'
        assert time.monotonic() - t0 < 5, 'daoShmNetd does not answer'
        time.sleep(0.05)
    return proc


def _stop(proc):
    if proc and proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(3)
        except subprocess.TimeoutExpired:
            proc.kill()


@pytest.fixture(scope='module')
def services():
    for d in (A['dir'], B['dir']):
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
    procs = {'A': _start(A, B), 'B': _start(B, A, ['--idle', '3'])}
    os.environ['DAO_NET_CONTROL'] = str(B['control'])        # this process: "machine B"
    yield procs                                              # a test may restart one: stop what is there
    for proc in procs.values():
        _stop(proc)
    if not os.environ.get('DAO_NET_TEST_KEEP'):              # =1: keep the services' logs to look at
        for d in (A['dir'], B['dir']):
            shutil.rmtree(d, ignore_errors=True)


def _source(name, data):
    return daoShm.shm(f"{A['dir']}/{name}.im.shm", data)


def _wait_listed(name, timeout=6.0):
    t0 = time.monotonic()
    while time.monotonic() - t0 < timeout:
        if any(s['name'] == name and s['node'] == 'nodeA' for s in daoNet.ls()):
            return
        time.sleep(0.1)
    raise AssertionError(f'{name} never listed on B')


def _eventually(check, timeout=5.0):
    t0 = time.monotonic()
    while time.monotonic() - t0 < timeout:
        if check():
            return True
        time.sleep(0.05)
    return False


def test_discovery_lists_the_other_machine(services):
    src = _source('listed', np.zeros((8, 4), np.float32))
    _wait_listed('listed')
    entry = [s for s in daoNet.ls() if s['name'] == 'listed'][0]
    assert entry == {'node': 'nodeA', 'name': 'listed', 'shape': '8x4', 'type': 'float32', 'state': 'remote'}
    peers = {p['node'] for p in daoNet.status()['peers']}
    assert 'nodeA' in peers
    del src


def test_open_by_name_like_a_local_shm(services):
    src = _source('frames', np.arange(12, dtype=np.float32).reshape(4, 3))
    _wait_listed('frames')
    t0 = time.monotonic()
    r = daoShm.shm('nodeA:frames')                            # not on "B": made local
    assert time.monotonic() - t0 < 2.0
    assert r.image.name.decode().endswith('/B_frames.im.shm')
    assert np.array_equal(r.get_data(), np.arange(12, dtype=np.float32).reshape(4, 3))
    for k in range(20):                                       # every write arrives, in order
        c0 = r.get_counter()
        src.set_data(np.full((4, 3), k, np.float32))
        assert _eventually(lambda: r.get_counter() > c0, 2.0)
        assert np.array_equal(r.get_data(), np.full((4, 3), k, np.float32))
    kw = daoNet.keywords(r)
    assert kw['NET_REPLICA'] == 'nodeA' and kw['NET_LINK'] == 1
    assert kw['NET_SRC_CNT'] == src.get_counter()
    assert 0 <= kw['NET_AGE_US'] < 1_000_000                  # one machine: one clock


def test_semaphores_wake_the_reader(services):
    """A reader waiting on a replica's semaphore wakes at each frame of the source."""
    import threading
    src = _source('sem', np.zeros(16, np.float64))
    _wait_listed('sem')
    r = daoShm.shm('nodeA:sem')
    while r.get_data(check=True, semNb=2, timeout=0.02) is not None:
        pass                                                  # the posts made so far
    got = []

    def reader():
        for _ in range(5):
            v = r.get_data(check=True, semNb=2, timeout=2.0)
            got.append(None if v is None else v[0])

    t = threading.Thread(target=reader)
    t.start()
    for k in range(5):
        time.sleep(0.05)
        src.set_data(np.full(16, k + 1, np.float64))
    t.join(10)
    assert got == [1.0, 2.0, 3.0, 4.0, 5.0]


def test_a_fast_writer_skips_frames_never_tears_them(services):
    n = 256 * 1024                                            # 1 MB of float32
    src = _source('fast', np.zeros(n, np.float32))
    _wait_listed('fast')
    r = daoShm.shm('nodeA:fast')
    for k in range(1, 400):                                   # as fast as it goes
        src.set_data(np.full(n, k, np.float32))
    assert _eventually(lambda: r.get_data()[0] == 399, 5.0)
    for _ in range(50):                                       # every frame read is whole
        v = r.get_data()
        assert v.min() == v.max()
    kw = daoNet.keywords(r)
    assert kw['NET_SRC_CNT'] == src.get_counter()
    assert kw['NET_DROPPED'] >= 0


def test_a_write_on_the_replica_goes_to_the_owner(services):
    src = _source('cmd', np.zeros(4, np.float32))
    _wait_listed('cmd')
    r = daoShm.shm('nodeA:cmd')
    c_src, c_rep = src.get_counter(), r.get_counter()
    r.set_data(np.array([1, 2, 3, 4], np.float32))           # written on "B"
    assert _eventually(lambda: np.array_equal(src.get_data(), [1, 2, 3, 4]))
    assert src.get_counter() == c_src + 1
    time.sleep(0.5)
    assert r.get_counter() == c_rep + 1                       # its echo is not published again
    src.set_data(np.array([5, 6, 7, 8], np.float32))          # and the owner's writes still arrive
    assert _eventually(lambda: np.array_equal(r.get_data(), [5, 6, 7, 8]))


def test_a_recreated_source_reaches_the_replica(services):
    _source('shape', np.zeros((4, 4), np.float32))
    _wait_listed('shape')
    r = daoShm.shm('nodeA:shape')
    assert r.get_data().shape == (4, 4)
    _source('shape', np.ones((2, 8), np.float32))            # recreated: another shape
    assert _eventually(lambda: daoShm.shm(r.image.name.decode()).get_data().shape == (2, 8), 6.0)


def test_the_link_goes_down_and_comes_back(services):
    src = _source('link', np.zeros(8, np.float32))
    _wait_listed('link')
    r = daoShm.shm('nodeA:link')
    assert daoNet.keywords(r)['NET_LINK'] == 1
    _stop(services['A'])
    assert _eventually(lambda: daoNet.keywords(r)['NET_LINK'] == 0, 8.0)
    assert np.array_equal(r.get_data(), np.zeros(8))          # the last value stays
    services['A'] = _start(A, B)
    src.set_data(np.full(8, 3, np.float32))
    assert _eventually(lambda: daoNet.keywords(r)['NET_LINK'] == 1 and r.get_data()[0] == 3, 12.0)


def test_an_unused_replica_is_dropped_a_kept_one_not(services):
    _source('idle', np.zeros(8, np.float32))
    _source('kept', np.zeros(8, np.float32))
    _wait_listed('idle')
    _wait_listed('kept')
    path = daoNet.want('idle')                                # replicated, nobody maps it
    kept = daoNet.want('kept', keep=True)
    assert os.path.exists(path) and os.path.exists(kept)
    assert _eventually(lambda: not os.path.exists(path), 12.0)
    assert os.path.exists(kept)
    daoNet.release('kept')
    assert not os.path.exists(kept)


def test_nowhere_and_no_service_fail_fast(services):
    with pytest.raises(FileNotFoundError):
        daoShm.shm('/tmp/noSuchShmAnywhere.im.shm')
    os.environ['DAO_NET_CONTROL'] = str(_free_port())           # no service there
    try:
        t0 = time.monotonic()
        with pytest.raises(FileNotFoundError):
            daoShm.shm('/tmp/noSuchShmAnywhere.im.shm')
        assert time.monotonic() - t0 < 0.5
    finally:
        os.environ['DAO_NET_CONTROL'] = str(B['control'])


def _free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]
