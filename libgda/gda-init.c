/* GDA Library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib/gmain.h>
#include <gmodule.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>

static GMainLoop *main_loop = NULL;

/* global variables */
GdaDict        *default_dict = NULL; /* available in all libgda, anways NOT NULL */
GdaDataHandler *default_handlers [GDA_VALUE_TYPE_UNKNOWN]; /* to be used by providers, not GdaDict because of locale issues */


/**
 * gda_init
 * @app_id: name of the program.
 * @version: revision number of the program.
 * @nargs: number of arguments, usually argc from main().
 * @args: list of arguments, usually argv from main().
 * 
 * Initializes the GDA library. 
 */
void
gda_init (const gchar *app_id, const gchar *version, gint nargs, gchar *args[])
{
	static gboolean initialized = FALSE;

	if (initialized) {
		gda_log_error (_("Attempt to initialize an already initialized client"));
		return;
	}

	bindtextdomain (GETTEXT_PACKAGE, LIBGDA_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* Threading support if possible */
#ifdef G_THREADS_ENABLED
#ifndef G_THREADS_IMPL_NONE
	if (! g_thread_supported ())
		g_thread_init (NULL);
#endif
#endif
	g_type_init ();
	g_set_prgname (app_id);

	if (!g_module_supported ())
		g_error (_("libgda needs GModule. Finishing..."));

	/* create a default dictionary */
	default_dict = GDA_DICT (gda_dict_new ());

	/* fill the default data handlers */
	default_handlers [GDA_VALUE_TYPE_NULL] = NULL;
	default_handlers [GDA_VALUE_TYPE_BIGINT] = gda_handler_numerical_new ();
	default_handlers [GDA_VALUE_TYPE_BIGUINT] = gda_handler_numerical_new ();
	default_handlers [GDA_VALUE_TYPE_BINARY] = gda_handler_bin_new ();
	default_handlers [GDA_VALUE_TYPE_BLOB] = gda_handler_bin_new ();
	default_handlers [GDA_VALUE_TYPE_BOOLEAN] = gda_handler_boolean_new ();
	default_handlers [GDA_VALUE_TYPE_DATE] = gda_handler_time_new_no_locale ();
	default_handlers [GDA_VALUE_TYPE_DOUBLE] = gda_handler_numerical_new ();
	default_handlers [GDA_VALUE_TYPE_GEOMETRIC_POINT] = NULL;
	default_handlers [GDA_VALUE_TYPE_GOBJECT] = NULL;
	default_handlers [GDA_VALUE_TYPE_INTEGER] = gda_handler_numerical_new ();
	default_handlers [GDA_VALUE_TYPE_LIST] = NULL;
	default_handlers [GDA_VALUE_TYPE_MONEY] = NULL;
	default_handlers [GDA_VALUE_TYPE_NUMERIC] = gda_handler_numerical_new ();
	default_handlers [GDA_VALUE_TYPE_SINGLE] = gda_handler_numerical_new ();
	default_handlers [GDA_VALUE_TYPE_SMALLINT] = gda_handler_numerical_new ();
	default_handlers [GDA_VALUE_TYPE_SMALLUINT] = gda_handler_numerical_new ();
	default_handlers [GDA_VALUE_TYPE_STRING] = gda_handler_string_new ();
	default_handlers [GDA_VALUE_TYPE_TIME] = gda_handler_time_new_no_locale ();
	default_handlers [GDA_VALUE_TYPE_TIMESTAMP] = gda_handler_time_new_no_locale ();
	default_handlers [GDA_VALUE_TYPE_TINYINT] = gda_handler_numerical_new ();
	default_handlers [GDA_VALUE_TYPE_TINYUINT] = gda_handler_numerical_new ();
	default_handlers [GDA_VALUE_TYPE_TYPE] = gda_handler_type_new ();
	default_handlers [GDA_VALUE_TYPE_UINTEGER] = gda_handler_numerical_new ();	

	initialized = TRUE;
}

/**
 * gda_get_default_dict
 *
 * Get the default dictionary.
 *
 * Returns: a not %NULL pointer to the default #GdaDict dictionary
 */
GdaDict *
gda_get_default_dict (void)
{
        return default_dict;
}

typedef struct {
	GdaInitFunc init_func;
	gpointer user_data;
} InitCbData;

static gboolean
idle_cb (gpointer user_data)
{
	InitCbData *cb_data = (InitCbData *) user_data;

	g_return_val_if_fail (cb_data != NULL, FALSE);

	if (cb_data->init_func)
		cb_data->init_func (cb_data->user_data);

	g_free (cb_data);

	return FALSE;
}

/**
 * gda_main_run
 * @init_func: function to be called when everything has been initialized.
 * @user_data: data to be passed to the init function.
 *
 * Runs the GDA main loop, which is nothing more than the Bonobo main
 * loop, but with internally added stuff specific for applications using
 * libgda.
 *
 * You can specify a function to be called after everything has been correctly
 * initialized (that is, for initializing your own stuff).
 */
void
gda_main_run (GdaInitFunc init_func, gpointer user_data)
{
	if (main_loop)
		return;

	if (init_func) {
		InitCbData *cb_data;

		cb_data = g_new (InitCbData, 1);
		cb_data->init_func = init_func;
		cb_data->user_data = user_data;

		g_idle_add ((GSourceFunc) idle_cb, cb_data);
	}

	main_loop = g_main_loop_new (g_main_context_default (), FALSE);
	g_main_loop_run (main_loop);
}

/**
 * gda_main_quit
 * 
 * Exits the main loop.
 */
void
gda_main_quit (void)
{
	g_main_loop_quit (main_loop);
	g_main_loop_unref (main_loop);

	main_loop = NULL;
}
