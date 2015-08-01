#include "stdio.h"    
#include "stdlib.h"    
#include "sys/types.h"    
#include "sys/socket.h"    
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

#include "gmtp.h"

#define PORT 2000
#define BUF_SIZE 64

inline void print_stats(int i, time_t start, int total, int total_data)
{
	time_t time_elapsed = time(0) - start;
	int elapsed = static_cast<int>(time_elapsed);
	if(elapsed==0) elapsed ++;

	printf("%d packets received in %d s!\n", i, (int)elapsed);
	printf("%d data bytes received (%d B/packet)\n", total_data, total/i);
	printf("%d bytes received (%d B/packet)\n", total, total/i);
	printf("Data RX: %d B/s\n", total_data/elapsed);
	printf("RX: %d B/s\n", total/elapsed);
	printf("-----------------\n");
}

int main(int argc, char**argv)
{
	struct sockaddr_in addr, cl_addr;
	int sockfd, ret;
	struct hostent * server;
	char * serverAddr;
	char buffer[BUF_SIZE];

	printf("Starting GMTP Client...\n");

	if(argc < 2) {
		printf("usage: client < ip address >\n");
		exit(1);
	}

	serverAddr = argv[1];

	disable_gmtp_inter();
	sockfd = socket(AF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(sockfd, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, "1234567812345678", 16);
//	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		printf("Error creating socket!\n");
		exit(1);
	}
	printf("Socket created...\n");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(serverAddr);
	addr.sin_port = htons (PORT);

	ret = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if(ret < 0) {
		printf("Error connecting to the server!\n");
		exit(1);
	}

	printf("Connected to the server...\n");
	time_t start = time(0);
	int i = 0;
	int size, total, total_data;
	const char *outstr = "out";
	do {
		memset(buffer, '\0', BUF_SIZE); //Clean buffer
		size = recv(sockfd, buffer, BUF_SIZE, 0);
		++i;
		total += size + 36 + 20;
		total_data += size;
//		if(i%100 == 0) {
			printf("Received (%d): %s\n",  i, buffer);
			print_stats(i, start, total, total_data);
//		}
	} while(strcmp(buffer, outstr) != 0);

	print_stats(i, start, total, total_data);

	return 0;
}

