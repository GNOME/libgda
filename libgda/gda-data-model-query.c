/* GDA library
 * Copyright (C) 2005 - 2006 The GNOME Foundation.
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
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-query.h>
#include <libgda/gda-query-field-field.h>
#include <libgda/gda-dict.h>
#include <libgda/gda-dict-field.h>
#include <libgda/gda-entity.h>
#include <libgda/gda-entity-field.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-renderer.h>
#include <libgda/gda-referer.h>

struct _GdaDataModelQueryPrivate {
	GdaQuery         *queries[4]; /* indexed by SEL_* */
	GdaParameterList *params[4];  /* parameters required to execute @query, indexed by SEL_* */

	GdaDataModel     *data;
	GError           *refresh_error; /* if @data is NULL, then can contain the error */

	gboolean          defer_refresh;
	gboolean          refresh_pending;
	GSList           *columns;
};

/* properties */
enum
{
        PROP_0,
        PROP_SEL_QUERY,
	PROP_INS_QUERY,
	PROP_UPD_QUERY,
	PROP_DEL_QUERY
};

enum 
{
	SEL_QUERY = 0,
	INS_QUERY = 1,
	UPD_QUERY = 2,
	DEL_QUERY = 3
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
static guint                gda_data_model_query_get_attributes_at (GdaDataModel *model, gint col, gint row);

static gboolean             gda_data_model_query_set_value_at    (GdaDataModel *model, gint col, gint row, 
								   const GdaValue *value, GError **error);
static gboolean             gda_data_model_query_set_values      (GdaDataModel *model, gint row, 
								   GList *values, GError **error);
static gint                 gda_data_model_query_append_values   (GdaDataModel *model, const GList *values, GError **error);
static gint                 gda_data_model_query_append_row      (GdaDataModel *model, GError **error);
static gboolean             gda_data_model_query_remove_row      (GdaDataModel *model, gint row, 
								  GError **error);
static void                 gda_data_model_query_send_hint       (GdaDataModel *model, GdaDataModelHint hint, 
								  const GdaValue *hint_value);

static void create_columns (GdaDataModelQuery *model);
static void to_be_destroyed_query_cb (GdaQuery *query, GdaDataModelQuery *model);
static void param_changed_cb (GdaParameterList *paramlist, GdaParameter *param, GdaDataModelQuery *model);

#ifdef GDA_DEBUG
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
	g_object_class_install_property (object_class, PROP_SEL_QUERY,
                                         g_param_spec_pointer ("query", "SELECT query", 
							       "SELECT Query to be executed to populate "
							       "the model with data", 
							       G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_INS_QUERY,
                                         g_param_spec_pointer ("insert_query", "INSERT query", 
							       "INSERT Query to be executed to add data",
							       G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_UPD_QUERY,
                                         g_param_spec_pointer ("update_query", "UPDATE query", 
							       "UPDATE Query to be executed to update data",
							       G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_DEL_QUERY,
                                         g_param_spec_pointer ("delete_query", "DELETE query", 
							       "DELETE Query to be executed to remove data",
							       G_PARAM_READABLE | G_PARAM_WRITABLE));

	/* virtual functions */
	object_class->dispose = gda_data_model_query_dispose;
	object_class->finalize = gda_data_model_query_finalize;
#ifdef GDA_DEBUG
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
	iface->i_get_attributes_at = gda_data_model_query_get_attributes_at;

	iface->i_create_iter = NULL; 
	iface->i_iter_at_row = NULL;
	iface->i_iter_next = NULL;
	iface->i_iter_prev = NULL;

	iface->i_set_value_at = gda_data_model_query_set_value_at;
	iface->i_set_values = gda_data_model_query_set_values;
        iface->i_append_values = gda_data_model_query_append_values;
	iface->i_append_row = gda_data_model_query_append_row;
	iface->i_remove_row = gda_data_model_query_remove_row;
	iface->i_find_row = NULL;
	
	iface->i_set_notify = NULL;
	iface->i_get_notify = NULL;
	iface->i_send_hint = gda_data_model_query_send_hint;
}

static void
gda_data_model_query_init (GdaDataModelQuery *model, GdaDataModelQueryClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (model));
	model->priv = g_new0 (GdaDataModelQueryPrivate, 1);
	model->priv->data = NULL;
	model->priv->refresh_error = NULL;
	model->priv->columns = NULL;

	/* model refreshing is performed as soon as any modification is done */
	model->priv->defer_refresh = FALSE;
	model->priv->refresh_pending = FALSE;
}

static void
gda_data_model_query_dispose (GObject *object)
{
	GdaDataModelQuery *model = (GdaDataModelQuery *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (model));

	/* free memory */
	if (model->priv) {
		gint i;
		if (model->priv->columns) {
			g_slist_foreach (model->priv->columns, (GFunc) g_object_unref, NULL);
			g_slist_free (model->priv->columns);
			model->priv->columns = NULL;
		}

		for (i = SEL_QUERY; i <= DEL_QUERY; i++) {
			if (model->priv->queries[i])
				to_be_destroyed_query_cb (model->priv->queries[i], model);
		
			if (model->priv->params[i]) {
				if (i == SEL_QUERY)
					g_signal_handlers_disconnect_by_func (model->priv->params[i],
									      G_CALLBACK (param_changed_cb), model);
				g_object_unref (model->priv->params[i]);
				model->priv->params[i] = NULL;
			}
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
		if (model->priv->refresh_error)
			g_error_free (model->priv->refresh_error);

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

/*
 * converts "[+-]<num>" to <num> and returns FALSE if @pname is not like
 * "[+-]<num>". <num> is stored in @result, and the +/- signed is stored in @old_val
 * (@old_val is set to TRUE if there is a "-")
 */
static gboolean
param_name_to_int (const gchar *pname, gint *result, gboolean *old_val)
{
	gint sum = 0;
	const gchar *ptr;

	if (!pname || ((*pname != '-') && (*pname != '+')))
		return FALSE;
	
	ptr = pname + 1;
	while (*ptr) {
		if ((*ptr > '9') || (*ptr < '0'))
			return FALSE;
		sum = sum * 10 + *ptr - '0';
		ptr++;
	}
	
	if (result) 
		*result = sum;
	if (old_val)
		*old_val = (*pname == '-') ? TRUE : FALSE;
	
	return TRUE;
}

static void
gda_data_model_query_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	GdaDataModelQuery *model;
	gint qindex = param_id - 1;

	model = GDA_DATA_MODEL_QUERY (object);
	if (model->priv) {
		switch (param_id) {
		case PROP_SEL_QUERY:
		case PROP_INS_QUERY:
		case PROP_UPD_QUERY:
		case PROP_DEL_QUERY:
			if (model->priv->queries[qindex])
				to_be_destroyed_query_cb (model->priv->queries[qindex], model);
			if (model->priv->params[qindex]) {
				g_signal_handlers_disconnect_by_func (model->priv->params[qindex],
								      G_CALLBACK (param_changed_cb), model);
				g_object_unref (model->priv->params[qindex]);
				model->priv->params[qindex] = NULL;
			}

			/* use @query without making a copy of it */
			model->priv->queries[qindex] = (GdaQuery *) g_value_get_pointer (value);
			if (model->priv->queries[qindex]) {
				g_object_ref (model->priv->queries[qindex]);
				g_signal_connect (model->priv->queries[qindex], "to_be_destroyed",
						  G_CALLBACK (to_be_destroyed_query_cb), model);
				
				model->priv->params[qindex] = gda_query_get_parameters_boxed (model->priv->queries[qindex]);

				if (qindex == SEL_QUERY) {
					/* SELECT query */
					g_signal_connect (model->priv->queries[qindex], "changed",
							  G_CALLBACK (query_changed_cb), model);

					if (model->priv->params[qindex])
						g_signal_connect (model->priv->params[qindex], "param_changed",
								  G_CALLBACK (param_changed_cb), model);
				}
				else {
					/* other queries: for all the parameters in the param list, 
					 * if some have a name like "[+-]<num>", then they will be filled with
					 * the value at the new/old <num>th column before being run, or, if the name is
					 * the same as a parameter for the SELECT query, then bind them to that parameter */
					gint num;

					if (model->priv->params [qindex]) {
						GSList *params = model->priv->params [qindex]->parameters;
						while (params) {
							const gchar *pname = gda_object_get_name (GDA_OBJECT (params->data));
							gboolean old_val;
							
							if (param_name_to_int (pname, &num, &old_val)) {
								if (old_val)
									g_object_set_data ((GObject*) params->data, "-num", 
											   GINT_TO_POINTER (num + 1));
								else
									g_object_set_data ((GObject*) params->data, "+num", 
											   GINT_TO_POINTER (num + 1));
								g_object_set_data ((GObject*) params->data, "_num",
										   GINT_TO_POINTER (num + 1));
							}
							else {
								if (pname && model->priv->params [SEL_QUERY]) {
									GdaParameter *bind_to;
									bind_to = gda_parameter_list_find_param 
										(model->priv->params [SEL_QUERY],
										 pname);
									if (bind_to)
										g_object_set_data ((GObject*) params->data, 
												   "_bind", bind_to);
								}
							}
							params = g_slist_next (params);
						}
					}
				}
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
	gint qindex = param_id - 1;

	model = GDA_DATA_MODEL_QUERY (object);
	if (model->priv) {
		switch (param_id) {
		case PROP_SEL_QUERY:
		case PROP_INS_QUERY:
		case PROP_UPD_QUERY:
		case PROP_DEL_QUERY:
			g_value_set_pointer (value, model->priv->queries[qindex]);
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
	gint i;
	gint qindex = -1;
	
	for (i = SEL_QUERY; (i <= DEL_QUERY) && (qindex < 0); i++) 
		if (query == model->priv->queries [i])
			qindex = i;
	g_assert (qindex >= 0);

	g_signal_handlers_disconnect_by_func (query,
					      G_CALLBACK (to_be_destroyed_query_cb), model);
	model->priv->queries [qindex] = NULL;

	if (qindex == SEL_QUERY) 
		g_signal_handlers_disconnect_by_func (query,
						      G_CALLBACK (query_changed_cb), model);
	g_object_unref (query);
}

static void
param_changed_cb (GdaParameterList *paramlist, GdaParameter *param, GdaDataModelQuery *model)
{
	/* first: sync the parameters which are bound */
	if (model->priv->params [SEL_QUERY]) {
		gint i;
		for (i = INS_QUERY; i <= DEL_QUERY; i++) {
			if (model->priv->params [i]) {
				GSList *params = model->priv->params [i]->parameters;
				while (params) {
					GdaParameter *bind_to;
					bind_to = g_object_get_data ((GObject*) params->data, "_bind");
					if (bind_to == param)
						gda_parameter_set_value (GDA_PARAMETER (params->data),
									 gda_parameter_get_value (bind_to));
					params = g_slist_next (params);
				}
			}
		}
	}

	/* second: do a refresh */
	if (gda_parameter_list_is_valid (paramlist))
		gda_data_model_query_refresh (model, NULL);
	else {
		if (model->priv->data) {
			g_object_unref (model->priv->data);
			model->priv->data = NULL;
		}
		gda_data_model_changed ((GdaDataModel *) model);
	}
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

	return model->priv->params [SEL_QUERY];
}

/**
 * gda_data_model_query_refresh
 * @model: a #GdaDataModelQuery data model
 * @error: a place to store errors, or %NULL
 *
 * (Re)-runs the SELECT query to update the contents of @model
 *
 * Returns: TRUE if no error occured
 */
gboolean
gda_data_model_query_refresh (GdaDataModelQuery *model, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	if (model->priv->data) {
		g_object_unref (model->priv->data);
		model->priv->data = NULL;
	}
	if (model->priv->refresh_error) {
		g_error_free (model->priv->refresh_error);
		model->priv->refresh_error = NULL;
	}

	if (! model->priv->queries[SEL_QUERY])
		return TRUE;

	if (!gda_query_is_select_query (model->priv->queries[SEL_QUERY])) {
		g_set_error (&model->priv->refresh_error, 0, 0,
			     _("Query is not a SELECT query"));
		if (error) 
			*error = g_error_copy (model->priv->refresh_error);
		return FALSE;
	}

	model->priv->data = gda_query_execute (model->priv->queries[SEL_QUERY], model->priv->params [SEL_QUERY],
					       TRUE, &model->priv->refresh_error);
	if (!model->priv->data || (model->priv->data == GDA_QUERY_EXEC_FAILED)) {
		model->priv->data = NULL;
		g_assert (model->priv->refresh_error);
		if (error) 
			*error = g_error_copy (model->priv->refresh_error);
		return FALSE;
	}

#ifdef GDA_DEBUG_NO
	{
		g_print ("GdaDataModelQuery refresh:\nSQL= %s\n", sql);
		if (model->priv->data) 
			gda_data_model_dump (model, stdout);
		else
			g_print ("\t=> error\n");
	}
#endif

	gda_data_model_changed ((GdaDataModel *) model);
	return model->priv->data ? TRUE : FALSE;
}

/**
 * gda_data_model_query_set_modification_query
 * @model: a #GdaDataModelQuery data model
 * @query: the SQL code for a query
 * @error: a place to store errors, or %NULL
 *
 * Sets the modification query to be used by @model to actually perform any change
 * to the dataset in the database.
 *
 * The provided query (the @query SQL) must be either a INSERT, UPDATE or DELETE query. It can contain
 * parameters, and the parameters named '[+-]&lt;num&gt;' will be replaced when the query is run:
 * <itemizedlist>
 *   <listitem><para>a parameter named +&lt;num&gt; will take the new value set at the 
 *                   &lt;num&gt;th column in @model</para></listitem>
 *   <listitem><para>a parameter named -&lt;num&gt; will take the old value set at the
 *                   &lt;num&gt;th column in @model</para></listitem>
 * </itemizedlist>
 * Please note that the "+0" and "-0" parameters names are valid and will respectively 
 * take the new and old values of the first column of @model.
 *
 * Examples of queries are: "INSERT INTO orders (customer, creation_date, delivery_before, delivery_date) VALUES (## [:name="Customer" :type="integer"], date('now'), ## [:name="+2" :type="date" :nullok="TRUE"], NULL)", "DELETE FROM orders WHERE id = ## [:name="-0" :type="integer"]" and "UPDATE orders set id=## [:name="+0" :type="integer"], delivery_before=## [:name="+2" :type="date" :nullok="TRUE"], delivery_date=## [:name="+3" :type="date" :nullok="TRUE"] WHERE id=## [:name="-0" :type="integer"]"
 *
 * Returns: TRUE if no error occured.
 */
gboolean
gda_data_model_query_set_modification_query (GdaDataModelQuery *model, const gchar *query, GError **error)
{
	GdaQuery *aq;
	gboolean done = FALSE;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	aq = GDA_QUERY (gda_query_new_from_sql (gda_object_get_dict ((GdaObject *) model), query, NULL));
	if (gda_query_is_insert_query (aq)) {
		g_object_set (model, "insert_query", aq, NULL);
		done = TRUE;
	}
	
	if (!done && gda_query_is_update_query (aq)) {
		g_object_set (model, "update_query", aq, NULL);
		done = TRUE;
	}

	if (!done && gda_query_is_delete_query (aq)) {
		g_object_set (model, "delete_query", aq, NULL);
		done = TRUE;
	}
	    
	g_object_unref (aq);
	if (!done) {
		g_set_error (error, 0, 0,
			     _("Wrong type of query"));
		return FALSE;
	}
	return TRUE;
}


#ifdef GDA_DEBUG
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

	if (!selmodel->priv->data && !selmodel->priv->refresh_error)
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
	
	if (!selmodel->priv->data && !selmodel->priv->refresh_error)
		gda_data_model_query_refresh (selmodel, NULL);
	
	create_columns (selmodel);

	if (selmodel->priv->columns)
		return g_slist_length (selmodel->priv->columns);
	else
		return 0;
}

static void
create_columns (GdaDataModelQuery *model)
{
	if (model->priv->columns)
		return;
	if (! model->priv->queries[SEL_QUERY])
		return;

	if (model->priv->data) {
		/* copy model->priv->data's columns */
		gint i, nb_cols;

		nb_cols = gda_data_model_get_n_columns (model->priv->data);
		for (i = 0; i < nb_cols; i++) {
			GdaColumn *orig, *col;
			
			orig = gda_data_model_describe_column (model->priv->data, i);
			col = gda_column_copy (orig);
			gda_column_set_position (col, i);
			model->priv->columns = g_slist_append (model->priv->columns, col);
		}
	}
	else {
		GSList *fields, *list;
		gboolean allok = TRUE;

		gda_referer_activate (GDA_REFERER (model->priv->queries[SEL_QUERY]));
		if (! gda_referer_is_active (GDA_REFERER (model->priv->queries[SEL_QUERY])))
			return;
		
		fields = gda_entity_get_fields (GDA_ENTITY (model->priv->queries[SEL_QUERY]));
		list = fields;
		while (list && allok) {
			if (gda_entity_field_get_gda_type (GDA_ENTITY_FIELD (list->data)) == GDA_VALUE_TYPE_UNKNOWN)
				allok = FALSE;
			list = g_slist_next (list);
		}
		if (! allok)
			return;

		list = fields;
		while (list) {
			GdaColumn *col;
			GdaEntityField *field = (GdaEntityField *) list->data;
			

			col = gda_column_new ();
			gda_column_set_name (col, gda_object_get_name (GDA_OBJECT (field)));
			gda_column_set_title (col, gda_object_get_name (GDA_OBJECT (field)));
			gda_column_set_gda_type (col, gda_entity_field_get_gda_type (field));
			if (GDA_IS_QUERY_FIELD_FIELD (field)) {
				GdaEntityField *ref_field;

				ref_field = gda_query_field_field_get_ref_field (GDA_QUERY_FIELD_FIELD (field));
				if (GDA_IS_DICT_FIELD (ref_field)) {
					GdaEntity *table;
					const GdaValue *value;
					gda_column_set_defined_size (col, gda_dict_field_get_length ((GdaDictField*) ref_field));
					table = gda_entity_field_get_entity (ref_field);
					gda_column_set_table (col, gda_object_get_name (GDA_OBJECT (table)));
					gda_column_set_scale (col, gda_dict_field_get_scale ((GdaDictField*) ref_field));
					gda_column_set_allow_null (col, gda_dict_field_is_null_allowed ((GdaDictField*) ref_field));
					value = gda_dict_field_get_default_value ((GdaDictField*) ref_field);
					if (value)
						gda_column_set_default_value (col, value);
				}
			}
			model->priv->columns = g_slist_append (model->priv->columns, col);
			
			list = g_slist_next (list);
		}

		g_slist_free (fields);
	}
}

static GdaColumn *
gda_data_model_query_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), NULL);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, NULL);

	if (!selmodel->priv->data  && !selmodel->priv->refresh_error)
		gda_data_model_query_refresh (selmodel, NULL);

	create_columns (selmodel);
	if (selmodel->priv->columns)
		return g_slist_nth_data (selmodel->priv->columns, col);
	else
		return NULL;
}

static guint
gda_data_model_query_get_access_flags (GdaDataModel *model)
{
	GdaDataModelQuery *selmodel;
	guint flags = GDA_DATA_MODEL_ACCESS_RANDOM;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, 0);

	if (!selmodel->priv->data && !selmodel->priv->refresh_error)
		gda_data_model_query_refresh (selmodel, NULL);
	
	if (selmodel->priv->data) {
		gint i;
		for (i = INS_QUERY; i <= DEL_QUERY; i++) {
			if (selmodel->priv->params [i]) {
				/* if all the parameters which are not named _[0-9]* are valid, 
				 * then we grant the corresponding flag */
				gboolean allok = TRUE;
				GSList *params = selmodel->priv->params [i]->parameters;
				while (params && allok) {
					gint num;
					
					num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "_num")) - 1;
					if (num < 0)
						allok = gda_parameter_is_valid ((GdaParameter*)(params->data));
					if (!allok) {
						g_print ("Not OK:\n");
						gda_object_dump (params->data, 10);
					}
					params = g_slist_next (params);
				}
				
				if (allok) {
					switch (i) {
					case INS_QUERY:
						flags |= GDA_DATA_MODEL_ACCESS_INSERT;
						/* g_print ("INS flag\n"); */
						break;
					case UPD_QUERY:
						flags |= GDA_DATA_MODEL_ACCESS_UPDATE;
						/* g_print ("UPD flag\n"); */
						break;
					case DEL_QUERY:
						flags |= GDA_DATA_MODEL_ACCESS_DELETE;
						/* g_print ("DEL flag\n"); */
						break;
					default:
						g_assert_not_reached ();
					}
				}
			}
		}
	}

	return flags;
}

static const GdaValue *
gda_data_model_query_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), NULL);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, NULL);

	if (!selmodel->priv->data && !selmodel->priv->refresh_error)
		gda_data_model_query_refresh (selmodel, NULL);
	
	if (selmodel->priv->data)
		return gda_data_model_get_value_at (selmodel->priv->data, col, row);
	else
		return NULL;
}

