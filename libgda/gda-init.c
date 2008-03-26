/* GDA Library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
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

#include <libgda/binreloc/gda-binreloc.h>
#include <sql-parser/gda-sql-parser.h>

static GStaticRecMutex gda_mutex = G_STATIC_REC_MUTEX_INIT;
#define GDA_LOCK() g_static_rec_mutex_lock(&gda_mutex)
#define GDA_UNLOCK() g_static_rec_mutex_unlock(&gda_mutex)

static GMainLoop *main_loop = NULL;

/* global variables */
xmlDtdPtr       gda_array_dtd = NULL;
xmlDtdPtr       gda_paramlist_dtd = NULL;
xmlDtdPtr       gda_server_op_dtd = NULL;


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
	GType type;
	gchar *file;

	if (initialized) {
		gda_log_error (_("Attempt to re-initialize GDA library. ignored."));
		return;
	}

	file = gda_gbr_get_file_path (GDA_LOCALE_DIR, NULL);
	bindtextdomain (GETTEXT_PACKAGE, file);
	g_free (file);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* Threading support if possible */
	if (!getenv ("LIBGDA_NO_THREADS")) {
#ifdef G_THREADS_ENABLED
#ifndef G_THREADS_IMPL_NONE
		if (! g_thread_supported ())
			g_thread_init (NULL);
#endif
#endif
	}

	g_type_init ();
	g_set_prgname (app_id);

	if (!g_module_supported ())
		g_error (_("libgda needs GModule. Finishing..."));

	/* create the required Gda types */
	type = G_TYPE_DATE;
	g_assert (type);
	type = GDA_TYPE_BINARY;
	g_assert (type);
	type = GDA_TYPE_BLOB;
	g_assert (type);
	type = GDA_TYPE_GEOMETRIC_POINT;
	g_assert (type);
	type = GDA_TYPE_LIST;
	g_assert (type);
	type = GDA_TYPE_NUMERIC;
	g_assert (type);
	type = GDA_TYPE_SHORT;
	g_assert (type);
	type = GDA_TYPE_USHORT;
	g_assert (type);
	type = GDA_TYPE_TIME;
	g_assert (type);
	type = G_TYPE_DATE;
	g_assert (type);
	type = GDA_TYPE_TIMESTAMP;
	g_assert (type);

	/* binreloc */
	gda_gbr_init ();

	/* array DTD */
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-array.dtd", NULL);
	gda_array_dtd = xmlParseDTD (NULL, (xmlChar*)file);
	if (!gda_array_dtd) {
		if (g_getenv ("GDA_TOP_SRC_DIR")) {
			g_free (file);
			file = g_build_filename (g_getenv ("GDA_TOP_SRC_DIR"), "libgda", "libgda-array.dtd", NULL);
			gda_array_dtd = xmlParseDTD (NULL, (xmlChar*)file);
		}
		if (!gda_array_dtd)
			g_message (_("Could not parse '%s': "
				     "XML data import validation will not be performed (some weird errors may occur)"),
				   file);
	}
	if (gda_array_dtd)
		gda_array_dtd->name = xmlStrdup((xmlChar*) "gda_array");
	g_free (file);

	/* paramlist DTD */
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-paramlist.dtd", NULL);
	gda_paramlist_dtd = xmlParseDTD (NULL, (xmlChar*)file);
	if (!gda_paramlist_dtd) {
		if (g_getenv ("GDA_TOP_SRC_DIR")) {
			g_free (file);
			file = g_build_filename (g_getenv ("GDA_TOP_SRC_DIR"), "libgda", "libgda-paramlist.dtd", NULL);
			gda_paramlist_dtd = xmlParseDTD (NULL, (xmlChar*)file);
		}
		if (!gda_paramlist_dtd)
			g_message (_("Could not parse '%s': "
				     "XML data import validation will not be performed (some weird errors may occur)"),
				   file);
	}
	if (gda_paramlist_dtd)
		gda_paramlist_dtd->name = xmlStrdup((xmlChar*) "data-set-spec");
	g_free (file);

	/* server operation DTD */
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-server-operation.dtd", NULL);
	gda_server_op_dtd = xmlParseDTD (NULL, (xmlChar*)file);
	if (!gda_server_op_dtd) {
		if (g_getenv ("GDA_TOP_SRC_DIR")) {
			g_free (file);
			file = g_build_filename (g_getenv ("GDA_TOP_SRC_DIR"), "libgda", "libgda-server-operation.dtd", NULL);
			gda_server_op_dtd = xmlParseDTD (NULL, (xmlChar*)file);
		}
		if (!gda_server_op_dtd)
			g_message (_("Could not parse '%s': "
				     "Validation for XML files for server operations will not be performed (some weird errors may occur)"),
				   file);
	}

	if (gda_server_op_dtd)
		gda_server_op_dtd->name = xmlStrdup((xmlChar*) "serv_op");
	g_free (file);

	initialized = TRUE;
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
 * Runs the GDA main loop, which is nothing more than the glib main
 * loop, but with internally added stuff specific for applications using
 * libgda.
 *
 * You can specify a function to be called after everything has been correctly
 * initialized (that is, for initializing your own stuff).
 */
void
gda_main_run (GdaInitFunc init_func, gpointer user_data)
{
	GDA_LOCK ();
	if (main_loop) {
		GDA_UNLOCK ();
		return;
	}

	if (init_func) {
		InitCbData *cb_data;

		cb_data = g_new (InitCbData, 1);
		cb_data->init_func = init_func;
		cb_data->user_data = user_data;

		g_idle_add ((GSourceFunc) idle_cb, cb_data);
	}

	main_loop = g_main_loop_new (g_main_context_default (), FALSE);
	g_main_loop_run (main_loop);
	GDA_UNLOCK ();
}

/**
 * gda_main_quit
 * 
 * Exits the main loop.
 */
void
gda_main_quit (void)
{
	GDA_LOCK ();
	g_main_loop_quit (main_loop);
	g_main_loop_unref (main_loop);

	main_loop = NULL;
	GDA_UNLOCK ();
}

/*
 * Convenient Functions
 */


/* module error */
GQuark gda_general_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_general_error");
	return quark;
}
