/* GDA common library
 * Copyright (C) 2007 - 2008 The GNOME Foundation.
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

#ifndef __GDA_DATA_MODEL_BDB_H__
#define __GDA_DATA_MODEL_BDB_H__

#include <db.h>
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
};

GType         gda_data_model_bdb_get_type     (void) G_GNUC_CONST;
GdaDataModel *gda_data_model_bdb_new          (const gchar *filename, const gchar *db_name);

GSList       *gda_data_model_bdb_get_errors   (GdaDataModelBdb *model);
void          gda_data_model_bdb_clean_errors (GdaDataModelBdb *model);
G_END_DECLS

#endif
