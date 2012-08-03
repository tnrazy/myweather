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


char *path_real(const char *path, char *filename)
{    
    	char *offset, *real, *home;
    
	if(path == NULL)
	{
		return NULL;
	}

	if(filename)
	{
		if(*filename == '/' || *(filename + strlen(filename) - 1) == '/')
		{
			/* invalide file name */
			return NULL;
		}
	}
	else 
		filename = "";

	offset = strstr(path, "~/");

	/* path begin with '~/' */
	if(offset == path)
	{
		if(home = getenv("HOME"), home == NULL)
		{
			return NULL;
		}

		real = calloc(strlen(home) + strlen(path) + strlen(filename), 1);

		strcpy(real, home);

		if(*(real + strlen(real) - 1) != '/')
		{
			*(real + strlen(real)) = '/';
		}

		/* +2 skip '~/' */
		//strcpy(real + strlen(real), path + 2);
		strcat(real, path + 2);

		if(*(real + strlen(real) - 1) != '/')
		{
			*(real + strlen(real)) = '/';
		}

		//strcpy(real + strlen(real), filename);
		strcat(real, filename);
	}
	else if(offset == NULL)
	{
		if(*path != '/')
		{
			/* invalide file path */
			return NULL;
		}

		real = calloc(strlen(path) + strlen(filename) + 2, 1);

		strcpy(real, path);

		if(*(real + strlen(real) - 1) != '/')
		{
			*(real + strlen(real)) = '/';
		}

		//strcpy(real + strlen(real), filename);
		strcat(real, filename);
	}
	/* invalid path */
	else
	{
		/* invalide file path */
		return NULL;
	}

	return real;
}

char *path_wildcard(const char *str)
{
    	if(str == NULL)
	{
		return NULL;
	}

	char *path, *filename, *offset, *realpath;

	/* file path should begin with '/' or '~/' */
	if(!(strstr(str, "~/") == str || strchr(str, '/') == str))
	{
		//_ERROR("%s, is not a path or filename.", str);
		return NULL;
	}

	offset = strrchr(str, '/');

	/* end with '/', not container file name */
	if(offset - str == strlen(str) - 1)
	{
		return path_real(str, NULL);
	}

	path = calloc(strlen(str), 1);
	strncpy(path, str, offset - str);

	filename = calloc(strlen(str), 1);
	strcpy(filename, offset + 1);

	/* generate full path name */
	realpath = path_real(path, filename);

	free(path);
	free(filename);
	return realpath;
}

