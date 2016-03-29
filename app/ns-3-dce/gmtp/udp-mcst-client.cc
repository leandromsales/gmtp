/* Receiver/client multicast Datagram example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include "gmtp.h"

char* get_addr(const char *interface) {

	struct ifreq ifr;
	int fd = socket(AF_INET, SOCK_DGRAM, 0);

	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;
	printf("Interface: %s\n", interface);

	/* I want IP address attached to "eth0" */
	strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);

	/* display result */
	//printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}

int main(int argc, char *argv[])
{
	struct sockaddr_in localSock;
	struct ip_mreq group;
	int sd;
	char buffer[BUFF_SIZE];

	FILE *log;
	log = fopen ("exemplo.log","w");
	if (log == NULL) {
		printf("Error while creating file\n");
		exit(1);
	}

	// Col 0: total bytes
	// Col 1: data bytes
	// Col 2: tstamp
	double hist[GMTP_SAMPLE][4];

	if(argc < 2) {
		printf("usage: client < interface0 >\n");
		exit(1);
	}
	char *interface = argv[1];

	/* Create a datagram socket on which to receive. */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0) {
		printf("Opening datagram socket error");
		exit(1);
	} else
		printf("Opening datagram socket....OK.\n");


	/* Enable SO_REUSEADDR to allow multiple instances of this */
	/* application to receive copies of the multicast datagrams. */
	{
		int reuse = 1;
		if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse,
				sizeof(reuse)) < 0) {
			printf("Setting SO_REUSEADDR error");
			close(sd);
			exit(1);
		} else
			printf("Setting SO_REUSEADDR...OK.\n");
	}

	/* Bind to the proper port number with the IP address */
	/* specified as INADDR_ANY. */
	memset((char *)&localSock, 0, sizeof(localSock));
	localSock.sin_family = AF_INET;
	localSock.sin_port = htons(SERVER_PORT);
	localSock.sin_addr.s_addr = INADDR_ANY;
	if(bind(sd, (struct sockaddr*)&localSock, sizeof(localSock))) {
		printf("Binding datagram socket error");
		close(sd);
		exit(1);
	} else
		printf("Binding datagram socket...OK.\n");

	/* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
	/* interface. Note that this IP_ADD_MEMBERSHIP option must be */
	/* called for each local interface over which the multicast */
	/* datagrams are to be received. */
	group.imr_multiaddr.s_addr = inet_addr("225.1.2.4");
	group.imr_interface.s_addr = inet_addr(get_addr(interface));
	if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group,
			sizeof(group)) < 0) {
		printf("Adding multicast group error");
		close(sd);
		exit(1);
	} else
		printf("Adding multicast group...OK.\n");

	printf("Waiting data...\r\n\r\n");
	print_client_log_header(log);

	/* Read from the socket. */
	int i = 0, seq;
	double rcv=0, rcv_data=0;
	double t1 = time_ms(tv);
	const char *outstr = "out";
	do {
		ssize_t bytes_read;
		memset(buffer, '\0', BUFF_SIZE); //Clean buffer
		bytes_read = read(sd, buffer, BUFF_SIZE);
		if(bytes_read < 1)
			break;
		++i;
		rcv += bytes_read + 36 + 20;
		rcv_data += bytes_read;

		char *seqstr = strtok(buffer, " ");
		update_client_stats(i, atoi(seqstr), t1, rcv, rcv_data, hist, 0, log);

	} while(strcmp(buffer, outstr) != 0);

	printf("End of simulation...\n");

	sleep(3);
	fclose(log);

	return 0;
}
