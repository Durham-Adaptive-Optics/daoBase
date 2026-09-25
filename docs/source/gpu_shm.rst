GPU Shared Memory
=================

.. warning::

   **Experimental Feature** — Linux and NVIDIA CUDA only. FIFO GPU SHMs (depth > 1) and partial
   writes (``daoShmSetDataPart``) are not supported yet.

Overview
--------

A GPU SHM is a normal DAO shared memory whose **payload lives on a GPU**. The ``/tmp`` file keeps
the metadata, counters, timestamps, keywords and semaphores exactly as for a CPU SHM, so opening,
waiting and naming work as usual. Only the data moves to GPU memory.

* ``daoShmOpen``, ``daoShmSetData``, ``daoShmGetData`` and ``daoShmClose`` handle GPU SHMs
  transparently: ``SetData`` copies the host buffer to the GPU, ``GetData`` returns the data in host
  memory.
* CUDA code works on the payload directly through ``image->d_array``. The writer marks the frame
  (``daoShmBeginWrite``), launches its kernel, then publishes it (``daoShmCommit`` or
  ``daoShmCommitSync``): once the kernel has finished, ``cnt0`` is incremented, the frame is
  timestamped and the semaphores are posted.
* Reads through ``daoShmGetData`` / ``get_data`` always return a complete frame, never one a kernel
  is still writing (see `Consistent reads`_).
* Stages can therefore be chained on the GPU (camera → calibration → reconstruction …) with no
  copy through host memory between them.

The payload **survives its creator**: it stays on the GPU after the process that created it exits
or crashes, and new processes can open it. It is freed when the ``/tmp`` file is removed, like a
CPU SHM.

How it works
------------

* The payload is a CUDA virtual-memory allocation (``cuMemCreate``) exported as a Linux file
  descriptor.
* **daoGpuShmd**, a small per-user daemon with no CUDA dependency, holds that descriptor. Holding
  it keeps the GPU memory alive independently of the processes using it. libdao starts the daemon
  automatically the first time a GPU SHM is created.
* ``daoShmOpen`` asks the daemon for the descriptor and maps the allocation into the calling
  process, in the primary CUDA context of the right GPU (found by UUID, so
  ``CUDA_VISIBLE_DEVICES`` does not matter). The CUDA runtime uses the same context, so
  ``d_array`` can be passed to kernels as is.
* The daemon releases a payload when its ``/tmp`` file is removed or recreated.

libdao loads the CUDA driver (``libcuda.so.1``) only when it meets a GPU SHM: it has no link
dependency on CUDA, and machines without a GPU keep using CPU SHMs unchanged. Building with GPU
support only needs the CUDA headers (``waf configure`` finds them from ``nvcc``, ``CUDA_HOME`` or
``/usr/local/cuda``; ``--without-gpu`` disables it).

Host SHMs read by the GPU: keep them in RAM (tmpfs)
---------------------------------------------------

A CPU SHM is a file mapped in memory: processes read and write it in RAM, but if its folder is on a
disk filesystem (ext4, xfs...), the kernel also writes its pages back to the disk every few seconds,
and CUDA cannot pin those pages. The GPU then cannot read or write such an SHM in place (zero copy)
and copies it whole instead, which is slower (for a 560x560 camera frame: about 95 µs for the whole
RAMA chain instead of about 52 µs). ``daoGpuPipeline`` says so when it starts: "*... is not in RAM
(its filesystem is not tmpfs) ...*".

``/dev/shm`` is always in RAM. ``/tmp`` is in RAM on some distributions (Fedora, Arch, Debian 13) and
on disk on others (Ubuntu). Check with ``df -T /tmp``: the type must be ``tmpfs``. Either create the
SHMs in ``/dev/shm``, or mount ``/tmp`` as tmpfs by adding to ``/etc/fstab`` (then reboot)::

   tmpfs  /tmp  tmpfs  defaults,noatime,size=8G,mode=1777  0  0

Everything in ``/tmp`` then lives in RAM (up to ``size``) and is erased at each reboot.

Several processes on one GPU: use MPS
--------------------------------------