static guint
gda_data_model_query_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	guint flags = 0;
	GdaDataModelQuery *selmodel;
	gboolean used = FALSE;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = (GdaDataModelQuery *) model;
	g_return_val_if_fail (selmodel->priv, 0);
	
	if (selmodel->priv->data)
		flags = gda_data_model_get_attributes_at (selmodel->priv->data, col, row);

	if ((row >= 0) && selmodel->priv->queries[UPD_QUERY] && selmodel->priv->params[UPD_QUERY]) {
		GSList *params = selmodel->priv->params[UPD_QUERY]->parameters;
		while (params && !used) {
			if (GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1 == col)
				used = TRUE;
			params = params->next;
		}
	}

	if ((row < 0) && selmodel->priv->queries[INS_QUERY] && selmodel->priv->params[INS_QUERY]) {
		GSList *params = selmodel->priv->params[INS_QUERY]->parameters;
		while (params && !used) {
			if (GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1 == col)
				used = TRUE;
			params = params->next;
		}
	}
	
	if (!used)
		flags |= GDA_VALUE_ATTR_NO_MODIF;

	return flags;
}

static gboolean
run_modif_query (GdaDataModelQuery *selmodel, gint query_type, GError **error)
{
	gboolean retval = FALSE;
	GdaDataModel *model;
	model = gda_query_execute (selmodel->priv->queries[query_type], 
				   selmodel->priv->params [query_type], TRUE, error);
	if (model != GDA_QUERY_EXEC_FAILED) {
		retval = TRUE;
		if (model)
			g_object_unref (model);
	}

	if (retval && !selmodel->priv->defer_refresh)
		/* do a refresh */
		gda_data_model_query_refresh (selmodel, NULL);
	else
		/* the refresh is delayed */
		selmodel->priv->refresh_pending = TRUE;

	return retval;
}

