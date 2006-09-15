/* GDA common library
 * Copyright (C) 2005 The GNOME Foundation.
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

#if !defined(__gda_data_model_query_h__)
#  define __gda_data_model_query_h__

#include <libgda/gda-data-model.h>
#include <libgda/gda-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_QUERY            (gda_data_model_query_get_type())
#define GDA_DATA_MODEL_QUERY(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_QUERY, GdaDataModelQuery))
#define GDA_DATA_MODEL_QUERY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_QUERY, GdaDataModelQueryClass))
#define GDA_IS_DATA_MODEL_QUERY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_MODEL_QUERY))
#define GDA_IS_DATA_MODEL_QUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_DATA_MODEL_QUERY))

typedef struct _GdaDataModelQuery        GdaDataModelQuery;
typedef struct _GdaDataModelQueryClass   GdaDataModelQueryClass;
typedef struct _GdaDataModelQueryPrivate GdaDataModelQueryPrivate;

/* error reporting */
extern GQuark gda_data_model_query_error_quark (void);
#define GDA_DATA_MODEL_QUERY_ERROR gda_data_model_query_error_quark ()

enum
{
        GDA_DATA_MODEL_QUERY_XML_LOAD_ERROR,
};

struct _GdaDataModelQuery {
	GdaObject                  object;
	GdaDataModelQueryPrivate  *priv;
};

struct _GdaDataModelQueryClass {
	GdaObjectClass             parent_class;
};

GType             gda_data_model_query_get_type              (void);
GdaDataModel     *gda_data_model_query_new                   (GdaQuery *query);

GdaParameterList *gda_data_model_query_get_parameter_list    (GdaDataModelQuery *model);
gboolean          gda_data_model_query_refresh               (GdaDataModelQuery *model, GError **error);
gboolean          gda_data_model_query_set_modification_query(GdaDataModelQuery *model, 
							      const gchar *query, GError **error);


G_END_DECLS

#endif

