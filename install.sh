#!/usr/bin/env bash
#
# install.sh - one-shot installer for daoBase.
#
# Automates the steps previously spread across README.md:
#   1. system packages   (apt / dnf / yum / pacman / zypper / brew)
#   2. Miniconda          (reused if already present, installed otherwise)
#   3. conda env          (a dedicated "dao" env by default)
#   4. waf                (reused from PATH / ~/bin, downloaded otherwise)
#   5. DAOROOT / DAODATA  (you are asked where they should live; defaults to
#                          /opt/dao/{DAOROOT,DAODATA}, created with sudo and
#                          chown'd to you if needed)
#   6. python deps        (pip install of the packages daoShm/dao/... need)
#   7. build + install    everything (incl. the Python modules) with
#                          waf --prefix=$DAOROOT, as in the README
#   8. dao_env.sh         one file with all the exports; you are asked
#                          before it is wired into your shell rc
#   9. verify             `import daoShm` + a SHM round-trip
#
# Notes
#   - Idempotent: re-run freely. Existing conda / waf / rc blocks are reused.
#   - Non-destructive: shell rc files are only touched after an explicit yes,
#     fenced with markers, with a timestamped .bak.
#   - Rehearse safely: --home <dir> redirects every path (conda, ~/bin, rc
#     files, DAOROOT default) into a throwaway tree.
#
# Run  ./install.sh --help  for options.

set -euo pipefail

# --------------------------------------------------------------------------
DAO_SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

HOME_DIR="${HOME}"
HOME_OVERRIDDEN=0
DRY_RUN=0
ASSUME_YES=0
DO_SYSTEM_DEPS=1
DO_CONDA=1
DO_WAF=1
DO_BUILD=1
DO_PYTHON=1
WITH_EXTRAS=0
CONDA_ENV="dao"
CONDA_PY="3.12"
PIP_EXTRA=""
DAOROOT_IN=""
DAODATA_IN=""
UPDATE_RC="ask"          # ask | yes | no

# --------------------------------------------------------------------------
if [ -t 1 ]; then
  C_B="\033[1m"; C_G="\033[32m"; C_Y="\033[33m"; C_R="\033[31m"; C_0="\033[0m"
else
  C_B=""; C_G=""; C_Y=""; C_R=""; C_0=""
fi
step()  { printf "\n${C_B}==> %s${C_0}\n" "$*"; }
info()  { printf "    %s\n" "$*"; }
ok()    { printf "    ${C_G}ok${C_0} %s\n" "$*"; }
warn()  { printf "    ${C_Y}warning:${C_0} %s\n" "$*" >&2; }
die()   { printf "\n${C_R}error:${C_0} %s\n" "$*" >&2; exit 1; }

run() {
  printf "    ${C_B}\$${C_0} %s\n" "$*"
  [ "$DRY_RUN" -eq 1 ] && return 0
  "$@"
}
run_sh() {
  printf "    ${C_B}\$${C_0} %s\n" "$*"
  [ "$DRY_RUN" -eq 1 ] && return 0
  bash -c "$*"
}
ask() {
  local q="$1" ans
  [ "$ASSUME_YES" -eq 1 ] && { info "$q -> yes (--yes)"; return 0; }
  read -r -p "    $q [y/N] " ans </dev/tty || ans=""
  [[ "$ans" == [yY] || "$ans" == [yY][eE][sS] ]]
}
ask_value() {
  local __var="$1" prompt="$2" def="$3" ans
  if [ "$ASSUME_YES" -eq 1 ]; then
    printf -v "$__var" '%s' "$def"; info "$prompt -> $def (--yes)"; return
  fi
  read -r -p "    $prompt [$def] " ans </dev/tty || ans=""
  printf -v "$__var" '%s' "${ans:-$def}"
}

