#ifndef UAPI_LINUX_GMTP_H
#define UAPI_LINUX_GMTP_H

#include <linux/types.h>

#define GMTP_VERSION (1)
#define GMTP_FLOWNAME_LEN (16) /* bytes */
#define GMTP_FLOWNAME_STR_LEN ((GMTP_FLOWNAME_LEN * 2) + 1)

#define GMTP_RELAY_ID_LEN (16) /* bytes */

#define GMTP_REPORTER_DEFAULT_PROPORTION (6)


/**
 * MSS: 1444
 *
 * GMTP fixed header: 36
 * GMTP Ack Header: 0
 * ----------------------
 *
 * GMTP Register-Reply header:
 * 1444 - 36 = 1408
 * ---------------------
 * Register-Reply->nrelays: 8
 * Register-Reply->relay_list 1400
 *
 * 1400/20 = 70
 *
 */
#define GMTP_MAX_RELAY_NUM 70

/**
 * GMTP packet header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  | V.|   Type  |     Header Length   |       Server RTT      |P|R|
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |          Source Port          |       Destination Port        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                         Sequence Number                       |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                        Transmission Rate                      |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |            Data Flow Name - part 1 of 4 (128 bits)            .
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  .                 Data Flow Name - part 2 of 4                  .
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  .                 Data Flow Name - part 3 of 4                  .
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  .                 Data Flow Name - part 4 of 4                  |
 *  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *  |                                                               |
 *  |              Variable part (depends of 'type')                |
 *  |             Max Length = (2^11-1) => 2047 bytes               |
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * struct gmtp_hdr - generic part of GMTP packet header
 *
 * @version: protocol version
 * @type: packet type
 * @hdrlen: header length (bytes)
 * @server_rtt: server RTT
 * @pull:  'P' (Pull) field
 * @relay:  'R' (Relay) field. It is activated when a relay send packets
 * @res: reserved to future <- [DEPRECATED]
 * @sport: source port
 * @dport: destiny port
 * @seq: sequence number
 * @transm_r: transmission rate
 * @flowname: data flow name
 */
struct gmtp_hdr {
	__u8 	version:2;
	__u8 	type:5;
	__be16 	hdrlen:11;
	__be16 	server_rtt:12;
	__u8 	pull:1;
	__u8 	relay:1;
	__be16 	sport; //source port
	__be16 	dport; //dest port
	__be32 	seq;
	__be32 	transm_r;
	__u8 	flowname[GMTP_FLOWNAME_LEN]; //128 bits
};

/**
 * struct gmtp_hdr_data - Data packets
 * @tstamp: time stamp of sent data packet
 */
struct gmtp_hdr_data {
	__be32 tstamp;
};

/**
 * struct gmtp_hdr_ack - ACK packets
 * @orig_tstamp: time stamp of original received packet
 * @wait: time wait from reception of packet to ack send
 *
 * The receiver will calculate RTT: now - (orig + wait)
 */
struct gmtp_hdr_ack {
	__be32 orig_tstamp;
};

/**
 * struct gmtp_hdr_dataack - DATAACK packets
 */
struct gmtp_hdr_dataack {
	struct gmtp_hdr_data	data_hdr;
	struct gmtp_hdr_ack	ack_hdr;
};

/**
 * struct gmtp_hdr_feedback - Data packets
 * @tstamp: time stamp of feedback packet
 * @nclients: active clients at reporter
 *
 */
struct gmtp_hdr_feedback {
	__be32 	orig_tstamp;
	__u8	nclients;
};

/**
 * struct gmtp_hdr_reset - Unconditionally shut down a connection
 *
 * @reset_code - one of %gmtp_reset_codes
 * @reset_data - the Data of reset
 */
struct gmtp_hdr_reset {
	__u8	reset_code,
		reset_data[3];
};

/**
 * struct gmtp_hdr_relay- Store data of a single relay in GMTP headers
 *
 * @relay_id: unique id of relay
 * @relay_ip: IP address of relay
 */
struct gmtp_hdr_relay {
	__u8 			relay_id[GMTP_RELAY_ID_LEN];
	__be32 			relay_ip;
};

/**
 * struct gmtp_hdr_register_reply - Register reply from servers
 *
 * @nrelays: 	number of relays
 * @relay_list:	list of relays in path
 */
struct gmtp_hdr_register_reply {
	__u8			ucc_type:2;
	__u8 			nrelays;
	struct gmtp_hdr_relay 	relay_list[GMTP_MAX_RELAY_NUM];
};

/**
 * struct gmtp_hdr_route_notify - RouteNotify to servers
 *
 * @nrelays: number of relays
 * @relay_list - list of relays in path
 */
struct gmtp_hdr_route {
	__u8 			nrelays;
	struct gmtp_hdr_relay 	relay_list[GMTP_MAX_RELAY_NUM];
};

/**
 * struct gmtp_hdr_reqnotify - RequestNotify to clients
 *
 * @error_code: One of gmtp_reqnotify_error_code
 * @channel_addr: multicast channel address
 * @mcst_port: multicast channel port
 */
struct gmtp_hdr_reqnotify {
	__u8			rn_code:3;
	__be32			mcst_addr;
	__be16  		mcst_port;
	__u8 			relay_id[GMTP_RELAY_ID_LEN];
	__be32			reporter_addr;
	__be16  		reporter_port;
	__u8 			max_nclients;
};

