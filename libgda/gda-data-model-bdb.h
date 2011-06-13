/*
 * Copyright (C) 2007 - 2009 Vivien Malerba <malerba@gnome-db.org>
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

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType         gda_data_model_bdb_get_type     (void) G_GNUC_CONST;
GdaDataModel *gda_data_model_bdb_new          (const gchar *filename, const gchar *db_name);

const GSList *gda_data_model_bdb_get_errors   (GdaDataModelBdb *model);
void          gda_data_model_bdb_clean_errors (GdaDataModelBdb *model);
G_END_DECLS

#endif
