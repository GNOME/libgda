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

#ifndef __GDA_DATA_MODEL_ROW_H__
#define __GDA_DATA_MODEL_ROW_H__

#include <glib-object.h>
#include <libgda/gda-value.h>
#include <libgda/gda-row.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_ROW            (gda_data_model_row_get_type())
#define GDA_DATA_MODEL_ROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_ROW, GdaDataModelRow))
#define GDA_DATA_MODEL_ROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_ROW, GdaDataModelRowClass))
#define GDA_IS_DATA_MODEL_ROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_MODEL_ROW))
#define GDA_IS_DATA_MODEL_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_DATA_MODEL_ROW))

typedef struct _GdaDataModelRow        GdaDataModelRow;
typedef struct _GdaDataModelRowClass   GdaDataModelRowClass;
typedef struct _GdaDataModelRowPrivate GdaDataModelRowPrivate;

struct _GdaDataModelRow {
	GObject                 object;
	GdaDataModelRowPrivate *priv;
};

struct _GdaDataModelRowClass {
	GObjectClass            parent_class;

	/* virtual methods */
	gint                (* get_n_rows)      (GdaDataModelRow *model);
	gint                (* get_n_columns)   (GdaDataModelRow *model);
	GdaRow             *(* get_row)         (GdaDataModelRow *model, gint row, GError **error);
	const GValue       *(* get_value_at)    (GdaDataModelRow *model, gint col, gint row);
	
	gboolean            (* is_updatable)    (GdaDataModelRow *model);

	GdaRow             *(* append_values)   (GdaDataModelRow *model, const GList *values, GError **error);
	gboolean            (* append_row)      (GdaDataModelRow *model, GdaRow *row, GError **error);
	gboolean            (* update_row)      (GdaDataModelRow *model, GdaRow *row, GError **error);
	gboolean            (* remove_row)      (GdaDataModelRow *model, GdaRow *row, GError **error);
};

GType        gda_data_model_row_get_type       (void) G_GNUC_CONST;
GdaRow      *gda_data_model_row_get_row        (GdaDataModelRow *model, gint row, GError **error);

G_END_DECLS

#endif