# --------------------------------------------------------------------------
usage() {
  sed -n '3,33p' "$0" | sed 's/^# \{0,1\}//'
  cat <<EOF

Options:
  --home DIR            Treat DIR as \$HOME for every path the installer touches.
  --prefix DIR          DAOROOT / install prefix.   Default: /opt/dao/DAOROOT
                         (or <home>/DAOROOT under --home). Created with sudo
                         + chown'd to you if the parent isn't writable yet.
                         Tip: --prefix "\$CONDA_PREFIX" removes the need for
                         LD_LIBRARY_PATH entirely.
  --data DIR            DAODATA (runtime data dir). Default: /opt/dao/DAODATA
                         (or <home>/DAODATA under --home). Same sudo handling
                         as --prefix.
  --conda-env NAME      Conda env to use/create.    Default: $CONDA_ENV
  --conda-python VER    Python for a new env.       Default: $CONDA_PY
  --with-extras         Also pip install GUI/plot extras (PyQt5, matplotlib...).
  --pip-extra 'SPEC'    Extra pip spec, repeatable (e.g. 'protobuf==3.20.*').
  --update-rc yes|no    Add / skip the shell-rc line without being asked.
  --skip-system-deps    Assume OS packages are already present.
  --skip-conda          Use whatever 'python' resolves to; do not touch conda.
  --skip-waf            Assume waf is available.
  --skip-python         Do not pip install the Python dependencies.
  --skip-build          Do not run the waf build/install.
  --yes                 Accept every default / prompt.
  --dry-run             Print commands without executing.
  -h, --help            This help.
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --home)             HOME_DIR="$2"; HOME_OVERRIDDEN=1; shift 2 ;;
    --prefix)           DAOROOT_IN="$2"; shift 2 ;;
    --data)             DAODATA_IN="$2"; shift 2 ;;
    --conda-env)        CONDA_ENV="$2"; shift 2 ;;
    --conda-python)     CONDA_PY="$2"; shift 2 ;;
    --with-extras)      WITH_EXTRAS=1; shift ;;
    --pip-extra)        PIP_EXTRA="$PIP_EXTRA $2"; shift 2 ;;
    --update-rc)        UPDATE_RC="$2"; shift 2 ;;
    --skip-system-deps) DO_SYSTEM_DEPS=0; shift ;;
    --skip-conda)       DO_CONDA=0; shift ;;
    --skip-waf)         DO_WAF=0; shift ;;
    --skip-python)      DO_PYTHON=0; shift ;;
    --skip-build)       DO_BUILD=0; shift ;;
    --yes|-y)           ASSUME_YES=1; shift ;;
    --dry-run)          DRY_RUN=1; shift ;;
    -h|--help)          usage; exit 0 ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
done
case "$UPDATE_RC" in yes|no|ask) ;; *) die "--update-rc must be yes or no" ;; esac

HOME_DIR="$(cd "$HOME_DIR" 2>/dev/null && pwd || echo "$HOME_DIR")"
BIN_DIR="$HOME_DIR/bin"
# Default install location is the shared /opt/dao/... prefix; under --home
# (rehearsal mode) default into the sandboxed home instead, so --home keeps
# meaning "nothing real gets touched" without the caller having to also pass
# --prefix/--data by hand.
if [ "$HOME_OVERRIDDEN" -eq 1 ]; then
  DAOROOT="${DAOROOT_IN:-$HOME_DIR/DAOROOT}"
  DAODATA="${DAODATA_IN:-$HOME_DIR/DAODATA}"
else
  DAOROOT="${DAOROOT_IN:-/opt/dao/DAOROOT}"
  DAODATA="${DAODATA_IN:-/opt/dao/DAODATA}"
fi
ENV_FILE="$DAO_SRC/dao_env.sh"

