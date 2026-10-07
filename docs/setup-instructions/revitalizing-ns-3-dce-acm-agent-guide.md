# Revitalizing ns-3's Direct Code Execution — Agent Reproduction Companion

Requested publication: Parth Pratim Chatterjee and Thomas R. Henderson, *Revitalizing ns-3's Direct Code Execution*, WNS3 2022, DOI [10.1145/3532577.3532606](https://doi.org/10.1145/3532577.3532606).

Reviewed: 2026-10-07 UTC.

**Access limitation:** the requested ACM PDF and landing page returned HTTP 403. The complete paper was not read. This companion is grounded in the authors' official conference slides, the ns-3 project wiki's explicit paper-reproduction section, publication metadata, and inspected experiment scripts. It must not be represented as a complete extraction or section-by-section summary of the ACM paper. No build or benchmark was executed.

## 1. What the accessible primary sources establish

The authors' slides explain how DCE executes real applications within an ns-3 process and why later glibc checks disrupted stream interception and PIE loading. Their modernization uses a private patched glibc, updates the Linux library port, and offers Docker packaging. The presentation compares native ns-3, DCE user mode, and DCE kernel modes; pure ns-3 is faster in the displayed benchmark. Docker and native Linux 5.10 curves are close, while Linux-library modes incur greater overhead. The slides identify further work on IPv6, loader support, portability, and profiling. These are qualitative findings from the presentation, not newly measured results. [A1]

The paper's artifact wiki identifies a specific performance branch, build/environment choices, and scripts for Figures 4–6. It cautions that absolute timings depend on hardware and compilation. Some curves labeled “Tazaki” were imported from an earlier publication, not rerun by these authors. [A2]

## 2. Architecture and experiment boundaries

For agent work, keep these categories separate:

| Category | Meaning | Do not confuse with |
| --- | --- | --- |
| Native ns-3 benchmark | Simulation using ns-3 models | Real application under DCE |
| DCE user mode | Real application with ns-3's stack | Host OS network-stack execution |
| DCE kernel mode | Real application with a Linux library inside DCE | Booting that kernel in a VM |
| Container packaging | Environment in which DCE runs | A replacement for simulation semantics |
| ELF loader configuration | How executable/library loading is handled | A TCP algorithm or queue discipline |

An OS version, DCE version, loader, simulated-kernel version, and build profile collectively define a benchmark condition. Changing one creates a different condition.

## 3. Inspected artifact identity

Performance repository: <https://github.com/tomhenderson/ns-3-dce/tree/performance>.

Observed and wiki-consistent commit: `a13b4433468f0c74db1a89b78e7819f9338bc09c`.

Relevant files inspected or confirmed present:

| File | Purpose / output |
| --- | --- |
| `figure3-elf.sh` | Script associated by wiki with paper Figure 4; writes `output-fig3-elf` |
| `run-rate-vs-time-ns3.sh` | Pure ns-3 curve; writes `rate-vs-time.ns3.dat` |
| `run-rate-vs-time-kernel.sh` | DCE kernel curve; writes `rate-vs-time.dce11.kernel.dat` |
| `run-rate-vs-time-elf-kernel.sh` | ELF-loader kernel-mode curve; presence verified |
| `run-rate-vs-time-elf-user.sh` | ELF-loader user-mode curve; presence verified |
| `run-rate-vs-time-user.sh` | User-mode script; presence verified |
| `example/dce-udp-perf.cc` | Benchmark implementation |

Only the first three scripts' contents were inspected in detail. Read the others before execution. The historic `figure3` filename does not mean it belongs to Figure 3 in the final paper. [A2, A3]

## 4. Choose the reproduction target

| Target | Historical environment documented by wiki |
| --- | --- |
| Figures 4–5 | Ubuntu 16.04, DCE 1.11-era performance branch, ns-3.34, optimized builds; ELF loader for designated curves |
| Figure 6 | Ubuntu 20.04, precursor DCE 1.12 code staged in the author's repositories; native and container cases |

The wiki names different CPUs for the older and newer sets. Do not infer a software-only speedup by comparing timings across those machines. ELF-loader availability also differs. [A2]

For Figure 6, the wiki points to author Bake branches `dce-1.12` and `dce-1.12-linux-5.10`. The latter is accessible at `29af6127b36f54b2fb144b844b43c26ed98c8d08`, but its inspected recipe has a `5.10.0`/`5.10.47` module-name mismatch. Resolve and document that before building. The separate modernization guide contains the detailed findings.

## 5. Agent instructions

The following workflow is added by this guide:

1. State whether the goal is a historical figure reproduction or a new performance comparison.
2. Use a dedicated VM/workspace for each incompatible toolchain, particularly Ubuntu 16.04 versus 20.04.
3. Pin all source revisions, loader implementations, kernel configuration, and container images. Keep patched libc private.
4. Establish a functioning DCE installation before replacing its source with the performance branch.
5. Ensure the selected DCE checkout is the one actually rebuilt and linked; a second unused clone is not integration.
6. Read scripts for deletion, output-appending, and direct-binary assumptions before running them.
7. Preserve raw output and environment metadata. Do not relabel modified experiments as unchanged historical results.
8. Report elapsed wall time separately from simulated time and packet-processing rate.

## 6. Obtain the benchmark source

