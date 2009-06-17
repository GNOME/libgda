/* gdaui-plugin.h
 *
 * Copyright (C) 2006 - 2009 Vivien Malerba <malerba@gnome-db.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDAUI_PLUGIN__
#define __GDAUI_PLUGIN__

#include <gtk/gtkcellrenderer.h>
#include <libgda/gda-value.h>
#include "gdaui-decl.h"

typedef GSList           *(*GdauiPluginInit)     (GError **);
typedef GdauiDataEntry   *(*GdauiEntryCreateFunc)(GdaDataHandler *, GType, const gchar *);
typedef GtkCellRenderer  *(*GdauiCellCreateFunc) (GdaDataHandler *, GType, const gchar *);


/**
 * Structure representing a plugin
 */
typedef struct {
	gchar                  *plugin_name;
	gchar                  *plugin_descr;
	gchar                  *plugin_file;

	guint                   nb_g_types; /* 0 if all types are accepted */
        GType                  *valid_g_types; /* not NULL if @nb_g_types is not 0 */

	gchar                  *options_xml_spec; /* NULL if no option possible */

	/* actual widget creation: one of them must be not NULL */
	GdauiEntryCreateFunc  entry_create_func;
	GdauiCellCreateFunc   cell_create_func;
} GdauiPlugin;

#endif
