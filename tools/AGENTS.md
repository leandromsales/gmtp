# External tools

`gmtp/tools/` holds upstream checkouts that GMTP builds consume. It is not GMTP source. Protocol, plugin, and experiment changes belong in `gmtp/linux/`, `gmtp/app/`, and `gmtp/patches/`.

Write new comments, docs, and identifiers in English.

| Checkout | Upstream | Used by |
| --- | --- | --- |
| `gstreamer/` | https://gitlab.freedesktop.org/gstreamer/gstreamer.git | `app/gstreamer/gst-plugin-gmtp` |

The checkout has its own upstream `README.md` and `AGENTS.md`. Follow those for configure, compile, and test commands. Follow this file for when to update it and how GMTP must consume the result.

The directory is gitignored by the GMTP repository except this file. Do not commit upstream trees, build directories, or subprojects into `gmtp/`.

There is no `ns-3-dev-git` checkout here. GMTP simulations that run Linux binaries use Direct Code Execution and the ns-3 revision that guide pins. See `docs/setup-instructions/AGENTS.md`.

## Use this tool during development

When a task needs media transport, use the checkout in this folder:

- Building or running `gst-plugin-gmtp`, or any `gst-launch-1.0` pipeline that loads `gmtpclientsrc`, `gmtpserversink`, `gmtpclientsink`, or `gmtpserversrc`: build GStreamer from `tools/gstreamer` and point the plugin at that prefix.

Do not download a second GStreamer tree for that task. Do not vendor GMTP changes into this checkout. If a symbol or API is missing, fix the GMTP consumer or record the required upstream revision.

## Before every build that uses the GStreamer checkout

Run this check every time a build references `tools/gstreamer`. A previous build in the same session does not skip it.

1. Enter `tools/gstreamer`.
2. `git fetch origin`.
3. Resolve the upstream tip:
   - On a branch with an upstream: `git rev-parse @{u}`.
   - On a detached HEAD: `git rev-parse origin/HEAD`.
4. Compare that tip with `git rev-parse HEAD`.
5. If they differ and the local commit is behind the upstream tip:
   - Stop if `git status --porcelain` is non-empty. Do not discard or stash local edits to force the pull.
   - On a branch: `git pull --ff-only`.
   - On a detached HEAD: check out the upstream branch (`main`, or whatever `origin/HEAD` names) and `git pull --ff-only`.
   - Compile that revision with the upstream README in the same checkout.
   - Point every reference at the new build: `PKG_CONFIG_PATH`, `LD_LIBRARY_PATH`, `--prefix`, `PATH`, and `tools/gstreamer/prefix`.
6. If HEAD already matches the upstream tip, reuse the existing install when its artifacts are present. Rebuild when objects are missing or configure flags changed.
7. Record the commit SHA that the GMTP build actually linked.
