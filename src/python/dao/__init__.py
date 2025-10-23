# dao/__init__.py
# Re-export selected submodules' public APIs.

from . import (
    daoLog     as _daoLog,
    daoComponentStateMachine as _daoComponentStateMachine,
    daoComponent as _daoComponent,
    daoDb      as _daoDb,
    daoDoubleBuffer as _daoDoubleBuffer,
    daoEvent   as _daoEvent,
    daoProxy   as _daoProxy,
    daoShm     as _daoShm,
)

__all__ = []

def _reexport(mod):
    public = getattr(mod, "__all__", [n for n in dir(mod) if not n.startswith("_")])
    for name in public:
        globals()[name] = getattr(mod, name)
    __all__.extend(public)

for _m in (
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
del _daoShm, _daoLog, _daoComponent, _daoComponentStateMachine, _daoDb, _daoDoubleBuffer, _daoEvent, _daoProxy
