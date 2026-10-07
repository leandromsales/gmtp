# Implementing L4S in ns-3 — Paper, Source, and Agent Reproduction Guide

Requested paper: <https://arxiv.org/html/2603.20166v1>

Authors: Maria Eduarda Veras, Eduardo Freitas, Assis T. de Oliveira Filho, Djamel Sadok, and Judith Kelner, UFPE/GPRT.

Paper version: arXiv:2603.20166v1, 20 March 2026. Reviewed: 2026-10-07 UTC.

This guide distinguishes the paper's claims, the repository state inspected later, and proposed reproduction procedures. No patch application, compilation, simulation, or physical testbed experiment was executed during preparation.

## 1. Paper digest

The paper implements TCP Prague and flag-based Accurate ECN in ns-3.46.1, using Linux's L4S code as a reference. It extends TCP header/state processing and adapts fractional windows, rounding, and pacing across packet/byte and byte/bit representations. Experiments combine Prague and Cubic with an experimental DualPI2 queue. Two bottleneck scenarios are studied over 30 runs and compared with a physical Linux testbed. Reported Jain fairness values are 0.9927 and 0.9876. Agreement is stronger in the higher-bandwidth scenario; discrepancies at lower bandwidth are associated with AQM behavior. Further work targets DualPI2 fidelity and broader experiments. [P1]

This is a native ns-3 model implementation, not an installation of a Linux kernel inside DCE. Do not apply the older DCE/Waf setup to this project.

## 2. Select the correct repository version

Repository: <https://github.com/GPRT/l4s-for-ns3>.

| Target | Snapshot inspected | Meaning |
| --- | --- | --- |
| Preprint code | `dev`, `b22fc7cbd3f0fff23288ad7d2771c4495df18b61` | Prague, AccECN, older experimental DualQ patch, paper-oriented scripts |
| Later work | `main`, `6297a2d78ccc5f9d8875d4c8ab93686c221ffefa` | Updated DualPI2 and DCTCP/Cubic experiments targeting ns-3.47 |

The current `main` README explicitly directs readers of this preprint to `dev`, describes that code as outdated, and says Prague is being updated. Therefore, reproducing the preprint and evaluating the later DualPI2 implementation are different tasks. A `main`-branch DCTCP result must not be labeled a reproduction of the paper's Prague result. [P2]

The observed `dev` SHA is a retrievable snapshot, not a verified archival tag for the exact paper submission. Preserve this distinction in reports.

## 3. Source map

| Path in `dev` | Role |
| --- | --- |
| `patches/l4s-implementation.patch` | Mailbox patch series; begins with patch 1/3 for DualQ, followed by AccECN and Prague according to README |
| `experiments/code.cc` | Six-node dumbbell scenario |
| `experiments/run-simulation.py` | Repeats runs, currently numbered 0–29 |
| `experiments/create_master_dataset.py` | Consolidates trace data using pandas/NumPy |
| `experiments/create_plots.ipynb` | Plotting notebook described by README as using an R kernel |

Inspect the patch for TCP header serialization, socket negotiation/ACK processing, congestion state, pacing, queue behavior, and build registration. Merely adding the `TcpPrague` class is insufficient to recreate the integration.

## 4. Verified inconsistencies to resolve before reproduction

| Source observation | Required agent decision |
| --- | --- |
| Paper says ns-3.46.1; `dev` README says ns-3.46 | Check patch applicability against the paper version first; keep a separate 3.46 comparison if needed |
| Repository README lists C++17, GCC 9+, and CMake 3.10+ | Inspected ns-3.46.1 build files instead set minimum CMake 3.20, GNU 11, Clang 17, and C++23; use the base project's actual checks |
| Scenario currently hard-codes `10Mbps` and `15ms` link delay | It represents a 30 ms propagation RTT over that bottleneck, not a 15 ms RTT |
| Batch runner uses `exps/results` | Consolidator currently uses `exps/results_scenario_2_modif`; align the paths explicitly |
| Batch runner skips a run whenever `throughput.csv` exists | Validate completeness; a partial file can cause an invalid run to be skipped |
| Runner catches failed builds/runs without a robust aggregate failure exit | Require a per-run success manifest and output checks |
| CLI exposes output path and FlowMonitor toggle, while link settings are fixed in C++ | Do not invent `--bandwidth` or `--rtt` flags |

These findings come from inspected files [P3–P7], not from executing them.

## 5. Agent operating rules

