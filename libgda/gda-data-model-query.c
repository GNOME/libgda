/* GDA library
 * Copyright (C) 2005 - 2007 The GNOME Foundation.
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

#include <string.h>
#include <glib/gi18n-lib.h>

/* define GDA_DEBUG so we can use gda_object_dump() here: */
#define GDA_DEBUG 1

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

	gboolean          multiple_updates;
	gboolean          needs_refresh;
	GSList           *columns;

	gboolean          transaction_allowed;
	gboolean          transaction_started; /* true if we have started a transaction here */
	gboolean          transaction_needs_commit;
	guint             svp_id;   /* counter for savepoints */
	gchar            *svp_name; /* non NULL if we have created a savepoint here */
	/* note: transaction_started and svp_name are mutually exclusive */

	gboolean          emit_reset;
};

/* properties */
enum
{
        PROP_0,
        PROP_SEL_QUERY,
	PROP_INS_QUERY,
	PROP_UPD_QUERY,
	PROP_DEL_QUERY,
	PROP_USE_TRANSACTION
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
static GdaDataModelAccessFlags gda_data_model_query_get_access_flags(GdaDataModel *model);
static const GValue        *gda_data_model_query_get_value_at    (GdaDataModel *model, gint col, gint row);
static GdaValueAttribute    gda_data_model_query_get_attributes_at (GdaDataModel *model, gint col, gint row);

static GdaDataModelIter    *gda_data_model_query_create_iter     (GdaDataModel *model);

static gboolean             gda_data_model_query_set_value_at    (GdaDataModel *model, gint col, gint row, 
								   const GValue *value, GError **error);
static gboolean             gda_data_model_query_set_values      (GdaDataModel *model, gint row, 
								   GList *values, GError **error);
static gint                 gda_data_model_query_append_values   (GdaDataModel *model, const GList *values, GError **error);
static gint                 gda_data_model_query_append_row      (GdaDataModel *model, GError **error);
static gboolean             gda_data_model_query_remove_row      (GdaDataModel *model, gint row, 
								  GError **error);
static void                 gda_data_model_query_send_hint       (GdaDataModel *model, GdaDataModelHint hint, 
								  const GValue *hint_value);

static void create_columns (GdaDataModelQuery *model);
static void to_be_destroyed_query_cb (GdaQuery *query, GdaDataModelQuery *model);
static void param_changed_cb (GdaParameterList *paramlist, GdaParameter *param, GdaDataModelQuery *model);

static void opt_start_transaction_or_svp (GdaDataModelQuery *selmodel);
static void opt_end_transaction_or_svp (GdaDataModelQuery *selmodel);

#ifdef GDA_DEBUG
static void gda_data_model_query_dump (GdaDataModelQuery *select, guint offset);
#endif

static GObjectClass *parent_class = NULL;

/* module error */
GQuark gda_data_model_query_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_data_model_query_error");
        return quark;
}


/**
 * gda_data_model_query_get_type
 *
 * Returns: the #GType of GdaDataModelQuery.
 */
