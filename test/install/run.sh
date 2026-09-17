#!/usr/bin/env bash
#
# test/install/run.sh - build the fresh-distro Docker images in this
# directory and run ../../install.sh inside one (throwaway container,
# nothing touches the host), exercising the real system_deps/conda/waf/
# build/verify flow end to end on a clean, non-root OS user.
#
# The current working tree (including uncommitted changes) is copied into
# the container read-write; the host repo itself is only ever bind-mounted
# read-only, so re-running this after editing install.sh never needs an
# image rebuild and never leaves stray files (dao_env.sh, etc.) on the host.
#
# Each image's non-root "daouser" has real sudo access (see Dockerfile.*),
# so this actually exercises install.sh's sudo-using code paths (system
# packages, the /opt/dao ensure_owned_dir step) instead of skipping them the
# way running as root silently would.
#
# Usage:
#   test/install/run.sh                  # test on every distro below
#   test/install/run.sh ubuntu           # just one
#   test/install/run.sh ubuntu rocky     # a subset
#   test/install/run.sh --shell ubuntu   # drop into a shell in the image instead
#                                         # of running install.sh (debugging)
#   test/install/run.sh ubuntu -- --skip-python --dry-run
#                                         # anything after -- is passed straight
#                                         # through to install.sh
#
# After install.sh finishes (success or failure), you're dropped into an
# interactive shell in the container with ~/.bashrc already sourced, so
# DAOROOT/DAODATA/etc. are set and you can poke around (`echo $DAOROOT`,
# `python -c 'import daoShm'`, ...) before the container is removed on exit.
#
# Requires: Docker (or a Docker-compatible CLI on PATH as `docker`).

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/../.." && pwd)"

ALL_DISTROS="ubuntu rocky"
declare -A IMAGE_TAG=( [ubuntu]="daobase-install-test:ubuntu" [rocky]="daobase-install-test:rocky" )
declare -A DOCKERFILE=( [ubuntu]="$HERE/Dockerfile.ubuntu"   [rocky]="$HERE/Dockerfile.rocky" )

SHELL_MODE=0
DISTROS=()
INSTALL_ARGS=(--yes)

while [ $# -gt 0 ]; do
  case "$1" in
    --shell) SHELL_MODE=1; shift ;;
    --help|-h)
      sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    --) shift; INSTALL_ARGS=("$@"); break ;;
    ubuntu|rocky) DISTROS+=("$1"); shift ;;
    *) echo "unknown arg: $1 (expected: ubuntu, rocky, --shell, --help, or -- <install.sh args>)" >&2; exit 2 ;;
  esac
done
[ ${#DISTROS[@]} -eq 0 ] && DISTROS=($ALL_DISTROS)

command -v docker >/dev/null 2>&1 || {
  echo "error: docker not found on PATH (see README.md for install instructions)" >&2
  exit 1
}

for d in "${DISTROS[@]}"; do
  img="${IMAGE_TAG[$d]}"
  dockerfile="${DOCKERFILE[$d]}"

  printf '\n=== [%s] building %s ===\n' "$d" "$img"
  docker build -f "$dockerfile" -t "$img" "$HERE"

  if [ "$SHELL_MODE" -eq 1 ]; then
    printf '\n=== [%s] shell (repo copied to ~/daoBase, install.sh not run yet) ===\n' "$d"
    docker run --rm -it \
      -v "$REPO_ROOT:/mnt/daoBase-src:ro" \
      "$img" bash -c '
        set -e
        cp -r /mnt/daoBase-src "$HOME/daoBase"
        cd "$HOME/daoBase"
        exec bash
      '
  else
    printf '\n=== [%s] running install.sh %s ===\n' "$d" "${INSTALL_ARGS[*]}"
    docker run --rm -it \
      -v "$REPO_ROOT:/mnt/daoBase-src:ro" \
      "$img" bash -c '
        set -e
        cp -r /mnt/daoBase-src "$HOME/daoBase"
        cd "$HOME/daoBase"
        status=0
        ./install.sh "$@" || status=$?
        echo
        echo "=== install.sh exited $status - dropping into a shell (bashrc loaded, DAOROOT etc. set) ==="
        . "$HOME/.bashrc" 2>/dev/null || true
        exec bash
      ' bash "${INSTALL_ARGS[@]}"
  fi
done
