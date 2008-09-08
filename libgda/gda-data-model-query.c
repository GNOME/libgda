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
#include <libgda/gda-data-select-extra.h>
#include <libgda/gda-set.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-data-select.h>

struct _GdaDataModelQueryPrivate {
	GdaConnection    *cnc;
	GdaDataModel     *data;
	GdaDataSelectInternals *inter;

	GError           *exec_error;
	GdaStatement     *select_stmt;
	GdaSet           *params;

	guint             svp_id;   /* counter for savepoints */
	gboolean          transaction_needs_commit;
	gboolean          transaction_allowed;
        gboolean          transaction_started; /* true if we have started a transaction here */
	gchar            *svp_name; /* non NULL if we have created a savepoint here */
        /* note: transaction_started and svp_name are mutually exclusive */
};

/* properties */
enum
{
        PROP_0,
	PROP_CNC,
	PROP_STMT,
	PROP_PARAMS
};


static void gda_data_model_query_class_init (GdaDataModelQueryClass *klass);
static GObject *gda_data_model_query_constructor (GType type,
						  guint n_construct_properties,
						  GObjectConstructParam *construct_properties);
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
static const GValue        *gda_data_model_query_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);
static GdaValueAttribute    gda_data_model_query_get_attributes_at (GdaDataModel *model, gint col, gint row);

static gboolean             gda_data_model_query_set_value_at    (GdaDataModel *model, gint col, gint row, 
								   const GValue *value, GError **error);
static gboolean             gda_data_model_query_set_values      (GdaDataModel *model, gint row, 
								   GList *values, GError **error);
static gint                 gda_data_model_query_append_values   (GdaDataModel *model, const GList *values, GError **error);
static gboolean             gda_data_model_query_remove_row      (GdaDataModel *model, gint row, 
								  GError **error);
static void                 gda_data_model_query_send_hint       (GdaDataModel *model, GdaDataModelHint hint, 
								  const GValue *hint_value);

static void holder_changed_cb (GdaSet *paramlist, GdaHolder *param, GdaDataModelQuery *model);

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
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaDataModelQuery", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		}
		g_static_mutex_unlock (&registering);
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
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_STMT,
                                         g_param_spec_object ("statement", "SELECT statement", 
							      "SELECT statement to be executed to populate "
							      "the model with data", 
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_PARAMS,
                                         g_param_spec_object ("exec-params", "SELECT statement's parameters", 
							      "Parameters used with the SELECT statement",
							      GDA_TYPE_SET,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));


	/* virtual functions */
	object_class->constructor = gda_data_model_query_constructor;
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

	iface->i_create_iter = NULL;
	iface->i_iter_at_row = NULL;
	iface->i_iter_next = NULL;
	iface->i_iter_prev = NULL;

	iface->i_set_value_at = gda_data_model_query_set_value_at;
	iface->i_set_values = gda_data_model_query_set_values;
        iface->i_append_values = gda_data_model_query_append_values;
	iface->i_append_row = NULL;
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
	model->priv->inter = NULL;
	model->priv->exec_error = NULL;
	model->priv->select_stmt = NULL;
	model->priv->params = NULL;
}

static GObject *
gda_data_model_query_constructor (GType type,
				  guint n_construct_properties,
				  GObjectConstructParam *construct_properties) 
{
	GObject *object;
	GdaDataModelQuery *model;

	object = G_OBJECT_CLASS (parent_class)->constructor (type,
							     n_construct_properties,
							     construct_properties);
	model = (GdaDataModelQuery *) object;
	gda_data_model_query_refresh (model, NULL);

	return object;
}

