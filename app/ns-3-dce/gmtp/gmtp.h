#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>

#ifndef GMTP_H_
#define GMTP_H_

#define SOCK_GMTP     7
#define IPPROTO_GMTP  254
#define SOL_GMTP      281

enum gmtp_sockopt_codes {
	GMTP_SOCKOPT_FLOWNAME = 1,
	GMTP_SOCKOPT_MAX_TX_RATE,
	GMTP_SOCKOPT_GET_CUR_MSS,
	GMTP_SOCKOPT_SERVER_RTT,
	GMTP_SOCKOPT_SERVER_TIMEWAIT,
	GMTP_SOCKOPT_PULL,
	GMTP_SOCKOPT_ROLE_RELAY,
	GMTP_SOCKOPT_RELAY_ENABLED
};

void set_gmtp_inter(int actived)
{
	int ok = 1;
	int rsock = socket(PF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(rsock, SOL_GMTP, GMTP_SOCKOPT_ROLE_RELAY, &ok, sizeof(int));
	setsockopt(rsock, SOL_GMTP, GMTP_SOCKOPT_RELAY_ENABLED, &actived,
			sizeof(int));
}

inline void disable_gmtp_inter()
{
	printf("Disabling gmtp_inter...\n");
	set_gmtp_inter(false);
}

inline void enable_gmtp_inter()
{
	printf("Enabling gmtp_inter...\n");
	set_gmtp_inter(true);
}

#endif /* GMTP_H_ */
