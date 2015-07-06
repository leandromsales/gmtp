#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <errno.h>

#include "gmtp.h"

#define SERVER_PORT 2000

int main (int argc, char *argv[])
{
  int clientSocket;
  char buffer[1024];
  struct sockaddr_in serverAddr;
  socklen_t addr_size;

  disable_gmtp_inter();
  clientSocket = socket (PF_INET, SOCK_GMTP, IPPROTO_GMTP);
  setsockopt(clientSocket, SOL_GMTP, GMTP_SOCKOPT_FLOWNAME, "1234567812345678", 16);
  
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons (SERVER_PORT);
  
  struct hostent *host = gethostbyname (argv[1]);
  memcpy (&serverAddr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);

  addr_size = sizeof serverAddr;
  connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size);
  
  std::cout << "Connected! Waiting for data...\n";
  
  int i = 0;
 
  do {
	  recv(clientSocket, buffer, 1024, 0);
	  std::cout << "Data received (" << ++i << "): " << buffer << std::endl;
	  
  } while(strcmp(buffer, "out") != 0);
     
  return 0;
}
