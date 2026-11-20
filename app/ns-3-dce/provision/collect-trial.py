#!/usr/bin/env python3
"""Summarise one article-1 DCE trial (run dir of run-scenario.sh) as JSON.

Reads stdout.txt (EXP_TOPO / EXP_NODES / EXP_LINK / EXP_DONE lines),
time.txt (/usr/bin/time -v) and the per-node files-<id>/media-client.txt,
media-server.txt, bulk-sink.txt written by the DCE apps.

Usage: collect-trial.py <rundir> <trial_id> [key=value ...]  -> JSON line
"""
import json, math, os, re, statistics, sys


def kv(line):
    return dict(re.findall(r'(\w+)=(\S+)', line))


def num(x):
    try:
        v = float(x)
        return None if math.isnan(v) else v
    except (TypeError, ValueError):
        return None


def node_file(rd, nid, name):
    p = os.path.join(rd, 'files-%s' % nid, name)
    if os.path.exists(p):
        with open(p, errors='replace') as f:
            t = f.read().strip()
        return kv(t.splitlines()[-1]) if t else {}
    return None


def server_file(rd, nid):
    """media-server.txt of a server node. DCE sometimes resolves the
    server's last fopen() against another node's files-<id> (seen in B1);
    if the node's own copy is not final, take the final summary with the
    same t_start_s from any node and count it as misplaced."""
    s = node_file(rd, nid, 'media-server.txt') or {}
    if s.get('done') == '1':
        return s, 0
    import glob
    for p in glob.glob(os.path.join(rd, 'files-*', 'media-server.txt')):
        with open(p, errors='replace') as f:
            t = f.read().strip()
        o = kv(t.splitlines()[-1]) if t else {}
        if o.get('done') == '1' and s and o.get('t_start_s') == s.get('t_start_s'):
            return o, 1
    return s, 0


