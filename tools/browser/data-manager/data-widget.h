/*
 * Copyright (C) 2010 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __DATA_WIDGET_H__
#define __DATA_WIDGET_H__

#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include "data-source.h"
#include "data-source-manager.h"

G_BEGIN_DECLS

#define DATA_WIDGET_TYPE            (data_widget_get_type())
#define DATA_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, DATA_WIDGET_TYPE, DataWidget))
#define DATA_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, DATA_WIDGET_TYPE, DataWidgetClass))
#define IS_DATA_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, DATA_WIDGET_TYPE))
#define IS_DATA_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), DATA_WIDGET_TYPE))


typedef struct _DataWidget        DataWidget;
typedef struct _DataWidgetClass   DataWidgetClass;
typedef struct _DataWidgetPrivate DataWidgetPrivate;

struct _DataWidget {
	GtkBox parent;
	DataWidgetPrivate *priv;
};

struct _DataWidgetClass {
	GtkBoxClass parent_class;
};


GType      data_widget_get_type   (void) G_GNUC_CONST;
GtkWidget *data_widget_new        (DataSourceManager *mgr);
GdaSet    *data_widget_get_export (DataWidget *dwid, DataSource *source);
void       data_widget_rerun      (DataWidget *dwid);

G_END_DECLS

#endif
