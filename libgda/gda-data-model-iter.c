/*
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2015 Corentin Noël <corentin@elementary.io>
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
#define G_LOG_DOMAIN "GDA-data-model-iter"

#include <glib/gi18n-lib.h>
#include <string.h>
#include "gda-data-model-iter.h"
#include "gda-data-model-iter-extra.h"
#include "gda-data-model.h"
#include "gda-data-model-private.h"
#include "gda-holder.h"
#include "gda-marshal.h"
#include "gda-data-proxy.h"
#include "gda-enums.h"
#include "gda-data-select.h"

/* private structure */
typedef struct
{
	GWeakRef               data_model; /* may be %NULL because there is only a weak ref on it */
	gulong                 model_changes_signals[3];
	gboolean               keep_param_changes;
	gint                   row; /* -1 if row is unknown */
} GdaDataModelIterPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GdaDataModelIter, gda_data_model_iter, GDA_TYPE_SET)

/*
 * Main static functions 
 */
static void gda_data_model_iter_finalize (GObject *object);

static void gda_data_model_iter_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_data_model_iter_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

/* Virtual default methods*/
static gboolean
real_gda_data_model_iter_move_to_row (GdaDataModelIter *iter, gint row);
static gboolean
real_gda_data_model_iter_move_prev (GdaDataModelIter *iter);

gboolean
real_gda_data_model_iter_move_next (GdaDataModelIter *iter);

/* follow model changes */
static void model_row_updated_cb (GdaDataModel *model, gint row, GdaDataModelIter *iter);
static void model_row_removed_cb (GdaDataModel *model, gint row, GdaDataModelIter *iter);
static void model_reset_cb (GdaDataModel *model, GdaDataModelIter *iter);

static GError *validate_holder_change_cb (GdaSet *paramlist, GdaHolder *param, const GValue *new_value);
static void holder_attr_changed_cb (GdaSet *paramlist, GdaHolder *param, const gchar *att_name, const GValue *att_value);

/* signals */
enum
{
        ROW_CHANGED,
	END_OF_DATA,
        LAST_SIGNAL
};

static gint gda_data_model_iter_signals[LAST_SIGNAL] = { 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_DATA_MODEL,
	PROP_CURRENT_ROW,
	PROP_UPDATE_MODEL
};

/* module error */
GQuark gda_data_model_iter_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_data_model_iter_error");
	return quark;
}

static void
gda_data_model_iter_class_init (GdaDataModelIterClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GdaSetClass *paramlist_class = GDA_SET_CLASS (class);
	/**
	 * GdaDataModelIter::row-changed:
	 * @iter: the #GdaDataModelIter
	 * @row: the new iter's row
	 *
	 * Gets emitted when the row @iter is currently pointing has changed
	 */
	gda_data_model_iter_signals [ROW_CHANGED] =
                g_signal_new ("row-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataModelIterClass, row_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
	/**
	 * GdaDataModelIter::end-of-data:
	 * @iter: the #GdaDataModelIter
	 *
	 * Gets emitted when @iter has reached the end of available data (which means the previous
	 * row it was on was the last one).
	 */
	gda_data_model_iter_signals [END_OF_DATA] =
                g_signal_new ("end-of-data",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataModelIterClass, end_of_data),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	class->move_to_row = real_gda_data_model_iter_move_to_row;
	class->move_next = real_gda_data_model_iter_move_next;
	class->move_prev = real_gda_data_model_iter_move_prev;
	class->set_value_at = gda_data_model_iter_set_value_at;
	class->row_changed = NULL;
	class->end_of_data = NULL;

	object_class->finalize = gda_data_model_iter_finalize;
	paramlist_class->validate_holder_change = validate_holder_change_cb;
	paramlist_class->holder_attr_changed = holder_attr_changed_cb;

	/* Properties */
	object_class->set_property = gda_data_model_iter_set_property;
	object_class->get_property = gda_data_model_iter_get_property;
	g_object_class_install_property (object_class, PROP_DATA_MODEL,
					 g_param_spec_object ("data-model", NULL, "Data model for which the iter is for", 
                                                               GDA_TYPE_DATA_MODEL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE |
								G_PARAM_CONSTRUCT)));
	g_object_class_install_property (object_class, PROP_CURRENT_ROW,
					 g_param_spec_int ("current-row", NULL, "Current represented row in the data model", 
							   -1, G_MAXINT, -1,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_UPDATE_MODEL,
					 g_param_spec_boolean ("update-model", "Tells if parameters changes are forwarded "
							       "to the GdaDataModel", NULL, TRUE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
gda_data_model_iter_init (GdaDataModelIter *iter)
{
	GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);
	g_weak_ref_set (&priv->data_model, NULL);
	priv->row = -1;
	priv->model_changes_signals[0] = 0;
	priv->model_changes_signals[1] = 0;
	priv->model_changes_signals[2] = 0;
	priv->keep_param_changes = FALSE;
}

static void 
model_row_updated_cb (GdaDataModel *model, gint row, GdaDataModelIter *iter)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (iter));

	GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	GdaDataModel *obj = g_weak_ref_get (&priv->data_model);
	if (obj == NULL) {
		g_warning (_("Iter has an invalid/non-existent reference to a data model"));
		return;
	}
	if (model != obj) {
		g_warning (_("Referenced data model in iterator doesn't belong to given data model"));
		g_object_unref (obj);
		return;
	}
	g_object_unref (obj);
	/* sync parameters with the new values in row */
	if (priv->row == row) {
		priv->keep_param_changes = TRUE;
		gda_data_model_iter_move_to_row (iter, row);
		priv->keep_param_changes = FALSE;
	}
}

static void 
model_row_removed_cb (G_GNUC_UNUSED GdaDataModel *model, gint row, GdaDataModelIter *iter)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (iter));

	GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	if (priv->row < 0)
		/* we are not concerned by handling this signal */
		return;

	/* if removed row is the one corresponding to iter, 
	 * then make all the parameters invalid */
	if (priv->row == row) {
		gda_data_model_iter_invalidate_contents (iter);
		gda_data_model_iter_move_to_row (iter, -1);
	}
	else {
		/* shift iter's row by one to keep good numbers */
		if (priv->row > row)
			priv->row--;
	}
}

