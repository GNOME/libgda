/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_PLUGIN__
#define __GDAUI_PLUGIN__

#include <gtk/gtk.h>
#include <libgda/gda-value.h>
#include <libgda-ui/gdaui-data-entry.h>
#include "gdaui-decl.h"

/**
 * GdauiEntryCreateFunc:
 * @dh: a #GdaDataHandler
 * @type: a #GType
 * @options: (allow-none): options, or %NULL
 * @Returns: a new #GdauiDataEntry
 *
 * Defines a function which creates a #GdauiDataEntry widget
 */
typedef GdauiDataEntry   *(*GdauiEntryCreateFunc)(GdaDataHandler *dh, GType type, const gchar *options);

/**
 * GdauiCellCreateFunc:
 * @dh: a #GdaDataHandler
 * @type: a #GType
 * @options: (allow-none): options, or %NULL
 * @Returns:a new #GtkCellRenderer
 *
 * Defines a function which creates a #GtkCellRenderer object
 */
typedef GtkCellRenderer  *(*GdauiCellCreateFunc) (GdaDataHandler *dh, GType type, const gchar *options);


/**
 * GdauiPlugin:
 * @plugin_name: the name of the plugin
 * @plugin_descr: (allow-none): a description for the plugin, or %NULL
 * @plugin_file: (allow-none): the shared object implementing the plugin, can be %NULL for internal plugins
 * @nb_g_types: number of types the plugin can handle, or %0 for any type
 * @valid_g_types: (allow-none): an array of #GType, containing the accepted types, its size is @nb_g_types, or %NULL if @nb_g_types is %0
 * @options_xml_spec: (allow-none): a string describing the plugin's options, or %NULL
 * @entry_create_func: (allow-none): the function called to create a #GdauiDataEntry, or %NULL
 * @cell_create_func: (allow-none): the function called to create a #GtkCellRenderer, or %NULL
 *
 * Structure representing a plugin.
 *
 * Note: @entry_create_func and @cell_create_func can't be %NULL at the same time
 */
typedef struct {
	gchar                  *plugin_name;
	gchar                  *plugin_descr;
	gchar                  *plugin_file;

	guint                   nb_g_types; /* 0 if all types are accepted */
        GType                  *valid_g_types; /* not NULL if @nb_g_types is not 0 */

	gchar                  *options_xml_spec; /* NULL if no option possible */

	GdauiEntryCreateFunc    entry_create_func;
	GdauiCellCreateFunc     cell_create_func;
} GdauiPlugin;

/**
 * SECTION:gdaui-plugins
 * @short_description: Plugin to customize dana entry widgets and call renderers in tree views
 * @title: UI plugins
 * @stability: Stable
 * @Image:
 * @see_also:
 *
 * This section describes the functions used to declare UI plugins: data entry and cell renderers.
 */

void gdaui_plugin_declare (const GdauiPlugin *plugin);

#endif
