# GMTP

Repository for the Global Media Transmission Protocol (GMTP): a Linux transport for live media, with relays, GMTP-MCC, and GMTP-UCC.

Write text and code in English. That covers new prose, comments, identifiers, log messages, Kconfig help, commit messages, and agent docs. Existing Portuguese comments and the 2016 dissertation stay as historical sources. When you edit a line that is still in Portuguese, translate that line.

Read this file, then the `AGENTS.md` next to the directory you change. Before editing GMTP kernel sources, read and apply `.cursor/rules/about-gmpt-source-code.mdc`. When this repository sits inside the thesis workspace that file is `../.cursor/rules/about-gmpt-source-code.mdc`; a copy with paths relative to this repository also lives at `.cursor/rules/about-gmpt-source-code.mdc`. The rule applies inside Cursor and outside it.

## Where to work

| Path | What it is | Read |
| --- | --- | --- |
| `gmtp/` | Tracked `net/gmtp` for the host kernel and the ns-3 library. `gmtp.sh --apply` symlinks these files into the kernel trees | `gmtp/AGENTS.md` |
| `linux/` | `host/` is the bootable kernel (`linux-net-next-latest`). `ns3/` is the DCE/NUSE library (`linux-net-next-nuse-latest`). Each has an `old/` archive. Recreate the gitignored clones with `linux/checkout-trees.sh` | `linux/AGENTS.md`, `linux/patches/README.md` |
| `app/` | Applications and experiments from Wendell Silva Soares's 2016 dissertation (`docs/master-wendell/dissertacao/dissertacao-wendell-final.pdf`) | `app/AGENTS.md`, `app/README.md` |
| `tools/` | Upstream GStreamer checkout consumed by builds | `tools/AGENTS.md` |
| `patches/` | Archive of the 2015 and 4.9.50 diffs. Not the way to update a tree | `patches/README.md` |
| `docs/setup-instructions/` | ns-3 and DCE setup guides | `docs/setup-instructions/AGENTS.md` |
| `site/` | Static 2014 project page | `site/AGENTS.md` |
| `docs/` | Dissertation, papers, drafts | the PDF or note for that task |

GMTP kernel edits go in `gmtp/common/net/gmtp` when both kernels can build the file, and in `gmtp/host/net/gmtp` or `gmtp/ns3/net/gmtp` when a tree already has its own copy. `gmtp/gmtp.sh --apply` symlinks the host and ns-3 paths into `linux/host/linux-net-next-latest` and `linux/ns3/linux-net-next-nuse-latest`. `gmtp/gmtp.sh --remove` deletes those kernel symlinks. Forking a common file is defined in `.cursor/rules/about-gmpt-source-code.mdc`. Both trees were ported from `linux/host/old/linux-5.4.21/net/gmtp`. Do not implement the protocol inside `tools/gstreamer`. Do not clone Linus Torvalds's `linux.git`. Do not replace the ns-3 library with LKL.

DCE 1.12 sources used by `app/ns-3-dce` are `linux/ns3/linux-net-next-nuse-latest/tools/testing/libos/buildtop/source/ns-3-dce`. There is no `ns-3-dce` symlink at the repository root.

## Builds that use tools/

Before every build that compiles or runs `tools/gstreamer`, follow `tools/AGENTS.md`: fetch, and if the upstream tip has new commits, pull, compile that revision, and point the GMTP command at that build (`PKG_CONFIG_PATH`, prefix, and `tools/gstreamer/prefix`).

A guide under `docs/setup-instructions/` that pins an ns-3 or DCE revision keeps that checkout. GMTP simulations use the ns-3 revision that guide pins. There is no `tools/ns-3-dev-git`.

## Patches

Rebuild a live kernel checkout with `linux/checkout-trees.sh` and the files under `linux/patches/`. The repository-root `patches/` directory is an archive of how GMTP was inserted into Linux 4.9.50 and how the 2015 Linux and DCE trees were synchronized. Do not apply those archive files to update a tree. See `linux/patches/README.md` and `patches/README.md`.


## Bot communication protocol (GMTP team: GMTP-CEO, GMTP-CTO, GMTP-REV)

Adopted 22 Nov 2026 by order of Leandro (applies to all his bot teams). Full text: `docs/bot-communication-protocol.md`.

1. Pointer plus diff: messages between bots carry only repo/PR/file, commit SHA and a one-line ask. Reviewers read only `git diff <last-reviewed-sha>..<new-sha>` and record the last reviewed SHA in their PR comment.
2. One review round per deliverable: reviewers work in parallel and leave one consolidated PR comment; the author applies small fixes and merges without another round (a second round only if the fix changes substance).
3. Review proportional to risk: full review only for experiment protocol, preregistrations, frozen records, security, money or external sends. Docs, records and housekeeping PRs are merged by the author alone, followed by one non-waking FYI.
4. FYIs, status and results nobody waits on go with priority false; no acknowledgements, no duplicates, no fan-out.
5. AWS: GMTP-CTO talks directly with Leandro-CTO. Only Leandro approves spend, in his own words.
6. Package each experiment: cost forecast, gates/checks and review verdict in one document; approval is asked once.
7. Quality stays the same: only communication is leaner, not the checks that matter.
