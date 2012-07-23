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
XX(TODAY_XY,  		"today_xy")                                   	\
XX(WIDTH, 		"width") 					\
XX(HEIGHT, 		"height")

enum cfg_keys
{
    	#define XX(CFG_KEY, CFG_NAME)                          		CFG_##CFG_KEY,
	CFG_MAP(XX)
	#undef XX

    	CFG_UNKNOW
};

void cfg_load();

void cfg_refresh();

unsigned int cfg_get_x();

unsigned int cfg_get_y();

unsigned int cfg_get_pos_lock();

void cfg_set_postion(const char *xy);

void cfg_set_postion_lock();

#endif