GType
gda_data_model_query_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
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
                                         g_param_spec_object ("query", "SELECT query", 
							       "SELECT Query to be executed to populate "
							       "the model with data", 
                                                               GDA_TYPE_QUERY,
							       G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_INS_QUERY,
                                         g_param_spec_object ("insert_query", "INSERT query", 
							       "INSERT Query to be executed to add data",
                                                               GDA_TYPE_QUERY,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_UPD_QUERY,
                                         g_param_spec_object ("update_query", "UPDATE query", 
							       "UPDATE Query to be executed to update data",
                                                               GDA_TYPE_QUERY,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_DEL_QUERY,
                                         g_param_spec_object ("delete_query", "DELETE query", 
							       "DELETE Query to be executed to remove data",
                                                               GDA_TYPE_QUERY,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_object_class_install_property (object_class, PROP_USE_TRANSACTION,
                                         g_param_spec_boolean ("use_transaction", "Use transaction", 
							       "Run modification queries within a transaction",
                                                               FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

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

	iface->i_create_iter = gda_data_model_query_create_iter; 
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
	model->priv->multiple_updates = FALSE;
	model->priv->needs_refresh = FALSE;

	model->priv->transaction_allowed = FALSE;
	model->priv->transaction_started = FALSE;
	model->priv->transaction_needs_commit = FALSE;
	model->priv->svp_id = 0;
	model->priv->svp_name = NULL;

	model->priv->emit_reset = FALSE;
}

static void
gda_data_model_query_dispose (GObject *object)
{
	GdaDataModelQuery *model = (GdaDataModelQuery *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (model));

	/* free memory */
	if (model->priv) {
		gint i;

		if (model->priv->transaction_started || model->priv->svp_name)
			opt_end_transaction_or_svp (model);

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
check_param_type (GdaParameter *param, GType type)
{
	if ((type != 0) && (type !=  gda_parameter_get_g_type (param))) {
		g_warning (_("Type of parameter '%s' is '%s' when it should be '%s', GdaDataModelQuery object will now work correctly"),
			   gda_object_get_name (GDA_OBJECT (param)), 
			   g_type_name (gda_parameter_get_g_type (param)),
			   g_type_name (type));
	}
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
			model->priv->queries[qindex] = (GdaQuery *) g_value_get_object (value);
			if (model->priv->queries[qindex]) {
				g_object_ref (model->priv->queries[qindex]);
				g_signal_connect (model->priv->queries[qindex], "to_be_destroyed",
						  G_CALLBACK (to_be_destroyed_query_cb), model);
				
				model->priv->params[qindex] = gda_query_get_parameter_list (model->priv->queries[qindex]);
				if (qindex == SEL_QUERY) {
					/* SELECT query */
					GSList *targets, *tlist;

					targets = gda_query_get_targets (model->priv->queries[qindex]);
					for (tlist = targets; tlist; tlist = tlist->next)
						g_slist_free (gda_query_expand_all_field (model->priv->queries[qindex],
											  GDA_QUERY_TARGET (tlist->data)));
					g_slist_free (targets);

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
							GdaParameter *param = GDA_PARAMETER (params->data);
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
								GdaColumn *col;
								col = gda_data_model_describe_column ((GdaDataModel *) model,
												      num);
								if (col) {
									check_param_type (param, gda_column_get_g_type (col));
									gda_parameter_set_not_null 
										((GdaParameter *)(params->data),
										 !gda_column_get_allow_null (col));
									if (gda_column_get_auto_increment (col) ||
									    gda_column_get_default_value (col))
										gda_parameter_set_exists_default_value
										 ((GdaParameter *)(params->data), TRUE);
									gda_object_set_id (GDA_OBJECT (param),
											   gda_column_get_name (col));
								}
							}
							else {
								if (pname && model->priv->params [SEL_QUERY]) {
									GdaParameter *bind_to;
									bind_to = gda_parameter_list_find_param 
										(model->priv->params [SEL_QUERY],
										 pname);
									if (bind_to) {
										check_param_type (param,
												  gda_parameter_get_g_type (bind_to));
										g_object_set_data ((GObject*) params->data, 
												   "_bind", bind_to);
									}
									else
										g_warning (_("Could not find a parameter named "
											     "'%s' among the SELECT query's "
											     "parameters, the specified "
											     "modification query will not be executable"),
											   pname);
								}
							}
							params = g_slist_next (params);
						}
					}
				}
			}
			break;
		case PROP_USE_TRANSACTION:
			model->priv->transaction_allowed = g_value_get_boolean (value);
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
			g_value_set_object (value, G_OBJECT (model->priv->queries[qindex]));
			break;
		case PROP_USE_TRANSACTION:
			g_value_set_boolean (value, model->priv->transaction_allowed);
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
		gda_data_model_reset ((GdaDataModel *) model);
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

	model = g_object_new (GDA_TYPE_DATA_MODEL_QUERY, 
			      "dict", gda_object_get_dict (GDA_OBJECT (query)),
			      "query", query, NULL);

	return GDA_DATA_MODEL (model);
}

/**
 * gda_data_model_query_get_parameter_list
 * @model: a #GdaDataModelQuery data model
 *
 * If some parameters are required to execute the SELECT query used in the @model data model, then
 * returns the #GdaParameterList used; otherwise does nothing and returns %NULL.
 *
 * Returns: a #GdaParameterList object, or %NULL
 */
GdaParameterList *
gda_data_model_query_get_parameter_list (GdaDataModelQuery *model)
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
 * Returns: TRUE if no error occurred
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

	model->priv->data = (GdaDataModel *) gda_query_execute (model->priv->queries[SEL_QUERY], 
								model->priv->params [SEL_QUERY],
								TRUE, &model->priv->refresh_error);
	if (!model->priv->data || !GDA_IS_DATA_MODEL (model->priv->data)) {
		model->priv->data = NULL;
		g_assert (model->priv->refresh_error);
		if (error) 
			*error = g_error_copy (model->priv->refresh_error);
		return FALSE;
	}

	gda_data_model_reset ((GdaDataModel *) model);
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
 * Examples of queries are: "INSERT INTO orders (customer, creation_date, delivery_before, delivery_date) VALUES (## / *name:'Customer' type:integer* /, date('now'), ## / *name:"+2" type:date nullok:TRUE * /, NULL)", "DELETE FROM orders WHERE id = ## / *name:"-0" type:integer* /" and "UPDATE orders set id=## / *name:"+0" type:integer* /, delivery_before=## / *name:"+2" type:date nullok:TRUE* /, delivery_date=## / *name:"+3" type:date nullok:TRUE* / WHERE id=## / *name:"-0" type:integer* /"
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_model_query_set_modification_query (GdaDataModelQuery *model, const gchar *query, GError **error)
{
	GdaQuery *aq;
	gboolean done = FALSE;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	aq = gda_query_new_from_sql (gda_object_get_dict ((GdaObject *) model), query, error);
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
	gchar *str;

	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (select));
	
        /* string for the offset */
        str = g_new (gchar, offset+1);
	memset (str, ' ', offset);
        str[offset] = 0;
	g_print ("%s" D_COL_H1 "GdaDataModelQuery %p" D_COL_NOR "\n", str, select);

	if (select->priv->queries [SEL_QUERY])
		gda_object_dump (GDA_OBJECT (select->priv->queries [SEL_QUERY]), offset + 5);

	if (select->priv->params [SEL_QUERY]) 
		gda_object_dump (GDA_OBJECT (select->priv->params [SEL_QUERY]), offset+5);
		
	g_free (str);
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
	GSList *fields;

	if (model->priv->columns)
		return;
	if (! model->priv->queries[SEL_QUERY])
		return;

	gda_referer_activate (GDA_REFERER (model->priv->queries[SEL_QUERY]));
	fields = gda_entity_get_fields (GDA_ENTITY (model->priv->queries[SEL_QUERY]));
	if (gda_referer_is_active (GDA_REFERER (model->priv->queries[SEL_QUERY])) && fields) {
		GSList *list;
		gboolean allok = TRUE;
		
		for (list = fields; list && allok; list = list->next) {
			if (gda_entity_field_get_g_type (GDA_ENTITY_FIELD (list->data)) == G_TYPE_INVALID) {
				allok = FALSE;
				g_warning (_("Can't determine the GType for field '%s', please fill a bug report"), 
					   gda_object_get_name (GDA_OBJECT (list->data)));
			}
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
			gda_column_set_g_type (col, gda_entity_field_get_g_type (field));
			if (GDA_IS_QUERY_FIELD_FIELD (field)) {
				GdaEntityField *ref_field;

				ref_field = gda_query_field_field_get_ref_field (GDA_QUERY_FIELD_FIELD (field));
				if (GDA_IS_DICT_FIELD (ref_field)) {
					GdaEntity *table;
					const GValue *value;
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
	else {
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
			/* Can't compute the list of columns because the SELECT query is not active 
			 * and the model does not contain any data;
			 * next time we emit a "reset" signal */
			model->priv->emit_reset = TRUE;
		}
	}

	if (model->priv->columns && model->priv->emit_reset) {
		model->priv->emit_reset = FALSE;
		gda_data_model_reset (GDA_DATA_MODEL (model));
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

static GdaDataModelAccessFlags
gda_data_model_query_get_access_flags (GdaDataModel *model)
{
	GdaDataModelQuery *selmodel;
	GdaDataModelAccessFlags flags = GDA_DATA_MODEL_ACCESS_RANDOM;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, 0);

	if (!selmodel->priv->data && !selmodel->priv->refresh_error)
		gda_data_model_query_refresh (selmodel, NULL);
	
	if (selmodel->priv->data) {
		gint i;
		for (i = INS_QUERY; i <= DEL_QUERY; i++) {
			gboolean allok = TRUE;

			if (selmodel->priv->params [i]) {
				/* if all the parameters which are not named _[0-9]* are valid, 
				 * then we grant the corresponding flag */
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
				
			}
			else {
				if (!selmodel->priv->queries [i])
					allok = FALSE;
			}

			if (allok && selmodel->priv->params [i]) {
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

	return flags;
}

static const GValue *
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

static GdaValueAttribute
gda_data_model_query_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	GdaValueAttribute flags = 0;
	GdaDataModelQuery *selmodel;
	GdaParameter *p_used = NULL;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = (GdaDataModelQuery *) model;
	g_return_val_if_fail (selmodel->priv, 0);
	
	if (selmodel->priv->data)
		flags = gda_data_model_get_attributes_at (selmodel->priv->data, col, row);

	if ((row >= 0) && selmodel->priv->queries[UPD_QUERY] && selmodel->priv->params[UPD_QUERY]) {
		GSList *params = selmodel->priv->params[UPD_QUERY]->parameters;
		while (params && !p_used) {
			if (GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1 == col)
				p_used = (GdaParameter *) params->data;
			params = params->next;
		}
	}

	if ((row < 0) && selmodel->priv->queries[INS_QUERY] && selmodel->priv->params[INS_QUERY]) {
		GSList *params = selmodel->priv->params[INS_QUERY]->parameters;
		while (params && !p_used) {
			if (GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1 == col)
				p_used = (GdaParameter *) params->data;
			params = params->next;
		}
	}

	if (!p_used)
		flags |= GDA_VALUE_ATTR_NO_MODIF;
	else {
		flags &= ~GDA_VALUE_ATTR_NO_MODIF;
		flags &= ~GDA_VALUE_ATTR_CAN_BE_NULL;
		flags &= ~GDA_VALUE_ATTR_CAN_BE_DEFAULT;
		if (! gda_parameter_get_not_null (p_used))
			flags |= GDA_VALUE_ATTR_CAN_BE_NULL;
		if (gda_parameter_get_default_value (p_used) ||
		    gda_parameter_get_exists_default_value (p_used))
			flags |= GDA_VALUE_ATTR_CAN_BE_DEFAULT;
	}

	/*g_print ("%d,%d: %d\n", col, row, flags);*/

	return flags;
}

static GdaDataModelIter *
gda_data_model_query_create_iter (GdaDataModel *model)
{
	GdaDataModelIter *iter;

	iter = (GdaDataModelIter *) g_object_new (GDA_TYPE_DATA_MODEL_ITER, 
						  "dict", gda_object_get_dict (GDA_OBJECT (model)), 
						  "data_model", model, NULL);
	/* set the "entry_plugin" for all the parameters depending on the SELECT query field */
	if (gda_query_is_select_query (GDA_DATA_MODEL_QUERY (model)->priv->queries[SEL_QUERY])) {
		GSList *list, *fields;
		GSList *params;

		fields = gda_entity_get_fields (GDA_ENTITY (GDA_DATA_MODEL_QUERY (model)->priv->queries[SEL_QUERY]));
		params = GDA_PARAMETER_LIST (iter)->parameters;

		for (list = fields; list && params; list = list->next, params = params->next) {
			GdaEntityField *field = (GdaEntityField *) list->data;
			if (GDA_IS_QUERY_FIELD_FIELD (field)) {
				gchar *plugin;

			        g_object_get (G_OBJECT (field), "entry_plugin", &plugin, NULL);
				if (plugin) {
					g_object_set (G_OBJECT (params->data), "entry_plugin", plugin, NULL);
					g_free (plugin);
				}
			}
		}
		g_slist_free (fields);
	}

	return iter;
}

static gchar *try_add_savepoint (GdaDataModelQuery *selmodel);
static void   try_remove_savepoint (GdaDataModelQuery *selmodel, const gchar *svp_name);

static gchar *
try_add_savepoint (GdaDataModelQuery *selmodel)
{
	GdaConnection *cnc;
	gchar *svp_name = NULL;
	cnc = gda_dict_get_connection (gda_object_get_dict (GDA_OBJECT (selmodel->priv->queries[SEL_QUERY])));
	if (cnc && gda_connection_supports_feature (cnc, GDA_CONNECTION_FEATURE_SAVEPOINTS)) {
		gchar *name;
		
		name = g_strdup_printf ("_gda_data_model_query_svp_%p_%d", selmodel, selmodel->priv->svp_id++);
		if (gda_connection_add_savepoint (cnc, name, NULL))
			svp_name = name;
		else
			g_free (name);
	}
	return svp_name;
}

static void
try_remove_savepoint (GdaDataModelQuery *selmodel, const gchar *svp_name)
{
	GdaConnection *cnc;

	if (!svp_name)
		return;
	cnc = gda_dict_get_connection (gda_object_get_dict (GDA_OBJECT (selmodel->priv->queries[SEL_QUERY])));
	if (cnc && gda_connection_supports_feature (cnc, GDA_CONNECTION_FEATURE_SAVEPOINTS_REMOVE))
		gda_connection_delete_savepoint (cnc, svp_name, NULL);
}

static gboolean
run_modify_query (GdaDataModelQuery *selmodel, gint query_type, GError **error)
{
	gboolean modify_done = FALSE;
	GdaParameterList *plist;
	gchar *svp_name = NULL;

	/* try to add a savepoint if we are not doing multiple updates */
	if (!selmodel->priv->multiple_updates)
		svp_name = try_add_savepoint (selmodel);

	plist = (GdaParameterList *) gda_query_execute (selmodel->priv->queries[query_type], 
							selmodel->priv->params [query_type], TRUE, error);
	if (plist) {
		modify_done = TRUE;
		g_object_unref (plist);
	}

	if (modify_done) {
		/* modif query executed without error */
		if (!selmodel->priv->multiple_updates)
			gda_data_model_query_refresh (selmodel, NULL);
		else 
			selmodel->priv->needs_refresh = TRUE;
	}
	else {
		/* modif query did not execute correctly */
		if (!selmodel->priv->multiple_updates)
			/* nothing to do */;
		else
			selmodel->priv->transaction_needs_commit = FALSE;
	}

	/* try to remove the savepoint added above */
	if (svp_name) {
		try_remove_savepoint (selmodel, svp_name);
		g_free (svp_name);
	}

#ifdef REMOVE
	if (modify_done && !selmodel->priv->multiple_updates)
		/* modifications were successfull => refresh needed */
		gda_data_model_query_refresh (selmodel, NULL);
	else
		/* the refresh is delayed */
		selmodel->priv->needs_refresh = TRUE;
#endif

	return modify_done;
}

static gboolean
gda_data_model_query_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *value, GError **error)
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
	return run_modify_query (selmodel, UPD_QUERY, error);
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
				GValue *value;
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
	return run_modify_query (selmodel, UPD_QUERY, error);
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
				GValue *value;
				value = g_list_nth_data ((GList *) values, num);
				if (value)
					gda_parameter_set_value (GDA_PARAMETER (params->data), value);
				else
					g_object_set (G_OBJECT (params->data), "use-default-value", TRUE, NULL);
			}
			params = g_slist_next (params);
		}
	}

	/* render the SQL and run it */
	retval = run_modify_query (selmodel, INS_QUERY, error);

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
	retval = run_modify_query (selmodel, INS_QUERY, error);

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
	return run_modify_query (selmodel, DEL_QUERY, error);
}

static void
opt_start_transaction_or_svp (GdaDataModelQuery *selmodel)
{
	GdaConnection *cnc;

	/* if the data model is not allowed to start transactions, then nothing to do */
	if (!selmodel->priv->transaction_allowed)
		return;

	/* if we already have started a transaction, or added a savepoint, then nothing to do */
	if (selmodel->priv->transaction_started ||
	    selmodel->priv->svp_name)
		return;

	/* re-init internal status */
	selmodel->priv->transaction_needs_commit = FALSE;

	/* start a transaction is no transaction has yet been started */
	cnc = gda_dict_get_connection (gda_object_get_dict (GDA_OBJECT (selmodel->priv->queries[SEL_QUERY])));
	if (cnc && gda_connection_supports_feature (cnc, GDA_CONNECTION_FEATURE_TRANSACTIONS)) {
		GdaTransactionStatus *tstat;
		
		tstat = gda_connection_get_transaction_status (cnc);
		if (tstat) {
			/* a transaction already exists, try to use a savepoint */
			if (gda_connection_supports_feature (cnc, GDA_CONNECTION_FEATURE_SAVEPOINTS)) {
				gchar *name;
				
				name = g_strdup_printf ("_gda_data_model_query_svp_%p_%d", selmodel, selmodel->priv->svp_id++);
				if (gda_connection_add_savepoint (cnc, name, NULL))
					selmodel->priv->svp_name = name;
				else
					g_free (name);
			}
		}
		else 
			/* start a transaction */
			selmodel->priv->transaction_started = gda_connection_begin_transaction (cnc, NULL,
							      GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL);
	}

	/* init internal status */
	selmodel->priv->transaction_needs_commit = TRUE;

	if (selmodel->priv->transaction_started)
		g_print ("GdaDataModelQuery %p: started transaction\n", selmodel);
	if (selmodel->priv->svp_name)
		g_print ("GdaDataModelQuery %p: added savepoint %s\n", selmodel, selmodel->priv->svp_name);
}

static void
opt_end_transaction_or_svp (GdaDataModelQuery *selmodel)
{
	GdaConnection *cnc;

	if (!selmodel->priv->transaction_started && !selmodel->priv->svp_name)
		return;

	g_print ("GdaDataModelQuery %p (END1): %s\n", selmodel, selmodel->priv->needs_refresh ? "needs refresh" : "no refresh");
	cnc = gda_dict_get_connection (gda_object_get_dict (GDA_OBJECT (selmodel->priv->queries[SEL_QUERY])));
	if (selmodel->priv->transaction_started) {
		g_assert (!selmodel->priv->svp_name);
		if (selmodel->priv->transaction_needs_commit) {
			/* try to commit transaction */
			if (!gda_connection_commit_transaction (cnc, NULL, NULL))
				selmodel->priv->needs_refresh = TRUE;
		}
		else {
			/* rollback transaction */
			selmodel->priv->needs_refresh = gda_connection_rollback_transaction (cnc, NULL, NULL) ? FALSE : TRUE;
		}
		selmodel->priv->transaction_started = FALSE;
	}
	else {
		if (!selmodel->priv->transaction_needs_commit) {
			selmodel->priv->needs_refresh = gda_connection_rollback_savepoint (cnc, selmodel->priv->svp_name, NULL) ?
				FALSE : TRUE;
		}
		else
			if (gda_connection_supports_feature (cnc, GDA_CONNECTION_FEATURE_SAVEPOINTS_REMOVE))
				if (!gda_connection_delete_savepoint (cnc, selmodel->priv->svp_name, NULL))
					selmodel->priv->needs_refresh = TRUE;
		g_free (selmodel->priv->svp_name);
		selmodel->priv->svp_name = NULL;
	}
	g_print ("GdaDataModelQuery %p (END2): %s\n", selmodel, selmodel->priv->needs_refresh ? "needs refresh" : "no refresh");
}



static void
gda_data_model_query_send_hint (GdaDataModel *model, GdaDataModelHint hint, const GValue *hint_value)
{
	GdaDataModelQuery *selmodel;
	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (model));
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_if_fail (selmodel->priv);

	if (hint == GDA_DATA_MODEL_HINT_REFRESH)
		gda_data_model_query_refresh (selmodel, NULL);
	else {
		if (hint == GDA_DATA_MODEL_HINT_START_BATCH_UPDATE) {
			opt_start_transaction_or_svp (selmodel);
			selmodel->priv->multiple_updates = TRUE;
		}
		else {
			if (hint == GDA_DATA_MODEL_HINT_END_BATCH_UPDATE) {
				selmodel->priv->multiple_updates = FALSE;
				opt_end_transaction_or_svp (selmodel);
					
				if (selmodel->priv->needs_refresh)
					gda_data_model_query_refresh (selmodel, NULL);
			}
		}
	}
}

/*
 * Computation of INSERT, DELETE and UPDATE queries
 */

static GdaQueryTarget *auto_compute_assert_modify_target (GdaDataModelQuery *model, const gchar *target, GError **error);
static GSList   *auto_compute_make_cond_query_fields (GdaDataModelQuery *model, GdaQueryTarget *modify_target, 
						      gboolean use_all_if_no_pk, GError **error);
static GSList   *auto_compute_make_mod_query_fields (GdaDataModelQuery *model, GdaQueryTarget *modify_target,
						     GError **error);
static void      auto_compute_add_mod_fields_to_query (GdaDataModelQuery *model, GdaQueryTarget *modify_target,
						       GSList *mod_query_fields, GdaQuery *query);
static void      auto_compute_add_where_cond_to_query (GdaDataModelQuery *model, GSList *mod_query_fields, 
						       GdaQuery *query);
/**
 * gda_data_model_query_compute_modification_queries
 * @model: a GdaDataModelQuery object
 * @target: the target table to modify, or %NULL
 * @options: options to specify how the queries must be built in some special cases
 * @error: a place to store errors or %NULL
 *
 * Try to compute the INSERT, DELETE and UPDATE queries; any previous modification query
 * will be discarded.
 *
 * If specified, the table which will be updated is the one represented by the @target.
 *
 * If @target is %NULL, then an error will be returned if @model's SELECT query has more than
 * one target.
 *
 * Returns: TRUE if the INSERT, DELETE and UPDATE queries have been computed.
 */
gboolean
gda_data_model_query_compute_modification_queries (GdaDataModelQuery *model, const gchar *target, 
						   GdaDataModelQueryOptions options, GError **error)
{
	GSList *cond_query_fields; /* list of GdaQueryField objects to be used in WHERE condition */
	GSList *mod_query_fields;  /* list of GdaQueryField objects to be modified in INSERT and UPDATE */
	GdaQuery *query;
	GdaQueryTarget *modify_target;

	if (!model->priv->queries[SEL_QUERY]) {
		g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR,
			     GDA_DATA_MODEL_QUERY_COMPUTE_MODIF_QUERIES_ERROR,
			     _("No SELECT query specified"));
		return FALSE;
	}

	if (! gda_referer_activate (GDA_REFERER (model->priv->queries[SEL_QUERY]))) {
		g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR,
			     GDA_DATA_MODEL_QUERY_COMPUTE_MODIF_QUERIES_ERROR,
			     _("SELECT query references objects not described in dictionary"));
		return FALSE;
	}

	modify_target = auto_compute_assert_modify_target (model, target, error);
	if (! modify_target)
		return FALSE;

	/* make lists of fields used to create INSERT, DELETE and UPDATE queries */
	cond_query_fields = auto_compute_make_cond_query_fields (model, modify_target, 
								 options & GDA_DATA_MODEL_QUERY_OPTION_USE_ALL_FIELDS_IF_NO_PK,
								 error);
	if (!cond_query_fields)
		return FALSE;

	mod_query_fields = auto_compute_make_mod_query_fields (model, modify_target, error);
	if (!mod_query_fields) {
		g_slist_free (cond_query_fields);
		return FALSE;
	}

	/* compute UPDATE query */
	query = gda_query_new (gda_object_get_dict (GDA_OBJECT (model->priv->queries[SEL_QUERY])));
	gda_query_set_query_type (query, GDA_QUERY_TYPE_UPDATE);
	auto_compute_add_mod_fields_to_query (model, modify_target, mod_query_fields, query);
	auto_compute_add_where_cond_to_query (model, cond_query_fields, query);
	g_object_set (G_OBJECT (model), "update_query", query, NULL);
	g_object_unref (query);

	/* compute INSERT query */
	query = gda_query_new (gda_object_get_dict (GDA_OBJECT (model->priv->queries[SEL_QUERY])));
	gda_query_set_query_type (query, GDA_QUERY_TYPE_INSERT);
	auto_compute_add_mod_fields_to_query (model, modify_target, mod_query_fields, query);
	g_object_set (G_OBJECT (model), "insert_query", query, NULL);
	g_object_unref (query);

	/* compute DELETE query */
	query = gda_query_new (gda_object_get_dict (GDA_OBJECT (model->priv->queries[SEL_QUERY])));
	gda_query_set_query_type (query, GDA_QUERY_TYPE_DELETE);
	{
		GdaQueryTarget *target;
		GdaDictTable *mod_table;
		
		mod_table = GDA_DICT_TABLE (gda_query_target_get_represented_entity (modify_target));
		target = gda_query_target_new (query, gda_object_get_name (GDA_OBJECT (mod_table)));
		gda_query_add_target (query, target, NULL);
		g_object_unref (target);
	}
	auto_compute_add_where_cond_to_query (model, cond_query_fields, query);
	g_object_set (G_OBJECT (model), "delete_query", query, NULL);
	g_object_unref (query);

	g_slist_free (cond_query_fields);
	g_slist_free (mod_query_fields);
	
	return TRUE;
}

static GdaQueryTarget *
auto_compute_assert_modify_target (GdaDataModelQuery *model, const gchar *target, GError **error)
{
	/* 
	 * ensure that model->priv->modify_target is not NULL and represents a dict table 
	 */
	GdaQueryTarget *modify_target;

	if (target && *target) {
		modify_target = gda_query_get_target_by_alias (model->priv->queries[SEL_QUERY], target);
		if (!modify_target) {
			g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR,
				     GDA_DATA_MODEL_QUERY_COMPUTE_MODIF_QUERIES_ERROR,
				     _("Could not identify target table '%s' to modify in SELECT query"), target);
			return FALSE;
		}
		else {
			GdaEntity *ent;
			
			ent = gda_query_target_get_represented_entity (modify_target);
			if (!GDA_IS_DICT_TABLE (ent)) {
				g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR,
					     GDA_DATA_MODEL_QUERY_COMPUTE_MODIF_QUERIES_ERROR,
					     _("specified target to modify in SELECT query does not represent a table"));
				return FALSE;
			}
		}
	}
	else {
		GSList *targets, *table_targets, *list;
		targets = gda_query_get_targets (model->priv->queries[SEL_QUERY]);
		table_targets = NULL;
		for (list = targets; list; list = list->next) {
			GdaEntity *ent = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (list->data));
			if (GDA_IS_DICT_TABLE (ent) && ! gda_dict_table_is_view ((GdaDictTable *) ent))
				table_targets = g_slist_append (table_targets, list->data);
		}
		
		if (!table_targets) {
			g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR,
				     GDA_DATA_MODEL_QUERY_COMPUTE_MODIF_QUERIES_ERROR,
				     _("Could not identify any target table to modify in SELECT query"));
			if (targets)
				g_slist_free (targets);
			return FALSE;
		}

		if (table_targets->next) {
			g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR,
				     GDA_DATA_MODEL_QUERY_COMPUTE_MODIF_QUERIES_ERROR,
				     _("More than one target table can be modified in SELECT query, specify which one"));
			g_slist_free (table_targets);
			g_slist_free (targets);
			return FALSE;
		}
		
		modify_target = GDA_QUERY_TARGET (table_targets->data);
		g_slist_free (table_targets);
		g_slist_free (targets);
	}
	
	return modify_target;
}

