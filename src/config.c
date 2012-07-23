/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#define ___DEBUG

#include "config.h"
#include "utils.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

static struct cfg cfgs[] = 
{
        #define XX(CFG_KEY, CFG_NAME)                               {.cfgkey = CFG_##CFG_KEY, .name = CFG_NAME, .value = NULL},
        CFG_MAP(XX)
        #undef XX

        {.cfgkey = CFG_UNKNOW, .name = NULL, .value = NULL}
};

static FILE *cfg_resolver();

static char *cfg_get(enum cfg_keys cfg_key);

static int cfg_init;

#define INIT_CHK()                                                          if(cfg_init) return;
#define INIT_OK()                                                           cfg_init = 1;

static char *cfg_filename;

static void cfg_check();

void cfg_load()
{
	INIT_CHK();

	FILE *cfgfile;

	cfgfile = cfg_resolver();

	if(cfgfile == NULL)
	{
		/* fopen() error */
		die("Failed to load config file '%s' ", cfg_filename);
	}

	/* init config value */
	char buf[512] = { 0 }, *line, *tmp;

	while(fgets(buf, sizeof buf, cfgfile))
	{
		_DEBUG("Line: %s\n", buf);

		line = trim(buf);

		/* skip comment line */
		if(BEGIN_WITH(line, "#"))
		{
			continue;
		}

		/* skip blank line */
		if(strcmp(line, "") == 0)
		{
			continue;
		}

		for(struct cfg *list = cfgs, *entity = list; entity;)
		{
			if(entity->cfgkey == CFG_UNKNOW)
			{
				fclose(cfgfile);
				die("Unknow config line: %s", line);
			}

			if(BEGIN_WITH(line, entity->name))
			{
				tmp = strstr(line, "=") + 1;

				entity->value = trim(strdup(tmp));

				_INFO("%s=%s", entity->name, entity->value);

				break;
			}

			entity = ++list;
		}
	}

	fclose(cfgfile);

	INIT_OK();

	cfg_check();

	_INFO("Load config file '%s' is ok.", cfg_filename);
}

struct position *cfg_get_pos_by_cfg(struct cfg *c, struct position *pos)
{
	pos->x = atoi(c->value);
	pos->y = atoi(strchr(c->value, ',') + 1);

	return pos;
}

struct position *cfg_get_pos(enum cfg_keys pos_key, struct position *pos)
{
	if(pos_key < CFG_MAIN_XY || pos_key > CFG_DAY3_XY)
	{
		_ERROR("Unknow pos_key: %d", pos_key);
		pos->x = 0;
		pos->y = 0;
		
		return pos;
	}

	char *value = cfg_get(pos_key);

	pos->x = atoi(value);
	pos->y = atoi(strchr(value, ',') + 1);

	free(value);
	return pos;
}

struct cfg **cfg_get_days_cfg()
{
	struct cfg **days_cfg = NULL;
	register int idx = 0;

	for(struct cfg *c = cfgs; c; ++c)
	{
		if(c->cfgkey == CFG_UNKNOW)
		{
			break;
		}

		if(BEGIN_WITH(c->name, "day"))
		{
			if(days_cfg == NULL)
			{
				days_cfg = calloc(sizeof *days_cfg, 1 + 1);
			}
			else
				days_cfg = realloc(days_cfg, (idx + 1 + 1) * sizeof *days_cfg);

			*(days_cfg + idx) = c;

			*(days_cfg + idx + 1) = NULL;

			++idx;
		}
	}

	return days_cfg;
}

/*struct position **cfg_get_days_pos()*/
//{
	//struct position **days_pos = NULL;
	//register int idx = 0;

	//for(struct cfg *c = cfgs; c; ++c)
	//{
		//if(c->cfgkey == CFG_UNKNOW)
		//{
			//break;
		//}

		//if(BEGIN_WITH(c->name, "day"))
		//{
			//if(days_pos == NULL)
			//{
				//days_pos = calloc(sizeof *days_pos, 1 + 1);
			//}
			//else
				//days_pos = realloc(days_pos, (idx + 1 + 1) * sizeof *days_pos);

			//*(days_pos + idx) = calloc(sizeof **days_pos, 1);

			//cfg_get_pos(c->cfgkey, *(days_pos + idx));

			//*(days_pos + idx + 1) = NULL;

			//++idx;
		//}
	//}

	//return days_pos;
/*}*/

unsigned int cfg_get_width()
{
	char *value = cfg_get(CFG_WIDTH);
	int width = atoi(value);

	free(value);
	return width;
}

unsigned int cfg_get_height()
{
	char *value = cfg_get(CFG_HEIGHT);
	int height = atoi(value);

	free(value);
	return height;
}

char *cfg_get_zipcode()
{
	return cfg_get(CFG_ZIPCODE);
}

static char *cfg_get(enum cfg_keys cfg_key)
{
    	cfg_load(cfg_filename);

	if(cfg_key < 0 || cfg_key >= CFG_UNKNOW)
	{
		_ERROR("Unknow cfg_key: %d", cfg_key);
		return NULL;
	}

	return strdup(cfgs[cfg_key].value);
}

static FILE *cfg_resolver()
{
	FILE *cfgfile = NULL;

	if(cfg_filename)
	{
		cfgfile = fopen(cfg_filename, "r");

		return cfgfile;
	}

	cfg_filename = path_real(getenv("XDG_CONFIG_HOME"), CFGHOMENAME);

	_DEBUG("Try load default config file: %s", cfg_filename);

	if(access(cfg_filename, F_OK | R_OK) == -1)
	{
		die("Config file '%s' not exists or not can read", cfg_filename);
	}

	/* save config file name */
	cfgfile = fopen(cfg_filename, "r");

	return cfgfile;
}

static void cfg_check()
{
	for(struct cfg *c = cfgs; c; ++c)
	{
		if(c->cfgkey == CFG_UNKNOW)
		{
			break;
		}

		if(c->value == NULL && BEGIN_WITH(c->name, "day0"))
		{
			die("Today(day0) cna not be null");
		}

		if(c->value == NULL && !BEGIN_WITH(c->name, "day"))
		{
			die("Config field '%s' is null", c->name);
		}
	}
}
