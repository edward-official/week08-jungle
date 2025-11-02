/*
💻 빌드 명령어
cd echo-server; gcc -o echo-server echo-server.c ../webproxy-lab/csapp.c -Wall -Wextra -pthread;
./echo-server 8080
telnet 127.0.0.1 8080
*/
#include "../webproxy-lab/csapp.h"

void echo(int connectFd) {
  ssize_t nBytes;
  char buffer[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, connectFd);
  while ((nBytes = Rio_readlineb(&rio, buffer, MAXLINE)) != 0) {
    printf("server received %zd bytes\n", nBytes);
    Rio_writen(connectFd, buffer, (size_t)nBytes);
  }
}

int main(int argc, char **argv) {
  /*
  argv[0] == program name
  argv[1] == port

  [Logical Flow]
  1. bind + listen == Open_listenFd()
  2. loop: accept the connect request and echo
  */

  /* Validate number of arguments */
  if(argc != 2) return -1;
  
  /* Open socket for listening */
  int listenFd, connectFd;
  listenFd = Open_listenfd(argv[1]);

  struct sockaddr_storage clientAddress;
  socklen_t lengthOfAddressStruct;
  while(1) {
    lengthOfAddressStruct = sizeof(clientAddress);
    connectFd = Accept(listenFd, (SA *)&clientAddress, &lengthOfAddressStruct);
    echo(connectFd);
    Close(connectFd);
  }

  return 0;
}
