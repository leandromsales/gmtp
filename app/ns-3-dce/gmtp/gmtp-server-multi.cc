#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <cstdlib>

#include "gmtp.h"

using namespace std;

pthread_mutex_t lock;

static inline void print_stats(int i, time_t start, int total, int total_data)
{
	time_t elapsed = time(0) - start;
	if(elapsed==0) elapsed=1;
	cout << i << " packets sent in " << elapsed << " s!" << endl;
	cout << total_data << " data bytes sent (" << total_data/i << " B/packet)" << endl;
	cout << total << " bytes sent (data+hdr) (" << total/i << " B/packet)" << endl;
	cout << "Data TX: " << total_data/elapsed << " B/s" << endl;
	cout << "TX: " << total/elapsed << " B/s" << endl;
}

void handle(int newsock)
{
	cout << "Connected with client!" << endl;
	time_t start = time(0);
	int i;
	const char *msg = "Hello, World! ";
	int total_data, total;
	for(i = 0; i < 10000; ++i) {
		const char *numstr = NumStr(i + 1);
		char *buffer = new char(BUFF_SIZE);
		strcpy(buffer, msg);
		strcat(buffer, numstr);
		int size = strlen(msg) + strlen(numstr) + 1;
		send(newsock, buffer, size, 0);
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
	close(newsock);
}

int main(int argc, char *argv[])
{
	int welcomeSocket;
	struct sockaddr_in serverAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size;
	int max_tx = 30000; // Bps

	cout << "Starting GMTP Server..." << endl;
	welcomeSocket = socket(PF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(welcomeSocket, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, "1234567812345678", 16);

	cout << "Limiting tx_rate to " << max_tx << " B/s" << endl;
	setsockopt(welcomeSocket, SOL_GMTP, GMTP_SOCKOPT_MAX_TX_RATE, &max_tx, sizeof(max_tx));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

	bind(welcomeSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

	if(listen(welcomeSocket, 5) == 0)
		cout << "Listening" << endl;
	else
		cout << "Error\n" << endl;
;
	addr_size = sizeof serverStorage;
	while(true) {

		int newSocket = accept(welcomeSocket,
				(struct sockaddr *)&serverStorage, &addr_size);

		if(newSocket > 0) {
			cout << "New socket: " << newSocket << endl;
			int pid = fork();
			if(pid < 0) {
				perror("ERROR on fork\n");
				exit(1);
			}
			if(pid == 0) {
				handle(newSocket);
			}
		}
	}

	return 0;
}
