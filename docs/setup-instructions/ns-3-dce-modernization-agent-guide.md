# DCE Modernization — Project and Agent Setup Guide

Source: <https://ns-3-dce-linux-upgrade.github.io/>

Reviewed: 2026-10-07 UTC. This document summarizes the project and adds operational instructions. Repository files and metadata were inspected; builds and simulations were not executed. Recommendations and proposed repairs are not upstream-tested fixes.

## 1. Project overview

Parth Pratim Chatterjee's GSoC 2021 project, mentored by Tom Henderson, Vivek Jain, and Apoorva Bhargava, addressed DCE's aging environment. The work covered Ubuntu 20.04 compatibility through a custom glibc 2.31 build, a Linux 5.10.47 library port, renewed Python bindings/API scanning, CI maintenance, and Docker packaging. The author reports working network applications over the newer kernel, with ethtool still incomplete. LKL was investigated but scheduler and synchronization constraints led to continued LibOS work. Proposed extensions included BBRv2 and flent-based experiments. [S1]

Interpret “modernization” relative to 2021. It does not establish compatibility with every later Ubuntu, glibc, Linux kernel, or ns-3 release.

## 2. Components and observed upstream state

Repository APIs were checked on the review date. An open PR is not proof its functionality is entirely absent elsewhere; a closed, unmerged MR is not a merged implementation.

