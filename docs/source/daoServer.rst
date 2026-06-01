Distributed SHM Server (daoServer)
===================================

``daoServer`` is a Python daemon that bridges local shared memory (SHM) segments
with a Redis-backed distributed registry, letting any machine on the network
subscribe to live ZMQ streams of another machine's SHM data.

.. contents:: On this page
   :local:
   :depth: 2

Overview
--------

Without ``daoServer`` each DAO process can only access SHM segments on the same
host.  ``daoServer`` lifts that restriction:

1. Each participating machine runs a ``daoServer`` instance.
2. On start-up the server discovers every ``*.im.shm`` file on the local host
   and publishes its name, shape, and dtype to a shared **Redis** registry.
3. When a client requests a remote SHM segment the server allocates a **ZMQ**
   PUB socket and starts streaming frame updates.  The client creates a local
   mirror SHM segment that is kept up-to-date by a background subscriber thread.
4. Both sides exchange heartbeat keys in Redis.  When a client goes silent its
   mirror subscription is torn down and the ZMQ publisher is stopped if no
   other clients remain.

.. image:: _static/daoServer_architecture.png
   :width: 600px
   :alt: daoServer architecture diagram
   :align: center

.. note::
   The diagram above will render once the file ``docs/source/_static/daoServer_architecture.png``
   is added to the repository.  In the meantime the text below describes the same topology.

Architecture
~~~~~~~~~~~~

.. code-block:: text

    ┌─────────────────────────────────────────────────────────────────────────┐
    │                            Redis Registry                               │
    │                                                                         │
    │   dao:machines:<host>:ip / hostname / role / last_connect               │
    │   dao:machines:<host>:shm:<name>:dimensions / dtype / connections       │
    │   dao:connections:<host>:<shm>  (pending connection request)            │
    │   dao:heartbeats:<host>:<shm>:<client>                                  │
    └──────────────────┬──────────────────────────────┬───────────────────────┘
                       │                              │
              ┌────────┴────────┐            ┌────────┴────────┐
              │   Machine A     │            │   Machine B     │
              │  (daoServer)    │  ZMQ PUB → │  (daoServer)    │
              │                 │ ─────────► │                 │
              │  /tmp/wfs.im.shm│            │ /tmp/remote_    │
              │                 │            │  MachineA_wfs   │
              └─────────────────┘            └─────────────────┘

Prerequisites
-------------

In addition to the standard DAO dependencies the following Python packages are
required:

.. code-block:: bash

   pip install redis pyzmq numpy

The ``redis`` package talks to a running Redis server.  Install the server itself
with your system package manager if it is not already present:

.. code-block:: bash

   # Ubuntu / Debian
   sudo apt install redis

   # RedHat / CentOS / Fedora
   sudo yum install redis

   # macOS
   brew install redis

Redis must be reachable from every machine that participates in the distributed
registry.  A single Redis instance is sufficient for a small cluster; point all
``daoServer`` instances at the same host and port.

Redis Key Schema
----------------

All keys share the ``dao:`` namespace prefix.

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Key pattern
     - Description
   * - ``dao:machines:<host>:ip``
     - IPv4 address of the registered machine
   * - ``dao:machines:<host>:hostname``
     - Hostname string
   * - ``dao:machines:<host>:role``
     - ``master`` or ``minion``
   * - ``dao:machines:<host>:last_connect``
     - ISO-8601 UTC timestamp of the last heartbeat
   * - ``dao:machines:<host>:shm:<name>:dimensions``
     - JSON array of the segment's array shape, e.g. ``[512, 512]``
   * - ``dao:machines:<host>:shm:<name>:dtype``
     - NumPy dtype string, e.g. ``float32``
   * - ``dao:machines:<host>:shm:<name>:connections``
     - JSON array of active client connection records (server IP, port, …)
   * - ``dao:connections:<host>:<shm>``
     - Transient request key written by a client; deleted once the server starts publishing
   * - ``dao:heartbeats:<host>:<shm>:<client>``
     - ISO-8601 UTC timestamp refreshed by the client every 10 s; auto-expires after 75 s

Master / Minion Roles
---------------------

Every cluster needs exactly one **master** node.  The master is responsible for
evicting stale machines (those that have not sent a heartbeat within the
configured TTL).  All other nodes are **minions**.

Role assignment:

* Pass ``--role master`` or ``--role minion`` explicitly on the command line.
* Omit ``--role`` to let ``daoServer`` auto-detect: if no master is found in
  Redis the node promotes itself to master and logs a warning.

