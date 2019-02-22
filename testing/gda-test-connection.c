/*
 * Copyright (C) 2006 - 2016 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2016 Ting-Wei Lan <lantw44@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#ifdef HAVE_LOCALE_H
  #include <locale.h>
#endif

/* options */
gchar *pass = NULL;
gchar *user = NULL;
gchar *dsn = NULL;
gchar *direct = NULL;
gchar *prov = NULL;

static GOptionEntry entries[] = {
	{ "cnc", 'c', 0, G_OPTION_ARG_STRING, &direct, "Direct connection string", NULL},
	{ "provider", 'p', 0, G_OPTION_ARG_STRING, &prov, "Provider name", NULL},
	{ "dsn", 's', 0, G_OPTION_ARG_STRING, &dsn, "Data source", NULL},
	{ "user", 'U', 0, G_OPTION_ARG_STRING, &user, "Username", "username" },
	{ "password", 'P', 0, G_OPTION_ARG_STRING, &pass, "Password", "password" },
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};


int 
main (int argc, char **argv)
{
	GError *error = NULL;	
	GOptionContext *context;

	GdaConnection *cnc;
	gchar *auth_string = NULL;

#ifdef HAVE_LOCALE_H
	setlocale (LC_ALL, "");
#endif

	/* command line parsing */
	context = g_option_context_new ("Tests opening a connection");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Can't parse arguments: %s", error->message);
		exit (1);
	}
	g_option_context_free (context);
	
	if (direct && dsn) {
		g_print ("DSN and connection string are exclusive\n");
		exit (1);
	}

	if (!direct && !dsn) {
		g_print ("You must specify a connection to open either as a DSN or a connection string\n");
		exit (1);
	}

	if (direct && !prov) {
		g_print ("You must specify a provider when using a connection string\n");
		exit (1);
	}

	gda_init ();

	/* open connection */
	if (user) {
		if (pass)
			auth_string = g_strdup_printf ("USERNAME=%s;PASSWORD=%s", user, pass);
		else
			auth_string = g_strdup_printf ("USERNAME=%s", user);
	}
	if (dsn) {
		GdaDsnInfo *info = NULL;
		info = gda_config_get_dsn_info (dsn);
		if (!info)
			g_error (_("DSN '%s' is not declared"), dsn);
		else {
			cnc = gda_connection_open_from_dsn_name (info->name, auth_string ? auth_string : info->auth_string,
							    0, &error);
			if (!cnc) {
				g_warning (_("Can't open connection to DSN %s: %s\n"), info->name,
					   error && error->message ? error->message : "???");
				exit (1);
			}
		}
	}
	else {
		
		cnc = gda_connection_open_from_string (prov, direct, auth_string, 0, &error);
		if (!cnc) {
			g_warning (_("Can't open specified connection: %s\n"),
				   error && error->message ? error->message : "???");
			exit (1);
		}
	}
	g_free (auth_string);

	g_print (_("Connection successfully opened!\n"));
	if (! gda_connection_close (cnc, &error)) {
		g_warning (_("Can't close connection: %s\n"),
			   error && error->message ? error->message : "???");
		exit (1);
	}

	return 0;
}
