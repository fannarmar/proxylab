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
int connectionClientServer(int clientfd, struct sockaddr_in *clientaddr);
int forwardHttpRequest(rio_t *rio_client, int serverfd, char method[], char serverPath[], char version[]);
int forwardResponseHeader(rio_t *rio_server, int clientfd, int *statusCode, int *contentLength, char *transferEncoding);
int forwardPayload(rio_t *rio_server, int clientfd, char transferEncoding[], int contentLength);
int parse_uri(char *uri, char *hostname, char *pathname, int *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
int Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);

void Logger(struct sockaddr_in *sockaddr,char *uri);
void *thread(void *varp);
int open_clientfd_ts(char *hostname, int port, int *privatep); 

sem_t mutex1,mutex2;
typedef struct{
	int clientfd;
	struct sockaddr_in clientaddr;
} client_struct;

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
	
	//argument input for pthread_create
	pthread_t tid;	
	client_struct *args;
	//initialize semaphores
	sem_init(&mutex1,0,1);   
	sem_init(&mutex2,0,1);
	
	/* Check arguments */
    if (argc != 2) 
	{
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
    }

	signal(SIGPIPE, SIG_IGN);

	port = atoi(argv[1]); // proxy port
	listenfd = Open_listenfd(port);

	// Start listener
	while (1)
	{
		args=(client_struct*)Malloc(sizeof(client_struct));
		clientfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
		
		args->clientfd = clientfd;
		args->clientaddr = clientaddr;
		
		if(pthread_create(&tid,NULL,thread,(void*) args)<0)
		printf("problem when creating thread \n");

		printf("\n");
		printf("------------ Start connection ------------ \n");

		/* Determine the domain name and IP address of the client */
		//hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
		//					sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		//haddrp = inet_ntoa(clientaddr.sin_addr);
		//printf("server connected to %s (%s)\n", hp->h_name, haddrp);	
		

		// Establish connection between client and server
	/*if(connectionClientServer(clientfd))
		{
			printf("Connection successful!\n");
		}
		else
			printf("Error - Connection unsuccessful\n");
	*/
		

		printf("------------ End connection ------------ \n");
		printf("\n");
	}	

    exit(0);
}

int connectionClientServer(int clientfd, struct sockaddr_in *clientaddr)
{
	int serverfd;
	rio_t rio_client, rio_server;
	char buf[MAXLINE], method[MAXLINE], URI[MAXLINE], version[MAXLINE];	
	char serverName[MAXLINE], serverPath[MAXLINE];
	int serverPort;
	char transferEncoding[32];
	int statusCode, contentLength;

	// Initialize client socket buffer.
	Rio_readinitb(&rio_client, clientfd);

	/*
 	 * 
 	 * Parse the URI from the request header
 	 *
 	 */ 
	printf("\n---------Debug: Read Header! \n\n");
	if ( (Rio_readlineb_w(&rio_client, buf, MAXLINE)) == 0)
		return 0;
	sscanf(buf, "%s %s %s", method, URI, version);
	
	// Check for merhod implementation	
	if (strcasecmp(method, "GET") != 0) {
		printf("Method %s is not implemented \n", method);
		return 0;
	}
	
	P(&mutex1);
	Logger(clientaddr, URI);
	V(&mutex1);

	// Get the server info from the URI
	if (parse_uri(URI, serverName, serverPath, &serverPort) == -1){	
		printf("Unable to parse the URI!\n");
		return 0;
	}

	/*
 	 *
 	 * Connect to web server
 	 *
 	 */ 	
	printf("\n---------Debug: Establish connection! \n\n");

	// Print server info
	
	printf("Server name: %s \n", serverName);
	printf("Server path: %s \n", serverPath);
	printf("Server port: %d \n", serverPort);
	printf("\n");	
	
	int privatep;
	// Establish connection to server
	if ( (serverfd = open_clientfd_ts(serverName, serverPort, &privatep)) < 0){
		printf("Unable to connect to web server! \n");
		Close(serverfd);
		return 0;
	}

	/*
 	 *
 	 * Forward http request from client to server
 	 *
 	 */
	printf("\n---------Debug: Send http request! \n\n");
	if (!forwardHttpRequest(&rio_client, serverfd, serverPath, method, version)){
		Close(serverfd);
		return 0;
	}


	/*
 	 *
 	 * Forward response header from server to client
 	 * Parse 
 	 * 		- Status Code
 	 * 		- Content Length (if any)
 	 * 		- Transfer Encoding (if any)
 	 *
 	 */ 
	printf("\n---------Debug: Receive response header! \n\n");
	Rio_readinitb(&rio_server, serverfd);
	if (!forwardResponseHeader(&rio_server, clientfd, &statusCode, &contentLength, transferEncoding)){
		Close(serverfd);
		return 0;
	}


	/*
	 *
 	 * Forward payload from server to client
 	 * Only if 200 OK
 	 */ 
	printf("\n---------Debug: Receive content! \n\n");
	if (statusCode == 200){
		forwardPayload(&rio_server, clientfd, transferEncoding, contentLength);
	}
	Close(serverfd);

	return 1;
}

