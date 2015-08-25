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

struct timeval  tv;

using namespace std;

inline void ms_sleep(double ms)
{
	struct timespec slt;
	slt.tv_nsec = (long)(ms * 1000000.0);
	slt.tv_sec = 0;
	nanosleep(&slt, NULL);
}

inline void print_stats(int i, double t1, double total, double total_data)
{
	double t2 = time_ms(tv);
	double elapsed = t2 - t1;

	cout << i << " packets sent in " << elapsed << " ms!" << endl;
	cout << total_data << " data bytes sent."<< endl;
	cout << total << " bytes sent (data+hdr)" << endl;
	cout << "Data TX: " << total_data*1000/elapsed << " B/s" << endl;
	cout << "TX: " << (total*1000)/elapsed << " B/s" << endl;
}

int main(int argc, char *argv[])
{
	int welcomeSocket, newSocket;
	struct sockaddr_in serverAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size;
	int max_tx = 300000; // Bps

	cout << "Starting GMTP Server..." << endl;
	welcomeSocket = socket(PF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(welcomeSocket, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, "1234567812345678", 16);
//	welcomeSocket = socket(AF_INET, SOCK_STREAM, 0);

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

	double t1  = time_ms(tv);
	int i;
	const char *msg = "Hello, World! ";
	double total_data, total;

	cout << "Sending data...\n" << endl;
	for(i = 0; i < 10000; ++i) {
		double tmp = time_ms(tv);
		ms_sleep(0.1);
//		cout << "sleep: " << time_ms(tv) - tmp << " ms" << endl;
		const char *numstr = NumStr(i+1);
		char *buffer = new char(BUFF_SIZE);
		strcpy(buffer, msg);
		strcat(buffer, numstr);
//		int size = strlen(msg) + strlen(numstr)+1;
		int size = BUFF_SIZE;
		send(newSocket, buffer, size, 0);
		total += size + 36 + 20;
		total_data += size;
		delete(buffer);
		if(i % 1000 == 0) {
			print_stats(i, t1, total, total_data);
			cout << endl;
		}
	}

	print_stats(i, t1, total, total_data);

	const char *outstr = "out";
	// Send 'out' 5 times for now... gmtp-inter bug...
	for(i = 0; i < 6; ++i) {
		printf("Sending out: %s\n", outstr);
		send(newSocket, outstr, strlen(outstr), 0);
	}

	printf("Closing server...\n");
	close(newSocket);
	close(welcomeSocket);

	printf("Server closed!\n\n");

	return 0;
}
