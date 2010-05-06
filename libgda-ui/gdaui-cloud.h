/* gdaui-cloud.h
 *
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_CLOUD__
#define __GDAUI_CLOUD__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_CLOUD          (gdaui_cloud_get_type())
#define GDAUI_CLOUD(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_cloud_get_type(), GdauiCloud)
#define GDAUI_CLOUD_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_cloud_get_type (), GdauiCloudClass)
#define GDAUI_IS_CLOUD(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_cloud_get_type ())
#define GDAUI_IS_CLOUD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDAUI_TYPE_CLOUD))

typedef struct _GdauiCloud      GdauiCloud;
typedef struct _GdauiCloudClass GdauiCloudClass;
typedef struct _GdauiCloudPriv  GdauiCloudPriv;

/* struct for the object's data */
struct _GdauiCloud
{
	GtkVBox             object;

	GdauiCloudPriv     *priv;
};

/* struct for the object's class */
struct _GdauiCloudClass
{
	GtkVBoxClass       parent_class;
	void            (* activate) (GdauiCloud *cloud, gint row);
};

/* 
 * Generic widget's methods 
 */
GType             gdaui_cloud_get_type             (void) G_GNUC_CONST;

GtkWidget        *gdaui_cloud_new                  (GdaDataModel *model, gint label_column, gint weight_column);
void              gdaui_cloud_set_selection_mode   (GdauiCloud *cloud, GtkSelectionMode mode);

void              gdaui_cloud_filter               (GdauiCloud *cloud, const gchar *filter);
GtkWidget        *gdaui_cloud_create_filter_widget (GdauiCloud *cloud);

typedef gdouble (*GdauiCloudWeightFunc)            (GdaDataModel *model, gint row, gpointer data);
void              gdaui_cloud_set_weight_func      (GdauiCloud *cloud, GdauiCloudWeightFunc func, gpointer data);

G_END_DECLS

#endif