static gboolean
gda_data_model_query_set_value_at (GdaDataModel *model, gint col, gint row, const GdaValue *value, GError **error)
{
	GdaDataModelQuery *selmodel;
	GdaParameterList *paramlist;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);

	/* make sure there is a UPDATE query */
	if (!selmodel->priv->queries[UPD_QUERY]) {
		g_set_error (error, 0, 0,
			     _("No UPDATE query specified, can't update row"));
		return FALSE;
	}

	/* set the values of the parameters in the paramlist if necessary */
	paramlist = selmodel->priv->params[UPD_QUERY];
	if (paramlist && paramlist->parameters) {
		GSList *params = paramlist->parameters;
		while (params) {
			gint num;

			num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1;
			if (num >= 0) {
				/* new values */
				if (num == col)
					gda_parameter_set_value (GDA_PARAMETER (params->data), value);
				else
					gda_parameter_set_value (GDA_PARAMETER (params->data), NULL);
			}
			else {
				num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "-num")) - 1;	
				if (num >= 0)
					/* old values */
					gda_parameter_set_value (GDA_PARAMETER (params->data),
								 gda_data_model_get_value_at ((GdaDataModel*) selmodel, 
											      num, row));
			}
			params = g_slist_next (params);
		}
	}

	/* render the SQL and run it */
	return run_modif_query (selmodel, UPD_QUERY, error);
}