static GSList *
auto_compute_make_cond_query_fields (GdaDataModelQuery *model, GdaQueryTarget *modify_target, 
				     gboolean use_all_if_no_pk, GError **error)
{
	/*
	 * ensure that all the PK fields of table are there
	 */
	GdaDictTable *mod_table;
	GdaDictConstraint* pk_constraint;
	GSList *cond_query_fields = NULL;
	gboolean error_is_set = FALSE;
		
	mod_table = GDA_DICT_TABLE (gda_query_target_get_represented_entity (modify_target));
	pk_constraint = gda_dict_table_get_pk_constraint (mod_table);
	if (pk_constraint) {
		/* use fields is Pk */
		GSList *cond_dict_fields;
		GSList *target_fields, *list;
		GSList *query_fields;
		gboolean found;
		cond_dict_fields = gda_dict_constraint_pkey_get_fields (pk_constraint);
		target_fields = gda_query_get_fields_by_target (model->priv->queries[SEL_QUERY],
								modify_target, TRUE);

		/* convert list of GdaQueryField objects (target_fields) 
		 * to list of GdaDictFieldField objects (query_fields) */
		for (query_fields = NULL, list = target_fields; list; list = list->next) {
			GdaQueryFieldField *qfield = (GdaQueryFieldField *) (list->data);
			GdaEntityField *efield;
				
			g_assert (GDA_IS_QUERY_FIELD_FIELD (qfield));
			efield = gda_query_field_field_get_ref_field (qfield);
			g_assert (GDA_IS_DICT_FIELD (efield));
			query_fields = g_slist_append (query_fields, efield);
		}
			
		for (found = TRUE, list = cond_dict_fields; list && found; list = list->next) {
			if (!g_slist_find (query_fields, list->data)) {
				found = FALSE;
				g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR,
					     GDA_DATA_MODEL_QUERY_COMPUTE_MODIF_QUERIES_ERROR,
					     _("Primary key field '%s.%s' not found in SELECT query"),
					     gda_object_get_name (GDA_OBJECT (mod_table)), 
					     gda_object_get_name (GDA_OBJECT (list->data)));
				error_is_set = TRUE;
			}
			else
				cond_query_fields = g_slist_append (cond_query_fields, 
								    g_slist_nth_data (target_fields,
										      g_slist_index (query_fields, list->data)));
		}

		g_slist_free (target_fields);
		g_slist_free (cond_dict_fields);
		g_slist_free (query_fields);

		if (!found) {
			g_slist_free (cond_query_fields);
			cond_query_fields = NULL;
		}
	}

	if (! cond_query_fields) {
		if (use_all_if_no_pk) {
			/* use ALL the present fields to make a WHERE condition */
			cond_query_fields = gda_query_get_fields_by_target (model->priv->queries[SEL_QUERY],
									    modify_target, TRUE);
			if (error_is_set && error) {
				g_error_free (*error);
				*error = NULL;
			}
		}
		else {
			if (!error_is_set) {
				/* no PK constraint => error */
				g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR,
					     GDA_DATA_MODEL_QUERY_COMPUTE_MODIF_QUERIES_ERROR,
					     _("Target table to modify does not have any defined primary key"));
			}
		}
	}

	return cond_query_fields;
}

