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

#include "gmtp.h"

struct timeval  tv;

inline void print_stats(int i, double t1, int total, int total_data)
{
	double t2 = time_ms(tv);
	double time_elapsed = t2 - t1;

	printf("%d packets received in %0.2f ms!\n", i, time_elapsed);
	printf("%d data bytes received\n", total_data);
	printf("%d bytes received\n", total);
	printf("Data RX: %0.2f B/s\n", ((double)total_data*1000)/time_elapsed);
	printf("RX: %0.2f B/s\n", ((double)total*1000)/time_elapsed);
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

	serverAddr = argv[1];
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
	addr.sin_port = htons (SERVER_PORT);

	ret = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if(ret < 0) {
		printf("Error connecting to the server!\n");
		exit(1);
	}

	printf("Connected to the server...\n\n");

	double t1 = time_ms(tv);
	int i = 0;
	double total, total_data;
	const char *outstr = "out";
	do {
		ssize_t bytes_read;
		memset(buffer, '\0', BUFF_SIZE); //Clean buffer
		bytes_read = recv(sockfd, buffer, BUFF_SIZE, 0);
		if(bytes_read < 1) break;
		++i;
		total += bytes_read + 36 + 20;
		total_data += bytes_read;
		printf("Received (%d): %s (%ld B)\n",  i, buffer, bytes_read);
		print_stats(i, t1, total, total_data);
	} while(strcmp(buffer, outstr) != 0);

	// Jamais remover!!!
	// Resolve o Bug do ether_addr_copy(...):
	//    Se o cliente morrer antes do servidor, o NS-3 não consegue
	//    ler/copiar o mac do skb que chega da rede...
	//     nem retorna nulo... não há como validar...
	//     toda vez que ocorrer esse bug é porque o nó cliente não existe mais
	//	21/08/15 - 4:00 AM
	print_stats(i, t1, total, total_data);

	double t2 = time_ms(tv);

	double diff = t2-t1;
	printf("%0.2f ms\n", diff);
	printf("RX rate: %0.2f B/s\n\n", (double)total*1000/diff);

	sleep(3);

	return 0;
}

