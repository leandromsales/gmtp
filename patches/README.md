# GMTP patches

Unified diffs that introduce GMTP into a Linux tree, adjust the user-space examples for the 4.9.50 port, and synchronize that protocol code with the ns-3 Direct Code Execution (DCE) kernel library.

Apply a patch from the `gmtp/` repository root, and dry-run first:

```sh
git apply --check patches/<file>
# older diffs that git apply rejects:
patch -p1 --dry-run < patches/<file>
```

These files are an archive. They are not the method for updating GMTP. The port now lives in `linux/host/linux-net-next-latest` and `linux/ns3/linux-net-next-nuse-latest`. Re-applying these files fails or reverts later edits. Do not replay them onto those trees.

Paths inside each file are relative to a specific layout. Match the layout before applying.

## Linux 4.9.50 port

### gmtp_linux-4.9.50.patch

Adds GMTP to `linux/host/old/linux-4.9.50` (about 470 KB).

- `config_minimal`: x86_64 kernel config carried with the port. The header still says `Linux/x86 4.0.3 Kernel Configuration`.
- Protocol registration: `include/linux/net.h`, `include/linux/socket.h`, `include/uapi/linux/in.h`, `include/net/net_namespace.h`, `include/net/netns/gmtp.h`, `include/net/secure_seq.h`, `net/core/secure_seq.c`.
- GMTP headers: `include/linux/gmtp.h`, `include/uapi/linux/gmtp.h`.
- Full `net/gmtp` implementation: sockets, IPv4, input, output, timers, hash tables (client and server), GMTP-MCC (`mcc/`: equation, loss intervals, packet history), and GMTP-Inter (`gmtp-inter/`: relay, UCC, inter hash, packet build).

The same files are already present under `linux/host/old/linux-4.9.50/`. `linux/host/old/linux-5.4.21/net/gmtp` is a later tree and is not produced by this patch. The current ports are `linux/host/linux-net-next-latest/net/gmtp` and `linux/ns3/linux-net-next-nuse-latest/net/gmtp`.

### gmtp_app_4.9.50.patch

User-space delta aimed at the 4.9.50 bring-up. It does not match the current `app/python` tree (`SOL_GMTP` is 300 and the default interface is `eth1` today).

- Deletes `client.R`, `client_audio.py`, `server_audio.py`, `server_multithreading.py`, and `clients.sh`.
- Sets `SOL_GMTP` from 281 to 282 in `gmtp.py`.
- In `server.py`, defaults the interface to `enp0s3`, constructs the socket with numeric type `7` and protocol `254`, and sets the flow name and TX rate with level `282` instead of `socket.SOL_GMTP`.

Keep the current Python sources. Consult this patch only to see the 4.9.50-era socket-level workaround.

## Linux 4.0.3 to DCE, July 2015

These diffs use the path prefix `linux-4.0.3/`. That directory is not in the repository. They capture GMTP fixes written on the host kernel that had to be replayed onto the DCE simulated kernel.

| File | What it changes |
| --- | --- |
| `gmtp_linux_to_dce_07-07-15.diff` | Declares `gmtp_poll` and installs it as `inet_gmtp_ops.poll` in `net/gmtp/ipv4.c` and `proto.c`. |
| `gmtp_linux_to_dce_25-07-15-a.diff` | Renames Speakup `GMTP_ALPHA` back to `ALPHA` so the GMTP protocol name does not collide with the staging speakup chartable. Also updates `gmtp-inter` build, hash, and UCC. The same hunks are concatenated twice in this file. |
| `gmtp_linux_to_dce_25-07-15-b.diff` | Adds `tx_ucc_rate` on `struct gmtp_sock`, ACK and feedback header accessors, and the matching relay, socket-option, timer, and output paths. |
| `gmtp_linux_to_dce_28-07-15.diff` | Adds `tx_avg_rtt`, drops the `wait` field from `gmtp_hdr_ack`, and switches relay ACK construction to `flow_rtt` and `min(ucc_rx, rcv_tx_rate)`. Touches inter input, MCC input, UCC, and `proto.c`. |

## DCE to Linux, July-August 2015

These diffs use the path prefix `net-next-sim/`. That directory was the DCE kernel library at the repository root. It now lives at `linux/ns3/old/linux-net-next-nuse-4.1.0-rc4/`. They carry fixes found under DCE back toward the Linux GMTP sources of that date.

| File | What it changes |
| --- | --- |
| `gmtp_dce_to_linux_06-07-15.diff` | `gmtp_inter_build_pkt` copies the source skb (`skb_copy`) instead of allocating from `hdrlen` alone. Follow-up edits in inter input, inter MCC, and `input.c`. |
| `gmtp_dce_to_linux_04-08-15.diff` | Adds `xmit_timer` and `struct gmtp_packet_info` on the socket header, `gmtp_data_hdr_len()`, and a wide set of inter, hash, output, proto, sockopt, and timer updates. |
| `gmtp_dce_to_linux_10-08-15.diff` | Relay ACK build uses netdev index 7, stores `worst_rtt` instead of the last/average RTT pair, and keeps the GMTP-UCC timer setup. Also touches inter input and MCC. |

## Which file to use

| Goal | File |
| --- | --- |
| See how GMTP was inserted into Linux 4.9.50 | `gmtp_linux-4.9.50.patch` |
| See the user-space socket tweak tried with that port | `gmtp_app_4.9.50.patch` |
| See a host-kernel fix that was forwarded to DCE in July 2015 | `gmtp_linux_to_dce_*.diff` |
| See a DCE fix that was forwarded back to the Linux layout of August 2015 | `gmtp_dce_to_linux_*.diff` |
| Change the host kernel now | Edit `linux/host/linux-net-next-latest/net/gmtp`. Do not start from these patches. |
| Change the DCE and NUSE stack | Edit `linux/ns3/linux-net-next-nuse-latest/net/gmtp` and keep it in step with the host tree. |
