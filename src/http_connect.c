/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#define ___DEBUG

#include "http.h"
#include "log.h"
#include "utils.h"

#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

int http_connect(struct http_addr *addr, unsigned int timeout)
{
	if(addr->port <= 0 || addr->port >= 0XFFFF)
	{
		_WARN("Error port: %d, set by DEF_PORT.", addr->port);
		addr->port = DEF_PORT;
	}

	int connfd, flag;
	struct hostent *he;
	struct sockaddr_in server;

	char *tmp;

	/* remove the port from host */
	tmp = calloc(strlen(addr->host) + 1, 1);
	memccpy(tmp, addr->host, ':', strlen(addr->host));

	/* remove port */
	if(strchr(tmp, ':'))
	{
		*(tmp + strlen(tmp) - 1) = '\0';
	}

	if(he = gethostbyname(tmp), he == NULL)
	{
		_ERROR("Get host(%s) info error.", addr->host);

		free(tmp);

		return -1;
	}

	free(tmp);

	/* init server's address information */
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(addr->port);
	server.sin_addr = *((struct in_addr *)he->h_addr);

	/* create tcp connection */
	connfd = socket(AF_INET, SOCK_STREAM, 0);
	if(connfd < 0)
	{
		_ERROR("Create TCP connection error: %s", strerror(errno));
		return -1;
	}

	/* set non-blocking */
	if(flag = fcntl(connfd, F_GETFL), flag < 0)
	{
		_ERROR("Get connection file descriptor flag error: %s", strerror(errno));

		close(connfd);
		return -1;
	}

	if(fcntl(connfd, F_SETFL, flag | O_NONBLOCK) < 0)
	{
		_ERROR("Set connection file descriptor flag error: %s", strerror(errno));

		close(connfd);
		return -1;
	}

	int ret, status;

	/* connect to server with timeout */
	ret = connect( connfd, (struct sockaddr *)&server, sizeof(struct sockaddr) );

	/* connect to server is ok */
	if(ret < 0)
	{
		/* timeout check */
		if(errno == EINPROGRESS)
		{
			while(1)
			{
				timeout_start(connfd, timeout > 60 ? 0 : timeout, TIMEOUT_WRITE, &status);
				break;
				timeout_end();

				if(status == EINTR)
					continue;

				else if(status < 0)
				{
					_ERROR("Connect to server(%s) error: %s", addr->host, strerror(status));
					return -1;
				}
				else if(status == 0)
				{
					_ERROR("Connect to server(%s) timeout.", addr->host);
					return -1;
				}

				/* connection is ok */
				break;
			}
		}
		else
		{
			_ERROR("Connect to server(%s) error: %s", addr->host, strerror(errno));
			return -1;
		}
	}

	/* set blocking */
	if(fcntl(connfd, F_SETFL, flag) < 0)
	{
		_ERROR("Set connfd blocking is error: %s", strerror(errno));
		close(connfd);
		return -1;
	}

	return connfd;
}

void http_closeconn(int connfd)
{
	if(connfd <= 0)
	{
		_WARN("Bad file descriptor.");
		return;
	}

    	shutdown(connfd, SHUT_RDWR);
}
