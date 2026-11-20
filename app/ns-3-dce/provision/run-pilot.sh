#!/usr/bin/env bash
# Run a list of article-1 trials in parallel inside the gmtp-dce container.
#
# Usage (inside container): run-pilot.sh <trials.txt> <outdir> [parallel]
# trials.txt: one trial per line, "<id> <scenario> [scenario args...]",
# e.g. "a1-gmtp-s1 dce-gmtp-a1 --proto=gmtp --RngRun=1".
# Each trial runs in <outdir>/runs/<id> (run-scenario.sh), is summarised
# into <outdir>/results/<id>.json (collect-trial.py), and its DCE
# elf-cache is deleted to save disk. Trials already summarised are
# skipped, so the script can be re-run after an interruption.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LIST="${1:?trials}"; OUT="${2:?outdir}"; P="${3:-7}"
mkdir -p "$OUT/runs" "$OUT/results"
one() {
	local id="$1" scen="$2"; shift 2
	[[ -s "$OUT/results/$id.json" ]] && { echo "skip $id"; return 0; }
	local rd="$OUT/runs/$id"
	rm -rf "$rd"
	"$HERE/run-scenario.sh" "$scen" "$rd" -- "$@" > "$rd.log" 2>&1
	local rc=$?
	python3 "$HERE/collect-trial.py" "$rd" "$id" "scenario_bin=$scen" "rc=$rc" \
		"args=$(echo "$*" | tr ' ' '_')" > "$OUT/results/$id.json.tmp" &&
		mv "$OUT/results/$id.json.tmp" "$OUT/results/$id.json"
	rm -rf "$rd"/files-*/var/cache "$rd"/elf-cache 2>/dev/null
	du -sh "$rd" | sed "s/^/disk $id /"
	echo "done $id rc=$rc $(date -u +%FT%TZ)"
}
export -f one
export HERE OUT
grep -v '^\s*#' "$LIST" | grep -v '^\s*$' |
	xargs -P "$P" -L 1 bash -c 'one "$@"' _
cat "$OUT"/results/*.json > "$OUT/pilot.jsonl"
echo "all done $(date -u +%FT%TZ): $(wc -l < "$OUT/pilot.jsonl") trials"
