/* GDA library
 * Copyright (C)  2005 The GNOME Foundation.
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

#include <glib/gi18n-lib.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-data-model-query.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-query.h>
#include <libgda/gda-dict.h>
#include <libgda/gda-entity.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-renderer.h>

struct _GdaDataModelQueryPrivate {
	GdaQuery         *query;
	GdaParameterList *params; /* ??? */
	GdaDataModel     *data;
};

/* properties */
enum
{
        PROP_0,
        PROP_QUERY,
};

static void gda_data_model_query_class_init (GdaDataModelQueryClass *klass);
static void gda_data_model_query_init       (GdaDataModelQuery *model,
					      GdaDataModelQueryClass *klass);
static void gda_data_model_query_dispose    (GObject *object);
static void gda_data_model_query_finalize   (GObject *object);

static void gda_data_model_query_set_property (GObject *object,
						guint param_id,
						const GValue *value,
						GParamSpec *pspec);
static void gda_data_model_query_get_property (GObject *object,
						guint param_id,
						GValue *value,
						GParamSpec *pspec);

/* GdaDataModel interface */
static void                 gda_data_model_query_data_model_init (GdaDataModelClass *iface);
static gint                 gda_data_model_query_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_query_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_query_describe_column (GdaDataModel *model, gint col);
static guint                gda_data_model_query_get_access_flags(GdaDataModel *model);
static const GdaValue      *gda_data_model_query_get_value_at    (GdaDataModel *model, gint col, gint row);
static GdaDataModelIter    *gda_data_model_query_create_iter     (GdaDataModel *model, gint row);
static gboolean             gda_data_model_query_iter_at_row     (GdaDataModel *model, 
								   GdaDataModelIter *iter, gint row);
static gboolean             gda_data_model_query_iter_next       (GdaDataModel *model, 
								   GdaDataModelIter *iter);
static gboolean             gda_data_model_query_iter_prev       (GdaDataModel *model, 
								   GdaDataModelIter *iter);

static gboolean             gda_data_model_query_set_value_at    (GdaDataModel *model, gint col, gint row, 
								   const GdaValue *value, GError **error);
static gboolean             gda_data_model_query_set_values      (GdaDataModel *model, gint row, 
								   GList *values, GError **error);
static gint                 gda_data_model_query_append_values   (GdaDataModel *model, const GList *values, GError **error);
static gint                 gda_data_model_query_append_row      (GdaDataModel *model, GError **error);
static gboolean             gda_data_model_query_remove_row      (GdaDataModel *model, gint row, 
								   GError **error);

static void to_be_destroyed_query_cb (GdaQuery *query, GdaDataModelQuery *model);

#ifdef debug
static void gda_data_model_query_dump (GdaDataModelQuery *select, guint offset);
#endif

static GObjectClass *parent_class = NULL;

/**
 * gda_data_model_query_get_type
 *
 * Returns: the #GType of GdaDataModelQuery.
 */
GType
gda_data_model_query_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelQueryClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_query_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelQuery),
			0,
			(GInstanceInitFunc) gda_data_model_query_init
		};

		static const GInterfaceInfo data_model_info = {
			(GInterfaceInitFunc) gda_data_model_query_data_model_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDataModelQuery", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
	}
	return type;
}

static void 
gda_data_model_query_class_init (GdaDataModelQueryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* properties */
	object_class->set_property = gda_data_model_query_set_property;
        object_class->get_property = gda_data_model_query_get_property;
	g_object_class_install_property (object_class, PROP_QUERY,
                                         g_param_spec_pointer ("query", "Query", 
							       "SELECT Query to be executed to populate "
							       "the model with data", 
							       G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY));
	/* virtual functions */
	object_class->dispose = gda_data_model_query_dispose;
	object_class->finalize = gda_data_model_query_finalize;
#ifdef debug
        GDA_OBJECT_CLASS (klass)->dump = (void (*)(GdaObject *, guint)) gda_data_model_query_dump;
#endif

	/* class attributes */
	GDA_OBJECT_CLASS (klass)->id_unique_enforced = FALSE;
}

