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
#include <ctype.h>

#define CFGNAME 					"myweather/weather.rc"
#define WEATHER_LOG 					"/tmp/weather.log"
#define WEATHER_API 					"http://weather.yahooapis.com/forecastrss?w=%s&u=c"

#define WEATHER_RES 					"myweather/icons"
#define WEATHER_DEF_ICON 				"na.png"
#define WEATHER_DEF_TEXT 				"---"
#define WEATHER_FMT_TEXT 				"<span foreground='%s' font_desc='%s'>%d ~ %d°C %s</span>"

struct location
{
	char city[64];
	char region[64];
	char country[64];
};

struct forecast
{
	char day[32];
	char date[32];
	char text[32];

	int low;
	int high;
	int code;

	GtkWidget *icon;
	GtkWidget *display;

	struct forecast *next;
};

struct app
{
	struct location *location;
	struct forecast *forecast;
};

static void weather_refresh();

static void weather_load();

static void weather_init();

static int ui_set_transparent(GtkWidget *widget, GdkScreen *ignore, void *data);

static int ui_do_transparent(GtkWidget *widget, GdkEventExpose *event, void *data);

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);

static struct cfg position 	= { .name = "position", .value = "0, 0" 	};
static struct cfg width 	= { .name = "width", 	.value = "200" 		};
static struct cfg height 	= { .name = "height", 	.value = "320" 		};
static struct cfg color 	= { .name = "color", 	.value = "white" 	};
static struct cfg font 		= { .name = "font", 	.value = "nu.se 7" };
static struct cfg zipcode  	= { .name = "zipcode", 	.value = "" 		};
static struct cfg days 		= { .name = "days", 	.value = "" 		};

static struct cfg *rc[] = 
{
	&position, &width, &height, &height, &zipcode, &days, NULL
};

static struct app app;

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

	stdout = freopen(WEATHER_LOG, "a+", stdout);
	stderr = freopen(WEATHER_LOG, "a+", stderr);

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

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

	struct position pos = { 0, 0 };
	struct forecast *last = NULL;

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
	mkpos(position.value, &pos);
	gtk_widget_set_uposition(window, pos.x, pos.y);

	container = gtk_fixed_new();

	gtk_container_add(GTK_CONTAINER(window), container);

	/* build the forecast */
	for(char *s = days.value; *s;)
	{
		struct forecast *forecast = calloc( 1, sizeof *forecast );

		mkpos(s, &pos);

		forecast->icon = gtk_event_box_new();

		/* set container invisible */
		gtk_event_box_set_visible_window(GTK_EVENT_BOX(forecast->icon), FALSE);

		/* set weather data as default */
		gtk_container_add(GTK_CONTAINER(forecast->icon), gtk_image_new_from_file(WEATHER_DEF_ICON));

		/* set position */
		gtk_fixed_put(GTK_FIXED(container), forecast->icon, pos.x, pos.y);

		forecast->display = gtk_label_new(NULL);

		gtk_label_set_label(GTK_LABEL(forecast->display), WEATHER_DEF_TEXT);

		gtk_fixed_put(GTK_FIXED(container), forecast->display, pos.x + 40, pos.y + 10);

		forecast->next = NULL;

		if ( !app.forecast )
		{
			last = app.forecast = forecast;
		} else
			last = last->next = forecast;

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

static void weather_refresh()
{
	GtkWidget *old;
	GtkWidget *new;

	char format[512] = { 0 }, icon[32] = { 0 };

	weather_load();

	for(struct forecast *last = app.forecast; last;)
	{
		printf("CODE: %d, DAY: %s, TEMP: %d, ICON: %s\n", last->code, last->date, last->high, last->day);
		
		snprintf(format, sizeof format, WEATHER_FMT_TEXT, color.value, font.value, last->low, last->high, last->day);
		snprintf(icon, sizeof icon, "%d.png", last->code);

		old = gtk_container_get_children(GTK_CONTAINER(last->icon))->data;
		new = gtk_image_new_from_file(icon);

		gtk_container_remove(GTK_CONTAINER(last->icon), old);
		gtk_container_add(GTK_CONTAINER(last->icon), new);


		gtk_label_set_markup(GTK_LABEL(last->display), format);

		gtk_widget_show(new);

		last = last->next;
	}
}

static void weather_load()
{
	char url[1024], *ptr, *line = NULL, key[16] = { 0 }, value[32] = { 0 };
	size_t size;

	FILE *fp;
	CURL *curl;

	struct forecast *walk = app.forecast;

	sprintf(url, WEATHER_API, zipcode.value);

	curl = curl_easy_init();
	
	if(curl)
	{
		fp = tmpfile();
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

		curl_easy_perform(curl);

		curl_easy_cleanup(curl);

		fseek( fp, SEEK_SET, 0 );

		while ( !feof( fp ) )
		{
			if ( getline( &line, &size, fp ) )
			{
				ptr = line;

				printf( "Parse line:%s", line );

				/* Get the location info */
				if ( ptr = strstr( line, "yweather:location" ), ptr && !app.location )
				{
					app.location = calloc( 1, sizeof( *(app.location) ) );

					while ( ptr = strstr( ptr, " " ), ptr )
					{
						/* Trim the blank */
						while ( isspace( *ptr++ ) || ( --ptr && 0 ) );

						memset( key, 0, 16 );
						memset( value, 0, 32 );

						if ( 2 == sscanf( ptr, "%15[^ =\t] = \"%31[^\r\n\"]", key, value ) )
						{
							if ( EQUAL( "city", key ) )
							{
								strncpy( app.location->city, value, strlen( value ) );
							} else if ( EQUAL( "region", key ) )
							{
								strncpy( app.location->region, value, strlen( value ) );
							} else if ( EQUAL( "country", key ) )
							{
								strncpy( app.location->country, value, strlen( value ) );
							}
						}
					}

				} else if ( ptr = strstr( line, "yweather:forecast" ), ptr && walk )
				{
					/* Get the forecast */
					while ( ptr = strstr( ptr, " " ), ptr )
					{
						while ( isspace( *ptr++ ) || ( --ptr && 0 ) );

						if ( 2 == sscanf( ptr, "%15[^ =\t] = \"%31[^\r\n\"]", key, value ) )
						{
							printf( "Key:%s, Value:%s, %d\n", key, value, walk == NULL );

							if ( EQUAL( "day", key ) )
							{
								strncpy( walk->day, value, strlen( value ) );
							} else if ( EQUAL( "date", key ) )
							{
								strncpy( walk->date, value, strlen( value ) );
							} else if ( EQUAL( "low", key ) )
							{
								walk->low = atoi( value );
							} else if ( EQUAL( "high", key ) )
							{
								walk->high = atoi( value );
							} else if ( EQUAL( "text", key ) )
							{
								strncpy( walk->text, value, strlen( value ) );
							} else if ( EQUAL( "code", key ) )
							{
								walk->code = atoi( value );
							}
						}
					}

					walk = walk->next;
				}


				free( line );

				line = NULL;
				size = 0;
			}
		}

		fclose(fp);
	}

	printf( "Parse forecast is Ok!\n" );
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

