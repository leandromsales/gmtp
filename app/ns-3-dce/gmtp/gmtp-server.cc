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

int main(int argc, char *argv[])
{
	int welcomeSocket, newSocket;
	char buffer[1024];
	struct sockaddr_in serverAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size;
	int max_tx = 20000;

	disable_gmtp_inter();
	welcomeSocket = socket(PF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(welcomeSocket, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, "1234567812345678", 16);

	printf("Limiting tx_rate...\n");
	setsockopt(welcomeSocket, SOL_GMTP, GMTP_SOCKOPT_MAX_TX_RATE, &max_tx, sizeof(max_tx));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

	bind(welcomeSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

	if(listen(welcomeSocket, 5) == 0)
		std::cout << "Listening\n";
	else
		std::cout << "Error\n";

	addr_size = sizeof serverStorage;
	newSocket = accept(welcomeSocket, (struct sockaddr *)&serverStorage,
			&addr_size);

	int i;
	const char *msg = "Hello, World!";
	for(i = 0; i < 50000; ++i) {
//		std::cout << "Sending (" << i << "): " << msg << std::endl;
		strcpy(buffer, msg);
		send(newSocket, buffer, strlen(msg) + 1, 0);
	}
	delete (msg);

	std::cout << i << " packets sent!" << std::endl;
	const char *out = "out";
	std::cout << "Sending out: " << out << std::endl;
	strcpy(buffer, out);
	send(newSocket, buffer, strlen(out) + 1, 0);
	delete (out);

	return 0;
}
