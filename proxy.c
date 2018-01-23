#include "csapp.h"
#include "pthread.h"
#include "cache.h"
#include "aqueue.h"

void log_msg(char *msg);
void *write_logs(void *file);
void *handle_connections(void *something);
void handle_request(int fd);
void read_requesthdrs(rio_t * rp, char * req_host, char * user_agent);
int	parse_uri(char *uri, char *filename, char *cgiargs);
void serve_proxy_request(int fd, char * host, char * user_agent, char * uri);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void  clienterror(int fd, char *cause, char *errnum,
					char *shortmsg, char *longmsg);

char *SERVER_NAME = "localhost:";
int LEN_SERVER_NAME = 10;
int THREAD_COUNT = 8;

char server_host[MAXLINE];
a_queue *msg_queue; 
a_queue *cfd_queue;
cache *resource_cache;

sem_t msg_sem, log_sem, cfd_sem, conn_sem, req_log_sem, req_conn_sem, cache_sem;

int main(int argc, char **argv)
{
	int		listenfd  , connfd;
	char		hostname  [MAXLINE], port[MAXLINE];
	socklen_t	clientlen;
	struct sockaddr_storage clientaddr;
	
	char *log = (char *) malloc(MAXBUF);
	
	sem_init(&msg_sem, 0, 64);
	sem_init(&log_sem, 0, 0);
	sem_init(&cfd_sem, 0, 64);
	sem_init(&conn_sem, 0, 0);
	sem_init(&req_log_sem, 0, 1);
	sem_init(&req_conn_sem, 0, 1);
	sem_init(&cache_sem, 0, 1);
	
	msg_queue = init_queue(16);
	cfd_queue = init_queue(16); 

	/* Check command line args */
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <port> <thread count> <logging file>\n", argv[0]);
		exit(1);
	}

	resource_cache = init_cache();

	strcpy(server_host, SERVER_NAME);
	strcpy(server_host + LEN_SERVER_NAME, argv[1]);
	listenfd = Open_listenfd(argv[1]);
	
	int i;
	for (i = 0; i < THREAD_COUNT; i++)
	{
		pthread_t *thread = (pthread_t *) malloc(sizeof(pthread_t));
		pthread_create(thread, NULL, handle_connections, &connfd);
		pthread_detach(*thread);
	}
	
	pthread_t *thread = (pthread_t *) malloc(sizeof(pthread_t));
	FILE *fp = fopen("proxy.log", "w");
	fclose(fp);
	pthread_create(thread, NULL, write_logs, "proxy.log");
	
	while (1)
	{
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *) & clientaddr, &clientlen);
		Getnameinfo((SA *) & clientaddr, clientlen, hostname, MAXLINE,
				port, MAXLINE, 0);

		snprintf(log, MAXBUF, "Accepted connection from (%s, %s)\n", hostname, port);
		log_msg(log);
		
		sem_wait(&cfd_sem);
		push_queue(cfd_queue, (void *) &connfd);
		sem_post(&conn_sem);
	}
}

void log_msg(char *log)
{
	char *msg = (char *) malloc(MAXBUF);
	strcpy(msg, log);
	sem_wait(&msg_sem);
	sem_wait(&req_log_sem);
	//printf("%p : %s", (void *) msg, msg);
	int success = push_queue(msg_queue, (void *) msg);
	sem_post(&req_log_sem);
	if (success) sem_post(&log_sem);
}

void *write_logs(void *file)
{
	while (1)
	{
		sem_wait(&log_sem);
		char *msg = (char *) pop_queue(msg_queue);
		sem_post(&msg_sem);
		FILE *fp = fopen((char *) file, "a");
		if (fp != NULL)
		{
			fputs(msg, fp);
			fclose(fp);
		}
		free(msg);
	}
	return NULL;
}

void *handle_connections(void *param)
{
	while (1)
	{
		sem_wait(&conn_sem);
		sem_wait(&req_conn_sem);
		int* cfd_p = pop_queue(cfd_queue);
		sem_post(&req_conn_sem);
		sem_post(&cfd_sem);
		int cfd = *cfd_p;
		handle_request(cfd);
	}
	return NULL;
}

/*
 * handle_request - handle one HTTP request/response transaction
 */
