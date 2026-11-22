/*
 * media-common.h - shared definitions for the article-1 DCE apps
 * (media-server, media-client, tcp-bulk).
 *
 * Every media packet carries a fixed header (magic, sequence number and
 * the sender's simulated send time in microseconds) followed by padding
 * up to MEDIA_PKT bytes, so the receiver can compute loss and one-way
 * delay. Under DCE gettimeofday() returns the simulated clock, which is
 * shared by every node, so receiver_time - send_time is the one-way delay.
 */
#ifndef MEDIA_COMMON_H_
#define MEDIA_COMMON_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define MEDIA_PKT      744          /* same payload size as gmtp-server */
#define MEDIA_MAGIC    0x474d5450u  /* "GMTP" */
#define MEDIA_END_SEQ  0xffffffffu
#define MEDIA_PORT     2000
#define MEDIA_HDR_OVH  56           /* GMTP/TCP + IP header estimate, as gmtp.h */
/*
 * Bytes per data packet that the GMTP sender's pacing counts
 * (packet_len() in net/gmtp/output.c): payload + GMTP data header (40)
 * + IP (20) + Ethernet (14). The media rate handed to the socket must be
 * in these units, or the socket paces slower than the application
 * generates packets and the send buffer fills up.
 */
#define MEDIA_GMTP_WIRE_OVH 74

#ifndef SOCK_GMTP
#define SOCK_GMTP      7
#endif
#ifndef IPPROTO_GMTP
#define IPPROTO_GMTP   254
#endif
#ifndef SOL_GMTP
#define SOL_GMTP       300
#endif
#define MEDIA_GMTP_SOCKOPT_FLOWNAME   1
#define MEDIA_GMTP_SOCKOPT_MEDIA_RATE 2
#define MEDIA_GMTP_SOCKOPT_ROLE_RELAY 9
#define MEDIA_GMTP_SOCKOPT_RELAY_ENABLED 10

struct media_hdr {
	uint32_t magic;
	uint32_t seq;
	uint64_t t_us;   /* actual send time (one-way delay) */
	/* nominal live schedule: packet seq is due at t0_us + seq * gap_us;
	 * n_nom packets in the whole stream (QoE proxies in media-client) */
	uint64_t t0_us;
	uint32_t gap_us;
	uint32_t n_nom;
} __attribute__((packed));

static inline uint64_t now_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000000ull + (uint64_t)tv.tv_usec;
}

static inline void sleep_us(uint64_t us)
{
	struct timespec ts;
	ts.tv_sec = us / 1000000ull;
	ts.tv_nsec = (long)(us % 1000000ull) * 1000;
	nanosleep(&ts, NULL);
}

#endif /* MEDIA_COMMON_H_ */
