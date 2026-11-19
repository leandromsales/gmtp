#!/usr/bin/env bash
# Build the GMTP host kernel (linux/host/linux-net-next-latest) as Debian
# packages on an Ubuntu VM and optionally install them.
#
# The config starts from the running distribution kernel, keeps only the
# modules loaded now (localmodconfig, so boot and cloud NIC/NVMe drivers
# stay), and builds GMTP and the GMTP relay in-tree.
#
# Usage: linux/build-host-kernel.sh [--install]
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
TREE="$REPO_ROOT/linux/host/linux-net-next-latest"
JOBS=${JOBS:-$(nproc)}

[[ -d "$TREE" ]] || "$REPO_ROOT/linux/checkout-trees.sh" --host

cd "$TREE"
if [[ ! -f .config ]]; then
	cp "/boot/config-$(uname -r)" .config
	scripts/config --set-str SYSTEM_TRUSTED_KEYS "" \
		--set-str SYSTEM_REVOCATION_KEYS "" \
		--disable MODULE_SIG --disable MODULE_SIG_ALL \
		--disable DEBUG_INFO_BTF --enable DEBUG_INFO_NONE \
		--disable DEBUG_INFO_DWARF5 --disable DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT \
		--disable WERROR \
		--set-str LOCALVERSION "-gmtp"
	make olddefconfig
	make localmodconfig < <(yes '')
	scripts/config --enable GMTP --enable GMTP_INTER
	make olddefconfig
fi
grep -E '^CONFIG_GMTP' .config

make -j"$JOBS" bindeb-pkg

if [[ "${1:-}" == "--install" ]]; then
	sudo dpkg -i ../linux-image-*-gmtp*_*.deb
fi
