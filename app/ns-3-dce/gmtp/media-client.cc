/*
 * media-client <gmtp|tcp> <server ip> [flowname]
 *
 * Receiver for media-server. Records packets delivered, unique sequence
 * numbers, one-way delay (mean, stddev, p50/p95/p99, max), startup delay
 * (app start -> first packet) and goodput. Rewrites media-client.txt every
 * 200 packets and at the end, so a run cut by Simulator::Stop still
 * leaves a summary.
 */
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>

#include "media-common.h"

#define HBINS 20000 /* 0.5 ms bins up to 10 s */
/*
 * Playback model for the QoE-proxy metrics (transport level, not PSNR):
 * a packet is on time if its one-way delay is at most PLAY_DEADLINE_US
 * (the 5 s maximum acceptable playback delay of the 2014 thesis).
 * on_time_frac = packets on time / packets expected in the session
 * (minseq..maxseq, from the first to the highest sequence number the
 * receiver got). continuity_index = fraction of 1 s media slots of
 * the session in which at least 95% of the expected packets were on time;
 * a sequence number is mapped to its slot by its send time (interpolated
 * between the first and last packets received for missing ones).
 */
#define PLAY_DEADLINE_US 5000000ull
#define IC_SLOT_US 1000000ull
#define IC_OK 0.95

static unsigned long hist[HBINS + 1];
static std::vector<unsigned char> seen;

static double pct(unsigned long n, double p)
{
	unsigned long want = (unsigned long)ceil(p * n), acc = 0;
	for (int i = 0; i <= HBINS; ++i) {
		acc += hist[i];
		if (acc >= want && want > 0)
			return (i + 0.5) * 0.5;
	}
	return NAN;
}

struct st {
	unsigned long pkts, uniq, dup, maxseq, minseq;
	double sum, sumsq, maxd;
	uint64_t tapp, tconn, tfirst, tlast;
	unsigned long ontime, seq_lo, seq_hi; /* seq_lo/hi: send-time anchors */
	uint64_t ts_lo, ts_hi;
	int ended, connect_tries;
};

