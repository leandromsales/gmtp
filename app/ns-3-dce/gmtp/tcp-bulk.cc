/*
 * tcp-bulk sink <port>
 * tcp-bulk send <ip> <port> <duration s> [cc]
 *
 * Greedy TCP cross traffic for A1 (the competing flow at the bottleneck).
 * The sender sets TCP_CONGESTION (default cubic) and writes 64 KiB blocks
 * for <duration> seconds. The sink accepts one connection and rewrites
 * bulk-sink.txt every MiB with bytes and first/last receive times.
 */
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "media-common.h"

static void dump_sink(unsigned long long bytes, uint64_t t0, uint64_t t1,
		      int done)
{
	FILE *f = fopen("bulk-sink.txt", "w");
	if (!f)
		return;
	double span = t1 > t0 ? (t1 - t0) / 1e6 : 0;
	fprintf(f, "bytes=%llu t_first_s=%.6f t_last_s=%.6f thr_bps=%.1f done=%d\n",
		bytes, t0 / 1e6, t1 / 1e6, span > 0 ? bytes * 8.0 / span : 0.0,
		done);
	fclose(f);
}

int main(int argc, char *argv[])
{
	if (argc < 3)
		return 1;
	if (!strcmp(argv[1], "sink")) {
		int ls = socket(AF_INET, SOCK_STREAM, 0), one = 1;
		setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
		struct sockaddr_in a;
		memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_port = htons(atoi(argv[2]));
		bind(ls, (struct sockaddr *)&a, sizeof(a));
		listen(ls, 4);
		int s = accept(ls, NULL, NULL);
		static char buf[65536];
		unsigned long long bytes = 0, mark = 0;
		uint64_t t0 = 0, t1 = 0;
		for (;;) {
			ssize_t r = recv(s, buf, sizeof(buf), 0);
			if (r <= 0)
				break;
			t1 = now_us();
			if (!t0)
				t0 = t1;
			bytes += r;
			if (bytes - mark >= (1 << 20)) {
				mark = bytes;
				dump_sink(bytes, t0, t1, 0);
			}
		}
		dump_sink(bytes, t0, t1, 1);
		close(s);
		close(ls);
		return 0;
	}
	if (!strcmp(argv[1], "send") && argc >= 5) {
		const char *cc = argc > 5 ? argv[5] : "cubic";
		int s = socket(AF_INET, SOCK_STREAM, 0);
		if (setsockopt(s, IPPROTO_TCP, TCP_CONGESTION, cc, strlen(cc)))
			perror("TCP_CONGESTION");
		char got[16] = {0};
		socklen_t gl = sizeof(got);
		getsockopt(s, IPPROTO_TCP, TCP_CONGESTION, got, &gl);
		struct sockaddr_in a;
		memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_port = htons(atoi(argv[3]));
		inet_pton(AF_INET, argv[2], &a.sin_addr);
		int ok = -1;
		for (int t = 0; t < 30 && ok; ++t) {
			ok = connect(s, (struct sockaddr *)&a, sizeof(a));
			if (ok)
				sleep(1);
		}
		static char buf[65536];
		memset(buf, 'b', sizeof(buf));
		uint64_t tend = now_us() + (uint64_t)(atof(argv[4]) * 1e6);
		unsigned long long sent = 0;
		while (!ok && now_us() < tend) {
			ssize_t r = send(s, buf, sizeof(buf), MSG_NOSIGNAL);
			if (r <= 0)
				break;
			sent += r;
		}
		FILE *f = fopen("bulk-send.txt", "w");
		if (f) {
			fprintf(f, "cc=%s connected=%d bytes_sent=%llu\n", got,
				ok == 0, sent);
			fclose(f);
		}
		close(s);
		return 0;
	}
	return 1;
}
