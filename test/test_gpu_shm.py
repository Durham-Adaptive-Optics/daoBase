#!/usr/bin/env python3
"""
End-to-end tests of GPU SHMs (daoShmCreateGpu, daoGpuShmd), across processes.

Needs a CUDA GPU, nvcc, and a daoBase build (waf build) with GPU support.
Uses its own daoGpuShmd (DAO_GPU_SOCKET in a temporary folder), so a daemon
already running for the user is not touched.

    python3 test/test_gpu_shm.py            # from the daoBase folder
"""
import ctypes
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILD = os.path.join(ROOT, "build", "src")
TMP = tempfile.mkdtemp(prefix="daogpu")
TOOL = os.path.join(TMP, "testGpuShm")
KERNEL = os.path.join(TMP, "testGpuShmKernel")
DAEMON = os.path.join(BUILD, "daoGpuShmd")

ENV = dict(os.environ, DAO_GPU_SOCKET=os.path.join(TMP, "d.sock"), DAO_GPU_DAEMON=DAEMON,
           LD_LIBRARY_PATH=BUILD + ":" + os.environ.get("LD_LIBRARY_PATH", ""))
NAMES = [f"/tmp/dqg_{k}.im.shm" for k in "abcdefgh"]
results = []


def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (f"   [{detail}]" if detail and not cond else ""))
    results.append(bool(cond))


def run(*args, env=None, timeout=30):
    p = subprocess.run([str(a) for a in args], env=env or ENV, capture_output=True, text=True, timeout=timeout)
    last = [l for l in p.stdout.splitlines() if l.strip()]
    return p.returncode, (last[-1] if last else "") + ("" if p.returncode == 0 else " | " + p.stderr[-300:])


def start(*args, env=None):
    p = subprocess.Popen([str(a) for a in args], env=env or ENV, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL, text=True)
    return p


def wait_line(p, text, timeout=10):
    t = time.time()
    while time.time() - t < timeout:
        line = p.stdout.readline()
        if text in line:
            return True
    return False


def daemon_list():
    return subprocess.run([DAEMON, "--list"], env=ENV, capture_output=True, text=True).stdout


def daemon_pid():
    out = subprocess.run([DAEMON, "--ping"], env=ENV, capture_output=True, text=True).stdout
    m = re.search(r"pid (\d+)", out)
    return int(m.group(1)) if m else None


def gpu_used_mib(index=0):
    out = subprocess.run(["nvidia-smi", "--query-gpu=memory.used", "--format=csv,noheader,nounits",
                          "-i", str(index)], capture_output=True, text=True).stdout
    return int(out.strip())


def cnt0(name):
    rc, out = run(TOOL, "info", name)
    return int(re.search(r"cnt0=(\d+)", out).group(1))


def build():
    inc = os.path.join(ROOT, "include")
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-I", inc, os.path.join(HERE, "testGpuShm.c"),
                    "-o", TOOL, "-L", BUILD, "-ldao"], check=True)
    subprocess.run(["nvcc", "-O2", "-I", inc, os.path.join(HERE, "testGpuShmKernel.cu"), "-o", KERNEL,
                    "-L", BUILD, "-ldao"], check=True, capture_output=True)