# --------------------------------------------------------------------------
OS="unknown"; DISTRO="unknown"; PKG=""; ARCH="$(uname -m)"
detect_platform() {
  case "$(uname -s)" in
    Linux)  OS="linux" ;;
    Darwin) OS="macos" ;;
    *) die "unsupported OS: $(uname -s)" ;;
  esac
  if [ "$OS" = macos ]; then
    DISTRO="macos"; PKG="brew"
  elif [ -r /etc/os-release ]; then
    . /etc/os-release
    DISTRO="${ID:-linux}"
    local like="${ID_LIKE:-}"
    if   command -v apt-get >/dev/null 2>&1; then PKG="apt"
    elif command -v dnf     >/dev/null 2>&1; then PKG="dnf"
    elif command -v yum     >/dev/null 2>&1; then PKG="yum"
    elif command -v pacman  >/dev/null 2>&1; then PKG="pacman"
    elif command -v zypper  >/dev/null 2>&1; then PKG="zypper"
    fi
    [ -n "$PKG" ] || case "$like" in
      *debian*) PKG="apt" ;; *rhel*|*fedora*) PKG="dnf" ;;
      *suse*)   PKG="zypper" ;; *arch*) PKG="pacman" ;;
    esac
  fi
  case "$ARCH" in
    x86_64|amd64) ARCH="x86_64" ;;
    aarch64|arm64) ARCH="arm64" ;;
  esac
  info "OS=$OS  distro=$DISTRO  pkg-manager=${PKG:-none}  arch=$ARCH"
}

# --------------------------------------------------------------------------
system_deps() {
  step "System packages"
  if [ "$DO_SYSTEM_DEPS" -eq 0 ]; then info "skipped (--skip-system-deps)"; return; fi

  local apt_pkgs="build-essential libtool pkg-config autoconf automake git curl wget \
libssl-dev libncurses-dev libgsl-dev libgtest-dev libzmq3-dev libprotobuf-dev protobuf-compiler \
libnuma-dev numactl redis-server"
  local dnf_pkgs="gcc gcc-c++ make autoconf automake libtool pkgconf-pkg-config git curl wget \
openssl-devel ncurses-devel gsl-devel gtest-devel zeromq-devel protobuf-devel protobuf-compiler \
numactl-devel numactl redis"
  local pacman_pkgs="base-devel git curl wget openssl ncurses gsl gtest zeromq protobuf numactl redis"
  local zypper_pkgs="gcc gcc-c++ make autoconf automake libtool pkg-config git curl wget \
libopenssl-devel ncurses-devel gsl-devel gtest zeromq-devel protobuf-devel libnuma-devel redis"
  local brew_pkgs="pkg-config zeromq protobuf gsl"

  local sudo=""; [ "$(id -u)" -ne 0 ] && sudo="sudo"

  case "$PKG" in
    apt)
      ask "Run: $sudo apt-get install -y <build deps> ?" || { warn "skipped; install manually"; return; }
      run_sh "$sudo apt-get update"
      run_sh "$sudo apt-get install -y $apt_pkgs"
      ;;
    dnf|yum)
      ask "Run: $sudo $PKG install -y <build deps> ?" || { warn "skipped; install manually"; return; }
      # --allowerasing: minimal RHEL/Rocky/Fedora images ship curl-minimal,
      # which conflicts with the full curl package below unless dnf is
      # allowed to swap it out. dnf-only flag - legacy yum (RHEL/CentOS 7)
      # doesn't understand it, so only add it when $PKG really is dnf.
      local dnf_extra=""; [ "$PKG" = dnf ] && dnf_extra="--allowerasing"
      run_sh "$sudo $PKG install -y $dnf_extra $dnf_pkgs" || warn "some packages may require EPEL/PowerTools"
      ;;
    pacman)
      ask "Run: $sudo pacman -S --needed <build deps> ?" || { warn "skipped"; return; }
      run_sh "$sudo pacman -S --needed --noconfirm $pacman_pkgs"
      ;;
    zypper)
      ask "Run: $sudo zypper install <build deps> ?" || { warn "skipped"; return; }
      run_sh "$sudo zypper --non-interactive install $zypper_pkgs"
      ;;
    brew)
      command -v brew >/dev/null 2>&1 || die "Homebrew not found - install from https://brew.sh then re-run"
      run_sh "brew install $brew_pkgs"
      if command -v conda >/dev/null 2>&1 && conda list 2>/dev/null | grep -q '^libprotobuf'; then
        warn "conda's libprotobuf can clash with brew's protobuf; consider: conda uninstall libprotobuf"
      fi
      ;;
    *)
      warn "unknown package manager - install manually: zeromq, protobuf(+compiler), gsl,"
      info "  gtest, ncurses, openssl, numactl (Linux), redis, and a C/C++ toolchain"
      ;;
  esac
}

