/* gda-data-model-iter.c
 *
 * Copyright (C) 2005 - 2007 Vivien Malerba
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "gda-data-model-iter.h"
#include "gda-data-model.h"
#include "gda-data-model-private.h"
#include "gda-parameter.h"
#include "gda-marshal.h"
#include "gda-data-proxy.h"
#include "gda-enums.h"

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

/* When the GdaDataModel or a parameter is destroyed */
static void destroyed_object_cb (GdaObject *obj, GdaDataModelIter *iter);
static void destroyed_param_cb (GdaObject *obj, GdaDataModelIter *iter);

/* follow model changes */
static void model_row_updated_cb (GdaDataModel *model, gint row, GdaDataModelIter *iter);
static void model_row_removed_cb (GdaDataModel *model, gint row, GdaDataModelIter *iter);

static void param_changed_cb (GdaParameterList *paramlist, GdaParameter *param);
static void param_attr_changed_cb (GdaParameterList *paramlist, GdaParameter *param);

#ifdef GDA_DEBUG
static void gda_data_model_iter_dump (GdaDataModelIter *iter, guint offset);
#endif

/* get a pointer to the parents to be able to cvalue their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	ROW_TO_CHANGE,
        ROW_CHANGED,
	END_OF_DATA,
        LAST_SIGNAL
};

static gint gda_data_model_iter_signals[LAST_SIGNAL] = { 0, 0, 0 };

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
	GdaDataModel          *data_model;
	gulong                 model_changes_signals[2];
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
		static const GTypeInfo info = {
			sizeof (GdaDataModelIterClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_iter_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelIter),
			0,
			(GInstanceInitFunc) gda_data_model_iter_init
		};

		
		type = g_type_register_static (GDA_TYPE_PARAMETER_LIST, "GdaDataModelIter", &info, 0);
	}
	return type;
}

static gboolean
row_to_change_accumulator (GSignalInvocationHint *ihint,
			   GValue *return_accu,
			   const GValue *handler_return,
			   gpointer data)
{
	gboolean thisvalue;

	thisvalue = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accu, thisvalue);

	return thisvalue; /* stop signal if 'thisvalue' is FALSE */
}

static gboolean
m_row_to_change (GdaDataModelIter *iter, gint row)
{
	return TRUE; /* defaults allows row to change */
}

