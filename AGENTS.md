# GMTP

Repository for the Global Media Transmission Protocol (GMTP): a Linux transport for live media, with relays, GMTP-MCC, and GMTP-UCC.

Write text and code in English. That covers new prose, comments, identifiers, log messages, Kconfig help, commit messages, and agent docs. Existing Portuguese comments and the 2016 dissertation stay as historical sources. When you edit a line that is still in Portuguese, translate that line.

Read this file, then the `AGENTS.md` next to the directory you change.

## Where to work

| Path | What it is | Read |
| --- | --- | --- |
| `linux/` | `host/` is the bootable kernel (`linux-net-next-latest`). `ns3/` is the DCE/NUSE library (`linux-net-next-nuse-latest`). Each has an `old/` archive | `linux/AGENTS.md` |
| `app/` | Applications and experiments from Wendell Silva Soares's 2016 dissertation (`docs/master-wendell/dissertacao/dissertacao-wendell-final.pdf`) | `app/AGENTS.md`, `app/README.md` |
| `tools/` | Upstream GStreamer checkout consumed by builds | `tools/AGENTS.md` |
| `patches/` | Archive of the 2015 and 4.9.50 diffs. Not the way to update a tree | `patches/README.md` |
| `docs/setup-instructions/` | ns-3 and DCE setup guides | `docs/setup-instructions/AGENTS.md` |
| `site/` | Static 2014 project page | `site/AGENTS.md` |
| `docs/` | Dissertation, papers, drafts | the PDF or note for that task |
| `ns-3-dce` | Symlink to the DCE source under `linux/ns3/linux-net-next-nuse-latest` | `docs/setup-instructions/AGENTS.md` |

GMTP kernel edits go in two trees and stay in step: `linux/host/linux-net-next-latest/net/gmtp` (bootable netdev `net-next`) and `linux/ns3/linux-net-next-nuse-latest/net/gmtp` (DCE/NUSE library, Linux 5.10.47, `arch/lib`). Both were ported from `linux/host/old/linux-5.4.21/net/gmtp`. Do not implement the protocol inside `tools/gstreamer`. Do not clone Linus Torvalds's `linux.git`. Do not replace the ns-3 library with LKL.

## Builds that use tools/

Before every build that compiles or runs `tools/gstreamer`, follow `tools/AGENTS.md`: fetch, and if the upstream tip has new commits, pull, compile that revision, and point the GMTP command at that build (`PKG_CONFIG_PATH`, prefix, and `tools/gstreamer/prefix`).

A guide under `docs/setup-instructions/` that pins an ns-3 or DCE revision keeps that checkout. GMTP simulations use the ns-3 revision that guide pins. There is no `tools/ns-3-dev-git`.

## Patches

`patches/` is an archive of how GMTP was inserted into Linux 4.9.50 and how the 2015 Linux and DCE trees were synchronized. Do not apply those files to update a tree. See `patches/README.md`.