# --------------------------------------------------------------------------
CONDA_BIN=""
find_conda() {
  local c
  for c in "${CONDA_EXE:-}" "$(command -v conda 2>/dev/null || true)" \
           "$HOME_DIR/miniconda3/bin/conda" "$HOME_DIR/anaconda3/bin/conda" \
           "$HOME_DIR/miniforge3/bin/conda" "$HOME_DIR/mambaforge/bin/conda" \
           "/opt/miniconda3/bin/conda" "/opt/conda/bin/conda"; do
    [ -n "$c" ] && [ -x "$c" ] && { CONDA_BIN="$c"; return 0; }
  done
  return 1
}
conda_root() { cd "$(dirname "$CONDA_BIN")/.." && pwd; }

conda_setup() {
  step "Miniconda / conda"
  if [ "$DO_CONDA" -eq 0 ]; then info "skipped (--skip-conda)"; CONDA_BIN=""; return; fi

  if find_conda; then
    ok "found existing conda: $CONDA_BIN"
    info "$("$CONDA_BIN" --version 2>/dev/null || true)"
  else
    info "no conda found under $HOME_DIR"
    ask "Download + install Miniconda to $HOME_DIR/miniconda3 ?" \
      || die "conda required (or pass --skip-conda to use system python)"
    local base
    case "$OS-$ARCH" in
      linux-x86_64) base="Miniconda3-latest-Linux-x86_64.sh" ;;
      linux-arm64)  base="Miniconda3-latest-Linux-aarch64.sh" ;;
      macos-arm64)  base="Miniconda3-latest-MacOSX-arm64.sh" ;;
      macos-x86_64) base="Miniconda3-latest-MacOSX-x86_64.sh" ;;
      *) die "no Miniconda build known for $OS-$ARCH" ;;
    esac
    run mkdir -p "$HOME_DIR/miniconda3"
    run_sh "curl -fsSL 'https://repo.anaconda.com/miniconda/$base' -o '$HOME_DIR/miniconda3/miniconda.sh'"
    run_sh "bash '$HOME_DIR/miniconda3/miniconda.sh' -b -u -p '$HOME_DIR/miniconda3'"
    run rm -f "$HOME_DIR/miniconda3/miniconda.sh"
    CONDA_BIN="$HOME_DIR/miniconda3/bin/conda"
    if ask "Run 'conda init' for bash + zsh (edits your rc files)?"; then
      run_sh "HOME='$HOME_DIR' '$CONDA_BIN' init bash" || true
      run_sh "HOME='$HOME_DIR' '$CONDA_BIN' init zsh"  || true
    fi
  fi
  [ "$DRY_RUN" -eq 1 ] && return

  if "$CONDA_BIN" env list | awk '{print $1}' | grep -qx "$CONDA_ENV"; then
    ok "conda env '$CONDA_ENV' already exists"
  elif ask "Create a dedicated conda env '$CONDA_ENV' (python $CONDA_PY)? (recommended - keeps daoBase's deps isolated)"; then
    # conda-forge + --override-channels avoids the defaults-channel Terms-of-
    # Service prompt that blocks non-interactive `conda create` on recent conda.
    run "$CONDA_BIN" create -y -n "$CONDA_ENV" -c conda-forge --override-channels "python=$CONDA_PY" \
      || die "conda create failed. If it mentions Terms of Service, run:
    $CONDA_BIN tos accept --override-channels --channel https://repo.anaconda.com/pkgs/main
  or create the env yourself and re-run with --conda-env <name>"
  else
    # Declined the dedicated env: fall back to whatever conda env is already
    # active (or 'base'), asking only which one - not whether to have one at
    # all. --skip-conda remains the way to avoid conda entirely.
    local active="${CONDA_DEFAULT_ENV:-base}"
    ask_value CONDA_ENV "Use which existing conda env instead?" "$active"
    if [ "$CONDA_ENV" != "base" ] && ! "$CONDA_BIN" env list | awk '{print $1}' | grep -qx "$CONDA_ENV"; then
      die "conda env '$CONDA_ENV' does not exist. Create it yourself and re-run with --conda-env '$CONDA_ENV', or re-run and accept the dedicated env."
    fi
    ok "using existing conda env '$CONDA_ENV'"
  fi
}