static void
gda_data_model_iter_class_init (GdaDataModelIterClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GdaParameterListClass *paramlist_class = GDA_PARAMETER_LIST_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_data_model_iter_signals [ROW_TO_CHANGE] =
                g_signal_new ("row_to_change",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelIterClass, row_to_change),
                              row_to_change_accumulator, NULL,
                              gda_marshal_BOOLEAN__INT, G_TYPE_BOOLEAN, 1, G_TYPE_INT);
	gda_data_model_iter_signals [ROW_CHANGED] =
                g_signal_new ("row_changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataModelIterClass, row_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
	gda_data_model_iter_signals [END_OF_DATA] =
                g_signal_new ("end_of_data",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataModelIterClass, end_of_data),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	class->row_to_change = m_row_to_change;
	class->row_changed = NULL;
	class->end_of_data = NULL;

	object_class->dispose = gda_data_model_iter_dispose;
	object_class->finalize = gda_data_model_iter_finalize;
	paramlist_class->param_changed = param_changed_cb;
	paramlist_class->param_attr_changed = param_attr_changed_cb;

	/* Properties */
	object_class->set_property = gda_data_model_iter_set_property;
	object_class->get_property = gda_data_model_iter_get_property;
	g_object_class_install_property (object_class, PROP_DATA_MODEL,
					 g_param_spec_object ("data_model", "Data model for which the iter is for", NULL,
                                                               GDA_TYPE_DATA_MODEL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE |
								G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_FORCED_MODEL,
					 g_param_spec_object ("forced_model", NULL, NULL, 
                                                               GDA_TYPE_DATA_MODEL,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_CURRENT_ROW,
					 g_param_spec_int ("current_row", "Current represented row in the data model", 
							   NULL, -1, G_MAXINT, -1,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_UPDATE_MODEL,
					 g_param_spec_boolean ("update_model", "Tells if parameters changes are forwarded "
							       "to the GdaDataModel", NULL, TRUE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_data_model_iter_dump;
#endif
}

static void
gda_data_model_iter_init (GdaDataModelIter *iter)
{
	iter->priv = g_new0 (GdaDataModelIterPrivate, 1);
	iter->priv->data_model = NULL;
	iter->priv->row = -1;
	iter->priv->model_changes_signals[0] = 0;
	iter->priv->model_changes_signals[1] = 0;
	iter->priv->keep_param_changes = FALSE;
}

/**
 * gda_data_model_iter_new
 * @model: Model used to create the #GdaDataModelIter
 *
 * Creates a new GdaDataModelIter object which represents a value or a parameter.
 *
 * Returns: the new object
 */
GdaDataModelIter *
gda_data_model_iter_new (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	/* use the data model's own creation method here */
	return gda_data_model_create_iter (model);
}

static void 
model_row_updated_cb (GdaDataModel *model, gint row, GdaDataModelIter *iter)
{
	g_assert (model == iter->priv->data_model);
		
	/* sync parameters with the new values in row */
	if (iter->priv->row == row) {
		iter->priv->keep_param_changes = TRUE;
		gda_data_model_move_iter_at_row (iter->priv->data_model, iter, row);
		iter->priv->keep_param_changes = FALSE;
	}
}

static void 
model_row_removed_cb (GdaDataModel *model, gint row, GdaDataModelIter *iter)
{
	if (iter->priv->row < 0)
		/* we are not concerned by handling this signal */
		return;

	/* if removed row is the one corresponding to iter, 
	 * then make all the parameters invalid */
	if (iter->priv->row == row) {
		gda_data_model_iter_invalidate_contents (iter);
		gda_data_model_iter_set_at_row (iter, -1);
	}
	else {
		/* shift iter's row by one to keep good numbers */
		if (iter->priv->row > row) 
			iter->priv->row--;
	}
}

/*
 * This function is called when a parameter in @paramlist is changed
 * to make sure the change is propagated to the GdaDataModel
 * paramlist is an iter for
 */
static void
param_changed_cb (GdaParameterList *paramlist, GdaParameter *param)
{
	GdaDataModelIter *iter;
	gint col;

	iter = GDA_DATA_MODEL_ITER (paramlist);
	if (!iter->priv->keep_param_changes && (iter->priv->row >= 0)) {
		g_signal_handler_block (iter->priv->data_model, iter->priv->model_changes_signals [0]);
		g_signal_handler_block (iter->priv->data_model, iter->priv->model_changes_signals [1]);
		
		/* propagate the value update to the data model */
		col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (param), "model_col")) - 1;
		g_return_if_fail (col >= 0);
		
		if (! gda_data_model_set_value_at (GDA_DATA_MODEL (iter->priv->data_model), 
						   col, iter->priv->row, gda_parameter_get_value (param), NULL)) {
			/* writing to the model failed, revert back the change to parameter */
			iter->priv->keep_param_changes = TRUE;
			gda_parameter_set_value (param, gda_data_model_get_value_at (GDA_DATA_MODEL (iter->priv->data_model), 
										     col, iter->priv->row)); 
			iter->priv->keep_param_changes = FALSE;
		}
		
		g_signal_handler_unblock (iter->priv->data_model, iter->priv->model_changes_signals [0]);
		g_signal_handler_unblock (iter->priv->data_model, iter->priv->model_changes_signals [1]);
	}

	/* for the parent class */
	if (((GdaParameterListClass *) parent_class)->param_changed)
		((GdaParameterListClass *) parent_class)->param_changed (paramlist, param);
}

/*
 * This function is called when a parameter in @paramlist has its attributes changed
 * to make sure the change is propagated to the GdaDataModel
 * paramlist is an iter for
 */
static void
param_attr_changed_cb (GdaParameterList *paramlist, GdaParameter *param)
{
	GdaDataModelIter *iter;
	gint col;
	gboolean toset;

	iter = GDA_DATA_MODEL_ITER (paramlist);
	if (!GDA_IS_DATA_PROXY (iter->priv->data_model))
		return;

	if (!iter->priv->keep_param_changes && (iter->priv->row >= 0)) {
		g_signal_handler_block (iter->priv->data_model, iter->priv->model_changes_signals [0]);
		g_signal_handler_block (iter->priv->data_model, iter->priv->model_changes_signals [1]);
		
		/* propagate the value update to the data model */
		col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (param), "model_col")) - 1;
		g_return_if_fail (col >= 0);
		
		g_object_get (G_OBJECT (param), "use-default-value", &toset, NULL);
		if (toset && gda_parameter_get_exists_default_value (param))
			gda_data_proxy_alter_value_attributes (GDA_DATA_PROXY (iter->priv->data_model), 
							       iter->priv->row, col, 
							       GDA_VALUE_ATTR_CAN_BE_DEFAULT | GDA_VALUE_ATTR_IS_DEFAULT);
		
		g_signal_handler_unblock (iter->priv->data_model, iter->priv->model_changes_signals [0]);
		g_signal_handler_unblock (iter->priv->data_model, iter->priv->model_changes_signals [1]);
	}

	/* for the parent class */
	if (((GdaParameterListClass *) parent_class)->param_attr_changed)
		((GdaParameterListClass *) parent_class)->param_attr_changed (paramlist, param);
}

static void 
destroyed_object_cb (GdaObject *obj, GdaDataModelIter *iter)
{
	g_assert (obj == (GdaObject*) iter->priv->data_model);
	g_signal_handler_disconnect (G_OBJECT (obj),
				     iter->priv->model_changes_signals [0]);
	g_signal_handler_disconnect (G_OBJECT (obj),
				     iter->priv->model_changes_signals [1]);
	g_signal_handlers_disconnect_by_func (G_OBJECT (obj),
					      G_CALLBACK (destroyed_object_cb), iter);
	iter->priv->data_model = NULL;
}

static void
destroyed_param_cb (GdaObject *obj, GdaDataModelIter *iter)
{
	g_signal_handlers_disconnect_by_func (obj,
					      G_CALLBACK (destroyed_param_cb), iter);
	g_signal_handlers_disconnect_by_func (obj,
					      G_CALLBACK (param_changed_cb), iter);
}

static void
gda_data_model_iter_dispose (GObject *object)
{
	GdaDataModelIter *iter;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (object));

	iter = GDA_DATA_MODEL_ITER (object);
	if (iter->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));
		if (iter->priv->data_model) 
			destroyed_object_cb ((GdaObject *) iter->priv->data_model, iter);
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
			gint col, ncols;
			GdaParameter *param;
			GdaDict *dict;
			GdaColumn *column;

			GObject* ptr = g_value_get_object (value);
			g_return_if_fail (ptr && GDA_IS_DATA_MODEL (ptr));
			model = GDA_DATA_MODEL (ptr);
			
			/* REM: model is actually set using the next property */

			/* compute parameters */
			dict = gda_object_get_dict (GDA_OBJECT (iter));
			ncols = gda_data_model_get_n_columns (model);
			for (col = 0; col < ncols; col++) {
				const gchar *str;
				column = gda_data_model_describe_column (model, col);
				param = (GdaParameter *) g_object_new (GDA_TYPE_PARAMETER, "dict", dict,
								       "g_type", 
								       gda_column_get_g_type (column), NULL);

				gda_parameter_set_not_null (param, !gda_column_get_allow_null (column));
				str = gda_column_get_title (column);
				if (!str)
					str = gda_column_get_name (column);
				if (str)
					gda_object_set_name (GDA_OBJECT (param), str);
				if (gda_column_get_default_value (column) || 
				    gda_column_get_auto_increment (column))
					gda_parameter_set_exists_default_value (param, TRUE);
				gda_parameter_list_add_param ((GdaParameterList *) iter, param);
				g_object_set_data (G_OBJECT (param), "model_col", GINT_TO_POINTER (col + 1));
				gda_object_connect_destroy (param,
							    G_CALLBACK (destroyed_param_cb), iter);
				g_object_unref (param);
			}
		}
		case PROP_FORCED_MODEL: {
			GdaDataModel* ptr = g_value_get_object (value);
			g_return_if_fail (ptr && GDA_IS_DATA_MODEL (ptr));

			if (iter->priv->data_model) {
				if (iter->priv->data_model == GDA_DATA_MODEL (ptr))
					return;

				destroyed_object_cb ((GdaObject *) iter->priv->data_model, iter);
			}

			iter->priv->data_model = GDA_DATA_MODEL (ptr);
			gda_object_connect_destroy (ptr,
						    G_CALLBACK (destroyed_object_cb), iter);
			iter->priv->model_changes_signals [0] = g_signal_connect (G_OBJECT (ptr), "row_updated",
										  G_CALLBACK (model_row_updated_cb), iter);
			iter->priv->model_changes_signals [1] = g_signal_connect (G_OBJECT (ptr), "row_removed",
										  G_CALLBACK (model_row_removed_cb), iter);
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
		case PROP_FORCED_MODEL:
			g_value_set_object (value, G_OBJECT (iter->priv->data_model));
			break;
		case PROP_CURRENT_ROW:
			g_value_set_int (value, iter->priv->row);
			break;
		case PROP_UPDATE_MODEL:
			g_value_set_boolean (value, ! iter->priv->keep_param_changes);
			break;
		}	
	}
}

