/*
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
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

extern GdaAttributesManager *_gda_column_attributes_manager;
extern GdaAttributesManager *gda_holder_attributes_manager;

/* 
 * Main static functions 
 */
static void gda_data_model_iter_class_init (GdaDataModelIterClass * class);
static void gda_data_model_iter_init (GdaDataModelIter *qf);
static void gda_data_model_iter_dispose (GObject *object);
static void gda_data_model_iter_finalize (GObject *object);

static void gda_data_model_iter_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_data_model_iter_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

/* follow model changes */
static void model_row_updated_cb (GdaDataModel *model, gint row, GdaDataModelIter *iter);
static void model_row_removed_cb (GdaDataModel *model, gint row, GdaDataModelIter *iter);
static void model_reset_cb (GdaDataModel *model, GdaDataModelIter *iter);

static GError *validate_holder_change_cb (GdaSet *paramlist, GdaHolder *param, const GValue *new_value);
static void holder_attr_changed_cb (GdaSet *paramlist, GdaHolder *param, const gchar *att_name, const GValue *att_value);

/* get a pointer to the parents to be able to cvalue their destructor */
static GObjectClass  *parent_class = NULL;

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
	PROP_FORCED_MODEL,
	PROP_UPDATE_MODEL
};

/* private structure */
struct _GdaDataModelIterPrivate
{
	GdaDataModel          *data_model; /* may be %NULL because there is only a weak ref on it */
	gulong                 model_changes_signals[3];
	gboolean               keep_param_changes;
	gint                   row; /* -1 if row is unknown */
};


/* module error */
GQuark gda_data_model_iter_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_data_model_iter_error");
	return quark;
}

