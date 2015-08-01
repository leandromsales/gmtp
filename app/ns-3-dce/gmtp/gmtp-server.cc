#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include <cstdio>
#include "gmtp.h"

#define SERVER_PORT 2000

using namespace std;

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

int main(int argc, char *argv[])
{
	int welcomeSocket, newSocket;
	struct sockaddr_in serverAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size;
	int max_tx = 30000; // Bps

	cout << "Starting GMTP Server..." << endl;

	disable_gmtp_inter();
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
		cout << "Listening\n";
	else
		cout << "Error\n";

	addr_size = sizeof serverStorage;
	newSocket = accept(welcomeSocket, (struct sockaddr *)&serverStorage,
			&addr_size);

	cout << "Connected with client!" << endl;
	time_t start = time(0);
	int i;
	const char *msg = "Hello, World!";
	int total_data, total, size = strlen(msg) + 1;
	for(i = 0; i < 50000; ++i) {
		send(newSocket, msg, size, 0);
		total += size + 36 + 20;
		total_data += size;
	}

	print_stats(i, start, total, total_data);

	const char *outstr = "out";
	// Send 'out' 5 times for now... gmtp-inter bug...
	for(i = 0; i < 6; ++i) {
		printf("Sending out: %s\n", outstr);
		send(newSocket, outstr, strlen(outstr), 0);
	}

	return 0;
}