```bash
mkdir -p "$HOME/dce-paper-benchmarks"
export DCE_BENCH_ROOT="$HOME/dce-paper-benchmarks"
cd "$DCE_BENCH_ROOT"
git clone --branch performance --single-branch \
  https://github.com/tomhenderson/ns-3-dce.git performance-source
cd performance-source
git checkout --detach a13b4433468f0c74db1a89b78e7819f9338bc09c
rg -n 'dce-udp-perf|dce-runner|time -o|rm -rf|>>' \
  figure3-elf.sh run-rate-vs-time-*.sh
```

These commands obtain and inspect source; they do not install its dependency stack.

In the chosen historical Bake workspace, configure a compatible dependency set, point the DCE module at this source revision, and rebuild ns-3/DCE consistently with the wiki's optimized profile. Inspect the generated configure arguments rather than assuming the default is optimized. If using a later environment, call it a port and record every difference.

Before timed runs, check that the built `build/bin/dce-udp-perf` exists, its shared-library dependencies resolve in that workspace, and any required `build/bin/dce-runner` ELF-loader executable exists. Run the applicable DCE tests and a small traffic smoke test.

## 7. Benchmark script behavior and safeguards

**Observed in source:** the scripts append to their output files. Several repeatedly delete `files-*`, `exitprocs`, and `elf-cache` in the current directory. Execute only in a disposable experiment working directory with no unrelated matching files. Archive results before another sweep. [A3]

The kernel script sweeps both 4-hop and 32-hop cases. The pure ns-3 script inspected sweeps 4-hop cases. Filter by topology before comparing curves; do not combine all rows into one series.

The scripts use GNU `time` and direct `build/bin/...` invocation. Confirm the external timing utility and runtime environment first. A Waf wrapper that rebuilds during measurement changes the measured quantity.

Once the matching installation is ready and prior outputs are archived:

```bash
# Run from the correctly built DCE performance checkout.
bash run-rate-vs-time-ns3.sh
bash run-rate-vs-time-kernel.sh
```

For the historical ELF-loader experiment, only after confirming the correct loader setup:

```bash
bash figure3-elf.sh
```

Do not run these scripts concurrently in one directory. The cleanup and output paths are shared. Review the remaining user/ELF scripts before executing their curves.

## 8. Interpret and validate results

The wiki describes Figure 4's processing rate as packet count divided by elapsed seconds. It describes Figure 5's plot using the sending-rate and execution-time columns. Inspect raw row layout before parsing: the program and GNU `time` both contribute to the same file, and units must be confirmed from the benchmark code. [A2]

Recommended analysis protocol:

1. Confirm every row represents a completed run with nonzero packet count and positive elapsed time.
2. Record offered rate, hop count, stack, loader, kernel-library revision, and optimization profile for each observation.
3. Separate compilation/download time from execution time.
4. Record hardware, VM/container overhead, CPU allocation, and competing workload.
5. For a new comparison, repeat measurements and report spread; preserve the original single-run protocol separately if reproducing it.
6. Compare like-for-like conditions. Treat data transcribed from prior work as external reference points, not fresh measurements.
7. Report throughput/packet-processing units explicitly; do not equate simulated traffic rate with host processing speed.

Do not read exact numerical data off a presentation plot when raw values are unavailable. A visually similar trend is weaker evidence than matching configurations and measured values.

## 9. Suggested run manifest

```yaml
publication_doi: "10.1145/3532577.3532606"
target_figure: "choose-4-5-or-6"
mode: "historical-reproduction-or-new-comparison"
dce_commit: "a13b4433468f0c74db1a89b78e7819f9338bc09c"
ns3_commit: "record-actual-revision"
bake_commit: "record-actual-revision"
simulated_linux_commit: "record-actual-revision"
loader: "record-implementation-and-revision"
glibc: "record-version-patches-and-private-prefix"
os_image: "record-image-or-snapshot"
cpu: "record-model-and-allocation"
compiler_and_flags: "record-exact-values"
script: "record-script-path-and-hash"
local_modifications: []
outputs: []
validation_status: "not-run"
```

## 10. What remains unverified

The complete ACM paper, its full prose, and any details absent from the accessible companion sources remain unverified. Historical dependency availability, container pulls, builds, test passes, and benchmark results were not established here. The inspected scripts provide a concrete starting point, not a certified one-command reproduction.

If the ACM PDF becomes available, reconcile this guide against its exact methodology, parameter tables, figure captions, limitations, and source references before claiming full-paper coverage.

## Sources

- A1: [Authors' official WNS3 2022 slides](https://www.nsnam.org/workshops/wns3-2022/08-chatterjee-slides.pdf). Downloaded and text-extracted; the Figure 6 slide was also visually inspected.
- A2: [ns-3 project wiki, publication-artifact section](https://www.nsnam.org/wiki/GSOC2021DCE#Artifacts_for_Workshop_on_ns-3_publication).
- A3: [Pinned performance repository](https://github.com/tomhenderson/ns-3-dce/tree/a13b4433468f0c74db1a89b78e7819f9338bc09c), including the scripts named above.
- [Publication DOI; full text unavailable during review](https://doi.org/10.1145/3532577.3532606).
- [Crossref metadata](https://api.crossref.org/works/10.1145/3532577.3532606), including publication identity and CC BY-SA 4.0 license metadata.
- [GSoC modernization site](https://ns-3-dce-linux-upgrade.github.io/).

No complete ACM text or unverified numerical benchmark table is reproduced in this companion.
