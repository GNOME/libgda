/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_data_model_list_h__)
#  define __gda_data_model_list_h__

#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_LIST            (gda_data_model_list_get_type())
#define GDA_DATA_MODEL_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_LIST, GdaDataModelList))
#define GDA_DATA_MODEL_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_LIST, GdaDataModelListClass))
#define GDA_IS_DATA_MODEL_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_MODEL_LIST))
#define GDA_IS_DATA_MODEL_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_DATA_MODEL_LIST))

typedef struct _GdaDataModelList        GdaDataModelList;
typedef struct _GdaDataModelListClass   GdaDataModelListClass;
typedef struct _GdaDataModelListPrivate GdaDataModelListPrivate;

struct _GdaDataModelList {
	GdaDataModel model;
	GdaDataModelListPrivate *priv;
};

struct _GdaDataModelListClass {
	GdaDataModelClass parent_class;
};

GType         gda_data_model_list_get_type (void);
GdaDataModel *gda_data_model_list_new (void);
GdaDataModel *gda_data_model_list_new_from_string_list (const GList *list);

void          gda_data_model_list_append_value (GdaDataModelList *model,
						const GdaValue *value);
void          gda_data_model_list_prepend_value (GdaDataModelList *model,
						 const GdaValue *value);
void          gda_data_model_list_insert_value (GdaDataModelList *model,
						const GdaValue *value,
						gint row);

G_END_DECLS

#endif