GType
gda_data_model_iter_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaDataModelIterClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_iter_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelIter),
			0,
			(GInstanceInitFunc) gda_data_model_iter_init,
			0
		};

		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_SET, "GdaDataModelIter", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_data_model_iter_class_init (GdaDataModelIterClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GdaSetClass *paramlist_class = GDA_SET_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

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

	class->row_changed = NULL;
	class->end_of_data = NULL;

	object_class->dispose = gda_data_model_iter_dispose;
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
	g_object_class_install_property (object_class, PROP_FORCED_MODEL,
					 g_param_spec_object ("forced-model", NULL, "Overrides the data model the iter "
							      "is attached to (reserved for internal usage)", 
                                                               GDA_TYPE_DATA_MODEL,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
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
	iter->priv = g_new0 (GdaDataModelIterPrivate, 1);
	iter->priv->data_model = NULL;
	iter->priv->row = -1;
	iter->priv->model_changes_signals[0] = 0;
	iter->priv->model_changes_signals[1] = 0;
	iter->priv->model_changes_signals[2] = 0;
	iter->priv->keep_param_changes = FALSE;
}

static void 
model_row_updated_cb (GdaDataModel *model, gint row, GdaDataModelIter *iter)
{
	g_assert (model == iter->priv->data_model);
		
	/* sync parameters with the new values in row */
	if (iter->priv->row == row) {
		iter->priv->keep_param_changes = TRUE;
		gda_data_model_iter_move_to_row (iter, row);
		iter->priv->keep_param_changes = FALSE;
	}
}

static void 
model_row_removed_cb (G_GNUC_UNUSED GdaDataModel *model, gint row, GdaDataModelIter *iter)
{
	if (iter->priv->row < 0)
		/* we are not concerned by handling this signal */
		return;

	/* if removed row is the one corresponding to iter, 
	 * then make all the parameters invalid */
	if (iter->priv->row == row) {
		gda_data_model_iter_invalidate_contents (iter);
		gda_data_model_iter_move_to_row (iter, -1);
	}
	else {
		/* shift iter's row by one to keep good numbers */
		if (iter->priv->row > row) 
			iter->priv->row--;
	}
}

static void
define_holder_for_data_model_column (GdaDataModel *model, gint col, GdaDataModelIter *iter)
{
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
	/* copy extra attributes */
	gda_attributes_manager_copy (_gda_column_attributes_manager, (gpointer) column,
				     gda_holder_attributes_manager, (gpointer) param);
	gda_set_add_holder ((GdaSet *) iter, param);
	g_object_set_data (G_OBJECT (param), "model_col", GINT_TO_POINTER (col + 1));
	g_object_unref (param);
}

static void
model_reset_cb (GdaDataModel *model, GdaDataModelIter *iter)
{
	gint row;

	row = gda_data_model_iter_get_row (iter);
	
	/* adjust GdaHolder's type if a column's type has changed from GDA_TYPE_NULL
	 * to something else */
	gint i, nbcols;
	GSList *list;
	nbcols = gda_data_model_get_n_columns (model);
	if (GDA_IS_DATA_PROXY (model))
		nbcols = nbcols / 2;

	for (i = 0, list = ((GdaSet*) iter)->holders;
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
	GdaDataModelIter *iter;
	gint col;
	GError *error = NULL;
	GValue *nvalue;

	if (new_value)
		nvalue = (GValue*) new_value;
	else
		nvalue = gda_value_new_null();

	iter = (GdaDataModelIter *) paramlist;
	if (!iter->priv->keep_param_changes && (iter->priv->row >= 0)) {
		g_signal_handler_block (iter->priv->data_model, iter->priv->model_changes_signals [0]);
		g_signal_handler_block (iter->priv->data_model, iter->priv->model_changes_signals [1]);
		
		/* propagate the value update to the data model */
		col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (param), "model_col")) - 1;
		if (col < 0) 
			g_set_error (&error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
				     _("Column %d out of range (0-%d)"), col, g_slist_length (paramlist->holders) - 1);
		else if (GDA_DATA_MODEL_GET_CLASS ((GdaDataModel *) iter->priv->data_model)->i_iter_set_value) {
			if (! (GDA_DATA_MODEL_GET_CLASS ((GdaDataModel *) iter->priv->data_model)->i_iter_set_value) 
			    ((GdaDataModel *) iter->priv->data_model, iter, col, nvalue, &error)) {
				if (!error)
					g_set_error (&error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
						      "%s", _("GdaDataModel refused value change"));
			}
		}
		else if (! gda_data_model_set_value_at ((GdaDataModel *) iter->priv->data_model, 
							col, iter->priv->row, nvalue, &error)) {
			if (!error)
				g_set_error (&error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
					      "%s", _("GdaDataModel refused value change"));
		}
		
		g_signal_handler_unblock (iter->priv->data_model, iter->priv->model_changes_signals [0]);
		g_signal_handler_unblock (iter->priv->data_model, iter->priv->model_changes_signals [1]);
	}
	if (!new_value)
		gda_value_free (nvalue);

	if (!error && ((GdaSetClass *) parent_class)->validate_holder_change)
		/* for the parent class */
		return ((GdaSetClass *) parent_class)->validate_holder_change (paramlist, param, new_value);

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
	GdaDataModelIter *iter;
	gint col;
	gboolean toset = FALSE;

	iter = GDA_DATA_MODEL_ITER (paramlist);
	if (!GDA_IS_DATA_PROXY (iter->priv->data_model))
		return;

	if (!strcmp (att_name, GDA_ATTRIBUTE_IS_DEFAULT) &&
	    !iter->priv->keep_param_changes && (iter->priv->row >= 0)) {
		g_signal_handler_block (iter->priv->data_model, iter->priv->model_changes_signals [0]);
		g_signal_handler_block (iter->priv->data_model, iter->priv->model_changes_signals [1]);
		
		/* propagate the value update to the data model */
		col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (param), "model_col")) - 1;
		g_return_if_fail (col >= 0);
		
		if (att_value && g_value_get_boolean (att_value))
			toset = TRUE;
		if (toset && gda_holder_get_default_value (param))
			gda_data_proxy_alter_value_attributes (GDA_DATA_PROXY (iter->priv->data_model), 
							       iter->priv->row, col, 
							       GDA_VALUE_ATTR_CAN_BE_DEFAULT | GDA_VALUE_ATTR_IS_DEFAULT);
		
		g_signal_handler_unblock (iter->priv->data_model, iter->priv->model_changes_signals [0]);
		g_signal_handler_unblock (iter->priv->data_model, iter->priv->model_changes_signals [1]);
	}

	/* for the parent class */
	if (((GdaSetClass *) parent_class)->holder_attr_changed)
		((GdaSetClass *) parent_class)->holder_attr_changed (paramlist, param, att_name, att_value);
}

