/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#define ___DEBUG
#define ___TEMPLATE_INIT

#include "http.h"
#include "log.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

/* 
 * check the url return 0 is matched 
 * */
static int http_url_check(char const *url);

/*
 * Generate a common request header
 * */
static char *http_req_hdr_comm(enum http_mtd_types method, char *host, char *uri);

/* 
 * new a struct of http_req 
 * */
struct http_req *http_compile(char *url, enum http_mtd_types method, hdr_gen func,
                                            enum http_cxt_types type,
                                            char *data)
{
	struct http_req *req;

	/* check the method type */
	if(method < 0 || method >= HTTP_MTD_UNKNOW)
	{
		_ERROR("Invalid method type: %d", method);
		return NULL;
	}

	/* set default handler */
	if(func == NULL)
	{
		_WARN("Func is null, try to generator header of HEAD.");
		func = http_req_hdr_hdr;
	}

	if(http_url_check(url))
	{
		_ERROR("Invalid url: %s", url);
		return NULL;
	}

	char *tmp;

	/* skip the :// */
	if(tmp = strstr(url, "://"), tmp != NULL)
	{
		url = tmp + 3;
	}

	req = calloc(sizeof(struct http_req), 1);
	req->addr = calloc(sizeof(struct http_addr), 1);

	/* set url */
	req->addr->url = strdup(url);

	/* set port */
	if(tmp = strchr(url, ':'), tmp != NULL)
	{
		++tmp;
		req->addr->port = atoi(tmp);
	}
	else
		req->addr->port = DEF_PORT;

	/* set host and uri */
	if(tmp = strchr(url, '/'), tmp != NULL)
	{
		req->addr->host = calloc(tmp - url + 1, 1);
		memccpy(req->addr->host, url, '/', tmp - url);

		req->addr->uri = strdup(tmp);
	}
	else
	{
		req->addr->host = strdup(url);

		/* set default URI */
		req->addr->uri = strdup("/");
	}

	/* set the request header */
	req->hdr = (*func)(req->addr->host, req->addr->uri, type, data);

	if(req->hdr == NULL)
	{
		_ERROR("Failed to genrator request header.");

		free(req->addr->host);
		free(req->addr->uri);
		free(req->addr->url);
		free(req->addr);
		free(req);

		return NULL;
	}

	req->hdr = realloc(req->hdr, strlen(req->hdr) + strlen(DCRLF) + 1);
	strcat(req->hdr, DCRLF);

	return req;
}

/* 
 * generate the request header
 * wrappr of req_hdr_comm
 * */
char *http_req_hdr_hdr(char *host, char *uri, enum http_cxt_types type, char *data)
{
    	return http_req_hdr_comm(HTTP_MTD_HEAD, host, uri);
}

char *http_req_hdr_get(char *host, char *uri, enum http_cxt_types type, char *data)
{
    	char *hdr = http_req_hdr_comm(HTTP_MTD_GET, host, uri);

	if(data)
	{
		hdr = realloc(hdr, strlen(hdr) + strlen(data) + 1);
		strcat(hdr, data);
	}

	return hdr;
}

void http_req_free(struct http_req *ptr)
{
	if(ptr == NULL)
	{
		return;
	}

	if(ptr->hdr)
	{
		free(ptr->hdr);
	}

	if(ptr->addr)
	{
		if(ptr->addr->uri)
			free(ptr->addr->url);

		if(ptr->addr->uri)
			free(ptr->addr->uri);

		if(ptr->addr->host)
			free(ptr->addr->host);

		free(ptr->addr);
	}

	free(ptr);
}

/* 
 * printf the request info 
 * */
void http_pr_req(struct http_req *req)
{
	if(req == NULL)
	{
		_WARN("Request is null.");
		return;
	}

	_DEBUG("URL:  %s", req->addr->url);
	_DEBUG("Host: %s", req->addr->host);
	_DEBUG("URI:  %s", req->addr->uri);
	_DEBUG("HDR:  \n%s", req->hdr);
}

/*
 * return 0 is ok
 * */
static int http_url_check(char const *url)
{
    	int ret;
	regex_t reg;

	ret = regcomp(&reg, 
	  			/* host */
	  			"^(http://)?\\w+(.[A-Za-z0-9\\-]+){1,3}"
	  			/* port */
	  			"(:[0-9]+)?"
	  			/* uri */
	  			"(/.+)?$"
	  			, REG_EXTENDED);

	if(ret)
	{
		_ERROR("Regex error.");

		regfree(&reg);
		return -1;
	}

	ret = regexec(&reg, url, 0, NULL, 0);

	regfree(&reg);
	return ret;
}


static char *http_req_hdr_comm(enum http_mtd_types method, char *host, char *uri)
{
    	char *tmp = req_hdr[method], *hdr;

	hdr = calloc(strlen(tmp) + strlen(host) + strlen(uri) + 1, 1);
	sprintf(hdr, tmp, uri, host);

	return hdr;
}