static void
define_holder_for_data_model_column (GdaDataModel *model, gint col, GdaDataModelIter *iter)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (iter));

	gchar *str;
	GdaHolder *param;
	GdaColumn *column;

	column = gda_data_model_describe_column (model, col);
	param = (GdaHolder *) g_object_new (GDA_TYPE_HOLDER, 
					    "g-type", 
					    gda_column_get_g_type (column), NULL);

	/*g_print ("COL %d allow null=%d\n", col, gda_column_get_allow_null (column));*/
	gda_holder_set_not_null (param, !gda_column_get_allow_null (column));
	g_object_get (G_OBJECT (column), "id", &str, NULL);
	if (str) {
		g_object_set (G_OBJECT (param), "id", str, NULL);
		g_free (str);
	}
	else {
		const gchar *cstr;
		gchar *tmp;

		/* ID attribute */
		cstr = gda_column_get_description (column);
		if (!cstr)
			cstr = gda_column_get_name (column);
		if (cstr)
			tmp = (gchar *) cstr;
		else
			tmp = g_strdup_printf ("col%d", col);

		if (gda_set_get_holder ((GdaSet *) iter, tmp)) {
			gint e;
			for (e = 1; ; e++) {
				gchar *tmp2 = g_strdup_printf ("%s_%d", tmp, e);
				if (! gda_set_get_holder ((GdaSet *) iter, tmp2)) {
					g_object_set (G_OBJECT (param), "id", tmp2, NULL);
					g_free (tmp2);
					break;
				}
				g_free (tmp2);
			}
		}
		else
			g_object_set (G_OBJECT (param), "id", tmp, NULL);

		if (!cstr)
			g_free (tmp);

		/* other attributes */
		cstr = gda_column_get_description (column);
		if (cstr)
			g_object_set (G_OBJECT (param), "description", cstr, NULL);
		cstr = gda_column_get_name (column);
		if (cstr)
			g_object_set (G_OBJECT (param), "name", cstr, NULL);
	}
	if (gda_column_get_default_value (column))
		gda_holder_set_default_value (param, gda_column_get_default_value (column));
	else if (gda_column_get_auto_increment (column)) {
		GValue *v = gda_value_new_null ();
		gda_holder_set_default_value (param, v);
		gda_value_free (v);
	}
	gda_set_add_holder ((GdaSet *) iter, param);
	g_object_set_data (G_OBJECT (param), "model_col", GINT_TO_POINTER (col + 1));
	g_object_unref (param);
}

