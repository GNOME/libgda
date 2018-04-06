/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include <sql-parser/gda-sql-parser.h>
#include <libgda-ui/libgda-ui.h>
#include <libgda-ui/gdaui-plugin.h>
#include <libgda/binreloc/gda-binreloc.h>

#include "gdaui-entry-filesel.h"
#include "gdaui-entry-cidr.h"
#include "gdaui-entry-text.h"
#include "gdaui-entry-pict.h"
#include "gdaui-entry-rt.h"
#include "gdaui-data-cell-renderer-pict.h"
#include "gdaui-entry-format.h"

#ifdef HAVE_LIBGCRYPT
#include "gdaui-entry-password.h"
#include "gdaui-data-cell-renderer-password.h"
#endif

#ifdef HAVE_GTKSOURCEVIEW
  #include <gtksourceview/gtksource.h>
#endif

static GdauiDataEntry *plugin_entry_filesel_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *plugin_entry_cidr_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *plugin_entry_format_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *plugin_entry_text_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *plugin_entry_rt_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *plugin_entry_pict_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GtkCellRenderer  *plugin_cell_renderer_pict_create_func (GdaDataHandler *handler, GType type, const gchar *options);

#ifdef HAVE_LIBGCRYPT
static GdauiDataEntry *plugin_entry_password_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GtkCellRenderer  *plugin_cell_renderer_password_create_func (GdaDataHandler *handler, GType type, const gchar *options);
#endif

