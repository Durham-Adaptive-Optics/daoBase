FIFO Shared Memory
==================

.. warning::

   **Experimental Feature** — The FIFO shared memory mode is experimental and is **not enabled by default**.
   All shared memory segments default to a FIFO depth of 1 (standard single-frame behaviour).

   When FIFO mode is enabled, the **new C, C++, and Python FIFO APIs must be used** to read and write
   data.  Using legacy API calls (e.g., accessing ``image->array.V`` directly, or calling the old
   ``get_data()`` / ``get_frame()`` without understanding FIFO semantics) will produce **unexpected
   behaviour** because the underlying memory layout has changed.

   **Windows is not currently supported.** The FIFO extension has only been tested on Linux and
   macOS.  The Windows code paths (file-mapping objects, Windows semaphores) have not been
   exercised with FIFO depth > 1 and may fail or produce incorrect results.

Overview
--------

The standard DAO shared memory model stores exactly one frame per shared memory segment.  The FIFO
extension adds a circular buffer of *N* independent frames (segments) inside a single shared memory
object.  Each segment has its own copy of ``IMAGE_METADATA``, so timestamps, counters, and the
``write`` flag are tracked independently.

A **writer** advances a shared ``fifo_last_written`` index every time a new frame is committed.
Each **reader** keeps its own private read-tail (``fifo_last_read`` / ``fifo_last_read_cnt0``) that
lives in the local ``IMAGE`` struct and is therefore never visible to other processes.  Readers can:

* consume frames one-by-one in order (FIFO / sequential mode),
* always jump to the newest available frame (latest-only / "newest" mode),
* read arbitrary historical segments by index.

.. note::

   Because the read-tail is local to each ``IMAGE`` struct, multiple independent readers can each
   maintain their own position without interfering with each other or with the writer.

Memory Layout
-------------

A FIFO SHM object allocates contiguous memory for all *N* segments in one go:

.. code-block:: text

   [ IMAGE_METADATA[0] | IMAGE_METADATA[1] | ... | IMAGE_METADATA[N-1] ]
   [ data segment [0]  | data segment [1]  | ... | data segment [N-1]  ]

* ``IMAGE_METADATA[0].fifo_size`` stores the depth *N* and is the authoritative value for all
  processes.
* ``IMAGE_METADATA[0].fifo_last_written`` is the index of the segment most recently committed by
  the writer.
* Each ``IMAGE_METADATA[i]`` holds the counters and flags for segment *i*.

For a depth-1 FIFO the layout is identical to the legacy format, so existing code that was compiled
against the new headers will continue to work as before — **as long as it uses the FIFO-aware API
functions**.

Return Codes
------------

The FIFO API introduces two new return codes in addition to the existing ``DAO_SUCCESS``,
``DAO_ERROR``, and ``DAO_TIMEOUT``:

.. list-table::
   :widths: 20 10 70
   :header-rows: 1

   * - Constant
     - Value
     - Meaning
   * - ``DAO_OVERWRITE``
     - ``-2``
     - The reader's tail was lapped by the writer.  The tail has been reset to the newest segment.
       The data pointer returned is still valid but some frames were skipped.
   * - ``DAO_NOTREADY``
     - ``-3``
     - No new segment is available yet (the reader is already at the head of the FIFO).

Creating a FIFO Shared Memory Segment
--------------------------------------

C API
~~~~~

Use ``daoShmImageCreate_FIFO`` instead of ``daoShmImageCreate``:

.. code-block:: c

   #include "dao.h"

   IMAGE image;
   uint32_t size[2] = {256, 256};
   uint32_t fifo_depth = 8;   // keep the last 8 frames

   // Create a FIFO shared memory segment with depth 8
   daoShmImageCreate_FIFO(&image, "/tmp/mydata.im.shm",
                          2,          // naxis
                          size,
                          _DATATYPE_FLOAT,
                          1,          // shared = 1 (POSIX shared memory)
                          0,          // no keywords
                          fifo_depth);

.. note::

   ``daoShmImageCreate`` is a thin wrapper around ``daoShmImageCreate_FIFO`` with ``fifo_size = 1``
   and continues to work as before.

C++ API
~~~~~~~

Pass the optional ``depth`` argument to the ``Dao::Shm`` constructor:

.. code-block:: cpp

   #include "daoShm.hpp"

   // 256×256 float32 SHM with 8-frame FIFO depth
   Dao::Shm<float> shm("/tmp/mydata.im.shm", {256, 256}, nullptr, /*depth=*/8);

Python API
~~~~~~~~~~

Pass the ``depth`` keyword argument when creating a ``shm`` object:

.. code-block:: python

   import numpy as np
   from daoShm import shm

   data = np.zeros((256, 256), dtype=np.float32)

   # Create SHM with FIFO depth of 8 frames
   shm_obj = shm("/tmp/mydata.im.shm", data=data, depth=8)

Writing Data
------------

All existing write functions (``daoShmImage2Shm``, ``set_frame()``, ``set_data()`` etc.) have been
updated to advance the FIFO automatically — no API change is required on the writer side.  Each
call internally increments ``fifo_last_written`` and writes into the next circular slot.

Reading Data
------------

C API
~~~~~

.. list-table::
   :widths: 35 65
   :header-rows: 1

   * - Function
     - Description
   * - ``daoShmGetNewestSegment``
     - Returns the most recently written segment.  Equivalent to the old ``image->array.V`` access pattern.
   * - ``daoShmGetNextSegment``
     - Advances the reader's tail by one and returns the next unread segment.  Returns ``DAO_OVERWRITE`` if frames were skipped, ``DAO_NOTREADY`` if no new frame is available.
   * - ``daoShmWaitForNextSegment``
     - Blocks (using the counter semaphore) until a new segment is written, then returns.  Call ``daoShmGetNextSegment`` immediately after.
   * - ``daoShmGetArbitrarySegment``
     - Returns a pointer to a specific FIFO slot by index (wraps modulo ``fifo_size``).
   * - ``daoShmCheckSegmentOverwrite``
     - Checks whether the segment last read by this tail has since been overwritten.  Returns ``DAO_OVERWRITE`` if so.
   * - ``daoShmResetTail``
     - Resets the reader's tail to the newest segment.  Useful after an overwrite or on initialisation.

**Sequential reading example (C):**

.. code-block:: c

   #include "dao.h"

   IMAGE image;
   daoShmShm2Img("/tmp/mydata.im.shm", &image);  // open existing SHM

   void   *segment_ptr  = NULL;
   uint32_t segment_idx = 0;
   uint64_t segment_cnt0 = 0;

   while (1) {
       // Block until the next frame arrives
       daoShmWaitForNextSegment(&image);

       int_fast8_t status = daoShmGetNextSegment(&image, &segment_ptr,
                                                 &segment_idx, &segment_cnt0);

       if (status == DAO_OVERWRITE) {
           fprintf(stderr, "Warning: frame(s) were overwritten – reader is too slow\n");
       }

       float *data = (float *)segment_ptr;
       // process data …
   }

**Latest-only reading example (C):**

.. code-block:: c

   daoShmGetNewestSegment(&image, &segment_ptr, &segment_idx, &segment_cnt0);
   float *data = (float *)segment_ptr;

C++ API
~~~~~~~

.. list-table::
   :widths: 35 65
   :header-rows: 1

   * - Method
     - Description
   * - ``get_frame()``
     - Returns the **newest** segment.  Existing synchronisation options (semaphore, counter spin) still work.
   * - ``get_next_frame(wait, status)``
     - Advances the tail and returns the next unread segment.  ``status`` is set to ``DAO_OVERWRITE`` if frames were skipped.
   * - ``get_next_frame(wait, status, cnt0)``
     - As above but also fills ``cnt0`` with the segment counter value.
   * - ``get_arbitrary_frame(segment_idx)``
     - Returns a pointer to an arbitrary FIFO slot.
   * - ``check_segment_overwrite()``
     - Returns ``DAO_OVERWRITE`` if the last-read segment has been overwritten.
   * - ``get_counter(fifo_idx)``
     - Returns ``cnt0`` for a specific FIFO slot.
   * - ``get_meta_data(fifo_idx)``
     - Returns a pointer to ``IMAGE_METADATA`` for a specific FIFO slot.

**Sequential reading example (C++):**

.. code-block:: cpp

   #include "daoShm.hpp"

   Dao::Shm<float> shm("/tmp/mydata.im.shm");  // open existing SHM

   while (true) {
       int_fast8_t status;
       float *data = shm.get_next_frame(/*wait=*/true, status);

       if (status == DAO_OVERWRITE) {
           std::cerr << "Warning: frame(s) overwritten – reader is too slow\n";
       }
       // process data …
   }

Python API
~~~~~~~~~~

.. list-table::
   :widths: 35 65
   :header-rows: 1

   * - Method
     - Description
   * - ``get_data()``
     - Returns the **newest** segment as a NumPy array.
   * - ``get_data_next(wait, reform)``
     - Advances the tail and returns the next unread segment.  Returns ``(status, array)`` where *status* is one of the class constants (``DAO_SUCCESS``, ``DAO_OVERWRITE``, ``DAO_NOTREADY``).
   * - ``get_data_arbitrary(index)``
     - Returns the segment at the given FIFO index as a NumPy array.
   * - ``get_history(num_items)``
     - Returns the last *num_items* segments stacked along axis 0 as a single NumPy array.
   * - ``get_meta_data(index)``
     - Returns metadata for the newest segment (``index=None``) or for a specific FIFO slot.

