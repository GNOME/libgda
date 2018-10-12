/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 Xabier Rodriguez Calvar <xrcalvar@igalia.com>
 * Copyright (C) 2002 Zbigniew Chyla <cyba@gnome.pl>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2006 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include <gmodule.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <libgda/binreloc/gda-binreloc.h>
#include <sql-parser/gda-sql-parser.h>

/*
 * global variables
 *
 * REM: to use them from another Windows DLL, they have to be declared like:
 *     __declspec(dllimport) extern gchar *gda_numeric_locale;
 *
 * Better yet is to define the IMPORT macro as:
 *
 *     #ifdef G_OS_WIN32
 *       #define IMPORT __declspec(dllimport)
 *     #else
 *       #define IMPORT
 *     #endif
 *     IMPORT extern gchar *gda_numeric_locale;
 */
xmlDtdPtr       _gda_array_dtd = NULL;
xmlDtdPtr       gda_paramlist_dtd = NULL;
xmlDtdPtr       _gda_server_op_dtd = NULL;
xmlDtdPtr		_gda_ddl_creator_dtd = NULL;

static gboolean numeric_locale_dyn = FALSE;
gchar          *gda_numeric_locale = "";
static gboolean lang_locale_dyn = FALSE;
gchar          *gda_lang_locale = "";

/**
 * gda_locale_changed:
 *
 * Call this function whenever the setlocale() function has been called
 * to change the current locale; this function is first called by gda_init() so you
 * don't need to call it if you have set the locale before calling gda_init().
 *
 * Failing to call this function after having changed the current locale may result
 * in Libgda reverting to the previous set locale.
 *
 * Since: 4.2.3
 */
void
gda_locale_changed (void)
{
	/* free previous setting */
	if (numeric_locale_dyn)
		g_free (gda_numeric_locale);
	if (lang_locale_dyn)
		g_free (gda_lang_locale);

	/* net new settings */
	gda_numeric_locale = setlocale (LC_NUMERIC, NULL);
	if (gda_numeric_locale) {
		numeric_locale_dyn = TRUE;
		gda_numeric_locale = g_strdup (gda_numeric_locale);
	}
	else {
		numeric_locale_dyn = FALSE;
		gda_numeric_locale = "";
	}
#ifdef HAVE_LC_MESSAGES
        gda_lang_locale = setlocale (LC_MESSAGES, NULL);
#else
        gda_lang_locale = setlocale (LC_CTYPE, NULL);
#endif
	if (gda_lang_locale) {
		lang_locale_dyn = TRUE;
		gda_lang_locale = g_strdup (gda_lang_locale);
	}
	else {
		lang_locale_dyn = FALSE;
		gda_lang_locale = "";
	}
}

/**
 * gda_init:
 *
 * Initializes the GDA library, must be called prior to any Libgda usage.
 *
 * Please note that if you call setlocale() to modify the current locale, you should also
 * call gda_locale_changed() before using Libgda again.
 */
