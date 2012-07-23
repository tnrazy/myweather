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
#include "http_utils.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

/* 
 * read response header from connfd
 * */

static char *http_res_gethdr(int connfd, char **tail, unsigned long *tail_len);

static int http_res_attr(char *hdr, struct http_res *res);

static struct file_type *http_cxt_type(char *tname);

/*
 * send request to server
 * if error return null
 * */
struct http_res *http_exec(int connfd, struct http_req *req, unsigned int timeout)
{
	struct http_res *res;
	char *hdr;

	if(connfd <= 0)
	{
		_ERROR("Connection error.");

		return NULL;
	}  

	if(timeout < 0)
	{
		_WARN("Timeout value is error.");
		timeout = DEF_TIMEOUT;
	}

	_DEBUG("Request header:\n%s", req->hdr);

	/* send request */
	int status = 0;

	timeout_start(connfd, timeout, (TIMEOUT_WRITE | TIMEOUT_ERROR), &status);
	while(send(connfd, req->hdr, strlen(req->hdr), 0) < 0)
	{
		if(errno == EINTR)
			continue;

		_ERROR("Send() error: %s", strerror(errno));

		return NULL;
	}
	timeout_end();


	if(status <= 0)
	{
		if(status == 0)
			_ERROR("Send message timeout.");
		else
			_ERROR("%s", strerror(status));

		return NULL;
	}

	res = calloc(sizeof(struct http_res), 1);

	/* get response header */
	if(hdr = http_res_gethdr(connfd, &res->tail, &res->tail_len), hdr == NULL)
	{
		_ERROR("Get response header error.");

		free(res);
		return NULL;
	}

	/* get response header attribute */
	if(http_res_attr(hdr, res) == 0)
	{
		_ERROR("Response header format error.");
		free(hdr);
		free(res);
	}

	free(hdr);
	return res;
}

void http_pr_res(struct http_res *res)
{
	if(res == NULL)
	{
		_WARN("Response is null.");
		return;
	}

	_DEBUG("Response status code: %d", res->status_code);
	_DEBUG("Context type: %s", res->type->tname);
	_DEBUG("Context length: %d", res->length);
}

void http_res_free(struct http_res *res)
{
	if(res == NULL)
	{
		return;
	}

	_DEBUG("Free the response.");
	if(res->cookie)
		free(res->cookie);

	if(res->tail)
		free(res->tail);

	free(res);
}

/* --- PRIVATE --- */
static char *http_res_gethdr(int connfd, char **tail, unsigned long *tail_len)
{
    	int num_bytes, read_bytes = 0;

	char buf[512] = { 0 }, *tmp, *hdr = NULL;

	while(1)
	{
		memset(&buf, 0, sizeof buf);

		while(num_bytes = recv(connfd, buf, sizeof buf - 1, 0), num_bytes < 0)
		{
			if(errno == EINTR)
				continue;

			_ERROR("Receive data error: %s", strerror(errno));

			free(hdr ? hdr : NULL);
			return NULL;
		}

		/* reach the end */
		if(num_bytes == 0)
			break;

		if(hdr == NULL)
			hdr = calloc(strlen(buf) + 1, 1);
		else
			hdr = realloc(hdr, strlen(hdr) + sizeof buf + 1);

		strcat(hdr, buf);

		/* read header finish */
		if(tmp = strstr(hdr, DCRLF), tmp)
		{
			/* (read size) - ((header size) - (size offset)) */
			*tail_len = num_bytes - ((tmp + 4 - hdr) - read_bytes);

			if((int)*tail_len > 0)
			{
				*tail = calloc(*tail_len + 1, 1);
				memcpy(*tail, buf + (num_bytes - *tail_len), *tail_len);
			}

			_DEBUG("Read size: %d, Tail: %d, Offset: %d", num_bytes, *tail_len, num_bytes - *tail_len);

			/* set '\0' end of header */
			*(tmp + 4) = '\0';

			break;
		}

		if(strlen(hdr) > HTTP_HDR_SIZE)
		{
			_ERROR("Header too big.");

			free(hdr);
			return NULL;
		}

		/* already read bytes */
		read_bytes += num_bytes;
	}

	if(hdr == NULL)
	{
		_ERROR("Header is null.");
		return NULL;
	}

	_DEBUG("Response header:\n%s", hdr);

	return hdr;
}


static int http_res_attr(char *hdr, struct http_res *res)
{
    	int valid = 0, field = 0;
	char *walk = hdr, line[512], *tmp;

	res->status_code = 0;
	res->length = 0;
	res->type = NULL;
	res->cookie = NULL;

	while(walk = strstr_ln(walk, line, sizeof line, "\r\n"), walk)
	{
		/* the first line is the response status line */
		if(res->status_code == 0)
		{ 
			sscanf(line, "HTTP/1.%d %d %*s", &res->minor, &res->status_code);
			_DEBUG("Response status code: %d, minor = %d", res->status_code, res->minor);

			++field;
			/* the reponse header must be has status line */
			++valid;

			continue;
		}

		/* invalid header format */
		if(valid == 0)
			break;

		/* get the content length, context length is before encode */
		if(res->length == 0)
		{
			if(tmp = strstr_igcase(line, "content-length: ", 0), tmp)
			{
				res->length = atoi(tmp);
				_DEBUG("Content length: %d", res->length);

				++field;
				continue;
			}
		}

		/* get context type desc */
		if(res->type == 0)
		{
			res->type = http_cxt_type(line);

			field += res->type ? 1 : 0;

			continue;
		}

		/* get response cookie */
		if(tmp = strstr_igcase(line, "set-cookie: ", 0), tmp)
		{
			if(res->cookie == NULL)
				res->cookie = calloc(strlen(tmp) + 1, 1);
			else
				res->cookie = realloc(res->cookie, strlen(res->cookie) + strlen(tmp) + 1);

			/* trim the \r\n */
			strncpy(res->cookie + strlen(res->cookie), tmp, strlen(tmp) - 2);

			++field;
		}

		/* other things */
	}

	/* check return value if 0 just free res */
	return valid == 1 ? field : 0;
}

static struct file_type *http_cxt_type(char *tname)
{
    	if(strstr_igcase(tname, "content-type: ", 0) == NULL)
    	{
    		return NULL;
    	}

	for(struct file_type *list = cxt_types, *ftype = list; ftype;)
	{
		if(ftype->idx == -1)
			break;

		if(strstr(tname, ftype->tname))
		{
			_DEBUG("Content-type: %s", ftype->tname);
			return ftype;
		}

		ftype = ++list;
	}

	return NULL;
}

