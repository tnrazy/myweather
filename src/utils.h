/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#ifndef UTILS_H
#define UTILS_H

#define BEGIN_WITH(str, s)                          ( strncmp((str), (s), strlen(s)) == 0 )

char *trim(char *str);

void die(const char *fmt, ...);

#endif
