# GMTP Linux trees

Read and apply `.cursor/rules/about-gmpt-source-code.mdc` before editing these trees. From this file the copy inside this repository is `../.cursor/rules/about-gmpt-source-code.mdc`; when the repository is nested in the thesis workspace the workspace copy is `../../.cursor/rules/about-gmpt-source-code.mdc`. The rule applies inside Cursor and outside it. This file does not replace it.

Kernels are split by how they run. New comments, Kconfig text, and identifiers are English.

| Path | Role |
| --- | --- |
| `host/linux-net-next-latest/` | Host kernel. Shallow clone of netdev `net-next` (Linux 7.3.0-rc5, `45ad84d2800e`). No `arch/lib`. `net/gmtp` is symlinked from `gmtp/host` by `gmtp/gmtp.sh --apply` and built in-tree (`CONFIG_GMTP`). |
| `host/old/linux-5.4.21/` | Previous host tree. Newest GMTP sources before this port, including `gmtp_hashtables.c`. Out-of-tree `gmtp.ko` recipe. |
| `host/old/linux-4.9.50/` | Archive of the 4.9.50 port recorded by `patches/gmtp_linux-4.9.50.patch`. |
| `ns3/linux-net-next-nuse-latest/` | DCE and NUSE library. Shallow clone of https://github.com/ParthPratim/net-next-nuse-5.10.47 (Linux 5.10.47, `4c7510b860629f1840fe76d5333e7fd071fd5698`). `ARCH=lib` builds `liblinux.so`. `net/gmtp` is symlinked from `gmtp/ns3` by `gmtp/gmtp.sh --apply`. |
| `ns3/old/linux-net-next-nuse-4.1.0-rc4/` | Archive of the former `net-next-sim` tree (Linux 4.1.0-rc4). It already had `arch/lib` and an older `net/gmtp`. |
| `ns3/old/linux-net-nuse-tazaki-latest/` | Reference clone of https://github.com/libos-nuse/net-next-nuse (Linux 4.4.0, last commit 2022-12-13). Not an edit target. |

Do not clone Linus Torvalds's `linux.git`. A pull from netdev `net-next` into that tree does not carry `arch/lib` or this port.

LKL (https://github.com/lkl/linux) is the library project Hajime Tazaki maintained after `net-next-nuse`. The DCE 5.10 modernization tried it and kept LibOS instead: LKL's uniprocessor CPU lock does not preempt the way DCE's lightweight processes require. ns-3 simulations stay on `ns3/linux-net-next-nuse-latest`.



## Checkout

The live kernel clones are gitignored. Recreate them from this repository with:

```sh
./linux/checkout-trees.sh
```

That script fetches the pinned SHAs (`45ad84d2800e` for the host tree,
`4c7510b860629f1840fe76d5333e7fd071fd5698` for the ns-3 tree) with
`--filter=blob:none`, applies `linux/patches/<tree>/registration.diff`,
copies the three GMTP headers from the same directory, and runs
`gmtp/gmtp.sh --apply` so `net/gmtp` is a set of relative symlinks into
`gmtp/host` and `gmtp/ns3`. If kernel.org refuses a fetch by SHA, the
script tries the googlesource mirror of netdev `net-next`, then a
kernel.org snapshot tarball. Do not use `torvalds/linux.git`. A nested
`ns-3-dce` checkout under the ns-3 tree is not created here; follow
`docs/setup-instructions/`. See `linux/patches/README.md`.

Pass `--host` or `--ns3` to update only one tree, `--force` to replace a
checkout that is not at the pinned SHA, and `--skip-link` to stop before
`gmtp.sh --apply`.

## Where to edit GMTP

Edit the tracked copies when the protocol changes, then `gmtp/gmtp.sh --apply` if the kernel trees are not already linked:

- Host: `gmtp/host/net/gmtp`
- ns-3 library: `gmtp/ns3/net/gmtp`

The paths under `host/linux-net-next-latest/net/gmtp` and `ns3/linux-net-next-nuse-latest/net/gmtp` are those symlinks. The 5.4.21 tree is the source this port was taken from. Do not keep editing it as the current protocol. Files that both kernels build are symlinks to `gmtp/common`. When an edit would break the other kernel, follow the fork steps in `.cursor/rules/about-gmpt-source-code.mdc`: replace that tree's symlink with its own copy and remove the symlink. Do not paste the breaking edit into the other tree.

Registration also touches `include/linux/gmtp.h`, `include/uapi/linux/gmtp.h`, `include/net/netns/gmtp.h`, `include/linux/net.h` (`SOCK_GMTP` 7), `include/linux/socket.h` (`SOL_GMTP` 300), `include/uapi/linux/in.h` (`IPPROTO_GMTP` 254), `include/net/net_namespace.h`, `include/net/secure_seq.h`, and `net/core/secure_seq.c`.

## Modules

On the host tree, `CONFIG_GMTP` (default y when `INET` is on) builds `gmtp.o` and `gmtp_ipv4.o` into the kernel. `CONFIG_GMTP_INTER` builds the relay objects. The same options are built into `liblinux.so` on the ns-3 tree.

The 5.4.21 tree still has the out-of-tree module Makefile that produces `gmtp.ko` and `gmtp_ipv4.ko`.

## Build

Host: configure and compile `host/linux-net-next-latest` on Linux. `CONFIG_GMTP` is selected by default with `INET`.

DCE and NUSE: `make library ARCH=lib` in `ns3/linux-net-next-nuse-latest`. That build clones https://github.com/ParthPratim/linux-libos-tools into `arch/lib/tools`. Build it on Linux. The DCE recipe is in `docs/setup-instructions/`.

DCE tag `dce-1.12` (`18fb3febeb2b9726551cb68346d936fec04e1ff8`) is checked out at `ns3/linux-net-next-nuse-latest/tools/testing/libos/buildtop/source/ns-3-dce`. `app/ns-3-dce` calls `./waf` there. There is no `ns-3-dce` symlink at the repository root. ns-3.35 and `liblinux.so` are still produced by Bake on Linux.

`patches/` is an archive. Do not apply those files to update a tree.

These clones were checked out on macOS, which drops one file from case-colliding netfilter headers. Build the kernels on Linux.
