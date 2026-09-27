#!/usr/bin/env python3
"""
Tests of automatic semaphores: each reader waits on a semaphore of its own
(daoShmWait, DAO_SEM_AUTO, daoShm.shm.get_data(check=True) without semNb).

Needs a daoBase build (waf build).

    python3 test/test_sem_claim.py            # from the daoBase folder
"""
import os
import signal
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILD = os.path.join(ROOT, "build", "src")
PYDIR = os.path.join(ROOT, "src", "python")
if not os.environ.get("LD_LIBRARY_PATH", "").startswith(BUILD):
    # the loader reads LD_LIBRARY_PATH at start: this build's libdao, not an installed one
    os.environ["LD_LIBRARY_PATH"] = BUILD + os.pathsep + os.environ.get("LD_LIBRARY_PATH", "")
    os.environ["PYTHONPATH"] = os.pathsep.join([PYDIR, os.path.join(ROOT, "build"),  # + generated *_pb2
                                                os.environ.get("PYTHONPATH", "")])
    os.execv(sys.executable, [sys.executable] + sys.argv)
ENV = dict(os.environ)
sys.path.insert(0, PYDIR)

import numpy as np   # noqa: E402
import daoShm        # noqa: E402

NAME = "/tmp/dqsem_%d.im.shm" % os.getpid()
results = []


def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (f"   [{detail}]" if detail and not cond else ""))
    results.append(bool(cond))


def reader(code):
    """A reader in another process: prints 'ready' once it holds its semaphore."""
    prog = ("import sys, daoShm\n"
            f"s = daoShm.shm({NAME!r})\n" + code)
    return subprocess.Popen([sys.executable, "-c", prog], env=ENV, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)


def wait_ready(p, timeout=10):
    t = time.time()
    while time.time() - t < timeout:
        line = p.stdout.readline()
        if line.startswith("ready"):
            return line.split()[1:]
    return None


