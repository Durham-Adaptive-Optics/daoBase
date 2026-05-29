#!/usr/bin/env python3
"""daoServer — distributed SHM registry daemon.

Bridges local shared memory (SHM) resources — discovered under /tmp/*.im.shm on
Linux/macOS, or the local working directory on Windows —
with a Redis-backed distributed registry.  Remote machines can request a live
ZMQ stream of any published segment; the server starts publishing on demand and
registers the connection port in Redis so the client can subscribe.
"""

import glob
import ipaddress
import json
import logging
import os
import platform
import re
import socket
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from typing import Any, Callable

import redis
import numpy as np

# ---------------------------------------------------------------------------
# daoShm import — resolve from the sibling daoBase repo
# ---------------------------------------------------------------------------
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "daoBase", "src", "python"))
from daoShm import shm, daoType2NpType  # noqa: E402

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Platform-aware SHM directory
# ---------------------------------------------------------------------------
# On Linux/macOS, shared memory files live under /tmp.
# On Windows, daoShm creates them in the current working directory.
SHM_DIR: str = "." if platform.system() == "Windows" else "/tmp"

# ---------------------------------------------------------------------------
# Key helpers
# ---------------------------------------------------------------------------

def _key(*parts: str) -> str:
    """Build a namespaced Redis key.

    Args:
        *parts: Key components joined with ':' after the 'dao:' prefix.

    Returns:
        A colon-separated Redis key string prefixed with 'dao:'.
    """
    return "dao:" + ":".join(parts)


def _sanitise_shm_name(name: str) -> str:
    """Sanitise a SHM name for safe use as a Redis key component.

    Args:
        name: Raw SHM name, potentially with leading slashes or spaces.

    Returns:
        Sanitised string with leading slashes stripped and spaces replaced
        with underscores.
    """
    return name.lstrip("/").replace(" ", "_")


# ---------------------------------------------------------------------------
# SHM discovery
# ---------------------------------------------------------------------------

def _discover_shm_segments() -> list[dict]:
    """Discover local SHM segments (*.im.shm) in SHM_DIR.

    Opens each segment via daoShm to read its metadata (dimensions, dtype).

    Returns:
        A list of dicts, each containing 'name' (str), 'dimensions' (list[int]),
        and 'dtype' (numpy dtype string).
    """
    segments: list[dict] = []
    for path in glob.glob(os.path.join(SHM_DIR, "*.im.shm")):
        name = os.path.basename(path)[: -len(".im.shm")]
        if name.startswith("remote_"):
            continue  # skip local mirrors of remote segments
        try:
            s = shm(path)
            md = s.get_meta_data()
            # size is a fixed-length array [x, y, z]; remove unused trailing zeros
            size = [int(x) for x in md["size"] if int(x) > 0]
            np_dtype = daoType2NpType(md["atype"])
            dtype_str = str(np.dtype(np_dtype)) if np_dtype is not None else "float32"
            segments.append({"name": name, "dimensions": size, "dtype": dtype_str})
        except Exception as exc:  # noqa: BLE001
            logger.warning("Could not read metadata for SHM '%s': %s", name, exc)
    return segments


# ---------------------------------------------------------------------------
# ZMQ connector (the only built-in transport)
# ---------------------------------------------------------------------------

def _connect_zmq(
    server_ip: str,
    port: int,
    mirror_name: str,
    dimensions: list[int],
    dtype_str: str,
    pub_port: int | None = None,
) -> shm:
    """Create a live-mirroring local shm object backed by a remote ZMQ publisher.

    When *pub_port* is supplied the mirror also acts as a ZMQ publisher so the
    remote server can subscribe back, making the link fully bidirectional.

    Args:
        server_ip: IP address of the remote machine.
        port: ZMQ PUB port on the remote machine.
        mirror_name: Name for the local mirror SHM segment.
        dimensions: Array shape for the local mirror.
        dtype_str: Numpy dtype string for the local mirror.
        pub_port: Optional local ZMQ PUB port for bidirectional mode.

    Returns:
        A started local shm object subscribed to the remote publisher and,
        when pub_port is given, also publishing back on that port.
    """
    np_dtype = np.dtype(dtype_str)
    initial_data = np.zeros(dimensions, dtype=np_dtype)
    local = shm(os.path.join(SHM_DIR, f"{mirror_name}.im.shm"), data=initial_data, subHost=server_ip, subPort=port)
    local.subEnable = True
    local.subThread.start()
    if pub_port is not None:
        local.pubPort = pub_port
        local.pubEnable = True
        local.pubThread.start()
        logger.info(
            "Bidirectional: mirror '%s' publishing back on ZMQ port %d.",
            mirror_name, pub_port,
        )
    return local


# Connector registry — add future transports here (e.g. "rdma")
_CONNECTORS: dict[str, Callable] = {
    "zmq": _connect_zmq,
}


# ---------------------------------------------------------------------------
# Redis discovery
# ---------------------------------------------------------------------------

def _get_local_ips() -> list[str]:
    """Return all non-loopback IPv4 addresses attached to local interfaces.

    Uses three complementary methods so that secondary interfaces (e.g. a
    host-only VM adapter on a different subnet) are not missed:

    1. ``socket.gethostbyname_ex`` — hostname-bound IPs.
    2. UDP-connect trick — primary outbound interface.
    3. ``ifconfig`` (macOS) / ``ip addr`` (Linux) — all interface IPs.

    Returns:
        Deduplicated list of non-loopback IPv4 address strings.
    """
    ips: set[str] = set()

    # Method 1: hostname resolution
    try:
        _, _, addresses = socket.gethostbyname_ex(socket.gethostname())
        for addr in addresses:
            if not addr.startswith("127."):
                ips.add(addr)
    except OSError:
        pass

    # Method 2: UDP-connect trick (primary outbound interface, no packets sent)
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as _s:
            _s.connect(("8.8.8.8", 80))
            addr = _s.getsockname()[0]
        if not addr.startswith("127."):
            ips.add(addr)
    except OSError:
        pass

    # Method 3: parse OS interface table to catch ALL adapters
    try:
        if platform.system() == "Darwin":
            out = subprocess.check_output(["ifconfig"], text=True, timeout=3)
            for line in out.splitlines():
                line = line.strip()
                if line.startswith("inet ") and not line.startswith("inet6"):
                    parts = line.split()
                    if len(parts) >= 2 and not parts[1].startswith("127."):
                        ips.add(parts[1])
        else:
            out = subprocess.check_output(
                ["ip", "-4", "addr", "show"], text=True, timeout=3
            )
            for match in re.finditer(r"inet\s+(\d+\.\d+\.\d+\.\d+)", out):
                addr = match.group(1)
                if not addr.startswith("127."):
                    ips.add(addr)
    except Exception:  # noqa: BLE001
        pass

    return list(ips)


