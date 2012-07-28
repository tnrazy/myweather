/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * tn.razy@gmail.com
 */

#include "log.h"
#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <curl/curl.h>
#include <gtk/gtk.h>
#include <gdk/gdkscreen.h>
#include <cairo.h>
#include <assert.h>
#include <libxml/parser.h>

#define WEATHER_XML 					"/tmp/weather.xml"
#define WEATHER_API 					"http://xml.weather.com/weather/local/%s?cc=*&unit=m&dayf=6"

#define WEATHER_RES 					"/usr/share/myweather"
#define WEATHER_DEF_ICON 				"na.png"
#define WEATHER_DEF_TEXT 				"---"
#define WEATHER_DEF_TMP 				0
#define WEATHER_FMT_TEXT 				"<span foreground='white' font_desc='nu.se 6'>%d°C %s</span>"

struct weather_info
{
	char name[16];
	char icon[16];

	int temperature;
};

struct weather
{
	struct weather_info *day_info;

	unsigned int day_id;
	
	GtkWidget *icon;

	GtkWidget *text;
};

static void weather_refresh();

static xmlDoc *weather_load();

static struct weather_info *weather_query(struct weather_info *info, unsigned int day, xmlDoc *doc);

static void weather_ui();

static int ui_set_transparent(GtkWidget *widget, GdkScreen *old_screen, void *data);

static int ui_do_transparent(GtkWidget *widget, GdkEventExpose *event, void *data);

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);

/* weather days */
static struct weather **days;

int main(int argc, const char **argv)
{
	if(argc >= 2)
	{
		fprintf(stderr, "Just type '%s', = =\n", *argv);
		exit(EXIT_SUCCESS);
	}

	if(access(WEATHER_RES, F_OK) == -1)
	{
		die("Weather resource directory '%s' is not exists", WEATHER_RES);
	}

	if(access(WEATHER_RES, R_OK) == -1)
	{
		die("Weather resource directory '%s' access denined", WEATHER_RES);
	}

	if(chdir(WEATHER_RES) == -1)
	{
		die("Failed to change working directory: %s", strerror(errno));
	}

	signal(SIGHUP, SIG_IGN);

	switch(fork())
	{
		case -1:
			die("fork() error: %s", strerror(errno));

		case 0:
			break;
			
		default:
			exit(EXIT_SUCCESS);
	}

	close(STDIN_FILENO);
	stdout = freopen("/dev/null", "rw", stdout);
	stderr = freopen("/dev/null", "rw", stderr);

	weather_ui();

	return EXIT_SUCCESS;
}

/* 
 * load ui
 * */
static void weather_ui()
{
	GtkWidget *window;
	GtkWidget *container;

	struct position win_pos, pos;

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

	container = gtk_fixed_new();

	gtk_container_add(GTK_CONTAINER(window), container);

	/* get all days config */
	struct cfg **days_cfg;
	struct cfg **ptr;

	days_cfg = cfg_get_days_cfg();
	ptr = days_cfg;

	register int idx = 0;

	for(struct cfg *c = *days_cfg; c;)
	{
		if(days == NULL)
		{
			days = calloc(sizeof *days, 1 + 1);
		}
		else
			days = realloc(days, (idx + 1 + 1) * sizeof *days);

		struct weather *w = calloc(sizeof **days, 1);
		
		/* today? tomorrow? or .. */
		w->day_id = atoi(strstr(c->name, "day") + 3);

		/* weather info */
		w->day_info = calloc(sizeof *w->day_info, 1);

		/* widget icon container */
		w->icon = gtk_event_box_new();

		/* set container invisible */
		gtk_event_box_set_visible_window(GTK_EVENT_BOX(w->icon), FALSE);

		/* set weather data as default */
		gtk_container_add(GTK_CONTAINER(w->icon), gtk_image_new_from_file(WEATHER_DEF_ICON));

		/* widget text */

		/* widget position */
		cfg_get_pos_by_cfg(c, &pos);

		gtk_fixed_put(GTK_FIXED(container), w->icon, pos.x, pos.y);

		w->text = gtk_label_new(NULL);

		gtk_label_set_label(GTK_LABEL(w->text), WEATHER_DEF_TEXT);

		char format[512];

		snprintf(format, sizeof format, WEATHER_FMT_TEXT, WEATHER_DEF_TMP, WEATHER_DEF_TEXT);

		gtk_label_set_markup(GTK_LABEL(w->text), format);

		gtk_fixed_put(GTK_FIXED(container), w->text, pos.x + 40, pos.y + 10);

		*(days + idx) = w;

		*(days + idx + 1) = NULL;

		c = *++days_cfg;

		++idx;
	}

	free(ptr);

	g_timeout_add(5 * 60 * 1000, (GSourceFunc)weather_refresh, NULL);

	weather_refresh();

	ui_set_transparent(window, NULL, NULL);

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

static struct weather_info *weather_query(struct weather_info *info, unsigned int day, xmlDoc *doc)
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

		strncpy(info->name, "Today", sizeof info->name);
	}
	else
	{
		node = weather_query_node(root, "day", "d", day);

		strncpy(info->name, (char *)xmlGetProp(node, (const xmlChar *)"t"), sizeof info->name);
	}

	/* get temperature */
	xmlNode *temp = weather_query_node(node, day == 0 ? "tmp" : "low", NULL, 0);
	info->temperature = atoi((char *)xmlNodeListGetString(doc, temp->xmlChildrenNode, 1));

	/* get temperature icon */
	xmlNode *icon = weather_query_node(node, "icon", NULL, 0);
	snprintf(info->icon, sizeof info->icon, "%s.png", xmlNodeListGetString(doc, icon->xmlChildrenNode, 1));

	return info;
}

