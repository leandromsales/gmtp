#!/usr/bin/env python3
"""collect-wvm.py <res dir>: one JSON line per W-VM trial (stdout)."""
import json, os, re, statistics, sys

def kv(path):
    try:
        line = open(path).read().split('\n')[0]
    except OSError:
        return {}
    return dict(x.split('=', 1) for x in line.split() if '=' in x)

def num(x):
    try:
        v = float(x)
        return None if v != v else v
    except (TypeError, ValueError):
        return None

root = sys.argv[1]
for name in sorted(os.listdir(root)):
    d = os.path.join(root, name)
    t = open(os.path.join(d, 'trial.txt')).read().split() if os.path.exists(os.path.join(d, 'trial.txt')) else []
    if len(t) < 7:
        continue
    row = dict(name=name, proto=t[1], inter=t[2], nclients=int(t[3]), rate=int(t[4]),
               duration=float(t[5]), seed=int(t[6]), done=os.path.exists(os.path.join(d, 'DONE')))
    cl = [kv(os.path.join(d, c, 'media-client.txt')) for c in sorted(os.listdir(d))
          if re.match(r'c[12]-\d+$', c)]
    ok = [c for c in cl if int(c.get('uniq', 0)) > 0]
    row['clients_rx'] = len(ok)
    row['clients_ended'] = sum(int(c.get('ended', 0)) for c in cl)
    rx = [num(c.get('goodput_Bps')) for c in ok if num(c.get('goodput_Bps'))]
    row['rx_Bps_mean'] = round(statistics.mean(rx), 1) if rx else None
    row['rx_Bps_min'] = round(min(rx), 1) if rx else None
    row['rx_Bps_max'] = round(max(rx), 1) if rx else None
    row['rx_Bps_total'] = round(sum(rx), 1) if rx else None
    exp = sum(int(c.get('maxseq', 0)) - int(c.get('minseq', 0)) for c in ok)
    got = sum(int(c.get('uniq', 0)) for c in ok)
    row['loss_session'] = round(1 - got / exp, 6) if exp else None
    for k in ('on_time_frac', 'continuity_index', 'startup_s'):
        v = [num(c.get(k)) for c in ok if num(c.get(k)) is not None]
        row[k + '_mean'] = round(statistics.mean(v), 6) if v else None
    s = {}
    try:
        for line in open(os.path.join(d, 'server', 'out.txt')):
            if line.startswith('EXP_SERVER_SUMMARY'):
                s = dict(x.split('=', 1) for x in line.split()[1:] if '=' in x)
    except OSError:
        pass
    row['server_conns'] = int(s['conns']) if 'conns' in s else None
    row['pkts_generated'] = int(s['pkts_generated']) if 'pkts_generated' in s else None
    # server NIC bytes during the trial (eth0 tx/rx)
    def eth0(p):
        try:
            for line in open(p):
                if line.strip().startswith('eth0:'):
                    f = line.split(':', 1)[1].split()
                    return int(f[0]), int(f[1]), int(f[8]), int(f[9])
        except OSError:
            return None
    a, b = eth0(os.path.join(d, 'server', 'netdev0.txt')), eth0(os.path.join(d, 'server', 'netdev1.txt'))
    if a and b:
        row['server_rx_bytes'], row['server_rx_pkts'] = b[0] - a[0], b[1] - a[1]
        row['server_tx_bytes'], row['server_tx_pkts'] = b[2] - a[2], b[3] - a[3]
    print(json.dumps(row, sort_keys=True))
