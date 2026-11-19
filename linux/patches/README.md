# Live-tree registration patches

These files reproduce the GMTP registration that the Mac working copies
carried on top of the pinned kernel bases. They are the supported way to
bring a fresh clone of each live tree to the same state as
`linux/checkout-trees.sh`.

The historical 4.9.50 and 2015 diffs stay in the repository-root
`patches/` directory. Do not apply those archive files to these trees.

| Directory | Tree | Pinned base |
| --- | --- | --- |
| `host-linux-net-next-latest/` | `linux/host/linux-net-next-latest` | `45ad84d2800e4a092fb8d96006a533b2d0ab13f6` (netdev `net-next`, Linux 7.3.0-rc5) |
| `ns3-linux-net-next-nuse-latest/` | `linux/ns3/linux-net-next-nuse-latest` | `4c7510b860629f1840fe76d5333e7fd071fd5698` (https://github.com/ParthPratim/net-next-nuse-5.10.47.git) |

Each directory contains:

- `registration.diff` — `SOCK_GMTP` 7, `SOL_GMTP` 300, `IPPROTO_GMTP` 254, `netns_gmtp`, `secure_gmtp_sequence_number`, and `net/Kconfig` plus `net/Makefile` hooks.
- `include/linux/gmtp.h`, `include/uapi/linux/gmtp.h`, `include/net/netns/gmtp.h` — registration headers that are new files, not patches of upstream ones.

`net/gmtp` itself is not stored here. After the patches and headers are
applied, `gmtp/gmtp.sh --apply` creates relative symlinks from each
kernel tree into the tracked sources under `gmtp/{host,ns3}/net/gmtp`.

Preferred checkout:

```sh
./linux/checkout-trees.sh
```

The script uses `git fetch --filter=blob:none` (shallow when the host
allows it). If kernel.org refuses a fetch by SHA, it tries
`https://kernel.googlesource.com/pub/scm/linux/kernel/git/netdev/net-next`
and then the kernel.org snapshot tarball for that commit. Do not use
`torvalds/linux.git` as a substitute: it does not carry this port or
`arch/lib`. A nested `ns-3-dce` clone that may exist under the ns-3
tree's Bake `buildtop` is not part of this repository and is not
created by the checkout script; see `docs/setup-instructions/`.
