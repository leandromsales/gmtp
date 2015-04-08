/*
 *  net/gmtp/qpolicy.c
 *
 *  Policy-based packet dequeueing interface for GMTP.
 *
 *  Thanks to Tomasz Grobelny <tomasz@grobelny.oswiecenia.net> (gmtp)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License v2
 *  as published by the Free Software Foundation.
 */


#include "gmtp.h"

/*
 *	Simple Dequeueing Policy:
 *	If qlen is different from 0, enqueue up to qlen elements.
 */
static void qpolicy_simple_push(struct sock *sk, struct sk_buff *skb)
{
	skb_queue_tail(&sk->sk_write_queue, skb);
}

static bool qpolicy_simple_full(struct sock *sk)
{
	return gmtp_sk(sk)->qlen &&
	       sk->sk_write_queue.qlen >= gmtp_sk(sk)->qlen;
}

static struct sk_buff *qpolicy_simple_top(struct sock *sk)
{
	return skb_peek(&sk->sk_write_queue);
}

/**
 * struct gmtp_qpolicy_operations  -  Packet Dequeueing Interface
 * @push: add a new @skb to the write queue
 * @full: indicates that no more packets will be admitted
 * @top:  peeks at whatever the queueing policy defines as its `top'
 */
static struct gmtp_qpolicy_operations {
	void		(*push)	(struct sock *sk, struct sk_buff *skb);
	bool		(*full) (struct sock *sk);
	struct sk_buff*	(*top)  (struct sock *sk);
	__be32		params;

} qpol_table[GMTPQ_POLICY_MAX] = {
	[GMTPQ_POLICY_SIMPLE] = {
		.push   = qpolicy_simple_push,
		.full   = qpolicy_simple_full,
		.top    = qpolicy_simple_top,
		.params = 0,
	},
};

/*
 *	Externally visible interface
 */
void gmtp_qpolicy_push(struct sock *sk, struct sk_buff *skb)
{
	qpol_table[gmtp_sk(sk)->qpolicy].push(sk, skb);
}

bool gmtp_qpolicy_full(struct sock *sk)
{
	return qpol_table[gmtp_sk(sk)->qpolicy].full(sk);
}

void gmtp_qpolicy_drop(struct sock *sk, struct sk_buff *skb)
{
	if (skb != NULL) {
		skb_unlink(skb, &sk->sk_write_queue);
		kfree_skb(skb);
	}
}

struct sk_buff *gmtp_qpolicy_top(struct sock *sk)
{
	return qpol_table[gmtp_sk(sk)->qpolicy].top(sk);
}

struct sk_buff *gmtp_qpolicy_pop(struct sock *sk)
{
	struct sk_buff *skb = gmtp_qpolicy_top(sk);

	if (skb != NULL) {
		/* Clear any skb fields that we used internally */
		skb->priority = 0;
		skb_unlink(skb, &sk->sk_write_queue);
	}
	return skb;
}
