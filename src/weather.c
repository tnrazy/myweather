/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#include "http.h"
#include "log.h"
#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>

#define WEATHER_XML 					"/tmp/weather.xml"
#define WEATHER_API 					"http://xml.weather.com/weather/local/%s?cc=*&unit=m&dayf=6"

struct weather
{
	char name[16];
	char icon[16];

	int low;
};

static xmlDoc *weather_refresh();

static xmlNode *weather_query_node(xmlNode *root, char *node_name, char *attr_name, unsigned int day);

static struct weather *weather_query(struct weather *w, unsigned int day, xmlDoc *doc);

static void weather_ui();

/* weather days */
static GtkWidget **days;

/* weather today */

int main(int argc, const char **argv)
{
	weather_refresh();

	weather_ui();

	return EXIT_SUCCESS;
}

/* 
 * load ui
 * */
static void weather_ui()
{
	GtkWidget *window;                      /* main window */
	GtkWidget *fixed;                       /* fixed container */
	struct position win_pos;
	struct cfg **days_cfg;

	gtk_init(NULL, NULL);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_widget_set_app_paintable(window, TRUE);

	/* remove the window decorate */
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

	/* set window size */
	gtk_window_set_default_size(GTK_WINDOW(window), 120, 220);
	
	/* remove taskbar */
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);

	gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);

	gtk_window_set_keep_below(GTK_WINDOW(window), TRUE);

	/* set window position */
	cfg_get_pos(CFG_MAIN_XY, &win_pos);
	gtk_widget_set_uposition(window, win_pos.x, win_pos.y);

	fixed = gtk_fixed_new();

	gtk_container_add(GTK_CONTAINER(window), fixed);

	/* get all days config */
	days_cfg = cfg_get_days_cfg();

	register int idx = 0, days_id;
	struct cfg **ptr = days_cfg;
	struct position pos;
	struct weather day;
	xmlDoc *doc = weather_refresh();

	for(struct cfg *c = *days_cfg; c;)
	{
		if(days == NULL)
		{
			days = calloc(sizeof *days, 1 + 1);
		}
		else
			days = realloc(days, (idx + 1 + 1) * sizeof *days);

		days_id = atoi(strstr(c->name, "day") + 3);

		cfg_get_pos_by_cfg(c, &pos);

		weather_query(&day, days_id, doc);

		chdir("icons");
		
		_INFO("%s", day.name);
		_INFO("%d", day.low);

		*(days + idx) = gtk_image_new_from_file(day.icon);

		gtk_fixed_put(GTK_FIXED(fixed), *(days + idx), pos.x, pos.y);

		*(days + idx + 1) = NULL;

		c = *++days_cfg;
	}

	free(ptr);

	gtk_widget_show_all(window);

	gtk_main();
}

static xmlNode *weather_query_node(xmlNode *root, char *node_name, char *attr_name, unsigned int day)
{
	xmlNode *node = NULL;

	for(xmlNode *cur_node = root; cur_node; cur_node = cur_node->next)
	{
		if(cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *)node_name))
		{
			/* query node by node name */
			if(attr_name == NULL)
			{
				return cur_node;
			}

			for(xmlAttr *cur_attr = cur_node->properties; cur_attr;)
			{
				if(!xmlStrcmp(cur_attr->name, (const xmlChar *)attr_name))
				{
					char *value = (char *)xmlGetProp(cur_node, (const xmlChar *)attr_name);

					if((unsigned int)atoi(value) == day)
					{
						return cur_node;
					}
				}

				cur_attr = cur_attr->next;
			}
		}

		/* whether has child node */
		node = weather_query_node(cur_node->children, node_name, attr_name, day);

		if(node)
		{
			return node;
		}
	}

	return NULL;
}

static struct weather *weather_query(struct weather *w, unsigned int day, xmlDoc *doc)
{
	xmlNode *root, *node = NULL;	
	
	root = xmlDocGetRootElement(doc);

	if(root == NULL)
	{
		xmlFreeDoc(doc);

		die("Empty document");
	}

	if(xmlStrcmp(root->name, (const xmlChar *)"weather"))
	{
		xmlFreeDoc(doc);

		die("Document wrong type");
	}

	/* get weather xml node */
	if(day == 0)
	{
		node = weather_query_node(root, "cc", NULL, 0);

		strncpy(w->name, "Today", sizeof w->name);
	}
	else
	{
		node = weather_query_node(root, "day", "d", day);

		strncpy(w->name, (char *)xmlGetProp(node, (const xmlChar *)"t"), sizeof w->name);
	}

	/* get temperature */
	xmlNode *temp = weather_query_node(node, day == 0 ? "tmp" : "low", NULL, 0);
	w->low = atoi((char *)xmlNodeListGetString(doc, temp->xmlChildrenNode, 1));

	/* get temperature icon */
	xmlNode *icon = weather_query_node(node, "icon", NULL, 0);
	snprintf(w->icon, sizeof w->icon, "%s.png", xmlNodeListGetString(doc, icon->xmlChildrenNode, 1));

	return w;
}

static xmlDoc *weather_refresh()
{
	char *zipcode, *url;

	zipcode = cfg_get_zipcode();

	url = calloc(strlen(WEATHER_API) + strlen(zipcode) + 1, 1);

	sprintf(url, WEATHER_API, zipcode);

	_DEBUG("URL: %s", url);

	//http_getfile(url, WEATHER_XML, HTTP_NOCOOKIE);

	free(zipcode);
	free(url);

	xmlDoc *doc = xmlReadFile(WEATHER_XML, 0, 0);

	return doc;
}
