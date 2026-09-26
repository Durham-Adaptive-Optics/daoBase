Network SHMs
============

.. note::

   **New.** Tested on Linux (x86-64). The service and the library compile for macOS
   (Intel, Apple silicon) and Windows, where they have not been run yet. GPU SHMs are
   listed but not shared yet.

Every dao SHM of the network, **opened by its name**, as if it were local.

A small service, ``daoShmNetd``, runs on each machine. The services find each other
and exchange the lists of their SHMs. When a program opens an SHM its machine does not
have, libdao asks the local service, which keeps a **replica with the same name** up to
date: the program uses it as any SHM -- data, counter, semaphores, keywords. Nothing
changes in the programs, in any language.

.. code-block:: python

   import daoShm
   s = daoShm.shm('/tmp/dm1Cmd.im.shm')    # dm1Cmd lives on another machine: works the same
   s.get_data(check=True)                  # wakes at each of its frames

* **Nothing to configure**: the services find each other by UDP multicast (or listed
  peers where multicast is not allowed), within a *domain*.
* **Only what is used travels**: an SHM is streamed only while a process of another
  machine has it open; a replica no process maps any more is dropped.
* **Fast**: the frames are streamed as they are written, straight into the replica; the
  owner always sends the newest (a slow reader skips frames, its latency never grows).
  About **10 µs** from the source's publication to the replica's for small frames in
  low-latency mode, **1.3–3 GB/s** for big ones (see `Performance`_).
* **Writes go back**: a write to a replica is sent to the machine owning the SHM, and
  reaches every replica from there.
* **Freshness**: each replica says where it comes from, how old its frame is, how many
  frames were skipped, and whether its owner is connected.


Quick start
-----------

One machine, one minute (two services as two machines; everything removed after):

.. code-block:: bash

   python examples/shmNet/demo.py

Two machines:

.. code-block:: bash

   # on each machine, once
   daoShmNet.py start

   # on any machine: every SHM of the network
   daoShmNet.py ls
   # MACHINE  SHM       SHAPE    TYPE     STATE
   # rtc1     dm1Cmd    97       float32  remote
   # rtc1     pyrIm     240x240  uint16   remote
   # gui1     lpCmd     1        uint32   local

Then any program on any machine opens any of them by its name. ``examples/shmNet``
has a writer, a reader in Python and in C, and their README.


Using it
--------

Names
^^^^^

A program opens an SHM by its usual name: ``/tmp/dm1Cmd.im.shm`` (Python ``daoShm.shm``,
C ``daoShmOpen``, and every binding built on them). What counts is the SHM's name
(``dm1Cmd``), not its directory.

* **A local SHM always wins**: the service is asked only when the file does not exist here.
* **The same name on several machines**: name the machine, ``rtc1:dm1Cmd`` or
  ``rtc1:/tmp/dm1Cmd.im.shm`` (anywhere a name is accepted); ``daoShmNet.py ls`` shows
  who has what.
* The replica is created in the service's SHM directory (``/tmp``) with the same name,
  so the program's own path works. The first open waits for the replica's first frame
  (a few ms; ``DAO_NET_TIMEOUT_MS``, default 5000).

Reading
^^^^^^^

A replica is a normal SHM: ``get_data(check=True)`` / ``daoShmWaitSem`` wake at each
frame of the source, ``get_counter()`` counts them, ``get_data()`` reads the newest.
Its keywords say how fresh it is (``daoNet.keywords(shm)`` in Python):