/**
 * gda_data_model_iter_can_be_moved
 * @iter: a #GdaDataModelIter object
 *
 * Tells if @iter can point to another row. Note the @iter by itself will not refuse
 * a row change, but that the row change may be refused by another object using
 * @iter.
 *
 * Returns: TRUE if the row represented by @iter can be changed
 */
gboolean
gda_data_model_iter_can_be_moved (GdaDataModelIter *iter)
{
	gboolean move_ok = TRUE;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	g_return_val_if_fail (iter->priv, FALSE);

	if (!gda_data_model_iter_is_valid (iter))
		return TRUE;

	/* optionaly validate the row change */
	g_signal_emit (G_OBJECT (iter),
		       gda_data_model_iter_signals[ROW_TO_CHANGE],
		       0, iter->priv->row, &move_ok);

	return move_ok;
}

/**
 * gda_data_model_iter_set_at_row
 * @iter: a #GdaDataModelIter object
 * @row: the row to set @iter to
 *
 * Synchronizes the values of the parameters in @iter with the values at the @row row
 *
 * If @row < 0 then @iter is not bound to any row of the data model it iters through.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_iter_set_at_row (GdaDataModelIter *iter, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	g_return_val_if_fail (iter->priv, FALSE);

	if (row < 0) {
		if (iter->priv->row != -1) {
			iter->priv->row = -1;
			g_signal_emit (G_OBJECT (iter),
				       gda_data_model_iter_signals[ROW_CHANGED],
				       0, iter->priv->row);
		}
		return TRUE;
	}
	else
		return gda_data_model_move_iter_at_row (iter->priv->data_model, iter, row);
}

/**
 * gda_data_model_iter_move_next
 * @iter: a #GdaDataModelIter object
 *
 * Moves @iter one row further than where it already is (synchronizes the values of the parameters in @iter 
 * with the values at the new row).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_iter_move_next (GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	g_return_val_if_fail (iter->priv, FALSE);

	return gda_data_model_move_iter_next (iter->priv->data_model, iter);
}

/**
 * gda_data_model_iter_move_prev
 * @iter: a #GdaDataModelIter object
 *
 * Moves @iter one row before where it already is (synchronizes the values of the parameters in @iter 
 * with the values at the new row).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_iter_move_prev (GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	g_return_val_if_fail (iter->priv, FALSE);

	return gda_data_model_move_iter_prev (iter->priv->data_model, iter);
}

/**
 * gda_data_model_iter_get_row
 * @iter: a #GdaDataModelIter object
 *
 * Get the row which @iter represents in the data model
 *
 * Returns: the row number, or -1 if not available
 */