By default a GPU **time-slices** between processes: each time the GPU switches from one process's
work to another's, it pays a context switch. A GPU SHM pipeline has one process per stage, all
working on the same GPU for every frame, so without MPS each hand-over between stages costs about
**100 µs**, whatever the frame size.

NVIDIA **MPS** (Multi-Process Service) removes this: the processes' work goes through one shared
server and runs without context switches. MPS works with GPU SHMs (the daoGpuShmd allocations are
shared between MPS clients). **Run MPS for any pipeline where several processes use the same GPU
every frame**, with or without GPU SHMs.

MPS is **not started automatically**: it affects every CUDA program of the user, not only dao.
Start it before the pipeline processes, with the ``daoGpuMps`` helper installed with daoBase:

.. code-block:: bash

   daoGpuMps start      # [--devices 0,1] to restrict the GPUs
   # ... start the pipeline processes: CUDA programs started from now on use MPS ...
   daoGpuMps status     # control daemon, servers and their client processes
   daoGpuMps stop       # once the pipeline processes have exited

Processes already running when MPS starts do not use it. ``daoGpuMps`` uses the standard
``CUDA_MPS_PIPE_DIRECTORY`` (default ``/tmp/nvidia-mps``) and ``CUDA_MPS_LOG_DIRECTORY``.
``daoGpuMps start`` can be called again at any time: when MPS already runs it says so and exits 0.

Starting MPS from a pipeline's start script
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

libdao never starts MPS itself, but a pipeline's start script can, so that nobody has to
remember it. The pattern:

* start it **only when the pipeline uses GPU SHMs** (a CPU-only start leaves the GPUs alone);
* start it **first**: before the process that creates the GPU SHMs (it is a CUDA client too) and
  before every stage, and restart the processes that were already attached to the SHMs (anything
  left running keeps using the GPU without MPS);
* give an opt-out flag, and on failure warn and go on: the pipeline still works, only slower;
* do not stop it in the stop script: other GPU processes (a simulator, a display) may still be
  its clients; ``daoGpuMps stop`` by hand once they have all exited.

.. code-block:: bash

   if [ "$USE_GPU_SHM" = 1 ] && [ "$MPS" = 1 ]; then     # e.g. MPS=0 with --no-mps
       if command -v daoGpuMps >/dev/null; then
           daoGpuMps start || echo "MPS did not start: running without it (~100 us per GPU hand-over)" >&2
       else
           echo "daoGpuMps not on PATH (daoBase): running without MPS" >&2
       fi
   fi
   # ... then create the GPU SHMs and start the stages ...

Check it with ``daoGpuMps status``: the pipeline's processes are listed as the server's clients,
and the *MPS is not running* warning below no longer appears in their logs.

If a process creates or opens a GPU SHM while MPS is not running, libdao logs once:
*NVIDIA MPS is not running: passing GPU SHMs between processes costs about 100 us per frame*.
``DAO_GPU_NO_MPS_WARNING=1`` hides it (e.g. for a single process using a GPU SHM alone).

Things to know about MPS:

* While one user's MPS server runs on a GPU, other users' CUDA programs wait until it stops.
* Programs sharing an MPS server are less isolated: some GPU faults in one can stop the others.
* On a dedicated real-time machine, MPS can run permanently, e.g. as a systemd user service
  (``~/.config/systemd/user/dao-mps.service``, then ``systemctl --user enable --now dao-mps``):

  .. code-block:: ini

     [Unit]
     Description=NVIDIA MPS for dao GPU pipelines

     [Service]
     Type=forking
     ExecStart=/path/to/DAOROOT/bin/daoGpuMps start
     ExecStop=/path/to/DAOROOT/bin/daoGpuMps stop

     [Install]
     WantedBy=default.target

* The GPU compute mode can also be set to exclusive-process, so that only the MPS server uses the
  GPU (``nvidia-smi -c EXCLUSIVE_PROCESS``, needs root).

Measured gain
^^^^^^^^^^^^^

``test/bench_gpu_shm.py``: stage A produces a frame on the GPU; stage B, another process, sums its
rows on the GPU and publishes the result. Time from the start of A's GPU work to B's result, median
of 500 frames, RTX 5060 Ti (PCIe), µs:

