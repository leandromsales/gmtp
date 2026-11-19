# Tracked GMTP kernel sources

This directory is the GMTP implementation that the repository tracks. The kernel checkouts under `linux/` are gitignored clones. Do not copy `net/gmtp` back into those clones as regular files.

| Path | Kernel it matches |
| --- | --- |
| `common/net/gmtp/` | The single copy of each file both kernels build. `host/` and `ns3/` symlink to it |
| `host/net/gmtp/` | `linux/host/linux-net-next-latest/` (bootable netdev `net-next`) |
| `ns3/net/gmtp/` | `linux/ns3/linux-net-next-nuse-latest/` (DCE/NUSE library, Linux 5.10.47, `arch/lib`) |

The directory layout under `host/` and `ns3/` is the kernel-relative path. `host/net/gmtp/ipv4.c` is the file the host kernel builds as `net/gmtp/ipv4.c`.

`gmtp.sh --apply` creates one relative symlink per file in both kernel trees. `gmtp.sh --remove` deletes those symlinks and leaves this directory in place. The script refuses to replace a regular file or a symlink that points somewhere else.

Read and apply `.cursor/rules/about-gmpt-source-code.mdc` before editing here. From this file the copy inside this repository is `../.cursor/rules/about-gmpt-source-code.mdc`; when the repository is nested in the thesis workspace the workspace copy is `../../.cursor/rules/about-gmpt-source-code.mdc`. The rule applies inside Cursor and outside it.

Edit a common file in `common/` when both kernels can build the change. `host/` and `ns3/` keep their symlinks. If the change would break the other kernel, fork first, as that rule says: replace the affected tree's symlink with a regular copy, remove the symlink, and write the kernel-specific change only in that copy. `gmtp.h`, `ipv4.c`, and `proto.c` are already separate regular files in `host/` and `ns3/`.

Registration headers that patch shared kernel files stay in each kernel checkout: `include/linux/gmtp.h`, `include/uapi/linux/gmtp.h`, `include/net/netns/gmtp.h`, `include/linux/net.h`, `include/linux/socket.h`, `include/uapi/linux/in.h`, `include/net/net_namespace.h`, `include/net/secure_seq.h`, and `net/core/secure_seq.c`.