1. Choose paper reproduction (`dev`) or later AQM work (`main`) and record the choice.
2. Read repository instructions and actual scripts before running batches. Preserve an untouched source snapshot.
3. Work on a local feature branch. Apply patches only to a clean, compatible ns-3 checkout.
4. Do not change TCP behavior simply to make a plot resemble the publication. Separate compatibility repairs from scientific changes.
5. Keep topology, traffic direction, queue placement, segment size, ECN settings, seed/run, measurement interval, and protocol versions in a manifest.
6. Confirm AccECN negotiation, marking, feedback, and sender response from traces before claiming L4S operation.
7. Preserve raw per-run data. Do not silently replace failed runs, smooth away discrepancies, or combine branches' results.
8. Treat simulation and physical-testbed setup as separate workflows; the simulator recipe does not install a modified host kernel.

## 6. Prepare a native ns-3 environment

Recommended starting point: an isolated Ubuntu 24.04 Linux development environment with its packaged compiler, CMake, and Python. This is an engineering choice for the inspected modern ns-3 build requirements, not the paper's asserted simulator-host OS. Linux testbed OS choices are a separate matter.

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git \
  ca-certificates python3 python3-dev python3-venv pkg-config ripgrep

mkdir -p "$HOME/l4s-paper-work"
export L4S_WORKSPACE="$HOME/l4s-paper-work"
cd "$L4S_WORKSPACE"
python3 -m venv analysis-env
. analysis-env/bin/activate
python -m pip install numpy pandas

git clone --branch dev --single-branch \
  https://github.com/GPRT/l4s-for-ns3.git l4s-paper
git -C l4s-paper checkout --detach b22fc7cbd3f0fff23288ad7d2771c4495df18b61
git -C l4s-paper switch -c local-reproduction

git clone --branch ns-3.46.1 --single-branch \
  https://gitlab.com/nsnam/ns-3-dev.git ns3-paper
