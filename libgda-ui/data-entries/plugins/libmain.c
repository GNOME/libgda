/* libmain.c
 * Copyright (C) 2006 - 2009 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gdaui.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "gdaui-data-cell-renderer-pict.h"
#include "gdaui-entry-cgrid.h"
#include "gdaui-data-cell-renderer-cgrid.h"

#ifdef HAVE_LIBGCRYPT
#include "gdaui-entry-password.h"
#include "gdaui-data-cell-renderer-password.h"
#endif

static GdauiDataEntry *plugin_entry_filesel_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *plugin_entry_cidr_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *plugin_entry_text_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *plugin_entry_pict_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GtkCellRenderer  *plugin_cell_renderer_pict_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *plugin_entry_cgrid_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GtkCellRenderer  *plugin_cell_renderer_cgrid_create_func (GdaDataHandler *handler, GType type, const gchar *options);

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
			g_set_error (error, 0, 0, _("Missing spec. file '%s'"), file);
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
			g_set_error (error, 0, 0, _("Missing spec. file '%s'"), file);
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
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_STRING;;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = plugin_entry_text_create_func;
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

	file = gda_gbr_get_file_path (GDA_LIB_DIR, LIBGDA_ABI_NAME, "plugins", "gdaui-entry-pict-spec.xml", NULL);
	if (! g_file_test (file, G_FILE_TEST_EXISTS)) {
		if (error && !*error)
			g_set_error (error, 0, 0, _("Missing spec. file '%s'"), file);
        }
	else {
		gsize len;
		g_file_get_contents (file, &(plugin->options_xml_spec), &len, error);
	}
	g_free (file);

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
			g_set_error (error, 0, 0, _("Missing spec. file '%s'"), file);
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
plugin_entry_cidr_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_cidr_new (handler, type);
}

static GdauiDataEntry *
plugin_entry_text_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_text_new (handler, type);
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

static GdauiDataEntry *
plugin_entry_cgrid_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_cgrid_new (handler, type, options);
}

static GtkCellRenderer *
plugin_cell_renderer_cgrid_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GtkCellRenderer *) gdaui_data_cell_renderer_cgrid_new (handler, type, options);
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
