/* GDA library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Bas Driessen <bas.driessen@xobas.com>
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

#if !defined(__gda_data_model_column_index_attributes_h__)
#  define __gda_data_model_column_index_attributes_h__

#include <libgda/gda-value.h>
#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	GDA_SORTING_ASCENDING,
	GDA_SORTING_DESCENDING
} GdaSorting;

typedef struct _GdaDataModelColumnIndexAttributes GdaDataModelColumnIndexAttributes;

struct _GdaDataModelColumnIndexAttributes {
	gchar *column_name;
	gint defined_size;
	GdaSorting sorting;
	gchar *references;
};

#define GDA_TYPE_DATA_MODEL_COLUMN_INDEX_ATTRIBUTES (gda_data_model_column_index_attributes_get_type ())

GType               gda_data_model_column_index_attributes_get_type (void);
GdaDataModelColumnIndexAttributes *gda_data_model_column_index_attributes_new (void);
GdaDataModelColumnIndexAttributes *gda_data_model_column_index_attributes_copy (GdaDataModelColumnIndexAttributes *dmcia);
void                gda_data_model_column_index_attributes_free (GdaDataModelColumnIndexAttributes *dmcia);
gboolean            gda_data_model_column_index_attributes_equal (const GdaDataModelColumnIndexAttributes *lhs, const GdaDataModelColumnIndexAttributes *rhs);
const gchar        *gda_data_model_column_index_attributes_get_column_name (GdaDataModelColumnIndexAttributes *dmcia);
void                gda_data_model_column_index_attributes_set_column_name (GdaDataModelColumnIndexAttributes *dmcia, const gchar *column_name);
glong               gda_data_model_column_index_attributes_get_defined_size (GdaDataModelColumnIndexAttributes *dmcia);
void                gda_data_model_column_index_attributes_set_defined_size (GdaDataModelColumnIndexAttributes *dmcia, glong size);
GdaSorting          gda_data_model_column_index_attributes_get_sorting (GdaDataModelColumnIndexAttributes *dmcia);
void                gda_data_model_column_index_attributes_set_sorting (GdaDataModelColumnIndexAttributes *dmcia, GdaSorting sorting);
const gchar        *gda_data_model_column_index_attributes_get_references (GdaDataModelColumnIndexAttributes *dmcia);
void                gda_data_model_column_index_attributes_set_references (GdaDataModelColumnIndexAttributes *dmcia, const gchar *ref);

G_END_DECLS

#endif