static gboolean
gda_data_model_query_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	GdaDataModelQuery *selmodel;
	GdaParameterList *paramlist;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);

	/* make sure there is a UPDATE query */
	if (!selmodel->priv->queries[UPD_QUERY]) {
		g_set_error (error, 0, 0,
			     _("No UPDATE query specified, can't update row"));
		return FALSE;
	}

	/* set the values of the parameters in the paramlist if necessary */
	paramlist = selmodel->priv->params[UPD_QUERY];
	if (paramlist && paramlist->parameters) {
		GSList *params = paramlist->parameters;
		while (params) {
			gint num;

			num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1;
			if (num >= 0) {
				/* new values */
				GdaValue *value;
				value = g_list_nth_data ((GList *) values, num);
				if (value)
					gda_parameter_set_value (GDA_PARAMETER (params->data), value);
				else
					gda_parameter_set_value (GDA_PARAMETER (params->data), NULL);
			}
			else {
				num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "-num")) - 1;
				if (num >= 0)
					/* old values */
					gda_parameter_set_value (GDA_PARAMETER (params->data),
								 gda_data_model_get_value_at ((GdaDataModel*) selmodel, 
											      num, row));
			}
			params = g_slist_next (params);
		}
	}

	/* render the SQL and run it */
	return run_modif_query (selmodel, UPD_QUERY, error);
}

