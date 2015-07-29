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

#include "gmtp.h"

#define PORT 2000
#define BUF_SIZE 1024

using namespace std;

int main(int argc, char**argv)
{
	struct sockaddr_in addr, cl_addr;
	int sockfd, ret;
	struct hostent * server;
	char * serverAddr;
	char buffer[BUF_SIZE];

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
	int i = 0;
	do {
		recv(sockfd, buffer, BUF_SIZE, 0);
		++i;
//		printf("Received (%d): %s\n", i, buffer);
	} while(strcmp(buffer, "out") != 0);

	printf("%d packets received!\n", i);

	return 0;
}

