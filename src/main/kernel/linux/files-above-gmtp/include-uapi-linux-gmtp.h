#ifndef _UAPI_LINUX_GMTP_H
#define _UAPI_LINUX_GMTP_H

#include <linux/types.h>
#include <asm/byteorder.h>

/**
 * struct dccp_hdr - generic part of DCCP packet header
 *
 * @dccph_sport - Relevant port on the endpoint that sent this packet
 * @dccph_dport - Relevant port on the other endpoint
 * @dccph_doff - Data Offset from the start of the DCCP header, in 32-bit words
 * @dccph_ccval - Used by the HC-Sender CCID
 * @dccph_cscov - Parts of the packet that are covered by the Checksum field
 * @dccph_checksum - Internet checksum, depends on dccph_cscov
 * @dccph_x - 0 = 24 bit sequence number, 1 = 48
 * @dccph_type - packet type, see DCCP_PKT_ prefixed macros
 * @dccph_seq - sequence number high or low order 24 bits, depends on dccph_x
 */
struct gmtp_hdr {
	__be16	gmtph_sport,
		gmtph_dport;
	__u8	gmtph_doff;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8	gmtph_cscov:4,
		gmtph_ccval:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8	gmtph_ccval:4,
		gmtph_cscov:4;
#else
#error  "Adjust your <asm/byteorder.h> defines"
#endif
	__sum16	gmtph_checksum;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8	gmtph_x:1,
		gmtph_type:4,
		gmtph_reserved:3;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8	gmtph_reserved:3,
		gmtph_type:4,
		gmtph_x:1;
#else
#error  "Adjust your <asm/byteorder.h> defines"
#endif
	__u8	gmtph_seq2;
	__be16	gmtph_seq;
};

/**
 * struct dccp_hdr_ext - the low bits of a 48 bit seq packet
 *
 * @dccph_seq_low - low 24 bits of a 48 bit seq packet
 */
struct gmtp_hdr_ext {
	__be32	gmtph_seq_low;
};

/**
 * struct dccp_hdr_request - Connection initiation request header
 *
 * @dccph_req_service - Service to which the client app wants to connect
 */
struct gmtp_hdr_request {
	__be32	gmtph_req_service;
};
/**
 * struct dccp_hdr_ack_bits - acknowledgment bits common to most packets
 *
 * @dccph_resp_ack_nr_high - 48 bit ack number high order bits, contains GSR
 * @dccph_resp_ack_nr_low - 48 bit ack number low order bits, contains GSR
 */
struct gmtp_hdr_ack_bits {
	__be16	gmtph_reserved1;
	__be16	gmtph_ack_nr_high;
	__be32	gmtph_ack_nr_low;
};
/**
 * struct dccp_hdr_response - Connection initiation response header
 *
 * @dccph_resp_ack - 48 bit Acknowledgment Number Subheader (5.3)
 * @dccph_resp_service - Echoes the Service Code on a received DCCP-Request
 */
struct gmtp_hdr_response {
	struct gmtp_hdr_ack_bits	gmtph_resp_ack;
	__be32				gmtph_resp_service;
};

/**
 * struct dccp_hdr_reset - Unconditionally shut down a connection
 *
 * @dccph_reset_ack - 48 bit Acknowledgment Number Subheader (5.6)
 * @dccph_reset_code - one of %dccp_reset_codes
 * @dccph_reset_data - the Data 1 ... Data 3 fields from 5.6
 */
struct gmtp_hdr_reset {
	struct gmtp_hdr_ack_bits	gmtph_reset_ack;
	__u8				gmtph_reset_code,
					gmtph_reset_data[3];
};

enum gmtp_pkt_type {
	GMTP_PKT_REQUEST = 0,
	GMTP_PKT_RESPONSE,
	GMTP_PKT_DATA,
	GMTP_PKT_ACK,
	GMTP_PKT_DATAACK,
	GMTP_PKT_CLOSEREQ,
	GMTP_PKT_CLOSE,
	GMTP_PKT_RESET,
	GMTP_PKT_SYNC,
	GMTP_PKT_SYNCACK,
	GMTP_PKT_INVALID,
};

#define GMTP_NR_PKT_TYPES GMTP_PKT_INVALID

static inline unsigned int gmtp_packet_hdr_len(const __u8 type)
{
	if (type == GMTP_PKT_DATA)
		return 0;
	if (type == GMTP_PKT_DATAACK	||
	    type == GMTP_PKT_ACK	||
	    type == GMTP_PKT_SYNC	||
	    type == GMTP_PKT_SYNCACK	||
	    type == GMTP_PKT_CLOSE	||
	    type == GMTP_PKT_CLOSEREQ)
		return sizeof(struct gmtp_hdr_ack_bits);
	if (type == GMTP_PKT_REQUEST)
		return sizeof(struct gmtp_hdr_request);
	if (type == GMTP_PKT_RESPONSE)
		return sizeof(struct gmtp_hdr_response);
	return sizeof(struct gmtp_hdr_reset);
}
enum gmtp_reset_codes {
	GMTP_RESET_CODE_UNSPECIFIED = 0,
	GMTP_RESET_CODE_CLOSED,
	GMTP_RESET_CODE_ABORTED,
	GMTP_RESET_CODE_NO_CONNECTION,
	GMTP_RESET_CODE_PACKET_ERROR,
	GMTP_RESET_CODE_OPTION_ERROR,
	GMTP_RESET_CODE_MANDATORY_ERROR,
	GMTP_RESET_CODE_CONNECTION_REFUSED,
	GMTP_RESET_CODE_BAD_SERVICE_CODE,
	GMTP_RESET_CODE_TOO_BUSY,
	GMTP_RESET_CODE_BAD_INIT_COOKIE,
	GMTP_RESET_CODE_AGGRESSION_PENALTY,

