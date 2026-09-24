#!/usr/bin/env python3
"""
Small runnable example for daoShm.py.

Run with:
    python3 example_shm.py
(needs libdao.so on your library path, e.g. via daoBase's dao_env.sh)
"""
import numpy as np
from daoShm import shm

name = "/tmp/test_python.im.shm"

# Create a new 4x4 float32 SHM (or overwrite an existing one).
writer = shm(name, data=np.zeros((4, 4), dtype=np.float32))
print(f"created {name}, counter = {writer.get_counter()}")

writer.set_data(np.ones((4, 4), dtype=np.float32))
print(f"wrote a frame, counter = {writer.get_counter()}")

# Attach to the same SHM from a second handle, as a separate process
# reading it would.
reader = shm(name)
data = reader.get_data()
print(f"read back shape={data.shape}, sum={data.sum()}")

writer.close()
reader.close()
