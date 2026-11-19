/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Userspace ABI for the Global Media Transmission Protocol.
 *
 * These numbers match the kernel uapi in include/uapi/linux/gmtp.h
 * (canonical 5.4 / live trees): SOCK_GMTP 7, IPPROTO_GMTP 254,
 * SOL_GMTP 300. Do not invent a third socket-level value.
 */
#ifndef GMTP_USER_ABI_H
#define GMTP_USER_ABI_H

#ifndef SOCK_GMTP
#define SOCK_GMTP 7
#endif

#ifndef IPPROTO_GMTP
#define IPPROTO_GMTP 254
#endif

#ifndef SOL_GMTP
#define SOL_GMTP 300
#endif

#define GMTP_FLOWNAME_LEN 16
#define GMTP_MAX_RELAY_NUM 69

enum gmtp_sockopt_codes {
	GMTP_SOCKOPT_FLOWNAME = 1,
	GMTP_SOCKOPT_MEDIA_RATE = 2,
	GMTP_SOCKOPT_MAX_TX_RATE = 3,
	GMTP_SOCKOPT_UCC_TX_RATE = 4,
	GMTP_SOCKOPT_GET_CUR_MSS = 5,
	GMTP_SOCKOPT_SERVER_RTT = 6,
	GMTP_SOCKOPT_SERVER_TIMEWAIT = 7,
	GMTP_SOCKOPT_PULL = 8,
	GMTP_SOCKOPT_ROLE_RELAY = 9,
	GMTP_SOCKOPT_RELAY_ENABLED = 10,
	GMTP_SOCKOPT_NDP_RCV = 11,
	GMTP_SOCKOPT_NDP_SENT = 12,
	GMTP_SOCKOPT_UCC_TYPE = 13
};

#endif /* GMTP_USER_ABI_H */
