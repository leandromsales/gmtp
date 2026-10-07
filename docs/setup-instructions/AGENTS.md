# Setup instructions

This folder tells agents how to set up ns-3 and Direct Code Execution (DCE) for GMTP work. Each `*-agent-guide.md` file is a reviewed operating guide for one source. They are not interchangeable recipes. Read this file, then read only the guide that matches the task, then follow that guide.

`README.md` lists the upstream repositories and the ns-3 model docs. It does not replace a guide.

## Which guide to use

| Task | Read |
| --- | --- |
| Use the ns-3 tree DCE pins for GMTP simulations | `ns-3-setup-agent-guide.md` |
| Install official DCE 1.12 (native ns-3 stack or Linux-as-library) | `ns-3-dce-getting-started-agent-guide.md` |
| Reproduce the 2021 Linux 5.10 / glibc 2.31 modernization | `ns-3-dce-modernization-agent-guide.md` |
| Reproduce the WNS3 2022 DCE paper figures | `revitalizing-ns-3-dce-acm-agent-guide.md` |
| Reproduce the L4S / TCP Prague preprint in native ns-3 | `ns-3-l4s-arxiv-2603-20166-agent-guide.md` |
| Compare ns-3 TCP with Linux (Reno, PRR, CUBIC) under DCE | `ns-3-tcp-validation-agent-guide.md` |
| Background note on the Linux 5.10 DCE port | `ns3-dce-linux-kernel-agent-guide.md` |

The library kernel for that modernization is the gitignored clone `gmtp/linux/ns3/linux-net-next-nuse-latest` (https://github.com/ParthPratim/net-next-nuse-5.10.47, `4c7510b860629f1840fe76d5333e7fd071fd5698`), with `net/gmtp` ported from Linux 5.4.21. Official DCE 1.12 still selects a Linux 4.4 library. Linux 5.10 is the separate modernization fork, not the default upstream recipe.

## How to use a guide

1. Name the goal before downloading anything: local ns-3, official DCE 1.12, Linux 5.10 modernization, a paper figure, L4S, or TCP validation.
2. Read that guide's corrections and version table before the commands. Several upstream docs are incomplete or point at the wrong module name.
3. Keep one toolchain in one workspace. Do not mix Bake, Waf, and CMake recipes, and do not mix DCE 1.11 with 1.12 or ns-3.35 with ns-3.46.
4. Pin the revisions the guide records. A branch name is not a lock. Write down the SHAs, OS image, compiler, and local patches you actually used.
5. Build in an isolated Linux environment. Keep DCE's patched glibc private. Do not install it over the host libc, and do not apply a simulated-kernel patch to the host kernel.
6. Run the guide's smoke test and inspect application logs and traffic. A zero exit code is not enough.
7. Stop when the guide says a dependency is missing or a paper was not fully read. Do not relabel a port or a partial run as the historical result.

L4S is a native ns-3 model. Do not install DCE for that task.

## Local ns-3 checkout

There is no `gmtp/tools/ns-3-dev-git/`. GMTP simulations use Direct Code Execution and the ns-3 revision that guide pins. `ns-3-setup-agent-guide.md` is that policy. A DCE, L4S, or TCP-validation guide that pins another ns-3 revision uses its own checkout.

New text and code in this repository are English. See `gmtp/AGENTS.md`.
