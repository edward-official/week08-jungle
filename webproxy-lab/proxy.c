#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "csapp.h"
#include "proxy-help.h"
#include "event-log/event-log.h"

static void processTransaction(int connectfd);
static int parseRequestLine(rio_t *clientBuffer, char *method, char *uri, char *version, char *proxyBuffer);
static void parseURI(const char *uri, char *hostname, char *port, char *path);
static void appendToBuffer(char *buffer, size_t *offset, size_t maxlen, const char *str);
static void buildHeaderBuffer(rio_t *clientBuffer, const char *hostname, char *headerBuffer);
static void deliverResponse(rio_t *serverBuffer, int clientfd);

int main(int argc, char **argv) {
  if(argc != 2) {
    writeEvent("Invalid number of arguments: expected 2 arguments(name, port number).");
    exit(1);
  }
  Signal(SIGPIPE, SIG_IGN);

  int listenfd, originfd;
  struct sockaddr_storage clientAddress;
  socklen_t sizeOfClientAddress;
  listenfd = Open_listenfd(argv[1]);
  while(True) {
    sizeOfClientAddress = sizeof(clientAddress);
    originfd = Accept(listenfd, (SA *)&clientAddress, &sizeOfClientAddress);
    processTransaction(originfd);
    Close(originfd);
  }
  return 0;
}

static void processTransaction(int originfd) {
  rio_t clientBuffer, serverBuffer; /* Internal Buffer */
  char proxyBuffer[MAXLINE]; /* User Buffer */
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE]; /* Components Of Request Line */
  char hostname[MAXLINE], port[16], path[MAXLINE]; /* Components Of URI */
  
  /* Read Client Request */
  Rio_readinitb(&clientBuffer, originfd);
  if(parseRequestLine(&clientBuffer, method, uri, version, proxyBuffer) < 0) return; /* Parse method, uri, version */
  parseURI(uri, hostname, port, path); /* Parse hostname, port, path */
  
  /* Send Request To The Destination Server */
  int destinationfd = Open_clientfd(hostname, port); /* Open the client socket connecting to the destination server */
  if(destinationfd < 0) {
    writeEvent("Failed to connect to server.");
    return;
  }
  Rio_readinitb(&serverBuffer, destinationfd); /* Setting up the internal buffer to read data from socket */
  char requestLine[MAXLINE], headerBuffer[MAXBUF];
  sprintf(requestLine, "GET %s HTTP/1.0\r\n", path); /* Build request line */
  buildHeaderBuffer(&clientBuffer, hostname, headerBuffer); /* Build header line */
  Rio_writen(destinationfd, requestLine, strlen(requestLine)); /* Write the request line on the socket */
  Rio_writen(destinationfd, headerBuffer, strlen(headerBuffer)); /* Write the header line on the socket */

  /* Send Response Back To Client */
  deliverResponse(&serverBuffer, originfd);
  Close(destinationfd);
}
static int parseRequestLine(rio_t *clientBuffer, char *method, char *uri, char *version, char *proxyBuffer) {
  if(Rio_readlineb(clientBuffer, proxyBuffer, MAXLINE) <= 0) return -1;
  if(sscanf(proxyBuffer, "%s %s %s", method, uri, version) != 3) return -1;
  if(strcasecmp(method, "GET")) return -1;
  return 0;
}
static void parseURI(const char *uri, char *hostname, char *port, char *path) {
  const char *pHost, *pLeftOfPort, *pPath;
  int tLength;

  /* Parse The Host */
  if(!strncasecmp(uri, "http://", 7)) pHost = uri + 7; /* Locate the starting point of host */
  else pHost = uri; /* Locate the starting point of host */
  pLeftOfPort = strpbrk(pHost, " :/\r\n"); /* Locate the left adjacent point of port*/
  if(pLeftOfPort == NULL) pLeftOfPort = pHost + strlen(pHost); /* Locate the left adjacent point of port*/
  tLength = pLeftOfPort - pHost;
  strncpy(hostname, pHost, tLength); /* Assign value to hostname */
  hostname[tLength] = '\0';
  
  /* Parse The Port */
  if(*pLeftOfPort == ':') {
    const char *pPort = pLeftOfPort + 1;
    pPath = strchr(pPort, '/'); /* Locate the starting point of path */
    if(pPath == NULL) pPath = pPort + strlen(pPort); /* Locate the starting point of path */
    tLength = pPath - pPort;
    strncpy(port, pPort, tLength);
    port[tLength] = '\0';
  }
  else {
    strcpy(port, "80");
    pPath = pLeftOfPort;
  }

  /* Parse The Path */
  if(*pPath == '\0') strcpy(path, "/");
  else strcpy(path, pPath);
}
static void appendToBuffer(char *buffer, size_t *offset, size_t capacity, const char *append) {
  if(*offset >= capacity - 1) return;
  int nWritten = snprintf(buffer + *offset, capacity - *offset, "%s", append); /* Safe method to write on buffer */
  if(nWritten < 0) return; /* Error from "snprintf" method */

  /* Update Offset */
  if((size_t)nWritten >= capacity - *offset) *offset = capacity - 1; /* Overflow: Safety bar for the next "appendToBuffer" method call */
  else *offset += (size_t)nWritten; /* Completed without overflow */
}
static void buildHeaderBuffer(rio_t *clientBuffer, const char *hostname, char *headerBuffer) {
  char proxyBuffer[MAXLINE];
  char hostHeader[MAXLINE];
  int hasHostHeader = 0;
  ssize_t n;
  size_t offset = 0;
  headerBuffer[0] = '\0';

  /* When Header Received */
  while((n = Rio_readlineb(clientBuffer, proxyBuffer, MAXLINE)) > 0) {
    if(!strcmp(proxyBuffer, "\r\n")) break;
    if(!strncasecmp(proxyBuffer, "Host:", 5)) {
      hasHostHeader = 1;
      appendToBuffer(headerBuffer, &offset, MAXBUF, proxyBuffer);
    } else if(!strncasecmp(proxyBuffer, "User-Agent:", 11)) {
    } else if(!strncasecmp(proxyBuffer, "Connection:", 11)) {
    } else if(!strncasecmp(proxyBuffer, "Proxy-Connection:", 17)) {
    } else {
      appendToBuffer(headerBuffer, &offset, MAXBUF, proxyBuffer);
    }
    if(offset >= MAXBUF - 1) break;
  }

  /* When No Header Received */
  if(!hasHostHeader) {
    snprintf(hostHeader, sizeof(hostHeader), "Host: %s\r\n", hostname);
    appendToBuffer(headerBuffer, &offset, MAXBUF, hostHeader);
  }
  appendToBuffer(headerBuffer, &offset, MAXBUF, user_agent_hdr);
  appendToBuffer(headerBuffer, &offset, MAXBUF, "Connection: close\r\n");
  appendToBuffer(headerBuffer, &offset, MAXBUF, "Proxy-Connection: close\r\n");
  appendToBuffer(headerBuffer, &offset, MAXBUF, "\r\n");

  if(offset >= MAXBUF) headerBuffer[MAXBUF - 1] = '\0';
  else headerBuffer[offset] = '\0';
}
static void deliverResponse(rio_t *serverBuffer, int originfd) {
  char proxyBuffer[MAXBUF];
  ssize_t n;
  while((n = Rio_readnb(serverBuffer, proxyBuffer, MAXBUF)) > 0) {
    Rio_writen(originfd, proxyBuffer, n);
  }
}
