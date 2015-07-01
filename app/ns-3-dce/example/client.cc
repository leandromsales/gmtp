#include "stdio.h"    
#include "stdlib.h"    
#include "sys/types.h"    
#include "sys/socket.h"    
#include "string.h"    
#include "netinet/in.h"    
#include "netdb.h"  
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
    
#define PORT 2000   
#define BUF_SIZE 2000   
#define SOCK_GMTP     7
#define IPPROTO_GMTP  254
#define SOL_GMTP      281
    
int main(int argc, char**argv) {    
 struct sockaddr_in addr, cl_addr;    
 int sockfd, ret;    
 char buffer[BUF_SIZE];    
 struct hostent * server;  
 char * serverAddr;  
 int i = 50;
  
 if (argc < 2) {  
  printf("usage: client < ip address >\n");  
  exit(1);    
 }  
  
 serverAddr = argv[1];   
   
 sockfd = socket(AF_INET, SOCK_GMTP, IPPROTO_GMTP);    
 setsockopt(sockfd, SOL_GMTP, 1, "1234567812345678", 16);
 if (sockfd < 0) {    
  printf("Error creating socket!\n");    
  exit(1);    
 }    
 printf("Socket created...\n");     
  
 memset(&addr, 0, sizeof(addr));    
 addr.sin_family = AF_INET;    
 addr.sin_addr.s_addr = inet_addr(serverAddr);  
 addr.sin_port = PORT;       
  
 ret = connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));    
 if (ret < 0) {    
  printf("Error connecting to the server!\n");    
  exit(1);    
 }    
 printf("Connected to the server...\n");    
  
 // memset(buffer, 0, BUF_SIZE);  
 // printf("Enter your message(s): ");  
 strcpy(buffer, "MENSAGEM TESTE\n");
 sleep(3);
  
 // while (fgets(buffer, BUF_SIZE, stdin) != NULL) {  
 // while (fgets(buffer, BUF_SIZE, stdin) != NULL) {  
 while (i--) {  
  ret = sendto(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *) &addr, sizeof(addr));    
  if (ret < 0) {    
   printf("Error sending data!\n\t-%s", buffer);    
  }  
  ret = recvfrom(sockfd, buffer, BUF_SIZE, 0, NULL, NULL);    
  if (ret < 0) {    
   printf("Error receiving data!\n");      
  } else {  
   printf("Received: ");  
   fputs(buffer, stdout);  
   printf("\n");  
  }    
 }  
   
 return 0;      
}    