Only the master can call ``deregister_machine()`` for a different host or run
``cleanup_stale_machines()``.

Running the Daemon
------------------

Command-line interface
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   python -m daoServer [OPTIONS]

   # or directly
   python daoServer.py [OPTIONS]

Options
^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Flag
     - Default
     - Description
   * - ``--redis-host HOST``
     - ``localhost``
     - Hostname or IP of the Redis server
   * - ``--redis-port PORT``
     - ``6379``
     - Redis TCP port
   * - ``--role {master,minion}``
     - auto
     - Node role; omit to auto-detect
   * - ``--heartbeat-interval SECONDS``
     - ``30``
     - Interval between heartbeat writes to Redis
   * - ``--base-pub-port PORT``
     - ``5560``
     - First ZMQ port allocated for SHM publishing
   * - ``--discover``
     - —
     - Scan local subnets for Redis / daoServer instances and exit

Scanning the network
~~~~~~~~~~~~~~~~~~~~

Before starting the daemon you can check which Redis instances are visible on
your local subnets:

.. code-block:: bash

   python daoServer.py --discover

Example output::

   Found 2 Redis instance(s):
     192.168.64.2:6379 [dao]  machines: daovm, rts-machine
     192.168.64.3:6379        machines: —

A ``[dao]`` tag means the instance already has daoServer keys registered.

Typical multi-machine setup
~~~~~~~~~~~~~~~~~~~~~~~~~~~

On the Redis host (or any machine that should be master):

.. code-block:: bash

   # Start Redis
   redis-server &

   # Start daoServer as master, pointing at local Redis
   python daoServer.py --role master --redis-host localhost

On every other machine:

.. code-block:: bash

   python daoServer.py --role minion --redis-host 192.168.64.2

Python API
----------

``discover_redis`` — subnet scanner
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   from daoServer import discover_redis

   results = discover_redis(port=6379, timeout=0.3, max_workers=64)
   for r in results:
       print(r["host"], r["dao"], r["machines"])

Probes every host on each local ``/24`` subnet.  Returns a list of dicts::

   [
     {"host": "192.168.64.2", "port": 6379, "dao": True,  "machines": ["rts"]},
     {"host": "192.168.64.5", "port": 6379, "dao": False, "machines": []},
   ]

``DaoServer`` class
~~~~~~~~~~~~~~~~~~~~

Constructor
^^^^^^^^^^^

.. code-block:: python

   from daoServer import DaoServer

   server = DaoServer(
       redis_host="192.168.64.2",
       redis_port=6379,
       role="minion",           # or "master", or None for auto-detect
       heartbeat_interval=30.0,
       base_pub_port=5560,
   )

Raises ``ConnectionError`` if Redis is unreachable.

Listing registered machines
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   machines = server.list_machines()
   for m in machines:
       print(m["hostname"], m["role"], m["shm_segments"])

Each entry contains:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Key
     - Description
   * - ``hostname``
     - Machine hostname string
   * - ``ip``
     - IPv4 address
   * - ``role``
     - ``master`` or ``minion``
   * - ``last_connect``
     - ISO-8601 UTC string of the last heartbeat
   * - ``shm_segments``
     - List of SHM segment names registered for this host

Accessing a remote SHM segment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # Get a local shm object that mirrors wfs@rts-machine
   mirror = server.get_remote_shm("rts-machine", "wfs")

   # Read the latest frame
   data = mirror.get_data()

   # When done, release the subscription
   server.release_remote_shm("rts-machine", "wfs")

``get_remote_shm`` blocks until the remote server starts publishing (up to
30 s).  It raises:

* ``KeyError`` — hostname or SHM name not found in Redis.
* ``TimeoutError`` — remote server did not respond within 30 s.
* ``NotImplementedError`` — unsupported transport type in the connection record.

The returned object is a standard ``daoShm.shm`` instance; call ``get_data()``,
``get_data(check=True)`` etc. exactly as with a locally-created segment.

Bidirectional mirroring
^^^^^^^^^^^^^^^^^^^^^^^

When ``bidirectional=True`` both ends run a publisher **and** a subscriber so
writes to the local mirror propagate back to the remote SHM:

.. code-block:: python

   mirror = server.get_remote_shm("rts-machine", "dm_commands", bidirectional=True)

   # Write a new DM command vector — it will appear in the remote SHM
   import numpy as np
   mirror.set_data(np.zeros(140, dtype=np.float32))

.. warning::

   Bidirectional mode doubles the ZMQ traffic.  Both machines must be able to
   reach each other's ``base_pub_port`` range.  Use unidirectional mode (the
   default) wherever write-back is not needed.

