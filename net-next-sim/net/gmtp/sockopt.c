#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "gmtp.h"

static int gmtp_setsockopt_flowname(struct gmtp_sock *gp, char __user *optval,
		unsigned int optlen)
{
	gmtp_print_function();

	if(optlen > GMTP_FLOWNAME_LEN)
		return -ENOBUFS;

	memcpy(gp->flowname, optval, optlen);
	return 0;
}

static int do_gmtp_setsockopt(struct sock *sk, int level, int optname,
		char __user *optval, unsigned int optlen)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	int val, err = 0;

	gmtp_print_function();
	gmtp_print_debug("Set optname: %d | optlen: %u", optname, optlen);

	if (optlen < (int)sizeof(int))
		return -EINVAL;

	if(get_user(val, (int __user *)optval))
		return -EFAULT;

	lock_sock(sk);
	switch(optname) {
	case GMTP_SOCKOPT_FLOWNAME:
		gmtp_pr_debug("GMTP_SOCKOPT_FLOWNAME");
		err = gmtp_setsockopt_flowname(gp, optval, optlen);
		break;
	case GMTP_SOCKOPT_MEDIA_RATE:
		gmtp_pr_debug("GMTP_SOCKOPT_MEDIA_RATE");
		if(val > 0)
			gp->tx_media_rate = (unsigned long)val;
		else
			err = -EINVAL;
		break;
	case GMTP_SOCKOPT_MAX_TX_RATE:
		gmtp_pr_debug("GMTP_SOCKOPT_MAX_TX_RATE");
		if(val > 0) {
			gp->tx_max_rate = (unsigned long)val;
			gp->tx_ucc_rate = gp->tx_max_rate;
		} else
			err = -EINVAL;
		break;
	case GMTP_SOCKOPT_UCC_TYPE:
		gmtp_pr_debug("GMTP_SOCKOPT_UCC_TYPE");
		if(val > 0)
			gp->tx_ucc_type = (enum gmtp_ucc_type)val;
		else
			err = -EINVAL;
		break;
	case GMTP_SOCKOPT_SERVER_TIMEWAIT:
		gmtp_pr_debug("GMTP_SOCKOPT_SERVER_TIMEWAIT");
		if(gp->role != GMTP_ROLE_SERVER)
			err = -EOPNOTSUPP;
		else
			gp->server_timewait = (val != 0)? 1 : 0;
		break;
	case GMTP_SOCKOPT_ROLE_RELAY:
		gmtp_pr_debug("GMTP_SOCKOPT_ROLE_RELAY");
		if(val != 0 && gp->role == GMTP_ROLE_UNDEFINED)
			gp->role = GMTP_ROLE_RELAY;
		else if(val == 0 && gp->role == GMTP_ROLE_RELAY)
			gp->role = GMTP_ROLE_UNDEFINED;
		else
			err = -EOPNOTSUPP;
		break;
	case GMTP_SOCKOPT_RELAY_ENABLED:
		gmtp_pr_debug("GMTP_SOCKOPT_RELAY_ENABLED");
		if(gp->role != GMTP_ROLE_RELAY)
			err = -EOPNOTSUPP;
		else
			gmtp_info->relay_enabled = (val != 0) ? 1 : 0;
		break;
	default:
		err = -ENOPROTOOPT;
		break;
	}
	release_sock(sk);

	if(err != 0)
		pr_warning("gmtp_setsockopt error: %d\n", err);

	return err;
}

int gmtp_setsockopt(struct sock *sk, int level, int optname,
		char __user *optval, unsigned int optlen)
{
	int ret;

	gmtp_print_function();

	if (level == SOL_GMTP)
		ret = do_gmtp_setsockopt(sk, level, optname, optval, optlen);
	else
		ret = inet_csk(sk)->icsk_af_ops->setsockopt(sk, level,
							     optname, optval,
							     optlen);
	return ret;
}
EXPORT_SYMBOL_GPL(gmtp_setsockopt);

static int gmtp_getsockopt_flowname(struct gmtp_sock *gp, char __user *optval,
		unsigned int *optlen)
{
	int len;
	__u8 *val;

	val = gp->flowname;
	len = GMTP_FLOWNAME_LEN * sizeof(*val);

	if (put_user(len, optlen) || copy_to_user(optval, val, len))
		return -EFAULT;

	return 0;
}

static int do_gmtp_getsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, int __user *optlen)
{
	/* TODO Validate every getsockoptc using a C/C++ application */
	struct gmtp_sock *gp;
	int val, len;

	gmtp_print_function();

	if (get_user(len, optlen))
		return -EFAULT;

	if (len < (int)sizeof(int))
		return -EINVAL;

	gp = gmtp_sk(sk);
	gmtp_print_debug("Role: %d", gp->role);
	gmtp_print_debug("Getting => optname: %d", optname);

	switch (optname) {
	case GMTP_SOCKOPT_FLOWNAME:
		return gmtp_getsockopt_flowname(gp, optval, optlen);
	case GMTP_SOCKOPT_MEDIA_RATE:
		val = (int)gp->tx_media_rate;
		break;
	case GMTP_SOCKOPT_MAX_TX_RATE:
		val = (int) gp->tx_max_rate;
		break;
	case GMTP_SOCKOPT_UCC_TX_RATE:
		val = (int) gp->tx_ucc_rate;
		break;
	case GMTP_SOCKOPT_GET_CUR_MSS:
		val = (int) gp->mss;
		break;
	case GMTP_SOCKOPT_SERVER_TIMEWAIT:
		val = (int) gp->server_timewait;
		break;
	case GMTP_SOCKOPT_ROLE_RELAY:
		val = (gp->role == GMTP_ROLE_RELAY)? 1 : 0;
		break;
	case GMTP_SOCKOPT_RELAY_ENABLED:
		val = gmtp_info->relay_enabled;
		break;
	default:
		return -ENOPROTOOPT;
	}

	len = sizeof(val);
	if (put_user(len, optlen) || copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

int gmtp_getsockopt(struct sock *sk, int level, int optname,
		char __user *optval, int __user *optlen)
{
	gmtp_print_function();
	if (level != SOL_GMTP)
		return inet_csk(sk)->icsk_af_ops->getsockopt(sk, level,
				optname, optval,
				optlen);
	return do_gmtp_getsockopt(sk, level, optname, optval, optlen);
}
EXPORT_SYMBOL_GPL(gmtp_getsockopt);