GSList *
plugin_init (GError **error)
{
	GdauiPlugin *plugin;
	GSList *retlist = NULL;
	gchar *file;

	/* file selector */
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "filesel";
	plugin->plugin_descr = "File selection entry";
	plugin->plugin_file = NULL; /* always leave NULL */
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_STRING;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = plugin_entry_filesel_create_func;
	plugin->cell_create_func = NULL;
	retlist = g_slist_append (retlist, plugin);
	file = gda_gbr_get_file_path (GDA_LIB_DIR, LIBGDA_ABI_NAME, "plugins", "gdaui-entry-filesel-spec.xml", NULL);
	if (! g_file_test (file, G_FILE_TEST_EXISTS)) {
		if (error && !*error)
			g_set_error (error, GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_FILE_NOT_FOUND_ERROR,
				     _("Missing spec. file '%s'"), file);
        }
	else {
		gsize len;
		g_file_get_contents (file, &(plugin->options_xml_spec), &len, error);
	}
	g_free (file);

	/* CIDR */
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "cird";
	plugin->plugin_descr = "Entry to hold an IPv4 network specification";
	plugin->plugin_file = NULL; /* always leave NULL */
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_STRING;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = plugin_entry_cidr_create_func;
	plugin->cell_create_func = NULL;
	retlist = g_slist_append (retlist, plugin);

	/* FORMAT */
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "format";
	plugin->plugin_descr = "Text entry with specific format";
	plugin->plugin_file = NULL; /* always leave NULL */
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_STRING;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = plugin_entry_format_create_func;
	plugin->cell_create_func = NULL;
	retlist = g_slist_append (retlist, plugin);
	file = gda_gbr_get_file_path (GDA_LIB_DIR, LIBGDA_ABI_NAME, "plugins", "gdaui-entry-format-spec.xml", NULL);
	if (! g_file_test (file, G_FILE_TEST_EXISTS)) {
		if (error && !*error)
			g_set_error (error, GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_FILE_NOT_FOUND_ERROR,
				     _("Missing spec. file '%s'"), file);
        }
	else {
		gsize len;
		g_file_get_contents (file, &(plugin->options_xml_spec), &len, error);
	}
	g_free (file);

#ifdef HAVE_LIBGCRYPT
	/* PASSWORD */
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "password";
	plugin->plugin_descr = "password entry";
	plugin->plugin_file = NULL; /* always leave NULL */
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_STRING;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = plugin_entry_password_create_func;
	plugin->cell_create_func = plugin_cell_renderer_password_create_func;
	retlist = g_slist_append (retlist, plugin);

	file = gda_gbr_get_file_path (GDA_LIB_DIR, LIBGDA_ABI_NAME, "plugins", "gdaui-entry-password.xml", NULL);
	if (! g_file_test (file, G_FILE_TEST_EXISTS)) {
		if (error && !*error)
			g_set_error (error,  GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_FILE_NOT_FOUND_ERROR,
				     _("Missing spec. file '%s'"), file);
        }
	else {
		gsize len;
		g_file_get_contents (file, &(plugin->options_xml_spec), &len, error);
	}
	g_free (file);
#endif
	
	/* TEXT */
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "text";
	plugin->plugin_descr = "Multiline text entry";
	plugin->plugin_file = NULL; /* always leave NULL */
	plugin->nb_g_types = 3;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_STRING;
	plugin->valid_g_types [1] = GDA_TYPE_BLOB;
	plugin->valid_g_types [2] = GDA_TYPE_BINARY;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = plugin_entry_text_create_func;
	plugin->cell_create_func = NULL;
	retlist = g_slist_append (retlist, plugin);

#ifdef HAVE_GTKSOURCEVIEW
	file = gda_gbr_get_file_path (GDA_LIB_DIR, LIBGDA_ABI_NAME, "plugins", "gdaui-entry-text-spec.xml", NULL);
	if (! g_file_test (file, G_FILE_TEST_EXISTS)) {
		if (error && !*error)
			g_set_error (error,  GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_FILE_NOT_FOUND_ERROR,
				     _("Missing spec. file '%s'"), file);
        }
	else {
		xmlDocPtr doc;
		doc = xmlParseFile (file);
		if (!doc) {
			if (error && !*error)
				g_set_error (error,  GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_FILE_NOT_FOUND_ERROR,
					     _("Missing spec. file '%s'"), file);
		}
		else {
			xmlNodePtr node;
			node = xmlDocGetRootElement (doc);
			for (node = node->children; node; node = node->next) {
				if (!strcmp ((gchar*) node->name, "sources")) {
					for (node = node->children; node; node = node->next) {
						if (!strcmp ((gchar*) node->name, "gda_array")) {
							for (node = node->children; node; node = node->next) {
								if (!strcmp ((gchar*) node->name,
									     "gda_array_data")) {
									break;
								}
							}

							break;
						}
					}
					break;
				}
			}
			GtkSourceLanguageManager *lm;
			const gchar * const * langs;
			lm = gtk_source_language_manager_get_default ();
			langs = gtk_source_language_manager_get_language_ids (lm);
			if (langs) {
				gint i;
				for (i = 0; ; i++) {
					const gchar *tmp;
					tmp = langs [i];
					if (!tmp)
						break;
					if (node) {
						xmlNodePtr row;
						row = xmlNewChild (node, NULL, BAD_CAST "gda_array_row", NULL);
						xmlNewChild (row, NULL, BAD_CAST "gda_value", BAD_CAST tmp);
						
						GtkSourceLanguage *sl;
						sl = gtk_source_language_manager_get_language (lm, tmp);
						if (sl)
							xmlNewChild (row, NULL, BAD_CAST "gda_value",
								     BAD_CAST gtk_source_language_get_name (sl));
						else
							xmlNewChild (row, NULL, BAD_CAST "gda_value", BAD_CAST tmp);
					}
				}
			}

			xmlChar *out;
			int size;
			xmlDocDumpFormatMemory (doc, &out, &size, 0);
			xmlFreeDoc (doc);
			plugin->options_xml_spec = g_strdup ((gchar*) out);
			xmlFree (out);
		}
	}
	g_free (file);
#endif

	/* TEXT */
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "rtext";
	plugin->plugin_descr = "Rich text editor entry";
	plugin->plugin_file = NULL; /* always leave NULL */
	plugin->nb_g_types = 3;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_STRING;
	plugin->valid_g_types [1] = GDA_TYPE_BLOB;
	plugin->valid_g_types [2] = GDA_TYPE_BINARY;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = plugin_entry_rt_create_func;
	plugin->cell_create_func = NULL;
	retlist = g_slist_append (retlist, plugin);

	/* Picture - binary */
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "picture";
	plugin->plugin_descr = "Picture entry";
	plugin->plugin_file = NULL; /* always leave NULL */
	plugin->nb_g_types = 2;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = GDA_TYPE_BINARY;
	plugin->valid_g_types [1] = GDA_TYPE_BLOB;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = plugin_entry_pict_create_func;
	plugin->cell_create_func = plugin_cell_renderer_pict_create_func;
	retlist = g_slist_append (retlist, plugin);

	/* picture - string encoded */
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "picture_as_string";
	plugin->plugin_descr = "Picture entry for data stored as a string";
	plugin->plugin_file = NULL; /* always leave NULL */
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_STRING;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = plugin_entry_pict_create_func;
	plugin->cell_create_func = plugin_cell_renderer_pict_create_func;
	retlist = g_slist_append (retlist, plugin);

	file = gda_gbr_get_file_path (GDA_LIB_DIR, LIBGDA_ABI_NAME, "plugins", "gdaui-entry-pict-spec_string.xml", NULL);
	if (! g_file_test (file, G_FILE_TEST_EXISTS)) {
		if (error && !*error)
			g_set_error (error,  GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_FILE_NOT_FOUND_ERROR,
				     _("Missing spec. file '%s'"), file);
        }
	else {
		gsize len;
		g_file_get_contents (file, &(plugin->options_xml_spec), &len, error);
	}
	g_free (file);

	return retlist;
}

static GdauiDataEntry *
plugin_entry_filesel_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_filesel_new (handler, type, options);
}

static GdauiDataEntry *
plugin_entry_cidr_create_func (GdaDataHandler *handler, GType type, G_GNUC_UNUSED const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_cidr_new (handler, type);
}

static GdauiDataEntry *
plugin_entry_format_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_format_new (handler, type, options);
}

static GdauiDataEntry *
plugin_entry_text_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_text_new (handler, type, options);
}

static GdauiDataEntry *
plugin_entry_rt_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_rt_new (handler, type, options);
}

static GdauiDataEntry *
plugin_entry_pict_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_pict_new (handler, type, options);
}

static GtkCellRenderer *
plugin_cell_renderer_pict_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return gdaui_data_cell_renderer_pict_new (handler, type, options);
}

#ifdef HAVE_LIBGCRYPT
static GdauiDataEntry *
plugin_entry_password_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
        return (GdauiDataEntry *) gdaui_entry_password_new (handler, type, options);
}

static GtkCellRenderer *
plugin_cell_renderer_password_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return gdaui_data_cell_renderer_password_new (handler, type, options);
}
#endif
