#!/bin/bash
# run-wvm.sh <dir> <trials.txt>
# Wendell's VM experiments (dissertation 4.1) on a real kernel: four
# QEMU/KVM guests (server, router, c1, c2) on one host, booting the host
# net-next 7.3 tree with GMTP (bzImage) and a busybox initramfs. <dir> holds
# bzImage, initramfs.cpio.gz and bin/{media-server,media-client,gmtp-inter}.
# Trial line: <name> <proto gmtp|tcp|udp> <inter on|off> <clients 1|10>
#             <rate B/s> <duration s> <seed>
# Results: <dir>/shared/res/<name>/ and <dir>/wvm.jsonl (collect-wvm.py).
set -u
D=$(readlink -f "$1"); TR=$(readlink -f "$2")
SH=$D/shared
log() { echo "$(date '+%F %T') $*" | tee -a $D/wvm.log; }

net_up() {
  for b in br1 br2; do ip link show $b >/dev/null 2>&1 || ip link add $b type bridge; ip link set $b up; done
  for t in tsrv:br1 trt0:br1 trt1:br2 tc1:br2 tc2:br2; do
    n=${t%%:*}; b=${t##*:}
    ip link show $n >/dev/null 2>&1 || ip tuntap add $n mode tap
    ip link set $n master $b; ip link set $n up
  done
  # bridges must not filter through the host's iptables
  sysctl -qw net.bridge.bridge-nf-call-iptables=0 2>/dev/null || true
}

boot() {
  pkill -f "wvm.role=" 2>/dev/null; sleep 1
  rm -rf $SH/cmd $SH/out; mkdir -p $SH/cmd $SH/out $SH/res
  cp -f $D/bin/* $SH/bin/ 2>/dev/null || { mkdir -p $SH/bin; cp -f $D/bin/* $SH/bin/; }
  declare -A NICS=( [server]="tsrv" [router]="trt0 trt1" [c1]="tc1" [c2]="tc2" )
  local i=0
  for r in server router c1 c2; do
    local nets="" k=0
    for t in ${NICS[$r]}; do
      nets="$nets -netdev tap,id=n$k,ifname=$t,script=no,downscript=no -device virtio-net-pci,netdev=n$k,mac=52:54:00:00:0$i:0$k"
      k=$((k+1))
    done
    qemu-system-x86_64 -enable-kvm -cpu host -smp 2 -m 1024 -nographic \
      -kernel $D/bzImage -initrd $D/initramfs.cpio.gz \
      -append "console=ttyS0 quiet panic=5 wvm.role=$r" \
      -fsdev local,id=fs0,path=$SH,security_model=none \
      -device virtio-9p-pci,fsdev=fs0,mount_tag=shared $nets \
      -serial file:$D/console-$r.log -monitor none -display none \
      > /dev/null 2>&1 &
    i=$((i+1))
  done
  for s in $(seq 120); do
    [ $(ls $SH/out/*.boot 2>/dev/null | wc -l) -eq 4 ] && break; sleep 1
  done
  [ $(ls $SH/out/*.boot 2>/dev/null | wc -l) -eq 4 ] || { log "boot FAILED"; return 1; }
  log "guests up: $(cat $SH/out/*.boot | tr '\n' ';')"
  declare -gA SEQ=( [server]=0 [router]=0 [c1]=0 [c2]=0 )
}

# cmd <role> <script>: queue one command; its sequence number goes to CMDN
# (no command substitution: SEQ must be updated in this shell)
cmd() { local n=${SEQ[$1]}; echo "$2" > $SH/cmd/$1.$n.sh.tmp; mv $SH/cmd/$1.$n.sh.tmp $SH/cmd/$1.$n.sh; SEQ[$1]=$((n+1)); CMDN=$n; }
waitdone() { # waitdone <role> <n> <timeout s>
  for s in $(seq $(( $3 * 5 ))); do [ -f $SH/out/$1.$2.done ] && return 0; sleep 0.2; done; return 1; }

net_up
boot || exit 1
while read -r name proto inter ncl rate dur seed; do
  [ -z "$name" ] || [ "${name:0:1}" = "#" ] && continue
  [ -f $SH/res/$name/DONE ] && continue
  R=/shared/res/$name; rm -rf $SH/res/$name; mkdir -p $SH/res/$name
  echo "$name $proto $inter $ncl $rate $dur $seed" > $SH/res/$name/trial.txt
  if [ "$ncl" -ge 10 ]; then n1=$((ncl/2)); n2=$((ncl-n1)); else n1=$ncl; n2=0; fi
  RANDOM=$seed
  ok=1
  cmd router "/shared/bin/gmtp-inter $inter; cat /proc/net/dev > $R/router-netdev0.txt"
  waitdone router $CMDN 30 || ok=0
  cmd server "mkdir -p $R/server && cd $R/server && cat /proc/net/dev > netdev0.txt && /shared/bin/media-server $proto $rate $dur > out.txt 2>&1; cat /proc/net/dev > netdev1.txt"
  s=$CMDN
  sleep 1
  declare -A CN=()
  for c in c1 c2; do
    nc=$n1; [ $c = c2 ] && nc=$n2
    [ $nc -eq 0 ] && continue
    sc="cd $R;"
    for k in $(seq $nc); do
      st=$(( RANDOM % 1000000 ))
      sc="$sc (mkdir -p $c-$k && cd $c-$k && usleep $st && /shared/bin/media-client $proto 10.10.1.2 > out.txt 2>&1) &"
    done
    cmd $c "$sc wait"
    CN[$c]=$CMDN
  done
  waitdone server $s $(( ${dur%.*} + 90 )) || ok=0
  for c in "${!CN[@]}"; do waitdone $c ${CN[$c]} 60 || ok=0; done
  unset CN
  cmd router "cat /proc/net/dev > $R/router-netdev1.txt; /shared/bin/gmtp-inter off"
  waitdone router $CMDN 30 || ok=0
  if [ $ok = 1 ]; then touch $SH/res/$name/DONE; log "$name done"
  else log "$name TIMEOUT -> reboot guests"; boot || exit 1; fi
  sleep 3
done < $TR
python3 $(dirname "$(readlink -f "$0")")/collect-wvm.py $SH/res > $D/wvm.jsonl
log "all trials processed: $(ls $SH/res/*/DONE 2>/dev/null | wc -l) done"
pkill -f "wvm.role=" 2>/dev/null
