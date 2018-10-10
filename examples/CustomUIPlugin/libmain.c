/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda-ui/libgda-ui.h>
#include <libgda-ui/gdaui-plugin.h>
#include <libgda/gda-binreloc.h>

#include "custom-entry-password.h"
//#include "custom-data-cell-renderer-password.h"

static GdauiDataEntry *plugin_password_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GtkCellRenderer  *plugin_cell_renderer_password_create_func (GdaDataHandler *handler, GType type, const gchar *options);

GSList *
plugin_init (GError **error)
{
	GdauiPlugin *plugin;
	GSList *retlist = NULL;
	gchar *file;

	/* PASSWORD */
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "custom password";
	plugin->plugin_descr = "custom password entry";
	plugin->plugin_file = NULL; /* always leave NULL */
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_STRING;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = plugin_password_create_func;
	plugin->cell_create_func = plugin_cell_renderer_password_create_func;
	retlist = g_slist_append (retlist, plugin);

	file = gda_gbr_get_file_path (GDA_LIB_DIR, "libgda-6.0", "plugins", "custom-entry-password.xml", NULL);
	if (! g_file_test (file, G_FILE_TEST_EXISTS)) {
		if (error && !*error)
			g_set_error (error, GDAUI_PLUGIN_ERROR, GDAUI_PLUGIN_GENERAL_ERROR, "Missing spec. file '%s'", file);
        }
	else {
		gsize len;
		g_file_get_contents (file, &(plugin->options_xml_spec), &len, error);
	}
	g_free (file);

	return retlist;
}

static GdauiDataEntry *
plugin_password_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
        return (GdauiDataEntry *) custom_entry_password_new (handler, type, options);
}

static GtkCellRenderer *
plugin_cell_renderer_password_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	//return gdaui_data_cell_renderer_password_new (handler, type, options);
	return NULL;
}