void
gda_init (void)
{
	static GMutex init_mutex;
	static gboolean initialized = FALSE;

	GType type;
	gchar *file;

	g_mutex_lock (&init_mutex);
	if (initialized) {
		g_mutex_unlock (&init_mutex);
		gda_log_error (_("Ignoring attempt to re-initialize GDA library."));
		return;
	}

	file = gda_gbr_get_file_path (GDA_LOCALE_DIR, NULL);
	bindtextdomain (GETTEXT_PACKAGE, file);
	g_free (file);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

#if GLIB_CHECK_VERSION(2,36,0)
#else
	g_type_init ();
#endif

	if (!g_module_supported ())
		g_error (_("libgda needs GModule. Finishing..."));

	/* create the required Gda types, to avoid multiple simultaneous type creation when
	 * multi threads are used */
	type = GDA_TYPE_NULL;
	g_assert (type);
	type = G_TYPE_DATE;
	g_assert (type);
	type = GDA_TYPE_BINARY;
	g_assert (type);
	type = GDA_TYPE_BLOB;
	g_assert (type);
	type = GDA_TYPE_GEOMETRIC_POINT;
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
	type = G_TYPE_DATE_TIME;
	g_assert (type);
	type = G_TYPE_ERROR;
	g_assert (type);

	/* force TZ init */
	tzset ();

	/* acquire locale */
	gda_locale_changed ();

	/* binreloc */
	gda_gbr_init ();

	/* array DTD */
	_gda_array_dtd = NULL;
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-array.dtd", NULL);
	if (g_file_test (file, G_FILE_TEST_EXISTS))
		_gda_array_dtd = xmlParseDTD (NULL, (xmlChar*)file);

	if (!_gda_array_dtd) {
		if (g_getenv ("GDA_TOP_SRC_DIR")) {
			g_free (file);
			file = g_build_filename (g_getenv ("GDA_TOP_SRC_DIR"), "libgda", "libgda-array.dtd", NULL);
			_gda_array_dtd = xmlParseDTD (NULL, (xmlChar*)file);
		}
		if (!_gda_array_dtd)
			g_message (_("Could not parse '%s': "
				     "XML data import validation will not be performed (some weird errors may occur)"),
				   file);
	}
	if (_gda_array_dtd)
		_gda_array_dtd->name = xmlStrdup((xmlChar*) "gda_array");
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
	_gda_server_op_dtd = NULL;
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-server-operation.dtd", NULL);
	if (g_file_test (file, G_FILE_TEST_EXISTS))
		_gda_server_op_dtd = xmlParseDTD (NULL, (xmlChar*)file);

	if (!_gda_server_op_dtd) {
		if (g_getenv ("GDA_TOP_SRC_DIR")) {
			g_free (file);
			file = g_build_filename (g_getenv ("GDA_TOP_SRC_DIR"), "libgda", "libgda-server-operation.dtd", NULL);
			_gda_server_op_dtd = xmlParseDTD (NULL, (xmlChar*)file);
		}
		if (!_gda_server_op_dtd)
			g_message (_("Could not parse '%s': "
				     "Validation for XML files for server operations will not be performed (some weird errors may occur)"),
				   file);
	}

	if (_gda_server_op_dtd)
		_gda_server_op_dtd->name = xmlStrdup((xmlChar*) "serv_op");
	g_free (file);

  /* GdaDdlCreator DTD */
  _gda_ddl_creator_dtd = NULL;
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd",
                                "libgda-ddl-creator.dtd", NULL);
	if (g_file_test (file, G_FILE_TEST_EXISTS))
		_gda_ddl_creator_dtd = xmlParseDTD (NULL, (xmlChar*)file);
  else
    {
	    if (g_getenv ("GDA_TOP_SRC_DIR"))
        {
          g_free (file);
          file = g_build_filename (g_getenv ("GDA_TOP_SRC_DIR"), "libgda",
                                   "libgda-ddl-creator.dtd", NULL);
          _gda_ddl_creator_dtd = xmlParseDTD (NULL, (xmlChar*)file);
		    }
	  }

  if (!_gda_ddl_creator_dtd)
	  g_message (_("Could not parse '%s': "
                 "Validation for XML files for GdaDdlCreator will not be performed (some weird errors may occur)"),
               file);
  else
		_gda_ddl_creator_dtd->name = xmlStrdup((xmlChar*) "ddl-creator");

	g_free (file);

  /* Initializing global GdaConfig */
  GdaConfig *config = gda_config_get ();
  g_assert (config != NULL);
  g_object_unref (config);

  initialized = TRUE;
	g_mutex_unlock (&init_mutex);
}

/**
 * gda_get_application_exec_path:
 * @app_name: the name of the application to find
 *
 * Find the path to the application identified by @app_name. For example if the application
 * is "gda-sql", then calling this function will return
 * "/your/prefix/bin/gda-sql-6.0" if Libgda is installed in
 * the "/your/prefix" prefix (which would usually be "/usr"), and for the ABI version 5.0.
 *
 * Returns: (transfer full): the path as a new string, or %NULL if the application cannot be found
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
