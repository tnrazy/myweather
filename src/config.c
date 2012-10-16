/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#include "config.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


void cfg_load(const char *filename, struct cfg **rc)
{
	FILE *fp = fopen(filename, "r");

	if(NULL == fp)
	{
		die("Failed to load config file '%s'", filename);
	}

	/* init config value */
	char buf[512] = { 0 }, *line = NULL;

	while(!feof(fp) && fgets(buf, 512, fp))
	{
		line = trim(buf);

		if(NULL == line)
		{
			continue;
		}

		/* skip comment line */
		if(BEGIN_WITH(line, "#"))
		{
			continue;
		}

		/* skip blank line */
		if(0 == strcmp(line, ""))
		{
			continue;
		}


		for(struct cfg **list = rc, *entity = *list; entity;)
		{
			if(NULL == entity->name)
			{
				break;
			}

			if(BEGIN_WITH(line, entity->name))
			{
				strncpy(entity->value, trim(strchr(line, '=') + 1), sizeof entity->value);

				break;
			}

			entity = *++list;
		}
	}

	fclose(fp);
}

struct position *mkpos(const char *text, struct position *pos)
{
	int x, y;

	if(NULL == text)
	{
		return NULL;
	}

	x = atoi(text);
	y = atoi(strchr(text, ',') + 1);

	pos->x = x;
	pos->y = y;

	return pos;
}