.. list-table::
   :header-rows: 1

   * - Frame
     - host SHM, MPS off
     - GPU SHM, MPS off
     - host SHM, MPS on
     - GPU SHM (commit), MPS on
     - GPU SHM (sync), MPS on
   * - 0.25 MB
     - 158
     - 140
     - 74
     - 30
     - 17
   * - 1 MB
     - 314
     - 130
     - 275
     - 34
     - 26
   * - 4 MB
     - 726
     - 144
     - 679
     - 49
     - 33
   * - 16 MB
     - 2472
     - 189
     - 2414
     - 84
     - 67

* The host SHM path copies each frame to the host and back, so it grows with the frame size; the
  GPU SHM path does not copy the frame.
* With MPS and GPU SHMs, a 16 MB frame goes from one process to the next in under 100 µs.
* Without MPS, GPU SHMs still avoid the copies, but every frame pays the context switches.
* For small data (a few kB, e.g. the input and output vectors of a matrix-vector multiply) the
  copies are negligible and GPU SHMs bring nothing: measured with daoMvMGPU, 8192 x 2048 matrix,
  170 µs either way (the time is spent reading the matrix).

Publishing: daoShmCommit or daoShmCommitSync
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``daoShmCommit`` does not block: the frame is published by a CUDA host callback when the stream
reaches it, which lets the writer queue more work. The callback runs on a CUDA driver thread and
adds about 10-15 µs. ``daoShmCommitSync`` waits for the stream and publishes directly: use it in a
loop that waits for its own GPU work anyway (set ``cudaSetDeviceFlags(cudaDeviceScheduleSpin)`` for
the lowest wake-up latency). Both update the host copy first when the SHM is mirrored.

Consistent reads
----------------

A kernel takes time to write a frame, so a reader copying the payload at that moment could get a
**torn** frame: part old, part new. To prevent it:

* the **writer** calls ``daoShmBeginWrite(image)`` before launching the kernel that writes
  ``d_array``. This sets the frame's ``write`` flag; publishing (``daoShmCommit``,
  ``daoShmCommitSync``) clears it and increments ``cnt0``. ``daoShmSetData`` and
  ``daoShmSetDataDevice`` do both on their own.
* ``daoShmGetData`` (and ``get_data`` in Python) and ``daoShmCopyToHost`` wait until no write is in
  progress, copy, and keep the copy only if no write started or finished meanwhile; otherwise they
  copy again. A read therefore waits at most for the write in progress.
* If a writer dies between ``daoShmBeginWrite`` and publishing, reads give up after 1 s with a
  warning and return the current data, rather than hang.

Measured with a deliberately slow writer (4 MB frames, about 6 ms per write): without
``daoShmBeginWrite``, 813 of 1000 reads were torn; with it, none.

Notes:

* A writer that does not call ``daoShmBeginWrite`` is only partly protected (``cnt0`` catches a
  write that is published during the copy, not one still in progress).
* With ``DAO_GPU_MIRROR``, ``daoShmGetData`` returns the host copy directly, like a CPU SHM: the
  usual rule applies (wait on the semaphore, then read).
* A kernel that **reads** a GPU SHM (the next stage of a pipeline) waits on the semaphore, so it
  starts on a complete frame. If the writer can start the next frame before that kernel has
  finished reading, the two overlap, as with CPU SHMs; FIFO GPU SHMs (planned) will remove this.

The host copy (mirror)
----------------------

The ``/tmp`` file still has room for one frame. With the ``DAO_GPU_MIRROR`` flag, every update also
refreshes this host copy (asynchronously, on the same CUDA stream, before the semaphores are
posted). Then:

* CPU readers and existing tools (viewers, daoDAQ, …) read GPU SHMs unchanged, through
  ``image->array``;
* processes without GPU access, or on a build without CUDA, can still read them.

Without the mirror, updates cost no host transfer. ``daoShmGetData`` then copies the frame from the
GPU when called, and a process without GPU access cannot open the SHM. The host copy always uses
``/tmp`` space (RAM) equal to one frame.

C API
-----

