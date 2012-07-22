/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>

#define DEF_PORT                                    80
#define DEF_TIMEOUT                                 5
#define PERM                                        0660

#define KB                                          (1024ULL) << 0
#define MB                                          (1024ULL) << 10
#define GB                                          (1024ULL) << 20

/* limit the response header size is 8KB */
#define HTTP_HDR_SIZE                               8 * KB
#define DCRLF                                       "\r\n\r\n"

#define UAGENT                                      "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.2.13) Gecko/20101225 Firefox/3.6.13"

#define METHOD_TYPE_MAP(XX)                                 \
XX(HEAD,        0,  "HEAD %s HTTP/1.1\r\n"                  \
                    "Host: %s\r\n"                          \
                    "User-Agent: " UAGENT "\r\n"            \
                    "Connection: keep-alive\r\n",           \
                    /* extension header */                  \
                    ""                                      )\
                                                            \
XX(GET,         1,  "GET %s HTTP/1.1\r\n"                   \
                    "Host: %s\r\n"                          \
                    "User-Agent: " UAGENT "\r\n"            \
                    "Connection: close\r\n",                \
                    /* extension header */                  \
                    "Cookie: %s\r\n"                        )

#define CONTEXT_TYPE_MAP(XX)                                \
XX(X_FORM,      0,  "application/x-www-form-urlencoded", 0  )\
XX(HTML,        0,  "text/html",        ".html"             )\
XX(JPEG,        1,  "image/jpeg",       ".jpg"              )\
XX(GIF,         1,  "image/png",        ".png"              )\
XX(PNG,         1,  "image/gif",        ".gif"              )

enum http_mtd_types
{
#define XX(NAME, VALUE, HDR, EXT_HDR)                       HTTP_MTD_##NAME = VALUE,
METHOD_TYPE_MAP(XX)
#undef XX
HTTP_MTD_UNKNOW
};

enum http_cxt_types
{
#define XX(NAME, VALUE, STR, EXT)                           HTTP_CXT_##NAME = VALUE,
CONTEXT_TYPE_MAP(XX)
#undef XX
HTTP_CXT_UNKNOW
};

enum http_flags
{
    HTTP_NOCOOKIE, HTTP_COOKIE
};

struct file_type
{
    enum http_cxt_types idx;
    char *tname;
    char *ext;
};

/* http response */
struct http_res
{
    unsigned int status_code;

    /* content length */
    unsigned long length;

    /* http minor version */
    unsigned int minor;

    char *cookie;

    /* content data start from here */
    char *tail;
    unsigned long tail_len;

    /* content type */
    struct file_type *type;
};

struct http_addr
{
    char *url;

    char *host;
    uint16_t port;
    char *uri;
};

/* http request */
struct http_req
{
    struct http_addr *addr;
    /* request header */
    char *hdr;
};

/* 
 * _TEMPLATE_INIT should only appear once
 * */
#ifdef ___TEMPLATE_INIT

char *req_hdr[] = 
{
    #define XX(NAME, VALUE, HDR, EXT_HDR)                   HDR,
    METHOD_TYPE_MAP(XX)
    #undef XX

    /* end */
    0
};

char *req_hdr_ext[] =
{
    #define XX(NAME, VALUE, HDR, EXT_HDR)                   EXT_HDR,
    METHOD_TYPE_MAP(XX)
    #undef XX

    /* end */
    0
};

struct file_type cxt_types[] =
{
    #define XX(NAME, VALUE, STR, EXT)                       {.idx = VALUE, .tname = STR, .ext = EXT},
    CONTEXT_TYPE_MAP(XX)
    #undef XX

    /* end */
    {.idx = -1 , .tname = 0, .ext = 0}
};

#else

    extern char *req_gdr[];
    extern char *req_hdr_ext[];
    extern struct file_type cxt_types[];

#endif

/* generator the request header */
typedef char *(*hdr_gen)(char *host, char *uri, enum http_cxt_types type, char *data);

/* create a request */
struct http_req *http_compile(char *url, enum http_mtd_types method, hdr_gen func,
                                            enum http_cxt_types type,
                                            char *data);

int http_connect(struct http_addr *addr, unsigned int timeout);

void http_closeconn(int connfd);

struct http_res *http_exec(int connfd, struct http_req *req, unsigned int timeout);

char *http_fetch(int connfd, struct http_res *res, char *filename);

/* clean */
void http_req_free(struct http_req *ptr);

void http_res_free(struct http_res *ptr);

/* debug */
void http_pr_req(struct http_req *req);

void http_pr_res(struct http_res *res);

/* header generator */
char *http_req_hdr_hdr(char *host, char *uri, enum http_cxt_types type, char *data);

char *http_req_hdr_get(char *host, char *uri, enum http_cxt_types type, char *data);

char *http_getfile(char *url, char *fname, enum http_flags flag);

#endif