static void
gda_data_model_query_dispose (GObject *object)
{
	GdaDataModelQuery *model = (GdaDataModelQuery *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (model));

	/* free memory */
	if (model->priv) {
		if (model->priv->cnc) {
			g_object_unref (model->priv->cnc);
			model->priv->cnc = NULL;
		}
		
		if (model->priv->data) {
			g_object_unref (model->priv->data);
			model->priv->data = NULL;
		}

		if (model->priv->select_stmt) {
			g_object_unref (model->priv->select_stmt);
			model->priv->select_stmt = NULL;
		}

		if (model->priv->params) {
			g_object_unref (model->priv->params);
			model->priv->params = NULL;
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
		g_free (model->priv->svp_name);
		if (model->priv->inter)
			_gda_data_select_internals_free (model->priv->inter);

		if (model->priv->exec_error)
			g_error_free (model->priv->exec_error);
		g_free (model->priv);
		model->priv = NULL;
	}

	/* chain to parent class */
	parent_class->finalize (object);
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
		case PROP_CNC:
			model->priv->cnc = g_value_get_object (value);
			if (model->priv->cnc)
				g_object_ref (model->priv->cnc);
			break;
		case PROP_STMT:
			model->priv->select_stmt = g_value_get_object (value);
			if (model->priv->select_stmt)
				g_object_ref (model->priv->select_stmt);
			break;
		case PROP_PARAMS:
			model->priv->params = g_value_get_object (value);
			if (model->priv->params) {
				g_object_ref (model->priv->params);
				g_signal_connect (model->priv->params, "holder-changed",
						  G_CALLBACK (holder_changed_cb), model);
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
		case PROP_CNC:
			g_value_set_object (value, model->priv->cnc);
			break;
		case PROP_STMT:
			g_value_set_object (value, model->priv->select_stmt);
			break;
		case PROP_PARAMS:
			g_value_set_object (value, model->priv->params);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
}


static void
holder_changed_cb (GdaSet *paramlist, GdaHolder *param, GdaDataModelQuery *model)
{
	gda_data_model_query_refresh (model, NULL);
}

/**
 * gda_data_model_query_new
 * @cnc: a #GdaConnection object
 * @select_stmt: a SELECT statement 
 * @params: a #GdaSet object containing @select_stmt's execution parameters (see gda_statement_get_parameters()), or %NULL
 *
 * Creates a new #GdaDataModel object using the data returned by the execution of the
 * @select_stmt SELECT statement.
 *
 * Note: if @select_stmt contains one or more parameters, then @params should not be %NULL otherwise
 * the resulting object will never contain anything and will be unuseable as the SELECT statement
 * will never be successfully be executed.
 *
 * Returns: a pointer to the newly created #GdaDataModel.
 */
GdaDataModelQuery *
gda_data_model_query_new (GdaConnection *cnc, GdaStatement *select_stmt, GdaSet *params)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (select_stmt), NULL);
	g_return_val_if_fail (!params || GDA_IS_SET (params), NULL);

	return (GdaDataModelQuery*)  g_object_new (GDA_TYPE_DATA_MODEL_QUERY, 
						   "connection", cnc, 
						   "statement", select_stmt, 
						   "exec-params", params, NULL);
}

static gboolean
do_notify_changes (GdaDataModel *model)
{
	gboolean notify_changes = TRUE;
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_notify)
		notify_changes = (GDA_DATA_MODEL_GET_CLASS (model)->i_get_notify) (model);
	return notify_changes;
}

static void
data_select_changed_cb (GdaDataModel *priv, GdaDataModelQuery *model)
{
	if (do_notify_changes ((GdaDataModel*) model))
		g_signal_emit_by_name (model, "changed");
}

static void
data_select_reset_cb (GdaDataModel *priv, GdaDataModelQuery *model)
{
	g_signal_emit_by_name (model, "reset");
}

static void
data_select_row_inserted_cb (GdaDataModel *priv, gint row, GdaDataModelQuery *model)
{
	if (do_notify_changes ((GdaDataModel*) model))
		g_signal_emit_by_name (model, "row-inserted", row);
}

static void
data_select_row_removed_cb (GdaDataModel *priv, gint row, GdaDataModelQuery *model)
{
	if (do_notify_changes ((GdaDataModel*) model))
		g_signal_emit_by_name (model, "row-removed", row);
}

