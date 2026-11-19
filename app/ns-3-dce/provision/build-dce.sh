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

step() { printf '\n=== %s (%s)\n' "$*" "$(date -u +%FT%TZ)"; }

step "kernel tree"
if [[ ! -d "$NUSE" ]]; then
	"$REPO_ROOT/linux/checkout-trees.sh" --ns3
fi

step "libos tools at $LIBOS_TOOLS_SHA"
if [[ ! -d "$NUSE/arch/lib/tools/.git" ]]; then
	git clone -q "$LIBOS_TOOLS_URL" "$NUSE/arch/lib/tools"
fi
git -C "$NUSE/arch/lib/tools" checkout -q "$LIBOS_TOOLS_SHA"

step "liblinux.so (make library ARCH=lib)"
cd "$NUSE"
[[ -f .config ]] || make defconfig ARCH=lib
# The Makefile defaults to git://, which GitHub no longer serves.
make library ARCH=lib -j"$JOBS" LIBOS_GIT_REPO="$LIBOS_TOOLS_URL"
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
./bake.py check || true
step "bake download"
./bake.py download
{
	echo "bake $(git -C "$BT/bake" rev-parse HEAD) + bake-dce-linux-gmtp.patch"
	echo "nuse $(git -C "$NUSE" rev-parse HEAD 2>/dev/null || cat "$NUSE/.gmtp-tree-sha")"
	echo "libos-tools $(git -C "$NUSE/arch/lib/tools" rev-parse HEAD)"
	echo "gmtp $(git -C "$REPO_ROOT" rev-parse HEAD)"
	for d in "$BT"/source/*/; do
		if [[ -d "$d/.git" ]]; then
			echo "$(basename "$d") $(git -C "$d" rev-parse HEAD)"
		fi
	done
} > "$BT/MANIFEST"
cat "$BT/MANIFEST"

step "bake build"
./bake.py build -j"$JOBS" -vvv

step "install liblinux.so into bin_dce"
mkdir -p "$BT/build/bin_dce"
lib=$(ls "$NUSE"/arch/lib/tools/libsim-linux-*.so 2>/dev/null | head -n1 || true)
[[ -n "$lib" ]] || lib=$(readlink -f "$NUSE/liblinux.so")
cp -f "$lib" "$BT/build/bin_dce/"
ln -sf "$(basename "$lib")" "$BT/build/bin_dce/liblinux.so"
sha256sum "$BT/build/bin_dce/$(basename "$lib")" | tee -a "$BT/MANIFEST"
step "done"
