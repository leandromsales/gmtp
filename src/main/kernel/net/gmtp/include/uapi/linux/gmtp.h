#ifndef UAPI_LINUX_GMTP_H
#define UAPI_LINUX_GMTP_H

#include <linux/types.h>

/**
 * GMTP packet header
 *
 * -------------------------------------------- 32 bits -------------------------------------------
 * [0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1]
 * [Version][Packet type ][          Header lenght         ][       Server RTT     ][P][R][Reserv.]
 * [                   Source port                   ][                 Destiny port              ]
 * [                                           Sequence number                                    ]
 * [                                          Transmission rate                                   ]
 * [                               Data flow name 128 bits (part i), i -> [1..4]                  ]
 * [                    { Variable part, depends of the 'Packet type' field...  }                 ]
 * [                                                (...)                                         ]
 *
 * struct gmtp_hdr - generic part of GMTP packet header
 *
 * @version: protocol version
 * @type: packet type
 * @hdrlen: header lenght
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
	__be16 src_port;
	__be16 dest_port;
	__be32 seq;
	__be32 transm_r;
	__be32 flowname[4]; //128
};

/**
 * struct gmtp_var_hdr - variable part of GMTP packet header
 * @hdata_var: Variable part, depends of the 'Packet type' field
*/
struct gmtp_var_hdr {
	__be32 *hdata_var;
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
};

static inline unsigned int gmtp_packet_hdr_len(const __u8 type)
{
	//TODO Implementar
//	if (type == GMTP_PKT_DATA)
//		return ?;
//	els if...
	return 0;
}



#endif /* UAPI_LINUX_GMTP_H */
