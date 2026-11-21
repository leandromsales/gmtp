/*
 * media-server <gmtp|tcp> <rate B/s> <duration s> [flowname]
 *
 * Live CBR media source for the article-1 scenarios (A1, A2, B1).
 * A thread accepts receivers (TCP: one per receiver; GMTP: the relay or
 * the receivers, as the protocol decides) and the main loop sends one
 * MEDIA_PKT packet per tick to every accepted socket, for <duration>
 * seconds after the first accept. TCP sends use MSG_DONTWAIT: a live
 * source does not wait for a slow receiver, so a full send buffer counts
 * as a skipped packet (live-media semantics). Summary in media-server.txt.
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>

#include "media-common.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static std::vector<int> conns;
static int lsock = -1;
static volatile int stop_accept;

static void *acceptor(void *)
{
	/*
	 * Non-blocking accept polled every 20 ms, so main() can stop and join
	 * this thread before the process exits. Exiting with this thread still
	 * blocked in accept() made DCE free a fiber stack it was still running
	 * on (SIGSEGV in munmap at the end of GMTP trials).
	 */
	while (!stop_accept) {
		struct sockaddr_storage ss;
		socklen_t sl = sizeof(ss);
		int s = accept(lsock, (struct sockaddr *)&ss, &sl);
		if (s < 0) {
			if (errno == EINTR)
				continue;
			sleep_us(errno == EAGAIN || errno == EWOULDBLOCK ?
				 20000 : 100000);
			continue;
		}
		pthread_mutex_lock(&lock);
		conns.push_back(s);
		printf("accepted conn %zu at %.3f s\n", conns.size(),
		       now_us() / 1e6);
		fflush(stdout);
		pthread_mutex_unlock(&lock);
	}
	return NULL;
}

static void write_summary(const char *proto, int rate, double dur,
			  unsigned long sent, unsigned long sends,
			  unsigned long skipped, size_t nconn, uint64_t t0,
			  uint64_t t1, int done)
{
	FILE *f = fopen("media-server.txt", "w");
	if (!f)
		return;
	fprintf(f, "proto=%s rate_Bps=%d duration_s=%.1f pkts_generated=%lu "
		"pkt_sends=%lu sends_skipped=%lu conns=%zu t_start_s=%.6f "
		"t_end_s=%.6f done=%d\n", proto, rate, dur, sent, sends,
		skipped, nconn, t0 / 1e6, t1 / 1e6, done);
	fclose(f);
}

int main(int argc, char *argv[])
{
	if (argc < 4) {
		fprintf(stderr, "usage: media-server <gmtp|tcp> <rate> <dur>\n");
		return 1;
	}
	const char *proto = argv[1];
	int rate = atoi(argv[2]);
	double dur = atof(argv[3]);
	const char *flow = argc > 4 ? argv[4] : "1234567812345678";
	int gmtp = strcmp(proto, "tcp") != 0;

	lsock = gmtp ? socket(PF_INET, SOCK_GMTP, IPPROTO_GMTP)
		     : socket(PF_INET, SOCK_STREAM, 0);
	if (lsock < 0) {
		perror("socket");
		return 1;
	}
	int one = 1;
	setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (gmtp) {
		setsockopt(lsock, SOL_GMTP, MEDIA_GMTP_SOCKOPT_FLOWNAME, flow, 16);
		setsockopt(lsock, SOL_GMTP, MEDIA_GMTP_SOCKOPT_MEDIA_RATE,
			   &rate, sizeof(rate));
	}
	struct sockaddr_in a;
	memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_port = htons(MEDIA_PORT);
	a.sin_addr.s_addr = INADDR_ANY;
	if (bind(lsock, (struct sockaddr *)&a, sizeof(a)) < 0) {
		perror("bind");
		return 1;
	}
	if (listen(lsock, 512) < 0) {
		perror("listen");
		return 1;
	}
	printf("media-server proto=%s rate=%d B/s dur=%.1f s listening\n",
	       proto, rate, dur);
	fflush(stdout);
	fcntl(lsock, F_SETFL, fcntl(lsock, F_GETFL, 0) | O_NONBLOCK);
	pthread_t th;
	pthread_create(&th, NULL, acceptor, NULL);

	/* wait for the first receiver */
	for (;;) {
		pthread_mutex_lock(&lock);
		size_t n = conns.size();
		pthread_mutex_unlock(&lock);
		if (n)
			break;
		sleep_us(10000);
	}

	char buf[MEDIA_PKT];
	memset(buf, 'x', sizeof(buf));
	struct media_hdr *h = (struct media_hdr *)buf;
	h->magic = MEDIA_MAGIC;
	uint64_t gap = (uint64_t)((double)(MEDIA_PKT + MEDIA_HDR_OVH) * 1e6 / rate);
	uint64_t t0 = now_us(), tend = t0 + (uint64_t)(dur * 1e6);
	uint64_t next = t0;
	unsigned long seq = 0, sends = 0, skipped = 0;
	size_t nconn = 0;
	while (now_us() < tend) {
		uint64_t t = now_us();
		if (t < next)
			sleep_us(next - t);
		next += gap;
		h->seq = (uint32_t)seq;
		h->t_us = now_us();
		pthread_mutex_lock(&lock);
		std::vector<int> cs = conns;
		pthread_mutex_unlock(&lock);
		nconn = cs.size();
		for (size_t i = 0; i < cs.size(); ++i) {
			if (cs[i] < 0)
				continue;
			ssize_t r = send(cs[i], buf, MEDIA_PKT,
					 gmtp ? 0 : MSG_DONTWAIT | MSG_NOSIGNAL);
			if (r == MEDIA_PKT)
				++sends;
			else
				++skipped;
		}
		++seq;
		if (seq % 500 == 0)
			write_summary(proto, rate, dur, seq, sends, skipped,
				      nconn, t0, now_us(), 0);
	}
	/* end markers (several: GMTP has no retransmission) */
	h->seq = MEDIA_END_SEQ;
	pthread_mutex_lock(&lock);
	std::vector<int> cs = conns;
	pthread_mutex_unlock(&lock);
	for (int k = 0; k < 6; ++k) {
		h->t_us = now_us();
		for (size_t i = 0; i < cs.size(); ++i)
			send(cs[i], buf, MEDIA_PKT, gmtp ? 0 : MSG_NOSIGNAL);
		sleep_us(gap);
	}
	write_summary(proto, rate, dur, seq, sends, skipped, cs.size(), t0,
		      now_us(), 1);
	printf("media-server done: generated=%lu sends=%lu skipped=%lu conns=%zu\n",
	       seq, sends, skipped, cs.size());
	fflush(stdout);
	stop_accept = 1;
	pthread_join(th, NULL);
	sleep(3);
	pthread_mutex_lock(&lock);
	cs = conns;
	pthread_mutex_unlock(&lock);
	for (size_t i = 0; i < cs.size(); ++i)
		close(cs[i]);
	close(lsock);
	return 0;
}