static void
model_reset_cb (GdaDataModel *model, GdaDataModelIter *iter)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (iter));

	gint row;

	row = gda_data_model_iter_get_row (iter);
	
	/* adjust GdaHolder's type if a column's type has changed from GDA_TYPE_NULL
	 * to something else */
	gint i, nbcols;
	GSList *list;
	nbcols = gda_data_model_get_n_columns (model);
	if (GDA_IS_DATA_PROXY (model))
		nbcols = nbcols / 2;

	for (i = 0, list = gda_set_get_holders (((GdaSet*) iter));
	     (i < nbcols) && list;
	     i++, list = list->next) {
		GdaColumn *col;
		col = gda_data_model_describe_column (model, i);

		if (gda_holder_get_g_type ((GdaHolder *) list->data) == GDA_TYPE_NULL) {
			if (gda_column_get_g_type (col) != GDA_TYPE_NULL)
				g_object_set (G_OBJECT (list->data), "g-type",
					      gda_column_get_g_type (col), NULL);
		}
		else if (gda_holder_get_g_type ((GdaHolder *) list->data) !=
			 gda_column_get_g_type (col))
			break;
	}

	if (list) {
		/* remove remaining GdaHolder objects */
		GSList *l2;
		l2 = g_slist_copy (list);
		for (list = l2; list; list = list->next)
			gda_set_remove_holder ((GdaSet*) iter, (GdaHolder *) list->data);
		g_slist_free (l2);
		row = -1; /* force actual reset of iterator's position */
	}

	if (i < nbcols) {
		for (; i < nbcols; i++)
			define_holder_for_data_model_column (model, i, iter);
		row = -1; /* force actual reset of iterator's position */
	}

	gda_data_model_iter_invalidate_contents (iter);
	if (row >= 0)
		gda_data_model_iter_move_to_row (iter, row);
	else
		/* reset the iter to before the 1st row */
		gda_data_model_iter_move_to_row (iter, -1);
}

/*
 * This function is called when a parameter in @paramlist is changed
 * to make sure the change is propagated to the GdaDataModel
 * paramlist is an iter for, return an error if the data model could not be modified
 */
static GError *
validate_holder_change_cb (GdaSet *paramlist, GdaHolder *param, const GValue *new_value)
{
	g_return_val_if_fail (paramlist != NULL, NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (paramlist), NULL);

	GdaDataModelIter *iter;
	gint col;
	GError *error = NULL;
	GValue *nvalue;

	iter = GDA_DATA_MODEL_ITER (paramlist);

	GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);
	GdaDataModel *model = g_weak_ref_get (&priv->data_model);

	if (model == NULL) {
		g_set_error (&error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_INVALID,
				     _("Invalid data model. Can't validate holder"));
		return error;
	}

	if (new_value)
		nvalue = (GValue*) new_value;
	else
		nvalue = gda_value_new_null();

	iter = (GdaDataModelIter *) paramlist;
	if (!priv->keep_param_changes && (priv->row >= 0) && (GDA_IS_DATA_MODEL (model))) {
		g_signal_handler_block (model, priv->model_changes_signals [0]);
		g_signal_handler_block (model, priv->model_changes_signals [1]);
		
		/* propagate the value update to the data model */
		col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (param), "model_col")) - 1;
		if (col < 0) {
			g_set_error (&error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
				     _("Column %d out of range (0-%d)"), col, g_slist_length (gda_set_get_holders (paramlist)) - 1);
		}
		else if (GDA_DATA_MODEL_ITER_GET_CLASS (iter)->set_value_at) {
			(GDA_DATA_MODEL_ITER_GET_CLASS (iter)->set_value_at)
				(iter, col, nvalue, &error);
		}
		else {
			g_print ("using Data Model default set_value_at\n");
			gda_data_model_set_value_at (model, col, priv->row, nvalue, &error);
		}
		
		g_signal_handler_unblock (model, priv->model_changes_signals [0]);
		g_signal_handler_unblock (model, priv->model_changes_signals [1]);
	}
	if (!new_value)
		gda_value_free (nvalue);

	if (!error && GDA_SET_CLASS(gda_data_model_iter_parent_class)->validate_holder_change) {
		g_object_unref (model);
		/* for the parent class */
		return (GDA_SET_CLASS(gda_data_model_iter_parent_class))->validate_holder_change (paramlist, param, new_value);
	}

	g_object_unref (model);

	return error;
}

/*
 * This function is called when a parameter in @paramlist has its attributes changed
 * to make sure the change is propagated to the GdaDataModel
 * paramlist is an iter for
 */