static GSList *
auto_compute_make_mod_query_fields (GdaDataModelQuery *model, GdaQueryTarget *modify_target, GError **error)
{
	GSList *mod_query_fields = NULL;
	GSList *mod_dict_fields = NULL;
	GSList *target_fields, *list;
	gboolean duplicate = FALSE;

	/* make sure there are no duplicates in the fields referenced by modify_target */
	target_fields = gda_query_get_fields_by_target (model->priv->queries[SEL_QUERY],
							modify_target, TRUE);
	for (list = target_fields; list && !duplicate; list = list->next) {
		GdaQueryFieldField *qfield = (GdaQueryFieldField *) (list->data);
		GdaEntityField *efield;
		
		g_assert (GDA_IS_QUERY_FIELD_FIELD (qfield));
		efield = gda_query_field_field_get_ref_field (qfield);
		g_assert (GDA_IS_DICT_FIELD (efield));
		if (!g_slist_find (mod_dict_fields, efield)) {
			mod_dict_fields = g_slist_prepend (mod_dict_fields, efield);
			mod_query_fields = g_slist_prepend (mod_query_fields, qfield);
		}
		else {
			GdaDictTable *mod_table;
			duplicate = TRUE;

			mod_table = GDA_DICT_TABLE (gda_query_target_get_represented_entity (modify_target));
			g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR,
				     GDA_DATA_MODEL_QUERY_COMPUTE_MODIF_QUERIES_ERROR,
				     _("Field '%s.%s' appears more than once in SELECT query"),
				     gda_object_get_name (GDA_OBJECT (mod_table)),
				     gda_object_get_name (GDA_OBJECT (efield)));
		}
	}
	
	g_slist_free (target_fields);
	g_slist_free (mod_dict_fields);

	if (duplicate) {
		g_slist_free (mod_query_fields);
		mod_query_fields = NULL;
	}

	return mod_query_fields;
}

