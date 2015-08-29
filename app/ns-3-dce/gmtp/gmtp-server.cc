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
	int media_rate = 300000; // B/s

	cout << "Starting GMTP Server..." << endl;
	welcomeSocket = socket(PF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(welcomeSocket, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, "1234567812345678", 16);
//	welcomeSocket = socket(AF_INET, SOCK_STREAM, 0);

	cout << "Limiting tx_rate to " << media_rate << " B/s" << endl;
	setsockopt(welcomeSocket, SOL_GMTP, GMTP_SOCKOPT_MEDIA_RATE, &media_rate, sizeof(media_rate));

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
		const char *numstr = NumStr(i+1);
		char *buffer = new char(BUFF_SIZE);
		strcpy(buffer, msg);
		strcat(buffer, numstr);
		int pkt_size = BUFF_SIZE + 36 + 20;

		//Control TX rate
		double sleep_time = (double)(pkt_size * 1000)/media_rate;
		ms_sleep(sleep_time); //control tx rate

		send(newSocket, buffer, BUFF_SIZE, 0);
		total += pkt_size;
		total_data += BUFF_SIZE;
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
