/*
 * gmtp.h
 *
 *  Created on: 18/06/2014
 *      Author: Wendell Silva Soares <wendell@compelab.org>
 */

#ifndef GMTP_H_
#define GMTP_H_

#include <net/inet_sock.h>

enum {
	IPPROTO_GMTP = 100
};


/**
 * struct dccp_request_sock  -  represent GMTP-specific connection request
 */
struct gmtp_request_sock {
	struct inet_request_sock dreq_inet_rsk;
	//TODO Specific gmtp fields
};

/**
 * struct dccp_sock - DCCP socket state
 */
struct gmtp_sock {
	/* inet_connection_sock has to be the first member of gmtp_sock */
	struct inet_connection_sock dccps_inet_connection;
};

#endif /* GMTP_H_ */