static void
gda_data_model_query_data_model_init (GdaDataModelClass *iface)
{
	iface->i_get_n_rows = gda_data_model_query_get_n_rows;
	iface->i_get_n_columns = gda_data_model_query_get_n_columns;
	iface->i_describe_column = gda_data_model_query_describe_column;
        iface->i_get_access_flags = gda_data_model_query_get_access_flags;
	iface->i_get_value_at = gda_data_model_query_get_value_at;

	iface->i_create_iter = gda_data_model_query_create_iter;
	iface->i_iter_at_row = gda_data_model_query_iter_at_row;
	iface->i_iter_next = gda_data_model_query_iter_next;
	iface->i_iter_prev = gda_data_model_query_iter_prev;

	iface->i_set_value_at = gda_data_model_query_set_value_at;
	iface->i_set_values = gda_data_model_query_set_values;
        iface->i_append_values = gda_data_model_query_append_values;
	iface->i_append_row = gda_data_model_query_append_row;
	iface->i_remove_row = gda_data_model_query_remove_row;
	
	iface->i_set_notify = NULL;
	iface->i_get_notify = NULL;
}

static void
gda_data_model_query_init (GdaDataModelQuery *model, GdaDataModelQueryClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (model));
	model->priv = g_new0 (GdaDataModelQueryPrivate, 1);
	model->priv->query = NULL;
	model->priv->params = NULL;
	model->priv->data = NULL;
}

static void
gda_data_model_query_dispose (GObject *object)
{
	GdaDataModelQuery *model = (GdaDataModelQuery *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (model));

	/* free memory */
	if (model->priv) {
		if (model->priv->query)
			to_be_destroyed_query_cb (model->priv->query, model);
		
		if (model->priv->params) {
			g_object_unref (model->priv->params);
			model->priv->params = NULL;
		}

		if (model->priv->data) {
			g_object_unref (model->priv->data);
			model->priv->data = NULL;
		}
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_data_model_query_finalize (GObject *object)
{
	GdaDataModelQuery *model = (GdaDataModelQuery *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (model));

	/* free memory */
	if (model->priv) {
		g_free (model->priv);
		model->priv = NULL;
	}

	/* chain to parent class */
	parent_class->finalize (object);
}

static void
query_changed_cb (GdaQuery *query, GdaDataModelQuery *model)
{
	g_error ("Query for GdaDataModelQuery has changed, which is not supported!");
}

static void
gda_data_model_query_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdaDataModelQuery *model;
	model = GDA_DATA_MODEL_QUERY (object);
	if (model->priv) {
		switch (param_id) {
		case PROP_QUERY:
			if (model->priv->query)
				to_be_destroyed_query_cb (model->priv->query, model);

			/* use @query without making a copy of it */
			model->priv->query = (GdaQuery *) g_value_get_pointer (value);
			if (model->priv->query) {
				g_object_ref (model->priv->query);
				g_signal_connect (model->priv->query, "to_be_destroyed",
						  G_CALLBACK (to_be_destroyed_query_cb), model);
				model->priv->params = gda_entity_get_param_list (GDA_ENTITY (model->priv->query));
				g_signal_connect (model->priv->query, "changed",
						  G_CALLBACK (query_changed_cb), model);
			}
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
}

static void
gda_data_model_query_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	GdaDataModelQuery *model;
	model = GDA_DATA_MODEL_QUERY (object);
	if (model->priv) {
		switch (param_id) {
		case PROP_QUERY:
			g_value_set_pointer (value, model->priv->query);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
}

static void
to_be_destroyed_query_cb (GdaQuery *query, GdaDataModelQuery *model)
{
	g_assert (query == model->priv->query);
	g_signal_handlers_disconnect_by_func (query,
					      G_CALLBACK (to_be_destroyed_query_cb), model);
	g_signal_handlers_disconnect_by_func (query,
					      G_CALLBACK (query_changed_cb), model);

	g_object_unref (query);
	model->priv->query = NULL;
}


/**
 * gda_data_model_query_new
 * @query: a SELECT query 
 *
 * Creates a new #GdaDataModel object using the data returned by the execution of the
 * @query SELECT query.
 *
 * Returns: a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_query_new (GdaQuery *query)
{
	GdaDataModelQuery *model;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);

	model = g_object_new (GDA_TYPE_DATA_MODEL_QUERY, "dict", gda_object_get_dict (GDA_OBJECT (query)),
			      "query", query, NULL);

	return GDA_DATA_MODEL (model);
}

/**
 * gda_data_model_query_get_param_list
 * @model: a #GdaDataModelQuery data model
 *
 * If some parameters are required to execute the SELECT query used in the @model data model, then
 * creates a new #GdaParameterList; otherwise does nothing and returns %NULL.
 *
 * Returns: a new #GdaParameterList object, or %NULL
 */
GdaParameterList *
gda_data_model_query_get_param_list (GdaDataModelQuery *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), NULL);
	g_return_val_if_fail (model->priv, NULL);

	return model->priv->params;
}

/**
 * gda_data_model_query_refresh
 */
gboolean
gda_data_model_query_refresh (GdaDataModelQuery *model, GError **error)
{
	gchar *sql;
	GdaCommand *cmd;
	GdaConnection *cnc;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	if (model->priv->data) {
		g_object_unref (model->priv->data);
		model->priv->data = NULL;
	}

	if (! model->priv->query)
		return TRUE;
	if (!gda_query_is_select_query (model->priv->query)) {
		g_set_error (error, 0, 0,
			     _("Query is not a SELECT query"));
		return FALSE;
	}

	cnc = gda_dict_get_connection (gda_object_get_dict (GDA_OBJECT (model)));
	if (!cnc) {
		g_set_error (error, 0, 0,
			     _("No connection specified"));
		return FALSE;
	}

	if (!gda_connection_is_open (cnc)) {
		g_set_error (error, 0, 0,
			     _("Connection is not opened"));
		return FALSE;
	}

	sql = gda_renderer_render_as_sql (GDA_RENDERER (model->priv->query), model->priv->params,
					  0, error);
	if (!sql)
		return FALSE;

	cmd = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	g_free (sql);
        model->priv->data = gda_connection_execute_single_command (cnc, cmd, NULL, error);
	gda_command_free (cmd);

#ifdef debug_NO
	{
		g_print ("GdaDataModelQuery refresh:\n");
		if (model->priv->data) 
			gda_data_model_dump (model, stdout);
		else
			g_print ("\t=> error\n");
	}
#endif

	return model->priv->data ? TRUE : FALSE;
}

#ifdef debug
static void
gda_data_model_query_dump (GdaDataModelQuery *select, guint offset)
{
	TO_IMPLEMENT;
}
#endif


/*
 * GdaDataModel interface implementation
 */
static gint
gda_data_model_query_get_n_rows (GdaDataModel *model)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, 0);

	if (!selmodel->priv->data)
		gda_data_model_query_refresh (selmodel, NULL);
	
	if (selmodel->priv->data)
		return gda_data_model_get_n_rows (selmodel->priv->data);
	else
		return 0;
}