# run a command in the target env (or plainly if --skip-conda)
env_run() {
  if [ -z "$CONDA_BIN" ]; then run "$@"; return; fi
  run "$CONDA_BIN" run --no-capture-output -n "$CONDA_ENV" "$@"
}
# prefix string to activate the env + dao_env.sh inside a bash -c
env_prefix() {
  local p=". \"$ENV_FILE\" 2>/dev/null || true;"
  [ -n "$CONDA_BIN" ] && p="$p . \"$(conda_root)/etc/profile.d/conda.sh\"; conda activate $CONDA_ENV;"
  printf '%s' "$p"
}

# --------------------------------------------------------------------------
waf_setup() {
  step "waf"
  if [ "$DO_WAF" -eq 0 ]; then info "skipped (--skip-waf)"; return; fi
  command -v waf >/dev/null 2>&1 && { ok "waf on PATH: $(command -v waf)"; return; }
  [ -x "$BIN_DIR/waf" ] && { ok "waf found: $BIN_DIR/waf"; return; }
  local ver="waf-2.0.27"
  ask "Download $ver into $BIN_DIR ?" || die "waf is required"
  run mkdir -p "$BIN_DIR"
  run_sh "curl -fsSL 'https://waf.io/$ver' -o '$BIN_DIR/$ver'"
  run chmod +x "$BIN_DIR/$ver"
  run ln -sf "$ver" "$BIN_DIR/waf"
  ok "installed $BIN_DIR/waf"
}

# Create $1 (and any missing parent dirs) if needed. The default DAOROOT/
# DAODATA now live under /opt/dao, which a fresh box won't have and won't be
# writable by a normal user -- so if the nearest *existing* ancestor isn't
# writable, this uses sudo to create just the missing part of the path, then
# chowns only that newly created subtree back to the current user (never
# anything that already existed, e.g. never `chown -R /opt` itself). After
# that, DAOROOT/DAODATA behave like any normal user-owned directory for the
# rest of the install (waf install, etc. never need sudo).
ensure_owned_dir() {
  local target="$1" existing="$1" missing_top="$1"
  [ -d "$target" ] && [ -w "$target" ] && return 0

  while [ ! -d "$existing" ]; do
    missing_top="$existing"
    existing="$(dirname "$existing")"
  done

  if [ -w "$existing" ]; then
    run mkdir -p "$target"
  else
    ask "$existing is not writable by $(id -un) -- create $missing_top with sudo (owned by $(id -un))?" \
      || die "$target must exist and be writable by $(id -un)"
    run_sh "sudo mkdir -p '$target'"
    run_sh "sudo chown -R '$(id -un):$(id -gn)' '$missing_top'"
  fi
}

# --------------------------------------------------------------------------
daoroot_setup() {
  step "DAOROOT / DAODATA"
  ask_value DAOROOT "Install prefix DAOROOT" "$DAOROOT"
  ask_value DAODATA "Runtime data dir DAODATA" "$DAODATA"
  # Allow the literal "$CONDA_PREFIX" as a convenience: resolve it to the
  # target env's path so libdao lands on that env's loader search path.
  if [ "$DAOROOT" = '$CONDA_PREFIX' ] || [ "$DAOROOT" = '${CONDA_PREFIX}' ]; then
    [ -n "$CONDA_BIN" ] || die "--prefix \$CONDA_PREFIX needs conda (drop --skip-conda)"
    DAOROOT="$(conda_root)/envs/$CONDA_ENV"
    info "resolved DAOROOT -> $DAOROOT"
  fi
  ensure_owned_dir "$DAOROOT"
  ensure_owned_dir "$DAODATA"
  ok "DAOROOT=$DAOROOT"
  ok "DAODATA=$DAODATA"
}

