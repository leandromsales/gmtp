#include <sys/socket.h>
#include <arpa/inet.h>
#include <sstream>
#include <string>
#include <iostream>
#include <sys/time.h>
#include <cstdio>

#ifndef GMTP_H_
#define GMTP_H_

#define SOCK_GMTP     7
#define IPPROTO_GMTP  254
#define SOL_GMTP      281

#define SERVER_PORT 2000
#define BUFF_SIZE (int) 744  /* 544 + 56 = 600 B/pkt */
#define GMTP_SAMPLE 100

#define NumStr(Number) static_cast<ostringstream*>( &(ostringstream() << Number) )->str().c_str()

static struct timeval  tv;

enum gmtp_ucc_type {
	GMTP_DELAY_UCC = 0,
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

void set_gmtp_inter(int actived)
{
	int ok = 1;
	int rsock = socket(PF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(rsock, SOL_GMTP, GMTP_SOCKOPT_ROLE_RELAY, &ok, sizeof(int));
	setsockopt(rsock, SOL_GMTP, GMTP_SOCKOPT_RELAY_ENABLED, &actived,
			sizeof(int));
}

inline void disable_gmtp_inter()
{
	set_gmtp_inter(false);
}

inline void enable_gmtp_inter()
{
	set_gmtp_inter(true);
}


double time_ms(struct timeval &tv)
{
	gettimeofday(&tv, NULL);
	double t2 = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000; // convert tv_sec & tv_usec to millisecond
	return t2;
}

static void ms_sleep(double ms)
{
	struct timespec slt;
	slt.tv_nsec = (long)(ms * 1000000.0);
	slt.tv_sec = 0;
	nanosleep(&slt, NULL);
}

static void print_stats(int i, double t1, double total, double total_data)
{
	double t2 = time_ms(tv);
	double elapsed = t2 - t1;

	std::cout << i << " packets sent in " << elapsed << " ms!" << std::endl;
	std::cout << total_data << " data bytes sent."<< std::endl;
	std::cout << total << " bytes sent (data+hdr)" << std::endl;
	std::cout << "Data TX: " << total_data*1000/elapsed << " B/s" << std::endl;
	std::cout << "TX: " << (total*1000)/elapsed << " B/s" << std::endl;
}


#define print_log_header() fprintf(stderr, "idx\tseq\tloss\telapsed\tbytes_rcv\trx_rate\tinst_rx_rate\r\n\r\n");

enum {
	TOTAL_BYTES,
	DATA_BYTES,
	TSTAMP,
	SEQ
};

/**
 * @i Sequence number
 * @begin
 * @rcv bytes received
 * @rcv_data data bytes received
 */
static void update_client_stats(int i, int seq, double begin, double rcv,
		double rcv_data, double hist[][4])
{
	double now = time_ms(tv);
	double total_time = now - begin;
	int index = (i-1) % GMTP_SAMPLE;
	int next = (index == (GMTP_SAMPLE-1)) ? 0 : (index + 1);
	int prev = (index == 0) ? (GMTP_SAMPLE-1) : (index - 1);
	double elapsed = 0;
	int loss = 0;

	hist[index][TOTAL_BYTES] = rcv;
	hist[index][DATA_BYTES] = rcv_data;
	hist[index][TSTAMP] = now;
	hist[index][SEQ] = seq;

	double rcv_sample = hist[index][TOTAL_BYTES] - hist[next][TOTAL_BYTES];
	double rcv_data_sample = hist[index][DATA_BYTES] - hist[next][DATA_BYTES];
	double instant = hist[index][TSTAMP] - hist[next][TSTAMP];
	if(i > 1) {
		elapsed = hist[index][TSTAMP] - hist[prev][TSTAMP];
		loss = (hist[index][SEQ] - hist[prev][SEQ]) - 1;
	}

	//index, seq, loss, elapsed, bytes_rcv, rx_rate, inst_rx_rate
	fprintf(stderr, "%d\t%d\t%d\t%0.2f\t%0.2f\t%0.2f\t%0.2f\r\n", i, seq, loss, elapsed, rcv,
			rcv*1000 / total_time, rcv_sample*1000 / instant);

}


#endif /* GMTP_H_ */
