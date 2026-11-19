#!/usr/bin/env bash
# Run one DCE GMTP scenario (one trial) inside the gmtp-dce:20.04 container,
# measuring wall time and max RSS with /usr/bin/time -v.
#
# Usage (inside container, repo mounted at /work/gmtp):
#   run-scenario.sh <scenario> <rundir> [-- scenario args...]
# Example:
#   run-scenario.sh dce-gmtp-simple /work/runs/simple-1
#   run-scenario.sh dce-gmtp-master /work/runs/master-1 -- --nclients=2
#
# Each trial gets its own --cwd (files-*/ and pcaps land there), so several
# trials can run in parallel on the same build tree.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../../.." && pwd)"
BT="$REPO/linux/ns3/linux-net-next-nuse-latest/tools/testing/libos/buildtop"
SCEN="${1:?scenario}"; RUNDIR="${2:?rundir}"; shift 2
[[ "${1:-}" == "--" ]] && shift
export PATH="$BT/build/bin:$BT/build/bin_dce:$PATH"
export LD_LIBRARY_PATH="$BT/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DCE_PATH="$BT/build/bin_dce:$BT/build/sbin"
mkdir -p "$RUNDIR"
cd "$BT/source/ns-3-dce"
ARGS="$SCEN"; [[ $# -gt 0 ]] && ARGS="$SCEN $*"
echo "=== $SCEN start $(date -u +%FT%TZ) rundir=$RUNDIR"
set +e
/usr/bin/time -v -o "$RUNDIR/time.txt" ./waf --run "$ARGS" --cwd="$RUNDIR" \
  > "$RUNDIR/stdout.txt" 2>&1
rc=$?
set -e
echo "=== $SCEN end $(date -u +%FT%TZ) rc=$rc"
grep -E "Elapsed|Maximum resident" "$RUNDIR/time.txt" || true
exit $rc
