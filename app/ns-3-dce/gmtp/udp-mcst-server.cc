/* Send Multicast Datagram code example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gmtp.h"

using namespace std;

int main(int argc, char *argv[])
{
	struct in_addr localInterface;
	struct sockaddr_in groupSock;
	int sd;
	char buffer[BUFF_SIZE];

	if(argc < 2) {
		printf("usage: server < local ip address >\n");
		exit(1);
	}
	char *saddr = argv[1];

	/* Create a datagram socket on which to send. */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0) {
		printf("Opening datagram socket error");
		exit(1);
	} else
		printf("Opening the datagram socket...OK.\n");


	/* Initialize the group sockaddr structure with a */
	/* group address of 225.1.1.1 and port 5555. */
	memset((char *)&groupSock, 0, sizeof(groupSock));
	groupSock.sin_family = AF_INET;
	groupSock.sin_addr.s_addr = inet_addr("225.1.2.4");
	groupSock.sin_port = htons(SERVER_PORT);

	/* Disable loopback so you do not receive your own datagrams.
	 {
	 char loopch = 0;
	 if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
	 {
	 printf("Setting IP_MULTICAST_LOOP error");
	 close(sd);
	 exit(1);
	 }
	 else
	 printf("Disabling the loopback...OK.\n");
	 }
	 */

	/* Set local interface for outbound multicast datagrams. */
	/* The IP address specified must be associated with a local, */
	/* multicast capable interface. */
//	localInterface.s_addr = inet_addr("192.168.1.100");
	localInterface.s_addr = inet_addr(saddr);
	if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface,
			sizeof(localInterface)) < 0) {
		printf("Setting local interface error");
		exit(1);
	} else
		printf("Setting the local interface...OK\n");

	double t1  = time_ms(tv);
	int i;
	const char *msg = " Hello, World!";
	double total_data, total;
	int media_rate = 300000; // B/s

	printf("Sending data...\n");
	for(i = 0; i < 10000; ++i) {
		const char *numstr = NumStr(i + 1); //Do not delete this.
		char *buffer = new char[BUFF_SIZE ];
		strcpy(buffer, numstr);
		strcat(buffer, msg);
		int pkt_size = BUFF_SIZE + 36 + 20;

		//Control TX rate
		double sleep_time = (double)(pkt_size * 1000) / media_rate;
		ms_sleep(sleep_time); //control tx rate

		int ret = sendto(sd, buffer, BUFF_SIZE, 0, (struct sockaddr*)&groupSock,
				sizeof(groupSock));
		if(ret < 0) {
			printf("Sending datagram message error");
		} else {
			total += pkt_size;
			total_data += BUFF_SIZE;
			if(i % 1000 == 0) {
				print_stats(i, t1, total, total_data);
				cout << endl;
			}
		}
		delete buffer;
	}

	const char *outstr = "out";
	printf("Sending out: %s\n", outstr);
	sendto(sd, outstr, strlen(outstr), 0, (struct sockaddr*)&groupSock,
					sizeof(groupSock));

	printf("Closing server...\n");
	printf("Server closed!\n\n");

	return 0;
}
