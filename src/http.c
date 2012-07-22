/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#include "http.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define TIMEOUT                 5

static char *http_get_cookie(int conn, struct http_req *req);

char *http_getfile(char *url, char *fname, enum http_flags flag)
{
    	if_null_return_null(url);
	if_null_return_null(fname);

	int conn;
	char *filename, *cookie = NULL;
	struct http_req *req, *tmp;
	struct http_res *res;

	if(tmp = http_compile(url, HTTP_MTD_HEAD, 0, 0, 0), tmp == NULL)
	{
		_ERROR("Compile request of head fail.");

		return NULL;
	}

	if(conn = http_connect(tmp->addr, TIMEOUT), conn == -1)
	{
		_ERROR("Create connection error.");

		http_req_free(tmp);
		return NULL;
	}

	if(flag == HTTP_COOKIE)
	{
		if(cookie = http_get_cookie(conn, tmp), cookie == NULL)
		{
			_WARN("Get cookie fail.");
		}
	}

	http_req_free(tmp);

	/* compile request */
	if(req = http_compile(url, HTTP_MTD_GET, http_req_hdr_get, 0, cookie), req == NULL)
	{
		_ERROR("Compile request of get fail.");

		free(cookie);
		http_closeconn(conn);
		return NULL;
	}

	/* compile is ok, free cookie */
	free(cookie);

	/* execute the request */
	if(res = http_exec(conn, req, TIMEOUT), res == NULL)
	{
		_ERROR("Execute the request fail.");

		http_req_free(req);
		http_closeconn(conn);
		return NULL;
	}

	if(filename = http_fetch(conn, res, fname), filename == NULL)
	{
		_ERROR("Fetch data from the response error.");
	}

	http_req_free(req);
	http_res_free(res);
	http_closeconn(conn);

	_INFO("Connection over.");

	return filename;
}

static char *http_get_cookie(int conn, struct http_req *req)
{
    	char *cookie = NULL;
	struct http_req *hdrreq;
	struct http_res *res;

	if(hdrreq = http_compile(req->addr->host, HTTP_MTD_HEAD, 0, 0, 0), hdrreq == NULL)
	{
		_ERROR("Complile request of header is error.");

		return NULL;
	}

	/* execute the request for get the cookie */
	if(res = http_exec(conn, hdrreq, TIMEOUT), res == NULL)
	{
		_ERROR("Execute request error.");

		http_req_free(hdrreq);
		http_closeconn(conn);
		return NULL;
	}    

	/* get the cookie */
	if(res->cookie)
	{
		cookie = calloc(strlen(res->cookie) + 20, 1);
		sprintf(cookie, "cookie: %s\r\n", res->cookie);
	}

	http_req_free(hdrreq);
	http_res_free(res);

	return cookie;
}
