from pathlib import Path
from setuptools import setup
from setuptools.command.build_py import build_py as _build_py
from setuptools.command.develop import develop as _develop
from setuptools.dist import Distribution
import subprocess, shutil, os, tempfile, sys


class BinaryDistribution(Distribution):
    def has_ext_modules(self):
        # Wheel is platform-specific because we include native libs
        return True


def _which(cmd):
    from shutil import which
    return which(cmd)


def _run(cmd, cwd=None, env=None):
    print(">>>", " ".join(map(str, cmd)))
    subprocess.check_call(cmd, cwd=cwd, env=env)


def _run_waf(args, repo_root: Path, env):
    """Try waf via local ./waf, then system 'waf' on PATH."""
    waf_local = repo_root / "waf"
    if waf_local.exists():
        _run([str(waf_local), *args], cwd=repo_root, env=env)
        return
    waf_sys = _which("waf")
    if waf_sys:
        _run([waf_sys, *args], cwd=repo_root, env=env)
        return
    raise RuntimeError("waf not found. Add ./waf at repo root or install a system 'waf'.")


def _stage_waf_build(repo_root: Path) -> Path:
    """Build with waf and install into a temporary staging prefix. Returns stage path."""
    env = os.environ.copy()
    stage = Path(tempfile.mkdtemp(prefix="dao_waf_stage_"))
    # Let caller override configure flags by env, otherwise use default prefix
    _run_waf(["configure", f"--prefix={stage}"], repo_root, env)
    _run_waf(["build"], repo_root, env)
    _run_waf(["install"], repo_root, env)
    return stage


def _copy_stage_into_package(stage: Path, build_root: Path, pkg_relpath: str = "dao"):
    """
    Copy native libs and generated protobufs from stage into the package
    directory inside build_lib.
    """
    pkg_dir = build_root / pkg_relpath
    pkg_dir.mkdir(parents=True, exist_ok=True)

    # Native libs (Linux/Mac/Windows)
    for libdir in (stage / "lib", stage / "lib64", stage / "bin"):
        if libdir.is_dir():
            for pat in ("libdao*", "libdaoProto*", "dao*.dll", "*.dylib", "*.so"):
                for p in libdir.glob(pat):
                    if p.is_file():
                        shutil.copy2(p, pkg_dir / p.name)
                        print(f"Copied native lib: {p} -> {pkg_dir / p.name}")

    # Generated protobufs (installed by waf into stage/python)
    py_stage = stage / "python"
    if py_stage.is_dir():
        for p in py_stage.glob("*_pb2.py"):
            shutil.copy2(p, pkg_dir / p.name)
            print(f"Copied protobuf: {p} -> {pkg_dir / p.name}")

def _copy_pb2_into_package(build_root: Path, pkg_relpath: str, preferred_dir: Path | None = None):
    """
    Ensure all dao*_pb2.py end up inside the built package.
    Priority:
      1) preferred_dir (typically stage/python)
      2) source package dir (src/python/dao)
      3) any build/ or repo tree locations (last resort, rglob)
    """
    repo_root = Path(__file__).resolve().parent
    pkg_dir = build_root / pkg_relpath
    pkg_dir.mkdir(parents=True, exist_ok=True)

    seen = set()

    def _copy_all(root: Path):
        if not root or not root.exists():
            return
        for p in root.rglob("dao*_pb2.py"):
            if p.is_file():
                dst = pkg_dir / p.name
                if dst.name not in seen:
                    shutil.copy2(p, dst)
                    seen.add(dst.name)
                    print(f"Copied protobuf: {p} -> {dst}")

    # 1) whatever waf installed to stage/python
    _copy_all(preferred_dir)

    # 2) source package tree (if you sometimes commit pb2 into src)
    _copy_all(repo_root / "src" / "python" / "dao")

    # 3) anywhere else in the repo (e.g. waf build dir)
    #    this catches build/**/dao*_pb2.py etc.
    for candidate in ("build", "out", "."):
        _copy_all(repo_root / candidate)

class build_py(_build_py):
    def run(self):
        repo_root = Path(__file__).resolve().parent

        # 1) Build + install with waf to a temporary stage
        stage = _stage_waf_build(repo_root)

        # 2) Run normal setuptools build to collect the package code
        super().run()

        # 3) Copy staged native bits into the built package
        build_root = Path(self.build_lib)    # site-packages build root
        _copy_stage_into_package(stage, build_root, pkg_relpath="dao")
        _copy_pb2_into_package(build_root, "dao")


class develop(_develop):
    """
    Ensure 'pip install -e .' also runs waf once so your local import sees the libs.
    We still install libs into the package directory inside the build area that
    editable installs reference.
    """
    def run(self):
        repo_root = Path(__file__).resolve().parent
        stage = _stage_waf_build(repo_root)

        # Let develop set up the .egg-link and scripts
        super().run()

        # Where setuptools puts the "build" tree for editable usage
        # Guard across variants by reusing build command's computed path
        build_cmd = self.get_finalized_command("build_py")
        build_root = Path(build_cmd.build_lib)
        _copy_stage_into_package(stage, build_root, pkg_relpath="dao")


setup(
    cmdclass={
        "build_py": build_py,
        "develop": develop,
    },
    distclass=BinaryDistribution,
)
