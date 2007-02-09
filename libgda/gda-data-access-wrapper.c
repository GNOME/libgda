/* GDA library
 * Copyright (C) 2006 The GNOME Foundation.
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
#include <libgda/gda-parameter.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-row.h>


struct _GdaDataAccessWrapperPrivate {
	GdaDataModel     *model;
	GdaDataModelAccessFlags model_access_flags;

	GdaDataModelIter *iter;    /* iterator on @model, NULL if @model already is random access */
	gint              iter_row;/* current row of @iter, starting at 0 when created */

	GHashTable       *rows;    /* NULL if @model already is random access */
	gint              nb_cols; /* number of columns of @model */
	gint              last_row;/* row number of the last row which has been read */
	gboolean          end_of_data; /* TRUE if the end of the data model has been reached by the iterator */
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
static void                 gda_data_access_wrapper_data_model_init (GdaDataModelClass *iface);
static gint                 gda_data_access_wrapper_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_access_wrapper_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_access_wrapper_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_access_wrapper_get_access_flags(GdaDataModel *model);
static const GValue      *gda_data_access_wrapper_get_value_at    (GdaDataModel *model, gint col, gint row);
static GdaValueAttribute    gda_data_access_wrapper_get_attributes_at (GdaDataModel *model, gint col, gint row);
#ifdef GDA_DEBUG
static void gda_data_access_wrapper_dump (GdaDataAccessWrapper *model, guint offset);
#endif

static void iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdaDataAccessWrapper *model);
static void iter_end_of_data_cb (GdaDataModelIter *iter, GdaDataAccessWrapper *model);

static GObjectClass *parent_class = NULL;

/**
 * gda_data_access_wrapper_get_type
 *
 * Returns: the #GType of GdaDataAccessWrapper.
 */
GType
gda_data_access_wrapper_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataAccessWrapperClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_access_wrapper_class_init,
			NULL,
			NULL,
			sizeof (GdaDataAccessWrapper),
			0,
			(GInstanceInitFunc) gda_data_access_wrapper_init
		};

		static const GInterfaceInfo data_model_info = {
			(GInterfaceInitFunc) gda_data_access_wrapper_data_model_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDataAccessWrapper", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
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
                                         g_param_spec_object ("model", "Data model being worked on", NULL,
                                                              GDA_TYPE_DATA_MODEL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	/* virtual functions */
	object_class->dispose = gda_data_access_wrapper_dispose;
	object_class->finalize = gda_data_access_wrapper_finalize;
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (klass)->dump = (void (*)(GdaObject *, guint)) gda_data_access_wrapper_dump;
#endif

	/* class attributes */
	GDA_OBJECT_CLASS (klass)->id_unique_enforced = FALSE;
}

static void
gda_data_access_wrapper_data_model_init (GdaDataModelClass *iface)
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
gda_data_access_wrapper_init (GdaDataAccessWrapper *model, GdaDataAccessWrapperClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model));
	model->priv = g_new0 (GdaDataAccessWrapperPrivate, 1);
	model->priv->iter_row = -1; /* because model->priv->iter does not yet exist */
	model->priv->rows = NULL;
	model->priv->end_of_data = FALSE;
}

static void
data_model_destroyed_cb (GdaDataModel *mod, GdaDataAccessWrapper *model)
{
	g_assert (model->priv->model == mod);
	g_signal_handlers_disconnect_by_func (mod, G_CALLBACK (data_model_destroyed_cb), model);

	if (model->priv->rows) {
		g_hash_table_destroy (model->priv->rows);
		model->priv->rows = NULL;
	}
	model->priv->model = NULL;

	g_object_unref (mod);
}

static void
gda_data_access_wrapper_dispose (GObject *object)
{
	GdaDataAccessWrapper *model = (GdaDataAccessWrapper *) object;

	g_return_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model));

	/* free memory */
	if (model->priv) {
		if (model->priv->rows) {
			g_hash_table_destroy (model->priv->rows);
			model->priv->rows = NULL;
		}

		if (model->priv->iter) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (model->priv->iter),
							      G_CALLBACK (iter_row_changed_cb), model);
			g_signal_handlers_disconnect_by_func (G_OBJECT (model->priv->iter),
							      G_CALLBACK (iter_end_of_data_cb), model);

			g_object_unref (model->priv->iter);
			model->priv->iter = NULL;
		}

		/* random access model free */
		if (model->priv->model) 
			data_model_destroyed_cb (model->priv->model, model);
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
					g_signal_connect (G_OBJECT (model->priv->iter), "row_changed",
							  G_CALLBACK (iter_row_changed_cb), model);
					g_signal_connect (G_OBJECT (model->priv->iter), "end_of_data",
							  G_CALLBACK (iter_end_of_data_cb), model);
					model->priv->iter_row = -1; /* because model->priv->iter is invalid */
					model->priv->rows = g_hash_table_new_full (g_direct_hash, g_direct_equal,
										   NULL, (GDestroyNotify) g_object_unref);
				}
  
                                if(model->priv->model)
                                  g_object_unref(model->priv->model);

				model->priv->model = mod;
				g_object_ref (mod);
				gda_object_connect_destroy (GDA_OBJECT (mod), 
							    G_CALLBACK (data_model_destroyed_cb), model);
				model->priv->nb_cols = gda_data_model_get_n_columns (mod);
			}
			break;
		}
		default:
			g_assert_not_reached ();
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
			g_assert_not_reached ();
			break;
		}
	}
}

