/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __GDA_DATA_COMPARATOR_H_
#define __GDA_DATA_COMPARATOR_H_

#include "gda-decl.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_COMPARATOR          (gda_data_comparator_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaDataComparator, gda_data_comparator, GDA, DATA_COMPARATOR, GObject)

/* error reporting */
extern GQuark gda_data_comparator_error_quark (void);
#define GDA_DATA_COMPARATOR_ERROR gda_data_comparator_error_quark ()

typedef enum {
	GDA_DATA_COMPARATOR_MISSING_DATA_MODEL_ERROR,
	GDA_DATA_COMPARATOR_COLUMN_TYPES_MISMATCH_ERROR,
	GDA_DATA_COMPARATOR_MODEL_ACCESS_ERROR,
	GDA_DATA_COMPARATOR_USER_CANCELLED_ERROR
} GdaDataComparatorError;

/* differences reporting */
typedef enum {
	GDA_DIFF_ADD_ROW,
	GDA_DIFF_REMOVE_ROW,
	GDA_DIFF_MODIFY_ROW
} GdaDiffType;

typedef struct {
	GdaDiffType  type;
	gint         old_row;
	gint         new_row;
	GHashTable  *values; /* key = ('+' or '-') and a column position starting at 0 (string)
			      * value = a GValue pointer */
} GdaDiff;

#define GDA_TYPE_DIFF (gda_diff_get_type ())

/* struct for the object's class */
struct _GdaDataComparatorClass
{
	GObjectClass              parent_class;
	gboolean               (* diff_computed)  (GdaDataComparator *comp, GdaDiff *diff);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-data-comparator
 * @short_description: Simple data model's contents comparison
 * @title: GdaDataComparator
 * @stability: Stable
 * @see_also: #GdaDataModel
 *
 * The #GdaDataComparator is a simple object which takes two #GdaDataModel objects and compare them.
 * Actual comparison is performed when the gda_data_comparator_compute_diff() is called; for each
 * difference found, the <link linkend="GdaDataComparator-diff-computed">diff-computed</link> signal
 * is emitted (any user installed signal handler which returns FALSE stops the computing process).
 *
 * There are some limitations to this object:
 * <itemizedlist>
 *   <listitem><para>The data models compared must have the same number and type of columns</para></listitem>
 *   <listitem><para>The comparison is done column-for-column: one cannot omit columns in the comparison, nor compare
 *   columns with different positions</para></listitem>
 * </itemizedlist>
 */


GObject          *gda_data_comparator_new             (GdaDataModel *old_model, GdaDataModel *new_model);
void              gda_data_comparator_set_key_columns (GdaDataComparator *comp, const gint *col_numbers, gint nb_cols);
gboolean          gda_data_comparator_compute_diff    (GdaDataComparator *comp, GError **error);
gint              gda_data_comparator_get_n_diffs     (GdaDataComparator *comp);
const GdaDiff    *gda_data_comparator_get_diff        (GdaDataComparator *comp, gint pos);

GType             gda_diff_get_type                   (void) G_GNUC_CONST;

G_END_DECLS

#endif
