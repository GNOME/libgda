/*
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
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

#include "gda-data-access-wrapper.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-row.h>
#include <libgda/gda-holder.h>


#define ROWS_POOL_SIZE 50
struct _GdaDataAccessWrapperPrivate {
	GdaDataModel     *model;
	GdaDataModelAccessFlags model_access_flags;

	GdaDataModelIter *iter;    /* iterator on @model, NULL if @model already is random access */
	gint              iter_row;/* current row of @iter, starting at 0 when created */

	GHashTable       *rows;    /* NULL if @model already is random access */
	gint              nb_rows; /* number of rows of @wrapper */
	gint              nb_cols; /* number of columns of @wrapper */
	gint              last_row;/* row number of the last row which has been read */
	gboolean          end_of_data; /* TRUE if the end of the data model has been reached by the iterator */

	GArray           *rows_buffer_array; /* Array of GdaRow */
	GArray           *rows_buffer_index; /* Array of indexes: GdaRow at index i in @rows_buffer_array
					      * is indexed in @rows with key rows_buffer_index[i] */

	/* rows mapping */
	GSList           *columns; /* not NULL if a mapping exists */
	gint             *rows_mapping; /* @nb_cols is set when @rows_mapping is set, and @rows_mapping's size is @nb_cols */
};

/* properties */
enum
{
        PROP_0,
	PROP_MODEL,
};

static void gda_data_access_wrapper_class_init (GdaDataAccessWrapperClass *klass);
static void gda_data_access_wrapper_init       (GdaDataAccessWrapper *model,
					      GdaDataAccessWrapperClass *klass);
static void gda_data_access_wrapper_dispose    (GObject *object);
static void gda_data_access_wrapper_finalize   (GObject *object);

static void gda_data_access_wrapper_set_property (GObject *object,
						    guint param_id,
						    const GValue *value,
						    GParamSpec *pspec);
static void gda_data_access_wrapper_get_property (GObject *object,
						    guint param_id,
						    GValue *value,
						    GParamSpec *pspec);

/* GdaDataModel interface */
static void                 gda_data_access_wrapper_data_model_init (GdaDataModelIface *iface);
static gint                 gda_data_access_wrapper_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_access_wrapper_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_access_wrapper_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_access_wrapper_get_access_flags(GdaDataModel *model);
static const GValue        *gda_data_access_wrapper_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);
static GdaValueAttribute    gda_data_access_wrapper_get_attributes_at (GdaDataModel *model, gint col, gint row);
static GError             **gda_data_access_wrapper_get_exceptions  (GdaDataModel *model);

static void iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdaDataAccessWrapper *model);
static void iter_end_of_data_cb (GdaDataModelIter *iter, GdaDataAccessWrapper *model);

static GdaRow *create_new_row (GdaDataAccessWrapper *model);

static GObjectClass *parent_class = NULL;

/**
 * gda_data_access_wrapper_get_type:
 *
 * Returns: the #GType of GdaDataAccessWrapper.
 */