static void
auto_compute_add_mod_fields_to_query (GdaDataModelQuery *model, GdaQueryTarget *modify_target, 
				      GSList *mod_query_fields, GdaQuery *query)
{
	GdaQueryTarget *target;
	GdaDictTable *mod_table;
	GSList *list;

	mod_table = GDA_DICT_TABLE (gda_query_target_get_represented_entity (modify_target));
	target = gda_query_target_new (query, gda_object_get_name (GDA_OBJECT (mod_table)));
	gda_query_add_target (query, target, NULL);
	g_object_unref (target);

	for (list = mod_query_fields; list; list = list->next) {
		GdaQueryFieldField *qfield;
		GdaQueryFieldValue *qvalue;
		GdaEntityField *efield;
		gchar *str;

		efield = gda_query_field_field_get_ref_field (GDA_QUERY_FIELD_FIELD (list->data));
		qfield = (GdaQueryFieldField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD,
							      "dict", gda_object_get_dict (GDA_OBJECT (query)),
							      "query", query,
							      "target", target,
							      "field", efield, NULL);
		gda_entity_add_field (GDA_ENTITY (query), GDA_ENTITY_FIELD (qfield));
		g_object_unref (qfield);

		qvalue = (GdaQueryFieldValue *) gda_query_field_value_new (query, 
									   gda_entity_field_get_g_type (efield));
		gda_entity_add_field (GDA_ENTITY (query), GDA_ENTITY_FIELD (qvalue));
		gda_query_field_value_set_is_parameter (qvalue, TRUE);
		gda_query_field_set_visible (GDA_QUERY_FIELD (qvalue), FALSE);
		str = g_strdup_printf ("+%d", 
				       gda_entity_get_field_index (GDA_ENTITY (model->priv->queries[SEL_QUERY]),
								   GDA_ENTITY_FIELD (list->data)));
		gda_object_set_name (GDA_OBJECT (qvalue), str);
		g_free (str);
		if (gda_dict_field_is_null_allowed (GDA_DICT_FIELD (efield)))
			gda_query_field_value_set_not_null (qvalue, FALSE);
		g_object_unref (qvalue);

		g_object_set (G_OBJECT (qfield), "value-provider", qvalue, NULL);
	}
	
}

