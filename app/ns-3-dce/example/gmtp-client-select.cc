#include <stdio.h>    
#include <stdlib.h>    
#include <sys/types.h>    
#include <sys/socket.h>    
#include <string.h>    
#include <netinet/in.h>    
#include <netdb.h>  
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#include "gmtp.h"

#define PORT 2000
#define BUF_SIZE 1024

using namespace std;

int main(int argc, char**argv)
{
	struct sockaddr_in addr, cl_addr;
	int sockfd, ret;
	struct hostent *server;
	char *serverAddr;
	char *buf = NULL;

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
	int i = 1;
	do {
		  fd_set testfds;
		  int maxfdp1;
		  ssize_t bytes_read;
		  int readsize;

		  /* do a blocking select on the socket */
		  FD_ZERO (&testfds);
		  FD_SET (sockfd, &testfds);
		  maxfdp1 = sockfd + 1;

		  /* no action (0) is also an error in our case */
		  printf("Esperando dados... ");
		  if (select (maxfdp1, &testfds, NULL, NULL, 0) <= 0) {
			printf("Select error!");
			exit(1);
		  }
		  printf("chegou!\n");

		  if (ioctl (sockfd, FIONREAD, &readsize) < 0) {
			printf("FIONREAD failed: %s", strerror (errno));
			exit(1);
		  }

		  if (readsize == 0) {
		  	printf("Got EOS on socket stream\n");
		  	exit(1);
		  }

		  delete(buf);
		  buf = (char *) malloc((int) readsize);
		  bytes_read = recv (sockfd, buf, (int) readsize, 0);

		  if (bytes_read != readsize) {
		  	printf("Error while reading data");
		  	exit(1);
		  }
		  i++;
		  printf("message index %d\n", i);
		  printf("bytes read %zu\n", bytes_read);
		  printf("buffer size %d\n", readsize);
		  printf("Content %s\n", buf);
		  printf("=============== \n\n");
	} while (strcmp(buf, "exit") != 0);
  return 0;
}

