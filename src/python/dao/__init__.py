# dao/__init__.py
# Re-export selected submodules' public APIs.

# --- Native loader: ensure packaged .so/.dylib/.dll are dlopened early ---
import ctypes, os, sys, pathlib, glob

_pkg_dir = pathlib.Path(__file__).resolve().parent

def _load_native_libs():
    # Windows: allow DLLs in package dir
    if hasattr(os, "add_dll_directory"):
        try:
            os.add_dll_directory(str(_pkg_dir))
        except Exception:
            pass

    # Choose platform suffixes and load dependencies first, then main lib
    if sys.platform == "win32":
        names = ["daoProto.dll", "daoNuma.dll", "dao.dll"]
    elif sys.platform == "darwin":
        names = ["libdaoProto.dylib", "libdaoNuma.dylib", "libdao.dylib"]
    else:  # Linux and others
        names = ["libdaoProto.so", "libdaoNuma.so", "libdao.so"]

    # Use RTLD_GLOBAL so libdao.so can resolve symbols from its deps
    mode = getattr(ctypes, "RTLD_GLOBAL", 0)
    for base in names:
        # tolerate versioned/symlinked .so (e.g., libdao.so.0.0.1)
        candidates = list(_pkg_dir.glob(base)) or sorted(_pkg_dir.glob(base + "*"))
        for p in candidates:
            try:
                ctypes.CDLL(str(p), mode=mode)
                break
            except OSError:
                continue
        else:
            # Not fatal if a dep isn't present (e.g., on Windows without that file),
            # but main lib must be loadable when actually used.
            pass

_load_native_libs()
# --- end native loader ---

from . import (
    # 1) protobuf modules first
    daoCommand_pb2     as _pbCommand,
    daoEvent_pb2       as _pbEvent,
    daoFileServer_pb2  as _pbFileServer,
    daoLogging_pb2     as _pbLogging,

    # 2) then your core modules in order
    daoLog                     as _daoLog,
    daoComponentStateMachine   as _daoComponentStateMachine,
    daoComponent               as _daoComponent,
    daoDb                      as _daoDb,
    daoDoubleBuffer            as _daoDoubleBuffer,
    daoEvent                   as _daoEvent,
    daoProxy                   as _daoProxy,
    daoShm                     as _daoShm,
)

__all__ = []

def _reexport(mod):
    public = getattr(mod, "__all__", [n for n in dir(mod) if not n.startswith("_")])
    for name in public:
        globals()[name] = getattr(mod, name)
    __all__.extend(public)

# Load order: pb2 first, then the rest
for _m in (
    _pbCommand,
    _pbEvent,
    _pbFileServer,
    _pbLogging,
    _daoLog,
    _daoComponentStateMachine,
    _daoComponent,
    _daoDb,
    _daoDoubleBuffer,
    _daoEvent,
    _daoProxy,
    _daoShm,
):
    _reexport(_m)

# Keep the package namespace tidy
del _reexport, _m
del _pbCommand, _pbEvent, _pbFileServer, _pbLogging
del _daoShm, _daoLog, _daoComponent, _daoComponentStateMachine, _daoDb, _daoDoubleBuffer, _daoEvent, _daoProxy
