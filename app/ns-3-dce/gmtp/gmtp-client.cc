#include "stdio.h"    
#include "stdlib.h"    
#include "sys/types.h"    
#include "sys/socket.h"
#include <sys/time.h>
#include "string.h"    
#include "netinet/in.h"    
#include "netdb.h"  
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <sys/ioctl.h>

#include "gmtp.h"

struct timeval tv;

double last_rcv=0, last_data_rcv=0;

// Col 0: total bytes
// Col 1: data bytes
// Col 2: tstamp
double hist[GMTP_SAMPLE][3];

inline void update_stats(int i, double begin, double rcv, double rcv_data)
{
	double now = time_ms(tv);
	double total_time = now - begin;
	int index = (i-1) % GMTP_SAMPLE;
	int next = (index == (GMTP_SAMPLE-1)) ? 0 : (index + 1);

	hist[index][0] = rcv;
	hist[index][1] = rcv_data;
	hist[index][2] = now;

	double rcv_sample = hist[index][0] - hist[next][0];
	double rcv_data_sample = hist[index][1] - hist[next][1];
	double instant = hist[index][2] - hist[next][2];

	if(i >= GMTP_SAMPLE) {
		printf("Data RX: %0.2f B/s\n", (rcv_data_sample*1000)/instant);
		printf("RX: %0.2f B/s\n", (rcv_sample*1000)/instant);
	}

	printf("Total Data RX: %0.2f B/s\n", (rcv_data*1000)/total_time);
	printf("Total RX: %0.2f B/s\n", (rcv*1000)/total_time);
	printf("-----------------\n");
}

int main(int argc, char**argv)
{
	struct sockaddr_in addr, cl_addr;
	int sockfd, ret;
	struct hostent * server;
	char * serverAddr;
	char buffer[BUFF_SIZE];

	if(argc < 2) {
		printf("usage: client < ip address >\n");
		exit(1);
	}
	printf("Starting GMTP Client...\n");
	memset(&hist, 0, sizeof(hist));

	serverAddr = argv[1];
	sockfd = socket(AF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(sockfd, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, "1234567812345678", 16);
//	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		printf("Error creating socket! (%d)\n", sockfd);
		exit(1);
	}
	printf("Socket created...\n");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(serverAddr);
	addr.sin_port = htons (SERVER_PORT);

	ret = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if(ret < 0) {
		printf("Error connecting to the server! (%d)\n", ret);
		exit(1);
	}

	printf("Connected to the server...\n\n");

	const char *outstr = "out";
	int i = 0;
	double rcv=0, rcv_data=0;
	double t1 = time_ms(tv);
	do {
		ssize_t bytes_read;
		memset(buffer, '\0', BUFF_SIZE); //Clean buffer
		bytes_read = recv(sockfd, buffer, BUFF_SIZE, 0);
		if(bytes_read < 1) break;
		++i;
		rcv += bytes_read + 36 + 20;
		rcv_data += bytes_read;

		printf("Received (%d): %s (%ld B)\n",  i, buffer, bytes_read);
		update_stats(i, t1, rcv, rcv_data);

	} while(strcmp(buffer, outstr) != 0);

	update_stats(i, t1, rcv, rcv_data);

	double t2 = time_ms(tv);

	double diff = t2-t1;
	printf("%0.2f ms\n", diff);
	printf("RX rate: %0.2f B/s\n\n", (double)rcv*1000/diff);

	// Jamais remover!!!
	// Resolve o Bug do ether_addr_copy(...):
	//    Se o cliente morrer antes do servidor, o NS-3 não consegue
	//    ler/copiar o mac do skb que chega da rede...
	//     nem retorna nulo... não há como validar...
	//     toda vez que ocorrer esse bug é porque o nó cliente não existe mais
	//	21/08/15 - 4:00 AM
	sleep(3);

//	delete [] outstr;

	return 0;
}

