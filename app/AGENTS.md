# GMTP applications

Everything under `gmtp/app/` implements the applications and experiments from Wendell Silva Soares, *Aplicabilidade e Desempenho do Global Media Transmission Protocol (GMTP)* (master's dissertation, UFAL, 2016):

`docs/master-wendell/dissertacao/dissertacao-wendell-final.pdf`

Read `README.md` in this directory before changing a tool. Read `gmtp/AGENTS.md` for repository-wide rules. New prose, comments, and identifiers are English. Leave existing Portuguese source comments as they are unless the edit already touches that line.

These programs assume a Linux kernel with GMTP loaded (`SOCK_GMTP` 7, `IPPROTO_GMTP` 254). Kernel code lives in `gmtp/linux/`, not here.

## Layout

| Path | Role in the dissertation work |
| --- | --- |
| `python/` | Socket-level server, client, relay, and GMTP-MCC calculator used for the virtual-machine experiments (chapter 4.1) and appendices A and B |
| `python/tcp_udp/` | TCP and UDP counterparts of the same send/receive loop, for comparison runs |
| `python/results/` | R scripts over client logs (gitignored output) |
| `gstreamer/gst-plugin-gmtp/` | GStreamer 1.0 elements that carry live media on GMTP sockets |
| `ns-3-dce/` | Direct Code Execution scenarios for the larger simulations (chapter 4.2): relays, dumbbell, WAN, Internet, multicast |
| `kernel_modules/dropper/` | Netfilter module that drops every 10th IPv4 packet, to inject loss |
| `openwrt/luci/` | OpenWRT LuCI administration snapshot for a GMTP router (dissertation figure 20) |

## How to change them

- Keep socket constants in `python/gmtp.py` aligned with `net/gmtp` sockopts (`SOL_GMTP`, flow name, max TX rate, UCC rate, relay role).
- Python scripts are Python 2 (`print` statements, `md5`). Do not convert them to Python 3 in a drive-by edit.
- The GStreamer plugin must link the GStreamer built from `gmtp/tools/gstreamer`. Before `./autogen.sh` or `make`, follow `gmtp/tools/AGENTS.md`: if that checkout has new upstream commits, pull, compile, and point `PKG_CONFIG_PATH` at that prefix.
- ns-3 DCE scenarios are compiled inside the DCE tree (`gmtp/ns-3-dce`, a symlink into `linux/ns3/linux-net-next-nuse-latest`). The stack those scenarios call is `net/gmtp` in that library tree. Pinned DCE revisions stay on the guide in `docs/setup-instructions/`.
- `openwrt/luci/` mixes a saved LuCI page (`thiago-VirtualBox - GMTP - LuCI.xhtml`) with an ExtJS 4 tree and form helpers. Treat the xhtml capture as the GMTP admin UI. The ExtJS helpers are a bundled snapshot. Do not rewrite them unless the task is that UI.
- `kernel_modules/dropper/` is a loss injector, not part of the GMTP protocol. Build it against the same kernel headers as the GMTP modules.