GType
gda_data_access_wrapper_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaDataAccessWrapperClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_access_wrapper_class_init,
			NULL,
			NULL,
			sizeof (GdaDataAccessWrapper),
			0,
			(GInstanceInitFunc) gda_data_access_wrapper_init,
			0
		};

		static const GInterfaceInfo data_model_info = {
			(GInterfaceInitFunc) gda_data_access_wrapper_data_model_init,
			NULL,
			NULL
		};

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaDataAccessWrapper", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_data_access_wrapper_class_init (GdaDataAccessWrapperClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* properties */
	object_class->set_property = gda_data_access_wrapper_set_property;
        object_class->get_property = gda_data_access_wrapper_get_property;
	g_object_class_install_property (object_class, PROP_MODEL,
                                         g_param_spec_object ("model", NULL, "Data model being wrapped",
                                                              GDA_TYPE_DATA_MODEL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	/* virtual functions */
	object_class->dispose = gda_data_access_wrapper_dispose;
	object_class->finalize = gda_data_access_wrapper_finalize;
}

static void
gda_data_access_wrapper_data_model_init (GdaDataModelIface *iface)
{
	iface->i_get_n_rows = gda_data_access_wrapper_get_n_rows;
	iface->i_get_n_columns = gda_data_access_wrapper_get_n_columns;
	iface->i_describe_column = gda_data_access_wrapper_describe_column;
        iface->i_get_access_flags = gda_data_access_wrapper_get_access_flags;
	iface->i_get_value_at = gda_data_access_wrapper_get_value_at;
	iface->i_get_attributes_at = gda_data_access_wrapper_get_attributes_at;

	iface->i_create_iter = NULL;
	iface->i_iter_at_row = NULL;
	iface->i_iter_next = NULL;
	iface->i_iter_prev = NULL;

	iface->i_set_value_at = NULL;
	iface->i_iter_set_value = NULL;
	iface->i_set_values = NULL;
        iface->i_append_values = NULL;
	iface->i_append_row = NULL;
	iface->i_remove_row = NULL;
	iface->i_find_row = NULL;
	
	iface->i_set_notify = NULL;
	iface->i_get_notify = NULL;
	iface->i_send_hint = NULL;

	iface->i_get_exceptions = gda_data_access_wrapper_get_exceptions;
}

static void
gda_data_access_wrapper_init (GdaDataAccessWrapper *model, G_GNUC_UNUSED GdaDataAccessWrapperClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model));
	model->priv = g_new0 (GdaDataAccessWrapperPrivate, 1);
	model->priv->iter_row = -1; /* because model->priv->iter does not yet exist */
	model->priv->rows = NULL;
	model->priv->nb_rows = -1;
	model->priv->end_of_data = FALSE;
	model->priv->last_row = -1;
	
	model->priv->rows_buffer_array = NULL;
	model->priv->rows_buffer_index = NULL;
}

static void
model_row_inserted_cb (G_GNUC_UNUSED GdaDataModel *mod, gint row, GdaDataAccessWrapper *model)
{
	gda_data_model_row_inserted ((GdaDataModel*) model, row);
}

static void
model_row_updated_cb (G_GNUC_UNUSED GdaDataModel *mod, gint row, GdaDataAccessWrapper *model)
{
	gda_data_model_row_updated ((GdaDataModel*) model, row);
}

static void
model_row_removed_cb (G_GNUC_UNUSED GdaDataModel *mod, gint row, GdaDataAccessWrapper *model)
{
	gda_data_model_row_removed ((GdaDataModel*) model, row);
}

static void
model_reset_cb (G_GNUC_UNUSED GdaDataModel *mod, GdaDataAccessWrapper *model)
{
	gda_data_model_reset ((GdaDataModel*) model);
}

static void
clear_internal_state (GdaDataAccessWrapper *model)
{
	if (model->priv) {
		if (model->priv->columns) {
			g_slist_foreach (model->priv->columns, (GFunc) g_object_unref, NULL);
			g_slist_free (model->priv->columns);
			model->priv->columns = NULL;
		}

		model->priv->nb_cols = 0;

		if (model->priv->rows_buffer_array) {
			g_array_free (model->priv->rows_buffer_array, TRUE);
			model->priv->rows_buffer_array = NULL;
		}

		if (model->priv->rows_buffer_index) {
			g_array_free (model->priv->rows_buffer_index, TRUE);
			model->priv->rows_buffer_index = NULL;
		}
	}
}

static void
gda_data_access_wrapper_dispose (GObject *object)
{
	GdaDataAccessWrapper *model = (GdaDataAccessWrapper *) object;

	g_return_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model));

	/* free memory */
	if (model->priv) {
		/* random access model free */
		clear_internal_state (model);

		if (model->priv->iter) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (model->priv->iter),
							      G_CALLBACK (iter_row_changed_cb), model);
			g_signal_handlers_disconnect_by_func (G_OBJECT (model->priv->iter),
							      G_CALLBACK (iter_end_of_data_cb), model);

			g_object_unref (model->priv->iter);
			model->priv->iter = NULL;
		}

		if (model->priv->model) {
			if (model->priv->rows) {
				g_hash_table_destroy (model->priv->rows);
				model->priv->rows = NULL;
			}
			else {
				g_signal_handlers_disconnect_by_func (G_OBJECT (model->priv->model),
								      G_CALLBACK (model_row_inserted_cb), model);
				g_signal_handlers_disconnect_by_func (G_OBJECT (model->priv->model),
								      G_CALLBACK (model_row_updated_cb), model);
				g_signal_handlers_disconnect_by_func (G_OBJECT (model->priv->model),
								      G_CALLBACK (model_row_removed_cb), model);
				g_signal_handlers_disconnect_by_func (G_OBJECT (model->priv->model),
								      G_CALLBACK (model_reset_cb), model);
			}
			g_object_unref (model->priv->model);
			model->priv->model = NULL;
		}
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_data_access_wrapper_finalize (GObject *object)
{
	GdaDataAccessWrapper *model = (GdaDataAccessWrapper *) object;

	g_return_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model));

	/* free memory */
	if (model->priv) {
		g_free (model->priv);
		model->priv = NULL;
	}

	/* chain to parent class */
	parent_class->finalize (object);
}

