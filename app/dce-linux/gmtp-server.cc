#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>

#define SERVER_PORT 2000

#define SOCK_GTMP     7
#define IPPROTO_GTMP  254
#define SOL_GTMP      281

int main (int argc, char *argv[])
{
  int welcomeSocket, newSocket;
  char buffer[1024];
  struct sockaddr_in serverAddr;
  struct sockaddr_storage serverStorage;
  socklen_t addr_size;

  welcomeSocket = socket (PF_INET, SOCK_GTMP, IPPROTO_GTMP);
  setsockopt(welcomeSocket, SOL_GTMP, 1, "1234567812345678", 16);
  
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(SERVER_PORT);
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  

  bind(welcomeSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

  if(listen(welcomeSocket,5)==0)
    std::cout << "Listening\n";
  else
     std::cout << "Error\n";

  addr_size = sizeof serverStorage;
  newSocket = accept(welcomeSocket, (struct sockaddr *) &serverStorage, &addr_size);
  
  std::cout << "Sending  'Hello, World!'\n";
  strcpy(buffer,"Hello, World!\n");
  send(newSocket,buffer,13,0);

  //close (newSocket);
  //close (welcomeSocket);

  return 0;
}