static GdaQueryCondition *auto_compute_create_cond (GdaDataModelQuery *model, GdaQuery *query, 
						    GdaQueryFieldField *cond_field);

static void
auto_compute_add_where_cond_to_query (GdaDataModelQuery *model, GSList *mod_query_fields, 
				      GdaQuery *query)
{
	GdaQueryCondition *where_cond;
	
	if (mod_query_fields->next) {
		/* more than one condition, create a AND condition as the top condition */
		GSList *list;
		where_cond = gda_query_condition_new (query, GDA_QUERY_CONDITION_NODE_AND);

		for (list = mod_query_fields; list; list = list->next) {
			GdaQueryCondition *cond;
			cond = auto_compute_create_cond (model, query, GDA_QUERY_FIELD_FIELD (list->data));
			gda_query_condition_node_add_child (where_cond, cond, NULL);
			g_object_unref (cond);
		}
	}
	else 
		where_cond = auto_compute_create_cond (model, query, GDA_QUERY_FIELD_FIELD (mod_query_fields->data));

	gda_query_set_condition (query, where_cond);
	g_object_unref (where_cond);
}

static GdaQueryCondition *
auto_compute_create_cond (GdaDataModelQuery *model, GdaQuery *query, GdaQueryFieldField *cond_field)
{
	GdaQueryCondition *cond;
	GdaQueryFieldField *qfield;
	GdaQueryFieldValue *qvalue;
	GdaEntityField *efield;
	GdaQueryTarget *target;
	gchar *str;
	GSList *targets;

	targets = gda_query_get_targets (query);
	target = (GdaQueryTarget *) (targets->data);
	g_slist_free (targets);

	efield = gda_query_field_field_get_ref_field (cond_field);
	qfield = (GdaQueryFieldField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD,
						      "dict", gda_object_get_dict (GDA_OBJECT (query)),
						      "query", query,
						      "target", target,
						      "field", efield, NULL);
	gda_entity_add_field (GDA_ENTITY (query), GDA_ENTITY_FIELD (qfield));
	gda_query_field_set_visible (GDA_QUERY_FIELD (qfield), FALSE);
	g_object_unref (qfield);

	qvalue = (GdaQueryFieldValue *)gda_query_field_value_new (query, 
								  gda_entity_field_get_g_type (efield));
	gda_entity_add_field (GDA_ENTITY (query), GDA_ENTITY_FIELD (qvalue));
	gda_query_field_value_set_is_parameter (qvalue, TRUE);
	gda_query_field_set_visible (GDA_QUERY_FIELD (qvalue), FALSE);
	str = g_strdup_printf ("-%d", 
			       gda_entity_get_field_index (GDA_ENTITY (model->priv->queries[SEL_QUERY]),
							   GDA_ENTITY_FIELD (cond_field)));
	gda_object_set_name (GDA_OBJECT (qvalue), str);
	g_free (str);
	g_object_unref (qvalue);

	cond = gda_query_condition_new (query, GDA_QUERY_CONDITION_LEAF_EQUAL);
	gda_query_condition_leaf_set_operator (cond, GDA_QUERY_CONDITION_OP_LEFT, GDA_QUERY_FIELD (qfield));
	gda_query_condition_leaf_set_operator (cond, GDA_QUERY_CONDITION_OP_RIGHT, GDA_QUERY_FIELD (qvalue));
	
	return cond;
}
