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

#include <gda-value.h>

G_BEGIN_DECLS

typedef struct _GdaDataModel        GdaDataModel;
typedef struct _GdaDataModelClass   GdaDataModelClass;
typedef struct _GdaDataModelPrivate GdaDataModelPrivate;

struct _GdaDataModel {
	GObject object;
	GdaDataModelPrivate *priv;
};

struct _GdaDataModelClass {
	GObjectClass parent_class;

	/* virtual methods */
	GdaValue * (* get_value_at) (GdaDataModel *model, gint col, gint row);
};

GType               gda_data_model_get_type (void);

gint                gda_data_model_get_n_columns (GdaDataModel *model);
void                gda_data_model_add_column (GdaDataModel *model, GdaFieldAttributes *attr);
void                gda_data_model_insert_column (GdaDataModel *model,
						  GdaFieldAttributes *attr,
						  gint pos);
void                gda_data_model_remove_column_by_pos (GdaDataModel *model, gint pos);
void                gda_data_model_remove_column_by_name (GdaDataModel *model,
							  const gchar *name);
GdaFieldAttributes *gda_data_model_describe_column_by_pos (GdaDataModel *model, gint pos);
GdaFieldAttributes *gda_data_model_describe_column_by_name (GdaDataModel *model,
							    const gchar *name);

GdaValue           *gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row);

G_END_DECLS

#endif