cd ns3-paper
git switch -c l4s-paper-reproduction
```

Capture `git rev-parse HEAD`, `g++ --version`, `cmake --version`, and Python package versions. Install additional dependencies only as needed by configure. The R plotting notebook requires its own kernel/packages; inspect its metadata and library calls before installing them.

## 7. Apply the patch and build

From the clean `ns3-paper` checkout:

```bash
git status --short
git apply --check "$L4S_WORKSPACE/l4s-paper/patches/l4s-implementation.patch"
```

If the check fails, stop before mutation. Diagnose whether the base version, an existing modification, or the patch itself causes the conflict. Do not force application or silently downgrade to 3.46.

The README uses `git am`, appropriate for the inspected mailbox patch series. Git needs an already-configured author/committer identity; do not invent the user's identity.

```bash
git am "$L4S_WORKSPACE/l4s-paper/patches/l4s-implementation.patch"
cp "$L4S_WORKSPACE/l4s-paper/experiments/code.cc" scratch/code.cc
./ns3 configure --enable-examples --enable-tests
./ns3 build
./test.py
```

If `git am` stops with conflicts, retain diagnostics and either resolve them deliberately or use `git am --abort` to return to the pre-application state. Record any changes made for 3.46.1 compatibility.

Discover available project-specific tests rather than assuming their names:

```bash
rg -n 'TestSuite|AccEcn|AccECN|TcpPrague|DualQCoupledPi2' src
./ns3 run "scratch/code --PrintHelp"
```

## 8. Run one scenario before a batch

The current `dev` source defaults to the lower-bandwidth case. Create the output directory before execution:

```bash
cd "$L4S_WORKSPACE/ns3-paper"
mkdir -p results-smoke
./ns3 run "scratch/code --pathOut=results-smoke --RngRun=0"
```

Confirm nonempty traces and actual data delivery. The inspected source writes Prague/Cubic cwnd and RTT files, queue probability/sojourn traces, queue marks, and `throughput.csv`. It uses a 1448-byte segment size. Sender activity starts at 1.5 s and ends at 58 s; the simulator ends at 63 s, despite the nominal 60 s scenario setting. Preserve the measured interval when computing throughput. [P4]

For the higher-bandwidth case, change the scenario's fixed link settings on the local branch, or add explicit CLI attributes with a recorded implementation change. For a target propagation RTT of 5 ms with zero-delay access links, a 2.5 ms one-way bottleneck delay is the corresponding configuration. Verify this against traces and any authors' exact scenario files rather than inferring delay semantics solely from a caption.

Keep the two scenarios in separate result roots and record the source hash used for each.

## 9. Batch execution and aggregation

Before running the supplied runner, set its `OUTPUT_BASE` to the chosen scenario directory and inspect its completion test. The inspected script executes 30 runs with different `RngRun` values; that alone does not prove statistically different replications if the scenario uses no effective random variables.

```bash
cd "$L4S_WORKSPACE/ns3-paper"
python "$L4S_WORKSPACE/l4s-paper/experiments/run-simulation.py"
```

Validate all 30 runs: process outcomes, time coverage, both flows, required metrics, and no truncation. Then change the consolidator's `OUTPUT_BASE` to exactly the runner's output root and keep `NUM_RUNS` consistent.

```bash
python "$L4S_WORKSPACE/l4s-paper/experiments/create_master_dataset.py"
```

The consolidator can skip missing files/runs. A generated CSV therefore does not establish a complete replication set. Check the run IDs and metric coverage before plotting. Preserve the local script modifications in Git.

## 10. Reproduction and analysis checklist

The following are proposed validation steps, not tests reported as executed here.

| Area | What to establish |
| --- | --- |
| Header/signaling | AE flag survives serialization; supported/unsupported peers negotiate appropriately |
| Congestion feedback | Counter updates and wrap handling are consistent with the chosen AccECN specification; packet/byte units are explicit |
| Prague dynamics | Fractional-window updates, rounding, pacing units, recovery, and state transitions match the intended reference |
| AQM | Traffic enters the intended queue; marking, scheduling, and sojourn traces use the intended direction and interface |
| Coexistence | Both flows carry traffic; report per-flow throughput and latency alongside fairness |
| Numerical comparison | Convert cwnd units and rate units explicitly; use matching observation windows |
| Statistics | Compute one summary per independent replication before confidence intervals across runs |

For per-run throughputs \(x_1,\ldots,x_n\), Jain's index is \(J=(\sum_i x_i)^2/(n\sum_i x_i^2)\). Average the per-run indices when matching the paper's stated approach. Do not calculate uncertainty by pretending correlated packet samples are independent runs.

The paper's algorithm reference uses a Linux 6.12 L4S branch, while its physical testbed names a 6.6-based L4S build. Record both versions separately. Do not infer that they are behaviorally identical. [P1]

A complete physical reproduction additionally requires the actual router/VM setup, L4S kernel/module builds, qdisc configuration, traffic orchestration, and raw measurement scripts. The inspected patch repository alone does not establish all of those details.

## 11. Results and scope of conclusions

The publication supports selected comparisons in a small dumbbell setting; it does not establish equivalence for every topology, transport mix, RTT, link rate, or loss condition. The repository itself reports continuing Prague fixes. Reproducing a reported discrepancy can be a valid result.

Handoff must include source SHAs, ns-3 tag/SHA, patch application outcome, local changes, toolchain, exact scenario settings, run inventory, raw outputs, aggregation settings, and unresolved differences.

## Sources and attribution

- P1: [Veras et al., arXiv:2603.20166v1](https://arxiv.org/html/2603.20166v1), paper text and code-availability statement. The paper is distributed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/); this guide paraphrases its findings and adds source inspection and operational guidance. Paper-derived adaptations are offered under that license; software retains its own licenses.
- P2: [Repository `main` at review snapshot](https://github.com/GPRT/l4s-for-ns3/tree/6297a2d78ccc5f9d8875d4c8ab93686c221ffefa).
- P3: [Preprint branch README](https://github.com/GPRT/l4s-for-ns3/blob/b22fc7cbd3f0fff23288ad7d2771c4495df18b61/README.md).
- P4: [Scenario source](https://github.com/GPRT/l4s-for-ns3/blob/b22fc7cbd3f0fff23288ad7d2771c4495df18b61/experiments/code.cc).
- P5: [Runner](https://github.com/GPRT/l4s-for-ns3/blob/b22fc7cbd3f0fff23288ad7d2771c4495df18b61/experiments/run-simulation.py).
- P6: [Consolidator](https://github.com/GPRT/l4s-for-ns3/blob/b22fc7cbd3f0fff23288ad7d2771c4495df18b61/experiments/create_master_dataset.py).
- P7: [ns-3.46.1 CMake configuration](https://gitlab.com/nsnam/ns-3-dev/-/blob/ns-3.46.1/CMakeLists.txt) and [compiler-standard configuration](https://gitlab.com/nsnam/ns-3-dev/-/blob/ns-3.46.1/build-support/macros-and-definitions.cmake).
- [L4S Linux reference cited by paper](https://github.com/L4STeam/linux/blob/l4steam-6.12.y/net/ipv4/tcp_prague.c).
