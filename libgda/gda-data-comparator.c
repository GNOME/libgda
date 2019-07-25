/*
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#define G_LOG_DOMAIN "GDA-data-comparator"

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-data-comparator.h"
#include "gda-marshal.h"
#include "gda-data-model.h"

/* 
 * Main static functions 
 */
static void gda_data_comparator_class_init (GdaDataComparatorClass * class);
static void gda_data_comparator_init (GdaDataComparator *srv);
static void gda_data_comparator_dispose (GObject *object);

static void gda_data_comparator_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_data_comparator_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

static void gda_diff_free (GdaDiff *diff);

/* signals */
enum
{
        DIFF_COMPUTED,
        LAST_SIGNAL
};

static gint gda_data_comparator_signals[LAST_SIGNAL] = { 0 };


/* properties */
enum
{
	PROP_0,
	PROP_OLD_MODEL,
	PROP_NEW_MODEL
};

typedef struct {
	GdaDataModel      *old_model;
	GdaDataModel      *new_model;
	gint               nb_key_columns;
	gint              *key_columns;
	GArray            *diffs; /* array of GdaDiff pointers */
} GdaDataComparatorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaDataComparator, gda_data_comparator, G_TYPE_OBJECT)


/* module error */
GQuark gda_data_comparator_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_data_comparator_error");
	return quark;
}

static gboolean
diff_computed_accumulator (G_GNUC_UNUSED GSignalInvocationHint *ihint,
			   GValue *return_accu,
			   const GValue *handler_return,
			   G_GNUC_UNUSED gpointer data)
{
        gboolean thisvalue;

        thisvalue = g_value_get_boolean (handler_return);
        g_value_set_boolean (return_accu, thisvalue);

        return !thisvalue; /* stop signal if 'thisvalue' is FALSE */
}

static gboolean
m_diff_computed (G_GNUC_UNUSED GdaDataComparator *comparator, G_GNUC_UNUSED GdaDiff *diff)
{
        return FALSE; /* default is to allow differences computing to proceed (understand it as: FALSE => don't stop) */
}