static void
holder_attr_changed_cb (GdaSet *paramlist, GdaHolder *param, const gchar *att_name, const GValue *att_value)
{
	g_return_if_fail (paramlist != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (paramlist));

	GdaDataModelIter *iter;
	gint col;
	gboolean toset = FALSE;

	iter = GDA_DATA_MODEL_ITER (paramlist);

	GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	GdaDataModel *model = g_weak_ref_get (&priv->data_model);

	if (model == NULL) {
		return;
	}

	if (!GDA_IS_DATA_PROXY (model)) {
		g_object_unref (model);
		return;
	}

	if (!strcmp (att_name, GDA_ATTRIBUTE_IS_DEFAULT) &&
	    !priv->keep_param_changes && (priv->row >= 0)) {
		g_signal_handler_block (model, priv->model_changes_signals [0]);
		g_signal_handler_block (model, priv->model_changes_signals [1]);
		
		/* propagate the value update to the data model */
		col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (param), "model_col")) - 1;
		g_return_if_fail (col >= 0);
		
		if (att_value && g_value_get_boolean (att_value))
			toset = TRUE;
		if (toset && gda_holder_get_default_value (param))
			gda_data_proxy_alter_value_attributes (GDA_DATA_PROXY (model),
							       priv->row, col,
							       GDA_VALUE_ATTR_CAN_BE_DEFAULT | GDA_VALUE_ATTR_IS_DEFAULT);
		
		g_signal_handler_unblock (model, priv->model_changes_signals [0]);
		g_signal_handler_unblock (model, priv->model_changes_signals [1]);
	}

	/* for the parent class */
	if ((GDA_SET_CLASS(gda_data_model_iter_parent_class))->holder_attr_changed)
		(GDA_SET_CLASS(gda_data_model_iter_parent_class))->holder_attr_changed (paramlist, param, att_name, att_value);

	g_object_unref (model);
}

static void
gda_data_model_iter_finalize (GObject *object)
{
	GdaDataModelIter *iter;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (object));

	iter = GDA_DATA_MODEL_ITER (object);
	GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);
	GdaDataModel *model = g_weak_ref_get (&priv->data_model);
	if (model != NULL) {
		g_signal_handler_disconnect (model, priv->model_changes_signals [0]);
		g_signal_handler_disconnect (model, priv->model_changes_signals [1]);
		g_signal_handler_disconnect (model, priv->model_changes_signals [2]);
		g_object_unref (model);
	}

	/* parent class */
	G_OBJECT_CLASS(gda_data_model_iter_parent_class)->finalize (object);
}