static void
compute_columns (GdaDataAccessWrapper *model)
{
	if (model->priv->rows_mapping) {
		/* use model->priv->rows_mapping to create columns, and correct it if
		 * needed to remove out of range columns */
		gint *nmapping;
		gint i, j, nb_cols;
		g_assert (!model->priv->columns);
		nmapping = g_new (gint, model->priv->nb_cols);
		nb_cols = gda_data_model_get_n_columns (model->priv->model);
		for (i = 0, j = 0; i < model->priv->nb_cols; i++) {
			gint nb = model->priv->rows_mapping [i];
			if (nb >= nb_cols)
				continue;
			GdaColumn *column;
			column = gda_data_model_describe_column (model->priv->model, nb);
			if (!column)
				continue;
			model->priv->columns = g_slist_append (model->priv->columns,
							       gda_column_copy (column));
			nmapping [j] = nb;
			j++;
		}
		model->priv->nb_cols = j;
		g_free (model->priv->rows_mapping);
		model->priv->rows_mapping = nmapping;
	}
	else
		model->priv->nb_cols = gda_data_model_get_n_columns (model->priv->model);
}

static void
gda_data_access_wrapper_set_property (GObject *object,
				      guint param_id,
				      const GValue *value,
				      GParamSpec *pspec)
{
	GdaDataAccessWrapper *model;

	model = GDA_DATA_ACCESS_WRAPPER (object);
	if (model->priv) {
		switch (param_id) {
		case PROP_MODEL: {
			GdaDataModel *mod = g_value_get_object (value);
			if (mod) {
				g_return_if_fail (GDA_IS_DATA_MODEL (mod));
				model->priv->model_access_flags = gda_data_model_get_access_flags (mod);

				if (model->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_RANDOM) {
					g_signal_connect (G_OBJECT (mod), "row-inserted",
							  G_CALLBACK (model_row_inserted_cb), model);
					g_signal_connect (G_OBJECT (mod), "row-updated",
							  G_CALLBACK (model_row_updated_cb), model);
					g_signal_connect (G_OBJECT (mod), "row-removed",
							  G_CALLBACK (model_row_removed_cb), model);
					g_signal_connect (G_OBJECT (mod), "reset",
							  G_CALLBACK (model_reset_cb), model);
				}
				else {
					model->priv->iter = gda_data_model_create_iter (mod);
					g_return_if_fail (model->priv->iter);
					g_object_set (model->priv->iter, "validate-changes", FALSE,
						      NULL);
					g_signal_connect (G_OBJECT (model->priv->iter), "row-changed",
							  G_CALLBACK (iter_row_changed_cb), model);
					g_signal_connect (G_OBJECT (model->priv->iter), "end-of-data",
							  G_CALLBACK (iter_end_of_data_cb), model);
					model->priv->iter_row = -1; /* because model->priv->iter is invalid */
					model->priv->rows = g_hash_table_new_full (g_int_hash, g_int_equal,
										   g_free,
										   (GDestroyNotify) g_object_unref);
				}
  
                                if (model->priv->model)
					g_object_unref (model->priv->model);

				model->priv->model = mod;
				g_object_ref (mod);

				compute_columns (model);
			}
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gda_data_access_wrapper_get_property (GObject *object,
					guint param_id,
					GValue *value,
					GParamSpec *pspec)
{
	GdaDataAccessWrapper *model;

	model = GDA_DATA_ACCESS_WRAPPER (object);
	if (model->priv) {
		switch (param_id) {
		case PROP_MODEL:
			g_value_set_object (value, G_OBJECT (model->priv->model));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * gda_data_access_wrapper_new:
 * @model: a #GdaDataModel
 *
 * Creates a new #GdaDataModel object which buffers the rows of @model. This object is useful
 * only if @model can only be accessed using cursor based method.
 *
 * Returns: (transfer full): a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_access_wrapper_new (GdaDataModel *model)
{
	GdaDataAccessWrapper *retmodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	
	retmodel = g_object_new (GDA_TYPE_DATA_ACCESS_WRAPPER,
				 "model", model, NULL);
			      
	return GDA_DATA_MODEL (retmodel);
}

/**
 * gda_data_access_wrapper_set_mapping:
 * @wrapper: a #GdaDataAccessWrapper object
 * @mapping: (allow-none) (array length=mapping_size): an array of #gint which represents the mapping between @wrapper's columns
 * and the columns of the wrapped data model
 * @mapping_size: the size of @mapping.
 *
 * @wrapper will report as many columns as @mapping_size, and for each value at position 'i' in @mapping,
 * @wrapper will report the 'i'th column, mapped to the wrapped data model column at position mapping[i].
 * For example if mapping is {3, 4, 0}, then @wrapper will report 3 columns, respectively mapped to the 4th,
 * 5th and 1st columns of the wrapped data model (as column numbers start at 0).
 *
 * If @mapping is %NULL, then no mapping is done and @wrapper's columns will be the same as the wrapped
 * data model.
 *
 * If a column in @mapping does not exist in the wrapped data model, then it is simply ignored (no error
 * reported).
 *
 * Please note that if @wrapper has already been used and if the wrapped data model offers a cursor forward
 * access mode, then this method will return %FALSE and no action will be done.
 *
 * If the mapping is applied, then any existing iterator will be invalid, and @wrapper is reset as if it
 * had just been created.
 *
 * Returns: %TRUE if the mapping actually changed
 *
 * Since: 5.2
 */
gboolean
gda_data_access_wrapper_set_mapping (GdaDataAccessWrapper *wrapper, gint *mapping, gint mapping_size)
{
	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (wrapper), FALSE);

	if ((! (wrapper->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD)) &&
	    (wrapper->priv->iter_row >= 0)) {
		/* error */
		return FALSE;
	}

	clear_internal_state (wrapper);

	if (mapping) {
		/* define mapping */
		wrapper->priv->rows_mapping = g_new (gint, mapping_size);
		memcpy (wrapper->priv->rows_mapping, mapping, mapping_size * sizeof (gint));
		wrapper->priv->nb_cols = mapping_size;
	}
	else {
		if (wrapper->priv->rows_mapping) {
			g_free (wrapper->priv->rows_mapping);
			wrapper->priv->rows_mapping = NULL;
		}
	}

	compute_columns (wrapper);
	gda_data_model_reset ((GdaDataModel*) wrapper);

	return TRUE;
}

/*
 * GdaDataModel interface implementation
 */
static gint
gda_data_access_wrapper_get_n_rows (GdaDataModel *model)
{
	GdaDataAccessWrapper *imodel;
	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), 0);
	imodel = (GdaDataAccessWrapper*) model;
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->nb_rows >= 0)
		return imodel->priv->nb_rows;

	if (imodel->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		/* imodel->priv->mode is a random access model, use it */
		imodel->priv->nb_rows = gda_data_model_get_n_rows (imodel->priv->model);
	else {
		/* go at the end */
		while (!imodel->priv->end_of_data) {
			if (! gda_data_model_iter_move_next (imodel->priv->iter)) 
				break;
		}
		if (imodel->priv->end_of_data)
			imodel->priv->nb_rows = imodel->priv->last_row +1;
		else
			imodel->priv->nb_rows = -1;
	}

	return imodel->priv->nb_rows;
}

static gint
gda_data_access_wrapper_get_n_columns (GdaDataModel *model)
{
	GdaDataAccessWrapper *imodel;
	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), 0);
	imodel = (GdaDataAccessWrapper*) model;
	g_return_val_if_fail (imodel->priv, 0);
	
	if (imodel->priv->model)
		return imodel->priv->nb_cols;
	else
		return 0;
}

static GdaColumn *
gda_data_access_wrapper_describe_column (GdaDataModel *model, gint col)
{
	GdaDataAccessWrapper *imodel;
	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), NULL);
	imodel = (GdaDataAccessWrapper*) model;
	g_return_val_if_fail (imodel->priv, NULL);

	if (imodel->priv->model) {
		if (imodel->priv->columns)
			return g_slist_nth_data (imodel->priv->columns, col);
		else
			return gda_data_model_describe_column (imodel->priv->model, col);
	}
	else
		return NULL;
}