/* $begin handle_request */
void handle_request(int fd)
{
	int		is_static;
	struct 	stat	sbuf;
	char	buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char	filename[MAXLINE], cgiargs[MAXLINE], host[MAXLINE], user_agent[MAXLINE];
	rio_t	rio;

	char *log = (char *) malloc(MAXBUF);

	/* Read request line and headers */
	Rio_readinitb(&rio, fd);
	if (!Rio_readlineb(&rio, buf, MAXLINE))
	{
		Close(fd);
		snprintf(log, MAXBUF, "Connection with %s closed\n\n", host);
		log_msg(log);
		return;
	}
	sscanf(buf, "%s %s %s", method, uri, version);
	if (strcasecmp(method, "GET"))
	{
		clienterror(fd, method, "501", "Not Implemented",
				"Proxy does not implement this method");
		Close(fd);
		snprintf(log, MAXBUF, "Connection with %s closed\n\n", host);
		log_msg(log);
		return;
	}	 	

	read_requesthdrs(&rio, host, user_agent);
	if (strcmp(host, server_host))
	{
		snprintf(log, MAXBUF, "Received proxy request for %s\n", host);
		log_msg(log);
		
		serve_proxy_request(fd, host, user_agent, uri);
		
		snprintf(log, MAXBUF, "Response sent to %s. Proxy content served\n", host);
		log_msg(log);
		Close(fd);
		snprintf(log, MAXBUF, "Connection with %s closed\n\n", host);
		log_msg(log);
		return;
	}


	/* Parse URI from GET request */
	is_static = parse_uri(uri, filename, cgiargs);
	if (stat(filename, &sbuf) < 0)
	{
		clienterror(fd, filename, "404", "Not found",
				"Proxy couldn't find this file");
		Close(fd);
		snprintf(log, MAXBUF, "Connection with %s closed\n\n", host);
		log_msg(log);
		return;
	} 
	if (is_static)
	{	
		/* Serve static content */
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
		{
			clienterror(fd, filename, "403", "Forbidden",
					"Proxy couldn't read the file");
			Close(fd);
			snprintf(log, MAXBUF, "Connection with %s closed\n\n", host);
			log_msg(log);
			return;
		}
	}
	else
	{			/* Serve dynamic content */
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
		{
			clienterror(fd, filename, "403", "Forbidden",
					"Proxy couldn't run the CGI program");
			Close(fd);
			snprintf(log, MAXBUF, "Connection with %s closed\n\n", host);
			log_msg(log);
			return;
		}
		serve_dynamic(fd, filename, cgiargs);
	}
	snprintf(log, MAXBUF, "Response sent to %s\n", host);
	log_msg(log);
	Close(fd);
	snprintf(log, MAXBUF, "Connection with %s closed\n\n", host);
	log_msg(log);
	return;
}
/* $end handle_request */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t * rp, char * req_host, char * user_agent)
{
	char buf[MAXLINE], header_name[MAXLINE], header_data[MAXLINE];

	Rio_readlineb(rp, buf, MAXLINE);
	sscanf(buf, "%s %s", header_name, header_data);
	if (!strcmp(header_name, "Host:"))
	{
		strcpy(req_host, header_data);
	}
	if (!strcmp(header_name, "User-Agent:"))
	{
		strcpy(user_agent, header_data);
	}
	while (strcmp(buf, "\r\n"))
	{
		Rio_readlineb(rp, buf, MAXLINE);
		sscanf(buf, "%s %s", header_name, header_data);
		if (!strcmp(header_name, "Host:"))
		{
			strcpy(header_name, header_data);
		}
		if (!strcmp(header_name, "User-Agent:"))
		{
			strcpy(user_agent, header_data);
		}
	}
	return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args return 0 if dynamic
 * content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
	char		   *ptr;

	if (!strstr(uri, "cgi-bin"))
	{			/* Static content */
		strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		if (uri[strlen(uri) - 1] == '/')
		{
			strcat(filename, "home.html");
		}
		return 1;
	} else
	{			/* Dynamic content */
		ptr = index(uri, '?');
		if (ptr)
		{
			strcpy(cgiargs, ptr + 1);
			*ptr = '\0';
		}
		else
		{
			strcpy(cgiargs, "");
		}
		strcpy(filename, ".");
		strcat(filename, uri);
		return 0;
	}
}
/* $end parse_uri */