def main():
    rd, tid = sys.argv[1], sys.argv[2]
    row = {'trial': tid}
    for a in sys.argv[3:]:
        k, v = a.split('=', 1)
        row[k] = v
    roles, links, topo, done = {}, {}, {}, {}
    out = os.path.join(rd, 'stdout.txt')
    for line in open(out, errors='replace') if os.path.exists(out) else []:
        if line.startswith('EXP_TOPO'):
            topo = kv(line)
        elif line.startswith('EXP_NODES'):
            d = kv(line)
            roles[d['role']] = [x for x in d.get('ids', '').split(',') if x]
        elif line.startswith('EXP_LINK'):
            d = kv(line)
            links[d['name']] = d
        elif line.startswith('EXP_DONE'):
            done = kv(line)
    row.update({k: v for k, v in topo.items()})
    tt = os.path.join(rd, 'time.txt')
    if os.path.exists(tt):
        t = open(tt).read()
        m = re.search(r'Elapsed \(wall clock\) time.*: (?:(\d+):)?(\d+):([\d.]+)', t)
        if m:
            h = int(m.group(1) or 0)
            row['wall_s'] = round(h * 3600 + int(m.group(2)) * 60 + float(m.group(3)), 2)
        m = re.search(r'Maximum resident set size \(kbytes\): (\d+)', t)
        if m:
            row['maxrss_mb'] = round(int(m.group(1)) / 1024, 1)
        m = re.search(r'Percent of CPU this job got: (\d+)%', t)
        if m:
            row['cpu_pct'] = int(m.group(1))
        m = re.search(r'Exit status: (\d+)', t)
        row['exit'] = int(m.group(1)) if m else None
    hb = os.path.join(rd, 'hang-bt.txt')
    row['watchdog_fired'] = int(os.path.exists(hb) and os.path.getsize(hb) > 0)
    row['wall_run_s'] = num(done.get('wall_run_s'))
    row['sim_stop_s'] = num(done.get('sim_stop_s'))

    # server
    gen = 0
    srv = {nid: server_file(rd, nid) for nid in roles.get('server', [])}
    row['server_summary_misplaced'] = sum(m for _, m in srv.values())
    for nid in roles.get('server', []):
        s = srv[nid][0]
        gen += int(s.get('pkts_generated', 0))
        row['server_conns'] = int(s.get('conns', 0)) + row.get('server_conns', 0)
        row['server_sends'] = int(s.get('pkt_sends', 0)) + row.get('server_sends', 0)
        row['server_skipped'] = int(s.get('sends_skipped', 0)) + row.get('server_skipped', 0)
    # clients
    cl = []
    for nid in roles.get('client', []):
        c = node_file(rd, nid, 'media-client.txt')
        cl.append(c or {})
    # media-server.txt is rewritten every 500 packets; if the run died
    # before the final rewrite, the highest sequence seen by any receiver
    # is a tighter lower bound of what the source generated.
    mx = max([int(c.get('maxseq', 0)) for c in cl] or [0])
    fin = 0
    for nid in roles.get('server', []):
        fin += int(srv[nid][0].get('done', 0))
    row['server_summary_final'] = fin
    if mx > gen:
        gen = mx
    row['pkts_generated'] = gen
    n = len(cl)
    ok = [c for c in cl if int(c.get('uniq', 0)) > 0]
    row['clients'] = n
    row['clients_rx'] = len(ok)
    row['clients_ended'] = sum(int(c.get('ended', 0)) for c in cl)
    delivered = sum(int(c.get('uniq', 0)) for c in cl)
    row['pkts_delivered'] = delivered
    exp_sess = sum(int(c.get('maxseq', 0)) - int(c.get('minseq', 0)) for c in ok)
    row['loss_session'] = round(1 - delivered / exp_sess, 6) if exp_sess else None
    # loss against everything the source generated, per receiver
    row['loss_vs_generated'] = round(1 - delivered / (gen * n), 6) if gen and n else None
    w = sum(int(c.get('pkts', 0)) for c in ok)
    row['delay_mean_ms'] = round(sum(num(c.get('delay_mean_ms')) * int(c['pkts'])
                                     for c in ok) / w, 3) if w else None
    p95 = [num(c.get('delay_p95_ms')) for c in ok if num(c.get('delay_p95_ms')) is not None]
    row['delay_p95_ms_mean'] = round(statistics.mean(p95), 3) if p95 else None
    st = [num(c.get('startup_s')) for c in ok if num(c.get('startup_s')) is not None]
    row['startup_s_mean'] = round(statistics.mean(st), 4) if st else None
    gp = [num(c.get('goodput_Bps')) for c in ok if num(c.get('goodput_Bps')) is not None]
    row['goodput_kbps_mean'] = round(statistics.mean(gp) * 8 / 1000, 2) if gp else None

    # cross traffic
    thr = []
    for nid in roles.get('xsink', []):
        b = node_file(rd, nid, 'bulk-sink.txt') or {}
        thr.append(num(b.get('thr_bps')) or 0.0)
    if thr:
        row['cross_mbps'] = round(sum(thr) / 1e6, 3)
        # Jain over the flows that cross the bottleneck: the cross flows
        # plus the media flows (1 for gmtp with relay, n otherwise).
        mflows = 1 if row.get('proto') == 'gmtp' else max(n, 1)
        mrate = (statistics.mean(gp) * 8) if gp else 0.0
        xs = thr + [mrate] * mflows
        row['jain_bottleneck'] = round(sum(xs) ** 2 / (len(xs) * sum(x * x for x in xs)), 4) if any(xs) else None

    for name, d in links.items():
        if name in ('bottleneck_fwd', 'bottleneck_rev', 'server_lan'):
            row[name + '_bytes'] = int(d['tx_bytes'])
            row[name + '_drops'] = int(d['drops'])
    bb = [int(d['tx_bytes']) for k, d in links.items()
          if k.startswith(('tree_', 'geant_', 'access_', 'srvaccess_'))]
    if bb:
        row['backbone_bytes'] = sum(bb)
        row['backbone_drops'] = sum(int(d['drops']) for k, d in links.items()
                                    if k.startswith(('tree_', 'geant_', 'access_', 'srvaccess_')))
    srvl = [int(d['tx_bytes']) for k, d in links.items() if k.startswith('server_lan')]
    if srvl:
        row['server_tx_bytes'] = sum(srvl)
    print(json.dumps(row, sort_keys=True))


if __name__ == '__main__':
    main()