static gint
gda_data_model_query_get_n_columns (GdaDataModel *model)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, 0);

	if (!selmodel->priv->data)
		gda_data_model_query_refresh (selmodel, NULL);
	
	if (selmodel->priv->data)
		return gda_data_model_get_n_columns (selmodel->priv->data);
	else
		return 0;
}

static GdaColumn *
gda_data_model_query_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), NULL);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, NULL);

	if (!selmodel->priv->data)
		gda_data_model_query_refresh (selmodel, NULL);
	
	if (selmodel->priv->data)
		return gda_data_model_describe_column (selmodel->priv->data, col);
	else
		return NULL;
}

static guint
gda_data_model_query_get_access_flags (GdaDataModel *model)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, 0);

	if (!selmodel->priv->data)
		gda_data_model_query_refresh (selmodel, NULL);
	
	if (selmodel->priv->data)
		return gda_data_model_get_access_flags (selmodel->priv->data);
	else
		return 0;
}

static const GdaValue *
gda_data_model_query_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), NULL);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, NULL);

	if (!selmodel->priv->data)
		gda_data_model_query_refresh (selmodel, NULL);
	
	if (selmodel->priv->data)
		return gda_data_model_get_value_at (selmodel->priv->data, col, row);
	else
		return NULL;
}

static GdaDataModelIter *
gda_data_model_query_create_iter (GdaDataModel *model, gint row)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), NULL);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, NULL);

	TO_IMPLEMENT;

	return NULL;
}

static gboolean
gda_data_model_query_iter_at_row (GdaDataModel *model, GdaDataModelIter *iter, gint row)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);

	TO_IMPLEMENT;

	return FALSE;
}

static gboolean
gda_data_model_query_iter_next (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);

	TO_IMPLEMENT;

	return FALSE;
}

static gboolean
gda_data_model_query_iter_prev (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);

	TO_IMPLEMENT;

	return FALSE;
}

static gboolean
gda_data_model_query_set_value_at (GdaDataModel *model, gint col, gint row, const GdaValue *value, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);

	TO_IMPLEMENT;

	return FALSE;
}

static gboolean
gda_data_model_query_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);

	TO_IMPLEMENT;

	return FALSE;
}

static gint
gda_data_model_query_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), -1);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, -1);

	TO_IMPLEMENT;

	return -1;
}

static gint
gda_data_model_query_append_row (GdaDataModel *model, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), -1);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, -1);

	TO_IMPLEMENT;

	return -1;
}

static gboolean
gda_data_model_query_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);

	TO_IMPLEMENT;

	return FALSE;
}
