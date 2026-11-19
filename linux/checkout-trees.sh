#!/usr/bin/env bash
# Clone the pinned host and ns-3 kernel trees, apply GMTP registration
# patches and headers, then symlink net/gmtp from the tracked sources.
#
# The kernel clones stay gitignored. Do not commit them.
# Nested DCE (tools/testing/libos/buildtop/source/ns-3-dce) is not
# checked out by this script; follow docs/setup-instructions/.
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
export GIT_TERMINAL_PROMPT=0

HOST_DIR="$REPO_ROOT/linux/host/linux-net-next-latest"
HOST_SHA="45ad84d2800e4a092fb8d96006a533b2d0ab13f6"
HOST_PATCH="$REPO_ROOT/linux/patches/host-linux-net-next-latest"
# kernel.org first; googlesource if a SHA fetch is refused.
HOST_REMOTES=(
	"https://git.kernel.org/pub/scm/linux/kernel/git/netdev/net-next.git"
	"https://kernel.googlesource.com/pub/scm/linux/kernel/git/netdev/net-next"
)
HOST_SNAPSHOT="https://git.kernel.org/pub/scm/linux/kernel/git/netdev/net-next.git/snapshot/net-next-${HOST_SHA}.tar.gz"

NS3_DIR="$REPO_ROOT/linux/ns3/linux-net-next-nuse-latest"
NS3_SHA="4c7510b860629f1840fe76d5333e7fd071fd5698"
NS3_PATCH="$REPO_ROOT/linux/patches/ns3-linux-net-next-nuse-latest"
NS3_REMOTES=(
	"https://github.com/ParthPratim/net-next-nuse-5.10.47.git"
)
NS3_SNAPSHOT="https://github.com/ParthPratim/net-next-nuse-5.10.47/archive/${NS3_SHA}.tar.gz"

WANT_HOST=0
WANT_NS3=0
FORCE=0
SKIP_LINK=0

usage() {
	cat <<'USAGE' >&2
Usage: linux/checkout-trees.sh [--host|--ns3|--all] [--force] [--skip-link]

  --host       Clone and patch linux/host/linux-net-next-latest
  --ns3        Clone and patch linux/ns3/linux-net-next-nuse-latest
  --all        Both trees (default when no tree flag is given)
  --force      Replace a checkout that is not at the pinned SHA
  --skip-link  Do not run gmtp/gmtp.sh --apply after patching

Uses git clone/fetch with --filter=blob:none. When a remote refuses a
SHA fetch (common on kernel.org), the next remote is tried, then a
kernel.org or GitHub snapshot tarball as a last resort. Do not use
torvalds/linux.git. The ns-3-dce Bake tree is not cloned here.
USAGE
	exit 2
}

log() { printf '%s\n' "$*"; }
err() { printf 'error: %s\n' "$*" >&2; }

current_sha() {
	local dest="$1"
	if [[ -d "$dest/.git" ]]; then
		git -C "$dest" rev-parse HEAD
	elif [[ -f "$dest/.gmtp-tree-sha" ]]; then
		cat "$dest/.gmtp-tree-sha"
	else
		echo ""
	fi
}

rm_tree() {
	local dest="$1"
	if [[ -e "$dest" ]]; then
		log "removing $dest (--force)"
		rm -rf "$dest"
	fi
}

ensure_parent() {
	mkdir -p "$(dirname "$1")"
}

# Fetch one SHA into an existing git dir (empty or not).
fetch_sha_into() {
	local dest="$1" sha="$2" remote="$3"
	git -C "$dest" remote remove origin >/dev/null 2>&1 || true
	git -C "$dest" remote add origin "$remote"
	# Prefer a blob-filtered, shallow tip at the exact commit.
	if git -C "$dest" fetch --filter=blob:none --depth 1 origin "$sha"; then
		return 0
	fi
	# Some hosts allow the SHA but reject a shallow request.
	if git -C "$dest" fetch --filter=blob:none origin "$sha"; then
		return 0
	fi
	# Explicit refspec, still filtered.
	if git -C "$dest" fetch --filter=blob:none origin "+${sha}:refs/gmtp/base"; then
		return 0
	fi
	return 1
}

