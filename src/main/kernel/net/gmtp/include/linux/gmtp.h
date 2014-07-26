#ifndef LINUX_GMTP_H_
#define LINUX_GMTP_H_

#include <net/inet_sock.h>
#include <net/inet_connection_sock.h>
#include <net/tcp_states.h>


#include "../uapi/linux/gmtp.h"

#define IPPROTO_GMTP 254
#define SOCK_GMTP 7

//TODO Study states
enum gmtp_state {
	GMTP_OPEN	     = TCP_ESTABLISHED,
	GMTP_REQUESTING	     = TCP_SYN_SENT,
	GMTP_LISTEN	     = TCP_LISTEN,
	GMTP_RESPOND	     = TCP_SYN_RECV,
	GMTP_ACTIVE_CLOSEREQ = TCP_FIN_WAIT1,
	GMTP_PASSIVE_CLOSE   = TCP_CLOSE_WAIT,
	GMTP_CLOSING	     = TCP_CLOSING,
	GMTP_TIME_WAIT	     = TCP_TIME_WAIT,
	GMTP_CLOSED	     = TCP_CLOSE
};

enum gmtp_role {
	GMTP_ROLE_UNDEFINED,
	GMTP_ROLE_LISTEN,
	GMTP_ROLE_CLIENT,
	GMTP_ROLE_SERVER,
};

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

	enum gmtp_role	gmtps_role:2;
};

static inline struct gmtp_sock *gmtp_sk(const struct sock *sk)
{
	return (struct gmtp_sock *)sk;
}

static inline struct dccp_hdr *gmtp_zeroed_hdr(struct sk_buff *skb, int headlen)
{
	skb_push(skb, headlen);
	skb_reset_transport_header(skb);
	return memset(skb_transport_header(skb), 0, headlen);
}

#endif /* LINUX_GMTP_H_ */
