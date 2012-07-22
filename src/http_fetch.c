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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int http_do_fetch(int connfd, char *fullname, struct http_res *res);

char *http_fetch(int connfd, struct http_res *res, char *filename)
{
	if(res == NULL || filename == NULL)
	{
		return NULL;
	}

	if(connfd <= 0)
	{
		_ERROR("Connection error.");
		return NULL;
	}

	if(res->status_code != 200)
	{
		_ERROR("Error response status: %d", res->status_code);
		return NULL;
	}

	if(res->length <= 0)
	{
		_WARN("No data.");
		return NULL;
	}

	_DEBUG("filename: %s", filename);

	//if(res->minor < 1)
	if(http_do_fetch(connfd, filename, res) != 0)
	{
		_ERROR("Fetch data error.");


		if(unlink(filename) == 0)
		{
			_INFO("Remove the file: %s", filename);
		}
		else
		{
			_WARN("Remove file: %s, error.", filename);
		}

		return NULL;
	}

	return filename;
}

static int http_do_fetch(int connfd, char *filename, struct http_res *res)
{
    	char buf[1024];
	int fd, num_bytes, remain = res->length;

	/* create file */
	fd = open(filename, O_CREAT | O_WRONLY, PERM);

	if((int)res->tail_len > 0)
	{
		if(num_bytes = write(fd, res->tail, res->tail_len), num_bytes != res->tail_len)
		{
			_ERROR("Write the tail to the file is error: %s", strerror(errno));

			return remain;
		}

		_DEBUG("Tail: %d, Write: %d", res->tail_len, num_bytes);
	}

	remain -= res->tail_len;

	while(1)
	{
		while(num_bytes = recv(connfd, buf, sizeof buf, 0), num_bytes < 0)
		{
			if(errno == EINTR)
			{
				_ERROR("EINTR");
				continue;
			}

			close(fd);
			_ERROR("Receive data error: %s", strerror(errno));
			return remain;
		}

		remain -= num_bytes;

		if(num_bytes == 0)
		{
			close(fd);
			break;
		}

		while(write(fd, buf, num_bytes) != num_bytes)
		{
			_ERROR("Write data error: %s", strerror(errno));

			close(fd);
			if(unlink(filename) == 0)
			{
				_INFO("Remove the file: %s", filename);
			}
			else
				_ERROR("Remove file: %s, error.", filename);

			return remain;
		}

		//_DEBUG("%s, PROGRESS: %d%%", fullname, (int)(((double)total - remain) / total * 100));

		if(remain <= 0)
		{
			break;
		}
	}

	return remain;
}
