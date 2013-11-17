/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#ifndef CONFIG_H
#define CONFIG_H

struct cfg
{
        char name[32];
        char value[256];
};

struct position
{
	int x;
	int y;
};

void cfg_load(const char *filename, struct cfg **rc);

struct position *mkpos(const char *text, struct position *pos);

#endif