static void
gda_data_comparator_class_init (GdaDataComparatorClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	/* signals */

	gda_data_comparator_signals [DIFF_COMPUTED] =
		g_signal_new ("diff-computed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataComparatorClass, diff_computed),
                              diff_computed_accumulator, NULL,
                              _gda_marshal_BOOLEAN__POINTER, G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);

	class->diff_computed = m_diff_computed;

	/* virtual functions */
	object_class->dispose = gda_data_comparator_dispose;

	/* Properties */
	object_class->set_property = gda_data_comparator_set_property;
	object_class->get_property = gda_data_comparator_get_property;

	g_object_class_install_property (object_class, PROP_OLD_MODEL,
					 g_param_spec_object ("old-model", _("Old data model"), NULL,
                                                               GDA_TYPE_DATA_MODEL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_NEW_MODEL,
					 g_param_spec_object ("new-model", _("New data model"), NULL,
                                                               GDA_TYPE_DATA_MODEL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
gda_data_comparator_init (GdaDataComparator *comparator)
{
	GdaDataComparatorPrivate *priv = gda_data_comparator_get_instance_private (comparator);
	priv->diffs = g_array_new (FALSE, FALSE, sizeof (GdaDiff *));
}

/**
 * gda_data_comparator_new:
 * @old_model: Data model to which the modifications should be applied
 * @new_model: Target data model.
 *
 * Creates a new comparator to compute the differences from @old_model to @new_model: if one applies
 * all the computed differences (as #GdaDiff structures) to @old_model, the resulting data model
 * should have the same contents as @new_model.
 *
 * Returns: (type GdaDataComparator) (transfer full): a new #GdaDataComparator object
 */
GObject *
gda_data_comparator_new (GdaDataModel *old_model, GdaDataModel *new_model)
{
	GObject *obj;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (old_model), NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (new_model), NULL);

	obj = g_object_new (GDA_TYPE_DATA_COMPARATOR, "old-model", old_model, "new-model", new_model, NULL);

	return obj;
}

static void
clean_diff (GdaDataComparator *comparator)
{
	GdaDataComparatorPrivate *priv = gda_data_comparator_get_instance_private (comparator);
	if (priv->diffs) {
		gsize i;
		for (i = 0; i < priv->diffs->len; i++) {
			GdaDiff *diff = g_array_index (priv->diffs, GdaDiff *, i);
			gda_diff_free (diff);
		}
		g_array_free (priv->diffs, TRUE);
	}
	priv->diffs = g_array_new (FALSE, FALSE, sizeof (GdaDiff *));
}

static void
gda_data_comparator_dispose (GObject *object)
{
	GdaDataComparator *comparator;

	g_return_if_fail (GDA_IS_DATA_COMPARATOR (object));

	comparator = GDA_DATA_COMPARATOR (object);
	GdaDataComparatorPrivate *priv = gda_data_comparator_get_instance_private (comparator);
	if (priv->old_model) {
		g_object_unref (priv->old_model);
		priv->old_model = NULL;
	}
	if (priv->new_model) {
		g_object_unref (priv->new_model);
		priv->new_model = NULL;
	}
	clean_diff (comparator);
	g_free (priv->key_columns);
	g_array_free (priv->diffs, TRUE);

	/* parent class */
	G_OBJECT_CLASS (gda_data_comparator_parent_class)->dispose (object);
}

static void
gda_data_comparator_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	GdaDataComparator *comparator;
	comparator = GDA_DATA_COMPARATOR (object);
	GdaDataComparatorPrivate *priv = gda_data_comparator_get_instance_private (comparator);
	if (priv) {
		GdaDataModel *model;

		switch (param_id) {
		case PROP_OLD_MODEL:
			model = (GdaDataModel*) g_value_get_object (value);
			if (priv->old_model && (priv->old_model != model)) {
				/* re-init */
				clean_diff (comparator);
				g_object_unref (priv->old_model);
				g_free (priv->key_columns);
				priv->key_columns = NULL;
			}
			priv->old_model = model;
			if (model)
				g_object_ref (model);
			break;
		case PROP_NEW_MODEL:
			model = (GdaDataModel*) g_value_get_object (value);
			if (priv->new_model && (priv->new_model != model)) {
				/* re-init */
				clean_diff (comparator);
				g_object_unref (priv->new_model);
				g_free (priv->key_columns);
				priv->key_columns = NULL;
			}
			priv->new_model = model;
			if (model)
				g_object_ref (model);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gda_data_comparator_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	GdaDataComparator *comparator;

	comparator = GDA_DATA_COMPARATOR (object);
	GdaDataComparatorPrivate *priv = gda_data_comparator_get_instance_private (comparator);
	if (priv) {
		switch (param_id) {
		case PROP_OLD_MODEL:
			g_value_set_object (value, priv->old_model);
			break;
		case PROP_NEW_MODEL:
			g_value_set_object (value, priv->new_model);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gda_diff_free (GdaDiff *diff)
{
	g_hash_table_destroy (diff->values);
	g_free (diff);
}

/**
 * gda_data_comparator_set_key_columns:
 * @comp: a #GdaDataComparator object
 * @nb_cols: the size of the @col_numbers array
 * @col_numbers: (array length=nb_cols): an array of @nb_cols values
 *
 * Defines the columns which will be used as a key when searching data. This is not mandatory but
 * will speed things up as less data will be processed.
 */
void
gda_data_comparator_set_key_columns (GdaDataComparator *comp, const gint *col_numbers, gint nb_cols)
{
	g_return_if_fail (GDA_IS_DATA_COMPARATOR (comp));
	GdaDataComparatorPrivate *priv = gda_data_comparator_get_instance_private (comp);

	g_free (priv->key_columns);
	priv->key_columns = NULL;
	if (nb_cols > 0) {
		priv->nb_key_columns = nb_cols;
		priv->key_columns = g_new (gint, nb_cols);
		memcpy (priv->key_columns, col_numbers, sizeof (gint) * nb_cols); /* Flawfinder: ignore */
	}
}

/*
 * Find the row in @priv->old_model from the values of @priv->new_model at line @row
 * It is assumed that both data model have the same number of columns and of "compatible" types.
 *
 * Returns: 
 *          -2 if an error occurred
 *          -1 if not found, 
 *          >=0 if found (if changes need to be made, then @out_has_changed is set to TRUE).
 */
static gint
find_row_in_model (GdaDataComparator *comp, gint row, gboolean *out_has_changed, GError **error)
{
	gint i, erow;
	gint ncols;
	GSList *values = NULL;
	GdaDataComparatorPrivate *priv = gda_data_comparator_get_instance_private (comp);
	
	*out_has_changed = FALSE;
	ncols = gda_data_model_get_n_columns (priv->old_model);
	if (!priv->key_columns) {
		priv->nb_key_columns = ncols;
		priv->key_columns = g_new (gint, ncols);
		for (i = 0; i < ncols; i++)
			priv->key_columns [i] = i;
	}

	for (i = 0; i < priv->nb_key_columns; i++) {
		const GValue *cvalue;
		cvalue = gda_data_model_get_value_at (priv->new_model, priv->key_columns[i], row, error);
		if (!cvalue) {
			if (values)
				g_slist_free (values);
			return -2;
		}
		values = g_slist_append (values, (gpointer) cvalue);
	}

	erow = gda_data_model_get_row_from_values (priv->old_model, values, priv->key_columns);
	g_slist_free (values);
	
	if (erow >= 0) {
		gboolean changed = FALSE;
		for (i = 0; i < ncols; i++) {
			const GValue *v1, *v2;
			v1 = gda_data_model_get_value_at (priv->old_model, i, erow, error);
			if (!v1)
				return -2;
			v2 = gda_data_model_get_value_at (priv->new_model, i, row, error);
			if (!v2) 
				return -2;
			if (gda_value_compare (v1, v2)) {
				changed = TRUE;
				break;
			}
		}
		*out_has_changed = changed;
	}
	
	return erow;
}

/**
 * gda_data_comparator_compute_diff:
 * @comp: a #GdaDataComparator object
 * @error: a place to store errors, or %NULL
 *
 * Actually computes the differences between the data models for which @comp is defined. 
 *
 * For each difference computed, stored in a #GdaDiff structure, the "diff-computed" signal is emitted.
 * If one connects to this signal and returns FALSE in the signal handler, then computing differences will be
 * stopped and an error will be returned.
 *
 * Returns: TRUE if all the differences have been successfully computed, and FALSE if an error occurred
 */
gboolean
gda_data_comparator_compute_diff (GdaDataComparator *comp, GError **error)
{
	gint oncols, nncols, i;
	gint onrows, nnrows;
	gboolean *rows_to_del = NULL;

	g_return_val_if_fail (GDA_IS_DATA_COMPARATOR (comp), FALSE);
	GdaDataComparatorPrivate *priv = gda_data_comparator_get_instance_private (comp);

	clean_diff (comp);

	/* check setup */
	if (!priv->old_model) {
		g_set_error (error, GDA_DATA_COMPARATOR_ERROR, GDA_DATA_COMPARATOR_MISSING_DATA_MODEL_ERROR,
			      "%s", _("Missing original data model"));
		return FALSE;
	}
	if (!priv->new_model) {
		g_set_error (error, GDA_DATA_COMPARATOR_ERROR, GDA_DATA_COMPARATOR_MISSING_DATA_MODEL_ERROR,
			      "%s", _("Missing new data model"));
		return FALSE;
	}
	if (! (gda_data_model_get_access_flags (priv->old_model) & GDA_DATA_MODEL_ACCESS_RANDOM) ||
	    ! (gda_data_model_get_access_flags (priv->new_model) & GDA_DATA_MODEL_ACCESS_RANDOM)) {
		g_set_error (error, GDA_DATA_COMPARATOR_ERROR, GDA_DATA_COMPARATOR_MODEL_ACCESS_ERROR,
			      "%s", _("Data models must support random access model"));
		return FALSE;
	}

	/* compare columns */
	oncols = gda_data_model_get_n_columns (priv->old_model);
	nncols = gda_data_model_get_n_columns (priv->new_model);
	if (oncols != nncols) {
		g_set_error (error, GDA_DATA_COMPARATOR_ERROR, GDA_DATA_COMPARATOR_MISSING_DATA_MODEL_ERROR,
			      "%s", _("Data models to compare don't have the same number of columns"));
		return FALSE;
	}

	for (i = 0; i < oncols; i++) {
		GdaColumn *ocol, *ncol;
		ocol = gda_data_model_describe_column (priv->old_model, i);
		ncol = gda_data_model_describe_column (priv->new_model, i);
		if (gda_column_get_g_type (ocol) != gda_column_get_g_type (ncol)) {
			g_set_error (error, GDA_DATA_COMPARATOR_ERROR, 
				     GDA_DATA_COMPARATOR_COLUMN_TYPES_MISMATCH_ERROR,
				     _("Type mismatch for column %d: '%s' and '%s'"), i,
				     g_type_name (gda_column_get_g_type (ocol)),
				     g_type_name (gda_column_get_g_type (ncol)));
			return FALSE;
		}
	}

	/* actual differences computations : rows to insert / update */
	onrows = gda_data_model_get_n_rows (priv->old_model);
	if (onrows < 0) {
		g_set_error (error, GDA_DATA_COMPARATOR_ERROR, GDA_DATA_COMPARATOR_MODEL_ACCESS_ERROR,
			     "%s", _("Can't get the number of rows of data model to compare from"));
		return FALSE;
	}
	nnrows = gda_data_model_get_n_rows (priv->new_model);
	if (nnrows < 0) {
		g_set_error (error, GDA_DATA_COMPARATOR_ERROR, GDA_DATA_COMPARATOR_MODEL_ACCESS_ERROR,
			     "%s", _("Can't get the number of rows of data model to compare to"));
		return FALSE;
	}
	if (onrows > 0) {
		rows_to_del = g_new (gboolean, onrows);
		memset (rows_to_del, TRUE, sizeof (gboolean) * onrows);
	}
	for (i = 0; i < nnrows; i++) {
		gint erow = -1;
		gboolean has_changed = FALSE;
		GdaDiff *diff = NULL;
		gboolean stop;
		
		erow = find_row_in_model (comp, i, &has_changed, error);
		
#ifdef DEBUG_STORE_MODIFY
		g_print ("FIND row %d returned row %d (%s)\n", i, erow, 
			 has_changed ? "CHANGED" : "unchanged");
#endif
		if (erow == -1) {
			gint j;
			diff = g_new0 (GdaDiff, 1);
			diff->type = GDA_DIFF_ADD_ROW;
			diff->old_row = -1;
			diff->new_row = i;
			diff->values = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
							      (GDestroyNotify) gda_value_free);
			for (j = 0; j < oncols; j++) {
				const GValue *cvalue;
				cvalue = gda_data_model_get_value_at (priv->new_model, j, i, error);
				if (!cvalue) {
					/* an error occurred */
					g_free (rows_to_del);
					gda_diff_free (diff);
					return FALSE;
				}
				g_hash_table_insert (diff->values, g_strdup_printf ("+%d", j),
						     gda_value_copy (cvalue));
			}
		}
		else if (erow < -1) {
			/* an error occurred */
			g_free (rows_to_del);
			return FALSE;
		}
		else if (has_changed) {
			gint j;
			diff = g_new0 (GdaDiff, 1);
			diff->type = GDA_DIFF_MODIFY_ROW;
			rows_to_del [erow] = FALSE;
			diff->old_row = erow;
			diff->new_row = i;
			diff->values = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
							      (GDestroyNotify) gda_value_free);
			for (j = 0; j < oncols; j++) {
				const GValue *cvalue;
				cvalue = gda_data_model_get_value_at (priv->new_model, j, i, error);
				if (!cvalue) {
					/* an error occurred */
					g_free (rows_to_del);
					gda_diff_free (diff);
					return FALSE;
				}
				g_hash_table_insert (diff->values, g_strdup_printf ("+%d", j),
						     gda_value_copy (cvalue));
				cvalue = gda_data_model_get_value_at (priv->old_model, j, i, error);
				if (!cvalue) {
					/* an error occurred */
					g_free (rows_to_del);
					gda_diff_free (diff);
					return FALSE;
				}
				g_hash_table_insert (diff->values, g_strdup_printf ("-%d", j),
						     gda_value_copy (cvalue));
			}
		}
		else
			rows_to_del [erow] = FALSE; /* row has not been changed */

		if (diff) {
			g_array_append_val (priv->diffs, diff);
			g_signal_emit (comp, gda_data_comparator_signals [DIFF_COMPUTED], 0, diff, &stop);
			if (stop) {
				g_set_error (error, GDA_DATA_COMPARATOR_ERROR,
					     GDA_DATA_COMPARATOR_USER_CANCELLED_ERROR,
					      "%s", _("Differences computation cancelled on signal handling"));
				g_free (rows_to_del);
				return FALSE;
			}
		}
	}

	/* actual differences computations : rows to delete */
	for (i = 0; i < onrows; i++) {
		GdaDiff *diff = NULL;
		gboolean stop;
		if (rows_to_del [i]) {
			gint j;
			diff = g_new0 (GdaDiff, 1);
			diff->type = GDA_DIFF_REMOVE_ROW;
			diff->old_row = i;
			diff->new_row = -1;
			diff->values = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
							      (GDestroyNotify) gda_value_free);
			for (j = 0; j < oncols; j++)  {
				const GValue *cvalue;
				cvalue = gda_data_model_get_value_at (priv->old_model, j, i, error);
				if (!cvalue) {
					/* an error occurred */
					g_free (rows_to_del);
					gda_diff_free (diff);
					return FALSE;
				}
				g_hash_table_insert (diff->values, g_strdup_printf ("-%d", j),
						     gda_value_copy (cvalue));
			}
			g_array_append_val (priv->diffs, diff);
			g_signal_emit (comp, gda_data_comparator_signals [DIFF_COMPUTED], 0, diff, &stop);
			if (stop) {
				g_set_error (error, GDA_DATA_COMPARATOR_ERROR,
					     GDA_DATA_COMPARATOR_USER_CANCELLED_ERROR,
					      "%s", _("Differences computation cancelled on signal handling"));
				g_free (rows_to_del);
				return FALSE;
			}
		}
	}

	g_free (rows_to_del);
	return TRUE;
}

/**
 * gda_data_comparator_get_n_diffs:
 * @comp: a #GdaDataComparator object
 *
 * Get the number of differences as computed by the last time gda_data_comparator_compute_diff() was called.
 *
 * Returns: the number of computed differences
 */
gint
gda_data_comparator_get_n_diffs  (GdaDataComparator *comp)
{
	g_return_val_if_fail (GDA_IS_DATA_COMPARATOR (comp), 0);
	GdaDataComparatorPrivate *priv = gda_data_comparator_get_instance_private (comp);

	return priv->diffs->len;
}

/**
 * gda_data_comparator_get_diff:
 * @comp: a #GdaDataComparator object
 * @pos: the requested difference number (starting at 0)
 *
 * Get a pointer to the #GdaDiff structure representing the difference which number is @pos
 *
 * Returns: (transfer none): a pointer to a #GdaDiff, or %NULL if @pos is invalid
 */
const GdaDiff *
gda_data_comparator_get_diff (GdaDataComparator *comp, gint pos)
{
	g_return_val_if_fail (GDA_IS_DATA_COMPARATOR (comp), NULL);
	GdaDataComparatorPrivate *priv = gda_data_comparator_get_instance_private (comp);

	return g_array_index (priv->diffs, GdaDiff*, pos);
}

void
copy_hash (gpointer key, gpointer value, GdaDiff *dst)
{
  g_hash_table_insert (dst->values, g_strdup (key), gda_value_copy (value));
}

GdaDiff*
gda_diff_copy (GdaDiff *src)
{
  GdaDiff *dst = g_new0 (GdaDiff, 1);
  dst->type = src->type;
  dst->old_row = src->old_row;
  dst->new_row = src->new_row;
  dst->values = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
                                       (GDestroyNotify) gda_value_free);
  g_hash_table_foreach (src->values, (GHFunc)copy_hash, dst);
  return dst;
}

G_DEFINE_BOXED_TYPE (GdaDiff, gda_diff, gda_diff_copy, gda_diff_free)