static void
data_select_row_updated_cb (GdaDataModel *priv, gint row, GdaDataModelQuery *model)
{
	if (do_notify_changes ((GdaDataModel*) model))
		g_signal_emit_by_name (model, "row-updated", row);
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
		/* copy the internals of the GdaDataSelect object */
		model->priv->inter = _gda_data_select_internals_steal ((GdaDataSelect*) model->priv->data);
		g_object_unref (model->priv->data);
		model->priv->data = NULL;
	}

	if (model->priv->exec_error) {
		g_error_free (model->priv->exec_error);
		model->priv->exec_error = NULL;
	}
	
	if (! model->priv->cnc) {
		g_set_error (&(model->priv->exec_error), GDA_DATA_MODEL_QUERY_ERROR, GDA_DATA_MODEL_QUERY_CONNECTION_ERROR,
			     _("No connection specified"));
		g_propagate_error (error, g_error_copy (model->priv->exec_error));
		return FALSE;
	}
	if (! gda_connection_is_opened (model->priv->cnc)) {
		g_set_error (&(model->priv->exec_error), GDA_DATA_MODEL_QUERY_ERROR, GDA_DATA_MODEL_QUERY_CONNECTION_ERROR,
			     _("Specified connection is closed"));
		g_propagate_error (error, g_error_copy (model->priv->exec_error));
		return FALSE;
	}

	model->priv->data = gda_connection_statement_execute_select (model->priv->cnc, 
								     model->priv->select_stmt, 
								     model->priv->params, &(model->priv->exec_error));
	if (!model->priv->data) {
		g_propagate_error (error, g_error_copy (model->priv->exec_error));
		return FALSE;
	}

	/* if there were some internals from a previous GdaDataSelect, then set the there */
	if (model->priv->inter) {
		_gda_data_select_internals_paste ((GdaDataSelect*) model->priv->data, model->priv->inter);
		model->priv->inter = NULL;
	}

	/* be ready to propagate signals */
	g_signal_connect (model->priv->data, "changed",
			  G_CALLBACK (data_select_changed_cb), model);
	g_signal_connect (model->priv->data, "reset",
			  G_CALLBACK (data_select_reset_cb), model);
	g_signal_connect (model->priv->data, "row-inserted",
			  G_CALLBACK (data_select_row_inserted_cb), model);
	g_signal_connect (model->priv->data, "row-removed",
			  G_CALLBACK (data_select_row_removed_cb), model);
	g_signal_connect (model->priv->data, "row-updated",
			  G_CALLBACK (data_select_row_updated_cb), model);

	/* emit the "reset" signal */
	gda_data_model_reset ((GdaDataModel *) model);
	return TRUE;
}

