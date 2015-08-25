#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>

#include "gmtp.h"

#define SERVER_PORT 2000
#define BUFF_SIZE 64

struct timeval  tv;

using namespace std;

inline void print_stats(int i, double t1, double total, double total_data)
{
	gettimeofday(&tv, NULL);
	double t2 = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000; // convert tv_sec & tv_usec to millisecond
	double elapsed = t2 - t1;

	cout << i << " packets sent in " << elapsed << " ms!" << endl;
	cout << total_data << " data bytes sent."<< endl;
	cout << total << " bytes sent (data+hdr)" << endl;
	cout << "Data TX: " << total_data*1000/elapsed << " B/s" << endl;
	cout << "TX: " << total*1000/elapsed << " B/s" << endl;
}

int main(int argc, char *argv[])
{
	int welcomeSocket, newSocket;
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
		cout << "Listening\n";
	else
		cout << "Error\n";

	addr_size = sizeof serverStorage;
	newSocket = accept(welcomeSocket, (struct sockaddr *)&serverStorage,
			&addr_size);

	cout << "Connected with client!" << endl;
	gettimeofday(&tv, NULL);
	double t1 = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000; // convert tv_sec & tv_usec to millisecond

	int i;
	const char *msg = "Hello, World! ";
	int total_data, total;
	for(i = 0; i < 10000; ++i) {
		const char *numstr = NumStr(i+1);
		char *buffer = new char(BUFF_SIZE);
		strcpy(buffer, msg);
		strcat(buffer, numstr);
		int size = strlen(msg) + strlen(numstr)+1;
		send(newSocket, buffer, size, 0);
		total += size + 36 + 20;
		total_data += size;
		delete(buffer);
	}

	print_stats(i, t1, total, total_data);

	const char *outstr = "out";
	// Send 'out' 5 times for now... gmtp-inter bug...
	for(i = 0; i < 6; ++i) {
		printf("Sending out: %s\n", outstr);
		send(newSocket, outstr, strlen(outstr), 0);
	}

	return 0;
}