gint
gda_data_model_iter_get_row (GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), -1);
	g_return_val_if_fail (iter->priv, -1);

	return iter->priv->row;
}

/**
 * gda_data_model_iter_invalidate_contents
 * @iter: a #GdaDataModelIter object
 *
 * Declare all the parameters in @iter invalid, without modifying the
 * #GdaDataModel @iter is for or changing the row it represents
 */
void
gda_data_model_iter_invalidate_contents (GdaDataModelIter *iter)
{
	GSList *list;
	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (iter));
	g_return_if_fail (iter->priv);

	iter->priv->keep_param_changes = TRUE;
	list = GDA_PARAMETER_LIST (iter)->parameters;
	while (list) {
		gda_parameter_declare_invalid (GDA_PARAMETER (list->data));
		list = g_slist_next (list);
	}
	iter->priv->keep_param_changes = FALSE;
}

/**
 * gda_data_model_iter_is_valid
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
 * gda_data_model_iter_get_column_for_param
 * @iter: a #GdaDataModelIter object
 * @param: a #GdaParameter object, listed in @iter
 *
 * Get the column number in the #GdaDataModel for which @iter is an iterator as
 * represented by the @param parameter
 *
 * Returns: the column number, or @param is not valid
 */
gint
gda_data_model_iter_get_column_for_param (GdaDataModelIter *iter, GdaParameter *param)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), -1);
	g_return_val_if_fail (iter->priv, -1);
	g_return_val_if_fail (GDA_IS_PARAMETER (param), -1);
	g_return_val_if_fail (g_slist_find (((GdaParameterList *) iter)->parameters, param), -1);

	return g_slist_index (((GdaParameterList *) iter)->parameters, param);
}

