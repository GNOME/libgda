/* GDA common library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
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

#ifndef __GDA_DATA_MODEL_ARRAY_H__
#define __GDA_DATA_MODEL_ARRAY_H__

#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-row.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_ARRAY            (gda_data_model_array_get_type())
#define GDA_DATA_MODEL_ARRAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_ARRAY, GdaDataModelArray))
#define GDA_DATA_MODEL_ARRAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_ARRAY, GdaDataModelArrayClass))
#define GDA_IS_DATA_MODEL_ARRAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_MODEL_ARRAY))
#define GDA_IS_DATA_MODEL_ARRAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_DATA_MODEL_ARRAY))

typedef struct _GdaDataModelArray        GdaDataModelArray;
typedef struct _GdaDataModelArrayClass   GdaDataModelArrayClass;
typedef struct _GdaDataModelArrayPrivate GdaDataModelArrayPrivate;

struct _GdaDataModelArray {
	GdaDataModelRow           model;
	GdaDataModelArrayPrivate *priv;
};

struct _GdaDataModelArrayClass {
	GdaDataModelRowClass      parent_class;
};

GType         gda_data_model_array_get_type          (void) G_GNUC_CONST;
GdaDataModel *gda_data_model_array_new_with_g_types  (gint cols, ...);
GdaDataModel *gda_data_model_array_new               (gint cols);
GdaDataModel *gda_data_model_array_copy_model        (GdaDataModel *src, GError **error);
void          gda_data_model_array_set_n_columns     (GdaDataModelArray *model, gint cols);
void          gda_data_model_array_clear             (GdaDataModelArray *model);

G_END_DECLS

#endif
