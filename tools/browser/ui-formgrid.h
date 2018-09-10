/*
 * Copyright (C) 2010 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __UI_FORMGRID__
#define __UI_FORMGRID__

#include <gtk/gtk.h>
#include <libgda/gda-data-model.h>
#include <libgda-ui/gdaui-data-proxy-info.h>
#include <libgda-ui/gdaui-raw-grid.h>
#include <libgda-ui/gdaui-data-proxy.h>
#include "../common/t-connection.h"

G_BEGIN_DECLS

#define UI_TYPE_FORMGRID          (ui_formgrid_get_type())
#define UI_FORMGRID(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, ui_formgrid_get_type(), UiFormGrid)
#define UI_FORMGRID_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, ui_formgrid_get_type (), UiFormGridClass)
#define UI_IS_FORMGRID(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, ui_formgrid_get_type ())


typedef struct _UiFormGrid      UiFormGrid;
typedef struct _UiFormGridClass UiFormGridClass;
typedef struct _UiFormGridPriv  UiFormGridPriv;

/* struct for the object's data */
struct _UiFormGrid
{
	GtkBox             object;

	UiFormGridPriv     *priv;
};

/* struct for the object's class */
struct _UiFormGridClass
{
	GtkBoxClass       parent_class;

	/* signals */
	void             (*data_set_changed) (UiFormGrid *fg);
};

/**
 * SECTION:ui-formgrid
 * @short_description: Widget embedding both a form and a grid to display a #GdaDataModel's contents
 * @title: UiFormgrid
 * @stability: Stable
 * @see_also:
 */

GType             ui_formgrid_get_type            (void);

GtkWidget        *ui_formgrid_new                 (GdaDataModel *model, gboolean scroll_form,
						   GdauiDataProxyInfoFlag flags);
void              ui_formgrid_handle_user_prefs   (UiFormGrid *formgrid, TConnection *bcnc,
						   GdaStatement *stmt);

GArray           *ui_formgrid_get_selection       (UiFormGrid *formgrid);
GdaDataModelIter *ui_formgrid_get_form_data_set   (UiFormGrid *formgrid);
GdaDataModelIter *ui_formgrid_get_grid_data_set   (UiFormGrid *formgrid);
void              ui_formgrid_set_sample_size     (UiFormGrid *formgrid, gint sample_size);
GdauiRawGrid     *ui_formgrid_get_grid_widget     (UiFormGrid *formgrid);

void              ui_formgrid_set_connection      (UiFormGrid *formgrid, TConnection *bcnc);
void              ui_formgrid_set_refresh_func    (UiFormGrid *formgrid, GCallback callback, gpointer user_data);

G_END_DECLS

#endif



