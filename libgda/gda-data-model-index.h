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

#if !defined(__gda_data_model_index_h__)
#  define __gda_data_model_index_h__

#include <libgda/gda-value.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _GdaDataModelIndex GdaDataModelIndex;

struct _GdaDataModelIndex {
	gchar *name;
	gchar *table_name;
	gboolean primary_key;
	gboolean unique_key;
	gchar *references;
	GList *col_idx_list;
};

#define GDA_TYPE_DATA_MODEL_INDEX (gda_data_model_index_get_type ())

GType               gda_data_model_index_get_type (void);
GdaDataModelIndex  *gda_data_model_index_new (void);
GdaDataModelIndex  *gda_data_model_index_copy (GdaDataModelIndex *dmi);
void                gda_data_model_index_free (GdaDataModelIndex *dmi);
gboolean            gda_data_model_index_equal (const GdaDataModelIndex *lhs, const GdaDataModelIndex *rhs);
const gchar        *gda_data_model_index_get_name (GdaDataModelIndex *dmi);
void                gda_data_model_index_set_name (GdaDataModelIndex *dmi, const gchar *name);
const gchar        *gda_data_model_index_get_table_name (GdaDataModelIndex *dmi);
void                gda_data_model_index_set_table_name (GdaDataModelIndex *dmi, const gchar *name);
gboolean            gda_data_model_index_get_primary_key (GdaDataModelIndex *dmi);
void                gda_data_model_index_set_primary_key (GdaDataModelIndex *dmi, gboolean pk);
gboolean            gda_data_model_index_get_unique_key (GdaDataModelIndex *dmi);
void                gda_data_model_index_set_unique_key (GdaDataModelIndex *dmi, gboolean uk);
const gchar        *gda_data_model_index_get_references (GdaDataModelIndex *dmi);
void                gda_data_model_index_set_references (GdaDataModelIndex *dmi, const gchar *ref);
GList              *gda_data_model_index_get_column_index_list (GdaDataModelIndex *dmi);
void                gda_data_model_index_set_column_index_list (GdaDataModelIndex *dmi, GList *col_idx_list);

G_END_DECLS

#endif
