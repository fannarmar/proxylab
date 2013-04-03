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
#include "sys/param.h"



/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void echo(int connfd);
char *getURIFromRequest(int connfd);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
int Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
int invalidURI(char URI[]);
int connectionClientServer(int clientfd);

void sigpipehandler(int sig)
{
	/* DO NOTHING */
}



/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
	int listenfd, clientfd, port;
	socklen_t clientlen = sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
	//struct hostent *hp;
	//char *haddrp;

	/* Check arguments */
    if (argc != 2) 
	{
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
    }

	signal(SIGPIPE, sigpipehandler);

	port = atoi(argv[1]); // proxy port
	listenfd = Open_listenfd(port);

	// Start listener
	while (1)
	{

		clientfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
		

		printf("\n");
		printf("------------ Start connection ------------ \n");

		/* Determine the domain name and IP address of the client */
		//hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
		//					sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		//haddrp = inet_ntoa(clientaddr.sin_addr);
		//printf("server connected to %s (%s)\n", hp->h_name, haddrp);	
		

		// Establish connection between client and server
		if (connectionClientServer(clientfd))
		{
			printf("Connection successful!\n");
		}
		else
			printf("Error - Connection unsuccessful\n");

		Close(clientfd);

		printf("------------ End connection ------------ \n");
		printf("\n");
	}	

    exit(0);
}

int connectionClientServer(int clientfd)
{
	int n, serverfd;
	rio_t rio_client, rio_server;
	char buf[MAXLINE];	

	char URI[MAXLINE];
	int get_post = 0;

	int server_status = -1;	
	
	char serverName[MAXLINE];
	char serverPathName[MAXLINE];
	int serverPort[1];

	int contentLength = -1;
	int encodingChunked = 0;

	// Initialize client socket buffer.
	Rio_readinitb(&rio_client, clientfd);


	printf("\n---------Debug: Read Header! \n\n");

	
	// Read the URI from the http request.
	if (( n = Rio_readlineb_w(&rio_client, buf, MAXLINE)) == 0)
	{
		printf("Failed to receive GET header from client\n");
		return 0;
	}
	sscanf(buf, "GET %s", (char *)URI);

	// Get the server info from the URI
	if (parse_uri(URI, serverName, serverPathName, serverPort) == -1)
	{	
		printf("Unable to parse the URI!\n");
		return 0;
	}

	// Print server info
	printf("Server name: %s \n", serverName);
	printf("Server path: %s \n", serverPathName);
	printf("Server port: %d \n", serverPort[0]);
	printf("\n");	


	printf("\n---------Debug: Establish connection! \n\n");

	
	// Establish connection to server
	if ( (serverfd = Open_clientfd(serverName, serverPort[0])) < 0)
	{
		printf("Unable to connect to web server! \n");
		Close(serverfd);
		return 0;
	}
	Rio_readinitb(&rio_server, serverfd);


	printf("\n---------Debug: Send http request! \n\n");

		
	// Send http request to web server
	while (strcmp(buf, "\r\n") != 0)
	{
		// Modify the GET 
		if (!strncmp(buf, "GET", 3))
		{
			sprintf(buf, "GET /%s HTTP/1.1\r\n", serverPathName);
			get_post = 1;
			n = strlen(buf);
		}
		else if (!strncmp(buf, "POST", 4))
		{
			sprintf(buf, "POST /%s HTTP/1.1\r\n", serverPathName);
			get_post = 1;
			n = strlen(buf);
		}

		Rio_writen_w(serverfd, buf, n);
		Fputs(buf, stdout);
		n = Rio_readlineb_w(&rio_client, buf, MAXLINE);	
	}

	// Send the final request header
	Rio_writen_w(serverfd, buf, n);
	Fputs(buf, stdout);
	



	printf("\n---------Debug: Receive response header! \n\n");

	


	// Receive server response header
	strcpy(buf, "\0");
	while ( strcmp(buf, "\r\n") != 0 && n > 0 )
	{	
		n = Rio_readlineb_w(&rio_server, buf, MAXLINE);
		Rio_writen_w(clientfd, buf, n);
		Fputs(buf, stdout);

		// Parse the server status code
		sscanf(buf, "HTTP/1.1 %d", &server_status);


		// Parse Transfer Encoding
		if (!strncmp(buf, "Transfer-Encoding: chunked", 26))
		{
			encodingChunked = 1;
		}

		// Parse Content Length
		sscanf(buf, "Content-Length: %d", &contentLength);
	}




	printf("\n---------Debug: Receive content! \n\n");

	

	// Receive server content
	if (server_status == 200)
	{
		printf("200 OK! \n");
		
		if (!encodingChunked && contentLength != -1)
		{
			while ( (n = Rio_readnb_w(&rio_server, buf, MIN(MAXLINE, contentLength))) != 0 )
			{
				printf("Content remaining: %d \n", MIN(MAXLINE, contentLength));
				printf("n: %d \n", n);
				Rio_writen_w(clientfd, buf, n);

				contentLength -= n;
			}
		}
		else
		{
			while ( (n = Rio_readnb_w(&rio_server, buf, 64)) == 64 )
			{
				//printf("Bytes read: %d \n", n);
				Rio_writen_w(clientfd,buf,n);
			}
			printf("Bytes read: %d \n", n);
			Rio_writen_w(clientfd,buf,n);
			
		}
	}

	printf("Done reading yo\n");

	Close(serverfd);

	return 1;
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
		Rio_writen_w(connfd, buf, n);
	}
}


/*
 * invalidURI - Returns 1 if the URI starts with http://www.
 */
int invalidURI(char URI[])
{
	if (!strncmp("http://www.", URI, 11))
		return 1;
	
	printf("Error: Invalid URI\n");
	return 0;
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
       
    /* Extract the host name */
    hostbegin = uri + 7;
	printf("Hostbegin: %s \n", (char *)hostbegin);
    hostend = strpbrk(hostbegin, " :/\r\n\0");
	printf("Hostend: %s \n", (char *)hostend);
    len = hostend - hostbegin;
	printf("Len: %d \n",len);

    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
	*port = atoi(hostend + 1);
     
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

ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n_size)
{
	ssize_t rc;

	if ( (rc = rio_readnb(rp, usrbuf, n_size)) < 0)
	{
		printf("Rio_readnb_w error!\n");
		return 0;
	}

	return rc;
}


