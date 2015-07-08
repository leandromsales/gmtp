#include "stdio.h"
#include "stdlib.h"
#include "sys/types.h"
#include "sys/socket.h"
#include "string.h"
#include "netinet/in.h"
#include "unistd.h"
#include "arpa/inet.h"

#include "gmtp.h"

#define BUF_SIZE 2000
#define CLADDR_LEN 100

#define PORT 2000

void handle(int newsock, fd_set *set);

int main()
{
	struct sockaddr_in addr, cl_addr;
	int sockfd, ret;
	socklen_t len;
	fd_set socks;
	fd_set readsocks;
	int maxsock;
	int reuseaddr = 1;

	disable_gmtp_inter();
	sockfd = socket(AF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(sockfd, SOL_GMTP, 1, "1234567812345678", 16);
//	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		printf("Error creating socket!\n");
		exit(1);
	}
	printf("Socket created...\n");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons (PORT);

	/* Enable the socket to reuse the address */
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
			sizeof(int))) {
		perror("setsockopt");
		return 1;
	}

	ret = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if(ret < 0) {
		printf("Error binding!\n");
		exit(1);
	}
	printf("Binding done...\n");

	printf("Waiting for a connection...\n");
	listen(sockfd, 5);

	/* Set up the fd_set */
	FD_ZERO(&socks);
	FD_SET(sockfd, &socks);
	maxsock = sockfd;

	/* Main loop */
	while(1) {
	        unsigned int s;
	        readsocks = socks;
	        if (select(maxsock + 1, &readsocks, NULL, NULL, NULL) == -1) {
	            perror("select");
	            return 1;
	        }
	        for (s = 0; s <= maxsock; s++) {
	            if (FD_ISSET(s, &readsocks)) {
	                printf("socket %d was ready\n", s);
	                if (s == sockfd) {
	                    /* New connection */
	                    int newsockfd;
	                    struct sockaddr_in their_addr;
	                    socklen_t size = sizeof(struct sockaddr_in);
	                    newsockfd = accept(sockfd, (struct sockaddr*)&their_addr, &size);
	                    if (newsockfd == -1) {
	                        perror("accept");
	                    } else {
	                        printf("Got a connection from %s on port %d\n",
	                                inet_ntoa(their_addr.sin_addr), htons(their_addr.sin_port));
	                        FD_SET(newsockfd, &socks);
	                        if (newsockfd > maxsock) {
	                            maxsock = newsockfd;
	                        }
	                    }
	                } else {
	                    /* Handle read or disconnection */
	                    handle(s, &socks);
	                }
	            }
	        }

	}

	close(sockfd);

	return 0;
}

void handle(int newsock, fd_set *set)
{
	char buffer[BUF_SIZE] = "Hello, World!";
	send(newsock, buffer, BUF_SIZE, 0);
	FD_CLR(newsock, set);
	close(newsock);
}

