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
* CUDA code works on the payload directly through ``image->d_array``. A kernel writes into it, then
  ``daoShmCommit`` publishes the frame: once the kernel has finished, ``cnt0`` is incremented, the
  frame is timestamped and the semaphores are posted, without blocking the writer.
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
   calibrate<<<grid, block, 0, stream>>>((float *) in.d_array, (float *) out.d_array);
   daoShmCommit(&out, stream);                        // publish when the kernel is done

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Function
     - Purpose
   * - ``daoShmCreateGpu(image, name, naxis, size, atype, device, NBkw, flags)``
     - Create a GPU SHM on CUDA device ``device``; ``flags``: ``DAO_GPU_MIRROR`` or 0.
   * - ``daoShmCommit(image, stream)``
     - Publish data written on the GPU, once the work queued on ``stream`` is done
       (``NULL`` = default stream). Keep ``image`` open until then.
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
   d *= 2                       # computed on the GPU
   s.commit()                   # publish (stream=0: default stream, or a CuPy stream)

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
