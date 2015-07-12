#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/param.h>

#include "gmtp.h"

#define SERVER_PORT 12345
#define MAX_CONNECTION 10
#define PACKET_SIZE 1024

int main(int argc, char *argv[])
{
	int welcomeSocket, clientSocket;
	struct sockaddr_in serverAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size;

	disable_gmtp_inter();
	welcomeSocket = socket(AF_INET, SOCK_GMTP, IPPROTO_GMTP);
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
	size_t bytes_written = 0;
	ssize_t wrote = 0;
	char msg[] = "Hello, world!";
	size_t size = sizeof(msg); 
	for(i = 1; i <= 1000; ++i) {
		while (bytes_written < size) {
			do {
				wrote = send (clientSocket, (char *) msg + bytes_written, MIN (PACKET_SIZE, size - bytes_written), 0);
			} while (wrote == -1 && errno == EAGAIN);
			if (wrote >= 0) {
				bytes_written += wrote;
				std::cout << "Message " << i << " sent: " << msg <<  std::endl;
			} else {
				std::cout << "Error writing message " << i << ". Message: " << strerror(errno) <<  std::endl;
				break;
			}
		}
		bytes_written = 0;
	}

	char exit[] = "exit";
	std::cout << "Sending exit: " << exit << std::endl;
	send(clientSocket, &exit, strlen(exit) + 1, 0);
	close(clientSocket);

	return 0;
}