def main():
    build()
    for n in NAMES:
        if os.path.exists(n):
            os.remove(n)
    a, b, c, d, e, f, g, big = NAMES

    # --- lifetime -----------------------------------------------------------------
    rc, out = run(TOOL, "create", a, 0, 1, 3, "exit")
    check("create (mirror) + normal exit", rc == 0, out)
    check("daemon started on demand and holds it", a in daemon_list())
    rc, out = run(TOOL, "read", a, 3)
    check("new process reads it after the creator exited", rc == 0 and "d_array=0x" in out, out)

    rc, out = run(TOOL, "create", b, 0, 0, 5, "crash")
    rc2, info = run(TOOL, "info", b)
    check("creator aborts right after creating (no mirror)", rc != 0 and "gpu=1" in info, out + " / " + info)
    rc, out = run(TOOL, "host", b, 5)
    check("no mirror: /tmp host copy not written by the creator", rc != 0, out)
    rc, out = run(TOOL, "read", b, 5)
    check("new process reads it after the creator crashed (GetData copies from GPU)", rc == 0, out)
    rc, out = run(TOOL, "host", b, 5)
    check("...and GetData refreshed the host copy", rc == 0, out)

    p = start(TOOL, "create", c, 0, 1, 11, "hold")
    wait_line(p, "OK id=")
    p.send_signal(signal.SIGKILL)
    p.wait()
    rc, out = run(TOOL, "read", c, 11)
    check("creator killed with SIGKILL while holding: data intact", rc == 0, out)

    # --- updates and semaphores -----------------------------------------------------
    target = cnt0(a) + 1
    w = start(TOOL, "wait", a, 1, target, 10, 7)
    wait_line(w, "READY")
    rc, out = run(TOOL, "write", a, 7)
    wout = w.communicate(timeout=15)[0]
    check("SetData from another process wakes a waiting reader, data correct",
          rc == 0 and w.returncode == 0 and "OK" in wout, out + " / " + wout.strip())
    rc, out = run(TOOL, "read", a, 7)
    check("mirrored host copy updated by SetData", rc == 0, out)

    # --- CUDA kernels on d_array ----------------------------------------------------
    target = cnt0(a) + 1
    w = start(TOOL, "wait", a, 2, target, 10, 14, 2)
    wait_line(w, "READY")
    rc, out = run(KERNEL, "scale", a, 2)
    wout = w.communicate(timeout=15)[0]
    check("kernel x2 + daoShmCommit wakes reader; mirror holds 2*(7+i)",
          rc == 0 and w.returncode == 0, out + " / " + wout.strip())
    rc, out = run(TOOL, "host", a, 14, 2)
    check("mirror updated asynchronously by the commit", rc == 0, out)

    rc, out = run(KERNEL, "scale", b, 3)
    rc2, out2 = run(TOOL, "read", b, 15, 3)
    check("kernel on a non-mirrored SHM, read back through GetData", rc == 0 and rc2 == 0, out + " / " + out2)

    rc, out = run(KERNEL, "device", c, 100)
    rc2, out2 = run(TOOL, "read", c, 100)
    check("daoShmSetDataDevice from a cudaMalloc buffer", rc == 0 and rc2 == 0, out + " / " + out2)

    # --- GPU pipeline: IN (host writes) -> kernel -> OUT, no host round trip -----------
    run(TOOL, "create", d, 0, 0, 0, "exit")
    run(TOOL, "create", e, 0, 1, 0, "exit")
    frames = 20
    pipe = start(KERNEL, "pipeline", d, e, frames)
    ok = wait_line(pipe, "READY")
    for k in range(1, frames + 1):
        target = cnt0(e) + 1
        w = start(TOOL, "wait", e, 4, target, 10, 2 * k, 2)
        wait_line(w, "READY")
        run(TOOL, "write", d, k)
        w.communicate(timeout=15)
        ok = ok and w.returncode == 0
    pout = pipe.communicate(timeout=20)[0]
    check(f"pipeline IN -> kernel -> OUT, {frames} frames, each checked by a waiting reader",
          ok and pipe.returncode == 0 and f"frames={frames}" in pout, pout.strip()[-200:])

    # --- two GPUs, UUID lookup ------------------------------------------------------
    n_gpus = len(subprocess.run(["nvidia-smi", "-L"], capture_output=True, text=True).stdout.splitlines())
    if n_gpus > 1:
        rc, out = run(TOOL, "create", f, 1, 1, 9, "exit")
        rc2, out2 = run(TOOL, "read", f, 9, env=dict(ENV, CUDA_VISIBLE_DEVICES="1"))
        rc3, out3 = run(TOOL, "read", f, 9, env=dict(ENV, CUDA_VISIBLE_DEVICES="0"))
        check("SHM on GPU 1 read with CUDA_VISIBLE_DEVICES=1 (found by UUID)", rc == 0 and rc2 == 0, out + " / " + out2)
        check("...and refused when that GPU is hidden", rc3 != 0, out3)

    # --- no GPU in the reader ---------------------------------------------------------
    nogpu = dict(ENV, CUDA_VISIBLE_DEVICES="")
    rc, out = run(TOOL, "read", a, 14, 2, env=nogpu)
    check("reader without GPU: mirrored SHM readable from its host copy", rc == 0 and "d_array=(nil)" in out, out)
    rc, out = run(TOOL, "read", b, 15, 3, env=nogpu)
    check("reader without GPU: non-mirrored SHM refused", rc != 0, out)

    # --- Python binding -------------------------------------------------------------------
    py = f"""
import sys, numpy as np
sys.path.insert(0, {os.path.join(ROOT, 'src', 'python')!r})
import daoShm
s = daoShm.shm({a!r})
x = s.get_data()
ok = np.array_equal(x.ravel(), (14 + 2 * np.arange(4096)).astype(np.float32))
s.set_data((20 + np.arange(4096)).astype(np.float32).reshape(x.shape))
print("OK" if ok else "FAIL", x.dtype, x.shape)
"""
    p = subprocess.run([sys.executable, "-c", py], env=ENV, capture_output=True, text=True, timeout=60)
    rc2, out2 = run(TOOL, "read", a, 20)
    check("Python daoShm: get_data / set_data on a GPU SHM", "OK" in p.stdout and rc2 == 0,
          p.stdout[-200:] + p.stderr[-300:] + " / " + out2)

    py = f"""
import sys, numpy as np, cupy
sys.path.insert(0, {os.path.join(ROOT, 'src', 'python')!r})
import daoShm
s = daoShm.shm({g!r}, (1 + np.arange(4096)).astype(np.float32).reshape(64, 64), gpu=0)
d = s.get_device_array()
ok = s.is_gpu() and isinstance(d, cupy.ndarray) and d.shape == (64, 64) and float(d[0, 0]) == 1.0
d *= 3                                   # in place on the GPU payload
s.commit()                               # publish (default stream)
cupy.cuda.Stream.null.synchronize()
c = daoShm.shm("/tmp/dqg_cpu.im.shm", np.arange(12, dtype=np.int16).reshape(3, 4))
ok = ok and not c.is_gpu() and c.device_ptr() is None
c.set_data(np.full((3, 4), 7, np.int16))
ok = ok and np.array_equal(daoShm.shm("/tmp/dqg_cpu.im.shm").get_data(), np.full((3, 4), 7, np.int16))
print("OK" if ok else "FAIL")
"""
    p = subprocess.run([sys.executable, "-c", py], env=ENV, capture_output=True, text=True, timeout=60)
    rc2, out2 = run(TOOL, "read", g, 3, 3)
    check("Python: create GPU SHM, CuPy in-place x3 + commit, read by a C process; CPU SHMs unchanged",
          "OK" in p.stdout and rc2 == 0, p.stdout[-200:] + p.stderr[-400:] + " / " + out2)
    if os.path.exists("/tmp/dqg_cpu.im.shm"):
        os.remove("/tmp/dqg_cpu.im.shm")

    # --- cleanup semantics --------------------------------------------------------------
    os.remove(c)
    time.sleep(2.2)
    check("removing the /tmp file makes the daemon release the payload", c not in daemon_list())
    rc, out = run(TOOL, "create", a, 0, 1, 42, "exit")
    lst = daemon_list()
    rc2, out2 = run(TOOL, "read", a, 42)
    check("recreating a SHM replaces its payload (one entry, new data)",
          rc == 0 and rc2 == 0 and lst.count(a) == 1, lst + out2)

    pid = daemon_pid()
    fds = [os.readlink(f"/proc/{pid}/fd/{x}") for x in os.listdir(f"/proc/{pid}/fd")] if pid else []
    held = len([l for l in daemon_list().splitlines()[1:] if l.strip()])
    check(f"daemon: one GPU handle per held payload ({held}), no SHM file or stray descriptors",
          pid and not any(".im.shm" in x for x in fds)
          and sum("nvidia" in x for x in fds) == held and fds.count("/dev/null") == 1, str(fds))

    before = gpu_used_mib(0)
    rc, out = run(TOOL, "big", big, 0, 512)
    during = gpu_used_mib(0)
    os.remove(big)
    time.sleep(2.5)
    after = gpu_used_mib(0)
    check(f"512 MiB payload: held after its creator exited, freed when the file is removed "
          f"({before} -> {during} -> {after} MiB)", rc == 0 and during - before >= 500 and after - before < 50)

    # --- teardown -----------------------------------------------------------------------
    for n in NAMES:
        if os.path.exists(n):
            os.remove(n)
    if pid:
        os.kill(pid, signal.SIGTERM)
    shutil.rmtree(TMP, ignore_errors=True)
    print(f"\n{sum(results)}/{len(results)} passed")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
