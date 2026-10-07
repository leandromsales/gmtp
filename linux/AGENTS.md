# GMTP Linux trees

Kernels are split by how they run. New comments, Kconfig text, and identifiers are English.

| Path | Role |
| --- | --- |
| `host/linux-net-next-latest/` | Host kernel. Shallow clone of netdev `net-next` (Linux 7.3.0-rc5, `45ad84d2800e`). No `arch/lib`. `net/gmtp` is ported here from Linux 5.4.21 and built in-tree (`CONFIG_GMTP`). |
| `host/old/linux-5.4.21/` | Previous host tree. Newest GMTP sources before this port, including `gmtp_hashtables.c`. Out-of-tree `gmtp.ko` recipe. |
| `host/old/linux-4.9.50/` | Archive of the 4.9.50 port recorded by `patches/gmtp_linux-4.9.50.patch`. |
| `ns3/linux-net-next-nuse-latest/` | DCE and NUSE library. Shallow clone of https://github.com/ParthPratim/net-next-nuse-5.10.47 (Linux 5.10.47, `4c7510b860629f1840fe76d5333e7fd071fd5698`). `ARCH=lib` builds `liblinux.so`. `net/gmtp` is ported here from Linux 5.4.21. |
| `ns3/old/linux-net-next-nuse-4.1.0-rc4/` | Archive of the former `net-next-sim` tree (Linux 4.1.0-rc4). It already had `arch/lib` and an older `net/gmtp`. |
| `ns3/old/linux-net-nuse-tazaki-latest/` | Reference clone of https://github.com/libos-nuse/net-next-nuse (Linux 4.4.0, last commit 2022-12-13). Not an edit target. |

Do not clone Linus Torvalds's `linux.git`. A pull from netdev `net-next` into that tree does not carry `arch/lib` or this port.

LKL (https://github.com/lkl/linux) is the library project Hajime Tazaki maintained after `net-next-nuse`. The DCE 5.10 modernization tried it and kept LibOS instead: LKL's uniprocessor CPU lock does not preempt the way DCE's lightweight processes require. ns-3 simulations stay on `ns3/linux-net-next-nuse-latest`.

## Where to edit GMTP

Edit both live trees when the protocol changes:

- Host: `host/linux-net-next-latest/net/gmtp`
- ns-3 library: `ns3/linux-net-next-nuse-latest/net/gmtp`

The 5.4.21 tree is the source this port was taken from. Do not keep editing it as the current protocol. The socket, skb, and timer calls were adapted per tree. A later edit in one tree has to be applied in the other.

Registration also touches `include/linux/gmtp.h`, `include/uapi/linux/gmtp.h`, `include/net/netns/gmtp.h`, `include/linux/net.h` (`SOCK_GMTP` 7), `include/linux/socket.h` (`SOL_GMTP` 300), `include/uapi/linux/in.h` (`IPPROTO_GMTP` 254), `include/net/net_namespace.h`, `include/net/secure_seq.h`, and `net/core/secure_seq.c`.

## Modules

On the host tree, `CONFIG_GMTP` (default y when `INET` is on) builds `gmtp.o` and `gmtp_ipv4.o` into the kernel. `CONFIG_GMTP_INTER` builds the relay objects. The same options are built into `liblinux.so` on the ns-3 tree.

The 5.4.21 tree still has the out-of-tree module Makefile that produces `gmtp.ko` and `gmtp_ipv4.ko`.

## Build

Host: configure and compile `host/linux-net-next-latest` on Linux. `CONFIG_GMTP` is selected by default with `INET`.

DCE and NUSE: `make library ARCH=lib` in `ns3/linux-net-next-nuse-latest`. That build clones https://github.com/ParthPratim/linux-libos-tools into `arch/lib/tools`. Build it on Linux. The DCE recipe is in `docs/setup-instructions/`.

The repository symlink `ns-3-dce` points at `ns3/linux-net-next-nuse-latest/tools/testing/libos/buildtop/source/ns-3-dce`, DCE tag `dce-1.12` (`18fb3febeb2b9726551cb68346d936fec04e1ff8`). ns-3.35 and `liblinux.so` are still produced by Bake on Linux.

`patches/` is an archive. Do not apply those files to update a tree.

These clones were checked out on macOS, which drops one file from case-colliding netfilter headers. Build the kernels on Linux.
