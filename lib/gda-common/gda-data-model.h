/* GDA common library
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#if !defined(__gda_data_model_h__)
#  define __gda_data_model_h__

#include <gda-common-defs.h>
#include <gda-value.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL            (gda_data_model_get_type())
#define GDA_DATA_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL, GdaDataModel))
#define GDA_DATA_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL, GdaDataModelClass))
#define GDA_IS_DATA_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_MODEL))
#define GDA_IS_DATA_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_DATA_MODEL))

typedef struct _GdaDataModel        GdaDataModel;
typedef struct _GdaDataModelClass   GdaDataModelClass;
typedef struct _GdaDataModelPrivate GdaDataModelPrivate;

struct _GdaDataModel {
	GObject object;
	GdaDataModelPrivate *priv;
};

struct _GdaDataModelClass {
	GObjectClass parent_class;

	/* signals */

	/* virtual methods to be implemented by subclasses */
};

GType        gda_data_model_get_type (void);

void         gda_data_model_append_column (GdaDataModel *model, const gchar *title);
void         gda_data_model_remove_column (GdaDataModel *model, gint col);
void         gda_data_model_append_row (GdaDataModel *model);
void         gda_data_model_remove_row (GdaDataModel *model, gint row);

gint         gda_data_model_get_n_columns (GdaDataModel *model);
const gchar *gda_data_model_get_column_title (GdaDataModel *model, gint col);
gint         gda_data_model_get_row_count (GdaDataModel *model);
GdaValue    *gda_data_model_get_value_at (GdaDataModel *model, gint row, gint col);
void         gda_data_model_set_value_at (GdaDataModel *model, gint row, gint col, GdaValue *value);

G_END_DECLS

#endif