static gint
gda_data_model_query_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	GdaDataModelQuery *selmodel;
	GdaParameterList *paramlist;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), -1);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, -1);

	/* make sure there is a INSERT query */
	if (!selmodel->priv->queries[INS_QUERY]) {
		g_set_error (error, 0, 0,
			     _("No INSERT query specified, can't insert row"));
		return -1;
	}

	/* set the values of the parameters in the paramlist if necessary */
	paramlist = selmodel->priv->params[INS_QUERY];
	if (paramlist && paramlist->parameters) {
		GSList *params = paramlist->parameters;
		while (params) {
			gint num;

			num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1;
			if (num >= 0) {
				/* new values only */
				GdaValue *value;
				value = g_list_nth_data ((GList *) values, num);
				gda_parameter_set_value (GDA_PARAMETER (params->data), value);
			}
			params = g_slist_next (params);
		}
	}

	/* render the SQL and run it */
	retval = run_modif_query (selmodel, INS_QUERY, error);

	if (retval)
		return 0;
	else
		return -1;
}

static gint
gda_data_model_query_append_row (GdaDataModel *model, GError **error)
{
	GdaDataModelQuery *selmodel;
	GdaParameterList *paramlist;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), -1);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, -1);

	/* make sure there is a INSERT query */
	if (!selmodel->priv->queries[INS_QUERY]) {
		g_set_error (error, 0, 0,
			     _("No INSERT query specified, can't insert row"));
		return -1;
	}

	/* set the values of the parameters in the paramlist if necessary */
	paramlist = selmodel->priv->params[INS_QUERY];
	if (paramlist && paramlist->parameters) {
		GSList *params = paramlist->parameters;
		while (params) {
			gint num;

			num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1;

			if (num >= 0) 
				/* new values only */
				gda_parameter_set_value (GDA_PARAMETER (params->data), NULL);
			params = g_slist_next (params);
		}
	}

	/* render the SQL and run it */
	retval = run_modif_query (selmodel, INS_QUERY, error);

	if (retval)
		return 0;
	else
		return -1;
}

