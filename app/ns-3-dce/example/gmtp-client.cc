#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <errno.h>

#define SOCK_GTMP     7
#define IPPROTO_GTMP  254
#define SOL_GTMP      281

int main (int argc, char *argv[])
{
  int clientSocket;
  char buffer[1024];
  struct sockaddr_in serverAddr;
  socklen_t addr_size;

  clientSocket = socket (PF_INET, SOCK_GTMP, IPPROTO_GTMP);
  setsockopt(clientSocket, SOL_GTMP, 1, "1234567812345678", 16);
  
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons (2000);
  
  struct hostent *host = gethostbyname (argv[1]);
  memcpy (&serverAddr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);

  addr_size = sizeof serverAddr;
  connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size);

  recv(clientSocket, buffer, 1024, 0);
  std::cout << "Data received: " << buffer << "\n";   

//  close (clientSocket);
   
  return 0;
}