static void
gda_data_model_iter_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	GdaDataModelIter *iter;
	iter = GDA_DATA_MODEL_ITER (object);
	GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	if (priv) {
		switch (param_id) {
		case PROP_DATA_MODEL: {
			GdaDataModel *model;

			GObject* ptr = g_value_dup_object (value);
			if (!GDA_IS_DATA_MODEL (ptr)) {
				g_object_unref (ptr);
				g_warning (_("Invalid data model object. Can't set property"));
				return;
			}
			model = GDA_DATA_MODEL (ptr);
			GdaDataModel *current = g_weak_ref_get (&priv->data_model);
			if (current != NULL) {
				if (current == model)
					return;
				g_signal_handler_disconnect (current,
							     priv->model_changes_signals [0]);
				g_signal_handler_disconnect (current,
							     priv->model_changes_signals [1]);
				g_signal_handler_disconnect (current,
							     priv->model_changes_signals [2]);
			}

			g_weak_ref_set (&priv->data_model, model);

			priv->model_changes_signals [0] = g_signal_connect (G_OBJECT (model), "row-updated",
										  G_CALLBACK (model_row_updated_cb),
										  iter);
			priv->model_changes_signals [1] = g_signal_connect (G_OBJECT (model), "row-removed",
										  G_CALLBACK (model_row_removed_cb),
										  iter);
			priv->model_changes_signals [2] = g_signal_connect (G_OBJECT (model), "reset",
										  G_CALLBACK (model_reset_cb), iter);
			model_reset_cb (GDA_DATA_MODEL (ptr), iter);
			g_object_unref (ptr);
			break;
		}
		case PROP_CURRENT_ROW:
			if (priv->row != g_value_get_int (value)) {
                                priv->row = g_value_get_int (value);
                                g_signal_emit (G_OBJECT (iter),
                                               gda_data_model_iter_signals[ROW_CHANGED],
                                               0, priv->row);
                        }
			break;
		case PROP_UPDATE_MODEL:
			priv->keep_param_changes = ! g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gda_data_model_iter_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	GdaDataModelIter *iter = GDA_DATA_MODEL_ITER (object);
  GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);
	
	if (priv) {
		switch (param_id) {
		case PROP_DATA_MODEL:
			{
				GObject *obj = g_weak_ref_get (&priv->data_model);
				if (obj != NULL) {
				  g_value_take_object (value, obj);
				}
			}
			break;
		case PROP_CURRENT_ROW:
			g_value_set_int (value, priv->row);
			break;
		case PROP_UPDATE_MODEL:
			g_value_set_boolean (value, ! priv->keep_param_changes);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * gda_data_model_iter_move_to_row: (virtual move_to_row)
 * @iter: a #GdaDataModelIter object
 * @row: the row to set @iter to
 *
 * Synchronizes the values of the parameters in @iter with the values at the @row row.
 *
 * If @row is not a valid row, then the returned value is %FALSE, and the "current-row"
 * property is set to -1 (which means that gda_data_model_iter_is_valid() would return %FALSE),
 * with the exception that if @row is -1, then the returned value is %TRUE.
 *
 * This function can return %FALSE if it was not allowed to be moved (as it emits the
 * "validate-set" signal before being moved).
 *
 * When this function returns %TRUE, then @iter has actually been moved to the next row,
 * but some values may not have been read correctly in the row, in which case the
 * correcsponding #GdaHolder will be left invalid.
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_data_model_iter_move_to_row (GdaDataModelIter *iter, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);

	GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	GdaDataModelIterClass *klass = GDA_DATA_MODEL_ITER_GET_CLASS(iter);
	if (klass->move_to_row) {
		if (klass->move_to_row (iter, row)) {
			g_signal_emit (G_OBJECT (iter),
			              gda_data_model_iter_signals[ROW_CHANGED],
			              0, priv->row);
			return TRUE;
		}
	}
	return FALSE;
}
static gboolean
real_gda_data_model_iter_move_to_row (GdaDataModelIter *iter, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);

	GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);
	GdaDataModel *model = g_weak_ref_get (&priv->data_model);
	gboolean ret = FALSE;

	ret = gda_data_model_iter_move_to_row_default (model, iter, row);
	if (model != NULL) {
		g_object_unref (model);
	}
	return ret;
}

static void
set_param_attributes (GdaHolder *holder, GdaValueAttribute flags)
{
	if (flags & GDA_VALUE_ATTR_IS_DEFAULT)
		gda_holder_set_value_to_default (holder);

	if (flags & GDA_VALUE_ATTR_IS_NULL)
		gda_holder_set_value (holder, NULL, NULL);
	if (flags & GDA_VALUE_ATTR_DATA_NON_VALID)
		gda_holder_force_invalid (holder);
}

/**
 * gda_data_model_iter_move_to_row_default:
 * @model: a #GdaDataModel
 * @iter: a #GdaDataModelIter iterating in @model
 * @row: the requested row
 * 
 * Method reserved to #GdaDataModelIter implementations, should not be called directly.
 *
 * Returns: %TRUE if @iter was moved correctly.
 */
gboolean
gda_data_model_iter_move_to_row_default (GdaDataModel *model, GdaDataModelIter *iter, gint row)
{
	/* default method */
	GSList *list;
	gint col;
	GdaDataModel *test;
	gboolean update_model;

	if ((gda_data_model_iter_get_row (iter) >= 0) &&
	    ! _gda_set_validate ((GdaSet*) iter, NULL))
		return FALSE;

	if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM))
		return FALSE;

	/* validity tests */
	if ((row < 0) || (row >= gda_data_model_get_n_rows (model))) {
		gda_data_model_iter_invalidate_contents (iter);
		g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
		return FALSE;
	}
		
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	
	g_object_get (G_OBJECT (iter), "data-model", &test, NULL);
	g_return_val_if_fail (test == model, FALSE);
	g_object_unref (test);
	
	/* actual sync. */
	g_object_get (G_OBJECT (iter), "update-model", &update_model, NULL);
	g_object_set (G_OBJECT (iter), "update-model", FALSE, NULL);
	for (col = 0, list = gda_set_get_holders ((GdaSet *) iter); list; col++, list = list->next) {
		const GValue *cvalue;
		GError *lerror = NULL;
		cvalue = gda_data_model_get_value_at (model, col, row, &lerror);
		if (!cvalue || 
		    !gda_holder_set_value ((GdaHolder*) list->data, cvalue, &lerror))
			gda_holder_force_invalid_e ((GdaHolder*) list->data, lerror);
		else
			set_param_attributes ((GdaHolder*) list->data, 
					      gda_data_model_get_attributes_at (model, col, row));
	}
	g_object_set (G_OBJECT (iter), "current-row", row, "update-model", update_model, NULL);
	return TRUE;
}