# --------------------------------------------------------------------------
build_waf() {
  step "Build & install daoBase (waf)"
  if [ "$DO_BUILD" -eq 0 ]; then info "skipped (--skip-build)"; return; fi
  local waf="waf"; command -v waf >/dev/null 2>&1 || waf="$BIN_DIR/waf"
  if ! run_sh "$(env_prefix) cd \"$DAO_SRC\" && \"$waf\" distclean 2>/dev/null; \"$waf\" configure --prefix=\"$DAOROOT\" && \"$waf\" && \"$waf\" install"; then    if [ "$OS" = linux ] && [ "$ARCH" = arm64 ]; then
    if [ "$OS" = linux ] && [ "$ARCH" = arm64 ]; then
      warn "build failed. On Linux arm64 this is commonly a libprotobuf/protoc"
      warn "version mismatch in the distro package (see README.md's 'Linux"
      warn "arm64' section) - the known fix is to rebuild abseil-cpp and"
      warn "protobuf from source:"
      warn "  git clone https://github.com/abseil/abseil-cpp.git && cd abseil-cpp"
      warn "  mkdir build && cd build"
      warn "  cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release"
      warn "  make -j\$(nproc) && sudo make install"
      warn "  (then protobuf from source the same way - full commands in README.md)"
    fi
    die "waf build/install failed (see above)"
  fi
}

python_deps() {
  step "Python dependencies (pip)"
  if [ "$DO_PYTHON" -eq 0 ]; then info "skipped (--skip-python)"; return; fi

  # Same set as README.md's "pip install ..." line. protobuf is left unpinned
  # unless the system protoc is old (waf regenerates the *_pb2.py files at
  # build time, so pip's protobuf just needs to be compatible with those).
  local protobuf_spec="protobuf"
  if command -v protoc >/dev/null 2>&1; then
    local pbver; pbver="$(protoc --version 2>/dev/null | awk '{print $2}')"
    case "$pbver" in
      2.*|3.0.*|3.1.*|3.2.0|3.20.*) protobuf_spec="protobuf==3.20.*" ;;
    esac
  fi
  local pkgs="numpy pyzmq $protobuf_spec astropy python-statemachine redis pyyaml \
posix_ipc screeninfo sphinx"
  if [ "$WITH_EXTRAS" -eq 1 ]; then
    pkgs="$pkgs PyQt5 pyqtgraph matplotlib magicplot tqdm click"
  fi

  env_run python -m pip install --upgrade pip
  # shellcheck disable=SC2086
  env_run python -m pip install $pkgs
  if [ -n "${PIP_EXTRA// }" ]; then
    # shellcheck disable=SC2086
    env_run python -m pip install $PIP_EXTRA
  fi
}