static gboolean
gda_data_model_query_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaDataModelQuery *selmodel;
	GdaParameterList *paramlist;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);
	
	/* make sure there is a REMOVE query */
	if (!selmodel->priv->queries[DEL_QUERY]) {
		g_set_error (error, 0, 0,
			     _("No DELETE query specified, can't delete row"));
		return FALSE;
	}

	/* set the values of the parameters in the paramlist if necessary */
	paramlist = selmodel->priv->params[DEL_QUERY];
	if (paramlist && paramlist->parameters) {
		GSList *params = paramlist->parameters;
		while (params) {
			gint num;

			num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "-num")) - 1;
			if (num >= 0) 
				/* old values only */
				gda_parameter_set_value (GDA_PARAMETER (params->data),
							 gda_data_model_get_value_at ((GdaDataModel*) selmodel, num, row));
			params = g_slist_next (params);
		}
	}

	/* render the SQL and run it */
	return run_modif_query (selmodel, DEL_QUERY, error);
}

static void
gda_data_model_query_send_hint (GdaDataModel *model, GdaDataModelHint hint, const GdaValue *hint_value)
{
	GdaDataModelQuery *selmodel;
	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (model));
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_if_fail (selmodel->priv);

	if (hint == GDA_DATA_MODEL_HINT_REFRESH)
		gda_data_model_query_refresh (selmodel, NULL);
	else {
		if (hint == GDA_DATA_MODEL_HINT_START_BATCH_UPDATE)
			selmodel->priv->defer_refresh = TRUE;
		else {
			if (hint == GDA_DATA_MODEL_HINT_END_BATCH_UPDATE) {
				selmodel->priv->defer_refresh = FALSE;
				if (selmodel->priv->refresh_pending)
					gda_data_model_query_refresh (selmodel, NULL);
			}
		}
	}
}