/**
 * gda_data_model_query_set_row_selection_condition
 * @model: a #GdaDataSelect data model
 * @expr: a #GdaSqlExpr expression
 * @error: a place to store errors, or %NULL
 *
 * Offers the same features as gda_data_model_query_set_row_selection_condition_sql() but using a #GdaSqlExpr
 * structure instead of an SQL syntax.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_query_set_row_selection_condition (GdaDataModelQuery *model, GdaSqlExpr *expr, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, FALSE);

	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return FALSE;
	}
	else
		return gda_data_select_set_row_selection_condition (GDA_DATA_SELECT (selmodel->priv->data), expr, error);
}

/**
 * gda_data_model_query_set_row_selection_condition_sql
 * @model: a #GdaDataSelect data model
 * @sql_where: an SQL condition (withouth the WHERE keyword)
 * @error: a place to store errors, or %NULL
 *
 * Specifies the SQL condition corresponding to the WHERE part of a SELECT statement which would
 * return only 1 row (the expression of the primary key).
 *
 * For example for a table created as <![CDATA["CREATE TABLE mytable (part1 int NOT NULL, part2 string NOT NULL, 
 * name string, PRIMARY KEY (part1, part2))"]]>, and if @pmodel corresponds to the execution of the 
 * <![CDATA["SELECT name, part1, part2 FROM mytable"]]>, then the sensible value for @sql_where would be
 * <![CDATA["part1 = ##-1::int AND part2 = ##-2::string"]]> because the values of the 'part1' field are located
 * in @pmodel's column number 1 and the values of the 'part2' field are located
 * in @pmodel's column number 2 and the primary key is composed of (part1, part2).
 *
 * For more information about the syntax of the parameters (named <![CDATA["##-1::int"]]> for example), see the
 * <link linkend="GdaSqlParser.description">GdaSqlParser</link> documentation, and 
 * gda_data_model_query_set_modification_statement().
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_query_set_row_selection_condition_sql (GdaDataModelQuery *model, const gchar *sql_where, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, FALSE);

	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return FALSE;
	}
	else
		return gda_data_select_set_row_selection_condition_sql (GDA_DATA_SELECT (selmodel->priv->data), sql_where, error);
}

/**
 * gda_data_model_query_compute_row_selection_condition
 * @model: a #GdaDataSelect object
 * @error: a place to store errors, or %NULL
 *
 * Offers the same features as gda_data_model_query_set_row_selection_condition() but the expression
 * is computed from the meta data associated to the connection being used when @model was created.
 *
 * NOTE1: make sure the meta data associated to the connection is up to date before using this
 * method, see gda_connection_update_meta_store().
 * 
 * NOTE2: if the SELECT statement from which @model has been created uses more than one table, or
 * if the table used does not have any primary key, then this method will fail
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_model_query_compute_row_selection_condition (GdaDataModelQuery *model, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, FALSE);

	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return FALSE;
	}
	else
		return gda_data_select_compute_row_selection_condition (GDA_DATA_SELECT (selmodel->priv->data), error);
}

/**
 * gda_data_model_query_set_modification_statement
 * @model: a #GdaDataSelect data model
 * @mod_stmt: a #GdaStatement (INSERT, UPDATE or DELETE)
 * @error: a place to store errors, or %NULL
 *
 * Informs @model that it should allow modifications to the data in some columns and some rows
 * using @mod_stmt to propagate those modifications into the database.
 *
 * If @mod_stmt is:
 * <itemizedlist>
 *  <listitem><para>an UPDATE statement, then all the rows in @model will be modifyable</para></listitem>
 *  <listitem><para>a DELETE statement, then it will be possible to delete rows in @model</para></listitem>
 *  <listitem><para>in INSERT statement, then it will be possible to add some rows to @model</para></listitem>
 *  <listitem><para>any other statement, then this method will return an error</para></listitem>
 * </itemizedlist>
 *
 * This method can be called several times to specify different types of modification.
 *
 * If @mod_stmt is an UPDATE or DELETE statement then it should have a WHERE part which identifies
 * a unique row in @model (please note that this property can't be checked but may result
 * in @model behaving in an unpredictable way).
 *
 * NOTE1: However, if the gda_data_model_query_set_row_selection_condition()
 * or gda_data_model_query_set_row_selection_condition_sql() have been successfully be called before, the WHERE
 * part of @mod_stmt <emphasis>WILL</emphasis> be modified to use the row selection condition specified through one of
 * these methods (please not that it is then possible to avoid specifying a WHERE part in @mod_stmt then).
 *
 * NOTE2: if gda_data_model_query_set_row_selection_condition()
 * or gda_data_model_query_set_row_selection_condition_sql() have not yet been successfully be called before, then
 * the WHERE part of @mod_stmt will be used as if one of these functions had been called.
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_model_query_set_modification_statement (GdaDataModelQuery *model, GdaStatement *mod_stmt, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, FALSE);

	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return FALSE;
	}
	else
		return gda_data_select_set_modification_statement (GDA_DATA_SELECT (selmodel->priv->data), mod_stmt, error);
}

/**
 * gda_data_model_query_set_modification_statement_sql
 * @model: a #GdaDataSelect data model
 * @sql: an SQL text
 * @error: a place to store errors, or %NULL
 *
 * Offers the same feature as gda_data_model_query_set_modification_statement() but using an SQL statement
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_model_query_set_modification_statement_sql  (GdaDataModelQuery *model, const gchar *sql, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, FALSE);

	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return FALSE;
	}
	else
		return gda_data_select_set_modification_statement_sql (GDA_DATA_SELECT (selmodel->priv->data), sql, error);
}

/**
 * gda_data_model_query_compute_modification_statements
 * @model: a #GdaDataSelect data model
 * @error: a place to store errors, or %NULL
 *
 * Makes @model try to compute INSERT, UPDATE and DELETE statements to be used when modifying @model's contents.
 * Note: any modification statement set using gda_data_model_query_set_modification_statement() will first be unset
 *
 * Returns: TRUE if no error occurred. If FALSE is returned, then some modification statement may still have been
 * computed
 */
gboolean
gda_data_model_query_compute_modification_statements (GdaDataModelQuery *model, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, FALSE);

	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return FALSE;
	}
	else
		return gda_data_select_compute_modification_statements (GDA_DATA_SELECT (selmodel->priv->data), error);
}

/*
 * GdaDataModel interface implementation
 */
static gint
gda_data_model_query_get_n_rows (GdaDataModel *model)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, 0);

	if (!selmodel->priv->data)
		return -1;
	else
		return gda_data_model_get_n_rows (selmodel->priv->data);
}

static gint
gda_data_model_query_get_n_columns (GdaDataModel *model)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, 0);
	
	if (!selmodel->priv->data)
		return 0;
	else
		return gda_data_model_get_n_columns (selmodel->priv->data);
}

