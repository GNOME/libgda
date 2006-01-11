/* GDA common library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_data_model_filter_sql_h__)
#  define __gda_data_model_filter_sql_h__

#include <libgda/gda-data-model-array.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_FILTER_SQL            (gda_data_model_filter_sql_get_type())
#define GDA_DATA_MODEL_FILTER_SQL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_FILTER_SQL, GdaDataModelFilterSQL))
#define GDA_DATA_MODEL_FILTER_SQL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_FILTER_SQL, GdaDataModelFilterSQLClass))
#define GDA_IS_DATA_MODEL_FILTER_SQL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_DATA_MODEL_FILTER_SQL))
#define GDA_IS_DATA_MODEL_FILTER_SQL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_DATA_MODEL_FILTER_SQL))

typedef struct _GdaDataModelFilterSQL        GdaDataModelFilterSQL;
typedef struct _GdaDataModelFilterSQLClass   GdaDataModelFilterSQLClass;
typedef struct _GdaDataModelFilterSQLPrivate GdaDataModelFilterSQLPrivate;

struct _GdaDataModelFilterSQL {
	GdaDataModelArray             model;
	GdaDataModelFilterSQLPrivate *priv;
};

struct _GdaDataModelFilterSQLClass {
	GdaDataModelArrayClass        parent_class;
};

GType         gda_data_model_filter_sql_get_type   (void);
GdaDataModel *gda_data_model_filter_sql_new        (void);
void          gda_data_model_filter_sql_add_source (GdaDataModelFilterSQL *sel, const gchar *name, GdaDataModel *source);
void          gda_data_model_filter_sql_set_sql    (GdaDataModelFilterSQL *sel, const gchar *sql);
gboolean      gda_data_model_filter_sql_run        (GdaDataModelFilterSQL *sel);

G_END_DECLS

#endif