.. code-block:: c

   #include <dao.h>

   IMAGE img;
   uint32_t size[2] = {240, 240};

   // create on GPU 0, with the /tmp host copy kept current
   daoShmCreateGpu(&img, "/tmp/pix.im.shm", 2, size, _DATATYPE_FLOAT, 0, 0, DAO_GPU_MIRROR);
   daoShmSetData(&img, hostFrame, 240 * 240);        // host -> GPU, then semaphores

   // in a CUDA program, another process:
   IMAGE in, out;
   daoShmOpen("/tmp/pix.im.shm", &in);
   daoShmOpen("/tmp/calib.im.shm", &out);
   daoShmWaitSem(&in, 1);
   daoShmBeginWrite(&out);                            // readers will not take a torn frame
   calibrate<<<grid, block, 0, stream>>>((float *) in.d_array, (float *) out.d_array);
   daoShmCommit(&out, stream);                        // publish when the kernel is done

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Function
     - Purpose
   * - ``daoShmCreateGpu(image, name, naxis, size, atype, device, NBkw, flags)``
     - Create a GPU SHM on CUDA device ``device``; ``flags``: ``DAO_GPU_MIRROR`` or 0.
   * - ``daoShmBeginWrite(image)``
     - Mark the frame as being written, before launching the kernel that writes ``d_array``.
   * - ``daoShmCommit(image, stream)``
     - Publish data written on the GPU, once the work queued on ``stream`` is done
       (``NULL`` = default stream). Returns at once; keep ``image`` open until then.
   * - ``daoShmCommitSync(image, stream)``
     - Wait for ``stream``, then publish (about 10-15 µs sooner than ``daoShmCommit``).
   * - ``daoShmSetDataDevice(image, d_src, nbVal, stream)``
     - Copy from another device buffer, then commit.
   * - ``daoShmCopyToHost(image, dst, nbVal)``
     - Copy the payload into a private host buffer.
   * - ``daoShmIsGpu(image)`` / ``daoShmGpuAvailable()``
     - Is this a GPU SHM / can this libdao and machine create GPU SHMs.

Streams are passed as ``void *``: a ``cudaStream_t`` or ``CUstream`` works as is.

Python
------

.. code-block:: python

   import numpy as np, daoShm

   s = daoShm.shm("/tmp/pix.im.shm", np.zeros((240, 240), np.float32), gpu=0)   # mirror=True
   s.set_data(frame)            # as for a CPU SHM
   x = s.get_data()             # numpy array

   d = s.get_device_array()     # CuPy array on the payload itself, no copy
   s.begin_write()              # readers will not take a torn frame
   d *= 2                       # computed on the GPU
   s.commit()                   # publish (stream=0: default stream, or a CuPy stream);
                                # s.commit(sync=True) waits for the stream and publishes directly

``s.is_gpu()`` and ``s.device_ptr()`` give the type and the device pointer. Opening an existing
GPU SHM is unchanged: ``daoShm.shm("/tmp/pix.im.shm")``.

The daemon
----------

``daoGpuShmd`` is installed in ``$DAOROOT/bin`` and started on demand (log:
``/tmp/daoGpuShmd-<uid>.log``). One runs per user, on ``/tmp/daoGpuShmd-<uid>.sock`` (or
``$DAO_GPU_SOCKET``).

.. code-block:: bash

   daoGpuShmd --list     # payloads held: id, size, creator pid, SHM
   daoGpuShmd --ping     # is it running

If the daemon is stopped, processes that already have a GPU SHM open keep working, but new
processes cannot open it until it is created again. Stopping the daemon while no process maps a
payload frees that GPU memory.

Limits
------

* Linux, NVIDIA GPUs supporting CUDA virtual memory with POSIX file-descriptor export (current
  desktop and data-centre GPUs; not Jetson/Tegra).
* One machine: GPU SHMs are not shared across hosts.
* Each payload is rounded up to the GPU allocation granularity (usually 2 MiB).
* FIFO GPU SHMs, ``daoShmSetDataPart`` and ``daoShmCombine`` are not supported yet.
* A reader may see a frame while a kernel is still writing it, as with CPU SHMs; wait on the
  semaphores (or ``cnt0``) to read complete frames.
* Several processes using the same GPU need MPS to be fast (see above).
