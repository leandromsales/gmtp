#!/usr/bin/env bash
# Link the tracked GMTP sources into the kernel checkouts, or remove those links.
# Sources keep the kernel-relative paths under gmtp/host and gmtp/ns3.
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

HOST_SRC="$SCRIPT_DIR/host"
HOST_KERN="$REPO_ROOT/linux/host/linux-net-next-latest"
NS3_SRC="$SCRIPT_DIR/ns3"
NS3_KERN="$REPO_ROOT/linux/ns3/linux-net-next-nuse-latest"

usage() {
	echo "Usage: gmtp.sh --apply | --remove" >&2
	exit 2
}

rel_link() {
	python3 -c 'import os, sys; print(os.path.relpath(sys.argv[1], os.path.dirname(sys.argv[2])))' "$1" "$2"
}

apply_tree() {
	local src_root="$1"
	local kern_root="$2"
	local src rel dest expected actual

	if [[ ! -d "$kern_root" ]]; then
		echo "kernel tree not found: $kern_root" >&2
		exit 1
	fi
	if [[ ! -d "$src_root/net/gmtp" ]]; then
		echo "GMTP sources not found: $src_root/net/gmtp" >&2
		exit 1
	fi

	while IFS= read -r -d '' src; do
		rel="${src#"$src_root"/}"
		dest="$kern_root/$rel"
		expected=$(rel_link "$src" "$dest")
		if [[ -L "$dest" ]]; then
			actual=$(readlink "$dest")
			if [[ "$actual" == "$expected" ]]; then
				continue
			fi
			echo "refusing to replace symlink: $dest -> $actual" >&2
			exit 1
		fi
		if [[ -e "$dest" ]]; then
			echo "refusing to replace existing file: $dest" >&2
			exit 1
		fi
		mkdir -p "$(dirname "$dest")"
		ln -s "$expected" "$dest"
	done < <(find "$src_root" \( -type f -o -type l \) -print0)
}

remove_tree() {
	local src_root="$1"
	local kern_root="$2"
	local src rel dest expected actual dir

	if [[ ! -d "$kern_root" ]]; then
		echo "kernel tree not found: $kern_root" >&2
		exit 1
	fi

	while IFS= read -r -d '' src; do
		rel="${src#"$src_root"/}"
		dest="$kern_root/$rel"
		if [[ ! -L "$dest" ]]; then
			continue
		fi
		expected=$(rel_link "$src" "$dest")
		actual=$(readlink "$dest")
		if [[ "$actual" != "$expected" ]]; then
			echo "leaving unexpected symlink: $dest -> $actual" >&2
			continue
		fi
		rm "$dest"
	done < <(find "$src_root" \( -type f -o -type l \) -print0)

	dir="$kern_root/net/gmtp"
	if [[ -d "$dir" ]]; then
		find "$dir" -depth -type d -empty -exec rmdir {} \;
	fi
}

if [[ $# -ne 1 ]]; then
	usage
fi

case "$1" in
--apply)
	apply_tree "$HOST_SRC" "$HOST_KERN"
	apply_tree "$NS3_SRC" "$NS3_KERN"
	;;
--remove)
	remove_tree "$HOST_SRC" "$HOST_KERN"
	remove_tree "$NS3_SRC" "$NS3_KERN"
	;;
*)
	usage
	;;
esac
