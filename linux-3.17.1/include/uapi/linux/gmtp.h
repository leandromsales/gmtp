#ifndef UAPI_LINUX_GMTP_H
#define UAPI_LINUX_GMTP_H

#include <linux/types.h>

#define GMTP_VERSION 1
#define GMTP_FLOWNAME_LEN 16 /* bytes */
#define GMTP_FLOWNAME_STR_LEN ((GMTP_FLOWNAME_LEN * 2) + 1)

#define GMTP_RELAY_ID_LEN 16 /* bytes */

/**
 * MSS: 1444
 *
 * GMTP fixed header: 36
 * GMTP Ack Header: 8
 * ----------------------
 *
 * GMTP Register-Reply header:
 * 1444 - 36 - 8 = 1400
 * ---------------------
 * Register-Reply->nrelays: 8
 * Register-Reply->relay_list 1392
 *
 * 1392/20 = 69, rest: 12
 *
 */
#define GMTP_MAX_RELAY_NUM 69

/**
 * GMTP packet header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  | Ver |   Type  |     Header Lenght   |   Server RTT  |P|R| Res |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |          Source Port          |           Dest Port           |
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
 *  |             Max Lenght = (2^11-1) => 2047 bytes               |
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
 * @relay:  'R' (Relay) field
 * @res: reserved to future
 * @src_port: source port
 * @dest_port: destiny port
 * @seq: sequence number
 * @transm_r: transmission rate
 * @flowname: data flow name
 * @varpart: Variable part, depends of the 'Packet type' field
 */
struct gmtp_hdr {
	__u8 version:3;
	__u8 type:5;
	__be16 hdrlen:11;
	__u8 server_rtt;
	__u8 pull:1;
	__u8 relay:1;
	__u8 res:3;
	__be16 sport; //source port
	__be16 dport; //dest port
	__be32 seq;
	__be32 transm_r;
	__u8 flowname[GMTP_FLOWNAME_LEN]; //128 bits
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
 * struct gmtp_hdr_register_reply - Register reply from servers
 *
 * @nrelays: number of relays
 * @relay_id: unique id of relay
 * @relay_ip: IP address of relay
 */
struct gmtp_relay {
	__u8 relay_id[GMTP_RELAY_ID_LEN];
	__be32 relay_ip;
};

/**
 * struct gmtp_hdr_register_reply - Register reply from servers
 *
 * @nrelays: 	number of relays
 * @relay_list:	list of relays in path
 */
struct gmtp_hdr_register_reply {
	__u8 nrelays;
	struct gmtp_relay relay_list[GMTP_MAX_RELAY_NUM];
};

/**
 * struct gmtp_hdr_route_notify - RouteNotify to servers
 *
 * @nrelays: number of relays
 * @relay_list - list of relays in path
 */
struct gmtp_hdr_route {
	__u8 nrelays;
	struct gmtp_relay relay_list[GMTP_MAX_RELAY_NUM];
};

/**
 * struct gmtp_hdr_ack - Ack packet from clients to relays
 *
 * @ackcode: type of acked packet
 */
struct gmtp_hdr_ack {
	__u8 ackcode:5;
};

/**
 * struct gmtp_hdr_reqnotify - RequestNotify to clients
 *
 * @relay_list - list of relays in path
 * @channel_addr - multicast channel address
 * @chport - multicast channel port
 */
struct gmtp_hdr_reqnotify {
	__u8	error_code;
	__be32	mcst_addr;
	__be16  mcst_port; 
};

/**
 *
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
	//TODO Implementar
	switch(type)
	{
	case GMTP_PKT_DATA:
		break;
	case GMTP_PKT_ACK:
	case GMTP_PKT_DATAACK:
		len = sizeof(struct gmtp_hdr_ack);
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
	case GMTP_PKT_RESET:
		len = sizeof(struct gmtp_hdr_reset);
		break;
	default:
		break;
	}
	
	return len;
}

/* GMTP priorities for outgoing/queued packets */
enum gmtp_packet_dequeueing_policy {
	GMTPQ_POLICY_SIMPLE,
	GMTPQ_POLICY_MAX
};

/* GMTP socket options */
enum gmtp_sockopt_codes {
	GMTP_SOCKOPT_SERVER_RTT = 1,
	GMTP_SOCKOPT_PULL,
	GMTP_SOCKOPT_RELAY,
	GMTP_SOCKOPT_MAX_TX_RATE,
	GMTP_SOCKOPT_FLOWNAME,
	GMTP_SOCKOPT_GET_CUR_MSS,
	GMTP_SOCKOPT_SERVER_TIMEWAIT
};



#endif /* UAPI_LINUX_GMTP_H */
