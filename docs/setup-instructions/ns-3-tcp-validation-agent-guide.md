# ns-3 TCP Validation — Project and Agent Setup Guide

Source website: <https://ns-3-tcp-validation.github.io/>

Reviewed: 2026-10-07 (UTC).

This is a source-grounded digest and an agent operating guide, not a verbatim website archive. Historical project facts, inspected source behavior, and newly proposed engineering procedures are distinguished below. Repository metadata and selected source files were checked; compilation and simulations were **not** executed while preparing this document.

## 1. Website content digest

The GSoC 2019 project, by Apoorva Bhargava with mentors Tom Henderson and Vivek Jain, compared ns-3 TCP behavior with Linux through Direct Code Execution (DCE). It developed Linux-style Reno and PRR models, tests, examples, and documentation. Differences involved acknowledgment-driven congestion-window growth and recovery arithmetic. CUBIC comparisons improved but did not achieve complete alignment. The website describes Reno as submitted for merging, PRR as patch-ready, and CUBIC as ongoing testing; these are historical statuses, not current merge-state assertions. Suggested extensions include completing CUBIC alignment, automating experiments, and investigating ECN, RACK, and SACK. Google Summer of Code funded the work. [S1]

The project wiki also contains an earlier, broader plan involving DCTCP, DSACK, and paced chirping. Treat that plan separately from completed deliverables. Its progress notes describe testing delayed acknowledgments, segment sizes, loss cases, and recovery behavior. [S2]

## 2. Project map and verified revisions

