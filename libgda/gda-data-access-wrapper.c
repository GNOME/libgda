/* GDA library
 * Copyright (C) 2006 - 2010 The GNOME Foundation.
 *
 * AUTHORS:
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
	gint              nb_cols; /* number of columns of @model */
	gint              last_row;/* row number of the last row which has been read */
	gboolean          end_of_data; /* TRUE if the end of the data model has been reached by the iterator */

	GArray           *rows_buffer_array; /* Array of GdaRow */
	GArray           *rows_buffer_index; /* Array of indexes: GdaRow at index i in @rows_buffer_array
					      * is indexed in @rows with key rows_buffer_index[i] */
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
}

static void
gda_data_access_wrapper_init (GdaDataAccessWrapper *model, G_GNUC_UNUSED GdaDataAccessWrapperClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model));
	model->priv = g_new0 (GdaDataAccessWrapperPrivate, 1);
	model->priv->iter_row = -1; /* because model->priv->iter does not yet exist */
	model->priv->rows = NULL;
	model->priv->end_of_data = FALSE;
	
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
gda_data_access_wrapper_dispose (GObject *object)
{
	GdaDataAccessWrapper *model = (GdaDataAccessWrapper *) object;

	g_return_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model));

	/* free memory */
	if (model->priv) {
		if (model->priv->iter) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (model->priv->iter),
							      G_CALLBACK (iter_row_changed_cb), model);
			g_signal_handlers_disconnect_by_func (G_OBJECT (model->priv->iter),
							      G_CALLBACK (iter_end_of_data_cb), model);

			g_object_unref (model->priv->iter);
			model->priv->iter = NULL;
		}

		/* random access model free */
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
			}
			g_object_unref (model->priv->model);
			model->priv->model = NULL;
		}

		if (model->priv->rows_buffer_array) {
			g_array_free (model->priv->rows_buffer_array, TRUE);
			model->priv->rows_buffer_array = NULL;
		}

		if (model->priv->rows_buffer_index) {
			g_array_free (model->priv->rows_buffer_index, TRUE);
			model->priv->rows_buffer_index = NULL;
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
			GdaDataModel *mod = g_value_get_object(value);
			if (mod) {
				g_return_if_fail (GDA_IS_DATA_MODEL (mod));
				model->priv->model_access_flags = gda_data_model_get_access_flags (mod);
				if (! (model->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_RANDOM)) {
					model->priv->iter = gda_data_model_create_iter (mod);
					g_return_if_fail (model->priv->iter);
					g_signal_connect (G_OBJECT (model->priv->iter), "row-changed",
							  G_CALLBACK (iter_row_changed_cb), model);
					g_signal_connect (G_OBJECT (model->priv->iter), "end-of-data",
							  G_CALLBACK (iter_end_of_data_cb), model);
					model->priv->iter_row = -1; /* because model->priv->iter is invalid */
					model->priv->rows = g_hash_table_new_full (g_direct_hash, g_direct_equal,
										   NULL, (GDestroyNotify) g_object_unref);
				}
				else {
					g_signal_connect (G_OBJECT (mod), "row-inserted",
							  G_CALLBACK (model_row_inserted_cb), model);
					g_signal_connect (G_OBJECT (mod), "row-updated",
							  G_CALLBACK (model_row_updated_cb), model);
					g_signal_connect (G_OBJECT (mod), "row-removed",
							  G_CALLBACK (model_row_removed_cb), model);
				}
  
                                if (model->priv->model)
					g_object_unref (model->priv->model);

				model->priv->model = mod;
				g_object_ref (mod);
				model->priv->nb_cols = gda_data_model_get_n_columns (mod);
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

/*
 * GdaDataModel interface implementation
 */
static gint
gda_data_access_wrapper_get_n_rows (GdaDataModel *model)
{
	GdaDataAccessWrapper *imodel;
	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), 0);
	imodel = GDA_DATA_ACCESS_WRAPPER (model);
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		/* imodel->priv->mode is a random access model, use it */
		return gda_data_model_get_n_rows (imodel->priv->model);
	else {
		/* go at the end */
		while (!imodel->priv->end_of_data) {
			if (! gda_data_model_iter_move_next (imodel->priv->iter)) 
				break;
		}
		if (imodel->priv->end_of_data)
			return imodel->priv->last_row +1;
		else
			return -1;
	}
}

