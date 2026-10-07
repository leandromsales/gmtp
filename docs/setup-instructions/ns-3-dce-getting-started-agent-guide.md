# ns-3 DCE Quick Start — Agent Installation and Usage Guide

Requested source: <https://github.com/direct-code-execution/ns-3-dce/blob/master/doc/source/getting-started.rst>

Reviewed: 2026-10-07 UTC. This guide summarizes the source and supplies a corrected, staged operating procedure. It is not a verbatim RST conversion. Commands have not been build-tested in this review.

## 1. Source identity and scope

The GitHub connector retrieved `doc/source/getting-started.rst` with blob SHA `73b61017797e4f0ea42bc2d8b04ec3c375a0e1ec`. The observed `master` commit was `f6f132a92b17de0068c28e28b76546512de0446f`.

[Commit-pinned source](https://github.com/direct-code-execution/ns-3-dce/blob/f6f132a92b17de0068c28e28b76546512de0446f/doc/source/getting-started.rst).

The source describes two DCE modes: real applications over ns-3's native network stack, or over a Linux stack compiled as a library. It covers Bake installation, a container recipe, manual Waf installation, test execution, UDP/iperf examples, and per-node logs. It identifies Ubuntu 20.04 for DCE 1.12 and Ubuntu 16.04 for 1.11, with roughly 12–15 GB required for a DCE 1.12 working tree. These are the document's tested-version claims, not an assertion that current operating systems are all supported. [S1]

## 2. Important corrections and version boundaries

| Observation | Agent action |
| --- | --- |
| The source's Docker recipe checks out Bake branch `dce-1.12`; the current branch API returned 404 and listed only `master` | Use the inspected Bake snapshot below, which defines the 1.12 modules; record this adaptation |
| Basic mode and Linux mode have different Bake module names | Select the mode before configuration and use separate build workspaces |
| Bake 1.12 definitions select ns-3.35; manual documentation substitution uses ns-3.34 | Do not mix the recipes into one installation |
| Manual instructions contain placeholders such as `GIT_NS3` and `LAST_VERSION` | Resolve real URLs/revisions; these words are not literal clone targets |
| Manual instructions redefine `HOME` and omit some directory transitions | Use a dedicated workspace variable; keep `HOME` unchanged |
| Manual configure/install prefixes are not consistently aligned | Prefer Bake; if using manual Waf, inspect the actual install and lookup paths |
| Official Bake's Linux 1.12 recipe selects `net-next-nuse-4.4.0-fix1` | Do not label it a Linux 5.10 setup |

The separate 2021 modernization fork contains Linux 5.10 work. Installing DCE 1.12 through the official recipe does not automatically select that experimental kernel.

## 3. Agent operating rules

These rules are added by this guide:

1. Read this document, repository instructions, Bake XML, and configure output before executing expensive builds.
2. Pin the entire dependency set, not just DCE. Keep compiler, glibc, ns-3, DCE, Linux-library, and application versions together.
3. Use a disposable Linux VM/container for the older toolchain. Keep DCE's patched libc private to the build; do not install it over system libraries.
4. Build and run as an ordinary user where practical; reserve privilege for system package installation.
5. Separate source, generated output, and experiment archives. Do not run simultaneous simulations in one output directory.
6. Verify application status and traffic, not only the simulator's exit code.
7. Treat warnings, skipped tests, and failed examples as evidence to investigate, not an implicit pass.

Recommended planning allocation: 4 CPUs, 8 GB RAM, and at least 30 GB free disk. These are practical starting estimates, not requirements stated by the source.

## 4. Prepare the environment

Begin inside an Ubuntu 20.04 x86-64 VM or compatible isolated Linux environment. On macOS/Windows, use Linux virtualization or a remote host. ARM operation is not verified here.

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build ccache git \
  ca-certificates wget python3 python3-pip python3-dev \
  libgsl-dev libgtk-3-dev libboost-dev automake bc bison flex gawk \
  libc6-dbg libdb-dev libssl-dev libpcap-dev rsync gdb mercurial \
  indent libsysfs-dev ripgrep
python3 -m pip install --user requests distro

mkdir -p "$HOME/ns3-dce-work"
export DCE_WORKSPACE="$HOME/ns3-dce-work"
cd "$DCE_WORKSPACE"
git clone https://gitlab.com/nsnam/bake.git bake
cd bake
git checkout --detach 0eaa70588c9bdba17d5054b92fcc4b6df3f1e93b
export DCE_BAKE_ROOT="$PWD"
```

This package list adapts the official recipe and adds inspection utilities. Dependency checks below remain authoritative for the chosen configuration. [S1, S2]

## 5. Configure shell paths and choose the stack

Run in each fresh shell, setting `DCE_BAKE_ROOT` to the same absolute checkout path:

```bash
export PATH="$DCE_BAKE_ROOT/build/bin:$DCE_BAKE_ROOT/build/bin_dce:$PATH"
export LD_LIBRARY_PATH="$DCE_BAKE_ROOT/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DCE_PATH="$DCE_BAKE_ROOT/build/bin_dce:$DCE_BAKE_ROOT/build/sbin"
cd "$DCE_BAKE_ROOT"
```

Choose exactly one configuration for this workspace:

```bash
# Native ns-3 network stack under DCE:
./bake.py configure -e dce-ns3-1.12
```

```bash
# Linux network stack under DCE, in a separate/fresh configuration:
./bake.py configure -e dce-linux-1.12
```

If Quagga is explicitly needed, use the documented combination `-e dce-linux-1.12 -e dce-quagga-1.12` after confirming both module definitions. Do not enable it merely for a TCP smoke test.

```bash
./bake.py check
./bake.py show
```

Resolve missing mandatory dependencies before downloading/building. Inspect the modules and URLs selected by the recipe. Relevant inspected definitions select DCE ref `dce-1.12`, ns-3.35, private glibc 2.31, and, in Linux mode, the Linux 4.4 component. [S2]

```bash
./bake.py download
# Record actual source revisions after download and before build.
./bake.py build
```

Some source definitions point to moving branches. Before treating the environment as reproducible, capture their resolved commits or archive checksums. A successful build against a moving dependency today may not repeat tomorrow.

## 6. Validate the installation

```bash
cd "$DCE_BAKE_ROOT/source/ns-3-dce"
./test.py
./waf --run dce-udp-simple
```

Inspect outputs before running another scenario:

```bash
rg --files -g stdout -g stderr -g status -g cmdline .
```

DCE places application records beneath per-node `files-N/var/log/` directories. Interpret `cmdline` as the invocation, `stdout`/`stderr` as process output, and `status` as process lifecycle/exit information. Archive these directories per run. Do not assume every example uses the same node numbering. [S1]

Then test traffic on the configured stack:

```bash
# Available after a suitable basic or Linux-mode build:
./waf --run dce-iperf
```

```bash
# Requires the Linux-mode build:
./waf --run "dce-iperf --stack=linux"
```

Record nonzero received traffic, expected endpoints, application exit status, and any warnings. The sample throughput numbers in the source are illustrations; they are not universal acceptance thresholds.

## 7. Container route

The official RST provides commands intended to run **inside an existing Ubuntu 20.04 container**. It does not provide a complete Dockerfile in that section.

One proposed way to create a persistent container, assuming Docker is already installed and the host directory exists:

```bash
mkdir -p "$HOME/dce-container-work"
docker run --name dce-lab -it \
  --mount "type=bind,src=$HOME/dce-container-work,dst=/work" \
  --workdir /work ubuntu:20.04 bash
```

Inside this container, run package installation without `sudo` if logged in as root. Create an ordinary build user or otherwise account for ownership on the mounted directory. Follow sections 4–6 with a workspace under `/work`, retaining the inspected Bake SHA instead of the unavailable branch. Record the base image digest and installed package versions.

The user-provided source is separate from the GSoC author's `parthpratim27/ns-3-dce:beta` image. Do not confuse the two container routes.

## 8. Manual Waf route: advanced integration only

The source also describes building ns-3, net-next-nuse, and DCE separately. This route requires a compatible set of revisions, consistent install prefixes, library lookup configuration, and, where required, private glibc configuration. A literal copy of the manual section is incomplete.

For agents adapting it:

1. Resolve `replace.txt`: the inspected file substitutes ns-3.34 and SSH repository URLs.
2. Choose HTTPS clone URLs if no SSH credentials are needed.
3. Enter each checkout before invoking its build commands.
4. Use an explicit private prefix such as `$DCE_WORKSPACE/install`; check the revision's `--with-ns3`, `--enable-kernel-stack`, and `--with-glibc` options.
5. Linux 4.4's documented build uses `ARCH=sim`; do not substitute the Linux 5.10 fork's `ARCH=lib` instructions.
6. Record configure commands for every dependency and prove the intended shared libraries are loaded.

Prefer the Bake route unless the task specifically needs this integration work.

## 9. Troubleshooting and completion criteria

| Failure | Check |
| --- | --- |
| Bake `dce-1.12` branch missing | Use the inspected recipe commit; do not change the DCE module name arbitrarily |
| Package/interpreter mismatch | Read exact revision requirements and first configure error |
| Missing `liblinux.so` | Linux-mode selection, build success, library search paths, architecture |
| glibc symbol/loading failure | Private prefix, toolchain compatibility, and selected DCE recipe |
| Waf target missing | Current directory, enabled examples, target name in build definitions |
| iperf completes without useful traffic | DCE process status, routing, addresses, loaded stack, application logs |
| Old logs appear in a new run | Archive and isolate per-run output directories |
| Newer ns-3 lacks Waf | You selected a different generation; do not mix CMake commands with this recipe |

Installation is complete only when its dependency manifest, build outcomes, tests, and at least one appropriate traffic smoke test are recorded. This guide asserts none of those runtime outcomes on the user's behalf.

## Sources

- S1: [Requested quick-start source at inspected commit](https://github.com/direct-code-execution/ns-3-dce/blob/f6f132a92b17de0068c28e28b76546512de0446f/doc/source/getting-started.rst).
- S2: [Official Bake configuration](https://gitlab.com/nsnam/bake/-/blob/0eaa70588c9bdba17d5054b92fcc4b6df3f1e93b/bakeconf.xml); [branch API](https://gitlab.com/api/v4/projects/nsnam%2Fbake/repository/branches).
- [RST substitutions](https://github.com/direct-code-execution/ns-3-dce/blob/f6f132a92b17de0068c28e28b76546512de0446f/doc/source/replace.txt).
- [Rendered DCE manual](https://ns-3-dce.readthedocs.io/en/latest/getting-started.html).
