#include "csapp.h"

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void create_log(char *url, char *host, int size);
void doit(int fd, struct sockaddr_in clientaddr) 
{
    struct stat sbuf;
    char buf[MAXLINE], buf2[MAXLINE], buf3[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], host[MAXLINE], port[MAXLINE];
    char filename[MAXLINE], fullurl[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    int my_clientfd;
  
    /* read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) { 
       clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
        return;
    }
    Rio_readlineb(&rio, buf2, MAXLINE);
    strcpy(buf3, buf2);
    while(strcmp(buf2, "\r\n")){
        strcpy(buf3, buf2);
        Rio_readlineb(&rio, buf2, MAXLINE);
    }
    strcat(buf3, "\r\n\r\n");
    printf("Finished read_request\n");

    /* parse URI from GET request */
    if (parse_uri(uri, host, port, filename, cgiargs) == 0) { 
        clienterror(fd, method, "501", "Not Implemented",
                "Parsing the uri has failed");
        return;
    }

    if ((my_clientfd = open_clientfd(host, port)) < 0) {
        printf("process_request: Unable to connect to end server.\n");
        return;
    }

    Rio_writen(my_clientfd, "GET /", strlen("GET /"));
    Rio_writen(my_clientfd, filename, strlen(filename));
    Rio_writen(my_clientfd, " HTTP/1.1\r\n", strlen(" HTTP/1.0\r\n"));
    Rio_writen(my_clientfd, buf3, strlen(buf3));
    
    rio_readinitb(&rio, my_clientfd);

    int len, i;
    len = 0;
    i = 0;
    while ((i = Rio_readn(my_clientfd, buf, MAXLINE)) != 0) {
        len += i;
        Rio_writen(fd, buf, i);
    }
    strcat(fullurl, host);
    strcat(fullurl, "/");
    strcat(fullurl, filename);
    create_log(fullurl, host, len);
    strcpy(fullurl, "");
    strcpy(host, "");
    strcpy(port, "");
    printf("Finished!\n");
}

int main(int argc, char**argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr; /* Enough space for any address */
    char client_hostname[MAXLINE], client_port[MAXLINE];
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        doit(connfd, clientaddr);
        Close(connfd);
    }
}
int parse_uri (char *uri, char *hostname, char *port, char *filename, char *cgiargs)
{
    char *ptr;
    ptr = uri;
    strcpy(port, "80");
    if (!strstr(uri, "cgi-bin")){ /*Static content*/
        strcpy(cgiargs, "");
        if (strncasecmp(uri, "h", 1) != 0) {
            printf("Input needs to start with http://\n");
            return 0;
        }
        ptr += 7; //advance to the part after "http://"
        while (strncasecmp(ptr, "/", 1) != 0) {
            strncat(hostname, ptr, 1);
            ptr++; //advance to next character
            if (strncasecmp(ptr, ":", 1) == 0) {
                ptr++; //advance past the :
                strcpy(port, "");
                while (strncasecmp(ptr, "/", 1) != 0) {
                    strncat(port, ptr, 1);
                    ptr++;
                }
            }
        }
        ptr++;
        strcpy(filename, ptr);
        return 1;
    }
    return 0;
}

void clienterror(int fd, char *cause, char *errnum, 
         char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void create_log(char *url, char *host, int size)
{
    FILE* file_pointer;
    time_t t= time(NULL);
    struct tm *tm = localtime(&t);
    char s[64];
    char hostip[MAXLINE];

    struct hostent *hp = gethostbyname(host);
    struct in_addr addr;
    char **pp;
    for (pp = hp->h_aliases; *pp != NULL; pp++);
    for (pp = hp->h_addr_list; *pp != NULL; pp++) {
        addr.s_addr = ((struct in_addr *)*pp)->s_addr;
    }
    strcpy(hostip, inet_ntoa(addr));

    strftime(s, sizeof(s), "%c", tm);
    file_pointer = fopen("proxy.log", "a");
    fprintf(file_pointer, "%s PDT: % s %s %i\n", s, hostip, url, size);
    fclose(file_pointer);
}