enum gmtp_reqnotify_error_code {
	GMTP_REQNOTIFY_CODE_OK = 0,
	GMTP_REQNOTIFY_CODE_WAIT,
	GMTP_REQNOTIFY_CODE_ERROR,
	GMTP_REQNOTIFY_MAX_CODES
};

/**
 * struct gmtp_hdr_elect_request - Elect request to reporters
 *
 * @relay_id: relay from where request come from
 * @max_nclients: Max number of clients of a reporter
 */
struct gmtp_hdr_elect_request {
	__u8 			relay_id[GMTP_RELAY_ID_LEN];
	__u8 			max_nclients;
};

/**
 * struct gmtp_hdr_elect_request - Elect request to reporters
 *
 * @elect_code: One of %gmtp_ack_codes
 */
struct gmtp_hdr_elect_response {
	__u8 	elect_code:2;
};

enum gmtp_elect_codes {
	GMTP_ELECT_ACCEPT = 0,
	GMTP_ELECT_REJECT,
	GMTP_ELECT_AUTO,
	GMTP_ELECT_MAX_CODES
};

/**
 * see www.gmtp-protocol.org
 */
enum gmtp_pkt_type {
	GMTP_PKT_REQUEST = 0,
	GMTP_PKT_REQUESTNOTIFY,
	GMTP_PKT_RESPONSE,
	GMTP_PKT_REGISTER,
	GMTP_PKT_REGISTER_REPLY,
	GMTP_PKT_ROUTE_NOTIFY,
	GMTP_PKT_RELAYQUERY,
	GMTP_PKT_RELAYQUERY_REPLY,
	GMTP_PKT_DATA,
	GMTP_PKT_ACK,
	GMTP_PKT_DATAACK,
	GMTP_PKT_MEDIADESC,
	GMTP_PKT_DATAPULL_REQUEST,
	GMTP_PKT_DATAPULL_RESPONSE,
	GMTP_PKT_ELECT_REQUEST,
	GMTP_PKT_ELECT_RESPONSE,
	GMTP_PKT_CLOSE,
	GMTP_PKT_RESET,

	GMTP_PKT_FEEDBACK,

	GMTP_PKT_INVALID,
	GMTP_PKT_MAX_STATES
};

#define GMTP_NR_PKT_TYPES GMTP_PKT_INVALID

enum gmtp_reset_codes {
	GMTP_RESET_CODE_UNSPECIFIED = 0,
	GMTP_RESET_CODE_CLOSED,
	GMTP_RESET_CODE_ABORTED,
	GMTP_RESET_CODE_NO_CONNECTION,
	GMTP_RESET_CODE_PACKET_ERROR,
	GMTP_RESET_CODE_MANDATORY_ERROR,
	GMTP_RESET_CODE_CONNECTION_REFUSED,
	GMTP_RESET_CODE_BAD_FLOWNAME,
	GMTP_RESET_CODE_TOO_BUSY,
	GMTP_RESET_CODE_AGGRESSION_PENALTY,

	GMTP_MAX_RESET_CODES		/* Leave at the end!  */
};

static inline unsigned int gmtp_packet_hdr_variable_len(const __u8 type)
{
	int len = 0;
	switch(type)
	{
	case GMTP_PKT_DATA:
		len = sizeof(struct gmtp_hdr_data);
		break;
	case GMTP_PKT_DATAACK:
		len = sizeof(struct gmtp_hdr_dataack);
		break;
	case GMTP_PKT_ACK:
		len = sizeof(struct gmtp_hdr_ack);
		break;
	case GMTP_PKT_FEEDBACK:
		len = sizeof(struct gmtp_hdr_feedback);
		break;
	case GMTP_PKT_REGISTER_REPLY:
		len = sizeof(struct gmtp_hdr_register_reply);
		break;
	case GMTP_PKT_ROUTE_NOTIFY:
		len = sizeof(struct gmtp_hdr_route);
		break;
	case GMTP_PKT_REQUESTNOTIFY:
		len = sizeof(struct gmtp_hdr_reqnotify);
		break;
	case GMTP_PKT_ELECT_REQUEST:
		len = sizeof(struct gmtp_hdr_elect_request);
		break;
	case GMTP_PKT_ELECT_RESPONSE:
		len = sizeof(struct gmtp_hdr_elect_response);
		break;
	case GMTP_PKT_RESET:
		len = sizeof(struct gmtp_hdr_reset);
		break;
	}
	
	return len;
}

enum gmtp_ucc_type {
	GMTP_DELAY_UCC = 1,
	GMTP_MEDIA_ADAPT_UCC,

	GMTP_MAX_UCC_TYPES
};

/* GMTP socket options */
enum gmtp_sockopt_codes {
	GMTP_SOCKOPT_FLOWNAME = 1,
	GMTP_SOCKOPT_MEDIA_RATE,
	GMTP_SOCKOPT_MAX_TX_RATE,
	GMTP_SOCKOPT_UCC_TX_RATE,
	GMTP_SOCKOPT_GET_CUR_MSS,
	GMTP_SOCKOPT_SERVER_RTT,
	GMTP_SOCKOPT_SERVER_TIMEWAIT,
	GMTP_SOCKOPT_PULL,
	GMTP_SOCKOPT_ROLE_RELAY,
	GMTP_SOCKOPT_RELAY_ENABLED,
	GMTP_SOCKOPT_UCC_TYPE
};


#endif /* UAPI_LINUX_GMTP_H */