def discover_redis(
    port: int = 6379,
    timeout: float = 0.3,
    max_workers: int = 64,
) -> list[dict]:
    """Probe every host on each local /24 subnet for a listening Redis server.

    Only subnets attached to non-loopback IPv4 interfaces are scanned.  For
    each reachable host the function attempts a Redis ``PING`` to confirm it is
    actually a Redis instance (not just an open TCP port), and — if it is —
    checks whether the ``dao:machines`` namespace is present to identify it as
    a daoServer Redis.

    The scan is fully parallel so even a /24 (254 hosts) completes in roughly
    ``timeout`` seconds wall-clock time.

    Args:
        port: TCP port to probe on each host (default ``6379``).
        timeout: Per-host TCP / Redis connect timeout in seconds.
        max_workers: Maximum number of concurrent probe threads.

    Returns:
        A list of dicts, one per discovered Redis instance::

            {
                "host":     "192.168.64.3",
                "port":     6379,
                "dao":      True,          # False if Redis but no dao keys
                "machines": ["daovm", …], # hostnames found in dao namespace
            }
    """
    # Collect unique local /24 subnets from ALL non-loopback interfaces.
    local_subnets: set[ipaddress.IPv4Network] = set()
    for ip in _get_local_ips():
        net = ipaddress.IPv4Network(f"{ip}/24", strict=False)
        local_subnets.add(net)

    if not local_subnets:
        logger.warning("discover_redis: no non-loopback subnets found.")
        return []

    # Build the full list of candidate hosts (skip network/broadcast).
    candidates: list[str] = []
    for net in local_subnets:
        for host in net.hosts():
            candidates.append(str(host))

    results: list[dict] = []
    lock = threading.Lock()

    def _probe(host: str) -> None:
        # Quick TCP reachability check first (cheaper than a Redis connect).
        try:
            with socket.create_connection((host, port), timeout=timeout):
                pass
        except OSError:
            return

        # TCP port is open — confirm it's Redis.
        try:
            r = redis.Redis(
                host=host, port=port,
                socket_connect_timeout=timeout,
                socket_timeout=timeout,
                decode_responses=True,
            )
            r.ping()
        except (redis.RedisError, OSError):
            return

        # It's Redis — check for dao namespace.
        try:
            cursor, keys = r.scan(0, match="dao:machines:*:hostname", count=10)
            machines = [k.split(":")[2] for k in keys if len(k.split(":")) >= 4]
            is_dao = bool(machines) or cursor != 0
            entry = {
                "host": host,
                "port": port,
                "dao": is_dao,
                "machines": machines,
            }
        except redis.RedisError:
            entry = {"host": host, "port": port, "dao": False, "machines": []}
        finally:
            try:
                r.close()
            except Exception:  # noqa: BLE001
                pass

        logger.info(
            "discover_redis: found Redis at %s:%d%s",
            host, port,
            " (dao)" if entry.get("dao") else "",
        )
        with lock:
            results.append(entry)

    logger.info(
        "discover_redis: scanning %d hosts across %d subnet(s) on port %d…",
        len(candidates), len(local_subnets), port,
    )
    with ThreadPoolExecutor(max_workers=max_workers) as pool:
        futures = [pool.submit(_probe, h) for h in candidates]
        for _ in as_completed(futures):
            pass  # exceptions are swallowed inside _probe

    results.sort(key=lambda x: ipaddress.IPv4Address(x["host"]))
    logger.info("discover_redis: found %d Redis instance(s).", len(results))
    return results

# Client heartbeat configuration
_CLIENT_HEARTBEAT_INTERVAL: float = 10.0  # seconds between client heartbeat writes
_CLIENT_HEARTBEAT_TTL: float = 25.0       # seconds before a silent client is evicted


# ---------------------------------------------------------------------------
# DaoServer
# ---------------------------------------------------------------------------