static GdaDataModelAccessFlags
gda_data_access_wrapper_get_access_flags (GdaDataModel *model)
{
	GdaDataAccessWrapper *imodel;

	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), 0);
	imodel = (GdaDataAccessWrapper*) model;
	g_return_val_if_fail (imodel->priv, 0);
	
	return GDA_DATA_MODEL_ACCESS_RANDOM;
}

static GdaRow *
create_new_row (GdaDataAccessWrapper *model)
{
	gint i;
	GdaRow *row;

	row = gda_row_new (model->priv->nb_cols);
	for (i = 0; i < model->priv->nb_cols; i++) {
		GdaHolder *holder;
		GValue *dest;
		dest = gda_row_get_value (row, i);
		if (model->priv->rows_mapping)
			holder = gda_set_get_nth_holder ((GdaSet *) model->priv->iter, model->priv->rows_mapping [i]);
		else
			holder = gda_set_get_nth_holder ((GdaSet *) model->priv->iter, i);
		if (holder) {
			const GValue *cvalue = gda_holder_get_value (holder);
			if (cvalue) {
				gda_value_reset_with_type (dest, G_VALUE_TYPE ((GValue *) cvalue));
				g_value_copy (cvalue, dest);
			}
			else
				gda_value_set_null (dest);
		}
		else
			gda_row_invalidate_value (row, dest);
	}

	gint *ptr;
	ptr = g_new (gint, 1);
	*ptr = model->priv->iter_row;
	g_hash_table_insert (model->priv->rows, ptr, row);
	/*g_print ("%s(%d)\n", __FUNCTION__, model->priv->iter_row);*/

	return row;
}