static void dump(const char *proto, struct st *s)
{
	FILE *f = fopen("media-client.txt", "w");
	if (!f)
		return;
	double mean = s->uniq ? s->sum / s->pkts : NAN;
	double sd = s->pkts > 1 ? sqrt((s->sumsq - s->pkts * mean * mean) /
					  (s->pkts - 1)) : NAN;
	double span = s->tlast > s->tfirst ? (s->tlast - s->tfirst) / 1e6 : NAN;
	/* continuity index over the session minseq .. maxseq-1 */
	double ic = NAN, otf = NAN;
	unsigned long nexp = s->maxseq > s->minseq ? s->maxseq - s->minseq : 0;
	if (nexp > 0 && s->seq_hi > s->seq_lo && s->ts_hi > s->ts_lo) {
		double us_per_seq = (double)(s->ts_hi - s->ts_lo) /
				    (double)(s->seq_hi - s->seq_lo);
		unsigned long nslots = (unsigned long)(nexp * us_per_seq /
						       IC_SLOT_US) + 1;
		std::vector<unsigned> ok(nslots, 0), all(nslots, 0);
		for (unsigned long q = s->minseq; q < s->maxseq; ++q) {
			unsigned long k = (unsigned long)((q - s->minseq) *
						us_per_seq / IC_SLOT_US);
			if (k >= nslots)
				k = nslots - 1;
			all[k]++;
			if (q < seen.size() && seen[q] == 2)
				ok[k]++;
		}
		unsigned long good = 0, used = 0;
		for (unsigned long k = 0; k < nslots; ++k)
			if (all[k]) {
				used++;
				if (ok[k] >= IC_OK * all[k])
					good++;
			}
		ic = used ? (double)good / used : NAN;
		otf = (double)s->ontime / nexp;
	} else if (nexp > 0) {
		otf = (double)s->ontime / nexp;
	}
	fprintf(f, "proto=%s pkts=%lu uniq=%lu dup=%lu minseq=%lu maxseq=%lu "
		"delay_mean_ms=%.3f delay_sd_ms=%.3f delay_p50_ms=%.3f "
		"delay_p95_ms=%.3f delay_p99_ms=%.3f delay_max_ms=%.3f "
		"startup_s=%.6f connect_s=%.6f t_first_s=%.6f t_last_s=%.6f "
		"goodput_Bps=%.1f ended=%d connect_tries=%d on_time_frac=%.6f "
		"continuity_index=%.6f\n", proto, s->pkts,
		s->uniq, s->dup, s->minseq, s->maxseq, mean, sd, pct(s->pkts, 0.5),
		pct(s->pkts, 0.95), pct(s->pkts, 0.99), s->maxd,
		s->tfirst ? (s->tfirst - s->tapp) / 1e6 : NAN,
		s->tconn ? (s->tconn - s->tapp) / 1e6 : NAN,
		s->tfirst / 1e6, s->tlast / 1e6,
		span > 0 ? s->uniq * (double)MEDIA_PKT / span : NAN,
		s->ended, s->connect_tries, otf, ic);
	fclose(f);
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr, "usage: media-client <gmtp|tcp> <ip>\n");
		return 1;
	}
	const char *proto = argv[1];
	int gmtp = strcmp(proto, "tcp") != 0;
	const char *flow = argc > 3 ? argv[3] : "1234567812345678";
	struct st s;
	memset(&s, 0, sizeof(s));
	s.tapp = now_us();
	seen.assign(1 << 20, 0);

	struct sockaddr_in a;
	memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_port = htons(MEDIA_PORT);
	inet_pton(AF_INET, argv[2], &a.sin_addr);
	int fd = -1;
	for (int t = 0; t < 30; ++t) {
		fd = gmtp ? socket(AF_INET, SOCK_GMTP, IPPROTO_GMTP)
			  : socket(AF_INET, SOCK_STREAM, 0);
		if (gmtp)
			setsockopt(fd, SOL_GMTP, MEDIA_GMTP_SOCKOPT_FLOWNAME,
				   flow, 16);
		s.connect_tries = t + 1;
		if (connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0)
			break;
		close(fd);
		fd = -1;
		sleep(1);
	}
	if (fd < 0) {
		dump(proto, &s);
		printf("media-client: connect failed\n");
		return 1;
	}
	s.tconn = now_us();
	dump(proto, &s);
	char buf[MEDIA_PKT];
	for (;;) {
		ssize_t r = recv(fd, buf, MEDIA_PKT, gmtp ? 0 : MSG_WAITALL);
		if (r <= 0)
			break;
		if (r < (ssize_t)sizeof(struct media_hdr))
			continue;
		struct media_hdr *h = (struct media_hdr *)buf;
		if (h->magic != MEDIA_MAGIC)
			continue;
		if (h->seq == MEDIA_END_SEQ) {
			s.ended = 1;
			break;
		}
		uint64_t t = now_us();
		if (!s.tfirst) {
			s.tfirst = t;
			s.minseq = h->seq;
		}
		if (h->seq < s.minseq)
			s.minseq = h->seq;
		s.tlast = t;
		s.pkts++;
		if (h->seq >= seen.size())
			seen.resize(h->seq * 2 + 1, 0);
		double d = t > h->t_us ? (t - h->t_us) / 1000.0 : 0.0;
		if (seen[h->seq])
			s.dup++;
		else {
			int on = t <= h->t_us + PLAY_DEADLINE_US;
			seen[h->seq] = on ? 2 : 1;
			s.uniq++;
			s.ontime += on;
			if (!s.ts_lo || h->seq < s.seq_lo) {
				s.seq_lo = h->seq;
				s.ts_lo = h->t_us;
			}
			if (h->seq > s.seq_hi) {
				s.seq_hi = h->seq;
				s.ts_hi = h->t_us;
			}
		}
		if (h->seq + 1 > s.maxseq)
			s.maxseq = h->seq + 1;
		s.sum += d;
		s.sumsq += d * d;
		if (d > s.maxd)
			s.maxd = d;
		int b = (int)(d / 0.5);
		hist[b > HBINS ? HBINS : b]++;
		if (s.pkts % 200 == 0)
			dump(proto, &s);
	}
	dump(proto, &s);
	printf("media-client done: pkts=%lu uniq=%lu ended=%d\n", s.pkts,
	       s.uniq, s.ended);
	fflush(stdout);
	sleep(3); /* see gmtp-client.cc: do not die before the server */
	close(fd);
	return 0;
}