void serve_proxy_request(int fd, char * host, char * user_agent, char * uri)
{

	char proxy_address[MAXLINE], resource[MAXLINE], *proxy_port;

	int address_len = 0;
	while (host[address_len] && host[address_len] != ':')
	{
		address_len++;
	}
	int resource_offset = address_len + 7;
	if (address_len == strlen(host))
	{
		proxy_port = "80";
	}
	else
	{
		proxy_port = (char *) malloc(sizeof(char));
		strcpy(proxy_port, host + address_len + 1);
		resource_offset += strlen(proxy_port) + 1;
	}
	memcpy(proxy_address, host, address_len);

	if (uri[7] == '/')
	{
		resource_offset++;
	}
	strcpy(resource, uri + resource_offset);
	
	cache_object *cobject;
	sem_wait(&cache_sem);
	cobject = read_cache(resource_cache, resource);
	sem_post(&cache_sem);

	char *log = (char *) malloc(MAXBUF);
	
	if (cobject != NULL)
	{
		snprintf(log, MAXBUF, "Cached proxy resource %s", resource);
		Rio_writen(fd, cobject->bytes, cobject->size);
	}

	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (!strcmp(proxy_address, "localhost"))
	{
		getaddrinfo("127.0.0.1", proxy_port, &hints, &res);
	}
	else
	{
		getaddrinfo(proxy_address, proxy_port, &hints, &res);
	}
	int cfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

	if (connect(cfd, res->ai_addr, res->ai_addrlen) < 0)
	{
		clienterror(fd, host, "502", "Bad Gateway", "Proxy server could not connect to destination");
		Close(fd);
		pthread_exit(NULL);
	}
	
	char req[MAXBUF];
	sprintf(req, "GET %s HTTP/1.1\n\rHost: %s\n\rUser-Agent: %s\n\rConnection: close\n\rProxy-Connection: close\n\r\n\r", 
			resource, host, user_agent);
	
	snprintf(log, MAXBUF, "Forwarding request to %s\n", host);
	log_msg(log);

	int bytes_sent = 0, req_len = strlen(req);
	while (bytes_sent < req_len)
	{
		int prev_bytes = bytes_sent;
		bytes_sent += send(cfd, req, req_len, MSG_NOSIGNAL);
		if (bytes_sent < prev_bytes)
		{
			clienterror(fd, host, "502", "Bad Gateway", "Proxy server could not connect to destination");
			Close(fd);
			pthread_exit(NULL);
		}
	}
	
	unsigned char *cache_res = (unsigned char *) malloc(MAX_OBJECT_SIZE);
	unsigned char buf[MAXBUF];
	int byte_count = 0, bytes_recvd = 1;
	while (bytes_recvd > 0)
	{
		bytes_recvd = recv(cfd, buf, MAXBUF, 0);
		Rio_writen(fd, buf, bytes_recvd);
		if (byte_count + bytes_recvd <= MAX_OBJECT_SIZE)
		{
			memcpy(cache_res + byte_count, buf, bytes_recvd);
		}
		byte_count += bytes_recvd;
		if (!bytes_recvd && byte_count)
		{
			snprintf(log, MAXBUF, "Response received from %s\n", host);
			log_msg(log);
		}
	}
	if (byte_count <= MAX_OBJECT_SIZE)
	{
		sem_wait(&cache_sem);
		cache_bytes(resource_cache, cache_res, resource, byte_count);
		sem_post(&cache_sem);
	}
	close(cfd);
}

/*
 * serve_static - copy a file back to the client
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
	int		srcfd;
	char	*srcp, filetype[MAXLINE], buf[MAXBUF];

	/* Send response headers to client */
	get_filetype(filename, filetype);
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%sServer: Proxy Web Server\r\n", buf);
	sprintf(buf, "%sConnection: close\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	Rio_writen(fd, buf, strlen(buf));
	char *log = (char *) malloc(MAXBUF);
	snprintf(log, MAXBUF, "Response headers:\n%s\n", buf);
	log_msg(log);

	/* Send response body to client */
	srcfd = Open(filename, O_RDONLY, 0);
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	Close(srcfd);
	Rio_writen(fd, srcp, filesize);
	Munmap(srcp, filesize);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype)
{
	if (strstr(filename, ".html"))
	{
		strcpy(filetype, "text/html");
	}
	else if (strstr(filename, ".gif"))
	{
		strcpy(filetype, "image/gif");
	}
	else if (strstr(filename, ".png"))
	{
		strcpy(filetype, "image/png");
	}
	else if (strstr(filename, ".jpg"))
	{
		strcpy(filetype, "image/jpeg");
	}
	else
	{
		strcpy(filetype, "text/plain");
	}
}
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
	char		buf	   [MAXLINE], *emptylist[] = {NULL};

	/* Return first part of HTTP response */
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server: Proxy Web Server\r\n");
	Rio_writen(fd, buf, strlen(buf));

	if (Fork() == 0)
	{			/* Child */
		/* Real server would set all CGI vars here */
		setenv("QUERY_STRING", cgiargs, 1);
		Dup2(fd, STDOUT_FILENO);	/* Redirect stdout to
											* client */
		Execve(filename, emptylist, environ);	/* Run CGI program */
	}
	Wait(NULL);		/* Parent waits for and reaps child */
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
	char		buf	   [MAXLINE], body[MAXBUF];

	/* Build the HTTP response body */
	sprintf(body, "<html><title>Proxy Error</title>");
	sprintf(body, "%s<body bgcolor=" "ffffff" ">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Proxy Web server</em>\r\n", body);

	/* Print the HTTP response */
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

