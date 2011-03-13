/*
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_DATA_STORE__
#define __GDAUI_DATA_STORE__

#include <gtk/gtk.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-proxy.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_STORE          (gdaui_data_store_get_type())
#define GDAUI_DATA_STORE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_data_store_get_type(), GdauiDataStore)
#define GDAUI_DATA_STORE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_data_store_get_type (), GdauiDataStoreClass)
#define GDAUI_IS_DATA_STORE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_data_store_get_type ())

typedef struct _GdauiDataStore GdauiDataStore;
typedef struct _GdauiDataStoreClass GdauiDataStoreClass;
typedef struct _GdauiDataStorePriv GdauiDataStorePriv;

enum {
	DATA_STORE_COL_MODEL_N_COLUMNS = -2, /* number of columns in the GdaDataModel */
	DATA_STORE_COL_MODEL_POINTER = -3, /* pointer to the GdaDataModel */
	DATA_STORE_COL_MODEL_ROW = -4, /* row number in the GdaDataModel, or -1 for new rows */
	DATA_STORE_COL_MODIFIED = -5, /* TRUE if row has been modified */
	DATA_STORE_COL_TO_DELETE = -6 /* TRUE if row is marked to be deleted */
};

/* struct for the object's data */
struct _GdauiDataStore
{
	GObject                object;

	GdauiDataStorePriv  *priv;
};

/* struct for the object's class */
struct _GdauiDataStoreClass
{
	GObjectClass           parent_class;
};

/**
 * SECTION:gdaui-data-store
 * @short_description: Bridge between a #GdaDataModel and a #GtkTreeModel
 * @title: GdauiDataStore
 * @stability: Stable
 * @Image:
 * @see_also:
 *
 * The #GdauiDataStore object implements the #GtkTreeModel interface
 * on top of a #GdaDataModel to be able to display its contents
 * in a #GtkTreeView.
 */

GType           gdaui_data_store_get_type             (void) G_GNUC_CONST;
GtkTreeModel   *gdaui_data_store_new                  (GdaDataModel *model);

GdaDataProxy   *gdaui_data_store_get_proxy            (GdauiDataStore *store);
gint            gdaui_data_store_get_row_from_iter    (GdauiDataStore *store, GtkTreeIter *iter);
gboolean        gdaui_data_store_get_iter_from_values (GdauiDataStore *store, GtkTreeIter *iter,
						       GSList *values, gint *cols_index);

gboolean        gdaui_data_store_set_value            (GdauiDataStore *store, GtkTreeIter *iter,
						       gint col, const GValue *value);
void            gdaui_data_store_delete               (GdauiDataStore *store, GtkTreeIter *iter);
void            gdaui_data_store_undelete             (GdauiDataStore *store, GtkTreeIter *iter);
gboolean        gdaui_data_store_append               (GdauiDataStore *store, GtkTreeIter *iter);

G_END_DECLS

#endif
