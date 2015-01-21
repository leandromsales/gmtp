#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/gmtp.h>
#include <linux/types.h>

#include <net/inet_sock.h>
#include <net/sock.h>

#include <uapi/linux/gmtp.h>
#include "gmtp.h"

static int gmtp_check_seqno(struct sock *sk, struct sk_buff *skb)
{
	gmtp_print_debug("gmtp_check_seqno");
	return 0;
}

/**
 * gmtp_parse_options  -  Parse GMTP options present in @skb
 * @sk: client|server|listening gmtp socket (when @dreq != NULL)
 * @dreq: request socket to use during connection setup, or NULL
 *
 * TODO Verificar se a fc eh necessaria
 */
int gmtp_parse_options(struct sock *sk, struct gmtp_request_sock *dreq,
		       struct sk_buff *skb)
{
	gmtp_print_debug("gmtp_parse_options");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_parse_options);

static int __gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
				  const struct gmtp_hdr *gh, const unsigned int len)
{
	//struct gmtp_sock *dp = gmtp_sk(sk);

	gmtp_print_debug("__gmtp_rcv_established");

	switch (gmtp_hdr(skb)->type) {
	case GMTP_PKT_DATAACK:
	case GMTP_PKT_DATA:
		return 0;
	case GMTP_PKT_ACK:
		goto discard;
	}

	return 0;
discard:
	__kfree_skb(skb);
	return 0;
}

int gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
			 const struct gmtp_hdr *gh, const unsigned int len)
{
	gmtp_print_debug("gmtp_rcv_established");

	//Check sequence numbers...
	if (gmtp_check_seqno(sk, skb))
		goto discard;

	if (gmtp_parse_options(sk, NULL, skb))
		return 1;

	//TODO verificar acks..s
//	dccp_handle_ackvec_processing(sk, skb);

	return __gmtp_rcv_established(sk, skb, gh, len);
discard:
	__kfree_skb(skb);
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_rcv_established);

int gmtp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
			   struct gmtp_hdr *gh, unsigned int len)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct gmtp_skb_cb *dcb = GMTP_SKB_CB(skb);
	const int old_state = sk->sk_state;
	int queued = 0;

	gmtp_print_debug("gmtp_rcv_state_process");

	/*
	 *  Step 3: Process LISTEN state
	 *
	 *  If P.type == Request
	 *  (* Generate a new socket and switch to that socket *)
	 *	      Set S := new socket for this port pair
	 *	      S.state = GMTP_RESPOND
	 *	      Choose S.ISS (initial seqno)
	 *	      Initialize S.GAR := S.ISS
	 *	      Set S.ISR, S.GSR, S.SWL, S.SWH from packet
	 *	      Continue with S.state == RESPOND
	 *	      (* A Response packet will be generated in Step 11 (dccp) *)
	 *	 Otherwise,
	 *	      Generate Reset(No Connection) unless P.type == Reset
	 *	      Drop packet and return
	 */

	if (sk->sk_state == GMTP_LISTEN) {
		gmtp_print_debug("state == GMTP_LISTEN");
		if (gh->type == GMTP_PKT_REQUEST) {

			gmtp_print_debug("type == GMTP_PKT_REQUEST");

			if (inet_csk(sk)->icsk_af_ops->conn_request(sk, skb) < 0) {
				gmtp_print_debug("inet_csk(sk)->icsk_af_ops->conn_request(sk, skb) < 0");
				return 1;
			}
			goto discard;
		}
		if (gh->type == GMTP_PKT_RESET) {
			gmtp_print_debug("type == GMTP_PKT_REQUEST (goto discard...)");
			goto discard;
		}

		/* Caller (gmtp_v4_do_rcv) will send Reset */
		gmtp_print_debug("Caller (gmtp_v4_do_rcv) will send Reset...");
		dcb->gmtpd_reset_code = GMTP_RESET_CODE_NO_CONNECTION;
		return 1;
	} else if (sk->sk_state == GMTP_CLOSED) {
		gmtp_print_debug("state == GMTP_CLOSED");
		dcb->gmtpd_reset_code = GMTP_RESET_CODE_NO_CONNECTION;
		return 1;
	}

	/* Step 6: Check sequence numbers (omitted in LISTEN/REQUEST state) */
	//TODO check sequence number
	if (sk->sk_state != GMTP_REQUESTING/* && dccp_check_seqno(sk, skb)*/)
	{
		gmtp_print_debug("state != GMTP_REQUESTING (goto discard...)");
		goto discard;
	}

	/*
	 *   Step 7: Check for unexpected packet types
	 *      If (S.is_server and P.type == Response)
	 *	    or (S.is_client and P.type == Request)
	 *	    or (S.state == RESPOND and P.type == Data),
	 *	  Send Sync packet acknowledging P.seqno
	 *	  Drop packet and return
	 */
	if ((dp->gmtps_role != GMTP_ROLE_CLIENT &&
			gh->type == GMTP_PKT_RESPONSE) ||
			(dp->gmtps_role == GMTP_ROLE_CLIENT &&
					gh->type == GMTP_PKT_REQUEST) ||
					(sk->sk_state == GMTP_RESPOND && gh->type == GMTP_PKT_DATA)) {
		//TODO Send sync
		//dccp_send_sync(sk, dcb->dccpd_seq, DCCP_PKT_SYNC);
		gmtp_print_debug("unexpected packet types...");
		goto discard;
	}

	/*  Step 8: Process options */
	//TODO Parse options