/**
 * gda_data_model_iter_move_next: (virtual move_next)
 * @iter: a #GdaDataModelIter object
 *
 * Moves @iter one row further than where it already is 
 * (synchronizes the values of the parameters in @iter with the values at the new row).
 *
 * If the iterator was on the data model's last row, then it can't be moved forward
 * anymore, and the returned value is %FALSE; note also that the "current-row" property
 * is set to -1 (which means that gda_data_model_iter_is_valid() would return %FALSE)
 *
 * This function can return %FALSE if it was not allowed to be moved (as it emits the
 * "validate-set" signal before being moved).
 *
 * When this function returns %TRUE, then @iter has actually been moved to the next row,
 * but some values may not have been read correctly in the row, in which case the
 * correcsponding #GdaHolder will be left invalid.
 *
 * Returns: %TRUE if the iterator is now at the next row
 */
gboolean
gda_data_model_iter_move_next (GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);

	if ((gda_data_model_iter_get_row (iter) >= 0) &&
		    ! _gda_set_validate ((GdaSet*) iter, NULL))
			return FALSE;

	GdaDataModelIterClass *klass = GDA_DATA_MODEL_ITER_GET_CLASS (iter);
	if (klass->move_next) {
		return klass->move_next (iter);
	}
	return FALSE;
}
gboolean
real_gda_data_model_iter_move_next (GdaDataModelIter *iter)
{
	GdaDataModel *model;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);

  GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);
	gboolean ret = FALSE;

	model = g_weak_ref_get (&priv->data_model);

	ret = gda_data_model_iter_move_next_default (model, iter);
	if (model != NULL) {
		g_object_unref (model);
	}
	return ret;
}

/**
 * gda_data_model_iter_move_next_default:
 * @model: a #GdaDataModel
 * @iter: a #GdaDataModelIter iterating in @model
 * 
 * Method reserved to #GdaDataModelIter implementations, should not be called directly.
 *
 * Returns: %TRUE if @iter was moved correctly.
 */
gboolean
gda_data_model_iter_move_next_default (GdaDataModel *model, GdaDataModelIter *iter)
{
	GSList *list;
	gint col;
	gint row;
	GdaDataModel *test;
	gboolean update_model;

	if ((gda_data_model_iter_get_row (iter) >= 0) &&
	    ! _gda_set_validate ((GdaSet*) iter, NULL))
		return FALSE;

	if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM))
		return FALSE;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	
	g_object_get (G_OBJECT (iter), "data-model", &test, NULL);
	g_return_val_if_fail (test == model, FALSE);
	g_object_unref (test);

	g_object_get (G_OBJECT (iter), "current-row", &row, NULL);
	row++;
	if (row >= gda_data_model_get_n_rows (model)) {
		gda_data_model_iter_invalidate_contents (iter);
		g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
		return FALSE;
	}
	
	/* actual sync. */
	g_object_get (G_OBJECT (iter), "update-model", &update_model, NULL);
	g_object_set (G_OBJECT (iter), "update-model", FALSE, NULL);
	for (col = 0, list = gda_set_get_holders ((GdaSet *) iter); list; col++, list = list->next) {
		const GValue *cvalue;
		GError *lerror = NULL;
		cvalue = gda_data_model_get_value_at (model, col, row, &lerror);
		if (!cvalue || 
		    !gda_holder_set_value ((GdaHolder *) list->data, cvalue, &lerror))
			gda_holder_force_invalid_e ((GdaHolder *) list->data, lerror);
		else
			set_param_attributes ((GdaHolder *) list->data, 
					      gda_data_model_get_attributes_at (model, col, row));
	}
	g_object_set (G_OBJECT (iter), "current-row", row, 
		      "update-model", update_model, NULL);
	return TRUE;
}