| Component | Repository / branch | Observed commit |
| --- | --- | --- |
| Phase 1: Reno | [ns-3-dev / LinuxRenoMergeRequest](https://gitlab.com/apoorvabhargava/ns-3-dev/-/tree/LinuxRenoMergeRequest) | `94cf915986c2316c9132ba0f707dc39628f822ba` |
| Phase 2: PRR | [ns-3-dev / LinuxPRRMergeRequest](https://gitlab.com/apoorvabhargava/ns-3-dev/-/tree/LinuxPRRMergeRequest) | `ff2bae1ba923df43936362b1ad001686f53dc9c4` |
| Phase 3: scenarios and scripts | [tcp_testing_and_alignment / master](https://gitlab.com/apoorvabhargava/tcp_testing_and_alignment) | `18ca1fb315b7a036234ff40a8332e976b064430a` |
| Historical CUBIC implementation dependency | [Tom Henderson / ns-3-dev / tcp-cubic](https://gitlab.com/tomhenderson/ns-3-dev/tree/tcp-cubic) | Unresolved: branch API returned HTTP 404 |

Commit values were retrieved through GitLab's public repository API. Branch heads may change; use the recorded commits when reproducing this guide. A 404 establishes that this retrieval failed, not whether the repository was deleted, renamed, made private, or otherwise unavailable.

The validation repository contains these inspected paths:

| Path | Role |
| --- | --- |
| `README.md` | Historical CUBIC integration and execution instructions |
| `Examples/dumbbell-topology-cubic-variant.cc` | Comparison scenario |
| `Examples/dumbbell-topology-ns3-receiver.cc` | Additional scenario; present, not analyzed here |
| `Scripts/parse_cwnd.py` | Extracts Linux congestion-window samples from DCE logs |
| `Patches/0001-Patch-to-trace-cwnd-in-Linux.patch` | Adds kernel logging for slow start, congestion avoidance, and PRR |

The validation repository is an experiment supplement, not a complete ns-3/DCE installation. [S3–S6]

## 3. Instructions for agents

The following operating rules are recommendations added for this guide.

1. Read this guide, the checked-out README, applicable `AGENTS.md` files, build definitions, and scenario arguments before editing.
2. Decide whether the task is historical reproduction, native-model testing, or porting to a newer stack. Record the choice.
3. Pin all source revisions. Record ns-3, DCE, Bake, simulated Linux, compiler, Python, OS image, architecture, and patches.
4. Keep the host OS kernel distinct from the Linux networking implementation loaded inside DCE. `uname -r` identifies the host kernel; it does not identify the simulated TCP implementation.
5. Establish a working baseline before altering TCP code. Use separate branches or worktrees for changes.
6. Treat missing historical dependencies as explicit blockers. Do not silently substitute a current implementation and label the result a reproduction.
7. Never infer a successful run from process exit status alone. Inspect application logs, packets, and nonempty traces.
8. Preserve original traces. Any filtering, unit conversion, time alignment, or sampling must be recorded in derived outputs.
9. Scope conclusions to the tested implementation revisions, topology, parameters, and observations. Similar plots do not prove general TCP equivalence.
10. Report failures with the command, working directory, revision, exit status, and first relevant diagnostic.

### Choose a workflow

| Goal | Starting point | Completion evidence |
| --- | --- | --- |
| Test historical Reno/PRR models | Pinned ns-3 branches in section 5 | Named suite results and successful example logs |
| Compare Linux and ns-3 under DCE | Sections 6–8, including dependency resolution | Both stacks transfer data; validated traces and experiment manifest |
| Port to a newer ns-3 release | Separate development branch | Explicit compatibility changes and regression evidence |

Do not treat these workflows as interchangeable.

## 4. Environment strategy

DCE documentation identifies Linux as its supported OS family, Ubuntu 20.04 as its tested environment for DCE 1.12, and Ubuntu 16.04 for DCE 1.11. These are documented combinations, not a claim that a 2019 scenario works unchanged with either release. [S7]

Recommended starting environment: an isolated x86-64 Linux VM, with a snapshot before dependency installation. Allocate approximately 4 CPUs, 8 GB RAM, and 30 GB free disk as planning defaults, then adjust to observed build needs. These resource allocations are guide recommendations, not project requirements.

On macOS or Windows, use a Linux VM or a remote Linux development machine. On ARM hosts, treat architecture compatibility as an additional verification step; an x86-64 VM/emulated environment is a possible route, not a verified configuration for this project.

For historical work, select the original toolchain only after inspecting repository requirements. Do not replace system Python symlinks or change the host toolchain to accommodate a legacy checkout. Use the isolated environment instead.

For an initial Ubuntu 20.04 DCE 1.12 environment, the official quick start provides a package recipe. The following adapts it with certificates, source-search, and trace-analysis utilities. Run inside the disposable VM; package installation needs administrator privileges. [S8]

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build ccache git ca-certificates wget \
  python3 python3-pip python3-numpy \
  libgsl-dev libgtk-3-dev libboost-dev automake bc bison flex gawk \
  libc6 libc6-dbg libdb-dev libssl-dev libpcap-dev rsync gdb \
  mercurial indent libsysfs-dev ripgrep
python3 -m pip install --user requests distro

mkdir -p "$HOME/ns3-tcp-validation-work"
export TCP_WORKSPACE="$HOME/ns3-tcp-validation-work"
cd "$TCP_WORKSPACE"
```

Do not assume this package list resolves every requirement of the older phase branches. Inspect configuration failures and install only the missing dependencies appropriate to the selected revision.

## 5. Native Reno and PRR setup

These commands use the website's historical Waf workflow. Keep the two revisions in separate working directories. [S1]

```bash
cd "$TCP_WORKSPACE"
git clone --branch LinuxRenoMergeRequest --single-branch \
  https://gitlab.com/apoorvabhargava/ns-3-dev.git ns3-reno
git -C ns3-reno checkout --detach 94cf915986c2316c9132ba0f707dc39628f822ba

git clone --branch LinuxPRRMergeRequest --single-branch \
  https://gitlab.com/apoorvabhargava/ns-3-dev.git ns3-prr
git -C ns3-prr checkout --detach ff2bae1ba923df43936362b1ad001686f53dc9c4
```

Check each checkout's `README`, `VERSION`, Waf launcher, and configuration output for supported interpreter/compiler requirements. The following are historical commands, not verified builds in the environment above.

### Reno

```bash
cd "$TCP_WORKSPACE/ns3-reno"
./waf configure --enable-tests --enable-examples
./waf
./test.py --suite=tcp-linux-reno-test
./waf --run "tcp-linux-reno --congControlVariant=TcpLinuxReno"
```

### PRR

```bash
cd "$TCP_WORKSPACE/ns3-prr"
./waf configure --enable-tests --enable-examples
./waf
./test.py --suite=tcp-linux-prr-recovery-test
```

**Verified mismatch:** the pinned PRR branch has the PRR test source, but its `examples/tcp/` listing does not contain `tcp-linux-reno.cc`; the Reno branch does. The website nevertheless gives the following PRR example command. Treat it as historical documentation, not an available executable in the pinned PRR checkout:

```bash
# Run only after locating or integrating the compatible example and its build target.
./waf --run "tcp-linux-reno --recoveryType=TcpLinuxPrrRecovery"
```

Resolve this by inspecting the Reno example and the PRR branch's APIs, then integrating a compatible example in a separate worktree with a recorded patch. Do not assume that copying the file alone is sufficient, or claim the PRR example has run when only its unit suite has run.

Before extending examples, inspect actual registrations and arguments:

```bash
rg -n 'TcpLinuxReno|TcpLinuxPrrRecovery|tcp-linux-reno|AddValue' \
  src examples wscript
```

If the suite or executable is absent, diagnose the checked-out revision and build registration. Do not rename the command based on a guess. Native suite success is a useful baseline, but does not establish Linux/DCE comparison success.

## 6. Build a DCE baseline

The DCE 1.12 recipe below is an **adaptation baseline**, not the recovered original 2019 environment. It follows the documented Bake branch and Linux-stack mode, omitting the optional Quagga module. Verify the resulting dependency selection with `show`. [S8]

```bash
cd "$TCP_WORKSPACE"
git clone --branch dce-1.12 --single-branch \
  https://gitlab.com/nsnam/bake.git bake
cd bake

export TCP_BAKE_ROOT="$PWD"
export PATH="$TCP_BAKE_ROOT/build/bin:$TCP_BAKE_ROOT/build/bin_dce:$PATH"
export LD_LIBRARY_PATH="$TCP_BAKE_ROOT/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DCE_PATH="$TCP_BAKE_ROOT/build/bin_dce:$TCP_BAKE_ROOT/build/sbin"

./bake.py configure -e dce-linux-1.12
./bake.py check
./bake.py show
./bake.py download
./bake.py build
```

Stop and resolve failed checks before continuing. Record the Bake SHA and every downloaded repository revision; selecting a branch alone is not a permanent lock.

Run the documented Linux-stack smoke test before importing historical scenarios:

```bash
cd "$TCP_BAKE_ROOT/source/ns-3-dce"
./test.py
./waf --run "dce-iperf --stack=linux"
```

Inspect DCE per-node application logs and confirm actual traffic. The installation includes `build/`, downloaded sources under `source/`, and may provide `bakeSetEnv.sh`; source-directory names vary with configuration. [S9]

Suggested inspection:

```bash
rg --files -g stdout -g stderr -g status -g cmdline .
rg -n 'error|failed|assert|fatal' files-* 2>/dev/null
```

A match is a diagnostic lead, not an automatic failure verdict; absence of matches is not proof of success. Restore environment variables in every new shell, using the same absolute Bake root.

## 7. Integrate the historical validation scenario

### 7.1 Obtain and pin experiment assets

```bash
cd "$TCP_WORKSPACE"
git clone https://gitlab.com/apoorvabhargava/tcp_testing_and_alignment.git experiments
git -C experiments checkout --detach 18ca1fb315b7a036234ff40a8332e976b064430a
```

### 7.2 Resolve the missing CUBIC source first

The historical README expects `tcp-cubic.cc` and `tcp-cubic.h` from Tom Henderson's `tcp-cubic` branch, adds them to the ns-3 internet module, and rebuilds DCE. That branch could not be resolved during this review. [S3]

An agent must choose and document one of these routes:

1. Recover the original source through an accessible historical commit, archive, or maintainer-provided reference; record its provenance and hash.
2. Explicitly perform a port using the CUBIC implementation in the selected ns-3 version. Document behavior changes and call the result a port, not exact historical reproduction.
3. Report historical CUBIC reproduction as blocked while completing the accessible Reno/PRR setup.

Also verify that the DCE-linked ns-3 tree supplies `TcpLinuxPrrRecovery`, which the historical ns-3 comparison command requests. A similarly named newer recovery implementation is not automatically equivalent.

### 7.3 Add files without overwriting existing models blindly

The README places the scenario in DCE's `examples/`, registers it in the build, and places the parser in `utils/`. [S3]

```bash
export TCP_DCE_ROOT="$TCP_BAKE_ROOT/source/ns-3-dce"
cp "$TCP_WORKSPACE/experiments/Examples/dumbbell-topology-cubic-variant.cc" \
  "$TCP_DCE_ROOT/examples/"
cp "$TCP_WORKSPACE/experiments/Scripts/parse_cwnd.py" "$TCP_DCE_ROOT/utils/"

cd "$TCP_DCE_ROOT"
rg -n 'dce-iperf|create_ns3_program|examples/|dumbbell' wscript examples
```

Use the existing comparable target to derive the executable registration and module dependencies. In a Waf-based ns-3 tree, inspect the internet module's `wscript` for source/header registration. Do not paste a build fragment without checking that checkout's API.

Before copying CUBIC model files, check whether they already exist. Before adding PRR, compare the source and required API changes. Rebuild the affected ns-3 dependency and DCE together through their configured build process; avoid mixing libraries from another checkout.

Preflight the resulting scenario:

```bash
cd "$TCP_DCE_ROOT"
./waf --run "dumbbell-topology-cubic-variant --PrintHelp"
```

The included kernel patch adds diagnostic logging, whereas the inspected parser reads `ss` output. Do not assume that patch is required for this parsing route or provides complete CUBIC tracing. If a task requires it, run `git apply --check` against the exact simulated-kernel checkout, then review and rebuild there. Never apply it to the host kernel. [S5–S6]

## 8. Run the CUBIC comparison

Only proceed after the integration checks above succeed. Commands below reproduce the README's comparison settings, with `python3` selected explicitly for the parser; that interpreter choice still requires local validation. [S3]

```bash
cd "$TCP_DCE_ROOT"
./waf --run "dumbbell-topology-cubic-variant --stack=linux --queue_disc_type=FifoQueueDisc --WindowScaling=true --Sack=true --stopTime=300 --delAckCount=2 --BQL=true"

cd utils
python3 parse_cwnd.py 2 2
cd ..
```

Archive the Linux run's `files-*` directories, timestamped results, and parsed `cwnd_data` **before** starting the next run. Use a unique archive directory per run. The parser writes shared output paths, so rerunning it can replace previous parsed data.

```bash
cd "$TCP_DCE_ROOT"
./waf --run "dumbbell-topology-cubic-variant --stack=ns3 --queue_disc_type=FifoQueueDisc --WindowScaling=true --Sack=true --delAckCount=2 --dataSize=524 --stopTime=300 --recovery=TcpLinuxPrrRecovery --BQL=true"
```

Do not parallelize these runs in the same working directory. The scenario and DCE log locations can collide. For a short smoke run, inspect application start times first; a run ending before traffic starts is not useful evidence.

### Inspected source details that affect interpretation

The C++ scenario uses two routers, one sender, and one receiver. The router link is 1 Mbps with 10 ms delay; edge links are 10 Mbps with 1 ms delay. It sets initial cwnd to 10 segments, CUBIC beta to 0.7, and HyStart on. The receiver uses an ns-3 TCP socket even for the Linux-sender experiment. Linux `ss` sampling occurs every 0.05 seconds starting at simulation time 10 seconds. [S4]

The ns-3 cwnd output divides byte counts by a hard-coded `524.0`. Therefore, changing `dataSize` without reviewing this conversion invalidates the plotted segment units. The scenario writes `results/dumbbell-topology/`, not the README's singular `result/`. [S4]

The parser expects to run from DCE's `utils/`, reads node IDs inclusively, looks for socket port 50000 and `NS3 Time:` markers, imports NumPy, and writes `cwnd_data/A-linux.plotme` for node 2. Its log concatenation and field parsing require checking on a different DCE version. [S5]

## 9. Validate outputs and compare fairly

These are proposed acceptance procedures, not success claims about an executed experiment.

1. **Build integrity:** confirm all participating binaries and libraries come from the recorded source/build tree.
2. **Execution integrity:** check simulation exit status and DCE application `status`, `stdout`, and `stderr`; verify packets or received bytes.
3. **Trace integrity:** require nonempty numeric time/cwnd rows, a sensible time range, consistent units, and no unexplained resets or mixed flows.
4. **Configuration parity:** compare MSS, delayed-ACK behavior, queue settings, SACK, window scaling, traffic timing, and recovery model. Identical command flags do not necessarily configure both stacks identically.
5. **Sampling parity:** keep original event-driven ns-3 traces and sampled Linux observations. For derived comparisons, document a common time grid and interpolation rule. Do not hide short-lived differences by undocumented smoothing.
6. **Behavioral checks:** examine slow start, congestion avoidance, loss/retransmission events, recovery entry/exit, and throughput where measured. Check packet traces when cwnd diverges.
7. **Result qualification:** distinguish a build pass, a traffic pass, a parser pass, and a model-alignment result.

Useful optional numerical summaries include mean/maximum cwnd error on a declared common time grid and relative throughput differences. Define tolerances before evaluating a change; this guide supplies no invented upstream pass threshold.

Official ns-3 TCP documentation also explains that its Reno validation used Linux 4.4.0 under DCE and that sampling differences can affect early cwnd plots. This reinforces the need to preserve exact kernel and measurement provenance. [S10]

### Suggested experiment manifest

```yaml
experiment_id: "replace-with-unique-id"
mode: "historical-reproduction-or-port"
os_image: "record-exact-image-or-vm-snapshot"
architecture: "record-architecture"
host_kernel: "record-uname-output"
compiler: "record-version"
python: "record-version"
revisions:
  ns3: "record-full-sha"
  dce: "record-full-sha"
  bake: "record-full-sha"
  simulated_linux: "record-full-sha"
  experiment_assets: "18ca1fb315b7a036234ff40a8332e976b064430a"
patches: []
working_directory: "record-absolute-directory"
commands: []
parameters: {}
random_seed_and_run: "record-actual-supported-controls"
outputs: []
validation:
  build: "not-run"
  traffic: "not-run"
  trace_parser: "not-run"
  comparison: "not-run"
limitations: []
```

## 10. Troubleshooting

| Symptom | Investigate / next action |
| --- | --- |
| `python` not found or Waf interpreter errors | Read launcher requirements; choose a compatible isolated interpreter/toolchain. Do not rewrite system symlinks. |
| Historical code fails with a newer compiler | Capture the first error; reproduce with a compatible compiler before proposing source patches. |
| `TcpLinuxPrrRecovery` or another TypeId is missing | Inspect the ns-3 source actually linked into DCE and its build registration. |
| `liblinux.so` missing or cannot load | Verify Linux-mode build, library architecture, dependency paths, and shell environment. |
| Scenario executable unavailable | Check the DCE build target, examples configuration, and rebuild output. |
| Parser cannot import NumPy | Install it for the selected parser interpreter in the isolated environment. |
| Empty or malformed Linux trace | Confirm node ID, port, `ss` availability, timestamp/log format, working directory, and actual traffic. |
| Linux data from an earlier run appears | Separate and archive per-run `files-*` and parser outputs before retrying. |
| cwnd scale changes after adjusting packet size | Inspect the fixed 524-byte conversion and actual negotiated MSS. |
| Plots differ despite a successful build | Check protocol parameters, receiver stack, sampling, packet losses, and recovery behavior before changing TCP algorithms. |
| Historical CUBIC branch remains unavailable | Recover a verifiable source snapshot or declare a port/blocker explicitly. |

## 11. Agent handoff checklist

Report: selected workflow; exact revisions and environment; executed commands; build and suite results; smoke-test evidence; paired-run output locations; applied patches; trace-conversion choices; and unresolved blockers. Preserve raw artifacts alongside any plots or summaries.

**Status of this guide:** website, documentation, repository structure, branch metadata, and selected source files inspected. Historical Reno/PRR commands documented. DCE setup proposed from official guidance. Missing CUBIC dependency identified. No build, test-suite pass, or successful end-to-end reproduction asserted.

## 12. Sources

- **S1 — Project website:** <https://ns-3-tcp-validation.github.io/>
- **S2 — GSoC project wiki:** <https://www.nsnam.org/wiki/GSOC2019TCPTestingAndAlignment>
- **S3 — Experiment README:** <https://gitlab.com/apoorvabhargava/tcp_testing_and_alignment/-/blob/18ca1fb315b7a036234ff40a8332e976b064430a/README.md>
- **S4 — CUBIC scenario:** <https://gitlab.com/apoorvabhargava/tcp_testing_and_alignment/-/blob/18ca1fb315b7a036234ff40a8332e976b064430a/Examples/dumbbell-topology-cubic-variant.cc>
- **S5 — Trace parser:** <https://gitlab.com/apoorvabhargava/tcp_testing_and_alignment/-/blob/18ca1fb315b7a036234ff40a8332e976b064430a/Scripts/parse_cwnd.py>
- **S6 — Kernel logging patch:** <https://gitlab.com/apoorvabhargava/tcp_testing_and_alignment/-/blob/18ca1fb315b7a036234ff40a8332e976b064430a/Patches/0001-Patch-to-trace-cwnd-in-Linux.patch>
- **S7 — DCE environment support:** <https://ns-3-dce.readthedocs.io/en/latest/intro.html>
- **S8 — DCE quick start:** <https://ns-3-dce.readthedocs.io/en/latest/getting-started.html>
- **S9 — DCE setup layout:** <https://ns-3-dce.readthedocs.io/en/latest/dce-user-install.html>
- **S10 — Official ns-3 TCP model documentation:** <https://www.nsnam.org/docs/models/html/tcp.html>
- **Repository metadata API:** <https://gitlab.com/api/v4/projects/apoorvabhargava%2Fns-3-dev/repository/branches/LinuxRenoMergeRequest>, <https://gitlab.com/api/v4/projects/apoorvabhargava%2Fns-3-dev/repository/branches/LinuxPRRMergeRequest>, and <https://gitlab.com/api/v4/projects/apoorvabhargava%2Ftcp_testing_and_alignment/repository/commits/master>.

The `latest` documentation URLs and API branch responses are mutable. Archive or pin the versions actually used when executing a reproducible study.
