/* GDA library
 * Copyright (C) 2005 - 2008 The GNOME Foundation.
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

#include <libgda/gda-decl.h>
#include <libgda/gda-data-model-query.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-util.h>
#include <sql-parser/gda-sql-statement.h>

struct _GdaDataModelQueryPrivate {
	GdaConnection    *cnc;
	GdaStatement     *statements[4]; /* indexed by SEL_* */
	GdaSet           *params[4];  /* parameters required to execute @query, indexed by SEL_* */

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
	PROP_USE_TRANSACTION,
	PROP_CNC
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
static void forget_statement (GdaDataModelQuery *model, GdaStatement *stmt);
static void holder_changed_cb (GdaSet *paramlist, GdaHolder *param, GdaDataModelQuery *model);

static void opt_start_transaction_or_svp (GdaDataModelQuery *selmodel);
static void opt_end_transaction_or_svp (GdaDataModelQuery *selmodel);


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

		type = g_type_register_static (G_TYPE_OBJECT, "GdaDataModelQuery", &info, 0);
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

	g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("connection", "Connection to use", 
							      "Connection to use to execute statements",
							      GDA_TYPE_CONNECTION,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_SEL_QUERY,
                                         g_param_spec_object ("query", "SELECT query", 
							      "SELECT Query to be executed to populate "
							      "the model with data", 
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_INS_QUERY,
                                         g_param_spec_object ("insert_query", "INSERT query", 
							      "INSERT Query to be executed to add data",
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_UPD_QUERY,
                                         g_param_spec_object ("update_query", "UPDATE query", 
							      "UPDATE Query to be executed to update data",
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_DEL_QUERY,
                                         g_param_spec_object ("delete_query", "DELETE query", 
							      "DELETE Query to be executed to remove data",
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_object_class_install_property (object_class, PROP_USE_TRANSACTION,
                                         g_param_spec_boolean ("use_transaction", "Use transaction", 
							       "Run modification statements within a transaction",
                                                               FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

	/* virtual functions */
	object_class->dispose = gda_data_model_query_dispose;
	object_class->finalize = gda_data_model_query_finalize;
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

		if (model->priv->cnc) {
			g_object_unref (model->priv->cnc);
			model->priv->cnc = NULL;
		}
		
		if (model->priv->transaction_started || model->priv->svp_name)
			opt_end_transaction_or_svp (model);

		if (model->priv->columns) {
			g_slist_foreach (model->priv->columns, (GFunc) g_object_unref, NULL);
			g_slist_free (model->priv->columns);
			model->priv->columns = NULL;
		}

		for (i = SEL_QUERY; i <= DEL_QUERY; i++) {
			if (model->priv->statements[i])
				forget_statement (model, model->priv->statements[i]);
		
			if (model->priv->params[i]) {
				if (i == SEL_QUERY)
					g_signal_handlers_disconnect_by_func (model->priv->params[i],
									      G_CALLBACK (holder_changed_cb), model);
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
check_param_type (GdaHolder *param, GType type)
{
	if ((type != 0) && (type !=  gda_holder_get_g_type (param))) {
		g_warning (_("Type of parameter '%s' is '%s' when it should be '%s', "
			     "GdaDataModelQuery object will not work correctly"),
			   gda_holder_get_id (param), 
			   g_type_name (gda_holder_get_g_type (param)),
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
		case PROP_CNC:
			if (model->priv->cnc) {
				g_object_unref (model->priv->cnc);
				model->priv->cnc = NULL;
			}
			model->priv->cnc = g_value_get_object (value);
			if (model->priv->cnc)
				g_object_ref (model->priv->cnc);
			break;
		case PROP_SEL_QUERY:
		case PROP_INS_QUERY:
		case PROP_UPD_QUERY:
		case PROP_DEL_QUERY:
			if (model->priv->statements[qindex])
				forget_statement (model, model->priv->statements[qindex]);
			if (model->priv->params[qindex]) {
				g_signal_handlers_disconnect_by_func (model->priv->params[qindex],
								      G_CALLBACK (holder_changed_cb), model);
				g_object_unref (model->priv->params[qindex]);
				model->priv->params[qindex] = NULL;
			}

			model->priv->statements[qindex] = (GdaStatement *) g_value_get_object (value);
			if (model->priv->statements[qindex]) {
				/* make a copy of statement */
				model->priv->statements[qindex] = gda_statement_copy (model->priv->statements[qindex]);
				
				if (!gda_statement_get_parameters (model->priv->statements[qindex], 
								   &(model->priv->params[qindex]), NULL)) {
					g_warning (_("Could not get statement's parameters, "
						     "expect some problems with the GdaDataModelQuery"));
					model->priv->params[qindex] = NULL;
				}

				if (qindex == SEL_QUERY) {
					/* SELECT statement */
					if (model->priv->params[qindex])
						g_signal_connect (model->priv->params[qindex], "holder_changed",
								  G_CALLBACK (holder_changed_cb), model);
				}
				else {
					/* other statements: for all the parameters in the param list, 
					 * if some have a name like "[+-]<num>", then they will be filled with
					 * the value at the new/old <num>th column before being run, or, if the name is
					 * the same as a parameter for the SELECT query, then bind them to that parameter */
					gint num;

					if (model->priv->params [qindex]) {
						GSList *params;
						for (params = model->priv->params [qindex]->holders; 
						     params; params = params->next) {
							GdaHolder *holder = GDA_HOLDER (params->data);
							const gchar *pname = gda_holder_get_id (holder);
							gboolean old_val;
							
							if (param_name_to_int (pname, &num, &old_val)) {
								if (old_val)
									g_object_set_data ((GObject*) holder, "-num", 
											   GINT_TO_POINTER (num + 1));
								else
									g_object_set_data ((GObject*) holder, "+num", 
											   GINT_TO_POINTER (num + 1));
								g_object_set_data ((GObject*) holder, "_num",
										   GINT_TO_POINTER (num + 1));
								GdaColumn *col;
								col = gda_data_model_describe_column ((GdaDataModel *) model,
												      num);
								if (col) {
									check_param_type (holder, gda_column_get_g_type (col));
									gda_holder_set_not_null (holder,
												 !gda_column_get_allow_null (col));
									if (gda_column_get_auto_increment (col) ||
									    gda_column_get_default_value (col)) {
										GValue *value;
										value = gda_value_new_null ();
										gda_holder_set_default_value (holder, value);
										gda_value_free (value);
									}
								}
							}
							else {
								if (pname && model->priv->params [SEL_QUERY]) {
									GdaHolder *bind_to;
									bind_to = gda_set_get_holder 
										(model->priv->params [SEL_QUERY],
										 pname);
									if (bind_to) {
										check_param_type (holder,
												  gda_holder_get_g_type (bind_to));
										gda_holder_set_bind (holder, bind_to);
									}
									else
										g_warning (_("Could not find a parameter named "
											     "'%s' among the SELECT query's "
											     "parameters, the specified "
											     "modification query will not be executable"),
											   pname);
								}
							}
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
		case PROP_CNC:
			g_value_set_object (value, model->priv->cnc);
			break;
		case PROP_SEL_QUERY:
		case PROP_INS_QUERY:
		case PROP_UPD_QUERY:
		case PROP_DEL_QUERY:
			g_value_set_object (value, G_OBJECT (model->priv->statements[qindex]));
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
forget_statement (GdaDataModelQuery *model, GdaStatement *stmt)
{
	gint i;
	gint qindex = -1;
	
	for (i = SEL_QUERY; (i <= DEL_QUERY) && (qindex < 0); i++) 
		if (stmt == model->priv->statements [i])
			qindex = i;
	g_assert (qindex >= 0);

	model->priv->statements [qindex] = NULL;
	g_object_unref (stmt);
}

static void
holder_changed_cb (GdaSet *paramlist, GdaHolder *param, GdaDataModelQuery *model)
{
	/* first: sync the parameters which are bound */
	if (model->priv->params [SEL_QUERY]) {
		gint i;
		for (i = INS_QUERY; i <= DEL_QUERY; i++) {
			if (model->priv->params [i]) {
				GSList *params;
				for (params = model->priv->params [i]->holders; params; params = params->next) {
					GdaHolder *bind_to;
					bind_to = gda_holder_get_bind (GDA_HOLDER (params->data));
					if (bind_to == param)
						gda_holder_set_value (GDA_HOLDER (params->data),
								      gda_holder_get_value (bind_to));
				}
			}
		}
	}

	/* second: do a refresh */
	if (gda_set_is_valid (paramlist))
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
 * @cnc: a #GdaConnection object, or %NULL
 * @select_stmt: a SELECT statement 
 *
 * Creates a new #GdaDataModel object using the data returned by the execution of the
 * @select_stmt SELECT statement.
 *
 * If @cnc is %NULL, then a real connection will have to be set before anything usefull can be done
 *
 * Returns: a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_query_new (GdaConnection *cnc, GdaStatement *select_stmt)
{
	GdaDataModelQuery *model;

	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (select_stmt), NULL);

	model = g_object_new (GDA_TYPE_DATA_MODEL_QUERY, "connection", cnc, 
			      "query", select_stmt, NULL);

	return GDA_DATA_MODEL (model);
}

/**
 * gda_data_model_query_get_parameter_list
 * @model: a #GdaDataModelQuery data model
 *
 * If some parameters are required to execute the SELECT query used in the @model data model, then
 * returns the #GdaSet used; otherwise does nothing and returns %NULL.
 *
 * Returns: a #GdaSet object, or %NULL
 */
GdaSet *
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

	if (! model->priv->statements[SEL_QUERY])
		return TRUE;
	
	if (! model->priv->cnc) {
		g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR, GDA_DATA_MODEL_QUERY_CONNECTION_ERROR,
			     _("No connection specified"));
		return FALSE;
	}
	if (! gda_connection_is_opened (model->priv->cnc)) {
		g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR, GDA_DATA_MODEL_QUERY_CONNECTION_ERROR,
			     _("Specified is closed"));
		return FALSE;
	}

	model->priv->data = gda_connection_statement_execute_select (model->priv->cnc, 
								     model->priv->statements[SEL_QUERY], 
								     model->priv->params [SEL_QUERY], error);
	if (!model->priv->data) {
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
 * @mod_stmt: a #GdaStatement object
 * @error: a place to store errors, or %NULL
 *
 * Sets the modification statement to be used by @model to actually perform any change
 * to the dataset in the database.
 *
 * The provided @mod_stmt statement must be either a INSERT, UPDATE or DELETE query. It can contain
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
 * Examples of statements are: "INSERT INTO orders (customer, creation_date, delivery_before, delivery_date) VALUES (## / *name:'Customer' type:integer* /, date('now'), ## / *name:"+2" type:date nullok:TRUE * /, NULL)", "DELETE FROM orders WHERE id = ## / *name:"-0" type:integer* /" and "UPDATE orders set id=## / *name:"+0" type:integer* /, delivery_before=## / *name:"+2" type:date nullok:TRUE* /, delivery_date=## / *name:"+3" type:date nullok:TRUE* / WHERE id=## / *name:"-0" type:integer* /"
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_model_query_set_modification_query (GdaDataModelQuery *model, GdaStatement *mod_stmt, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (mod_stmt), FALSE);

	switch (gda_statement_get_statement_type (mod_stmt)) {
	case GDA_SQL_STATEMENT_INSERT:
		g_object_set (model, "insert_query", mod_stmt, NULL);
		break;
	case GDA_SQL_STATEMENT_UPDATE:
		g_object_set (model, "update_query", mod_stmt, NULL);
		break;
	case GDA_SQL_STATEMENT_DELETE:
		g_object_set (model, "delete_query", mod_stmt, NULL);
		break;
	default:
		g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR, GDA_DATA_MODEL_QUERY_MODIF_STATEMENT_ERROR,
			     _("Wrong type of query"));
		return FALSE;
	}
	return TRUE;
}

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
	if (! model->priv->statements[SEL_QUERY])
		return;

	/* we need to be able to get (or build) a list of column's title and GType. Scan through the
	 * statement's GdaSqlSelectFields and if a name and GType can be determined, then use them, otherwise
	 * use "column%d" and G_TYPE_STRING, and let the user modify that directly using gda_data_model_describe () 
	 */
	GdaSqlStatement *sqlst;
	g_object_get (G_OBJECT (model->priv->statements[SEL_QUERY]), "structure", &sqlst, NULL);

	if (gda_sql_statement_normalize (sqlst, model->priv->cnc, NULL)) {
		GdaSqlStatementSelect *selst = (GdaSqlStatementSelect*) sqlst->contents;
		GSList *list;
		
		for (list = selst->expr_list; list; list = list->next) {
			GdaSqlSelectField *field = (GdaSqlSelectField*) list->data;
			GdaColumn *col;

			col = gda_column_new ();
			if (field->validity_meta_table_column) {
				gda_column_set_name (col, field->validity_meta_table_column->column_name);
				gda_column_set_title (col, field->validity_meta_table_column->column_name);
				gda_column_set_g_type (col, field->validity_meta_table_column->gtype);
			}
			else {
				gda_column_set_name (col, "col");
				gda_column_set_title (col, "col");
				gda_column_set_g_type (col, G_TYPE_STRING);
			}
			model->priv->columns = g_slist_append (model->priv->columns, col);
		}
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
	gda_sql_statement_free (sqlst);

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
				GSList *params;
				for (params = selmodel->priv->params [i]->holders; params; params = params->next) {
					gint num;
					
					num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "_num")) - 1;
					if (num < 0)
						allok = gda_holder_is_valid ((GdaHolder*)(params->data));
					if (!allok) {
						break;
						g_print ("Not OK:\n");
					}
				}
				
			}
			else {
				if (!selmodel->priv->statements [i])
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
	GdaHolder *p_used = NULL;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = (GdaDataModelQuery *) model;
	g_return_val_if_fail (selmodel->priv, 0);
	
	if (selmodel->priv->data)
		flags = gda_data_model_get_attributes_at (selmodel->priv->data, col, row);

	if ((row >= 0) && selmodel->priv->statements[UPD_QUERY] && selmodel->priv->params[UPD_QUERY]) {
		GSList *params;
		for (params = selmodel->priv->params[UPD_QUERY]->holders; params; params = params->next) {
			if (GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1 == col) {
				p_used = (GdaHolder *) params->data;
				break;
			}
		}
	}

	if ((row < 0) && selmodel->priv->statements[INS_QUERY] && selmodel->priv->params[INS_QUERY]) {
		GSList *params;
		for (params = selmodel->priv->params[INS_QUERY]->holders; params; params = params->next) {
			if (GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1 == col) {
				p_used = (GdaHolder *) params->data;
				break;
			}
		}
	}

	if (!p_used)
		flags |= GDA_VALUE_ATTR_NO_MODIF;
	else {
		flags &= ~GDA_VALUE_ATTR_NO_MODIF;
		flags &= ~GDA_VALUE_ATTR_CAN_BE_NULL;
		flags &= ~GDA_VALUE_ATTR_CAN_BE_DEFAULT;
		if (! gda_holder_get_not_null (p_used))
			flags |= GDA_VALUE_ATTR_CAN_BE_NULL;
		if (gda_holder_get_default_value (p_used)) 
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
						  "data_model", model, NULL);
	/* set the "__gda_entry_plugin" property for all the parameters depending on the SELECT query field */
	static gboolean warned = FALSE;
	if (!warned) {
		warned = TRUE;
		TO_IMPLEMENT;
	}
	/*
	if (gda_query_is_select_query (GDA_DATA_MODEL_QUERY (model)->priv->statements[SEL_QUERY])) {
		GSList *list, *fields;
		GSList *params;

		fields = gda_entity_get_fields (GDA_ENTITY (GDA_DATA_MODEL_QUERY (model)->priv->statements[SEL_QUERY]));
		params = GDA_SET (iter)->holders;

		for (list = fields; list && params; list = list->next, params = params->next) {
			GdaEntityField *field = (GdaEntityField *) list->data;
			if (GDA_IS_QUERY_FIELD_FIELD (field)) {
				gchar *plugin;

			        g_object_get (G_OBJECT (field), "__gda_entry_plugin", &plugin, NULL);
				if (plugin) {
					g_object_set (G_OBJECT (params->data), "__gda_entry_plugin", plugin, NULL);
					g_free (plugin);
				}
			}
		}
		g_slist_free (fields);
	}
	*/

	return iter;
}

static gchar *try_add_savepoint (GdaDataModelQuery *selmodel);
static void   try_remove_savepoint (GdaDataModelQuery *selmodel, const gchar *svp_name);

static gchar *
try_add_savepoint (GdaDataModelQuery *selmodel)
{
	gchar *svp_name = NULL;

	if (selmodel->priv->cnc && 
	    gda_connection_supports_feature (selmodel->priv->cnc, GDA_CONNECTION_FEATURE_SAVEPOINTS)) {
		gchar *name;
		
		name = g_strdup_printf ("_gda_data_model_query_svp_%p_%d", selmodel, selmodel->priv->svp_id++);
		if (gda_connection_add_savepoint (selmodel->priv->cnc, name, NULL))
			svp_name = name;
		else
			g_free (name);
	}
	return svp_name;
}

static void
try_remove_savepoint (GdaDataModelQuery *selmodel, const gchar *svp_name)
{
	if (!svp_name)
		return;
	if (selmodel->priv->cnc && 
	    gda_connection_supports_feature (selmodel->priv->cnc, GDA_CONNECTION_FEATURE_SAVEPOINTS_REMOVE))
		gda_connection_delete_savepoint (selmodel->priv->cnc, svp_name, NULL);
}

static gboolean
run_modify_query (GdaDataModelQuery *selmodel, gint query_type, GError **error)
{
	gboolean modify_done = FALSE;
	gchar *svp_name = NULL;

	/* try to add a savepoint if we are not doing multiple updates */
	if (!selmodel->priv->multiple_updates)
		svp_name = try_add_savepoint (selmodel);

	if (! selmodel->priv->cnc) {
		g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR, GDA_DATA_MODEL_QUERY_CONNECTION_ERROR,
			     _("No connection specified"));
		return FALSE;
	}
	if (! gda_connection_is_opened (selmodel->priv->cnc)) {
		g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR, GDA_DATA_MODEL_QUERY_CONNECTION_ERROR,
			     _("Specified is closed"));
		return FALSE;
	}

	if (gda_connection_statement_execute_non_select (selmodel->priv->cnc, 
							 selmodel->priv->statements[query_type], 
							 selmodel->priv->params [query_type], NULL, error) != -1)
		modify_done = TRUE;

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
	GdaSet *paramlist;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);

	/* make sure there is a UPDATE query */
	if (!selmodel->priv->statements[UPD_QUERY]) {
		g_set_error (error, 0, 0,
			     _("No UPDATE query specified, can't update row"));
		return FALSE;
	}

	/* set the values of the parameters in the paramlist if necessary */
	paramlist = selmodel->priv->params[UPD_QUERY];
	if (paramlist && paramlist->holders) {
		GSList *params;
		for (params = paramlist->holders; params; params = params->next) {
			gint num;

			num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1;
			if (num >= 0) {
				/* new values */
				if (num == col)
					gda_holder_set_value (GDA_HOLDER (params->data), value);
				else
					gda_holder_set_value (GDA_HOLDER (params->data), NULL);
			}
			else {
				num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "-num")) - 1;	
				if (num >= 0)
					/* old values */
					gda_holder_set_value (GDA_HOLDER (params->data),
							      gda_data_model_get_value_at ((GdaDataModel*) selmodel, 
											   num, row));
			}
		}
	}

	/* render the SQL and run it */
	return run_modify_query (selmodel, UPD_QUERY, error);
}

static gboolean
gda_data_model_query_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	GdaDataModelQuery *selmodel;
	GdaSet *paramlist;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);

	/* make sure there is a UPDATE query */
	if (!selmodel->priv->statements[UPD_QUERY]) {
		g_set_error (error, 0, 0,
			     _("No UPDATE query specified, can't update row"));
		return FALSE;
	}

	/* set the values of the parameters in the paramlist if necessary */
	paramlist = selmodel->priv->params[UPD_QUERY];
	if (paramlist && paramlist->holders) {
		GSList *params;
		for (params = paramlist->holders; params; params = params->next) {
			gint num;

			num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1;
			if (num >= 0) {
				/* new values */
				GValue *value;
				value = g_list_nth_data ((GList *) values, num);
				if (value)
					gda_holder_set_value (GDA_HOLDER (params->data), value);
				else
					gda_holder_set_value (GDA_HOLDER (params->data), NULL);
			}
			else {
				num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "-num")) - 1;
				if (num >= 0)
					/* old values */
					gda_holder_set_value (GDA_HOLDER (params->data),
							      gda_data_model_get_value_at ((GdaDataModel*) selmodel, 
											   num, row));
			}
		}
	}

	/* render the SQL and run it */
	return run_modify_query (selmodel, UPD_QUERY, error);
}

static gint
gda_data_model_query_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	GdaDataModelQuery *selmodel;
	GdaSet *paramlist;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), -1);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, -1);

	/* make sure there is a INSERT query */
	if (!selmodel->priv->statements[INS_QUERY]) {
		g_set_error (error, 0, 0,
			     _("No INSERT query specified, can't insert row"));
		return -1;
	}

	/* set the values of the parameters in the paramlist if necessary */
	paramlist = selmodel->priv->params[INS_QUERY];
	if (paramlist && paramlist->holders) {
		GSList *params;
		for (params = paramlist->holders; params; params = params->next) {
			gint num;

			num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1;
			if (num >= 0) {
				/* new values only */
				GValue *value;
				value = g_list_nth_data ((GList *) values, num);
				if (value)
					gda_holder_set_value (GDA_HOLDER (params->data), value);
				else
					g_object_set (G_OBJECT (params->data), "use-default-value", TRUE, NULL);
			}
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
	GdaSet *paramlist;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), -1);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, -1);

	/* make sure there is a INSERT query */
	if (!selmodel->priv->statements[INS_QUERY]) {
		g_set_error (error, 0, 0,
			     _("No INSERT query specified, can't insert row"));
		return -1;
	}

	/* set the values of the parameters in the paramlist if necessary */
	paramlist = selmodel->priv->params[INS_QUERY];
	if (paramlist && paramlist->holders) {
		GSList *params;
		for (params = paramlist->holders; params; params = params->next) {
			gint num;

			num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "+num")) - 1;

			if (num >= 0) 
				/* new values only */
				gda_holder_set_value (GDA_HOLDER (params->data), NULL);
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
	GdaSet *paramlist;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = GDA_DATA_MODEL_QUERY (model);
	g_return_val_if_fail (selmodel->priv, FALSE);
	
	/* make sure there is a REMOVE query */
	if (!selmodel->priv->statements[DEL_QUERY]) {
		g_set_error (error, 0, 0,
			     _("No DELETE query specified, can't delete row"));
		return FALSE;
	}

	/* set the values of the parameters in the paramlist if necessary */
	paramlist = selmodel->priv->params[DEL_QUERY];
	if (paramlist && paramlist->holders) {
		GSList *params;
		for (params = paramlist->holders; params; params = params->next) {
			gint num;

			num = GPOINTER_TO_INT (g_object_get_data ((GObject*) params->data, "-num")) - 1;
			if (num >= 0) 
				/* old values only */
				gda_holder_set_value (GDA_HOLDER (params->data),
						      gda_data_model_get_value_at ((GdaDataModel*) selmodel, num, row));
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
	cnc = selmodel->priv->cnc;
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
	cnc = selmodel->priv->cnc;
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
 * Computation of INSERT, DELETE and UPDATE statements
 */
/**
 * gda_data_model_query_compute_modification_queries
 * @model: a GdaDataModelQuery object
 * @target: the target table to modify, or %NULL
 * @options: options to specify how the statements must be built in some special cases
 * @error: a place to store errors or %NULL
 *
 * Try to compute the INSERT, DELETE and UPDATE statements; any previous modification query
 * will be discarded.
 *
 * If specified, the table which will be updated is the one represented by the @target.
 *
 * If @target is %NULL, then an error will be returned if @model's SELECT query has more than
 * one target.
 *
 * Returns: TRUE at least one of the INSERT, DELETE and UPDATE statements has been computed
 */
gboolean
gda_data_model_query_compute_modification_queries (GdaDataModelQuery *model, const gchar *target, 
						   GdaDataModelQueryOptions options, GError **error)
{
	gint i;
	gboolean require_pk = TRUE;
	GdaStatement *ins, *upd, *del;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	if (!model->priv->statements[SEL_QUERY]) {
		g_set_error (error, GDA_DATA_MODEL_QUERY_ERROR,
			     GDA_DATA_MODEL_QUERY_COMPUTE_MODIF_STATEMENTS_ERROR,
			     _("No SELECT query specified"));
		return FALSE;
	}

	for (i = SEL_QUERY + 1; i <= DEL_QUERY; i++) {
		if (model->priv->statements[i])
			forget_statement (model, model->priv->statements[i]);
	}
	
	if (options & GDA_DATA_MODEL_QUERY_OPTION_USE_ALL_FIELDS_IF_NO_PK)
		require_pk = FALSE;
	if (gda_compute_dml_statements (model->priv->cnc, model->priv->statements[SEL_QUERY], require_pk, 
					&ins, &upd, &del, error)) {
		
		g_object_set (G_OBJECT (model), "insert_query", ins, 
			      "update-query", upd, "delete-query", del, NULL);
		if (ins) g_object_unref (ins);
		if (upd) g_object_unref (upd);
		if (del) g_object_unref (del);
		return TRUE;
	}

	return FALSE;
}
