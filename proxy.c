/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu 
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void echo(int connfd);
char *getURIFromRequest(int connfd);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
int Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readnb(rio_t *rp, void *usrbuf, size_t n);

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
	int listenfd, connfd, port;
	socklen_t clientlen = sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
	struct hostent *hp;
	char *haddrp;
	char *URI;

	char serverName[128];
	serverName[0] = '\0';
	char serverPathName[128];
	int serverPort[1];

    /* Check arguments */
    if (argc != 2) 
	{
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
    }

	port = atoi(argv[1]);
	listenfd = Open_listenfd(port);

	// Start listener
	while (1)
	{
		connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);

		/* Determine the domain name and IP address of the client */
		hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
							sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		haddrp = inet_ntoa(clientaddr.sin_addr);
		printf("server connected to %s (%s)\n", hp->h_name, haddrp);	
		
		// Get the URI from the HTTP request	
		URI = getURIFromRequest(connfd);	

		printf("URI: %s, %lu \n", (char *)URI, strlen(URI));					

		
		//serverName = malloc(sizeof(char) * 128);
		//serverPathName = malloc(sizeof(char) * 128);
		//serverPort = malloc(sizeof(int));
		
		if (parse_uri(URI, serverName, serverPathName, serverPort) == -1)
			printf("Error yo!\n");

		Close(connfd);
	}	

    exit(0);
}

/*
 * echo - ECHO
 * Function that reads and echoes text lines.
 */
void echo(int connfd)
{
	int n;
	char buf[MAXLINE];
	rio_t rio;

	Rio_readinitb(&rio, connfd);
	while (( n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
	{
		printf("%s",buf);
		Rio_writen(connfd, buf, n);
	}
}

/*
 * getURIFromRequest - Parses the URI from the HTTP Request.
 */
char *getURIFromRequest(int connfd)
{
	int n;
	char buf[MAXLINE];
	rio_t rio;
	char *host = malloc(sizeof(char) * 128);
	char *URI_short = malloc(sizeof(char) * 128);
	char *URI = malloc(sizeof(char) * 128);	

	Rio_readinitb(&rio, connfd);
	while (( n = Rio_readlineb_w(&rio, buf, MAXLINE)) != 0)
	{
		sscanf(buf, "%s %s", host, URI_short);

		if (!strcmp(host, "Host:"))
		{
			strcpy(URI, "http://");
			strcat(URI, URI_short);
			URI[strlen(URI)] = '\0';	
	
			free(host);
			free(URI_short);

			printf("Size %lu: \n", strlen(URI));
			return URI;
		}
	}
	
	return NULL;
}


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	hostname[0] = '\0';
	return -1;
    }
	printf("1\n");
       
    /* Extract the host name */
    hostbegin = uri + 7;
	printf("Hostbegin: %s \n", (char *)hostbegin);
    hostend = strpbrk(hostbegin, " :/\r\n\0");
	printf("Hostend: %s \n", (char *)hostend);
	printf("2\n");
    len = hostend - hostbegin;
	printf("Len: %d \n",len);
    strncpy(hostname, hostbegin, len);
	printf("2\n");
    hostname[len] = '\0';

	printf("1\n");

    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
	*port = atoi(hostend + 1);
    
	printf("1\n");
    
	/* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	pathname[0] = '\0';
    }
    else {
	pathbegin++;	
	strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}

/************************************
 * Wrappers for rpbust I/O routines
 ************************************/
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen)
{
	ssize_t rc;

	if (( rc = rio_readlineb(rp, usrbuf, maxlen )) < 0)
	{
		printf("Rio_readlineb_w error!\n");
		return 0; // Return EOF if error
	}

	return rc;
}

int Rio_writen_w(int fd, void *usrbuf, size_t n)
{
	if (rio_writen(fd, usrbuf, n) != n)
	{
		printf("Rio_writen_w error!\n");
		return 0;
	}

	return 1;
}

ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n)
{
	size_t rc;

	if ( (rc = rio_readnb(rp, usrbuf, n)) < 0)
	{
		printf("Rio_readnb_w error!\n");
		return 0;
	}

	return rc;
}


