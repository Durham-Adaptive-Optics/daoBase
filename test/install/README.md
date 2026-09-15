# Testing `install.sh` on a fresh distribution

Docker images + a runner script for exercising [`../../install.sh`](../../install.sh)
end to end (system packages, conda, waf, build, shell-rc, verify) on a clean
OS, instead of trusting it against whatever is already installed on your dev
machine.

## Install Docker (if you don't have it)

Ubuntu / Debian, via Docker's official apt repo:

```bash
sudo apt-get update
sudo apt-get install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# run docker without sudo (log out/in, or `newgrp docker`, afterwards)
sudo usermod -aG docker "$USER"

docker run hello-world   # sanity check
```

Fedora / RHEL / Rocky:

```bash
sudo dnf -y install dnf-plugins-core
sudo dnf config-manager --add-repo https://download.docker.com/linux/fedora/docker-ce.repo
sudo dnf install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
sudo systemctl enable --now docker
sudo usermod -aG docker "$USER"
```

macOS: install [Docker Desktop](https://docs.docker.com/desktop/setup/install/mac-install/).

## Usage

```bash
test/install/run.sh                  # test on every distro below (ubuntu + rocky)
test/install/run.sh ubuntu           # just one
test/install/run.sh ubuntu rocky     # a subset
test/install/run.sh --shell ubuntu   # drop into a shell in the image instead of
                                      # running install.sh (debugging)
test/install/run.sh ubuntu -- --skip-python --dry-run
                                      # anything after -- is passed straight
                                      # through to install.sh
```

Each run: builds the distro's image (cached after the first time — it's just
bootstrap tools, see below), starts a throwaway container, copies the
*current* working tree into it (uncommitted changes included), and runs
`./install.sh --yes` inside as **`daouser`**, a non-root user with real sudo
access (password `!daousr4u`, also passwordless via `NOPASSWD` so the
non-interactive `docker run` doesn't hang on a prompt). The host repo is only
ever bind-mounted **read-only**, so nothing the installer writes (`dao_env.sh`,
`~/.bashrc`, `~/miniconda3`, `$DAOROOT`, ...) ever touches your actual machine
— the whole container is discarded (`--rm`) when it exits.

When `install.sh` finishes (pass or fail), you land in an interactive shell
inside the container with `~/.bashrc` already sourced — `$DAOROOT`,
`$DAODATA`, and the conda env are set, so you can immediately run things like
`echo $DAOROOT` or `python -c 'import daoShm'` before exiting (which removes
the container).

## What the images are

`Dockerfile.ubuntu` / `Dockerfile.rocky` are deliberately close to a bare OS
install — just enough (`bash`, `curl`, `ca-certificates`, `sudo`, plus the
`daouser` account) to fetch and run `install.sh` itself. Everything else
(build toolchain, zeromq, protobuf, conda, waf, ...) is left for
`install.sh`'s own `system_deps()`/`conda_setup()`/`waf_setup()` steps to
install, on purpose: pre-installing them in the image would defeat the point
of testing the installer's own bootstrap logic. Ubuntu exercises the `apt`
branch of `system_deps()`; Rocky exercises `dnf`. Running as non-root
`daouser` (rather than the container's default root) also means the
installer's `sudo`-using steps — system packages, and creating `/opt/dao` the
first time — actually get exercised instead of silently short-circuiting.

## Interpreting results

A clean run ends with `install.sh`'s own `Verify` step: a real
`import daoShm` + shared-memory round-trip inside the container. If that
prints `daoShm OK` and `libdao -> ...`, the installer worked end to end on
that distro. Anything short of that (a step earlier in the log failing, or
`verify` printing its warning) means `install.sh` needs a fix before trusting
it on a real fresh machine.
