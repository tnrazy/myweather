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

#define EQUAL( a, b) 				( 0 == strcmp( (a), (b) ) )
#define STARTWITH( string, start ) 		( 0 == strncmp( (string), (start), strlen( (start) ) ) )
#define ENDWITH( string, end ) 			EQUAL( (string) + strlen( (string) - strlen( (end) ) ), (end) )

char *trim(char *str);

void die(const char *fmt, ...);

#endif