static gint
gda_data_access_wrapper_get_n_columns (GdaDataModel *model)
{
	GdaDataAccessWrapper *imodel;
	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), 0);
	imodel = GDA_DATA_ACCESS_WRAPPER (model);
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
	imodel = GDA_DATA_ACCESS_WRAPPER (model);
	g_return_val_if_fail (imodel->priv, NULL);

	if (imodel->priv->model)
		return gda_data_model_describe_column (imodel->priv->model, col);
	else
		return NULL;
}

static GdaDataModelAccessFlags
gda_data_access_wrapper_get_access_flags (GdaDataModel *model)
{
	GdaDataAccessWrapper *imodel;

	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), 0);
	imodel = GDA_DATA_ACCESS_WRAPPER (model);
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
		holder = gda_data_model_iter_get_holder_for_field (model->priv->iter, i);
		if (holder) {
			const GValue *cvalue = gda_holder_get_value (holder);
			if (cvalue) {
				gda_value_reset_with_type (dest, G_VALUE_TYPE ((GValue *) cvalue));
				gda_value_set_from_value (dest, cvalue);
			}
			else
				gda_value_set_null (dest);
		}
		else
			gda_row_invalidate_value (row, dest);
	}

	g_hash_table_insert (model->priv->rows, GINT_TO_POINTER (model->priv->iter_row), row);
	/*g_print ("%s(%d)\n", __FUNCTION__, model->priv->iter_row);*/

	return row;
}

static const GValue *
gda_data_access_wrapper_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaDataAccessWrapper *imodel;

	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), NULL);
	imodel = GDA_DATA_ACCESS_WRAPPER (model);
	g_return_val_if_fail (imodel->priv, NULL);
	g_return_val_if_fail (imodel->priv->model, NULL);
	g_return_val_if_fail (row >= 0, NULL);

	if (col >= imodel->priv->nb_cols) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
			     _("Column %d out of range (0-%d)"), col, imodel->priv->nb_cols - 1);
		return NULL;
	}

	if (!imodel->priv->rows)
		/* imodel->priv->model is a random access model, use it */
		return gda_data_model_get_value_at (imodel->priv->model, col, row, error);
	else {
		GdaRow *gda_row;

		gda_row = g_hash_table_lookup (imodel->priv->rows, GINT_TO_POINTER (row));
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
					gda_row = g_hash_table_lookup (imodel->priv->rows, GINT_TO_POINTER (row));
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
				gda_row = g_hash_table_lookup (imodel->priv->rows, GINT_TO_POINTER (row));

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
				gda_row = g_hash_table_lookup (imodel->priv->rows, GINT_TO_POINTER (row));

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
						g_hash_table_remove (imodel->priv->rows, GINT_TO_POINTER (index_row));
					}

					gda_row = create_new_row (imodel);
					g_array_prepend_val (imodel->priv->rows_buffer_array, gda_row);
					g_array_prepend_val (imodel->priv->rows_buffer_index, imodel->priv->iter_row);
				}
				GValue *val = gda_row_get_value (gda_row, col);
				if (gda_row_value_is_valid (gda_row, val))
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

	if (gda_data_model_iter_is_valid (iter)) {
		/*g_print ("%s(%d)\n", __FUNCTION__, row);*/
		model->priv->iter_row = row;
		if (model->priv->last_row < row)
			model->priv->last_row = row;

		if (! (model->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD) ||
		    ! (model->priv->model_access_flags & GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD)) {
			/* keep the changes in rows */
			GdaRow *gda_row;
			
			gda_row = g_hash_table_lookup (model->priv->rows, GINT_TO_POINTER (row));
			if (!gda_row) {
				create_new_row (model);
			}
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

	if (imodel->priv->model)
		flags = gda_data_model_get_attributes_at (imodel->priv->model, col, row);
	flags |= GDA_VALUE_ATTR_NO_MODIF;
	
	return flags;
}