**Sequential reading example (Python):**

.. code-block:: python

   from daoShm import shm

   shm_obj = shm("/tmp/mydata.im.shm")  # open existing SHM

   while True:
       status, data = shm_obj.get_data_next(wait=True)

       if status == shm.DAO_OVERWRITE:
           print("Warning: frame(s) overwritten – reader is too slow")
       elif status == shm.DAO_NOTREADY:
           continue  # no new frame yet

       # process data (NumPy array) …

**Reading recent history (Python):**

.. code-block:: python

   # Retrieve the last 4 frames as a (4, 256, 256) NumPy array
   history = shm_obj.get_history(num_items=4)

Overwrite Detection
-------------------

Because the FIFO is a circular buffer, a slow reader can be lapped by a fast writer.  There are two
mechanisms to detect this:

1. **Inline with** ``get_data_next`` / ``get_next_frame`` / ``daoShmGetNextSegment``:
   The return value is ``DAO_OVERWRITE`` whenever the tail was behind the writer by more than one
   full FIFO cycle.  The tail is automatically reset to the newest segment.

2. **Explicit check** via ``check_segment_overwrite`` / ``daoShmCheckSegmentOverwrite``:
   Checks whether the ``write`` flag or ``cnt0`` of the last-read slot has changed since the read.
   Useful if you copy data out of shared memory and then want to validate the copy.

To avoid overwrite events, increase the FIFO depth or reduce the time between reader calls.

FIFO Depth Considerations
--------------------------

The primary motivation for using a FIFO depth greater than 1 is **retaining a history of frames**
in shared memory.  Two typical use-cases drive the choice of depth:

**Telemetry and diagnostics**
   A telemetry or logging process can read back the last *N* frames at any time using
   ``get_history()`` / ``daoShmGetArbitrarySegment``.  This gives a rolling window of recent
   data without requiring a separate ring-buffer or file-write on every cycle.  The depth should
   cover at least the longest expected gap between telemetry reads.

**Algorithms that depend on previous results**
   Many adaptive optics algorithms (predictive controllers, integrators, temporal filters) need
   access to one or more past frames at each iteration.  Rather than maintaining separate history
   buffers inside the component, the FIFO provides this history directly in shared memory,
   accessible to any process that holds an ``IMAGE`` handle.  Choose a depth equal to the number
   of past frames the algorithm requires, plus a margin for scheduling jitter.

The FIFO depth multiplies the shared memory allocation:

.. code-block:: text

   Total size ≈ N × (sizeof(IMAGE_METADATA) + nelement × sizeof(element_type))
               + semaphores + keywords

Typical guidance:

* Depth **1** — identical to standard (non-FIFO) behaviour.  No overhead.
* Depth **2–8** — sufficient for algorithms requiring a small number of past frames (e.g.,
  a first- or second-order temporal filter).
* Depth **10–100** — suitable for telemetry windows, longer predictive filters, or components
  that process data in bursts rather than frame-by-frame.
* Depth **> 100** — feasible for large telemetry histories; bear in mind that the entire buffer
  lives in RAM.  For very long histories consider whether a dedicated file-based ring buffer is
  more appropriate.

Thread Safety
-------------

* The **writer** side (``daoShmImage2Shm`` and equivalents) is safe to call from a single writer
  thread.  Concurrent writers are not supported.
* Multiple **readers** are safe because each reader's tail is stored in its own local ``IMAGE``
  struct.  Readers never modify shared memory.
* The ``write`` flag in each segment's ``IMAGE_METADATA`` provides a lightweight consistency
  indicator but is **not** a full memory barrier.  Use ``daoShmCheckSegmentOverwrite`` if strict
  data integrity is required.

Backwards Compatibility
------------------------

All existing code that creates SHM with ``daoShmImageCreate`` (depth 1) will continue to compile
and run correctly — the function now delegates to ``daoShmImageCreate_FIFO`` with ``fifo_size = 1``.

Code that **loads** an existing SHM (``daoShmShm2Img``) reads ``fifo_size`` from the metadata and
adjusts all internal offsets automatically, so readers compiled with the new headers can open both
depth-1 and depth-N segments transparently.

.. warning::

   SHM segments created with the **old** library (before the FIFO branch was merged) have
   ``fifo_size = 0`` in memory, which the new loader interprets incorrectly.  Delete and recreate
   any existing shared memory files after upgrading to the FIFO-enabled build.