	GMTP_MAX_RESET_CODES		/* Leave at the end!  */
};

/* DCCP options */
enum {
	GMTPO_PADDING = 0,
	GMTPO_MANDATORY = 1,
	GMTPO_MIN_RESERVED = 3,
	GMTPO_MAX_RESERVED = 31,
	GMTPO_CHANGE_L = 32,
	GMTPO_CONFIRM_L = 33,
	GMTPO_CHANGE_R = 34,
	GMTPO_CONFIRM_R = 35,
	GMTPO_NDP_COUNT = 37,
	GMTPO_ACK_VECTOR_0 = 38,
	GMTPO_ACK_VECTOR_1 = 39,
	GMTPO_TIMESTAMP = 41,
	GMTPO_TIMESTAMP_ECHO = 42,
	GMTPO_ELAPSED_TIME = 43,
	GMTPO_MAX = 45,
	GMTPO_MIN_RX_CCID_SPECIFIC = 128,	/* from sender to receiver */
	GMTPO_MAX_RX_CCID_SPECIFIC = 191,
	GMTPO_MIN_TX_CCID_SPECIFIC = 192,	/* from receiver to sender */
	GMTPO_MAX_TX_CCID_SPECIFIC = 255,
};
/* maximum size of a single TLV-encoded DCCP option (sans type/len bytes) */
#define GMTP_SINGLE_OPT_MAXLEN	253

/* DCCP CCIDS */
enum {
	GMTPC_CCID2 = 2,
	GMTPC_CCID3 = 3,
};

/* DCCP features (RFC 4340 section 6.4) */
enum gmtp_feature_numbers {
	GMTPF_RESERVED = 0,
	GMTPF_CCID = 1,
	GMTPF_SHORT_SEQNOS = 2,
	GMTPF_SEQUENCE_WINDOW = 3,
	GMTPF_ECN_INCAPABLE = 4,
	GMTPF_ACK_RATIO = 5,
	GMTPF_SEND_ACK_VECTOR = 6,
	GMTPF_SEND_NDP_COUNT = 7,
	GMTPF_MIN_CSUM_COVER = 8,
	GMTPF_DATA_CHECKSUM = 9,
	/* 10-127 reserved */
	GMTPF_MIN_CCID_SPECIFIC = 128,
	GMTPF_SEND_LEV_RATE = 192,	/* RFC 4342, sec. 8.4 */
	GMTPF_MAX_CCID_SPECIFIC = 255,
};

/* DCCP socket control message types for cmsg */
enum gmtp_cmsg_type {
	GMTP_SCM_PRIORITY = 1,
	GMTP_SCM_QPOLICY_MAX = 0xFFFF,
	/* ^-- Up to here reserved exclusively for qpolicy parameters */
	GMTP_SCM_MAX
};

/* DCCP priorities for outgoing/queued packets */
enum gmtp_packet_dequeueing_policy {
	GMTPQ_POLICY_SIMPLE,
	GMTPQ_POLICY_PRIO,
	GMTPQ_POLICY_MAX
};

/* DCCP socket options */
#define GMTP_SOCKOPT_PACKET_SIZE	1 /* XXX deprecated, without effect */
#define GMTP_SOCKOPT_SERVICE		2
#define GMTP_SOCKOPT_CHANGE_L		3
#define GMTP_SOCKOPT_CHANGE_R		4
#define GMTP_SOCKOPT_GET_CUR_MPS	5
#define GMTP_SOCKOPT_SERVER_TIMEWAIT	6
#define GMTP_SOCKOPT_SEND_CSCOV		10
#define GMTP_SOCKOPT_RECV_CSCOV		11
#define GMTP_SOCKOPT_AVAILABLE_CCIDS	12
#define GMTP_SOCKOPT_CCID		13
#define GMTP_SOCKOPT_TX_CCID		14
#define GMTP_SOCKOPT_RX_CCID		15
#define GMTP_SOCKOPT_QPOLICY_ID		16
#define GMTP_SOCKOPT_QPOLICY_TXQLEN	17
#define GMTP_SOCKOPT_CCID_RX_INFO	128
#define GMTP_SOCKOPT_CCID_TX_INFO	192

/* maximum number of services provided on the same listening port */
#define GMTP_SERVICE_LIST_MAX_LEN      32


#endif /* _UAPI_LINUX_GMTP_H */
