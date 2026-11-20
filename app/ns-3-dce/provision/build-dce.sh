#!/usr/bin/env bash
# Build DCE 1.12 (ParthPratim glibc-build), ns-3.35 and liblinux.so with
# GMTP from the pinned NUSE 5.10.47 tree. Run inside the image built from
# app/ns-3-dce/provision/Dockerfile (see provision/README.md).
#
# Layout (all gitignored):
#   linux/ns3/linux-net-next-nuse-latest/                     kernel library
#   .../tools/testing/libos/buildtop/bake                     Bake checkout
#   .../tools/testing/libos/buildtop/source/ns-3-dce          DCE
#   .../tools/testing/libos/buildtop/build                    install prefix
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../../.." && pwd)
NUSE="$REPO_ROOT/linux/ns3/linux-net-next-nuse-latest"
BT="$NUSE/tools/testing/libos/buildtop"
JOBS=${JOBS:-$(nproc)}
export GIT_TERMINAL_PROMPT=0

# Pins. Bake and the libos helper are fetched at fixed commits. DCE
# (glibc-build) and some Bake modules follow moving refs; their resolved
# SHAs are written to $BT/MANIFEST after the download step.
BAKE_URL="https://gitlab.com/ParthPratim1/bake.git"
BAKE_BRANCH="dce-1.12-linux-5.10"
BAKE_SHA="29af6127b36f54b2fb144b844b43c26ed98c8d08"
LIBOS_TOOLS_URL="https://github.com/ParthPratim/linux-libos-tools"
LIBOS_TOOLS_SHA="782531b009a129c4200df7e5882d939dcb61a404"

