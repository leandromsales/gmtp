#ifndef LINUX_GMTP_H_
#define LINUX_GMTP_H_

#include <net/inet_sock.h>

#include "../uapi/linux/gmtp.h"

#define IPPROTO_GMTP 254
#define SOCK_GMTP 7

/**
 * struct gmtp_request_sock  -  represent GMTP-specific connection request
 */
struct gmtp_request_sock {
	struct inet_request_sock dreq_inet_rsk;
	//TODO Specific gmtp fields
};

/**
 * struct gmtp_sock - GMTP socket state
 */
struct gmtp_sock {
	/* inet_connection_sock has to be the first member of gmtp_sock */
	struct inet_connection_sock gmtp_inet_connection;
};

static inline struct gmtp_sock *gmtp_sk(const struct sock *sk)
{
	return (struct gmtp_sock *)sk;
}

#endif /* LINUX_GMTP_H_ */
