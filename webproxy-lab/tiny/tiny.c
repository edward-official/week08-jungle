/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include "tiny-interface.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connectfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t sizeOfClientAddress;
  struct sockaddr_storage clientAddress;

  /* Check command line args */
  if(argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while(True) {
    /* Logical Flow
    - complete the connection
    - handle the transaction
    - close the connection
    */
    sizeOfClientAddress = sizeof(clientAddress); /* Why must it be inside the loop? */
    connectfd = Accept(listenfd, (SA *)&clientAddress, &sizeOfClientAddress); /* Accept the connection request */
    Getnameinfo((SA *)&clientAddress, sizeOfClientAddress, hostname, MAXLINE, port, MAXLINE, 0); /* Set up the hostname and port */
    printf("Accepted connection from (%s, %s)\n", hostname, port); /* Nothing logical, just logging */
    doit(connectfd); /* Handle one transaction */
    Close(connectfd); /* Close the connection request */
  }
}

void doit(int fd) {
  /* Logical Flow
  - read request line
  - locate the file requested
  - serve
  */
  int isRequestStatic;
  struct stat fileInformation; /* Information about the file */
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE); /* Read one request line */
  printf("Request headers: %s\n", buf);
  sscanf(buf, "%s %s %s", method, uri, version); /* Set up the variables */
  if(strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return; /* Return when the request isn't GET METHOD */
  }
  read_requesthdrs(&rio); /* Drain the buffer */

  isRequestStatic = parse_uri(uri, filename, cgiargs); /* Set up the file name and arguments */
  if(stat(filename, &fileInformation) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return; /* Return when cannot find the file */
  }

  if(isRequestStatic) {
    if(!(S_ISREG(fileInformation.st_mode)) || !(S_IRUSR & fileInformation.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, fileInformation.st_size);
  }
  else {
    if(!(S_ISREG(fileInformation.st_mode)) || !(S_IXUSR & fileInformation.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s</p>\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];
  /* Read the first line */ Rio_readlineb(rp, buf, MAXLINE);
  while(/* If the next line is not a blank, drain one more line */ strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("💻 Drain the buffer: %s", buf);
  }
  return;
}
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;
  if(/* CASE 1: content is not dynamic */ !strstr(uri, "cgi-bin")) {
    /* No arguments required */
    strcpy(cgiargs, "");
    
    /* Setting up the file name */
    strcpy(filename, ".");
    strcat(filename, uri);
    if(uri[strlen(uri) - 1] == '/') strcat(filename, "home.html"); /* When will this happen? */

    return CONTENT_IS_STATIC;
  }
  else /* CASE 2: content is dynamic */ {
    ptr = index(uri, '?');
    
    /* Setting up the arguments */ 
    if(ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else strcpy(cgiargs, "");
    
    /* Setting up the file name */
    strcpy(filename, ".");
    strcat(filename, uri);
    
    return CONTENT_IS_DYNAMIC;
  }
}
void serve_static(int fd, char *filename, int filesize) {
  /* Logical Flow
  - Open file
  - Memory mapping
  - Close file
  - Write on socket
  - Memory unmapping
  */
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers: %s\n", buf);

  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);
}
void get_filetype(char *filename, char *filetype) {
  if(strstr(filename, ".html")) strcpy(filetype, "text/html");
  else if(strstr(filename, ".gif")) strcpy(filetype, "image/gif");
  else if(strstr(filename, ".png")) strcpy(filetype, "image/png");
  else if(strstr(filename, ".jpg")) strcpy(filetype, "image/jpeg");
  else strcpy(filetype, "text/plain");
}
void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if(Fork() == 0) {
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO); /* Replace "STDOUT_FILENO" with "fd": "printf" prints directly to the client socket, not the terminal */
    Execve(filename, emptylist, environ); /* Execute the CGI program */
  }
  Wait(NULL);
}
