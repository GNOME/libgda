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
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <libgda/binreloc/gda-binreloc.h>
#include <sql-parser/gda-sql-parser.h>
#include <libgda/gda-error.h>

/* global variables */
xmlDtdPtr       gda_array_dtd = NULL;
xmlDtdPtr       gda_paramlist_dtd = NULL;
xmlDtdPtr       gda_server_op_dtd = NULL;

gchar          *gda_numeric_locale = "";
gchar          *gda_lang_locale = "";

/**
 * gda_init
 * 
 * Initializes the GDA library, must be called prior to any Libgda usage. Note that unless the
 * LIBGDA_NO_THREADS environment variable is set (to any value), the GLib thread system will
 * be initialized as well if not yet initialized.
 */
void
gda_init (void)
{
	static GStaticMutex init_mutex = G_STATIC_MUTEX_INIT;
	static gboolean initialized = FALSE;

	GType type;
	gchar *file;

	g_static_mutex_lock (&init_mutex);
	if (initialized) {
		g_static_mutex_unlock (&init_mutex);
		gda_log_error (_("Ignoring attempt to re-initialize GDA library."));
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
	type = GDA_TYPE_ERROR;
	g_assert (type);

	/* acquire locale */
	gda_numeric_locale = setlocale (LC_NUMERIC, NULL);
	if (gda_numeric_locale)
		gda_numeric_locale = g_strdup (gda_numeric_locale);
	else
		gda_numeric_locale = g_strdup ("");
#ifdef HAVE_LC_MESSAGES
        gda_lang_locale = setlocale (LC_MESSAGES, NULL);
#else
        gda_lang_locale = setlocale (LC_CTYPE, NULL);
#endif
	if (gda_lang_locale)
		gda_lang_locale = g_strdup (gda_lang_locale);
	else
		gda_lang_locale = g_strdup ("");

	/* binreloc */
	gda_gbr_init ();

	/* array DTD */
	gda_array_dtd = NULL;
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-array.dtd", NULL);
	if (g_file_test (file, G_FILE_TEST_EXISTS))
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
	gda_paramlist_dtd = NULL;
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-paramlist.dtd", NULL);
	if (g_file_test (file, G_FILE_TEST_EXISTS))
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
	gda_server_op_dtd = NULL;
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-server-operation.dtd", NULL);
	if (g_file_test (file, G_FILE_TEST_EXISTS))
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
	g_static_mutex_unlock (&init_mutex);
}

/**
 * gda_get_application_exec_path
 * @app_name: the name of the application to find
 *
 * Find the path to the application identified by @app_name. For example if the application
 * is "gda-sql", then calling this function will return
 * "/your/prefix/bin/gda-sql-4.0" if Libgda is installed in
 * the "/your/prefix" prefix (which would usually be "/usr"), and for the ABI version 4.0.
 *
 * Returns: the path as a new string, or %NULL if the application cannot be found
 */
gchar *
gda_get_application_exec_path (const gchar *app_name)
{
        gchar *str;
        gchar *fname;
        g_return_val_if_fail (app_name, NULL);

        gda_gbr_init ();
        fname = g_strdup_printf ("%s-%s", app_name, ABI_VERSION);
        str = gda_gbr_get_file_path (GDA_BIN_DIR, fname, NULL);
        g_free (fname);
        if (!g_file_test (str, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE)) {
                g_free (str);
                str = NULL;
        }

        return str;
}