static GdaColumn *
gda_data_model_query_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), NULL);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, NULL);

	if (!selmodel->priv->data)
		return NULL;
	else
		return gda_data_model_describe_column (selmodel->priv->data, col);
}

static GdaDataModelAccessFlags
gda_data_model_query_get_access_flags (GdaDataModel *model)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, 0);

	if (!selmodel->priv->data)
		return 0;
	else
		return gda_data_model_get_access_flags (selmodel->priv->data);
}

static const GValue *
gda_data_model_query_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), NULL);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, NULL);

	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return NULL;
	}
	else
		return gda_data_model_get_value_at (selmodel->priv->data, col, row, error);
}

static GdaValueAttribute
gda_data_model_query_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	GdaDataModelQuery *selmodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), 0);
	selmodel = (GdaDataModelQuery *) model;
	g_return_val_if_fail (selmodel->priv, 0);
	
	if (!selmodel->priv->data)
		return 0;
	else
		return gda_data_model_get_attributes_at (selmodel->priv->data, col, row);
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
gda_data_model_query_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *value, GError **error)
{
	GdaDataModelQuery *selmodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, FALSE);

	/* make sure there is a UPDATE query */
	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return FALSE;
	}
	else
		return gda_data_model_set_value_at (selmodel->priv->data, col, row, value, error);
}

static gboolean
gda_data_model_query_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	GdaDataModelQuery *selmodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, FALSE);

	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return FALSE;
	}
	else
		return gda_data_model_set_values (selmodel->priv->data, row, values, error);
}

static gint
gda_data_model_query_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	GdaDataModelQuery *selmodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), -1);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, -1);

	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return -1;
	}
	else
		return gda_data_model_append_values (selmodel->priv->data, values, error);
}


static gboolean
gda_data_model_query_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaDataModelQuery *selmodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_QUERY (model), FALSE);
	selmodel = (GdaDataModelQuery*) model;
	g_return_val_if_fail (selmodel->priv, FALSE);
	
	if (!selmodel->priv->data) {
		g_assert (selmodel->priv->exec_error);
		g_propagate_error (error, g_error_copy (selmodel->priv->exec_error));
		return FALSE;
	}
	else
		return gda_data_model_remove_row (selmodel->priv->data, row, error);
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

static gboolean
opt_end_transaction_or_svp (GdaDataModelQuery *selmodel, GError **error)
{
	GdaConnection *cnc;

	if (!selmodel->priv->transaction_started && !selmodel->priv->svp_name)
		return TRUE;

	cnc = selmodel->priv->cnc;
	if (selmodel->priv->transaction_started) {
		g_assert (!selmodel->priv->svp_name);
		if (selmodel->priv->transaction_needs_commit) {
			/* try to commit transaction */
			if (!gda_connection_commit_transaction (cnc, NULL, error))
				return FALSE;
		}
		else {
			/* rollback transaction */
			if (! gda_connection_rollback_transaction (cnc, NULL, error))
				return FALSE;
		}
		selmodel->priv->transaction_started = FALSE;
		return TRUE;
	}
	else {
		if (!selmodel->priv->transaction_needs_commit) {
			if (!gda_connection_rollback_savepoint (cnc, selmodel->priv->svp_name, error))
				return FALSE;
		}
		else
			if (gda_connection_supports_feature (cnc, GDA_CONNECTION_FEATURE_SAVEPOINTS_REMOVE))
				if (!gda_connection_delete_savepoint (cnc, selmodel->priv->svp_name, error))
					return FALSE;
		g_free (selmodel->priv->svp_name);
		selmodel->priv->svp_name = NULL;
		return TRUE;
	}
}

static void
gda_data_model_query_send_hint (GdaDataModel *model, GdaDataModelHint hint, const GValue *hint_value)
{
	GdaDataModelQuery *selmodel;
	g_return_if_fail (GDA_IS_DATA_MODEL_QUERY (model));
	selmodel = (GdaDataModelQuery*) model;
	g_return_if_fail (selmodel->priv);

	if (hint == GDA_DATA_MODEL_HINT_REFRESH)
		gda_data_model_query_refresh (selmodel, NULL);
	else {
		if (hint == GDA_DATA_MODEL_HINT_START_BATCH_UPDATE)
			opt_start_transaction_or_svp (selmodel);
		else {
			if (hint == GDA_DATA_MODEL_HINT_END_BATCH_UPDATE)
				opt_end_transaction_or_svp (selmodel, NULL);
		}
	}
}