def main():
    w = daoShm.shm(NAME, np.zeros((4, 4), np.float32))
    nsem = w.image.md.contents.sem
    check("the SHM has semaphores", nsem >= 2, nsem)

    # two handles of one process: two semaphores
    a, b = daoShm.shm(NAME), daoShm.shm(NAME)
    sa, sb = a.sem, b.sem
    check("a handle gets a semaphore", 0 <= sa < nsem, sa)
    check("it keeps it", a.sem == sa)
    check("two handles of one process get different ones", sb >= 0 and sb != sa, (sa, sb))
    check("sem_in_use sees them", a.sem_in_use(sa) and a.sem_in_use(sb))
    b.close()
    c = daoShm.shm(NAME)
    check("a closed handle's semaphore is free again", not c.sem_in_use(sb))
    check("... and taken by the next reader", c.sem == sb, (c.sem, sb))
    c.close()
    r = daoShm.shm(NAME)
    first = r.sem
    r = daoShm.shm(NAME)                          # the old object is dropped: its semaphore freed
    check("a dropped object frees its semaphore", r.sem == first, (first, r.sem))
    r.close()
    r.close()
    check("closing twice is harmless", True)

    # Ctrl+C in a wait: the semaphore is given back, though the traceback keeps the object
    r = daoShm.shm(NAME)
    threading.Timer(0.3, lambda: os.kill(os.getpid(), signal.SIGINT)).start()
    try:
        r.get_data(check=True)
        kept = None
    except KeyboardInterrupt as exc:
        kept = exc                                # as IPython keeps the last traceback
    check("Ctrl+C in a wait gives the semaphore back", kept is not None and not daoShm.shm(NAME).sem_in_use(first),
          (kept, first))
    taken = r.sem
    check("... and the next wait takes one again", taken >= 0, taken)
    del r, kept

    # other processes: never a semaphore already taken
    p = reader("import time; print('ready', s.sem, flush=True); time.sleep(60)\n")
    got = wait_ready(p)
    theirs = int(got[0]) if got else -1
    check("another process gets a semaphore", theirs >= 0, got)
    check("... not one this process holds", theirs != sa, (theirs, sa))
    check("this process sees it taken", a.sem_in_use(theirs))
    users = a.sem_users()
    check("sem_users names the processes", users.get(theirs) == [p.pid] and users.get(sa) == [os.getpid()],
          (users, theirs, p.pid, sa))
    p.send_signal(signal.SIGKILL)                 # a crash: the OS drops its lock
    p.wait()
    check("a killed reader's semaphore is free again", not a.sem_in_use(theirs))

    # every reader gets every frame
    n = 50
    progs = [reader("print('ready', s.sem, flush=True)\n"
                    "k = 0\n"
                    f"for i in range({n}):\n"
                    "    d = s.get_data(check=True, timeout=5)\n"
                    "    if d is None: break\n"
                    "    k += 1\n"
                    "print('got', k, flush=True)\n") for _ in range(3)]
    sems = [wait_ready(q) for q in progs]
    sems = [int(x[0]) if x else -1 for x in sems]
    check("three readers, three semaphores", len(set(sems)) == 3 and -1 not in sems and sa not in sems,
          (sa, sems))
    time.sleep(0.2)
    for i in range(n):
        w.set_data(np.full((4, 4), i, np.float32))
        time.sleep(0.01)
    outs = [q.communicate(timeout=20)[0] for q in progs]
    counts = [int(o.split()[-1]) if o.strip() else -1 for o in outs]
    check("each reader got every frame", counts == [n] * 3, counts)

    # the first wait is for the next frame, not one written before
    d = daoShm.shm(NAME)
    w.set_data(np.full((4, 4), 7, np.float32))
    t = time.time()
    got = d.get_data(check=True, timeout=0.3)
    check("a new reader waits for the next frame", got is None and time.time() - t > 0.25)
    w.set_data(np.full((4, 4), 8, np.float32))
    got = d.get_data(check=True, timeout=1)
    check("... and gets it", got is not None and got[0, 0] == 8)
    d.close()

    # an explicit number is honoured (and warned about when taken)
    e = daoShm.shm(NAME)
    w.set_data(np.full((4, 4), 9, np.float32))
    got = e.get_data(check=True, semNb=sa, timeout=1)
    check("an explicit semaphore still works, taken or not", got is not None)
    check("an automatic one then skips it", e.sem != sa)
    e.close()

    # a forked child claims its own
    r, wfd = os.pipe()
    pid = os.fork()
    if pid == 0:
        os.close(r)
        os.write(wfd, str(a.sem).encode())
        os._exit(0)
    os.close(wfd)
    child = int(os.read(r, 32) or b"-1")
    os.waitpid(pid, 0)
    check("a forked child takes its own semaphore", child >= 0 and child != sa, (child, sa))
    check("... and the parent keeps its own", a.sem == sa and a.sem_in_use(sa))

    # all taken: an error, not a shared semaphore
    hs = [daoShm.shm(NAME) for _ in range(nsem)]
    ok = [h.sem for h in hs]
    check("the last free semaphores are handed out", sorted(ok[:nsem - 1]) == sorted(set(range(nsem)) - {sa}),
          ok)
    check("none left: -1", ok[-1] == -1, ok)
    try:
        hs[-1].get_data(check=True, timeout=0.1)
        check("none left: a wait raises", False)
    except RuntimeError:
        check("none left: a wait raises", True)
    for h in hs:
        h.close()
    a.close()
    w.close()


if __name__ == "__main__":
    try:
        main()
    finally:
        if os.path.exists(NAME):
            os.remove(NAME)
        base = os.path.basename(NAME).split(".")[0]
        for f in os.listdir("/dev/shm") if os.path.isdir("/dev/shm") else []:
            if f.startswith("sem." + base + "_"):
                os.remove(os.path.join("/dev/shm", f))
    print("%d/%d passed" % (sum(results), len(results)))
    sys.exit(0 if all(results) else 1)