clone_git_at_sha() {
	local dest="$1" sha="$2"
	shift 2
	local remotes=("$@")
	local remote tmp

	ensure_parent "$dest"
	tmp="${dest}.gmtp-checkout-tmp"
	rm -rf "$tmp"

	for remote in "${remotes[@]}"; do
		log "trying $remote @ ${sha:0:12}"
		mkdir -p "$tmp"
		git -C "$tmp" init --quiet
		if fetch_sha_into "$tmp" "$sha" "$remote"; then
			git -C "$tmp" checkout --detach --quiet FETCH_HEAD 2>/dev/null \
				|| git -C "$tmp" checkout --detach --quiet "$sha" \
				|| git -C "$tmp" checkout --detach --quiet refs/gmtp/base
			got=$(git -C "$tmp" rev-parse HEAD)
			if [[ "$got" != "$sha" ]]; then
				err "fetched $got from $remote, expected $sha"
				rm -rf "$tmp"
				continue
			fi
			mv "$tmp" "$dest"
			log "checked out $sha from $remote"
			return 0
		fi
		rm -rf "$tmp"
		log "SHA fetch failed at $remote (will try the next source)"
	done
	return 1
}

unpack_snapshot() {
	local dest="$1" sha="$2" url="$3"
	local tmp tarball unpack
	ensure_parent "$dest"
	tmp="${dest}.gmtp-snapshot-tmp"
	rm -rf "$tmp"
	mkdir -p "$tmp"
	tarball="$tmp/tree.tgz"
	log "trying snapshot $url"
	if ! curl -fsSL --retry 3 --retry-delay 2 -o "$tarball" "$url"; then
		rm -rf "$tmp"
		return 1
	fi
	mkdir -p "$tmp/unpack"
	if ! tar -C "$tmp/unpack" -xzf "$tarball"; then
		rm -rf "$tmp"
		return 1
	fi
	# Snapshot tarballs wrap the tree in a single top-level directory.
	unpack=$(find "$tmp/unpack" -mindepth 1 -maxdepth 1 -type d | head -n 1)
	if [[ -z "$unpack" ]]; then
		err "snapshot $url had no top-level directory"
		rm -rf "$tmp"
		return 1
	fi
	printf '%s\n' "$sha" > "$unpack/.gmtp-tree-sha"
	# Keep the marker out of any later accidental git add via exclusion in .gitignore of the whole tree.
	mv "$unpack" "$dest"
	rm -rf "$tmp"
	log "unpacked snapshot of $sha into $dest (no .git; SHA recorded in .gmtp-tree-sha)"
	return 0
}

checkout_tree() {
	local dest="$1" sha="$2" snapshot="$3"
	shift 3
	local remotes=("$@")
	local have

	if [[ -e "$dest" ]]; then
		have=$(current_sha "$dest")
		if [[ "$have" == "$sha" ]]; then
			log "already at $sha: $dest"
			return 0
		fi
		if [[ "$FORCE" -eq 1 ]]; then
			rm_tree "$dest"
		else
			err "$dest exists at ${have:-unknown}, expected $sha (pass --force to replace)"
			exit 1
		fi
	fi

	if clone_git_at_sha "$dest" "$sha" "${remotes[@]}"; then
		return 0
	fi
	if [[ -n "$snapshot" ]] && unpack_snapshot "$dest" "$sha" "$snapshot"; then
		return 0
	fi
	err "could not obtain $dest at $sha"
	err "tried remotes: ${remotes[*]}"
	[[ -n "$snapshot" ]] && err "and snapshot: $snapshot"
	err "Do not fall back to torvalds/linux.git. See linux/AGENTS.md and linux/patches/README.md."
	exit 1
}

