/* GNOME DB library
 * Copyright (C) 2010 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __DATA_WIDGET_H__
#define __DATA_WIDGET_H__

#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include "data-source.h"

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
	GtkVBox parent;
	DataWidgetPrivate *priv;
};

struct _DataWidgetClass {
	GtkVBoxClass parent_class;
};


GType      data_widget_get_type   (void) G_GNUC_CONST;
GtkWidget *data_widget_new        (GArray *sources_array);
GdaSet    *data_widget_get_export (DataWidget *dwid, DataSource *source);
void       data_widget_rerun      (DataWidget *dwid);

G_END_DECLS

#endif
