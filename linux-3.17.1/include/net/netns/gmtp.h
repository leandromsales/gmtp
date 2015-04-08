#ifndef __NETNS_GMTP_H__
#define __NETNS_GMTP_H__

struct sock;

struct netns_gmtp {
	struct sock *v4_ctl_sk;
	struct sock *v6_ctl_sk;
};

#endif
