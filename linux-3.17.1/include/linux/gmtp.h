#ifndef LINUX_GMTP_H_
#define LINUX_GMTP_H_

#include <net/inet_sock.h>
#include <net/inet_connection_sock.h>
#include <net/tcp_states.h>
#include <linux/types.h>

#include <uapi/linux/gmtp.h>

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

enum {
	GMTPF_OPEN	      = TCPF_ESTABLISHED,
	GMTPF_REQUESTING      = TCPF_SYN_SENT,
	GMTPF_LISTEN	      = TCPF_LISTEN,
	GMTPF_RESPOND	      = TCPF_SYN_RECV,
	GMTPF_ACTIVE_CLOSEREQ = TCPF_FIN_WAIT1,
	GMTPF_CLOSING	      = TCPF_CLOSING,
	GMTPF_TIME_WAIT	      = TCPF_TIME_WAIT,
	GMTPF_CLOSED	      = TCPF_CLOSE
};


enum gmtp_role {
	GMTP_ROLE_UNDEFINED,
	GMTP_ROLE_LISTEN,
	GMTP_ROLE_CLIENT,
	GMTP_ROLE_SERVER,
};

/**
 * struct gmtp_request_sock  -  represent GMTP-specific connection request
 * @greq_inet_rsk: structure inherited from
 * @greq_service: service code present on the Request (there is just one)
 */
struct gmtp_request_sock {
	struct inet_request_sock greq_inet_rsk;

	__be32			 greq_service;
	//TODO Specific gmtp fields
};

static inline struct gmtp_request_sock *gmtp_rsk(const struct request_sock *req)
{
	return (struct gmtp_request_sock *)req;
}

/**
 * struct gmtp_sock - GMTP socket state
 *
 * @dccps_role - role of this sock, one of %gmtp_role
 * @gmtps_service - first (passive sock) or unique (active sock) service code
 * @gmtps_iss - initial sequence number sent
 * @gmtps_isr - initial sequence number received
 * @gmtps_gss - greatest sequence number sent
 * @gmtps_gsr - greatest valid sequence number received
 */
struct gmtp_sock {
	/* inet_connection_sock has to be the first member of gmtp_sock */
	struct inet_connection_sock gmtps_inet_connection;
#define gmtps_syn_rtt			gmtps_inet_connection.icsk_ack.lrcvtime

	enum gmtp_role		gmtps_role:2;
	__be32				gmtps_service;
	__u64				gmtps_iss;
	__u64				gmtps_isr;
	__u64				gmtps_gss;
	__u64				gmtps_gsr;

};

