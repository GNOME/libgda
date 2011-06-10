/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DATA_MODEL_BDB_H__
#define __GDA_DATA_MODEL_BDB_H__

#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_BDB            (gda_data_model_bdb_get_type())
#define GDA_DATA_MODEL_BDB(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_BDB, GdaDataModelBdb))
#define GDA_DATA_MODEL_BDB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_BDB, GdaDataModelBdbClass))
#define GDA_IS_DATA_MODEL_BDB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_DATA_MODEL_BDB))
#define GDA_IS_DATA_MODEL_BDB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_DATA_MODEL_BDB))

typedef struct _GdaDataModelBdb        GdaDataModelBdb;
typedef struct _GdaDataModelBdbClass   GdaDataModelBdbClass;
typedef struct _GdaDataModelBdbPrivate GdaDataModelBdbPrivate;

struct _GdaDataModelBdb {
	GObject                 object;
	GdaDataModelBdbPrivate *priv;
};

struct _GdaDataModelBdbClass {
	GObjectClass            parent_class;

	/* virtual methods */
	GSList                *(*create_key_columns)  (GdaDataModelBdb *model);
	GSList                *(*create_data_columns) (GdaDataModelBdb *model);
	GValue                *(*get_key_part)        (GdaDataModelBdb *model, 

						       gpointer data, gint length, gint part);
	GValue                *(*get_data_part)       (GdaDataModelBdb *model,
						       gpointer data, gint length, gint part);
	gboolean               (*update_key_part)     (GdaDataModelBdb *model,
						       gpointer data, gint length, gint part, 
						       const GValue *value, GError **error);
	gboolean               (*update_data_part)    (GdaDataModelBdb *model,
						       gpointer data, gint length, gint part, 
						       const GValue *value, GError **error);
	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-data-model-bdb
 * @short_description: GdaDataModel to access Berkeley DB database contents
 * @title: GdaDataModelBdb
 * @stability: Stable
 * @see_also: #GdaDataModel
 *
 * The #GdaDataModelBdb object allows to access the contents of a Berkeley DB database as a
 * #GdaDataModel object.
 *
 * By default the resulting GdaDataModel contains only two columns (named "key" and "data") of type
 * GDA_TYPE_BINARY, but this object can be subclassed to convert the key or data part of a BDB record
 * into several columns (implement the create_key_columns(), create_data_columns(), get_key_part(), and get_data_part() 
 * virtual methods).
 *
 * Note: this type of data model is available only if the Berkeley DB library was found at compilation time.
 */

GType         gda_data_model_bdb_get_type     (void) G_GNUC_CONST;
GdaDataModel *gda_data_model_bdb_new          (const gchar *filename, const gchar *db_name);

const GSList *gda_data_model_bdb_get_errors   (GdaDataModelBdb *model);
void          gda_data_model_bdb_clean_errors (GdaDataModelBdb *model);
G_END_DECLS

#endif