static const GValue *
gda_data_access_wrapper_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaDataAccessWrapper *imodel;

	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), NULL);
	imodel = (GdaDataAccessWrapper*) model;
	g_return_val_if_fail (imodel->priv, NULL);
	g_return_val_if_fail (imodel->priv->model, NULL);
	g_return_val_if_fail (row >= 0, NULL);

	if (col >= imodel->priv->nb_cols) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
			     _("Column %d out of range (0-%d)"), col, imodel->priv->nb_cols - 1);
		return NULL;
	}

	if (!imodel->priv->rows) {
		/* imodel->priv->model is a random access model, use it */
		if (imodel->priv->rows_mapping)
			return gda_data_model_get_value_at (imodel->priv->model, imodel->priv->rows_mapping [col],
							    row, error);
		else
			return gda_data_model_get_value_at (imodel->priv->model, col, row, error);
	}
	else {
		GdaRow *gda_row;
		gint tmp;
		tmp = row;
		gda_row = g_hash_table_lookup (imodel->priv->rows, &tmp);
		if (gda_row) {
			GValue *val = gda_row_get_value (gda_row, col);
			if (gda_row_value_is_valid (gda_row, val))
				return val;
			else
				return NULL;
		}
		else {
			g_assert (imodel->priv->iter);
			if (imodel->priv->iter_row < 0) {
				if (gda_data_model_iter_move_next (imodel->priv->iter)) {
					tmp = row;
					gda_row = g_hash_table_lookup (imodel->priv->rows, &tmp);
					if (row == imodel->priv->iter_row) {
						GValue *val = gda_row_get_value (gda_row, col);
						if (gda_row_value_is_valid (gda_row, val))
							return val;
						else
							return NULL;
					}
				}
				else {
					g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
						      "%s", _("Can't set iterator's position"));
					return NULL;
				}
			}
				
			gda_row = NULL;
			if (row != imodel->priv->iter_row) {
				if (row > imodel->priv->iter_row) {
					/* need to move forward */
					while ((imodel->priv->iter_row < row) && 
					       gda_data_model_iter_move_next (imodel->priv->iter));
				}
				else {
					/* need to move backwards */
					g_assert (imodel->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD);
					while ((imodel->priv->iter_row > row) && 
					       gda_data_model_iter_move_prev (imodel->priv->iter)) ;
				}
			}

			if (! (imodel->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD) ||
			    ! (imodel->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD)) {
				tmp = row;
				gda_row = g_hash_table_lookup (imodel->priv->rows, &tmp);

				if (gda_row) {
					GValue *val = gda_row_get_value (gda_row, col);
					if (gda_row_value_is_valid (gda_row, val))
						return val;
					else
						return NULL;
				}
			}
			else {
				/* in this case iter can be moved forward and backward at will => we only
				 * need to keep a pool of GdaRow for performances reasons */
				tmp = row;
				gda_row = g_hash_table_lookup (imodel->priv->rows, &tmp);

				if (!gda_row) {
					if (! imodel->priv->rows_buffer_array) {
						imodel->priv->rows_buffer_array = g_array_sized_new (FALSE, FALSE, 
												     sizeof (GdaRow*),
												     ROWS_POOL_SIZE);
						imodel->priv->rows_buffer_index = g_array_sized_new (FALSE, FALSE, 
												     sizeof (gint), 
												     ROWS_POOL_SIZE);
					}
					else if (imodel->priv->rows_buffer_array->len == ROWS_POOL_SIZE) {
						/* get rid of the oldest row (was model's index_row row)*/
						gint index_row;

						index_row = g_array_index (imodel->priv->rows_buffer_index, gint,
									   ROWS_POOL_SIZE - 1);
						g_array_remove_index (imodel->priv->rows_buffer_array,
								      ROWS_POOL_SIZE - 1);
						g_array_remove_index (imodel->priv->rows_buffer_index,
								      ROWS_POOL_SIZE - 1);
						g_hash_table_remove (imodel->priv->rows, &index_row);
					}
					if (gda_data_model_iter_move_to_row (imodel->priv->iter, row)) {
						gda_row = create_new_row (imodel);
						g_array_prepend_val (imodel->priv->rows_buffer_array, gda_row);
						g_array_prepend_val (imodel->priv->rows_buffer_index, imodel->priv->iter_row);
					}
				}

				GValue *val;
				val = gda_row ? gda_row_get_value (gda_row, col) : NULL;
				if (gda_row && gda_row_value_is_valid (gda_row, val))
					return val;
				else
					return NULL;
			}
		}
	}

	g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
		      "%s", _("Can't access data"));
	return NULL;
}

