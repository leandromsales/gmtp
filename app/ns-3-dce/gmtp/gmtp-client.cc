#include "stdio.h"    
#include "stdlib.h"    
#include "sys/types.h"    
#include "sys/socket.h"
#include <sys/time.h>
#include "netinet/in.h"    
#include "netdb.h"  
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sys/ioctl.h>

#include "gmtp.h"

#define MY_TIME(time) (time - (double)1262300000000 + (double)(rand()%1000000))

int main(int argc, char**argv)
{
	struct sockaddr_in addr, cl_addr;
	int sockfd, ret;
	struct hostent * server;
	char * serverAddr;
	char buffer[BUFF_SIZE];
	srand(time(NULL));

	double tlog = time_ms(tv);
	char filename[17];
	sprintf(filename, "gmtp-%0.0f.log", MY_TIME(time_ms(tv)));
	FILE *log;
	log = fopen (filename,"w");
	if (log == NULL) {
		printf("Error while creating file\n");
		exit(1);
	}
	//symlink(filename, "gmtp.log");

	// Col 0: total bytes
	// Col 1: data bytes
	// Col 2: tstamp
	double hist[GMTP_SAMPLE][4];

	if(argc < 2) {
		printf("usage: client < ip address >\n");
		exit(1);
	}
	printf("Starting GMTP Client...\n");
	printf("Logfile: %s\n", filename);
	memset(&hist, 0, sizeof(hist));

	serverAddr = argv[1];
	sockfd = socket(AF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(sockfd, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, "1234567812345678", 16);

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

	printf("Connected to the server...\r\n\r\n");
	print_log_header(log);

	int i = 0, seq;
	double rcv=0, rcv_data=0;
	double t0 = time_ms(tv);
//	double ti = time_ms(tv);
	const char *outstr = "out";
	do {
		ssize_t bytes_read;
		memset(buffer, '\0', BUFF_SIZE); //Clean buffer
		bytes_read = recv(sockfd, buffer, BUFF_SIZE, 0);
		if(bytes_read < 1) break;
		++i;
		rcv += bytes_read + 36 + 20;
		rcv_data += bytes_read;
		printf("Received (%d): %s\n", i, buffer);
		char *seqstr = strtok(buffer, " ");

//		if(time_ms(tv) - ti >= 500) {  //Log every 500ms
//			ti = time_ms(tv);
//			update_client_stats(i, atoi(seqstr), t0, rcv, rcv_data,
//					hist, log);
//		}

		update_client_stats(i, atoi(seqstr), t0, rcv, rcv_data, hist,
				log);

	} while(strcmp(buffer, outstr) != 0);

	printf("End of simulation...\n");

	// Jamais remover!!!
	// Resolve o Bug do ether_addr_copy(...):
	//    Se o cliente morrer antes do servidor, o NS-3 não consegue
	//    ler/copiar o mac do skb que chega da rede...
	//     nem retorna nulo... não há como validar...
	//     toda vez que ocorrer esse bug é porque o nó cliente não existe mais
	//	21/08/15 - 4:00 AM
	sleep(3);
	fclose(log);

	return 0;
}

