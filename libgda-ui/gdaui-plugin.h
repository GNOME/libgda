/*
 * Copyright (C) 2009 - 2010 Vivien Malerba <malerba@gnome-db.org>
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
 * @Param1: a #GdaDataHandler
 * @Param2: a #GType
 * @Param3: options, or %NULL
 * @Returns: a new #GdauiDataEntry
 *
 * Defines a function which creates a #GdauiDataEntry widget
 */
typedef GdauiDataEntry   *(*GdauiEntryCreateFunc)(GdaDataHandler *, GType, const gchar *);

/**
 * GdauiCellCreateFunc:
 * @Param1: a #GdaDataHandler
 * @Param2: a #GType
 * @Param3: options, or %NULL
 * @Returns:a new #GtkCellRenderer
 *
 * Defines a function which creates a #GtkCellRenderer object
 */
typedef GtkCellRenderer  *(*GdauiCellCreateFunc) (GdaDataHandler *, GType, const gchar *);


/**
 * GdauiPlugin:
 * @plugin_name: the name of the plugin
 * @plugin_descr: a description for the plugin, or %NULL
 * @plugin_file: the shared object implementing the plugin, can be %NULL for internal plugins
 * @nb_g_types: number of types the plugin can handle, or %0 for any type
 * @valid_g_types: an array of #GType, containing the accepted types, its size is @nb_g_types, or %NULL if @nb_g_types is %0
 * @options_xml_spec: a string describing the plugin's options, or %NULL
 * @entry_create_func: the function called to create a #GdauiDataEntry, or %NULL
 * @cell_create_func: the function called to create a #GtkCellRenderer, or %NULL
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

void gdaui_plugin_declare (const GdauiPlugin *plugin);

#endif