already_registered() {
	local dest="$1"
	grep -q 'SOCK_GMTP' "$dest/include/linux/net.h" 2>/dev/null
}

# Optional build fixes for a tree (toolchain or upstream bugs that are not
# part of the GMTP registration). Applied once; skipped when already in.
apply_build_fixes() {
	local dest="$1" patchdir="$2"
	local patch="$patchdir/build-fixes.diff"

	[[ -f "$patch" ]] || return 0
	if patch -d "$dest" -p1 -R --dry-run --batch --silent < "$patch" >/dev/null 2>&1; then
		log "build fixes already present in $dest"
		return 0
	fi
	log "applying $patch"
	# --forward skips hunks already applied (exit 1 then); verify the end state.
	patch -d "$dest" -p1 --forward --batch --reject-file=- < "$patch" || true
	patch -d "$dest" -p1 -R --dry-run --batch --silent < "$patch" >/dev/null ||
		{ err "build fixes did not apply cleanly to $dest"; return 1; }
}

apply_registration() {
	local dest="$1" patchdir="$2"
	local patch="$patchdir/registration.diff"

	if already_registered "$dest"; then
		log "GMTP registration already present in $dest"
	else
		log "applying $patch"
		if [[ -d "$dest/.git" ]]; then
			git -C "$dest" apply --whitespace=nowarn "$patch"
		else
			patch -d "$dest" -p1 --forward --batch < "$patch"
		fi
	fi

	mkdir -p "$dest/include/linux" "$dest/include/uapi/linux" "$dest/include/net/netns"
	cp -f "$patchdir/include/linux/gmtp.h" "$dest/include/linux/gmtp.h"
	cp -f "$patchdir/include/uapi/linux/gmtp.h" "$dest/include/uapi/linux/gmtp.h"
	cp -f "$patchdir/include/net/netns/gmtp.h" "$dest/include/net/netns/gmtp.h"
	log "installed GMTP headers into $dest/include"
	apply_build_fixes "$dest" "$patchdir"
}

while [[ $# -gt 0 ]]; do
	case "$1" in
	--host) WANT_HOST=1 ;;
	--ns3) WANT_NS3=1 ;;
	--all) WANT_HOST=1; WANT_NS3=1 ;;
	--force) FORCE=1 ;;
	--skip-link) SKIP_LINK=1 ;;
	-h|--help) usage ;;
	*) err "unknown argument: $1"; usage ;;
	esac
	shift
done

if [[ "$WANT_HOST" -eq 0 && "$WANT_NS3" -eq 0 ]]; then
	WANT_HOST=1
	WANT_NS3=1
fi

if [[ "$WANT_HOST" -eq 1 ]]; then
	checkout_tree "$HOST_DIR" "$HOST_SHA" "$HOST_SNAPSHOT" "${HOST_REMOTES[@]}"
	apply_registration "$HOST_DIR" "$HOST_PATCH"
fi

if [[ "$WANT_NS3" -eq 1 ]]; then
	checkout_tree "$NS3_DIR" "$NS3_SHA" "$NS3_SNAPSHOT" "${NS3_REMOTES[@]}"
	apply_registration "$NS3_DIR" "$NS3_PATCH"
fi

if [[ "$SKIP_LINK" -eq 0 ]]; then
	# Link only the trees requested in this run.
	if [[ "$WANT_HOST" -eq 1 && "$WANT_NS3" -eq 1 ]]; then
		log "linking net/gmtp via gmtp/gmtp.sh --apply"
		"$REPO_ROOT/gmtp/gmtp.sh" --apply
	elif [[ "$WANT_HOST" -eq 1 ]]; then
		log "linking host net/gmtp via gmtp/gmtp.sh --apply --host"
		"$REPO_ROOT/gmtp/gmtp.sh" --apply --host
	else
		log "linking ns-3 net/gmtp via gmtp/gmtp.sh --apply --ns3"
		"$REPO_ROOT/gmtp/gmtp.sh" --apply --ns3
	fi
fi

log "done."
