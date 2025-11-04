#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "csapp.h"
#include "proxy-help.h"
#include "event-log/event-log.h"

static void processTransaction(int connectfd);
static void parseURI(const char *uri, char *hostname, char *path, char *port);
static int parseRequestLine(rio_t *clientBuffer, char *method, char *uri, char *version, char *proxyBuffer);
static void buildHeaderBuffer(rio_t *clientBuffer, const char *hostname, char *headerBuffer);
static void deliverResponse(rio_t *serverBuffer, int clientfd);
static void appendToBuffer(char *buffer, size_t *offset, size_t maxlen, const char *str);

int main(int argc, char **argv) {
  if(argc != 2) {
    writeEvent("Invalid number of arguments: expected 2 arguments(name, port number).");
    exit(1);
  }

  Signal(SIGPIPE, SIG_IGN);

  int listenfd, connectfd;
  struct sockaddr_storage clientAddress;
  socklen_t sizeOfClientAddress;

  listenfd = Open_listenfd(argv[1]);

  while(True) {
    sizeOfClientAddress = sizeof(clientAddress);
    connectfd = Accept(listenfd, (SA *)&clientAddress, &sizeOfClientAddress);
    processTransaction(connectfd);
    Close(connectfd);
  }

  return 0;
}

static void processTransaction(int connectfd) {
  rio_t clientBuffer, serverBuffer;
  char proxyBuffer[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE], port[16];
  int serverfd;

  Rio_readinitb(&clientBuffer, connectfd);

  if(parseRequestLine(&clientBuffer, method, uri, version, proxyBuffer) < 0) {
    return;
  }

  parseURI(uri, hostname, path, port);

  serverfd = Open_clientfd(hostname, port);
  if(serverfd < 0) {
    writeEvent("Failed to connect to server.");
    return;
  }

  Rio_readinitb(&serverBuffer, serverfd);

  char requestLine[MAXLINE];
  sprintf(requestLine, "GET %s HTTP/1.0\r\n", path);

  char headerBuffer[MAXBUF];
  buildHeaderBuffer(&clientBuffer, hostname, headerBuffer);

  Rio_writen(serverfd, requestLine, strlen(requestLine));
  Rio_writen(serverfd, headerBuffer, strlen(headerBuffer));

  deliverResponse(&serverBuffer, connectfd);

  Close(serverfd);
}

static int parseRequestLine(rio_t *clientBuffer, char *method, char *uri, char *version, char *proxyBuffer) {
  if(Rio_readlineb(clientBuffer, proxyBuffer, MAXLINE) <= 0) {
    return -1;
  }
  if(sscanf(proxyBuffer, "%s %s %s", method, uri, version) != 3) {
    return -1;
  }
  if(strcasecmp(method, "GET")) {
    return -1;
  }
  return 0;
}

static void buildHeaderBuffer(rio_t *clientBuffer, const char *hostname, char *headerBuffer) {
  char proxyBuffer[MAXLINE];
  char hostHeader[MAXLINE];
  int hasHostHeader = 0;
  ssize_t n;

  size_t offset = 0;
  headerBuffer[0] = '\0';

  while((n = Rio_readlineb(clientBuffer, proxyBuffer, MAXLINE)) > 0) {
    if(!strcmp(proxyBuffer, "\r\n")) {
      break;
    }
    if(!strncasecmp(proxyBuffer, "Host:", 5)) {
      hasHostHeader = 1;
      appendToBuffer(headerBuffer, &offset, MAXBUF, proxyBuffer);
    } else if(!strncasecmp(proxyBuffer, "User-Agent:", 11)) {
    } else if(!strncasecmp(proxyBuffer, "Connection:", 11)) {
    } else if(!strncasecmp(proxyBuffer, "Proxy-Connection:", 17)) {
    } else {
      appendToBuffer(headerBuffer, &offset, MAXBUF, proxyBuffer);
    }
    if(offset >= MAXBUF - 1) {
      break;
    }
  }

  if(!hasHostHeader) {
    snprintf(hostHeader, sizeof(hostHeader), "Host: %s\r\n", hostname);
    appendToBuffer(headerBuffer, &offset, MAXBUF, hostHeader);
  }

  appendToBuffer(headerBuffer, &offset, MAXBUF, user_agent_hdr);
  appendToBuffer(headerBuffer, &offset, MAXBUF, "Connection: close\r\n");
  appendToBuffer(headerBuffer, &offset, MAXBUF, "Proxy-Connection: close\r\n");
  appendToBuffer(headerBuffer, &offset, MAXBUF, "\r\n");

  if(offset >= MAXBUF) {
    headerBuffer[MAXBUF - 1] = '\0';
  } else {
    headerBuffer[offset] = '\0';
  }
}

static void deliverResponse(rio_t *serverBuffer, int clientfd) {
  char proxyBuffer[MAXBUF];
  ssize_t n;

  while((n = Rio_readnb(serverBuffer, proxyBuffer, MAXBUF)) > 0) {
    Rio_writen(clientfd, proxyBuffer, n);
  }
}

static void parseURI(const char *uri, char *hostname, char *path, char *port) {
  const char *hostbegin;
  const char *hostend;
  const char *pathbegin;
  int len;

  if(!strncasecmp(uri, "http://", 7)) {
    hostbegin = uri + 7;
  } else {
    hostbegin = uri;
  }

  hostend = strpbrk(hostbegin, " :/\r\n");
  if(hostend == NULL) {
    hostend = hostbegin + strlen(hostbegin);
  }

  len = hostend - hostbegin;
  strncpy(hostname, hostbegin, len);
  hostname[len] = '\0';

  if(*hostend == ':') {
    const char *portbegin = hostend + 1;
    pathbegin = strchr(portbegin, '/');
    if(pathbegin == NULL) {
      pathbegin = portbegin + strlen(portbegin);
    }
    len = pathbegin - portbegin;
    strncpy(port, portbegin, len);
    port[len] = '\0';
  } else {
    strcpy(port, "80");
    pathbegin = hostend;
  }

  if(*pathbegin == '\0') {
    strcpy(path, "/");
  } else {
    strcpy(path, pathbegin);
  }
}

static void appendToBuffer(char *buffer, size_t *offset, size_t maxlen, const char *str) {
  if(*offset >= maxlen - 1) {
    return;
  }
  int written = snprintf(buffer + *offset, maxlen - *offset, "%s", str);
  if(written < 0) {
    return;
  }
  if((size_t)written >= maxlen - *offset) {
    *offset = maxlen - 1;
  } else {
    *offset += (size_t)written;
  }
}
