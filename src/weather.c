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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>

#define WEATHER_XML 					"/tmp/weather.xml"
#define WEATHER_URL 					"http://xml.weather.com/weather/local/CHXX0120?cc=*&unit=m&dayf=6"

struct weather
{
	char name[16];
	char icon[16];

	int low;
	int hight;
};

static void weather_refresh();

static xmlNode *weather_query_node(xmlNode *root, char *node_name, char *attr_name, unsigned int day);

static struct weather *weather_query(struct weather *w, unsigned int day, xmlDoc *doc);

static void weather_ui();

/* main window */
static GtkWidget *window;

int main(int argc, const char *argv[])
{
	weather_refresh();

	xmlDoc *doc;

	doc = xmlReadFile("weather.xml", 0, 0);

	if(doc == NULL)
	{
		die("Failed to read xml file '%s'", WEATHER_XML);
	}

	struct weather today, day1, day2, day3;

	weather_query(&today, 0, doc);
	weather_query(&day1, 1, doc);
	weather_query(&day2, 2, doc);
	weather_query(&day3, 3, doc);

	xmlFreeDoc(doc);
	return EXIT_SUCCESS;
}

/* 
 * load ui
 * */
static weather_ui()
{
	if(window != NULL)
	{
		return;
	}

	/* fixed container */
	GtkWidget *fixed;

	gtk_init(NULL, NULL);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_widget_set_app_paintable(window, TRUE);

	/* remove the window decorate */
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

	/* set window size */
	gtk_window_set_default_size(GTK_WINDOW(window), 120, 480);
	
	/* remove taskbar */
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);

	gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);

	gtk_window_set_keep_below(GTK_WINDOW(window), TRUE);

	/* set window position */
	gtk_widget_set_uposition(window, 0, 0);

	fixed = gtk_fixed_new();

	gtk_container_add(GTK_WINDOW(window), fixed);
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


	node = weather_query_node(root, "day", "d", day);

	if(day == 0)
	{
		strncpy(w->name, "Today", sizeof w->name);
	}
	else
		strncpy(w->name, (char *)xmlGetProp(node, (const xmlChar *)"t"), sizeof w->name);

	/* get temperature */
	xmlNode *temp = weather_query_node(node, "low", NULL, 0);
	w->low = atoi((char *)xmlNodeListGetString(doc, temp->xmlChildrenNode, 1));

	/* get temperature icon */
	xmlNode *icon = weather_query_node(node, "icon", NULL, 0);
	snprintf(w->icon, sizeof w->icon, "%s.png", xmlNodeListGetString(doc, icon->xmlChildrenNode, 1));

	return w;
}

static void weather_refresh()
{
	http_getfile(WEATHER_URL, WEATHER_XML, HTTP_NOCOOKIE);
}
