# GMTP application examples

Sample applications and experiment tools for the Global Media Transmission Protocol (GMTP).

They implement the user-space side of Wendell Silva Soares, *Aplicabilidade e Desempenho do Global Media Transmission Protocol (GMTP)*, master's dissertation, Universidade Federal de Alagoas, 2016. The PDF is `docs/master-wendell/dissertacao/dissertacao-wendell-final.pdf`. Chapter 3 describes the Linux implementation (GMTP-Intra, GMTP-Inter, relays, reporters, GMTP-MCC, GMTP-UCC). Chapter 4 describes the virtual-machine experiments and the larger ns-3 DCE simulations. Appendix A is the socket client and server. Appendix B is the socket options. Figure 20 is the OpenWRT administration screen.

Load the GMTP kernel modules before running the socket or GStreamer programs. See the repository `README.md` and `linux/AGENTS.md`.

## Requirements

- Linux with `gmtp.ko` and `gmtp_ipv4.ko` installed. Relay nodes also load `gmtp-inter`.
- Python 2 for `app/python`. The scripts use Python 2 `print` statements and the `md5` module.
- GStreamer built from `tools/gstreamer` when building `gst-plugin-gmtp`. Follow `tools/AGENTS.md` before that build.
- ns-3 Direct Code Execution for `app/ns-3-dce`. Follow `docs/setup-instructions/AGENTS.md` for the DCE revision. The helper scripts call `./waf` in the `ns-3-dce` tree linked at the repository root.

## Python sockets

```sh
cd app/python
```

`gmtp.py` is the shared helper. It sets `SOCK_GMTP = 7`, `IPPROTO_GMTP = 254`, `SOL_GMTP = 300`, and the sockopt numbers for flow name, maximum TX rate, UCC TX rate, MSS, server RTT, time-wait, pull, relay role, and relay enabled.

### server.py

Binds a GMTP socket and sends a repeated text payload (`Welcome to the jungle!`) for `-n` packets (default 1000). Prints packet size, elapsed time, and send rate.

```sh
./server.py -i eth1 -p 12345
./server.py -a 10.0.1.101 -p 12345 -n 1000
```

`-a` takes precedence over `-i`. Default interface is `eth1`. Default port is `12345`.

### client.py

Connects a GMTP socket, reads the payload, and writes a tab-separated log under `logs/`. Columns include sequence, time, size, elapsed time, and instant rate. The terminal prints a summary every 100 messages.

```sh
./client.py -a 10.0.1.101 -p 12345
```

### relay.py

Opens a GMTP socket, sets `GMTP_SOCKOPT_ROLE_RELAY`, then sets `GMTP_SOCKOPT_RELAY_ENABLED` from the first argument (default `1`).

```sh
./relay.py 1
```

### mcc.py

Evaluates the TCP-friendly GMTP-MCC rate equation used in the dissertation:

```text
X = s / (R * f(p))
f(p) = sqrt(2*p/3) + 12 * sqrt(3*p/8) * (p + 32*p^3)
```

Arguments are packet size `s` in bytes (default 203), RTT in microseconds (divided by 1e6 before the formula, default 0.0001 s), and loss event rate `p` (default 0.5).

```sh
./mcc.py 203 100 0.01
```

### TCP and UDP counterparts

`python/tcp_udp/` repeats the same send/receive loop on TCP (`SOCK_STREAM`) and UDP (`SOCK_DGRAM`): `server_tcp.py`, `client_tcp.py`, `server_udp.py`, `client_udp.py`, plus `tcp_clients.sh` and `udp_clients.sh`. Client logs go to `python/logs/`. `client_tcp.py` imports modules it does not use (`twisted`, `ubuntu_sso`, `dbus`). Those imports are leftover and are not part of the socket path.

### Logs and R analysis

Client logs are text files in `python/logs/`. `python/results/result_analysis.R` summarizes reception rate. Generated results under `python/results/` are gitignored.

## GStreamer plugin

`gstreamer/gst-plugin-gmtp` registers four elements:

| Element | Direction |
| --- | --- |
| `gmtpserversink` | Send media on a GMTP server socket |
| `gmtpclientsrc` | Receive media on a GMTP client socket |
| `gmtpserversrc` | Read from a GMTP server socket into a pipeline |
| `gmtpclientsink` | Write a pipeline into a GMTP client socket |

Build GStreamer from `tools/gstreamer` first (`tools/AGENTS.md`). Then point this plugin at that prefix:

```sh
export GMTP_ROOT="$(pwd)"
export PKG_CONFIG_PATH="$GMTP_ROOT/tools/gstreamer/prefix/lib/pkgconfig"
export LD_LIBRARY_PATH="$GMTP_ROOT/tools/gstreamer/prefix/lib"
cd app/gstreamer/gst-plugin-gmtp
./autogen.sh && ./configure --prefix="$GMTP_ROOT/tools/gstreamer/prefix" && make
```

Serve and play an MP3:

```sh
gst-launch-1.0 --gst-plugin-path=$PWD/src -v \
  filesrc location=~/audiofile.mp3 ! mpegaudioparse ! gmtpserversink port=12345

gst-launch-1.0 --gst-plugin-path=<plugin-src> -v \
  gmtpclientsrc host=10.10.0.101 port=12345 ! decodebin ! audioconvert ! pulsesink
```

More pipelines are in `gstreamer/gst-plugin-gmtp/README.md`.

## ns-3 DCE scenarios

`ns-3-dce/gmtp/` contains the simulation programs for chapter 4.2:

- `dce-gmtp-simple.cc`, `dce-gmtp-relay.cc`, `dce-gmtp-dumbbell.cc`, `dce-gmtp-wan.cc`, `dce-gmtp-internet.cc`
- `dce-gmtp-master.cc` and `dce-gmtp-master - mutlipath.cc` (relay and client counts)
- `dce-gmtp-mpi.cc`, `dce-multicast-master.cc`
- `gmtp-server.cc`, `gmtp-client.cc`, `gmtp-inter.cc`
- `udp-mcst-server.cc`, `udp-mcst-client.cc` for the UDP multicast baseline

`run_simulation` repeats `dce-gmtp-master` and copies logs:

```sh
cd app/ns-3-dce
./run_simulation <times> <nrelays> <nclients> <dest>
```

The script `cd`s to the repository `ns-3-dce` tree and runs `./waf`. `run_all` batches scenarios. `copy_results` and `cleangmtp` move and delete run files. `analysis/*.R` plots reception rate, loss, continuity, and control overhead (`graphics.R`, `graphics-en.R`, `sim-1.R` through `sim-6.R`, `sim-geant.R`, `master.R`).

Set up DCE from `docs/setup-instructions/AGENTS.md`. The `ns-3-dce` symlink at the repository root points at DCE sources under `linux/ns3/linux-net-next-nuse-latest`. `--enable-kernel-stack` loads `liblinux.so` from that tree, whose `net/gmtp` serves `socket(SOCK_GMTP)`.

## Packet-loss injector

`kernel_modules/dropper/` is a Netfilter `NF_INET_PRE_ROUTING` module. It accepts packets and drops every 10th one. Build it the same way as the GMTP modules, against the running kernel headers:

```sh
cd app/kernel_modules/dropper
make
sudo insmod dropper.ko
```

## OpenWRT LuCI snapshot

`openwrt/luci/` is the router administration UI from the dissertation (figure 20). The saved page is `thiago-VirtualBox - GMTP - LuCI.xhtml`, with its sidecar assets. `extjs4/` is the ExtJS 4 tree used by that UI, and `util/` holds form widgets. This folder is a snapshot of that interface, not a current OpenWRT package feed.