static void
gda_data_model_iter_dispose (GObject *object)
{
	GdaDataModelIter *iter;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (object));

	iter = GDA_DATA_MODEL_ITER (object);
	if (iter->priv) {
		if (iter->priv->data_model) { 
			g_signal_handler_disconnect (iter->priv->data_model, iter->priv->model_changes_signals [0]);
			g_signal_handler_disconnect (iter->priv->data_model, iter->priv->model_changes_signals [1]);
			g_signal_handler_disconnect (iter->priv->data_model, iter->priv->model_changes_signals [2]);
			g_object_remove_weak_pointer (G_OBJECT (iter->priv->data_model), 
						      (gpointer*) &(iter->priv->data_model));
			iter->priv->data_model = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_data_model_iter_finalize (GObject   * object)
{
	GdaDataModelIter *iter;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (object));

	iter = GDA_DATA_MODEL_ITER (object);
	if (iter->priv) {
		g_free (iter->priv);
		iter->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void 
gda_data_model_iter_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	GdaDataModelIter *iter;

	iter = GDA_DATA_MODEL_ITER (object);
	if (iter->priv) {
		switch (param_id) {
		case PROP_DATA_MODEL: {
			GdaDataModel *model;

			GObject* ptr = g_value_get_object (value);
			g_return_if_fail (ptr && GDA_IS_DATA_MODEL (ptr));
			model = GDA_DATA_MODEL (ptr);
			if (iter->priv->data_model) {
				if (iter->priv->data_model == model)
					return;
				g_signal_handler_disconnect (iter->priv->data_model,
							     iter->priv->model_changes_signals [0]);
				g_signal_handler_disconnect (iter->priv->data_model,
							     iter->priv->model_changes_signals [1]);
				g_signal_handler_disconnect (iter->priv->data_model,
							     iter->priv->model_changes_signals [2]);
				g_object_remove_weak_pointer (G_OBJECT (iter->priv->data_model), 
							      (gpointer*) &(iter->priv->data_model)); 	
			}

			iter->priv->data_model = model;
			g_object_add_weak_pointer (G_OBJECT (iter->priv->data_model),
						   (gpointer*) &(iter->priv->data_model));

			iter->priv->model_changes_signals [0] = g_signal_connect (G_OBJECT (model), "row-updated",
										  G_CALLBACK (model_row_updated_cb),
										  iter);
			iter->priv->model_changes_signals [1] = g_signal_connect (G_OBJECT (model), "row-removed",
										  G_CALLBACK (model_row_removed_cb),
										  iter);
			iter->priv->model_changes_signals [2] = g_signal_connect (G_OBJECT (model), "reset",
										  G_CALLBACK (model_reset_cb), iter);
			model_reset_cb (GDA_DATA_MODEL (ptr), iter);
			break;
		}
		case PROP_FORCED_MODEL: {
			g_warning ("Deprecated property, not to be used");
			break;
                }
		case PROP_CURRENT_ROW:
			if (iter->priv->row != g_value_get_int (value)) {
                                iter->priv->row = g_value_get_int (value);
                                g_signal_emit (G_OBJECT (iter),
                                               gda_data_model_iter_signals[ROW_CHANGED],
                                               0, iter->priv->row);
                        }
			break;
		case PROP_UPDATE_MODEL:
			iter->priv->keep_param_changes = ! g_value_get_boolean (value);
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
	
	if (iter->priv) {
		switch (param_id) {
		case PROP_DATA_MODEL:
			g_value_set_object (value, G_OBJECT (iter->priv->data_model));
			break;
		case PROP_FORCED_MODEL:
			g_warning ("Deprecated property, not to be used");
			g_value_set_object (value, G_OBJECT (iter->priv->data_model));
			break;
		case PROP_CURRENT_ROW:
			g_value_set_int (value, iter->priv->row);
			break;
		case PROP_UPDATE_MODEL:
			g_value_set_boolean (value, ! iter->priv->keep_param_changes);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * gda_data_model_iter_move_to_row:
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
	g_return_val_if_fail (iter->priv, FALSE);

	if ((gda_data_model_iter_get_row (iter) >= 0) &&
	    (gda_data_model_iter_get_row (iter) == row)) {
		/* refresh @iter's contents */
		GdaDataModel *model;
		model = iter->priv->data_model;
		g_return_val_if_fail (model, FALSE);

		if (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_at_row)
			return (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_at_row) (model, iter,
										  row);
		else
			return gda_data_model_iter_move_to_row_default (model, iter, row);
	}

	if (row < 0) {
		if (gda_data_model_iter_get_row (iter) >= 0) {
			if (! _gda_set_validate ((GdaSet*) iter, NULL))
				return FALSE;

			iter->priv->row = -1;
			g_signal_emit (G_OBJECT (iter),
				       gda_data_model_iter_signals[ROW_CHANGED],
				       0, iter->priv->row);
		}
		return TRUE;
	}
	else {
		GdaDataModel *model;
		model = iter->priv->data_model;
		g_return_val_if_fail (model, FALSE);

		if (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_at_row) {
			if ((gda_data_model_iter_get_row (iter) >= 0) &&
			    ! _gda_set_validate ((GdaSet*) iter, NULL))
				return FALSE;
			return (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_at_row) (model, iter,
										  row);
		}
		else
			return gda_data_model_iter_move_to_row_default (model, iter, row);
	}
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
 * gda_data_model_iter_move_to_row_default: (skip)
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
	for (col = 0, list = ((GdaSet *) iter)->holders; list; col++, list = list->next) {
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
 * gda_data_model_iter_move_next:
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
	GdaDataModel *model;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	g_return_val_if_fail (iter->priv, FALSE);
	g_return_val_if_fail (iter->priv->data_model, FALSE);

	model = iter->priv->data_model;
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_next) {
		if ((gda_data_model_iter_get_row (iter) >= 0) &&
		    ! _gda_set_validate ((GdaSet*) iter, NULL))
			return FALSE;
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_next) (model, iter);
	}
	else 
		/* default method */
		return gda_data_model_iter_move_next_default (model, iter);
}

/**
 * gda_data_model_iter_move_next_default: (skip)
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
	for (col = 0, list = ((GdaSet *) iter)->holders; list; col++, list = list->next) {
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
 * gda_data_model_iter_move_prev:
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
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	g_return_val_if_fail (iter->priv, FALSE);
	g_return_val_if_fail (iter->priv->data_model, FALSE);

	model = iter->priv->data_model;
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_prev) {
		if ((gda_data_model_iter_get_row (iter) >= 0) &&
		    ! _gda_set_validate ((GdaSet*) iter, NULL))
			return FALSE;
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_prev) (model, iter);
	}
	else 
		/* default method */
		return gda_data_model_iter_move_prev_default (model, iter);
}

/**
 * gda_data_model_iter_move_prev_default: (skip)
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
	for (col = 0, list = ((GdaSet *) iter)->holders; list; col++, list = list->next) {
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
	g_return_val_if_fail (iter->priv, -1);

	return iter->priv->row;
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
	g_return_if_fail (iter->priv);

	iter->priv->keep_param_changes = TRUE;
	for (list = GDA_SET (iter)->holders; list; list = list->next)
		gda_holder_force_invalid (GDA_HOLDER (list->data));
	iter->priv->keep_param_changes = FALSE;
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
	g_return_val_if_fail (iter->priv, FALSE);

	return iter->priv->row >= 0 ? TRUE : FALSE;
}

/**
 * gda_data_model_iter_get_column_for_param:
 * @iter: a #GdaDataModelIter object
 * @param: a #GdaHolder object, listed in @iter
 *
 * Get the column number in the #GdaDataModel for which @iter is an iterator as
 * represented by the @param parameter
 *
 * Returns: the column number, or @param is not valid
 *
 * Deprecated: 5.2: not very useful
 */
gint
gda_data_model_iter_get_column_for_param (GdaDataModelIter *iter, GdaHolder *param)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), -1);
	g_return_val_if_fail (iter->priv, -1);
	g_return_val_if_fail (GDA_IS_HOLDER (param), -1);
	g_return_val_if_fail (g_slist_find (((GdaSet *) iter)->holders, param), -1);

	return g_slist_index (((GdaSet *) iter)->holders, param);
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
	g_return_val_if_fail (iter->priv, NULL);

	return gda_set_get_nth_holder ((GdaSet *) iter, col);
}

/**
 * gda_data_model_iter_get_value_at:
 * @iter: a #GdaDataModelIter object
 * @col: the requested column
 *
 * Get the value stored at the column @col in @iter. The returned value must not be modified.
 *
 * Returns: (transfer none): the #GValue, or %NULL if the value could not be fetched
 */
const GValue *
gda_data_model_iter_get_value_at (GdaDataModelIter *iter, gint col)
{
	GdaHolder *param;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), NULL);
	g_return_val_if_fail (iter->priv, NULL);

	param = (GdaHolder *) g_slist_nth_data (((GdaSet *) iter)->holders, col);
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
 * @error: (allow-none): a place to store errors, or %NULL
 *
 * Get the value stored at the column @col in @iter. The returned value must not be modified.
 *
 * Returns: (transfer none): the #GValue, or %NULL if the value could not be fetched
 *
 * Since: 4.2.10
 */
const GValue *
gda_data_model_iter_get_value_at_e (GdaDataModelIter *iter, gint col, GError **error)
{
	GdaHolder *param;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), NULL);
	g_return_val_if_fail (iter->priv, NULL);

	param = (GdaHolder *) g_slist_nth_data (((GdaSet *) iter)->holders, col);
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
 * gda_data_model_iter_set_value_at:
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
	g_return_val_if_fail (iter->priv, FALSE);
	g_return_val_if_fail (value, FALSE);

	holder = gda_data_model_iter_get_holder_for_field (iter, col);
	if (!holder) {
		g_set_error (error, GDA_DATA_MODEL_ITER_ERROR, GDA_DATA_MODEL_ITER_COLUMN_OUT_OF_RANGE_ERROR,
			     _("Column %d out of range (0-%d)"), col, 
			     g_slist_length (((GdaSet *) iter)->holders) - 1);
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
 * Returns: (transfer none): the #GValue, or %NULL
 */
const GValue *
gda_data_model_iter_get_value_for_field (GdaDataModelIter *iter, const gchar *field_name)
{
	GdaHolder *param;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), NULL);
	g_return_val_if_fail (iter->priv, NULL);

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