| Deliverable | Primary reference | Observed state |
| --- | --- | --- |
| Linux 5.10 port | [net-next-nuse PR 56](https://github.com/libos-nuse/net-next-nuse/pull/56) | Open, unmerged |
| Custom glibc support | [DCE PR 117](https://github.com/direct-code-execution/ns-3-dce/pull/117) | Open, unmerged |
| Bake glibc recipe | [Bake MR 9](https://gitlab.com/nsnam/bake/-/merge_requests/9) | Closed, not recorded as merged |
| Python bindings | [DCE PR 121](https://github.com/direct-code-execution/ns-3-dce/pull/121) | Merged; commit `5a25643fc4c59b331aa17521633f9275a57ec7ba` |
| API scanning | [DCE PR 129](https://github.com/direct-code-execution/ns-3-dce/pull/129) | Open, unmerged |
| CircleCI repair | [DCE PR 115](https://github.com/direct-code-execution/ns-3-dce/pull/115) | Merged |
| GitHub Actions | [DCE PR 118](https://github.com/direct-code-execution/ns-3-dce/pull/118) | Merged on 2026-08-23 |
| Docker prototype | [dce-docker-beta](https://github.com/ParthPratim/dce-docker-beta) | Accessible source; image pull not tested |

Useful inspected revisions:

| Repository/ref | SHA |
| --- | --- |
| `ParthPratim1/bake`, `dce-1.12-linux-5.10` | `29af6127b36f54b2fb144b844b43c26ed98c8d08` |
| `ParthPratim1/bake`, `docker` | `4ca262e29c33daefc73ac1e1e59eb33fe81d2774` |
| `ParthPratim/ns-3-dce`, PR 117 head | `87a3688d22d2824ca0da4c787dbde07ecd9e3abe` |
| `ParthPratim/net-next-nuse-5.10.47`, observed HEAD | `4c7510b860629f1840fe76d5333e7fd071fd5698` |
| `ParthPratim/dce-docker-beta`, `main` | `5a0021442644fa8b67c0dfcb8f5651d62542c2a1` |

The kernel PR head belongs to a different fork/ref than the standalone `net-next-nuse-5.10.47` repository. Do not interchange those snapshots without comparison.

## 3. What agents must distinguish

| Layer | Purpose | Evidence to retain |
| --- | --- | --- |
| Host OS / VM / container | Runs the experiment process | OS image, CPU architecture, host kernel |
| ns-3 | Topology and discrete-event scheduling | Source SHA and build profile |
| DCE | Executes compatible real applications in simulation | Source SHA, loader, configure flags |
| Private glibc | Supports DCE's historical interception/loading behavior | Patch, source SHA, install prefix |
| Linux-as-library | Supplies the simulated network stack | Kernel SHA, configuration, library hash |
| Bake | Downloads, configures, builds dependencies | Recipe SHA and local modifications |

Agent rules added by this guide:

1. Choose historical reproduction or forward porting explicitly.
2. Work in an isolated environment. Keep the modified glibc under the experiment's private prefix; never replace the host libc.
3. Pin every dependency, including helper repositories fetched during compilation. A pinned Bake checkout can still reference moving branches.
4. Read the selected recipe before building. Verify module names, directories, configure flags, and copy/symlink destinations.
5. Keep baseline and experimental kernel outputs in separate workspaces. Do not overwrite an unrecorded `liblinux.so`.
6. Run a small traffic test before a large experiment; examine DCE application logs and received bytes.
7. Do not equate a successful kernel-library build with complete protocol support.

## 4. Environment preparation

Use an Ubuntu 20.04 x86-64 VM as a historical starting point. This is a reproduction recommendation based on the documented environment, not a verified installation here. Plan roughly 4 CPUs, 8 GB RAM, and 30 GB disk, adjusting to build requirements. Use Linux virtualization or a remote Linux host on macOS/Windows; ARM compatibility needs separate validation.

Install dependencies inside the VM. This adapts the DCE quick-start package list. [S6]

```bash
sudo apt-get update
sudo apt-get install -y build-essential git ca-certificates cmake ninja-build \
  ccache wget python3 python3-pip python3-dev automake bc bison flex gawk \
  libc6-dbg libdb-dev libssl-dev libpcap-dev libgsl-dev libgtk-3-dev \
  libboost-dev mercurial indent libsysfs-dev rsync gdb ripgrep
python3 -m pip install --user requests distro
mkdir -p "$HOME/dce-modernization-work"
export DCE_MOD_ROOT="$HOME/dce-modernization-work"
```

Use configuration output to resolve additional dependencies; do not assume modern replacement packages are compatible with the historical toolchain.

## 5. Native Linux 5.10 route: inspect and repair before building

The project wiki's later reproduction section points to the `dce-1.12-linux-5.10` Bake branch and module `dce-linux-1.12`. [S2]

```bash
cd "$DCE_MOD_ROOT"
git clone --branch dce-1.12-linux-5.10 --single-branch \
  https://gitlab.com/ParthPratim1/bake.git bake-linux510
cd bake-linux510
git checkout --detach 29af6127b36f54b2fb144b844b43c26ed98c8d08
git switch -c local-reproduction
export DCE_BAKE_ROOT="$PWD"
rg -n 'dce-linux-1.12|net-next-nuse-5.10|ARCH=|iproute2-5.10|with-glibc' bakeconf.xml
```

**Verified recipe inconsistency:** the inspected XML defines `net-next-nuse-5.10.47`, but `dce-linux-1.12` depends on `net-next-nuse-5.10.0` and uses that directory in `--enable-kernel-stack`. [S3]

Proposed local repair, requiring validation: change the dependency and the corresponding kernel source-directory argument in the `dce-linux-1.12` module to the defined `5.10.47` component. Do not blindly replace every `5.10.0` occurrence: `iproute2-5.10.0` is a separate dependency. Inspect `git diff` and retain the patch.

The source also selects DCE's `glibc-build` branch and ns-3.35. Record and pin downloaded SHAs. Inspect the kernel-header dependency separately from the simulated kernel; they serve different purposes.

After checking/repairing the recipe:

```bash
export PATH="$DCE_BAKE_ROOT/build/bin:$DCE_BAKE_ROOT/build/bin_dce:$PATH"
export LD_LIBRARY_PATH="$DCE_BAKE_ROOT/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DCE_PATH="$DCE_BAKE_ROOT/build/bin_dce:$DCE_BAKE_ROOT/build/sbin"
./bake.py configure -e dce-linux-1.12
./bake.py check
./bake.py show
# Continue only after resolving required dependencies and unexpected selections.
./bake.py download
./bake.py build
```

### Kernel build details

The website's uppercase `ARCH=LIB` differs from the inspected source's lowercase `arch/lib` and Bake recipe. Use the checkout's build definitions. The inspected kernel Makefile also fetches `ParthPratim/linux-libos-tools` during the build. Pin that helper too. [S3, S4]

For a separately checked-out kernel, after satisfying its dependencies:

```bash
make defconfig ARCH=lib
make library ARCH=lib
```

The Bake recipe expects `arch/lib/tools/libsim-linux-5.10.47.so` and installs it in `build/bin_dce/` with `liblinux.so` as a symlink. The website shows a different spelling and destination. Verify actual generated artifacts and runtime lookup before selecting one; do not infer success from a filename alone.

## 6. Docker alternative

The prototype repository uses an existing image, `parthpratim27/ns-3-dce:beta`; the inspected tree contains no Dockerfile. Its Compose file mounts host `./bake` at `/home/bake` and sets `DCE_WITH_GLIBC=/bake-utils/glibc`. Pin the image digest if it remains available. [S5]

```bash
cd "$DCE_MOD_ROOT"
git clone https://github.com/ParthPratim/dce-docker-beta.git docker-prototype
cd docker-prototype
git checkout --detach 5a0021442644fa8b67c0dfcb8f5651d62542c2a1
docker compose config
docker compose pull
docker compose up -d
docker exec -it ns-3-dce /bin/bash
```

This adapts the README's legacy `docker-compose` spelling to Compose v2. Confirm compatibility locally. The image was not pulled during this review. If unavailable, use the native recipe; do not claim this repository can rebuild it from a provided Dockerfile.

Inside a fresh container workspace, the README initializes `/home/bake` from the author's `docker` Bake branch. Inspect an existing checkout before doing so; do not reinitialize or replace someone else's work. Then configure `dce-linux-dev`, check dependencies, download, build, and run `./test.py` from `source/ns-3-dce`. The Docker recipe does not by itself prove Linux 5.10 is selected; inspect its dependency graph and loaded library.

## 7. Python bindings and validation

Python bindings and API scanning are distinct deliverables. Confirm their presence in the selected revision before using them. API scanning additionally needs the compatible scanner toolchain, including PyBindGen/CastXML as described by the project. [S1, PR 121, PR 129]

```bash
cd "$DCE_BAKE_ROOT/source/ns-3-dce"
./waf --help
./test.py
./waf --run dce-udp-simple
./waf --run "dce-iperf --stack=linux"
```

Where the selected revision actually supports them, use `./waf --apiscan` to regenerate API descriptions and `./waf --pyrun "path/to/verified-example.py"` to run an existing Python simulation. Python orchestration of ns-3 is not the same capability as executing arbitrary Python applications under DCE.

Acceptance checklist: successful builds; recorded test outcomes including skips; expected simulated kernel loaded; nonzero traffic received; valid per-application logs; and a complete environment manifest. Inspect loader errors, missing symbols, timeouts, and one-way traffic before interpreting performance.

## 8. Handoff and limitations

Deliver the selected route, exact SHAs, local recipe repairs, compiler/Python versions, private glibc prefix, kernel configuration/hash, image digest if used, commands, logs, and test outcomes. No benchmark or protocol-alignment result is established by this guide.

The linked Google design document was discoverable, but its body was not retrievable through the public page in this review. Its contents are not summarized here.

## Sources

- S1: [GSoC final project site](https://ns-3-dce-linux-upgrade.github.io/).
- S2: [Project wiki and publication reproduction notes](https://www.nsnam.org/wiki/GSOC2021DCE).
- S3: [Author Bake recipe](https://gitlab.com/ParthPratim1/bake/-/blob/dce-1.12-linux-5.10/bakeconf.xml), branch SHA recorded above.
- S4: [Kernel architecture Makefile](https://github.com/ParthPratim/net-next-nuse-5.10.47/blob/4c7510b860629f1840fe76d5333e7fd071fd5698/arch/lib/Makefile).
- S5: [Docker prototype](https://github.com/ParthPratim/dce-docker-beta/tree/5a0021442644fa8b67c0dfcb8f5651d62542c2a1).
- S6: [Official DCE quick start](https://ns-3-dce.readthedocs.io/en/latest/getting-started.html).
- [Design document, content not retrieved](https://docs.google.com/document/d/1o3xsukgDN9e4-q8n6KbLDX2c9fTxKIhRhimDsn4ivr8/edit).

Mutable branch names, PR state, and documentation must be rechecked before a new build.