/**
 * gda_data_model_iter_move_prev: (virtual move_prev)
 * @iter: a #GdaDataModelIter object
 *
 * Moves @iter one row before where it already is (synchronizes the values of the parameters in @iter 
 * with the values at the new row).
 *
 * If the iterator was on the data model's first row, then it can't be moved backwards
 * anymore, and the returned value is %FALSE; note also that the "current-row" property
 * is set to -1 (which means that gda_data_model_iter_is_valid() would return %FALSE).
 *
 * This function can return %FALSE if it was not allowed to be moved (as it emits the
 * "validate-set" signal before being moved).
 *
 * When this function returns %TRUE, then @iter has actually been moved to the next row,
 * but some values may not have been read correctly in the row, in which case the
 * correcsponding #GdaHolder will be left invalid.
 *
 * Returns: %TRUE if the iterator is now at the previous row
 */
gboolean
gda_data_model_iter_move_prev (GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);

	GdaDataModelIterClass *klass = GDA_DATA_MODEL_ITER_GET_CLASS (iter);
	if (klass->move_prev) {
		return klass->move_prev (iter);
	}
	return FALSE;
}
static gboolean
real_gda_data_model_iter_move_prev (GdaDataModelIter *iter)
{
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);

  GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);
	gboolean ret = FALSE;

	model = g_weak_ref_get (&priv->data_model);
	/* default method */
	ret = gda_data_model_iter_move_prev_default (model, iter);
	if (model != NULL) {
		g_object_unref (model);
	}
	return ret;
}

/**
 * gda_data_model_iter_move_prev_default:
 * @model: a #GdaDataModel
 * @iter: a #GdaDataModelIter iterating in @model
 * 
 * Method reserved to #GdaDataModelIter implementations, should not be called directly.
 *
 * Returns: %TRUE if @iter was moved correctly.
 */
gboolean
gda_data_model_iter_move_prev_default (GdaDataModel *model, GdaDataModelIter *iter)
{
	GSList *list;
	gint col;
	gint row;
	GdaDataModel *test;
	gboolean update_model;

	if ((gda_data_model_iter_get_row (iter) >= 0) &&
	    ! _gda_set_validate ((GdaSet*) iter, NULL))
		return FALSE;

	if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM))
		return FALSE;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	
	g_object_get (G_OBJECT (iter), "data-model", &test, NULL);
	g_return_val_if_fail (test == model, FALSE);
	g_object_unref (test);

	g_object_get (G_OBJECT (iter), "current-row", &row, NULL);
	row--;
	if (row < 0) {
		gda_data_model_iter_invalidate_contents (iter);
		g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
		return FALSE;
	}
	
	/* actual sync. */
	g_object_get (G_OBJECT (iter), "update-model", &update_model, NULL);
	g_object_set (G_OBJECT (iter), "update-model", FALSE, NULL);
	for (col = 0, list = gda_set_get_holders ((GdaSet *) iter); list; col++, list = list->next) {
		const GValue *cvalue;
		GError *lerror = NULL;
		cvalue = gda_data_model_get_value_at (model, col, row, &lerror);
		if (!cvalue || 
		    !gda_holder_set_value ((GdaHolder*) list->data, cvalue, &lerror))
			gda_holder_force_invalid_e ((GdaHolder*) list->data, lerror);
		else
			set_param_attributes ((GdaHolder*) list->data,
					      gda_data_model_get_attributes_at (model, col, row));
	}
	g_object_set (G_OBJECT (iter), "current-row", row, 
		      "update-model", update_model, NULL);
	return TRUE;
}


/**
 * gda_data_model_iter_get_row:
 * @iter: a #GdaDataModelIter object
 *
 * Get the row which @iter represents in the data model
 *
 * Returns: the row number, or -1 if @iter is invalid
 */
gint
gda_data_model_iter_get_row (GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), -1);

  GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	g_return_val_if_fail (priv, -1);

	return priv->row;
}

/**
 * gda_data_model_iter_invalidate_contents:
 * @iter: a #GdaDataModelIter object
 *
 * Declare all the parameters in @iter invalid, without modifying the
 * #GdaDataModel @iter is for or changing the row it represents. This method
 * is for internal usage. Note that for gda_data_model_iter_is_valid() to return %FALSE,
 * it is also necessary to set the "current-row" property to -1.
 */
void
gda_data_model_iter_invalidate_contents (GdaDataModelIter *iter)
{
	GSList *list;
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (iter));

  GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	g_return_if_fail (priv);

	priv->keep_param_changes = TRUE;
	for (list = gda_set_get_holders (GDA_SET (iter)); list; list = list->next)
		gda_holder_force_invalid (GDA_HOLDER (list->data));
	priv->keep_param_changes = FALSE;
}

