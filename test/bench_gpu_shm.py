#!/usr/bin/env python3
"""
Benchmark: pass frames between two GPU processes through a host SHM or a GPU SHM.

Stage A produces a frame on the GPU; stage B (another process) sums its rows
on the GPU and publishes the result. Each frame is timed from the start of A's
GPU work to B's result being published, for several frame sizes, with and
without NVIDIA MPS. See benchGpuShm.cu for the modes.

    python3 test/bench_gpu_shm.py [--frames 500] [--device 0]

Needs a CUDA GPU, nvcc, nvidia-cuda-mps-control, and a daoBase build with GPU
support. Uses its own daoGpuShmd and its own MPS server (private folders), so
other GPU programs and a daemon already running are not affected.
"""
import argparse
import os
import signal
import subprocess
import tempfile
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILD = os.path.join(ROOT, "build", "src")
SIZES = [(256, 256), (512, 512), (1024, 1024), (2048, 2048)]
MODES = ["host", "gpu", "gpu-sync"]
SHMS = ["/tmp/bgs_in.im.shm", "/tmp/bgs_out.im.shm"]


def clean():
    for s in SHMS:
        if os.path.exists(s):
            os.remove(s)


def run_case(exe, env, mode, w, h, frames, dev):
    clean()
    time.sleep(1.2)                      # daoGpuShmd drops the previous payload
    r = subprocess.run([exe, "setup", mode, str(w), str(h), str(dev)], env=env, capture_output=True, text=True)
    if "SETUP OK" not in r.stdout:
        return None, "setup: " + r.stdout[-200:] + r.stderr[-200:]
    b = subprocess.Popen([exe, "stageB", mode, str(dev)], env=env, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL, text=True)
    t = time.time()
    while time.time() - t < 20:          # libdao logs to stdout too: look for READY
        line = b.stdout.readline()
        if not line or "READY" in line:
            break
    if "READY" not in line:
        b.kill()
        return None, "stage B did not start"
    time.sleep(0.3)
    r = subprocess.run([exe, "run", mode, str(frames), str(dev)], env=env, capture_output=True, text=True,
                       timeout=300)
    b.send_signal(signal.SIGINT)
    try:
        b.wait(timeout=10)
    except subprocess.TimeoutExpired:
        b.kill()
    lat = check = None
    for line in r.stdout.splitlines():
        if line.startswith("LAT "):
            lat = np.array([float(x) for x in line.split()[1:]])
        if line.startswith("CHECK"):
            check = line.split()[1]
    if lat is None:
        return None, r.stdout[-300:] + r.stderr[-300:]
    return (lat, check), None


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--frames", type=int, default=500)
    p.add_argument("--device", type=int, default=0)
    args = p.parse_args()

    with tempfile.TemporaryDirectory(prefix="bgs") as tmp:
        exe = os.path.join(tmp, "benchGpuShm")
        subprocess.run(["nvcc", "-O2", "-I", os.path.join(ROOT, "include"), os.path.join(HERE, "benchGpuShm.cu"),
                        "-o", exe, "-L", BUILD, "-ldao"], check=True, capture_output=True)
        base = dict(os.environ, DAO_GPU_SOCKET=os.path.join(tmp, "d.sock"),
                    DAO_GPU_DAEMON=os.path.join(BUILD, "daoGpuShmd"),
                    LD_LIBRARY_PATH=BUILD + ":" + os.environ.get("LD_LIBRARY_PATH", ""))
        mps = dict(base, CUDA_MPS_PIPE_DIRECTORY=os.path.join(tmp, "mps"),
                   CUDA_MPS_LOG_DIRECTORY=os.path.join(tmp, "mpslog"))
        os.makedirs(mps["CUDA_MPS_PIPE_DIRECTORY"])
        os.makedirs(mps["CUDA_MPS_LOG_DIRECTORY"])

        print(f"{'MPS':>4} {'frame':>14} {'case':>9} {'median':>8} {'p99':>8} {'max':>8}  check"
              f"   (us from A's GPU work to B's result, {args.frames} frames)")
        for use_mps in (False, True):
            env = mps if use_mps else base
            if use_mps:
                subprocess.run(["nvidia-cuda-mps-control", "-d"], env=env, check=True)
                time.sleep(1)
            try:
                for w, h in SIZES:
                    size = f"{w}x{h} {w * h * 4 / 2**20:g}MB"
                    for mode in MODES:
                        res, err = run_case(exe, env, mode, w, h, args.frames, args.device)
                        if err:
                            print(f"{'on' if use_mps else 'off':>4} {size:>14} {mode:>9}  ERROR {err}")
                            continue
                        lat, check = res
                        print(f"{'on' if use_mps else 'off':>4} {size:>14} {mode:>9} {np.median(lat):8.1f} "
                              f"{np.percentile(lat, 99):8.1f} {lat.max():8.1f}  {check}")
            finally:
                if use_mps:
                    subprocess.run(["nvidia-cuda-mps-control"], input="quit\n", env=env, text=True)
                    time.sleep(1)
        clean()
        out = subprocess.run([os.path.join(BUILD, "daoGpuShmd"), "--ping"], env=base, capture_output=True, text=True)
        if "pid" in out.stdout:
            os.kill(int(out.stdout.split("pid")[1]), signal.SIGTERM)


if __name__ == "__main__":
    main()