/**
 * struct dccp_sock - DCCP socket state
 *
 * @dccps_swl - sequence number window low
 * @dccps_swh - sequence number window high
 * @dccps_awl - acknowledgement number window low
 * @dccps_awh - acknowledgement number window high
 * @dccps_iss - initial sequence number sent
 * @dccps_isr - initial sequence number received
 * @dccps_osr - first OPEN sequence number received
 * @dccps_gss - greatest sequence number sent
 * @dccps_gsr - greatest valid sequence number received
 * @dccps_gar - greatest valid ack number received on a non-Sync; initialized to %dccps_iss
 * @dccps_service - first (passive sock) or unique (active sock) service code
 * @dccps_service_list - second .. last service code on passive socket
 * @dccps_timestamp_echo - latest timestamp received on a TIMESTAMP option
 * @dccps_timestamp_time - time of receiving latest @dccps_timestamp_echo
 * @dccps_l_ack_ratio - feature-local Ack Ratio
 * @dccps_r_ack_ratio - feature-remote Ack Ratio
 * @dccps_l_seq_win - local Sequence Window (influences ack number validity)
 * @dccps_r_seq_win - remote Sequence Window (influences seq number validity)
 * @dccps_pcslen - sender   partial checksum coverage (via sockopt)
 * @dccps_pcrlen - receiver partial checksum coverage (via sockopt)
 * @dccps_send_ndp_count - local Send NDP Count feature (7.7.2)
 * @dccps_ndp_count - number of Non Data Packets since last data packet
 * @dccps_mss_cache - current value of MSS (path MTU minus header sizes)
 * @dccps_rate_last - timestamp for rate-limiting DCCP-Sync (RFC 4340, 7.5.4)
 * @dccps_featneg - tracks feature-negotiation state (mostly during handshake)
 * @dccps_hc_rx_ackvec - rx half connection ack vector
 * @dccps_hc_rx_ccid - CCID used for the receiver (or receiving half-connection)
 * @dccps_hc_tx_ccid - CCID used for the sender (or sending half-connection)
 * @dccps_options_received - parsed set of retrieved options
 * @dccps_qpolicy - TX dequeueing policy, one of %dccp_packet_dequeueing_policy
 * @dccps_tx_qlen - maximum length of the TX queue
 * @dccps_role - role of this sock, one of %dccp_role
 * @dccps_hc_rx_insert_options - receiver wants to add options when acking
 * @dccps_hc_tx_insert_options - sender wants to add options when sending
 * @dccps_server_timewait - server holds timewait state on close (RFC 4340, 8.3)
 * @dccps_sync_scheduled - flag which signals "send out-of-band message soon"
 * @dccps_xmitlet - tasklet scheduled by the TX CCID to dequeue data packets
 * @dccps_xmit_timer - used by the TX CCID to delay sending (rate-based pacing)
 * @dccps_syn_rtt - RTT sample from Request/Response exchange (in usecs)
 */
/*
struct dccp_sock {
	* inet_connection_sock has to be the first member of dccp_sock *
	struct inet_connection_sock	dccps_inet_connection;
#define dccps_syn_rtt			dccps_inet_connection.icsk_ack.lrcvtime
	__u64				dccps_swl;
	__u64				dccps_swh;
	__u64				dccps_awl;
	__u64				dccps_awh;
	__u64				dccps_iss;
	__u64				dccps_isr;
	__u64				dccps_osr;
	__u64				dccps_gss;
	__u64				dccps_gsr;
	__u64				dccps_gar;
	__be32				dccps_service;
	__u32				dccps_mss_cache;
	struct dccp_service_list	*dccps_service_list;
	__u32				dccps_timestamp_echo;
	__u32				dccps_timestamp_time;
	__u16				dccps_l_ack_ratio;
	__u16				dccps_r_ack_ratio;
	__u64				dccps_l_seq_win:48;
	__u64				dccps_r_seq_win:48;
	__u8				dccps_pcslen:4;
	__u8				dccps_pcrlen:4;
	__u8				dccps_send_ndp_count:1;
	__u64				dccps_ndp_count:48;
	unsigned long			dccps_rate_last;
	struct list_head		dccps_featneg;
	struct dccp_ackvec		*dccps_hc_rx_ackvec;
	struct ccid			*dccps_hc_rx_ccid;
	struct ccid			*dccps_hc_tx_ccid;
	struct dccp_options_received	dccps_options_received;
	__u8				dccps_qpolicy;
	__u32				dccps_tx_qlen;
	enum dccp_role			dccps_role:2;
	__u8				dccps_hc_rx_insert_options:1;
	__u8				dccps_hc_tx_insert_options:1;
	__u8				dccps_server_timewait:1;
	__u8				dccps_sync_scheduled:1;
	struct tasklet_struct		dccps_xmitlet;
	struct timer_list		dccps_xmit_timer;
}
****/

static inline struct gmtp_sock *gmtp_sk(const struct sock *sk)
{
	return (struct gmtp_sock *)sk;
}

static inline struct gmtp_hdr *gmtp_hdr(const struct sk_buff *skb)
{
	return (struct gmtp_hdr *)skb_transport_header(skb);
}

static inline struct gmtp_hdr *gmtp_zeroed_hdr(struct sk_buff *skb, int headlen)
{
	skb_push(skb, headlen);
	skb_reset_transport_header(skb);
	return memset(skb_transport_header(skb), 0, headlen);
}
#endif /* LINUX_GMTP_H_ */