/**
 * gda_data_access_wrapper_new
 * @model: a #GdaDataModel
 *
 * Creates a new #GdaDataModel object which buffers the rows of @model. This object is usefull
 * only if @model can only be accessed using cursor based method.
 *
 * Returns: a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_access_wrapper_new (GdaDataModel *model)
{
	GdaDataAccessWrapper *retmodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	
	retmodel = g_object_new (GDA_TYPE_DATA_ACCESS_WRAPPER,
				 "dict", gda_object_get_dict (GDA_OBJECT (model)),
				 "model", model, NULL);
			      
	return GDA_DATA_MODEL (retmodel);
}

/**
 * gda_data_access_wrapper_row_exists
 * @wrapper: a #GdaDataAccessWrapper objects
 * @row: a row number to test existance
 *
 * Tests if the wrapper model of @wrapper has a row number @row
 *
 * Returns: TRUE if row number @row exists
 */
gboolean
gda_data_access_wrapper_row_exists (GdaDataAccessWrapper *wrapper, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (wrapper), FALSE);
	
	if (wrapper->priv->nb_cols == 0)
		return FALSE;

	if (gda_data_model_get_value_at ((GdaDataModel*) wrapper, 0, row))
		return TRUE;
	else
		return FALSE;
}

#ifdef GDA_DEBUG
static void
gda_data_access_wrapper_dump (GdaDataAccessWrapper *model, guint offset)
{
	gchar *str;

	str = g_new0 (gchar, offset+1);
	memset (str, ' ', offset);
	if (model->priv) {
		gint i;
		if (model->priv->rows) {
			for (i = 0; i <= model->priv->last_row; i++) {
				GdaRow *row = g_hash_table_lookup (model->priv->rows, GINT_TO_POINTER (i));
				g_print ("%srow %d: %s\n", str, i, row ? "SET" : "---");
			}
		}
		if (model->priv->model) 
			gda_data_model_dump (model->priv->model, stdout);
	}
	else
		g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, model);
	g_free (str);
}
#endif

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

	row = gda_row_new ((GdaDataModel *) model, model->priv->nb_cols);
	for (i = 0; i < model->priv->nb_cols; i++) {
		GdaParameter *param;

		param = gda_data_model_iter_get_param_for_column (model->priv->iter, i);
		if (param)
			gda_row_set_value (row, i, gda_parameter_get_value (param));
	}

	g_hash_table_insert (model->priv->rows, GINT_TO_POINTER (model->priv->iter_row), row);
	/*g_print ("%s(%d)\n", __FUNCTION__, model->priv->iter_row);*/

	return row;
}

static const GValue *
gda_data_access_wrapper_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaDataAccessWrapper *imodel;

	g_return_val_if_fail (GDA_IS_DATA_ACCESS_WRAPPER (model), NULL);
	imodel = GDA_DATA_ACCESS_WRAPPER (model);
	g_return_val_if_fail (imodel->priv, NULL);
	g_return_val_if_fail (imodel->priv->model, NULL);
	g_return_val_if_fail (row >= 0, NULL);

	if (col >= imodel->priv->nb_cols) {
		g_warning (_("Column %d out of range 0 - %d"), col, imodel->priv->nb_cols);
		return NULL;
	}

	if (!imodel->priv->rows)
		/* imodel->priv->model is a random access model, use it */
		return gda_data_model_get_value_at (imodel->priv->model, col, row);
	else {
		GdaRow *gda_row;

		gda_row = g_hash_table_lookup (imodel->priv->rows, GINT_TO_POINTER (row));
		if (gda_row) 
			return gda_row_get_value (gda_row, col);
		else {
			g_assert (imodel->priv->iter);
			if (imodel->priv->iter_row < 0) {
				if (gda_data_model_iter_move_next (imodel->priv->iter)) {
					gda_row = create_new_row (imodel);
					if (row == imodel->priv->iter_row)
						return gda_row_get_value (gda_row, col);
				}
				else
					return NULL;
			}
				
			gda_row = NULL;
			if (row != imodel->priv->iter_row) {
				if (row > imodel->priv->iter_row) {
					/* need to move forward */
					while ((imodel->priv->iter_row < row) && 
					       gda_data_model_iter_move_next (imodel->priv->iter)) ;
				}
				else {
					/* need to move backwards */
					while ((imodel->priv->iter_row > row) && 
					       gda_data_model_iter_move_prev (imodel->priv->iter)) ;
				}
			}

			if (row == imodel->priv->iter_row) 
				gda_row = create_new_row (imodel);

			if (gda_row)
				return gda_row_get_value (gda_row, col);
		}
	}

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
				/*gda_object_dump (model, 10);*/
			}
		}
	}
}

static void
iter_end_of_data_cb (GdaDataModelIter *iter, GdaDataAccessWrapper *model)
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
	
	flags = GDA_VALUE_ATTR_NO_MODIF;
	TO_IMPLEMENT;
	
	return flags;
}