# MANIFEST: pins and resolved SHAs of everything that goes into a run.
# Rewritten at the end of every build (also with SKIP_BAKE=1), so it never
# lags behind the tree that produced bin_dce/liblinux.so. The NUSE tree is
# a nested clone (or a snapshot with .gmtp-tree-sha); "git -C $NUSE" on a
# snapshot would silently report the enclosing gmtp repo instead.
tree_sha() {
	local d="$1"
	if [[ -d "$d/.git" ]]; then
		git -C "$d" rev-parse HEAD
	elif [[ -f "$d/.gmtp-tree-sha" ]]; then
		echo "$(cat "$d/.gmtp-tree-sha") (snapshot)"
	elif [[ -n "${2:-}" ]]; then
		echo "$2 (pinned; tree has no .git)"
	else
		echo "unknown"
	fi
}
write_manifest() {
	{
		echo "bake $(git -C "$BT/bake" rev-parse HEAD) + bake-dce-linux-gmtp.patch"
		local pin
		pin=$(sed -n 's/^NS3_SHA="\([0-9a-f]*\)"/\1/p' "$REPO_ROOT/linux/checkout-trees.sh")
		echo "nuse $(tree_sha "$NUSE" "$pin")"
		echo "libos-tools $(tree_sha "$NUSE/arch/lib/tools" "$LIBOS_TOOLS_SHA")"
		local dirty=""
		git -C "$REPO_ROOT" diff --quiet HEAD -- gmtp app linux 2>/dev/null || dirty=" (dirty)"
		echo "gmtp $(git -C "$REPO_ROOT" rev-parse HEAD)$dirty"
		for d in "$BT"/source/*/; do
			if [[ -d "$d/.git" ]]; then
				echo "$(basename "$d") $(git -C "$d" rev-parse HEAD)"
			fi
		done
		local lib
		lib=$(ls "$BT"/build/bin_dce/libsim-linux-*.so 2>/dev/null | head -1)
		[[ -n "$lib" ]] && echo "libsim-sha256 $(sha256sum "$lib" | cut -d" " -f1) $(basename "$lib")"
		echo "written $(date -u +%FT%TZ)"
	} > "$BT/MANIFEST"
	cat "$BT/MANIFEST"
}

step() { printf '\n=== %s (%s)\n' "$*" "$(date -u +%FT%TZ)"; }

step "kernel tree"
# Idempotent: checks out if absent, (re)applies registration + build fixes, links net/gmtp.
"$REPO_ROOT/linux/checkout-trees.sh" --ns3

step "libos tools at $LIBOS_TOOLS_SHA"
if [[ ! -d "$NUSE/arch/lib/tools/.git" ]]; then
	git clone -q "$LIBOS_TOOLS_URL" "$NUSE/arch/lib/tools"
fi
git -C "$NUSE/arch/lib/tools" checkout -q "$LIBOS_TOOLS_SHA"

step "liblinux.so (make library ARCH=lib)"
cd "$NUSE"
[[ -f .config ]] || make defconfig ARCH=lib
# The Makefile defaults to git://, which GitHub no longer serves.
# "make library" also builds the NUSE/rump userspace stack in
# arch/lib/tools, whose submodules use dead git:// and code.google.com
# URLs. DCE only needs libsim. Make those clones fail fast (this rewrite
# lives in the container's throwaway ~/.gitconfig) and keep going (-k)
# so the libsim target is still built.
git config --global url."https://invalid.invalid/".insteadOf "git://"
make -k library ARCH=lib -j"$JOBS" LIBOS_GIT_REPO="$LIBOS_TOOLS_URL" || true
git config --global --unset-all url."https://invalid.invalid/".insteadOf
KVER=$(make -s kernelversion)
for f in "liblinux-$KVER.so" "arch/lib/tools/libsim-linux-$KVER.so"; do
	[[ -f "$f" ]] || { echo "$f was not built" >&2; exit 1; }
done
grep -E '^CONFIG_GMTP' .config

step "Bake at $BAKE_SHA"
mkdir -p "$BT"
if [[ ! -d "$BT/bake/.git" ]]; then
	git clone -q --branch "$BAKE_BRANCH" --single-branch "$BAKE_URL" "$BT/bake"
	git -C "$BT/bake" checkout -q --detach "$BAKE_SHA"
	git -C "$BT/bake" apply "$SCRIPT_DIR/bake-dce-linux-gmtp.patch"
fi
cd "$BT/bake"
export PATH="$BT/build/bin:$BT/build/bin_dce:$PATH"
export LD_LIBRARY_PATH="$BT/build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DCE_PATH="$BT/build/bin_dce:$BT/build/sbin"
if [[ ! -f bakefile.xml ]]; then
	./bake.py configure -e dce-linux-1.12 --sourcedir="$BT/source" -i "$BT/build"
fi
if [[ "${SKIP_BAKE:-0}" == 1 && -f "$BT/MANIFEST" && -d "$BT/build/lib" ]]; then
	# Rebuild only liblinux/libsim and the GMTP scenarios.
	step "SKIP_BAKE=1: reusing $BT/build"
else
./bake.py check || true
step "bake download"
./bake.py download
write_manifest

step "bake build"
./bake.py build -j"$JOBS" -vvv
fi

step "install liblinux.so into bin_dce"
mkdir -p "$BT/build/bin_dce"
lib="$NUSE/arch/lib/tools/libsim-linux-$(make -s -C "$NUSE" kernelversion).so"
cp -f "$lib" "$BT/build/bin_dce/"
ln -sf "$(basename "$lib")" "$BT/build/bin_dce/liblinux.so"
sha256sum "$BT/build/bin_dce/$(basename "$lib")" | tee "$BT/LIBSIM.sha256"
step "GMTP scenarios (myscripts/gmtp)"
"$REPO_ROOT/app/ns-3-dce/configure"
cd "$BT/source/ns-3-dce"
./waf configure --prefix="$BT/build" --with-ns3="$BT/build" \
	--with-elf-loader="$BT/build/lib" --with-glibc="$BT/build/glibc" \
	--with-libaspect="$BT/build" --enable-kernel-stack="$NUSE/arch"
./waf build -j"$JOBS"
echo "gmtp-tree $(git -C "$REPO_ROOT" rev-parse HEAD)" | tee -a "$BT/LIBSIM.sha256"
ls -l build/bin/dce-gmtp-* build/bin_dce/gmtp-* 2>/dev/null || true
step "MANIFEST"
write_manifest
step "done"
