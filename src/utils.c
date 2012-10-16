/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

char *trim(char *str)
{
	char *s = NULL;

	while(isspace(*str++));

	char *left = --str;

	/* -1 '\0' */
	char *right = left + strlen(left) - 1;

	while(isspace(*right) && right > left)
	{
		*right-- = '\0';
	}

	s = left;

	return s;
}

void die(const char *format, ...)
{
	char buf[BUFSIZ] = { 0 };
	va_list argv;

	va_start(argv, format);
	vsnprintf(buf, sizeof buf, format, argv);
	va_end(argv);

	fprintf(stderr, "%s\n", buf);

	exit(EXIT_FAILURE);
}