static void
iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdaDataAccessWrapper *model)
{
	g_assert (model->priv->rows);

	/*g_print ("%s(%d)\n", __FUNCTION__, row);*/
	if (gda_data_model_iter_is_valid (iter)) {
		model->priv->iter_row = row;
		if (model->priv->last_row < row)
			model->priv->last_row = row;

		if (! (model->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD) ||
		    ! (model->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD)) {
			/* keep the changes in rows */
			GdaRow *gda_row;
			gint tmp;
			tmp = row;
			gda_row = g_hash_table_lookup (model->priv->rows, &tmp);
			if (!gda_row)
				create_new_row (model);
		}
	}
}

static void
iter_end_of_data_cb (G_GNUC_UNUSED GdaDataModelIter *iter, GdaDataAccessWrapper *model)
{
	g_assert (GDA_IS_DATA_ACCESS_WRAPPER (model));
	model->priv->end_of_data = TRUE;
}

static GdaValueAttribute
gda_data_access_wrapper_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	GdaValueAttribute flags = 0;
	GdaDataAccessWrapper *imodel;

	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), 0);
	imodel = (GdaDataAccessWrapper *) model;
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->model) {
		if (imodel->priv->rows_mapping)
			flags = gda_data_model_get_attributes_at (imodel->priv->model, imodel->priv->rows_mapping [col],
								  row);
		else
			flags = gda_data_model_get_attributes_at (imodel->priv->model, col, row);
	}

	flags |= GDA_VALUE_ATTR_NO_MODIF;
	
	return flags;
}

static GError **
gda_data_access_wrapper_get_exceptions (GdaDataModel *model)
{
	GdaDataAccessWrapper *imodel;

	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), NULL);
	imodel = (GdaDataAccessWrapper *) model;
	g_return_val_if_fail (imodel->priv, NULL);
	return gda_data_model_get_exceptions (imodel->priv->model);
}
