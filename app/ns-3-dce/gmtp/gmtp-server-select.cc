#include "stdio.h"
#include "stdlib.h"
#include "sys/types.h"
#include "sys/socket.h"
#include "string.h"
#include "netinet/in.h"
#include "unistd.h"
#include "arpa/inet.h"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <cerrno>
#include<pthread.h>

#include "gmtp.h"

#define BUFF_SIZE 64
#define CLADDR_LEN 100
#define MAX_THREADS 256

#define SERVER_PORT 2000

using namespace std;

pthread_mutex_t lock;

static inline void print_stats(int i, time_t start, int total, int total_data)
{
	time_t elapsed = time(0) - start;
	if(elapsed == 0)
		elapsed = 1;
	cout << i << " packets sent in " << elapsed << " s!" << endl;
	cout << total_data << " data bytes sent (" << total_data / i << " B/packet)" << endl;
	cout << total << " bytes sent (data+hdr) (" << total / i << " B/packet)" << endl;
	cout << "Data TX: " << total_data / elapsed << " B/s" << endl;
	cout << "TX: " << total / elapsed << " B/s" << endl;
}

void handle(int newsock, fd_set *set)
{
	time_t start = time(0);
	int i;
	const char *msg = "Hello, World! ";
	int total_data, total;
	for(i = 0; i < 10000; ++i) {
		const char *numstr = NumStr(i + 1);
		pthread_mutex_lock(&lock);
		char *buffer = new char(BUFF_SIZE);
		strcpy(buffer, msg);
		strcat(buffer, numstr);
		int size = strlen(msg) + strlen(numstr) + 1;
		send(newsock, buffer, size, 0);
		pthread_mutex_unlock(&lock);
		total += (size + 36 + 20);
		total_data += size;
		delete (buffer);
	}

	print_stats(i, start, total, total_data);

	const char *outstr = "out";
	// FIXME Send 'out' 6 times for now... gmtp-inter bug in NS-3
	for(i = 0; i < 6; ++i) {
		printf("Sending out: %s\n", outstr);
		send(newsock, outstr, strlen(outstr), 0);
	}
	FD_CLR(newsock, set);
	close(newsock);
}

int main()
{
	struct sockaddr_in addr, cl_addr;
	int sockfd, ret;
	socklen_t len;
	fd_set socks;
	fd_set readsocks;
	fd_set writesocks;
	int maxsock;
	int reuseaddr = 1;
	int max_tx = 20000;

	sockfd = socket(AF_INET, SOCK_GMTP, IPPROTO_GMTP);
//	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		printf("Error creating socket!\n");
		exit(1);
	}
	printf("Socket created...\n");

	printf("Calling setsockopt...\n");
	setsockopt(sockfd, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, "1234567812345678", 16);
	setsockopt(sockfd, SOL_GMTP, GMTP_SOCKOPT_MAX_TX_RATE, &max_tx, sizeof(max_tx));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(SERVER_PORT);

	/* Enable the socket to reuse the address */
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int))) {
		perror("setsockopt");
		return 1;
	}

	ret = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if(ret < 0) {
		printf("Error binding!\n");
		exit(1);
	}
	printf("Binding done...\n");

	printf("Waiting for connections...\n");
	listen(sockfd, 0);

	/* Set up the fd_set */
	FD_ZERO(&socks);
	FD_SET(sockfd, &socks);
	maxsock = sockfd;

	/* Main loop */
	while(1) {
		unsigned int s;
		readsocks = socks;
		writesocks = socks;
		if(select(maxsock + 1, &readsocks, &writesocks, NULL, NULL)== -1) {
			perror("Select");
			return 1;
		}
		for(s = 0; s <= maxsock; s++) {
			if(FD_ISSET(s, &readsocks) || FD_ISSET(s, &writesocks)) {
				printf("Socket %d was ready\n", s);

				if(s == sockfd) {
					/* New connection */
					int newsockfd;
					struct sockaddr_in their_addr;
					socklen_t size = sizeof(struct sockaddr_in);
					newsockfd = accept(sockfd, (struct sockaddr*)&their_addr,&size);
					if(newsockfd == -1) {
						perror("accept");
					} else {
						printf("Got a connection from %s on port %d\n",
								inet_ntoa(their_addr.sin_addr),
								htons(their_addr.sin_port));
						FD_SET(newsockfd, &socks);
						if(newsockfd > maxsock) {
							maxsock = newsockfd;
						}
					}
				} else {
					/* Handle read or disconnection */

					int pid = fork();
					if(pid < 0) {
						perror("ERROR on fork");
						exit(1);
					}
					if(pid == 0) {
						/* This is the client process */
						close(sockfd);
						handle(s, &socks);
					}
				}
			}
		}
	}

	close(sockfd);

	return 0;
}