static void weather_refresh()
{
	GtkWidget *old;
	GtkWidget *new;

	xmlDoc *doc = weather_load();
	char format[512] = { 0 };

	for(struct weather **list = days, *day = *list; day;)
	{
		/* query weather info by day id */
		weather_query(day->day_info, day->day_id, doc);
		
		old = gtk_container_get_children(GTK_CONTAINER(day->icon))->data;
		new = gtk_image_new_from_file(day->day_info->icon);

		gtk_container_remove(GTK_CONTAINER(day->icon), old);
		gtk_container_add(GTK_CONTAINER(day->icon), new);

		snprintf(format, sizeof format, WEATHER_FMT_TEXT, day->day_info->temperature, day->day_info->name);

		gtk_label_set_markup(GTK_LABEL(day->text), format);

		gtk_widget_show(new);

		day = *++list;
	}

	xmlFreeDoc(doc);
}

static xmlDoc *weather_load()
{
	char *zipcode, *url;
	FILE *fp;
	CURL *curl;
	xmlDoc *doc = NULL;

	zipcode = cfg_get_zipcode();

	url = calloc(strlen(WEATHER_API) + strlen(zipcode) + 1, 1);

	sprintf(url, WEATHER_API, zipcode);

	remove(WEATHER_XML);

	curl = curl_easy_init();
	
	if(curl)
	{
		fp = fopen(WEATHER_XML, "w+");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

		curl_easy_perform(curl);

		curl_easy_cleanup(curl);

		fclose(fp);

		doc = xmlReadFile(WEATHER_XML, 0, 0);
	}

	free(zipcode);
	free(url);

	return doc;
}

static int ui_set_transparent(GtkWidget *widget, GdkScreen *old_screen, void *data)
{
	GdkScreen *screen = gtk_widget_get_screen(widget);
	GdkColormap *colormap = gdk_screen_get_rgba_colormap(screen);

	if(!colormap)
	{
		return FALSE;
	}

	gtk_widget_set_colormap(widget, colormap);

	g_signal_connect(widget, "screen_changed", G_CALLBACK(ui_set_transparent), NULL);
	g_signal_connect(widget, "expose_event", G_CALLBACK(ui_do_transparent), NULL);

	return FALSE;
}

/* 
 * draw the transparent before show widget 
 * */
static int ui_do_transparent(GtkWidget *widget, GdkEventExpose *event, void *data)
{
   	cairo_t *cr = gdk_cairo_create(widget->window);

	/* transparent */
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.0);

	/* draw the background */
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);

	cairo_destroy(cr);

	return FALSE;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t num_bytes;

	num_bytes = fwrite(ptr, size, nmemb, stream);

	return num_bytes;
}

