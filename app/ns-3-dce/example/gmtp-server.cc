#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>

#include "gmtp.h"

#define SERVER_PORT 2000
#define MAX_CONNECTION 10

int main(int argc, char *argv[])
{
	int welcomeSocket, clientSocket;
	struct sockaddr_in serverAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size;

	disable_gmtp_inter();
	welcomeSocket = socket(PF_INET, SOCK_GMTP, IPPROTO_GMTP);
	setsockopt(welcomeSocket, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, "1234567812345678", 16);

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

	bind(welcomeSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

	if (listen(welcomeSocket, MAX_CONNECTION) == 0) {
		std::cout << "Listening for connections\n";
	} else {
		std::cout << "Error while trying to listen connection. Is GMTP modules loaded?\n";
		return 1;
	}
	addr_size = sizeof serverStorage;
	clientSocket = accept(welcomeSocket, (struct sockaddr *)&serverStorage,
			&addr_size);

	int i;
	char msg[] = "Hello, World!";
	for(i = 1; i <= 100; ++i) {
		std::cout << "Sending (" << i << "): " << msg << std::endl;
		send(clientSocket, &msg, strlen(msg) + 1, 0);
	}

	char exit[] = "exit";
	std::cout << "Sending exit: " << exit << std::endl;
	send(clientSocket, &exit, strlen(exit) + 1, 0);
	close(clientSocket);

	return 0;
}
