// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Host-kernel smoke test for the GMTP socket API.
 *
 * Opens a GMTP socket, round-trips the flowname and media-rate sockopts,
 * and reads the MSS and non-data packet counters. Exits non-zero on the
 * first failure. Run on a kernel built with CONFIG_GMTP.
 *
 *   cc -O2 -Wall -I../include -o gmtp_socket_smoke gmtp_socket_smoke.c
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "gmtp.h"

#define CHECK(cond, what) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s: %s\n", what, strerror(errno)); \
		return 1; \
	} \
	printf("ok   %s\n", what); \
} while (0)

int main(void)
{
	const char flow[GMTP_FLOWNAME_LEN] = "1234567812345678";
	char flow_out[GMTP_FLOWNAME_LEN];
	socklen_t len;
	int fd, rate = 300000, val;

	fd = socket(AF_INET, SOCK_GMTP, IPPROTO_GMTP);
	CHECK(fd >= 0, "socket(AF_INET, SOCK_GMTP, IPPROTO_GMTP)");

	CHECK(setsockopt(fd, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, flow,
			 sizeof(flow)) == 0, "setsockopt FLOWNAME");
	len = sizeof(flow_out);
	CHECK(getsockopt(fd, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, flow_out,
			 &len) == 0, "getsockopt FLOWNAME");
	errno = EINVAL;
	CHECK(len == sizeof(flow) && !memcmp(flow, flow_out, sizeof(flow)),
	      "FLOWNAME round trip");

	CHECK(setsockopt(fd, SOL_GMTP, GMTP_SOCKOPT_MEDIA_RATE, &rate,
			 sizeof(rate)) == 0, "setsockopt MEDIA_RATE");
	len = sizeof(val);
	CHECK(getsockopt(fd, SOL_GMTP, GMTP_SOCKOPT_MEDIA_RATE, &val,
			 &len) == 0, "getsockopt MEDIA_RATE");
	errno = EINVAL;
	CHECK(val == rate, "MEDIA_RATE round trip");

	len = sizeof(val);
	CHECK(getsockopt(fd, SOL_GMTP, GMTP_SOCKOPT_GET_CUR_MSS, &val,
			 &len) == 0, "getsockopt GET_CUR_MSS");
	printf("     mss=%d\n", val);
	len = sizeof(val);
	CHECK(getsockopt(fd, SOL_GMTP, GMTP_SOCKOPT_NDP_RCV, &val,
			 &len) == 0, "getsockopt NDP_RCV");

	close(fd);
	printf("PASS\n");
	return 0;
}
