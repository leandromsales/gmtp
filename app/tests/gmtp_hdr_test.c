/* Userspace checks for the GMTP header parser rules (issue #7). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "../include/gmtp.h"

#define GMTP_PKT_DATA 1
#define GMTP_PKT_REGISTER_REPLY 7
#define GMTP_PKT_ROUTE_NOTIFY 8
#define GMTP_PKT_INVALID 16 /* sentinel used by the kernel check */

struct fake_hdr {
	uint8_t type;
	uint16_t hdrlen;
	uint8_t nrelays;
	unsigned int min_len;
	unsigned int skb_len;
};

/* Mirror of gmtp_check_packet after the pskb_may_pull of the fixed header. */
static int check_hdr(const struct fake_hdr *h)
{
	unsigned int min_len = h->min_len;
	unsigned int hdrlen = h->hdrlen;
	unsigned int extra = 0;

	if (h->type >= GMTP_PKT_INVALID)
		return 1;
	if (hdrlen < min_len || hdrlen > h->skb_len)
		return 1;
	if (h->type == GMTP_PKT_REGISTER_REPLY || h->type == GMTP_PKT_ROUTE_NOTIFY) {
		if (h->nrelays > GMTP_MAX_RELAY_NUM)
			return 1;
		extra = h->nrelays * 20; /* sizeof(struct gmtp_hdr_relay) */
		if (hdrlen < min_len + extra)
			return 1;
	}
	return 0;
}

#define EXPECT_DROP(cond, name) do { \
	if (!(cond)) { fprintf(stderr, "FAIL %s: expected drop\n", name); return 1; } \
} while (0)
#define EXPECT_OK(cond, name) do { \
	if (cond) { fprintf(stderr, "FAIL %s: expected accept\n", name); return 1; } \
} while (0)

int main(void)
{
	struct fake_hdr h;

	/* hdrlen stored in bytes, not 32-bit words. */
	memset(&h, 0, sizeof(h));
	h.type = GMTP_PKT_DATA;
	h.min_len = 40; /* 36 + 4 */
	h.hdrlen = 40;
	h.skb_len = 64;
	EXPECT_OK(check_hdr(&h), "data-ok");

	h.hdrlen = 10;
	EXPECT_DROP(check_hdr(&h), "hdrlen-too-small");

	h.hdrlen = 40;
	h.skb_len = 20;
	EXPECT_DROP(check_hdr(&h), "hdrlen-past-skb");

	h.type = GMTP_PKT_INVALID;
	h.hdrlen = 40;
	h.skb_len = 64;
	EXPECT_DROP(check_hdr(&h), "bad-type");

	/* nrelays must be capped and covered by hdrlen. */
	memset(&h, 0, sizeof(h));
	h.type = GMTP_PKT_REGISTER_REPLY;
	h.min_len = 38;
	h.nrelays = 2;
	h.hdrlen = 38 + 2 * 20;
	h.skb_len = 200;
	EXPECT_OK(check_hdr(&h), "reply-ok");

	h.nrelays = GMTP_MAX_RELAY_NUM + 1;
	EXPECT_DROP(check_hdr(&h), "nrelays-too-large");

	h.nrelays = 5;
	h.hdrlen = 38; /* no room for the relay list */
	EXPECT_DROP(check_hdr(&h), "nrelays-not-in-hdrlen");

	/* Word-count comparison (the old kernel bug) would wrongly accept
	 * this: hdrlen 40 bytes vs min_len/4 = 10. */
	memset(&h, 0, sizeof(h));
	h.type = GMTP_PKT_DATA;
	h.min_len = 40;
	h.hdrlen = 20;
	h.skb_len = 64;
	EXPECT_DROP(check_hdr(&h), "byte-not-word-length");

	printf("gmtp_hdr_test: ok\n");
	return 0;
}
