/* gdaui-data-cell-renderer-cgrid.h
 *
 * Copyright (C) 2007 - 2007 Carlos Savoretti
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

#ifndef __GDAUI_DATA_CELL_RENDERER_CGRID_H__
#define __GDAUI_DATA_CELL_RENDERER_CGRID_H__

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <libgdaui/gdaui-decl.h>

#include <libgda/libgda.h>
#include <libgda/gda-enum-types.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_CELL_RENDERER_CGRID            (gdaui_data_cell_renderer_cgrid_get_type ())
#define GDAUI_DATA_CELL_RENDERER_CGRID(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_CGRID, GdauiDataCellRendererCGrid))
#define GDAUI_DATA_CELL_RENDERER_CGRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_CGRID, GdauiDataCellRendererCGridClass))
#define GDAUI_IS_DATA_CELL_RENDERER_CGRID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_CGRID))
#define GDAUI_IS_DATA_CELL_RENDERER_CGRID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_CGRID))
#define GDAUI_DATA_CELL_RENDERER_CGRID_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_CGRID, GdauiDataCellRendererCGridClass))

typedef struct _GdauiDataCellRendererCGrid GdauiDataCellRendererCGrid;
typedef struct _GdauiDataCellRendererCGridClass GdauiDataCellRendererCGridClass;
typedef struct _GdauiDataCellRendererCGridPrivate GdauiDataCellRendererCGridPrivate;

typedef void (* GdauiDataCellRendererCGridCgridChangedFunc) (GdauiDataCellRendererCGrid  *cgrid,
							       const GValue                  *gvalue);

struct _GdauiDataCellRendererCGrid
{
	GtkCellRendererText                  parent;
	GdauiDataCellRendererCGridPrivate *priv;
};

struct _GdauiDataCellRendererCGridClass
{
	GtkCellRendererTextClass  parent;
	void                     (* changed) (GdauiDataCellRendererCGrid  *cgrid,
					      const GValue                  *gvalue);
};

GType                         gdaui_data_cell_renderer_cgrid_get_type          (void) G_GNUC_CONST;
GdauiDataCellRendererCGrid *gdaui_data_cell_renderer_cgrid_new               (GdaDataHandler *data_handler, 
										   GType gtype, const gchar *options);
GdaDataHandler               *gdaui_data_cell_renderer_cgrid_get_data_handler  (GdauiDataCellRendererCGrid *cgrid);
void                          gdaui_data_cell_renderer_cgrid_set_data_handler  (GdauiDataCellRendererCGrid *cgrid, 
										   GdaDataHandler *data_handler);
GType                         gdaui_data_cell_renderer_cgrid_get_gtype         (GdauiDataCellRendererCGrid *cgrid);
void                          gdaui_data_cell_renderer_cgrid_set_gtype         (GdauiDataCellRendererCGrid  *cgrid, 
										   GType gtype);
gboolean                      gdaui_data_cell_renderer_cgrid_get_editable      (GdauiDataCellRendererCGrid *cgrid);
void                          gdaui_data_cell_renderer_cgrid_set_editable      (GdauiDataCellRendererCGrid  *cgrid, 
										   gboolean editable);
gboolean                      gdaui_data_cell_renderer_cgrid_get_to_be_deleted (GdauiDataCellRendererCGrid *cgrid);
void                          gdaui_data_cell_renderer_cgrid_set_to_be_deleted (GdauiDataCellRendererCGrid  *cgrid, 
										   gboolean to_be_deleted);

G_END_DECLS

#endif
