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

struct sockaddr_in localSock;
struct ip_mreq group;
int sd;
int datalen;
char databuf[1024];

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
	printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

	return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}



int main(int argc, char *argv[])
{
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
	localSock.sin_port = htons(4321);
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
//	group.imr_interface.s_addr = inet_addr("192.168.1.100");
//	group.imr_interface.s_addr = inet_addr(saddr);
//	group.imr_interface.s_addr = inet_addr("10.1.1.1");
	group.imr_interface.s_addr = inet_addr(get_addr(interface));
	if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group,
			sizeof(group)) < 0) {
		printf("Adding multicast group error");
		close(sd);
		exit(1);
	} else
		printf("Adding multicast group...OK.\n");

	printf("Waiting data...\n");
	/* Read from the socket. */
	datalen = sizeof(databuf);
	if(read(sd, databuf, datalen) < 0) {
		printf("Reading datagram message error");
		close(sd);
		exit(1);
	} else {
		printf("Reading datagram message...OK.\n");
		printf("The message from multicast server is: \"%s\"\n",
				databuf);
	}
	return 0;
}