class DaoServer:
    """Daemon that registers local SHM segments in Redis and serves them remotely.

    Args:
        redis_host: Redis server hostname or IP.
        redis_port: Redis server port.
        role: 'master' or 'minion'. Pass None to auto-detect.
        heartbeat_interval: Seconds between last_connect refreshes.
        base_pub_port: First ZMQ port to allocate for SHM publishing.

    Raises:
        ConnectionError: If Redis is unreachable at construction time.
    """

    def __init__(
        self,
        redis_host: str = "localhost",
        redis_port: int = 6379,
        role: str | None = None,
        heartbeat_interval: float = 30.0,
        base_pub_port: int = 5560,
    ) -> None:
        self._redis_host = redis_host
        self._redis_port = redis_port
        self._heartbeat_interval = heartbeat_interval
        self._base_pub_port = base_pub_port

        self._hostname: str = socket.gethostname()
        self._ip: str = self._get_local_ip()

        # Redis connection
        try:
            self._redis: redis.Redis = redis.Redis(
                host=redis_host, port=redis_port, decode_responses=True
            )
            self._redis.ping()
        except redis.RedisError as exc:
            raise ConnectionError(
                f"Cannot connect to Redis at {redis_host}:{redis_port}: {exc}"
            ) from exc

        # Role resolution (may log a warning)
        self._role: str = self._resolve_role(role)

        # Thread coordination
        self._stop_event = threading.Event()
        self._publisher_lock = threading.Lock()

        # shm_name → active shm publisher object
        self._active_publishers: dict[str, shm] = {}
        # shm_name → ZMQ publish port
        self._publisher_ports: dict[str, int] = {}
        # Ports currently in use for publishing
        self._used_ports: set[int] = set()
        # (server_hostname, shm_name) → Event to cancel client heartbeat thread
        self._client_heartbeat_events: dict[tuple[str, str], threading.Event] = {}
        # shm_name → client pub port the server has subscribed to (bidirectional)
        self._bidirectional_sub_ports: dict[str, int] = {}
        # (server_hostname, shm_name) → client-side mirror pub port
        self._mirror_pub_ports: dict[tuple[str, str], int] = {}
        # (server_hostname, shm_name) → active mirror shm object (for cleanup)
        self._active_mirrors: dict[tuple[str, str], shm] = {}

    # ------------------------------------------------------------------
    # Network helpers
    # ------------------------------------------------------------------

    @staticmethod
    def discover(
        port: int = 6379,
        timeout: float = 0.3,
        max_workers: int = 64,
    ) -> list[dict]:
        """Scan local subnets for Redis instances running daoServer.

        Delegates to the module-level :func:`discover_redis` function.  Can be
        called on the class without constructing a :class:`DaoServer` first::

            results = DaoServer.discover()
            if results:
                server = DaoServer(redis_host=results[0]["host"])

        Args:
            port: TCP port to probe (default ``6379``).
            timeout: Per-host connect timeout in seconds.
            max_workers: Maximum parallel probe threads.

        Returns:
            See :func:`discover_redis`.
        """
        return discover_redis(port=port, timeout=timeout, max_workers=max_workers)

    @staticmethod
    def _get_local_ip() -> str:
        """Return the first non-loopback IPv4 address for this machine.

        Uses a UDP connect trick (no packets sent) to ask the OS which source
        address it would use to reach an external host, then falls back to
        enumerating interface addresses if that fails.

        Returns:
            A routable IPv4 address string, or '127.0.0.1' as a last resort.
        """
        # Preferred: ask the OS which source IP it routes external traffic from.
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
                s.connect(("8.8.8.8", 80))
                return s.getsockname()[0]
        except OSError:
            pass

        # Fallback: iterate all addresses and pick the first non-loopback one.
        try:
            for info in socket.getaddrinfo(
                socket.gethostname(), None, socket.AF_INET
            ):
                addr = info[4][0]
                if not addr.startswith("127."):
                    return addr
        except OSError:
            pass

        return "127.0.0.1"

    # ------------------------------------------------------------------
    # Role resolution
    # ------------------------------------------------------------------

    def _resolve_role(self, requested: str | None) -> str:
        """Determine this machine's role.

        Args:
            requested: Explicit role string ('master'/'minion') or None.

        Returns:
            The resolved role string.
        """
        if requested in ("master", "minion"):
            return requested

        # Auto-detect: scan for any registered master
        master_exists = False
        cursor = 0
        while True:
            cursor, keys = self._redis.scan(
                cursor, match=_key("machines", "*", "role"), count=100
            )
            for k in keys:
                if self._redis.get(k) == "master":
                    master_exists = True
                    break
            if master_exists or cursor == 0:
                break

        if not master_exists:
            logger.warning(
                "No master found in Redis — promoting this machine ('%s') to master. "
                "Pass --role explicitly to suppress this message.",
                self._hostname,
            )
            return "master"

        logger.info("Existing master found; registering '%s' as minion.", self._hostname)
        return "minion"

    # ------------------------------------------------------------------
    # Internal key builders
    # ------------------------------------------------------------------

    def _machine_key(self, *parts: str) -> str:
        return _key("machines", self._hostname, *parts)

    def _shm_key(self, shm_name: str, *parts: str) -> str:
        return self._machine_key("shm", _sanitise_shm_name(shm_name), *parts)

    def _connection_request_key(self, hostname: str, shm_name: str) -> str:
        return _key("connections", hostname, _sanitise_shm_name(shm_name))

    # ------------------------------------------------------------------
    # Port allocation
    # ------------------------------------------------------------------

    def _allocate_port(self) -> int:
        """Allocate the next free ZMQ publish port.

        Returns:
            An integer port number not currently in use by this server.
        """
        port = self._base_pub_port
        while port in self._used_ports:
            port += 1
        self._used_ports.add(port)
        return port

    # ------------------------------------------------------------------
    # Machine registration
    # ------------------------------------------------------------------

    def register_machine(self) -> None:
        """Write this machine's entry and all local SHM segment metadata to Redis.

        Uses a pipeline for atomic writes. Discovered segments are enumerated
        from SHM_DIR (platform-specific).

        Raises:
            redis.RedisError: Propagated on pipeline failure after logging.
        """
        now = datetime.now(timezone.utc).isoformat()
        segments = _discover_shm_segments()
        try:
            pipe = self._redis.pipeline(transaction=True)
            pipe.set(self._machine_key("ip"), self._ip)
            pipe.set(self._machine_key("hostname"), self._hostname)
            pipe.set(self._machine_key("last_connect"), now)
            pipe.set(self._machine_key("role"), self._role)

            for seg in segments:
                safe = _sanitise_shm_name(seg["name"])
                pipe.set(
                    _key("machines", self._hostname, "shm", safe, "dimensions"),
                    json.dumps(seg["dimensions"]),
                )
                pipe.set(
                    _key("machines", self._hostname, "shm", safe, "dtype"),
                    seg["dtype"],
                )
            pipe.execute()
            logger.info(
                "Registered '%s' (%s) as %s with %d SHM segment(s).",
                self._hostname,
                self._ip,
                self._role,
                len(segments),
            )
        except redis.RedisError as exc:
            logger.error("Failed to register machine in Redis: %s", exc)
            raise

    def deregister_machine(self, hostname: str | None = None) -> None:
        """Delete all Redis keys for a machine.

        Uses SCAN + DELETE in batches; never KEYS *.

        Args:
            hostname: Machine to deregister. Defaults to the local machine.

        Raises:
            PermissionError: If a minion attempts to deregister another machine.
        """
        target = hostname or self._hostname
        if target != self._hostname and self._role != "master":
            raise PermissionError("Only master may deregister other machines.")

        pattern = _key("machines", target, "*")
        cursor = 0
        deleted = 0
        try:
            while True:
                cursor, keys = self._redis.scan(cursor, match=pattern, count=100)
                if keys:
                    self._redis.delete(*keys)
                    deleted += len(keys)
                if cursor == 0:
                    break
            logger.info(
                "Deregistered machine '%s' (%d key(s) removed).", target, deleted
            )
        except redis.RedisError as exc:
            logger.error("Failed to deregister '%s': %s", target, exc)

    # ------------------------------------------------------------------
    # Heartbeat
    # ------------------------------------------------------------------

    def heartbeat(self) -> None:
        """Refresh last_connect for the local machine on a background thread.

        Runs until the stop event is set. Redis errors are logged as warnings
        and retried on the next interval rather than crashing the thread.
        """
        while not self._stop_event.wait(self._heartbeat_interval):
            try:
                now = datetime.now(timezone.utc).isoformat()
                self._redis.set(self._machine_key("last_connect"), now)
                logger.debug("Heartbeat sent for '%s'.", self._hostname)
            except redis.RedisError as exc:
                logger.warning("Heartbeat failed (will retry): %s", exc)

    # ------------------------------------------------------------------
    # Periodic local SHM registry sync
    # ------------------------------------------------------------------

    def _sync_shm_registry(self) -> None:
        """Sync local SHM segments to Redis at 1 Hz on a background thread.

        Adds newly created segments and removes segments that no longer exist.
        Remote mirror segments (prefixed 'remote_') are always excluded.
        """
        while not self._stop_event.wait(1.0):
            try:
                self._do_sync_shm_registry()
            except redis.RedisError as exc:
                logger.warning("SHM registry sync failed: %s", exc)

    def _do_sync_shm_registry(self) -> None:
        """Perform one sync pass: add new segments, remove missing ones."""
        # Current segments on disk (exclude remote mirrors)
        current: set[str] = {
            os.path.basename(p)[: -len(".im.shm")]
            for p in glob.glob(os.path.join(SHM_DIR, "*.im.shm"))
            if not os.path.basename(p).startswith("remote_")
        }

        # Segments currently registered in Redis for this host
        pattern = _key("machines", self._hostname, "shm", "*", "dtype")
        registered: set[str] = set()
        cursor = 0
        while True:
            cursor, keys = self._redis.scan(cursor, match=pattern, count=100)
            for k in keys:
                parts = k.split(":")
                if len(parts) >= 5:
                    registered.add(parts[4])
            if cursor == 0:
                break

        # Remove stale segments
        for name in registered - current:
            safe = _sanitise_shm_name(name)
            stale_pattern = _key("machines", self._hostname, "shm", safe, "*")
            cursor = 0
            while True:
                cursor, keys = self._redis.scan(
                    cursor, match=stale_pattern, count=100
                )
                if keys:
                    self._redis.delete(*keys)
                if cursor == 0:
                    break
            logger.info("SHM '%s' no longer exists — removed from registry.", name)
            # Stop any active publisher for this segment
            with self._publisher_lock:
                pub = self._active_publishers.pop(name, None)
                port = self._publisher_ports.pop(name, None)
            if pub:
                pub.pubEnable = False
                pub.pubEvent.set()
            if port is not None:
                with self._publisher_lock:
                    self._used_ports.discard(port)

        # Add new segments
        new_names = current - registered
        if not new_names:
            return
        pipe = self._redis.pipeline(transaction=True)
        added = 0
        for name in new_names:
            try:
                s = shm(os.path.join(SHM_DIR, f"{name}.im.shm"))
                md = s.get_meta_data()
                size = [int(x) for x in md["size"] if int(x) > 0]
                np_dtype = daoType2NpType(md["atype"])
                dtype_str = (
                    str(np.dtype(np_dtype)) if np_dtype is not None else "float32"
                )
                safe = _sanitise_shm_name(name)
                pipe.set(
                    _key("machines", self._hostname, "shm", safe, "dimensions"),
                    json.dumps(size),
                )
                pipe.set(
                    _key("machines", self._hostname, "shm", safe, "dtype"),
                    dtype_str,
                )
                added += 1
                logger.info("New SHM '%s' discovered — added to registry.", name)
            except Exception as exc:  # noqa: BLE001
                logger.warning(
                    "Could not read metadata for new SHM '%s': %s", name, exc
                )
        if added:
            pipe.execute()

    # ------------------------------------------------------------------
    # Stale machine cleanup (master only)
    # ------------------------------------------------------------------

    def cleanup_stale_machines(self, ttl_seconds: float = 120.0) -> None:
        """Evict machines that have not sent a heartbeat within ttl_seconds.

        Args:
            ttl_seconds: Maximum acceptable age of last_connect before eviction.

        Raises:
            PermissionError: If called on a minion node.
        """
        if self._role != "master":
            raise PermissionError("Only master may perform this operation.")

        pattern = _key("machines", "*", "last_connect")
        now = datetime.now(timezone.utc)
        cursor = 0
        try:
            while True:
                cursor, keys = self._redis.scan(cursor, match=pattern, count=100)
                for k in keys:
                    raw = self._redis.get(k)
                    if not raw:
                        continue
                    try:
                        last = datetime.fromisoformat(raw)
                        age = (now - last).total_seconds()
                        if age > ttl_seconds:
                            # Key format: dao:machines:<hostname>:last_connect
                            parts = k.split(":")
                            if len(parts) >= 3:
                                stale_host = parts[2]
                                logger.warning(
                                    "Evicting stale machine '%s' (last seen %.0fs ago).",
                                    stale_host,
                                    age,
                                )
                                self.deregister_machine(stale_host)
                    except (ValueError, TypeError) as exc:
                        logger.warning(
                            "Could not parse last_connect for key '%s': %s", k, exc
                        )
                if cursor == 0:
                    break
        except redis.RedisError as exc:
            logger.warning("Stale cleanup scan failed: %s", exc)

    # ------------------------------------------------------------------
    # Connection request monitoring (server side, background thread)
    # ------------------------------------------------------------------

    def _monitor_connection_requests(self) -> None:
        """Watch Redis for incoming SHM connection requests and start publishing.

        Polls every 2 seconds.  When a request key is found under
        dao:connections:<this_hostname>:<shm_name>, the server allocates a ZMQ
        port, starts publishing that SHM, and writes the port back to Redis so
        the client can connect.  The request key is then deleted.
        """
        pattern = _key("connections", self._hostname, "*")
        while not self._stop_event.is_set():
            try:
                cursor = 0
                while True:
                    cursor, keys = self._redis.scan(
                        cursor, match=pattern, count=100
                    )
                    for k in keys:
                        # dao:connections:<hostname>:<shm_name>
                        parts = k.split(":")
                        if len(parts) < 4:
                            continue
                        shm_name = parts[3]
                        try:
                            client_info = json.loads(self._redis.get(k) or "{}")
                        except (json.JSONDecodeError, TypeError):
                            client_info = {}
                        self._start_publishing(shm_name, client_info)
                        self._redis.delete(k)
                    if cursor == 0:
                        break
            except redis.RedisError as exc:
                logger.warning("Connection request monitor error: %s", exc)
            self._stop_event.wait(2.0)

    def _start_publishing(
        self, shm_name: str, client_info: dict | None = None
    ) -> None:
        """Allocate a port and start a ZMQ publisher for a local SHM segment.

        If already publishing, appends the new client to the connections list.
        Multiple clients subscribe to the same ZMQ PUB port (broadcast).

        Args:
            shm_name: Name of the local SHM segment to publish.
            client_info: Dict with client_hostname/client_ip from the request.
        """
        with self._publisher_lock:
            already_publishing = shm_name in self._active_publishers
            if already_publishing:
                port = self._publisher_ports[shm_name]
            else:
                port = self._allocate_port()

        if not already_publishing:
            try:
                s = shm(os.path.join(SHM_DIR, f"{shm_name}.im.shm"), pubPort=port)
                s.pubEnable = True
                s.pubThread.start()
                with self._publisher_lock:
                    self._active_publishers[shm_name] = s
                    self._publisher_ports[shm_name] = port
                logger.info(
                    "Started publishing SHM '%s' on ZMQ port %d.", shm_name, port
                )
            except Exception as exc:  # noqa: BLE001
                logger.error(
                    "Failed to start publishing SHM '%s': %s", shm_name, exc
                )
                with self._publisher_lock:
                    self._used_ports.discard(port)
                    self._active_publishers.pop(shm_name, None)
                return

        # Trigger a one-shot publish of the current SHM value so newly connecting
        # clients see the real buffer contents rather than zeros.  A short delay
        # lets the client's ZMQ subscriber socket connect before the frame is sent.
        def _nudge_initial_publish() -> None:
            time.sleep(0.15)
            with self._publisher_lock:
                pub_obj = self._active_publishers.get(shm_name)
            if pub_obj is not None:
                try:
                    pub_obj.pubThreadCounter = pub_obj.get_counter() - 1
                except Exception as exc:  # noqa: BLE001
                    logger.debug(
                        "Initial publish nudge failed for '%s': %s", shm_name, exc
                    )

        threading.Thread(
            target=_nudge_initial_publish,
            name=f"dao-nudge-{shm_name}",
            daemon=True,
        ).start()

        # Start bidirectional subscriber on the server's SHM if requested and not
        # already subscribed to another client for this segment.
        info = client_info or {}
        client_pub_port = info.get("client_pub_port")
        client_ip = info.get("client_ip", "")
        with self._publisher_lock:
            already_bidir = shm_name in self._bidirectional_sub_ports
        if info.get("bidirectional") and client_pub_port and client_ip and not already_bidir:
            s = self._active_publishers.get(shm_name)
            if s is not None:
                try:
                    s.subHost = client_ip
                    s.subPort = int(client_pub_port)
                    s.subEnable = True
                    s.subThread.start()
                    with self._publisher_lock:
                        self._bidirectional_sub_ports[shm_name] = int(client_pub_port)
                    logger.info(
                        "Bidirectional: server subscribing to '%s' on port %d "
                        "for SHM '%s'.",
                        client_ip, client_pub_port, shm_name,
                    )
                except Exception as exc:  # noqa: BLE001
                    logger.warning(
                        "Failed to start bidirectional sub for '%s': %s", shm_name, exc
                    )

        # Append this client to the connections list in Redis (WATCH/MULTI for safety)
        safe = _sanitise_shm_name(shm_name)
        conn_key = _key("machines", self._hostname, "shm", safe, "connections")
        new_entry = {
            "server_ip": self._ip,
            "port": port,
            "transport": "zmq",
            "client_hostname": (client_info or {}).get("client_hostname", ""),
            "client_ip": (client_info or {}).get("client_ip", ""),
            "bidirectional": bool((client_info or {}).get("bidirectional", False)),
            "client_pub_port": (client_info or {}).get("client_pub_port"),
        }
        pipe = self._redis.pipeline(True)
        try:
            while True:
                try:
                    pipe.watch(conn_key)
                    raw = pipe.get(conn_key)
                    connections: list[dict] = json.loads(raw) if raw else []
                    # Avoid duplicate entries for the same client
                    if not any(
                        c.get("client_hostname") == new_entry["client_hostname"]
                        and c.get("port") == port
                        for c in connections
                    ):
                        connections.append(new_entry)
                    pipe.multi()
                    pipe.set(conn_key, json.dumps(connections))
                    pipe.execute()
                    break
                except redis.WatchError:
                    continue
        finally:
            pipe.reset()

    def _stop_publisher(self, shm_name: str) -> None:
        """Stop the ZMQ publisher for a segment and remove its Redis connection keys.

        Args:
            shm_name: Name of the local SHM segment whose publisher should stop.
        """
        with self._publisher_lock:
            pub = self._active_publishers.pop(shm_name, None)
            port = self._publisher_ports.pop(shm_name, None)
            if port is not None:
                self._used_ports.discard(port)
            self._bidirectional_sub_ports.pop(shm_name, None)
        if pub:
            try:
                pub.pubEnable = False
                pub.pubEvent.set()
                # Stop bidirectional subscriber if it was started
                if pub.subEnable:
                    pub.subEnable = False
                    pub.subEvent.set()
            except Exception as exc:  # noqa: BLE001
                logger.warning("Error stopping publisher for '%s': %s", shm_name, exc)
        # Remove the connections key so future clients know they must request again
        safe = _sanitise_shm_name(shm_name)
        try:
            self._redis.delete(
                _key("machines", self._hostname, "shm", safe, "connections")
            )
        except redis.RedisError:
            pass
        logger.info("Publisher for SHM '%s' stopped.", shm_name)

    # ------------------------------------------------------------------
    # Client heartbeat monitor (server side)
    # ------------------------------------------------------------------

    def _monitor_client_heartbeats(self) -> None:
        """Periodically evict stale clients and stop publishers with no subscribers.

        Runs on a daemon thread at 15-second intervals.  When a client's
        heartbeat key has not been refreshed within _CLIENT_HEARTBEAT_TTL seconds
        it is removed from the connections list.  If the list becomes empty the
        publisher is stopped to free ZMQ and SHM resources.
        """
        while not self._stop_event.wait(15.0):
            try:
                self._do_monitor_client_heartbeats()
            except redis.RedisError as exc:
                logger.warning("Client heartbeat monitor error: %s", exc)

    def _do_monitor_client_heartbeats(self) -> None:
        """Single pass: evict stale clients from every active publisher."""
        with self._publisher_lock:
            active_names = list(self._active_publishers.keys())

        now = datetime.now(timezone.utc)
        for shm_name in active_names:
            safe = _sanitise_shm_name(shm_name)
            conn_key = _key("machines", self._hostname, "shm", safe, "connections")
            hb_pattern = _key("heartbeats", self._hostname, safe, "*")

            # Collect alive client hostnames via their heartbeat keys
            alive_clients: set[str] = set()
            cursor = 0
            while True:
                cursor, keys = self._redis.scan(
                    cursor, match=hb_pattern, count=100
                )
                for k in keys:
                    raw = self._redis.get(k)
                    if not raw:
                        continue
                    try:
                        age = (now - datetime.fromisoformat(raw)).total_seconds()
                        if age <= _CLIENT_HEARTBEAT_TTL:
                            # key: dao:heartbeats:<host>:<shm>:<client_hostname>
                            parts = k.split(":")
                            if len(parts) >= 5:
                                alive_clients.add(parts[4])
                    except (ValueError, TypeError):
                        pass
                if cursor == 0:
                    break

            # Update the connections list atomically
            pipe = self._redis.pipeline(True)
            try:
                while True:
                    try:
                        pipe.watch(conn_key)
                        raw = pipe.get(conn_key)
                        connections: list[dict] = json.loads(raw) if raw else []
                        live = [
                            c for c in connections
                            if c.get("client_hostname") in alive_clients
                            or not c.get("client_hostname")  # keep anonymous entries
                        ]
                        evicted = [
                            c["client_hostname"] for c in connections
                            if c.get("client_hostname")
                            and c.get("client_hostname") not in alive_clients
                        ]
                        for ch in evicted:
                            logger.warning(
                                "Evicting stale client '%s' from SHM '%s' "
                                "(no heartbeat).",
                                ch, shm_name,
                            )
                        pipe.multi()
                        if live:
                            pipe.set(conn_key, json.dumps(live))
                        else:
                            pipe.delete(conn_key)
                        pipe.execute()
                        break
                    except redis.WatchError:
                        continue
            finally:
                pipe.reset()

            if not live:
                logger.info(
                    "No active clients for SHM '%s' — stopping publisher.",
                    shm_name,
                )
                self._stop_publisher(shm_name)

    # ------------------------------------------------------------------
    # Remote SHM access (client side)
    # ------------------------------------------------------------------

    def get_remote_shm(
        self,
        hostname: str,
        shm_name: str,
        bidirectional: bool = False,
    ) -> Any:
        """Return a local shm object that mirrors a remote machine's SHM segment.

        If the remote server is not yet publishing the segment, a connection
        request is written to Redis and this method blocks until the remote
        server starts publishing (up to 30 s).

        When *bidirectional* is True both sides run a publisher and subscriber,
        so writes to the local mirror propagate back to the remote SHM and vice
        versa.  The remote server subscribes to a port allocated on this machine.

        Args:
            hostname: Hostname of the remote machine as registered in Redis.
            shm_name: Name of the SHM segment on the remote machine.
            bidirectional: If True, also publish from the local mirror back to
                the remote server so writes go both ways.

        Returns:
            A local shm object mirroring the remote segment.  In bidirectional
            mode it is both a subscriber (receives from remote) and a publisher
            (sends back to remote).

        Raises:
            KeyError: If hostname or shm_name is not registered in Redis.
            NotImplementedError: If the registered connection type is unsupported.
            TimeoutError: If the remote server does not start publishing within 30 s.
        """
        safe = _sanitise_shm_name(shm_name)

        # Validate registration
        host_ip = self._redis.get(_key("machines", hostname, "ip"))
        if host_ip is None:
            raise KeyError(f"No machine '{hostname}' registered in Redis.")

        dims_raw = self._redis.get(
            _key("machines", hostname, "shm", safe, "dimensions")
        )
        if dims_raw is None:
            raise KeyError(
                f"No SHM '{shm_name}' registered for host '{hostname}'."
            )

        # Check whether the remote is already publishing (non-empty connections list)
        conn_key = _key("machines", hostname, "shm", safe, "connections")
        connections_raw = self._redis.get(conn_key)
        connections: list[dict] = json.loads(connections_raw) if connections_raw else []

        # Allocate a local pub port now if bidirectional (before sending the request
        # so the server knows which port to subscribe to).
        client_pub_port: int | None = None
        if bidirectional:
            with self._publisher_lock:
                client_pub_port = self._allocate_port()
            self._mirror_pub_ports[(hostname, shm_name)] = client_pub_port

        if not connections:
            # Register a connection request in Redis
            logger.info(
                "Requesting SHM '%s' from remote host '%s'%s.",
                shm_name, hostname,
                " (bidirectional)" if bidirectional else "",
            )
            req_key = self._connection_request_key(hostname, shm_name)
            self._redis.set(
                req_key,
                json.dumps(
                    {
                        "client_ip": self._ip,
                        "client_hostname": self._hostname,
                        "transport": "zmq",
                        "bidirectional": bidirectional,
                        "client_pub_port": client_pub_port,
                    }
                ),
            )

            # Poll until the remote server populates the connections list
            timeout_s = 30.0
            deadline = time.monotonic() + timeout_s
            while time.monotonic() < deadline:
                connections_raw = self._redis.get(conn_key)
                connections = (
                    json.loads(connections_raw) if connections_raw else []
                )
                if connections:
                    break
                time.sleep(0.5)
            else:
                raise TimeoutError(
                    f"Remote server '{hostname}' did not start publishing SHM "
                    f"'{shm_name}' within {timeout_s:.0f} s."
                )

        conn_port: int = int(connections[0]["port"])
        conn_type: str = connections[0].get("transport", "zmq")

        connector = _CONNECTORS.get(conn_type)
        if connector is None:
            raise NotImplementedError(f"{conn_type} connector not implemented.")

        # Build local mirror
        dimensions: list[int] = json.loads(dims_raw)
        dtype_str: str = (
            self._redis.get(_key("machines", hostname, "shm", safe, "dtype"))
            or "float32"
        )
        mirror_name = f"remote_{hostname}_{safe}"

        logger.info(
            "Mirroring remote SHM '%s@%s' → local '%s/%s.im.shm' "
            "via tcp://%s:%d.",
            shm_name,
            hostname,
            SHM_DIR,
            mirror_name,
            host_ip,
            conn_port,
        )
        mirror = connector(
            host_ip, conn_port, mirror_name, dimensions, dtype_str,
            pub_port=client_pub_port,
        )

        # Wait for the server's initial publish so the mirror is seeded with
        # the real buffer contents rather than the zero-initialised array.
        _init_deadline = time.monotonic() + 2.0
        while time.monotonic() < _init_deadline:
            if mirror.get_counter() > 0:
                break
            time.sleep(0.05)
        else:
            logger.debug(
                "No initial frame received for '%s@%s' within 2 s; "
                "mirror initialised to zero.",
                shm_name,
                hostname,
            )

        self._active_mirrors[(hostname, shm_name)] = mirror
        self._start_client_heartbeat(hostname, shm_name)
        return mirror

    def _start_client_heartbeat(self, hostname: str, shm_name: str) -> None:
        """Start a daemon thread that refreshes a client heartbeat key in Redis.

        The key dao:heartbeats:<hostname>:<shm_name>:<self._hostname> is written
        every _CLIENT_HEARTBEAT_INTERVAL seconds so the remote server knows this
        client is still subscribed.  The key is deleted when the thread exits.

        Args:
            hostname: Hostname of the remote machine serving the SHM.
            shm_name: Name of the remote SHM segment.
        """
        # Cancel any existing heartbeat for this (hostname, shm_name) pair
        existing = self._client_heartbeat_events.pop((hostname, shm_name), None)
        if existing:
            existing.set()

        stop_event = threading.Event()
        self._client_heartbeat_events[(hostname, shm_name)] = stop_event

        safe = _sanitise_shm_name(shm_name)
        hb_key = _key("heartbeats", hostname, safe, self._hostname)
        redis_ex = int(_CLIENT_HEARTBEAT_TTL * 3)  # auto-expire as safety net

        def _loop() -> None:
            # Write immediately so the server sees us before the first interval
            try:
                self._redis.set(
                    hb_key, datetime.now(timezone.utc).isoformat(), ex=redis_ex
                )
            except redis.RedisError as exc:
                logger.warning("Initial client heartbeat failed: %s", exc)

            while not stop_event.wait(_CLIENT_HEARTBEAT_INTERVAL):
                try:
                    self._redis.set(
                        hb_key,
                        datetime.now(timezone.utc).isoformat(),
                        ex=redis_ex,
                    )
                    logger.debug(
                        "Client heartbeat sent for '%s@%s'.", shm_name, hostname
                    )
                except redis.RedisError as exc:
                    logger.warning("Client heartbeat write failed: %s", exc)

            # Clean up the key when disconnecting
            try:
                self._redis.delete(hb_key)
            except redis.RedisError:
                pass

        threading.Thread(
            target=_loop,
            name=f"dao-client-hb-{hostname}-{safe}",
            daemon=True,
        ).start()

    def release_remote_shm(self, hostname: str, shm_name: str) -> None:
        """Stop the client heartbeat and remove this client from the connections list.

        Should be called before closing a mirror shm returned by get_remote_shm.

        Args:
            hostname: Hostname of the remote machine.
            shm_name: Name of the remote SHM segment.
        """
        # Stop heartbeat thread
        stop_event = self._client_heartbeat_events.pop((hostname, shm_name), None)
        if stop_event:
            stop_event.set()

        # Stop the mirror's bidirectional publisher (if any) and free the port
        mirror_pub_port = self._mirror_pub_ports.pop((hostname, shm_name), None)
        mirror = self._active_mirrors.pop((hostname, shm_name), None)
        if mirror is not None and mirror_pub_port is not None:
            with self._publisher_lock:
                self._used_ports.discard(mirror_pub_port)
            try:
                mirror.pubEnable = False
                mirror.pubEvent.set()
            except Exception as exc:  # noqa: BLE001
                logger.warning("Error stopping mirror pub for '%s@%s': %s", shm_name, hostname, exc)

        # Remove this client from the connections list
        safe = _sanitise_shm_name(shm_name)
        conn_key = _key("machines", hostname, "shm", safe, "connections")
        pipe = self._redis.pipeline(True)
        try:
            while True:
                try:
                    pipe.watch(conn_key)
                    raw = pipe.get(conn_key)
                    connections: list[dict] = json.loads(raw) if raw else []
                    connections = [
                        c for c in connections
                        if c.get("client_hostname") != self._hostname
                    ]
                    pipe.multi()
                    if connections:
                        pipe.set(conn_key, json.dumps(connections))
                    else:
                        pipe.delete(conn_key)
                    pipe.execute()
                    break
                except redis.WatchError:
                    continue
        except redis.RedisError as exc:
            logger.warning("release_remote_shm Redis error: %s", exc)
        finally:
            pipe.reset()

        logger.info("Released remote SHM '%s@%s'.", shm_name, hostname)

    # ------------------------------------------------------------------
    # Machine listing
    # ------------------------------------------------------------------

    def list_machines(self) -> list[dict]:
        """Return a list of all registered machines with their SHM segments.

        Returns:
            List of dicts, each with keys: 'hostname', 'ip', 'role',
            'last_connect', and 'shm_segments' (list of segment names).
        """
        machines: dict[str, dict] = {}
        cursor = 0
        try:
            # Collect all registered hostnames
            while True:
                cursor, keys = self._redis.scan(
                    cursor, match=_key("machines", "*", "hostname"), count=100
                )
                for k in keys:
                    # dao:machines:<hostname>:hostname
                    parts = k.split(":")
                    if len(parts) < 3:
                        continue
                    host = parts[2]
                    machines[host] = {
                        "hostname": self._redis.get(_key("machines", host, "hostname")),
                        "ip": self._redis.get(_key("machines", host, "ip")),
                        "role": self._redis.get(_key("machines", host, "role")),
                        "last_connect": self._redis.get(
                            _key("machines", host, "last_connect")
                        ),
                        "shm_segments": [],
                    }
                if cursor == 0:
                    break

            # Enumerate SHM segments for each machine
            for host, info in machines.items():
                shm_pattern = _key("machines", host, "shm", "*", "dtype")
                cursor = 0
                while True:
                    cursor, keys = self._redis.scan(
                        cursor, match=shm_pattern, count=100
                    )
                    for k in keys:
                        # dao:machines:<host>:shm:<shm_name>:dtype
                        parts = k.split(":")
                        if len(parts) >= 5:
                            info["shm_segments"].append(parts[4])
                    if cursor == 0:
                        break

        except redis.RedisError as exc:
            logger.error("Failed to list machines: %s", exc)

        return list(machines.values())

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self) -> None:
        """Register this machine and start all background threads.

        Blocks until stop() is called.
        """
        self.register_machine()

        heartbeat_thread = threading.Thread(
            target=self.heartbeat, name="dao-heartbeat", daemon=True
        )
        heartbeat_thread.start()

        sync_thread = threading.Thread(
            target=self._sync_shm_registry, name="dao-shm-sync", daemon=True
        )
        sync_thread.start()

        monitor_thread = threading.Thread(
            target=self._monitor_connection_requests,
            name="dao-conn-monitor",
            daemon=True,
        )
        monitor_thread.start()

        client_hb_thread = threading.Thread(
            target=self._monitor_client_heartbeats,
            name="dao-client-hb-monitor",
            daemon=True,
        )
        client_hb_thread.start()

        if self._role == "master":
            def _stale_cleanup_loop() -> None:
                while not self._stop_event.wait(60.0):
                    try:
                        self.cleanup_stale_machines()
                    except redis.RedisError as exc:
                        logger.warning("Stale cleanup failed: %s", exc)

            cleanup_thread = threading.Thread(
                target=_stale_cleanup_loop,
                name="dao-stale-cleanup",
                daemon=True,
            )
            cleanup_thread.start()

        logger.info(
            "daoServer running on '%s' (%s) as %s.",
            self._hostname,
            self._ip,
            self._role,
        )
        self._stop_event.wait()

    def stop(self) -> None:
        """Gracefully shut down the server.

        Signals all background threads to exit, stops active ZMQ publishers,
        deregisters this machine from Redis, and closes the Redis connection.
        """
        logger.info("Shutting down daoServer on '%s'...", self._hostname)
        self._stop_event.set()

        # Stop all active publishers
        with self._publisher_lock:
            names = list(self._active_publishers.keys())
        for name in names:
            self._stop_publisher(name)

        self.deregister_machine()

        try:
            self._redis.close()
        except redis.RedisError:
            pass

        logger.info("daoServer stopped.")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import argparse
    import signal

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s  %(levelname)-8s  %(name)s: %(message)s",
    )

    parser = argparse.ArgumentParser(
        description="daoServer — distributed SHM registry daemon"
    )
    parser.add_argument(
        "--discover",
        action="store_true",
        help="Scan local subnets for Redis/daoServer instances and exit",
    )
    parser.add_argument(
        "--redis-host",
        default="localhost",
        metavar="HOST",
        help="Redis hostname or IP (default: localhost)",
    )
    parser.add_argument(
        "--redis-port",
        type=int,
        default=6379,
        metavar="PORT",
        help="Redis port (default: 6379)",
    )
    parser.add_argument(
        "--role",
        choices=["master", "minion"],
        default=None,
        help="Node role; omit for auto-detection",
    )
    parser.add_argument(
        "--heartbeat-interval",
        type=float,
        default=30.0,
        metavar="SECONDS",
        help="Heartbeat interval in seconds (default: 30)",
    )
    parser.add_argument(
        "--base-pub-port",
        type=int,
        default=5560,
        metavar="PORT",
        help="First ZMQ port to use for SHM publishing (default: 5560)",
    )
    args = parser.parse_args()

    if args.discover:
        found = discover_redis()
        if not found:
            print("No Redis instances found on local subnets.")
        else:
            print(f"Found {len(found)} Redis instance(s):")
            for entry in found:
                dao_tag = " [dao]" if entry["dao"] else ""
                machines = ", ".join(entry["machines"]) if entry["machines"] else "—"
                print(
                    f"  {entry['host']}:{entry['port']}{dao_tag}"
                    f"  machines: {machines}"
                )
        sys.exit(0)

    server = DaoServer(
        redis_host=args.redis_host,
        redis_port=args.redis_port,
        role=args.role,
        heartbeat_interval=args.heartbeat_interval,
        base_pub_port=args.base_pub_port,
    )

    def _handle_signal(sig: int, frame: Any) -> None:
        logger.info("Received signal %d — stopping.", sig)
        server.stop()

    signal.signal(signal.SIGINT, _handle_signal)
    signal.signal(signal.SIGTERM, _handle_signal)

    server.start()
