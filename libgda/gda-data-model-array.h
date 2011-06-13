/*
 * Copyright (C) 2001 Cleber Rodrigues <cleberrrjr@bol.com.br>
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DATA_MODEL_ARRAY_H__
#define __GDA_DATA_MODEL_ARRAY_H__

#include <libgda/gda-data-model.h>
#include <libgda/gda-row.h>

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
	GObject                   object;
	GdaDataModelArrayPrivate *priv;
};

struct _GdaDataModelArrayClass {
	GObjectClass              parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType              gda_data_model_array_get_type          (void) G_GNUC_CONST;
GdaDataModel      *gda_data_model_array_new_with_g_types  (gint cols, ...);
GdaDataModel      *gda_data_model_array_new_with_g_types_v (gint cols, GType *types);
GdaDataModel      *gda_data_model_array_new               (gint cols);
GdaDataModelArray *gda_data_model_array_copy_model        (GdaDataModel *src, GError **error);

GdaRow            *gda_data_model_array_get_row           (GdaDataModelArray *model, gint row, GError **error);
void               gda_data_model_array_set_n_columns     (GdaDataModelArray *model, gint cols);
void               gda_data_model_array_clear             (GdaDataModelArray *model);

G_END_DECLS

#endif
