#!/bin/bash
# build-initramfs.sh <out.cpio.gz>: busybox initramfs for the W-VM guests.
set -e
OUT=${1:-initramfs.cpio.gz}
D=$(mktemp -d)
mkdir -p $D/{bin,proc,sys,dev,shared,tmp}
cp /bin/busybox $D/bin/busybox; ln -s busybox $D/bin/sh
cp "$(dirname "$0")/init" $D/init; chmod +x $D/init
(cd $D && find . | cpio -o -H newc 2>/dev/null | gzip -9) > "$OUT"
rm -rf $D
echo "$OUT"
