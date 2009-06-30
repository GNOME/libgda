/* gdaui-entry-cgrid.h
 *
 * Copyright (C) 2007-2007 Carlos Savoretti
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __GDAUI_ENTRY_CGRID_H__
#define __GDAUI_ENTRY_CGRID_H__

#include <libgda-ui/data-entries/gdaui-entry-wrapper.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_CGRID            (gdaui_entry_cgrid_get_type ())
#define GDAUI_ENTRY_CGRID(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_ENTRY_CGRID, GdauiEntryCGrid))
#define GDAUI_ENTRY_CGRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_ENTRY_CGRID, GdauiEntryCGridClass))
#define GDAUI_IS_ENTRY_CGRID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_ENTRY_CGRID))
#define GDAUI_IS_ENTRY_CGRID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_ENTRY_CGRID))
#define GDAUI_ENTRY_CGRID_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_ENTRY_CGRID, GdauiEntryCGridClass))

typedef struct _GdauiEntryCGrid GdauiEntryCGrid;
typedef struct _GdauiEntryCGridClass GdauiEntryCGridClass;
typedef struct _GdauiEntryCGridPrivate GdauiEntryCGridPrivate;

typedef void (* GdauiEntryCGridCgridChangedFunc) (GdauiEntryCGrid  *cgrid);

struct _GdauiEntryCGrid
{
	GdauiEntryWrapper       parent;
	GdauiEntryCGridPrivate *priv;
};

struct _GdauiEntryCGridClass
{
	GdauiEntryWrapperClass   parent;
	void                      (* cgrid_changed) (GdauiEntryCGrid  *cgrid);
};

GType               gdaui_entry_cgrid_get_type            (void) G_GNUC_CONST;
GdauiEntryCGrid  *gdaui_entry_cgrid_new                 (GdaDataHandler *data_handler, GType gtype, 
							      const gchar *options);
gint                gdaui_entry_cgrid_get_text_column     (GdauiEntryCGrid *cgrid);
void                gdaui_entry_cgrid_set_text_column     (GdauiEntryCGrid *cgrid, gint text_column);
gint                gdaui_entry_cgrid_get_grid_height     (GdauiEntryCGrid *cgrid);
void                gdaui_entry_cgrid_set_grid_height     (GdauiEntryCGrid *cgrid, gint grid_height);
gboolean            gdaui_entry_cgrid_get_headers_visible (GdauiEntryCGrid *cgrid);
void                gdaui_entry_cgrid_set_headers_visible (GdauiEntryCGrid *cgrid, gboolean headers_visible);
GdaDataModel       *gdaui_entry_cgrid_get_model           (GdauiEntryCGrid *cgrid);
void                gdaui_entry_cgrid_set_model           (GdauiEntryCGrid *cgrid, GdaDataModel *model);
void                gdaui_entry_cgrid_append_column       (GdauiEntryCGrid *cgrid, GtkTreeViewColumn *column);
gboolean            gdaui_entry_cgrid_get_active_iter     (GdauiEntryCGrid *cgrid, GtkTreeIter *iter);

G_END_DECLS

#endif 
