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
	GMTP_REQUEST_RECV    = TCP_SYN_RECV,
	GMTP_ACTIVE_CLOSEREQ = TCP_FIN_WAIT1,
	GMTP_PASSIVE_CLOSE   = TCP_CLOSE_WAIT, /* any node receiving a Close */
	GMTP_CLOSING	     = TCP_CLOSING,
	GMTP_TIME_WAIT	     = TCP_TIME_WAIT,
	GMTP_CLOSED	     = TCP_CLOSE,
	GMTP_MAX_STATES
};

enum {
	GMTPF_OPEN	      = TCPF_ESTABLISHED,
	GMTPF_REQUESTING      = TCPF_SYN_SENT,
	GMTPF_LISTEN	      = TCPF_LISTEN,
	GMTPF_REQUEST_RECV	      = TCPF_SYN_RECV, //relay
	GMTPF_ACTIVE_CLOSEREQ = TCPF_FIN_WAIT1,
	GMTPF_PASSIVE_CLOSE   = TCPF_CLOSE_WAIT,
	GMTPF_CLOSING	      = TCPF_CLOSING,
	GMTPF_TIME_WAIT	      = TCPF_TIME_WAIT,
	GMTPF_CLOSED	      = TCPF_CLOSE
};


enum gmtp_role {
	GMTP_ROLE_UNDEFINED,
	GMTP_ROLE_LISTEN,
	GMTP_ROLE_CLIENT,
	GMTP_ROLE_SERVER,
	GMTP_ROLE_RELAY
};

/*
 * Number of loss intervals (RFC 4342, 8.6.1). The history size is one more than
 * NINTERVAL, since the `open' interval I_0 is always stored as the first entry.
 */
#define NINTERVAL	8
#define LIH_SIZE	(NINTERVAL + 1)

/* Number of packets to wait after a missing packet (RFC 4342, 6.1) */
#define MCC_NDUPACK 3

/* GMTP-MCC receiver states */
enum mcc_rx_states {
	MCC_RSTATE_NO_DATA = 1,
	MCC_RSTATE_DATA,
};

/**
 * mcc_rx_hist  -  RX history structure for TFRC-based protocols
 * @ring:		Packet history for RTT sampling and loss detection
 * @loss_count:		Number of entries in circular history
 * @loss_start:		Movable index (for loss detection)
 * @rtt_sample_prev:	Used during RTT sampling, points to candidate entry
 */
struct mcc_rx_hist {
	struct mcc_rx_hist_entry *ring[MCC_NDUPACK + 1];
	u8			  loss_count:2,
				  loss_start:2;
#define rtt_sample_prev		  loss_start
};

/**
 *  tfrc_loss_hist  -  Loss record database
 *  @ring:	Circular queue managed in LIFO manner
 *  @counter:	Current count of entries (can be more than %LIH_SIZE)
 *  @i_mean:	Current Average Loss Interval [RFC 3448, 5.4]
 */
struct mcc_loss_hist {
	struct mcc_loss_interval	*ring[LIH_SIZE];
	u8				counter;
	u32				i_mean;
};

/**
 * struct gmtp_request_sock  -  represent GMTP-specific connection request
 *
 * @greq_inet_rsk: structure inherited from
 *
 * @iss: initial sequence number, sent on the first REQUEST (or register)
 * @gss: greatest sequence number sent (for retransmitted REPLYS)
 * @isr: initial sequence number received in the first REQUEST
 * @gsr: greatest sequence number received (for retransmitted REQUEST(s))
 *
 **/
struct gmtp_request_sock {
	struct inet_request_sock greq_inet_rsk;

	__u64			 iss;
	__u64			 gss;
	__u64			 isr;
	__u64			 gsr;
	
	__u8 flowname[GMTP_FLOWNAME_LEN];
};

static inline struct gmtp_request_sock *gmtp_rsk(const struct request_sock *req)
{
	return (struct gmtp_request_sock *)req;
}

/**
 * struct gmtp_sock - GMTP socket state
 *
 * @iss: initial sequence number sent
 * @isr: initial sequence number received
 * @gss: greatest sequence number sent
 * @gsr: greatest valid sequence number received
 * @mss: current value of MSS (path MTU minus header sizes)
 * @role: role of this sock, one of %gmtp_role
 * @req_stamp: time stamp of request sent (jiffies)
 * @reply_stamp: time stamp of Request-Reply sent (jiffies)
 * @rtt: RTT from client to server (milisseconds)
 * @relay_rtt: RTT from client to origin relay  (milisseconds)
 * @server_timewait: server holds timewait state on close
 * @xmitlet: tasklet scheduled by the TX CCID to dequeue data packets
 * @xmit_timer: used to control the TX (rate-based pacing)
 * @rx_last_counter:	     Tracks window counter (RFC 4342, 8.1)
 * @rx_state:		     Receiver state, one of %mcc_rx_states
 * @rx_bytes_recv:	     Total sum of GMTP payload bytes
 * @rx_x_recv:		     Receiver estimate of send rate (RFC 3448, sec. 4.3)
 * @rx_rtt:		     Receiver estimate of RTT (average)
 * @rx_tstamp_last_feedback: Time at which last feedback was sent
 * @rx_hist:		     Packet history (loss detection + RTT sampling)
 * @rx_li_hist:		     Loss Interval database
 * @rx_s:		     Received packet size in bytes
 * @rx_pinv:		     Inverse of Loss Event Rate (RFC 4342, sec. 8.5)
 * @pkt_sent: Number of packets (type == data) sent by socket
 * @data_sent: Number of bytes (only app data) sent by socket
 * @bytes_sent: Number of bytes (app data + headers) sent by socket
 * @tx_sample_len: Size of the sample window (to measure 'instant' Tx Rate)
 * @tx_time_sample: Elapsed time at sample window (jiffies)
 * @tx_byte_sample: Bytes sent at sample window
 * @tx_first_stamp: time stamp of first data packet sent (jiffies)
 * @tx_last_stamp: time stamp of last data packet sent (jiffies)
 * @tx_max_rate: Maximum transmission rate (bytes/s). 0 means no limits.
 * tx_byte_budget: the amount of bytes that can be sent immediately.
 * tx_adj_budget: memory of last adjustment in tx rate.
 * @flowname: name of the dataflow
 *
 */