//	if (dccp_parse_options(sk, NULL, skb))
//		return 1;

	/*
	 *  Step 9: Process Reset
	 *	If P.type == Reset,
	 *		Tear down connection
	 *		S.state := TIMEWAIT
	 *		Set TIMEWAIT timer
	 *		Drop packet and return
	 */
	//TODO Tratar RESET e CLOSE
	if (gh->type == GMTP_PKT_RESET) {
		//dccp_rcv_reset(sk, skb);
		return 0;
//	} else if (gh->type == GMTP_PKT_CLOSEREQ) {	/* Step 13 */
//		if (dccp_rcv_closereq(sk, skb))
//			return 0;
//		goto discard;
	} else if (gh->type == GMTP_PKT_CLOSE) {		/* Step 14 */
//		if (dccp_rcv_close(sk, skb))
//			return 0;
		goto discard;
	}

	switch (sk->sk_state) {
	case GMTP_REQUESTING:
		gmtp_print_debug("State == GMTP_REQUESTING");
//		queued = dccp_rcv_request_sent_state_process(sk, skb, gh, len);
		if (queued >= 0)
			return queued;

		__kfree_skb(skb);
		return 0;

		//TODO Tratar GMTP_CLOSE...
//	case DCCP_PARTOPEN:
//		/* Step 8: if using Ack Vectors, mark packet acknowledgeable */
//		dccp_handle_ackvec_processing(sk, skb);
//		dccp_deliver_input_to_ccids(sk, skb);
//		/* fall through */
	case GMTP_RESPOND:
//		queued = dccp_rcv_respond_partopen_state_process(sk, skb,
//				dh, len);
		queued = 0;
		break;
	}

//	if (gh->type == GMTP_PKT_ACK ||
//			gh->type == GMTP_PKT_DATAACK) {
//		switch (old_state) {
//		case DCCP_PARTOPEN:
//			sk->sk_state_change(sk);
//			sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);
//			break;
//		}
//	} else if (unlikely(gh->type == GMTP_PKT_SYNC)) {
//		dccp_send_sync(sk, dcb->dccpd_seq, GMTP_PKT_SYNCACK);
		//goto discard;
	//}


	if (!queued) {
discard:
		__kfree_skb(skb);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_rcv_state_process);








