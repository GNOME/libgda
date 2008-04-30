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

/* global variables */
xmlDtdPtr       gda_array_dtd = NULL;
xmlDtdPtr       gda_paramlist_dtd = NULL;
xmlDtdPtr       gda_server_op_dtd = NULL;


/**
 * gda_init
 * 
 * Initializes the GDA library. Must be called prior to any Libgda usage.
 */
void
gda_init (void)
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
