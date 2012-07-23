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

#define CFGHOMENAME 							"myweather/myweather.conf"

#define CFG_MAP(XX) 							\
XX(MAIN_XY,    		"main_xy") 					\
XX(DAY0_XY,  		"day0_xy")                                   	\
XX(DAY1_XY, 		"day1_xy") 					\
XX(DAY2_XY, 		"day2_xy") 					\
XX(DAY3_XY, 		"day3_xy") 					\
XX(WIDTH, 		"width") 					\
XX(HEIGHT, 		"height") 					\
XX(ZIPCODE, 		"zipcode")

enum cfg_keys
{
    	#define XX(CFG_KEY, CFG_NAME)                          		CFG_##CFG_KEY,
	CFG_MAP(XX)
	#undef XX

    	CFG_UNKNOW
};

struct cfg
{
        enum cfg_keys cfgkey;
        char *name;
        char *value;
};

struct position
{
	int x;
	int y;
};

void cfg_load();

struct position *cfg_get_pos(enum cfg_keys pos_key, struct position *pos);

unsigned int cfg_get_width();

unsigned int cfg_get_height();

char *cfg_get_zipcode();

struct cfg **cfg_get_days_cfg();

struct position *cfg_get_pos_by_cfg(struct cfg *c, struct position *pos);

#endif
