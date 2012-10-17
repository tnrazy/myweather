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

#define CFGNAME 					"myweather/weather.rc"
#define WEATHER_XML 					"/tmp/weather.xml"
#define WEATHER_API 					"http://xml.weather.com/weather/local/%s?cc=*&unit=m&dayf=6"

#define WEATHER_RES 					"myweather/icons"
#define WEATHER_DEF_ICON 				"na.png"
#define WEATHER_DEF_TEXT 				"---"
#define WEATHER_DEF_TMP 				0
#define WEATHER_FMT_TEXT 				"<span foreground='white' font_desc='nu.se 6'>%d°C %s</span>"

struct info
{
	char name[16];
	char icon[16];

	int temperature;
};

struct weather
{
	struct info *info;

	GtkWidget *icon;
	GtkWidget *text;

	unsigned int id;
	
	struct weather *next;
};

static void weather_refresh();

static xmlDoc *weather_load();

static struct info *weather_query(struct info *info, unsigned int day, xmlDoc *doc);

static void weather_init();

static int ui_set_transparent(GtkWidget *widget, GdkScreen *ignore, void *data);

static int ui_do_transparent(GtkWidget *widget, GdkEventExpose *event, void *data);

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);

static struct cfg position 	= { .name = "position", .value = "0, 0" };
static struct cfg width 	= { .name = "width", 	.value = "200" 	};
static struct cfg height 	= { .name = "height", 	.value = "320" 	};
static struct cfg zipcode  	= { .name = "zipcode", 	.value = "" 	};
static struct cfg days 		= { .name = "days", 	.value = "" 	};

static struct cfg *rc[] = 
{
	&position, &width, &height, &height, &zipcode, &days, NULL
};

static struct weather *weathers;

int main(int argc, const char **argv)
{
	char filename[FILENAME_MAX] = { 0 };
	char resdir[FILENAME_MAX] = { 0 };

	if(argc >= 2)
	{
		fprintf(stderr, "Just type '%s', = =\n", *argv);
		exit(EXIT_SUCCESS);
	}

	snprintf(filename, FILENAME_MAX, "%s/%s", getenv("XDG_CONFIG_HOME"), CFGNAME);
	snprintf(resdir, FILENAME_MAX, "%s/%s", getenv("XDG_CONFIG_HOME"), WEATHER_RES);

	cfg_load(filename, (struct cfg **)&rc);

	if(-1 == access(resdir, F_OK))
	{
		die("Weather resource directory '%s' is not exists", resdir);
	}

	if(-1 == access(resdir, R_OK))
	{
		die("Weather resource directory '%s' access denined", resdir);
	}

	if(-1 == chdir(resdir))
	{
		die("Failed to change working directory: %s", strerror(errno));
	}

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

	weather_init();

	return EXIT_SUCCESS;
}

/* 
 * load ui
 * */
static void weather_init()
{
	GtkWidget *window;
	GtkWidget *container;

	struct position app_pos = { 0, 0 }, pos = { 0, 0 };
	struct weather *last = NULL;

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
	mkpos(position.value, &app_pos);
	gtk_widget_set_uposition(window, app_pos.x, app_pos.y);

	container = gtk_fixed_new();

	gtk_container_add(GTK_CONTAINER(window), container);

	/* get all days position */
	register int idx = 0;

	for(char *s = days.value; *s;)
	{
		struct weather *w = calloc(sizeof *w, 1);

		mkpos(s, &pos);

		w->icon = gtk_event_box_new();

		/* set container invisible */
		gtk_event_box_set_visible_window(GTK_EVENT_BOX(w->icon), FALSE);

		/* set weather data as default */
		gtk_container_add(GTK_CONTAINER(w->icon), gtk_image_new_from_file(WEATHER_DEF_ICON));

		/* set position */
		gtk_fixed_put(GTK_FIXED(container), w->icon, pos.x, pos.y);

		w->text = gtk_label_new(NULL);

		gtk_label_set_label(GTK_LABEL(w->text), WEATHER_DEF_TEXT);

		gtk_fixed_put(GTK_FIXED(container), w->text, pos.x + 40, pos.y + 10);

		w->id = idx;
		w->info = calloc(sizeof *(w->info), 1);
		w->next = NULL;

		if(NULL == last)
		{
			weathers = w;
			last = w;
		}
		else
		{
			last->next = w;
			last = w;
		}

		++idx;

		s = strchr(s, ';');

		if(NULL == s)
		{
			break;
		}

		/* skip ';' */
		++s;
	}

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

static struct info *weather_query(struct info *info, unsigned int id, xmlDoc *doc)
{
	xmlNode *root, *node = NULL;	
	
	root = xmlDocGetRootElement(doc);

	if(root == NULL)
	{
		return NULL;
	}

	if(xmlStrcmp(root->name, (const xmlChar *)"weather"))
	{
		return NULL;
	}

	/* get weather xml node */
	if(0 == id)
	{
		node = weather_query_node(root, "cc", NULL, 0);

		strncpy(info->name, "Today", sizeof info->name);
	}
	else
	{
		node = weather_query_node(root, "day", "d", id);

		strncpy(info->name, (char *)xmlGetProp(node, (const xmlChar *)"t"), sizeof info->name);
	}

	/* get temperature */
	xmlNode *temp = weather_query_node(node, 0 == id ? "tmp" : "low", NULL, 0);
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

	for(struct weather *last = weathers; last;)
	{
		printf("ID: %d, ICON: %s, TEMP: %s\n", last->id, last->info->name, last->info->icon);

		/* query weather info by day id */
		if(NULL == weather_query(last->info, last->id, doc))
		{
			continue;
		}
		
		old = gtk_container_get_children(GTK_CONTAINER(last->icon))->data;
		new = gtk_image_new_from_file(last->info->icon);

		gtk_container_remove(GTK_CONTAINER(last->icon), old);
		gtk_container_add(GTK_CONTAINER(last->icon), new);

		snprintf(format, sizeof format, WEATHER_FMT_TEXT, last->info->temperature, last->info->name);

		gtk_label_set_markup(GTK_LABEL(last->text), format);

		gtk_widget_show(new);

		last = last->next;
	}

	xmlFreeDoc(doc);
}

static xmlDoc *weather_load()
{
	char url[1024];
	FILE *fp;
	CURL *curl;
	xmlDoc *doc = NULL;

	sprintf(url, WEATHER_API, zipcode.value);

	unlink(WEATHER_XML);

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

	return doc;
}

static int ui_set_transparent(GtkWidget *widget, GdkScreen *ignore, void *data)
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