# --------------------------------------------------------------------------
write_env_file() {
  step "Environment file: $ENV_FILE"
  [ "$DRY_RUN" -eq 1 ] && { info "(dry-run) would write $ENV_FILE"; return; }

  local conda_block="" mac_block=""
  if [ -n "$CONDA_BIN" ]; then
    conda_block="# conda
if [ -f \"$(conda_root)/etc/profile.d/conda.sh\" ]; then
    . \"$(conda_root)/etc/profile.d/conda.sh\"
    conda activate $CONDA_ENV 2>/dev/null || true
fi
"
  fi
  if [ "$OS" = macos ]; then
    local bp="/opt/homebrew"; [ "$ARCH" = x86_64 ] && bp="/usr/local"
    mac_block="# macOS / Homebrew
export DYLD_LIBRARY_PATH=\"\$DYLD_LIBRARY_PATH:$bp/lib:\$DAOROOT/lib\"
export PKG_CONFIG_PATH=\"\$PKG_CONFIG_PATH:$bp/lib/pkgconfig\"
export CPATH=\"\$CPATH:$bp/include\"
export C_INCLUDE_PATH=\"\$C_INCLUDE_PATH:$bp/include\"
export CPLUS_INCLUDE_PATH=\"\$CPLUS_INCLUDE_PATH:$bp/include\"
"
  fi

  cat > "$ENV_FILE" <<EOF
# dao_env.sh - environment for daoBase. Generated by install.sh on $(date -u +%Y-%m-%dT%H:%M:%SZ).
# Re-run install.sh to regenerate. Safe to edit by hand.

export DAOROOT="$DAOROOT"
export DAODATA="$DAODATA"
export PATH="\$PATH:$BIN_DIR:\$DAOROOT/bin"
export LD_LIBRARY_PATH="\${LD_LIBRARY_PATH:-}:\$DAOROOT/lib:\$DAOROOT/lib64"
export PYTHONPATH="\${PYTHONPATH:-}:\$DAOROOT/python"
export PKG_CONFIG_PATH="\${PKG_CONFIG_PATH:-}:\$DAOROOT/lib/pkgconfig:\$DAOROOT/lib64/pkgconfig"

$mac_block${conda_block}
EOF
  ok "wrote $ENV_FILE"
}

update_shell_rc() {
  step "Shell startup file"
  local ma="# >>> dao environment >>>"
  local mb="# <<< dao environment <<<"
  local snippet="$ma
# Managed by $DAO_SRC/install.sh - edit dao_env.sh, not this block.
[ -f \"$ENV_FILE\" ] && . \"$ENV_FILE\"
$mb"

  local rc
  case "$(basename "${SHELL:-/bin/bash}")" in
    zsh) rc="$HOME_DIR/.zshrc" ;;
    *)   rc="$HOME_DIR/.bashrc"; [ "$OS" = macos ] && rc="$HOME_DIR/.bash_profile" ;;
  esac

  info "Line for $rc:"
  printf '%s\n' "$snippet" | sed 's/^/        /'

  local go=0
  case "$UPDATE_RC" in
    yes) go=1 ;;
    no)  info "skipped (--update-rc no)"; return ;;
    ask) ask "Append it to $rc now? (a .bak copy is kept)" && go=1 ;;
  esac
  [ "$go" -eq 1 ] || { info "not modified"; return; }
  [ "$DRY_RUN" -eq 1 ] && { info "(dry-run) would edit $rc"; return; }

  touch "$rc"
  cp -p "$rc" "$rc.bak.$(date +%Y%m%d%H%M%S)"
  if grep -qF "$ma" "$rc"; then
    local tmp; tmp="$(mktemp)"
    awk -v a="$ma" -v b="$mb" -v s="$snippet" '
      $0==a {print s; skip=1; next} $0==b {skip=0; next} !skip {print}' "$rc" > "$tmp"
    mv "$tmp" "$rc"; ok "refreshed dao block in $rc"
  else
    printf '\n%s\n' "$snippet" >> "$rc"; ok "appended dao block to $rc"
  fi
}

# --------------------------------------------------------------------------
verify() {
  step "Verify"
  [ "$DRY_RUN" -eq 1 ] && { info "(dry-run) skipping"; return; }
  local t="/tmp/dao_install_selftest.im.shm"
  if run_sh "$(env_prefix) python - <<PY
import numpy, daoShm
s = daoShm.shm('$t', numpy.zeros((4, 4), numpy.float32))
s.set_data(numpy.ones((4, 4), numpy.float32))
assert s.get_data().sum() == 16.0
print('daoShm OK  ->', daoShm.__file__)
print('libdao     ->', daoShm.daoLib._name)
PY"; then
    ok "Python import + SHM round-trip works"
  else
    warn "verification failed - open a new shell (or: source $ENV_FILE) and retry"
  fi
}

# --------------------------------------------------------------------------
main() {
  step "daoBase installer"
  info "source tree : $DAO_SRC"
  info "home        : $HOME_DIR"
  info "conda env   : $CONDA_ENV"
  [ "$DRY_RUN" -eq 1 ] && warn "DRY RUN - nothing will be changed"
  detect_platform

  system_deps
  conda_setup
  waf_setup
  daoroot_setup
  write_env_file
  python_deps
  build_waf
  update_shell_rc
  verify

  step "Done"
  info "New shell, or:  source $ENV_FILE"
  [ -n "$CONDA_BIN" ] && info "conda activate $CONDA_ENV"
  info "Then:  python -c 'import daoShm'"
}

main
