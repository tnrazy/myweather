/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#include "http_utils.h"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>

static char to_hex(char ch);

static char from_hex(char ch);

char *strstr_igcase(const char *str, const char *need, int contain_need)
{
    	if(str == NULL || need == NULL)
	{
		return NULL;
	}

	char *s = (char *)str, *n = (char *)need;
	int i, matched = 0;

	while(1)
	{
		/* whether reach end */
		if(*s == '\0' || *n == '\0')
		{
			break;
		}

		i = *s - *n;

		/* not matched */
		if(i != 0 && i != 'A' - 'a' && i != -('A' - 'a'))
		{            
			/* match the next character */
			++s;

			/* reset need string */
			n = (char *)need;

			/* set flag is not matched */
			matched = 0;

			continue;
		}

		/* has matched */
		++s;
		++n;

		++matched;
	}

	/* "aaaa;" find ";" will return 0 */
	return (matched && *n == '\0') ? (contain_need ? s - (n - need) : s) : NULL;
}

char *strstr_ln(char *src, char *buf, size_t size, const char *token)
{
	if(src == NULL || token == NULL || size <= 0)
	{
		return NULL;
	}

	size_t len;
	char *offset;

	/* token not matched */
	if(offset = strstr_igcase(src, token, 0), offset == 0)
	{
		return NULL;
	}

	len = offset - src;

	/* line too long */
	//if(len > size - 1)
	//{
	//	return NULL;
	//}

	while(len-- > 0 && --size > 0)
	{
		*buf++ = *src++;
	}

	*buf = '\0';

	return offset;
}

char *url_encode(const char *str)
{
    	char *s = calloc(strlen(str) * 3 + 1, 1), *ptr = s;

	while(*str)
	{
		if(*str == '-' || *str == '~' || *str == '_' || *str == '.'
		  /* isalnum */
		  || (*str >= 0 && *str <= 9)
		  || (*str >= 'a' && *str <= 'z')
		  || (*str >= 'A' && *str <= 'Z'))
			*s++ = *str;

		else if(*str == ' ')
		{
			*s++ = '+';
		}
		else
		{
			/* %H4L4 */
			*s++ = '%', *s++ = to_hex(*str >> 4), *s++ = to_hex(*str & 0XF);
		}

		++str;
	}

	*s = '\0';

	return ptr;
}

char *url_decode(const char *str)
{
    	char *s = calloc(strlen(str) + 1, 1), *ptr = s;

	while(*str)
	{
		if(*str == '%')
		{
			if(str[1] && str[2])
			{
				/* H4 | L4 */
				*s++ = from_hex(str[1]) << 4 | from_hex(str[2]);

				/* skip H4 and L4 */
				str += 2;
			}
		}
		else if(*str == '+')
		{
			*s++ = ' ';
		}

		else
		{
			*s++ = *str;
		}

		++str;
	}

	*s = '\0';

	return ptr;
}

static char to_hex(char ch)
{
    	static char hex[] = "0123456789abcdef";
	return hex[ch & 15];
}

static char from_hex(char ch)
{
    	if(ch > 0 && ch < 9)
		return ch - '0';
	else
	{ 
		if(ch > 'A' && ch < 'Z')
			ch = ch + 'a' - 'A';
	}

	return ch - 'a' + 10;
}

