#!/usr/bin/env python3
"""Pilot analysis for article 1 (docs/paper1/experimental-design.md, 7, 9.2).

Input: pilot.jsonl (one collect-trial.py row per trial).
Output: pilot.csv, and on stdout a Markdown report with, for every A1
metric, mean and 95% CI (Student t) per protocol, Welch t-test, the
Welch CI of the difference, Mann-Whitney U, plus wall time / RSS and the
cost projection of the full campaign.

Usage: analyze-pilot.py pilot.jsonl outdir [--price USD/h] [--vcpus N]
                         [--trials 1240] [--parallel P]
"""
import argparse, csv, json, math, os, statistics as st
from scipy import stats

METRICS = [
    ('goodput_kbps_mean', 'goodput médio por receptor (kbit/s)'),
    ('loss_session', 'perda na sessão (fração)'),
    ('loss_vs_generated', 'perda vs. gerado (fração)'),
    ('delay_mean_ms', 'atraso médio fim a fim (ms)'),
    ('delay_p95_ms_mean', 'atraso p95 médio (ms)'),
    ('startup_s_mean', 'atraso de inicialização (s)'),
    ('pkts_delivered', 'pacotes entregues (soma dos receptores)'),
    ('cross_mbps', 'vazão do Cubic concorrente (Mbit/s)'),
    ('jain_bottleneck', 'índice de Jain no gargalo'),
    ('bottleneck_fwd_bytes', 'bytes no gargalo (sentido S->C)'),
    ('bottleneck_fwd_drops', 'descartes na fila do gargalo'),
    ('server_tx_bytes', 'bytes enviados pelo servidor'),
    ('wall_s', 'tempo de parede (s)'),
    ('maxrss_mb', 'pico de RSS (MB)'),
]


def ci(xs):
    n = len(xs)
    m = st.mean(xs)
    if n < 2:
        return m, float('nan')
    h = stats.t.ppf(0.975, n - 1) * st.stdev(xs) / math.sqrt(n)
    return m, h


def fmt(x, nd=3):
    if x is None or (isinstance(x, float) and math.isnan(x)):
        return 'n/d'
    if abs(x) >= 1e5:
        return '%.4g' % x
    return ('%.' + str(nd) + 'f') % x


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('jsonl')
    ap.add_argument('outdir')
    ap.add_argument('--price', type=float, default=0.135)
    ap.add_argument('--vcpus', type=int, default=8)
    ap.add_argument('--trials', type=int, default=1240)
    ap.add_argument('--parallel', type=int, default=7)
    a = ap.parse_args()
    rows = [json.loads(l) for l in open(a.jsonl) if l.strip()]
    os.makedirs(a.outdir, exist_ok=True)
    keys = sorted({k for r in rows for k in r})
    first = ['trial', 'scenario', 'proto', 'seed_run', 'rc', 'wall_s', 'maxrss_mb']
    keys = [k for k in first if k in keys] + [k for k in keys if k not in first]
    with open(os.path.join(a.outdir, 'pilot.csv'), 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for r in sorted(rows, key=lambda r: r['trial']):
            w.writerow(r)

    def num(r, k):
        v = r.get(k)
        try:
            v = float(v)
            return None if math.isnan(v) else v
        except (TypeError, ValueError):
            return None

    a1 = [r for r in rows if r.get('scenario') == 'a1']
    protos = sorted({r['proto'] for r in a1})
    out = ['# Piloto do artigo 1 — análise preliminar', '',
           'Ensaios: %d (A1: %d; demais: %d). IC de 95%% com t de Student; '
           'Welch (bicaudal) e Mann–Whitney (bicaudal) GMTP × TCP Cubic, '
           'sementes não pareadas.' % (len(rows), len(a1), len(rows) - len(a1)), '']
    if len(protos) >= 2:
        g = [r for r in a1 if r['proto'] == 'gmtp']
        t = [r for r in a1 if r['proto'] == 'tcp']
        out += ['## A1 (N=%s receptores, %s fluxo(s) Cubic concorrente(s))' % (
            a1[0].get('nclients'), a1[0].get('ncross')), '',
            '| métrica | GMTP média ± IC95 (n) | TCP Cubic média ± IC95 (n) | '
            'diferença (GMTP−TCP) e IC95 Welch | p Welch | p Mann–Whitney |',
            '|---|---|---|---|---|---|']
        for k, label in METRICS:
            xg = [num(r, k) for r in g if num(r, k) is not None]
            xt = [num(r, k) for r in t if num(r, k) is not None]
            if not xg or not xt:
                continue
            mg, hg = ci(xg)
            mt, ht = ci(xt)
            pw = pm = dlo = dhi = float('nan')
            if len(xg) > 1 and len(xt) > 1 and (st.pstdev(xg) > 0 or st.pstdev(xt) > 0):
                r_ = stats.ttest_ind(xg, xt, equal_var=False)
                pw = r_.pvalue
                c = r_.confidence_interval(0.95)
                dlo, dhi = c.low, c.high
                pm = stats.mannwhitneyu(xg, xt, alternative='two-sided').pvalue
            out.append('| %s | %s ± %s (%d) | %s ± %s (%d) | %s [%s; %s] | %s | %s |' % (
                label, fmt(mg), fmt(hg), len(xg), fmt(mt), fmt(ht), len(xt),
                fmt(mg - mt), fmt(dlo), fmt(dhi), fmt(pw, 4), fmt(pm, 4)))
        out.append('')
    other = [r for r in rows if r.get('scenario') != 'a1']
    if other:
        out += ['## Fumaça A2 e B1', '',
                '| ensaio | receptores | com dados | encerrados | perda sessão | '
                'atraso médio (ms) | startup (s) | parede (s) | RSS (MB) | rc |',
                '|---|---|---|---|---|---|---|---|---|---|']
        for r in sorted(other, key=lambda r: r['trial']):
            out.append('| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |' % (
                r['trial'], r.get('clients'), r.get('clients_rx'),
                r.get('clients_ended'), fmt(num(r, 'loss_session'), 4),
                fmt(num(r, 'delay_mean_ms')), fmt(num(r, 'startup_s_mean')),
                fmt(num(r, 'wall_s'), 1), fmt(num(r, 'maxrss_mb'), 0), r.get('rc')))
        out.append('')
    # cost projection
    walls = [num(r, 'wall_s') for r in a1 if num(r, 'wall_s')]
    if walls:
        wm = st.mean(walls)
        rss = max(num(r, 'maxrss_mb') or 0 for r in rows)
        ht = a.trials * wm / 3600
        vm_h = ht / a.parallel
        out += ['## Projeção da campanha (%d ensaios)' % a.trials, '',
                '- Parede média por ensaio A1 no piloto: %.1f s (%.2f min); pico de RSS no piloto: %.0f MB.' % (wm, wm / 60, rss),
                '- Horas-ensaio se todos custassem como A1: %.1f h.' % ht,
                '- Com %d ensaios em paralelo por c6i.2xlarge (%d vCPU) a USD %.3f/h spot: %.1f VM-h ≈ USD %.2f.' % (
                    a.parallel, a.vcpus, a.price, vm_h, vm_h * a.price),
                '- Fator de segurança 3× (A2 L=3–4 e B1 N=200 são maiores que A1 N=10): ≈ USD %.2f.' % (3 * vm_h * a.price),
                '']
    open(os.path.join(a.outdir, 'analise-preliminar.md'), 'w').write('\n'.join(out))
    print('\n'.join(out))


if __name__ == '__main__':
    main()