Lifecycle
^^^^^^^^^

Use ``start()`` / ``stop()`` when embedding ``DaoServer`` in your own daemon:

.. code-block:: python

   import threading, signal
   from daoServer import DaoServer

   server = DaoServer(redis_host="192.168.64.2", role="minion")

   def _stop(sig, frame):
       server.stop()

   signal.signal(signal.SIGINT, _stop)
   signal.signal(signal.SIGTERM, _stop)

   # start() blocks until stop() is called
   threading.Thread(target=server.start, daemon=True).start()

   # ... do your own work here ...

``start()`` spawns the following background daemon threads:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Thread name
     - Purpose
   * - ``dao-heartbeat``
     - Refreshes ``last_connect`` in Redis every ``heartbeat_interval`` seconds
   * - ``dao-shm-sync``
     - Syncs local ``*.im.shm`` files with the Redis registry at 1 Hz
   * - ``dao-conn-monitor``
     - Watches for incoming connection requests and starts ZMQ publishers
   * - ``dao-client-hb-monitor``
     - Evicts stale clients and stops publishers with no active subscribers
   * - ``dao-stale-cleanup`` *(master only)*
     - Deregisters machines that have not heartbeated within the TTL (default 120 s)

Manual registration / deregistration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These are called automatically by ``start()`` / ``stop()``.  They can also be
used directly for scripting or testing:

.. code-block:: python

   server.register_machine()      # write local SHM metadata to Redis
   server.deregister_machine()    # remove all Redis keys for this host

   # Master may deregister another host
   server.deregister_machine("stale-rts-machine")

Network discovery via class method
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``discover()`` is a static method so it can be called without constructing a
``DaoServer`` first:

.. code-block:: python

   results = DaoServer.discover()
   if results:
       server = DaoServer(redis_host=results[0]["host"])

End-to-End Examples
-------------------

Unidirectional: read a wavefront sensor on a remote machine
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Machine A (WFS host — ``192.168.64.2``):

.. code-block:: bash

   python daoServer.py --role master --redis-host localhost

Machine B (consumer):

.. code-block:: python

   from daoServer import DaoServer

   server = DaoServer(redis_host="192.168.64.2", role="minion")

   # Start background threads (non-blocking — run in a thread)
   import threading
   t = threading.Thread(target=server.start, daemon=True)
   t.start()

   # Mirror the remote WFS shared memory
   wfs_mirror = server.get_remote_shm("machineA", "wfs")

   import time
   for _ in range(100):
       frame = wfs_mirror.get_data(check=True)   # blocks until new frame
       print("frame shape:", frame.shape, "  mean:", frame.mean())
       time.sleep(0)

   server.release_remote_shm("machineA", "wfs")
   server.stop()

Bidirectional: write DM commands back to the RTS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Machine A (RTS — runs the DM loop):

.. code-block:: bash

   python daoServer.py --role master --redis-host localhost

Machine B (external controller):

.. code-block:: python

   from daoServer import DaoServer
   import numpy as np, threading

   server = DaoServer(redis_host="192.168.64.2", role="minion")
   threading.Thread(target=server.start, daemon=True).start()

   dm = server.get_remote_shm("rts-machine", "dm_commands", bidirectional=True)

   # Push a new command vector — it is immediately visible on the RTS
   new_cmd = np.ones(140, dtype=np.float32) * 0.01
   dm.set_data(new_cmd)

   server.release_remote_shm("rts-machine", "dm_commands")
   server.stop()

Troubleshooting
---------------

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Symptom
     - Resolution
   * - ``ConnectionError: Cannot connect to Redis``
     - Ensure ``redis-server`` is running and reachable on the specified host/port.  Check firewall rules on port 6379.
   * - ``KeyError: No machine '<host>' registered``
     - The remote ``daoServer`` has not started or has not completed ``register_machine()``.  Wait a few seconds and retry.
   * - ``TimeoutError`` from ``get_remote_shm``
     - The remote server received the connection request but could not open the SHM file.  Check that the segment exists in ``/tmp/`` on the remote host.
   * - Mirror receives zeros indefinitely
     - The remote SHM exists but nothing is writing to it.  The initial-publish nudge fires after 150 ms; subsequent updates only arrive when the remote SHM counter increments.
   * - Two servers both claim ``master``
     - Restart both and pass ``--role`` explicitly.  Only one master should exist per Redis instance.
   * - ZMQ ports unreachable between machines
     - Open the port range ``base_pub_port`` – ``base_pub_port + N`` in the firewall on the **server** machine (default 5560+).