.. list-table::
   :header-rows: 1

   * - keyword
     - meaning
   * - ``NET_REPLICA``
     - the machine owning the SHM (the mark of a replica)
   * - ``NET_LINK``
     - 1: connected to its owner; 0: not (the last frame stays)
   * - ``NET_AGE_US``
     - the last frame's age when it arrived: its write on the source to its publication
       here, µs (needs the machines' clocks in sync: NTP, better PTP)
   * - ``NET_DROPPED``
     - frames the owner skipped for this reader (it always sends the newest)
   * - ``NET_SRC_CNT``, ``NET_SRC_TIME``
     - the source's counter and write time (UTC ns) of the last frame

The source's own keywords follow it too.

Writing
^^^^^^^

Writing to a replica writes the SHM on its owner: ``set_data`` / ``daoShmSetData`` on
the replica as usual. The service sends the frame to the owner, which writes the source
(counter, semaphores, as a local write); every replica then gets it. The writer's own
replica does not publish it a second time. One writer per SHM, as always: two machines
writing the same SHM race as two local processes would. ``daoShmNetd --read-only``
refuses writes from other machines.

The service, from Python and the command line
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   daoShmNet.py start [options]     # the service, in the background (options: below)
   daoShmNet.py stop
   daoShmNet.py ls                  # every SHM of the network
   daoShmNet.py status              # peers, replicas (link, frames, skipped, rate, age), streams
   daoShmNet.py want NAME [--keep]  # replicate NAME here now; --keep: even when unused
   daoShmNet.py release NAME        # stop replicating NAME here
   daoShmNet.py ping

.. code-block:: python

   import daoNet
   daoNet.ls()                        # [{'node', 'name', 'shape', 'type', 'state'}, ...]
   daoNet.status()
   s = daoNet.open('rtc1:dm1Cmd')     # a daoShm.shm
   daoNet.keywords(s)                 # {'NET_LINK': 1, 'NET_AGE_US': 42, ...}
   daoNet.want('dm1Cmd', keep=True)
   daoNet.release('dm1Cmd')
   daoNet.start(['--domain', 'myInstrument'])

From C, ``daoNetResolve(name, path, len, timeout_ms)`` (``daoNet.h``) returns the local
path of a remote SHM; ``daoShmOpen`` calls it when the file does not exist.


The service
-----------

.. code-block:: text

   daoShmNetd [options]            (daoShmNet.py start runs it in the background)

.. list-table::
   :header-rows: 1

   * - option
     - default
     - what
   * - ``--name NODE``
     - the hostname
     - this machine's name on the network
   * - ``--domain NAME``
     - ``dao`` (``$DAO_NET_DOMAIN``)
     - only services of one domain see each other: one per instrument
   * - ``--dir DIR``
     - ``/tmp`` (Windows: ``$DAO_SHM_DIR`` or the current directory)
     - where the SHMs are, and the replicas go
   * - ``--peer HOST[:PORT]``
     -
     - a service reached directly, without multicast (repeatable)
   * - ``--no-multicast``
     -
     - no discovery beacons: listed peers only
   * - ``--allow PREFIX``
     - all
     - accept only addresses starting so (``192.168.1.``; repeatable)
   * - ``--iface ADDR`` / ``--bind ADDR``
     - all interfaces
     - the network used for the multicast / for listening
   * - ``--read-only``
     -
     - refuse writes coming from other machines
   * - ``--idle SECONDS``
     - 30
     - drop a replica no process maps for so long (0: never; Linux)
   * - ``--spin``
     -
     - low latency: the streams poll instead of sleeping (see `Performance`_)
   * - ``--port`` / ``--control-port`` / ``--beacon-port``
     - 7710 / 7709 / 7711
     - TCP data, TCP control (127.0.0.1 only), UDP discovery
   * - ``--group ADDR``
     - 239.255.77.10
     - the discovery's multicast group
   * - ``--replica-prefix P``
     -
     - replicas named ``P<name>``: a second service on one machine (tests, demo)
   * - ``-v``
     -
     - verbose

Environment of the programs: ``DAO_NET=0`` (never ask the service),
``DAO_NET_CONTROL`` (its control port), ``DAO_NET_TIMEOUT_MS`` (how long an open waits).

**Firewalls**: TCP 7710 and UDP 7711 open between the machines. The control port is
bound to 127.0.0.1: only local programs reach it.

**At boot**: run it as a service of the user that owns the SHMs, e.g. with systemd
(``~/.config/systemd/user/daoShmNetd.service``, then ``systemctl --user enable --now
daoShmNetd``):

.. code-block:: ini

   [Unit]
   Description=dao network SHMs

   [Service]
   ExecStart=/path/to/DAOROOT/bin/daoShmNetd --domain myInstrument
   Restart=on-failure

   [Install]
   WantedBy=default.target

On macOS, a launchd agent; on Windows, a scheduled task at log-on (or ``daoShmNet.py
start``).


How it works
------------

.. code-block:: text

   owner                                             reader
   writer -> /tmp/X.im.shm                           /tmp/X.im.shm -> any process (unchanged)
                  | semaphore 9 (no polling)                ^ written in place, then published:
                  v                                         | counter + 1, every semaphore posted
            Source: a watcher --- TCP: header + frame + trailer ---> Replica
            Session per reader <--- WRITE: a write on the replica ---

**Discovery.** Every second, each service sends a one-line UDP beacon (its domain, name,
data port and the version of its SHM list) to a multicast group (TTL 1: the local
network). A service seeing a new version fetches that list (TCP, text: name, shape,
type). No data yet. The list is read from the SHM files' headers: listing an SHM never
opens it. Peers not heard of for 6 s are forgotten.

**Opening.** libdao's ``daoShmOpen`` finds no file and asks the local service
(``daoNetResolve``: one line on the control port). Without a service the connection is
refused at once: the open fails as it always did. The service finds the owner, opens a
TCP connection to it (``SUBSCRIBE``), creates the replica from the shape, type and
keywords sent first, and answers once the first frame is in. The first open takes a few
milliseconds.

**Streaming.** On the owner, one watcher per SHM sleeps on its semaphore 9 and wakes the
sessions (one per reader). A session copies the newest frame **consistently** -- the
SHM's counter and write flag checked around the copy, so a frame is never torn -- and
sends it with its header and a trailer in **one system call**. If the writer wrote during
the copy, the session waits for its next frame (no spinning). A slow reader thus
receives the newest frame each time: frames in between are skipped and counted, the
latency does not build up. On the reader, the frame is received **straight into the
replica** (no copy there) with its trailer in one call, then published as a write:
counter + 1, every semaphore posted.

**Writes back.** A second thread of each replica sleeps on its semaphore 9: a counter it
did not leave means a local write, sent up to the owner (``WRITE``), which writes the
source. The owner marks the frame of that write as an *echo* to that reader, which does
not publish it again.

**Links.** Heartbeats both ways every second; no word for 4 s means a broken link:
``NET_LINK`` goes to 0, the replica keeps its last frame, and the service reconnects
(0.25 s to 2 s apart) as long as the replica is wanted.

**Recreated SHMs.** A source recreated (another shape or type, even in place) is noticed
within a second by its creation time and shape; its readers receive the new description
and their replicas are recreated.

**Idle replicas.** A replica no process maps (``/proc/*/maps``) for ``--idle`` seconds is
dropped, and its stream stops. ``want --keep`` replicas stay.

**Semaphore 9.** The service waits on each SHM's semaphore 9 (of the 10 dao creates).
Programs should leave it to it, as a program leaves the other semaphores to the other
programs. An SHM with fewer semaphores is polled every millisecond instead.


Performance
-----------

``test/bench_shm_net.py``: two services on one machine (as two machines, TCP over the
loopback), a Python writer and a Python reader; latency with the writer paced so that
each frame travels alone; *service* = the source's publication to the replica's
(``NET_AGE_US``: copy, TCP, receive, publish); *reader* = to the Python reader woken by
the replica's semaphore; *max rate* = the highest paced rate (doubled from a start) with
fewer than 1 % frames skipped.

Measured on the development machine (Intel Core i5-14600KF, Linux 6.17) **while it ran a real-time AO
pipeline** (busy threads pinned on its cores): each thread wake-up is slow there. For
reference, **local dao on that machine**, a write to a Python reader woken by the
semaphore, takes **136 µs median, 310 µs p99**. On one machine this measures the
software, not a network: across machines add the network's latency (a few tens of µs on
a LAN), and its bandwidth caps the big frames (10 GbE: ~1.2 GB/s).

Default (idle: 0 % CPU; each stream wakes only for its frames):

.. list-table::
   :header-rows: 1

   * - frame
     - service median / p99
     - reader median / p99
     - CPU owner / reader
     - max rate, no skip
     - MB/s
   * - 1 kB
     - 123 / 300 µs
     - 191 / 460 µs
     - 0.4 / 0.4 % @ 200 Hz
     - 12.8 kHz
     - 13
   * - 64 kB
     - 131 / 357 µs
     - 216 / 624 µs
     - 1.2 / 0.4 % @ 200 Hz
     - 3.2 kHz
     - 198
   * - 1 MB
     - 347 / 899 µs
     - 438 / 1009 µs
     - 4.4 / 2.0 % @ 200 Hz
     - 1.6 kHz
     - 1592
   * - 16 MB
     - 3.6 / 8.9 ms
     - 3.7 / 9.1 ms
     - 13.6 / 6.8 % @ 50 Hz
     - 80 Hz
     - 1280

``--spin`` (low latency: an active stream keeps a CPU core busy on each side; idle
services still use none):

.. list-table::
   :header-rows: 1

   * - frame
     - service median / p99
     - reader median / p99
     - max rate, no skip
     - MB/s
   * - 1 kB
     - **8 / 44 µs**
     - 125 / 264 µs
     - 102 kHz
     - 100
   * - 64 kB
     - **34 / 110 µs**
     - 123 / 273 µs
     - 51 kHz
     - 3173
   * - 1 MB
     - 175 / 533 µs
     - 273 / 592 µs
     - 3.2 kHz
     - 3184
   * - 16 MB
     - 3.3 / 6.0 ms
     - 3.4 / 6.1 ms
     - 80 Hz
     - 1280

With ``--spin`` the service's own latency is a few µs for small frames: the reader's
latency is then that of a local SHM on the same machine (its own wake-up). Big frames
are bound by memory copies (one at the owner) and, across machines, by the network.

Run it:

.. code-block:: bash

   PYTHONPATH=src/python LD_LIBRARY_PATH=build/src python test/bench_shm_net.py \
       [--sizes 1k 64k 1M 16M] [--spin] [--json results.json]


Platforms
---------

* **Linux**: tested (``test/test_shm_net.py``, the benchmark, the examples).
* **macOS, Windows**: ``daoShmNetd`` and ``daoNetResolve`` compile there without a
  warning (checked by cross-compilation), through one small portability layer
  (``src/c/daoNetPlatform.h``: sockets, threads, clocks, directories); not run yet.
  Differences: idle replicas are not dropped automatically (``daoShmNet.py release``);
  on Windows the SHMs are in ``$DAO_SHM_DIR`` or the current directory; allow
  ``daoShmNetd`` through the firewall (TCP 7710, UDP 7711).
* Little-endian machines only (x86-64, ARM): the wire format is little-endian.
* Machines can mix: a Linux RTC and a macOS or Windows display share their SHMs.


Limits
------

* **GPU SHMs** are listed (``,gpu``) but not shared yet: their host copy could be.
* **FIFO SHMs** (depth > 1) are shared as their newest segment: the replica is a plain SHM.
* **One writer per SHM**, the rule of dao SHMs across machines too.
* **Trusted network**: no authentication or encryption. Restrict with ``--allow``,
  ``--bind`` / ``--iface`` and the firewall; ``--read-only`` for machines that must not
  write.
* **One replica name per machine**: two services on one machine need
  ``--replica-prefix`` (dao's semaphores are named after the SHM's name).
* ``NET_AGE_US`` compares two machines' clocks: meaningful with NTP (~ms) or PTP (~µs).


Wire protocol
-------------

Version 1, integers little-endian; the structures are in ``include/daoNet.h``.

* **Discovery** (UDP, every second): ``DAONET 1 domain=<d> node=<n> port=<p> catalog=<v>``.
* **Data port** (TCP): the client sends one line.

  * ``CATALOG`` → ``NODE <node> <version> <domain>``, lines ``SHM <name> <naxis> <s0>
    <s1> <s2> <atype> <nbkw> <fifo> <gpu>``, ``END``; closed.
  * ``SUBSCRIBE <name> <node>`` → ``OK`` (or ``ERR <why>``), then binary messages both
    ways, each a 64-byte ``DaoNetHeader`` (magic ``DNET``, version, type, flags,
    dropped, counter, sequence, write time, payload size) and its payload:
    ``META`` (``DaoNetMeta``: name, shape, type; then the keywords, ``DaoNetKeyword``),
    ``FRAME`` (the frame, then a uint64 trailer: the counter after the copy, equal to
    the header's), ``WRITE`` (a frame to write into the source), ``HEARTBEAT``, ``BYE``.

* **Control port** (TCP, 127.0.0.1): ``WANT <name> [ms]``, ``KEEP <name> [ms]`` →
  ``OK <path>`` / ``ERR <why>``; ``RELEASE <name>``; ``LIST``; ``STATUS``; ``PING``;
  ``QUIT``.


Design notes
------------

Why a **replica with the same name**, rather than a network API: every program, tool and
language binding keeps working unchanged, with dao's own semantics (wait on a semaphore,
counter, keywords), and a program does not know nor care where an SHM lives.

Why **TCP**, one connection per stream: reliable, ordered, everywhere, no special
hardware; one connection per SHM so that a big frame never delays a small one behind it.
UDP (tiny latest-value SHMs), multicast (one big frame to many readers) and RDMA (µs,
no CPU on the reader) fit the same design as other data paths behind the same service.

Why **one copy at the owner, none at the reader**: the copy makes each frame consistent
(never torn, even with a writer faster than the network) and frees the SHM at once; the
reader receives in place and publishes, as a local write would.

Why **the newest frame, not every frame**: an AO loop, a display or a controller wants
the current state; queuing frames for a slow reader only adds latency. A recorder
wanting every frame reads on the owner's machine.


Roadmap and status
------------------

.. list-table::
   :header-rows: 1

   * - step
     - status
   * - 0. Design: protocol, replica keywords, discovery, rules
     - done: this page, ``include/daoNet.h``
   * - 1. One stream: consistent copy, one-call send, receive in place, freshness
     - done, measured (`Performance`_)
   * - 2. The service: discovery (multicast, listed peers), the lists, ``daoShmNet.py ls``
     - done
   * - 3. Synchronisation on request, idle replicas dropped, ``status``, ``--keep``
     - done (idle drop: Linux)
   * - 4. Transparency: ``daoShmOpen`` and ``daoShm.shm`` ask the service; ``host:`` names
     - done
   * - 5. Writes from any machine, the echo not published twice
     - done
   * - 6. Robustness: links lost and back, SHMs recreated, timeouts both ways, ``--allow``,
       ``--read-only``
     - done; next: a fault-injection run on two real machines
   * - 7. Real-time tuning: ``--spin``
     - done; next: pinning the stream threads (``daoNuma``), ``SO_BUSY_POLL``, an
       every-frame mode for recorders
   * - 8. Integration: install, docs, examples, tests
     - done (``waf`` builds ``daoShmNetd`` and ``daoShmNet.py``); next: macOS and Windows
       runs, a CI job, ``daoServer`` reading the services' lists
   * - 9. Later: GPU SHMs (host copy), UDP for tiny SHMs, multicast for one big frame to
       many readers, RDMA
     - open

Tests: ``test/test_shm_net.py`` (two services on one machine: discovery, transparent
open, data and semaphores, a fast writer never tearing a frame, writes back without
echo, a recreated source, a link lost and back, idle and kept replicas, fast failure
without a service).

.. code-block:: bash

   PYTHONPATH=src/python LD_LIBRARY_PATH=build/src python -m pytest test/test_shm_net.py