/**
 * gda_data_model_iter_is_valid:
 * @iter: a #GdaDataModelIter object
 *
 * Tells if @iter is a valid iterator (if it actually corresponds to a valid row in the model)
 *
 * Returns: TRUE if @iter is valid
 */
gboolean
gda_data_model_iter_is_valid (GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);

  GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	g_return_val_if_fail (priv, FALSE);

	return priv->row >= 0 ? TRUE : FALSE;
}

/**
 * gda_data_model_iter_get_holder_for_field:
 * @iter: a #GdaDataModelIter object
 * @col: the requested column
 *
 * Fetch a pointer to the #GdaHolder object which is synchronized with data at 
 * column @col
 *
 * Returns: (transfer none): the #GdaHolder, or %NULL if an error occurred
 */
GdaHolder *
gda_data_model_iter_get_holder_for_field (GdaDataModelIter *iter, gint col)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), NULL);

  GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	g_return_val_if_fail (priv, NULL);

	return gda_set_get_nth_holder ((GdaSet *) iter, col);
}

/**
 * gda_data_model_iter_get_value_at:
 * @iter: a #GdaDataModelIter object
 * @col: the requested column
 *
 * Get the value stored at the column @col in @iter. The returned value must not be modified.
 *
 * Returns: (nullable) (transfer none): the #GValue, or %NULL if the value could not be fetched
 */
const GValue *
gda_data_model_iter_get_value_at (GdaDataModelIter *iter, gint col)
{
	GdaHolder *param;
  GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), NULL);
	g_return_val_if_fail (priv, NULL);

	param = (GdaHolder *) g_slist_nth_data (gda_set_get_holders ((GdaSet *) iter), col);
	if (param) {
		if (gda_holder_is_valid (param))
			return gda_holder_get_value (param);
		else
			return NULL;
	}
	else
		return NULL;
}

/**
 * gda_data_model_iter_get_value_at_e:
 * @iter: a #GdaDataModelIter object
 * @col: the requested column
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Get the value stored at the column @col in @iter. The returned value must not be modified.
 *
 * Returns: (nullable) (transfer none): the #GValue, or %NULL if the value could not be fetched
 *
 * Since: 4.2.10
 */
const GValue *
gda_data_model_iter_get_value_at_e (GdaDataModelIter *iter, gint col, GError **error)
{
	GdaHolder *param;
  GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), NULL);
	g_return_val_if_fail (priv, NULL);

	param = (GdaHolder *) g_slist_nth_data (gda_set_get_holders ((GdaSet *) iter), col);
	if (param) {
		if (gda_holder_is_valid_e (param, error))
			return gda_holder_get_value (param);
		else
			return NULL;
	}
	else
		return NULL;
}

/**
 * gda_data_model_iter_set_value_at: (virtual set_value_at)
 * @iter: a #GdaDataModelIter object
 * @col: the column number
 * @value: a #GValue (not %NULL)
 * @error: a place to store errors, or %NULL
 * 
 * Sets a value in @iter, at the column specified by @col
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_iter_set_value_at (GdaDataModelIter *iter, gint col, const GValue *value, GError **error)
{
	GdaHolder *holder;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	g_return_val_if_fail (value, FALSE);

	holder = gda_data_model_iter_get_holder_for_field (iter, col);
	if (!holder) {
		g_set_error (error, GDA_DATA_MODEL_ITER_ERROR, GDA_DATA_MODEL_ITER_COLUMN_OUT_OF_RANGE_ERROR,
			     _("Column %d out of range (0-%d)"), col, 
			     g_slist_length (gda_set_get_holders ((GdaSet *) iter)) - 1);
		return FALSE;
	}
	return gda_holder_set_value (holder, value, error);
}

/**
 * gda_data_model_iter_get_value_for_field:
 * @iter: a #GdaDataModelIter object
 * @field_name: the requested column name
 *
 * Get the value stored at the column @field_name in @iter
 *
 * Returns: (nullable) (transfer none): the #GValue, or %NULL
 */
const GValue *
gda_data_model_iter_get_value_for_field (GdaDataModelIter *iter, const gchar *field_name)
{
	GdaHolder *param;
  GdaDataModelIterPrivate *priv = gda_data_model_iter_get_instance_private (iter);

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), NULL);
	g_return_val_if_fail (priv, NULL);

	param = gda_set_get_holder ((GdaSet *) iter, field_name);
	if (param) {
		if (gda_holder_is_valid (param))
			return gda_holder_get_value (param);
		else
			return NULL;
	}
	else
		return NULL;
}