/**
 * gda_data_model_iter_get_param_for_column
 * @iter: a #GdaDataModelIter object
 * @col: the requested column
 *
 * Fetch a pointer to the #GdaParameter object which is synchronized with data at 
 * column @col
 *
 * Returns: the #GdaParameter, or %NULL if an error occurred
 */
GdaParameter *
gda_data_model_iter_get_param_for_column (GdaDataModelIter *iter, gint col)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), NULL);
	g_return_val_if_fail (iter->priv, NULL);

	return g_slist_nth_data (((GdaParameterList *) iter)->parameters, col);
}

/**
 * gda_data_model_iter_get_value_at
 * @iter: a #GdaDataModelIter object
 * @col: the requested column
 *
 * Get the value stored at the column @col in @iter
 *
 * Returns: the #GValue, or %NULL
 */
const GValue *
gda_data_model_iter_get_value_at (GdaDataModelIter *iter, gint col)
{
	GdaParameter *param;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), NULL);
	g_return_val_if_fail (iter->priv, NULL);

	param = (GdaParameter *) g_slist_nth_data (((GdaParameterList *) iter)->parameters, col);
	if (param)
		return gda_parameter_get_value (param);
	else
		return NULL;
}

/**
 * gda_data_model_iter_get_value_for_field
 * @iter: a #GdaDataModelIter object
 * @field_name: the requested column name
 *
 * Get the value stored at the column @field_name in @iter
 *
 * Returns: the #GValue, or %NULL
 */
const GValue *
gda_data_model_iter_get_value_for_field (GdaDataModelIter *iter, const gchar *field_name)
{
	GdaParameter *param;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), NULL);
	g_return_val_if_fail (iter->priv, NULL);

	param = gda_parameter_list_find_param ((GdaParameterList *) iter, field_name);
	if (param)
		return gda_parameter_get_value (param);
	else
		return NULL;
}


#ifdef GDA_DEBUG
static void
gda_data_model_iter_dump (GdaDataModelIter *iter, guint offset)
{
	gchar *str;
	GdaDict *dict;

	g_return_if_fail (GDA_IS_DATA_MODEL_ITER (iter));
	dict = gda_object_get_dict (GDA_OBJECT (iter));
       
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
	memset (str, ' ', offset);

        /* dump */
        if (iter->priv) {
		GdaObjectClass *plist_class;
		plist_class = g_type_class_peek (GDA_TYPE_PARAMETER_LIST);
		(plist_class->dump) ((GdaObject *) iter, offset);
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, iter);
	g_free (str);
}
#endif