/*
 * forwardHttpRequest
 *
 * Forwards http request from client to server
 *
 */
int forwardHttpRequest(rio_t *rio_client, int serverfd, char serverPath[], char method[], char version[]){
	char buf[MAXLINE];
	int n;

	// Restructure the http method line
	sprintf(buf, "GET /%s HTTP/1.1\r\n", serverPath);
	n = strlen(buf);
	
	// Forward the request to web server
	while ( strcmp(buf, "\r\n") != 0 ){
		Rio_writen_w(serverfd,buf,n);
		Fputs(buf, stdout);

		if ( (n = Rio_readlineb_w(rio_client, buf, MAXLINE)) == 0)
			return 0;

		if ( !strncmp(buf, "Connection", 10) ){
			sprintf(buf, "Connection: close\r\n");
			n = strlen(buf);
		}
		else if ( !strncmp(buf, "Proxy-Connection", 16) ){
			sprintf(buf, "Proxy-Connection: close\r\n");
			n = strlen(buf);
		}
			
	}		
	
	// Send the final http request line ("\r\n")
	Rio_writen_w(serverfd,buf,n);
	Fputs(buf, stdout);

	return 1;
}

/*
 * forwardResponseHeader
 *
 * Forwards the response header from server to client
 * Parses
 * 		- Status Code
 * 		- Content Length (if any)
 * 		- Transfer Encoding (if any)
 */
int forwardResponseHeader(rio_t *rio_server, int clientfd, int *statusCode, int *contentLength, char *transferEncoding){
	char buf[MAXLINE];
	int n;

	strcpy(buf, "\0"); // Initialize buffer

	while ( strcmp(buf, "\r\n") != 0)	{	
		if ( (n = Rio_readlineb_w(rio_server, buf, MAXLINE)) == 0)
			return 1;
		Rio_writen_w(clientfd, buf, n);
		Fputs(buf, stdout);

		// Parse status code, content length and transfer encoding
		sscanf(buf, "HTTP/1.1 %d", statusCode);
		sscanf(buf, "Content-Length: %d", contentLength);
		sscanf(buf, "Transfer-Encoding: %s", transferEncoding);
	}
	
	return 1;
}

/*
 * forwardPayload
 *
 * Forwards the payload from the server to the client
 *
 */
int forwardPayload(rio_t *rio_server, int clientfd, char transferEncoding[], int contentLength){
	char buf[MAXLINE];
	int n;
	int chunksize = 0;	

	if (contentLength > 0 && strcmp(transferEncoding, "chunked") != 0){
		// Only use content length if transfer encoding is not defined as chunks and
		// content length is defined.
		while ( (n = Rio_readnb_w(rio_server, buf, MIN(MAXLINE, contentLength))) > 0 ){
			Rio_writen_w(clientfd, buf, n);
			contentLength -= n;
		}
	}
	else {
		// BUG - Bottle neck
		// If any additional data is requested with readnb than there is on the socket
		// buffer, the function stops and waits for the data.
		// The process therefore stops!
		//
		while ( (n = Rio_readlineb_w(rio_server, buf, MAXLINE)) > 0 ){
			Rio_writen_w(clientfd, buf, n);
			sscanf(buf, "%x", &chunksize);
			printf("chunksize: %d\n", chunksize);
			if (!chunksize)
				break; 
    		n = Rio_readnb_w(rio_server, buf, MIN(MAXLINE, chunksize));
			//chunksize -= n;
			//sscanf(buf, "%x", &chunksize);
			Rio_writen_w(clientfd, buf, n);
			chunksize = 0;

			n = Rio_readlineb_w(rio_server, buf, MAXLINE);
			Rio_writen_w(clientfd, buf, n);
		}
		Rio_writen_w(clientfd, buf, n);
	}
	return 1;
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
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d \n", time_str, a, b, c, d, uri, size);
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
void Logger(struct sockaddr_in *sockaddr,char *uri)
{	
	char my_string[MAXLINE];
	int size=10;
	format_log_entry(my_string, sockaddr,uri, size);
	FILE *fp;

	fp = fopen("proxy.log","a");
	if(fp ==NULL) {
		printf("I could not open proxy.log for writing. \n");
	}
	else
		fprintf(fp, my_string);
	//free(my_string);
	fclose(fp);
}

void *thread(void *vargp)
{
	client_struct *args =vargp; 
	pthread_detach(pthread_self());
	
	int clientfd = args->clientfd;
	struct sockaddr_in clientaddr = args->clientaddr;
	
	// Establish connection between client and server
	if (!connectionClientServer(clientfd, &clientaddr))
	{
		printf("Error - Connection unsuccessful\n");
	}
	else
		printf("Connection successful\n");
	
	Free(vargp);
	//int myid = (int)vargp;
	//static int cnt = 0;
	//printf(" [%d] : (cnt=%d)\n", myid, ++cnt);
	Close(clientfd);
	return NULL;
}  
int open_clientfd_ts(char *hostname, int port, int *privatep)
{
	int sharedp;

	P(&mutex2);
	sharedp = Open_clientfd(hostname, port);
	*privatep = sharedp;
	V(&mutex2);
	return *privatep;
}