struct gmtp_sock {
	/* inet_connection_sock has to be the first member of gmtp_sock */
	struct inet_connection_sock gmtps_inet_connection;
#define gmtps_syn_rtt 	gmtps_inet_connection.icsk_ack.lrcvtime

	__u32				iss;
	__u32				isr;
	__u32				gss;
	__u32				gsr;
	
	__u32				mss;

	enum gmtp_role			role:3;

	unsigned long			req_stamp; /* jiffies */
	unsigned long			reply_stamp; /* jiffies */
	__u8 				rtt;       /* milisseconds */
	__u8				relay_rtt;  /* milisseconds */

	__u8				server_timewait:1;
	struct tasklet_struct		xmitlet;
	struct timer_list		xmit_timer;

	/** Rx variables */

	u8				rx_last_counter:4;
	enum mcc_rx_states		rx_state:8;
	u32				rx_bytes_recv;
	u32				rx_x_recv;
	u32				rx_rtt;
	ktime_t				rx_tstamp_last_feedback;
	struct mcc_rx_hist		rx_hist;
	struct mcc_loss_hist		rx_li_hist;
	u16				rx_s;
#define rx_pinv				rx_li_hist.i_mean

	/** Tx variables */

	unsigned int 			tx_pkts_sent;
	unsigned long			tx_data_sent;
	unsigned long			tx_bytes_sent;

	unsigned int 			tx_sample_len;
	unsigned long 			tx_time_sample;
	unsigned long 			tx_byte_sample;

	unsigned long			tx_sample_rate;
	unsigned long			tx_total_rate;

	unsigned long			tx_first_stamp;  /* jiffies */
	unsigned long 			tx_last_stamp;	/* jiffies */
	unsigned long			tx_max_rate;
	long 				tx_byte_budget;
	int				tx_adj_budget;

	__u8 flowname[GMTP_FLOWNAME_LEN];
};

static inline struct gmtp_sock *gmtp_sk(const struct sock *sk)
{
	return (struct gmtp_sock *)sk;
}

static inline const char *gmtp_role_name(const struct sock *sk)
{
	switch (gmtp_sk(sk)->role) {
	case GMTP_ROLE_UNDEFINED: return "undefined";
	case GMTP_ROLE_LISTEN:	  return "listen";
	case GMTP_ROLE_SERVER:	  return "server";
	case GMTP_ROLE_CLIENT:	  return "client";
	case GMTP_ROLE_RELAY:	  return "relay";
	}
	return NULL;
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

static inline struct gmtp_hdr_ack *gmtp_hdr_ack(const struct sk_buff *skb)
{
	return (struct gmtp_hdr_ack *)(skb_transport_header(skb) +
						 sizeof(struct gmtp_hdr));
}

static inline struct gmtp_hdr_register_reply *gmtp_hdr_register_reply(
		const struct sk_buff *skb)
{
	return (struct gmtp_hdr_register_reply *)(skb_transport_header(skb) +
						 sizeof(struct gmtp_hdr));
}

static inline struct gmtp_hdr_route *gmtp_hdr_route(const struct sk_buff *skb)
{
	return (struct gmtp_hdr_route *)(skb_transport_header(skb) +
						 sizeof(struct gmtp_hdr));
}

static inline struct gmtp_hdr_reqnotify *gmtp_hdr_reqnotify(
		const struct sk_buff *skb)
{
	return (struct gmtp_hdr_reqnotify *)(skb_transport_header(skb) +
						 sizeof(struct gmtp_hdr));
}

static inline struct gmtp_hdr_reset *gmtp_hdr_reset(const struct sk_buff *skb)
{
	return (struct gmtp_hdr_reset *)(skb_transport_header(skb) +
					 sizeof(struct gmtp_hdr));
}

static inline unsigned int __gmtp_hdr_len(const struct gmtp_hdr *gh)
{
	return sizeof(struct gmtp_hdr) + gmtp_packet_hdr_variable_len(gh->type);
}

static inline unsigned int gmtp_hdr_len(const struct sk_buff *skb)
{
	return __gmtp_hdr_len(gmtp_hdr(skb));
}

#endif /* LINUX_GMTP_H_ */


