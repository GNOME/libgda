/*
 * Copyright (C) 2009 - 2010 Vivien Malerba
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
#include "browser-connection.h"
#include <libgda/thread-wrapper/gda-thread-wrapper.h>
#include "support.h"
#include "marshal.h"
#include <sql-parser/gda-sql-parser.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-sql-builder.h>

#include "data-source.h"


/* signals */
enum {
	EXEC_STARTED,
	EXEC_FINISHED,
	LAST_SIGNAL
};

gint data_source_signals [LAST_SIGNAL] = {0, 0};

/* 
 * Main static functions 
 */
static void data_source_class_init (DataSourceClass *klass);
static void data_source_init (DataSource *source);
static void data_source_dispose (GObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _DataSourcePrivate {
	BrowserConnection *bcnc;
	gchar             *title;
	gchar             *id;
	GError            *init_error;
	GArray            *export_names; /* array of strings, memory allocated in export_columns */
	GHashTable        *export_columns; /* key = export name, value = column number */

	gchar             *select_sql;
	guint              exec_id;
	gboolean           executing;
	gboolean           exec_again;

	GdaStatement      *stmt;
	GdaSet            *ext_params; /* "free" parameters */
	GdaSet            *params; /* all the params used when executing @stmt */
	gboolean           need_rerun; /* set to %TRUE if @params has changed since the last exec */

	GError            *exec_error;

	GdaDataModel      *model;
};

GType
data_source_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (DataSourceClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) data_source_class_init,
			NULL,
			NULL,
			sizeof (DataSource),
			0,
			(GInstanceInitFunc) data_source_init
		};

		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "DataSource", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
data_source_class_init (DataSourceClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	data_source_signals [EXEC_STARTED] =
                g_signal_new ("execution-started",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DataSourceClass, execution_started),
                              NULL, NULL,
                              _dm_marshal_VOID__VOID, G_TYPE_NONE, 0);
	data_source_signals [EXEC_FINISHED] =
                g_signal_new ("execution-finished",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DataSourceClass, execution_finished),
                              NULL, NULL,
                              _dm_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
        klass->execution_started = NULL;
        klass->execution_finished = NULL;

	object_class->dispose = data_source_dispose;
}

static void
data_source_init (DataSource *source)
{
	source->priv = g_new0 (DataSourcePrivate, 1);
	source->priv->bcnc = NULL;
	source->priv->need_rerun = FALSE;
	source->priv->exec_id = 0;
	source->priv->executing = FALSE;
	source->priv->exec_again = FALSE;
}

static void
params_changed_cb (GdaSet *params, GdaHolder *holder, DataSource *source)
{
	source->priv->need_rerun = TRUE;
}

static void
ext_params_changed_cb (GdaSet *params, GdaHolder *holder, DataSource *source)
{
	source->priv->need_rerun = TRUE;
	data_source_execute (source, NULL);
}

static void
data_source_dispose (GObject *object)
{
	DataSource *source;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_DATA_SOURCE (object));

	source = DATA_SOURCE (object);
	if (source->priv) {
		if (source->priv->bcnc)
			g_object_unref (source->priv->bcnc);
		g_clear_error (& source->priv->init_error);
		if (source->priv->stmt)
			g_object_unref (source->priv->stmt);
		if (source->priv->params) {
			g_signal_handlers_disconnect_by_func (source->priv->params,
							      G_CALLBACK (params_changed_cb), source);
			g_object_unref (source->priv->params);
		}
		if (source->priv->ext_params) {
			g_signal_handlers_disconnect_by_func (source->priv->ext_params,
							      G_CALLBACK (ext_params_changed_cb),
							      source);
			g_object_unref (source->priv->ext_params);
		}

		g_free (source->priv->id);
		g_free (source->priv->title);
		g_free (source->priv->select_sql);

		g_clear_error (&source->priv->exec_error);
		if (source->priv->model)
			g_object_unref (source->priv->model);
		
		if (source->priv->export_names)
			g_array_free (source->priv->export_names, TRUE);
		if (source->priv->export_columns)
			g_hash_table_destroy (source->priv->export_columns);

		g_free (source->priv);
		source->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void init_from_query (DataSource *source, xmlNodePtr node);
static gboolean init_from_table_node (DataSource *source, xmlNodePtr node, GError **error);

/**
 * data_source_new
 * @bcnc: a #BrowserConnection
 * @node:
 * @error:
 *
 * Creates a new #DataSource object
 *
 * Returns: a new object
 */
DataSource*
data_source_new_from_xml_node (BrowserConnection *bcnc, xmlNodePtr node, GError **error)
{
	DataSource *source;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	g_return_val_if_fail (node, NULL);

	source = DATA_SOURCE (g_object_new (DATA_SOURCE_TYPE, NULL));
	source->priv->bcnc = g_object_ref (bcnc);
	xmlChar *prop;
	prop = xmlGetProp (node, BAD_CAST "title");
	if (prop) {
		source->priv->title = g_strdup ((gchar*) prop);
		xmlFree (prop);
	}
	prop = xmlGetProp (node, BAD_CAST "id");
	if (prop) {
		source->priv->id = g_strdup ((gchar*) prop);
		xmlFree (prop);
	}

	if (!strcmp ((gchar*)node->name, "table")) {
		if (! init_from_table_node (source, node, error)) {
			g_object_unref (source);
			source = NULL;
		}
	}
	else if (!strcmp ((gchar*)node->name, "query")) {
		init_from_query (source, node);
	}
	else {
		g_set_error (error, 0, 0,
			     /* Translators: Do not translate "table" nor "query" */
			     _("Node must be \"table\" or \"query\", and is \"%s\""), (gchar*)node->name);
		g_object_unref (source);
		source = NULL;
	}
	
	return source;
}

static void
init_from_query (DataSource *source, xmlNodePtr node)
{
	GdaSqlParser *parser;
	const gchar *remain;
	xmlChar *contents;

	contents = xmlNodeGetContent (node);

	g_clear_error (& source->priv->init_error);
	parser = browser_connection_create_parser (source->priv->bcnc);
	if (contents) {
		source->priv->stmt = gda_sql_parser_parse_string (parser, (gchar*) contents,
								  &remain, &source->priv->init_error);
		xmlFree (contents);
	}
	g_object_unref (parser);

	if (source->priv->stmt) {
		if (remain)
			g_set_error (& source->priv->init_error, 0, 0,
				     _("Multiple statements detected, only the first will be used"));

		/* try to normalize the statement */
		GdaSqlStatement *sqlst;
		g_object_get ((GObject*) source->priv->stmt, "structure", &sqlst, NULL);
		if (browser_connection_normalize_sql_statement (source->priv->bcnc, sqlst, NULL))
			g_object_set ((GObject*) source->priv->stmt, "structure", sqlst, NULL);

		/* compute export data */
		if (source->priv->id) {
			if (sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) {
				GdaSqlStatementSelect *selst;
				selst = (GdaSqlStatementSelect*) sqlst->contents;
				GSList *list;
				gint i;
				for (i = 0, list = selst->expr_list; list; i++, list = list->next) {
					gchar *tmp;
					if (! source->priv->export_names)
						source->priv->export_names = g_array_new (FALSE, FALSE,
											  sizeof (gchar*));
					if (! source->priv->export_columns)
						source->priv->export_columns =
							g_hash_table_new_full (g_str_hash, g_str_equal,
									       g_free, NULL);

					tmp = g_strdup_printf ("%s@%d", source->priv->id, i+1);
					g_array_append_val (source->priv->export_names, tmp);
					g_hash_table_insert (source->priv->export_columns, tmp,
							     GINT_TO_POINTER (i + 1));
					/* g_print ("EXPORT [%s]\n", tmp); */

					GdaSqlSelectField *sf = (GdaSqlSelectField *) list->data;
					if (sf->validity_meta_table_column) {
						tmp = g_strdup_printf ("%s@%s", source->priv->id,
								       sf->validity_meta_table_column->column_name);
						g_array_append_val (source->priv->export_names, tmp);
						g_hash_table_insert (source->priv->export_columns, tmp,
								     GINT_TO_POINTER (i + 1));
						/* g_print ("EXPORT [%s]\n", tmp); */
					}
				}
			}
		}
		gda_sql_statement_free (sqlst);

		/* compute parameters */
		source->priv->need_rerun = FALSE;
		gda_statement_get_parameters (source->priv->stmt, &source->priv->params,
					      &source->priv->init_error);
		if (source->priv->params) {
			GSList *list;
			for (list = source->priv->params->holders; list; list = list->next)
				gda_holder_set_not_null (GDA_HOLDER (list->data), FALSE);

			g_signal_connect (source->priv->params, "holder-changed",
					  G_CALLBACK (params_changed_cb), source);
		}
	}
}

static GdaMetaTable *
get_meta_table (DataSource *source, const gchar *table_name, GError **error)
{
	GdaMetaStruct *mstruct;
	GdaMetaDbObject *dbo;
	GValue *vname;

	mstruct = browser_connection_get_meta_struct (source->priv->bcnc);
	if (! mstruct) {
		g_set_error (error, 0, 0,
			     _("Not ready"));
		return NULL;
	}

	g_value_set_string ((vname = gda_value_new (G_TYPE_STRING)), table_name);
	dbo = gda_meta_struct_get_db_object (mstruct, NULL, NULL, vname);
	gda_value_free (vname);
	if (! dbo) {
		g_set_error (error, 0, 0,
			     _("Could not find the \"%s\" table"), table_name);
		return NULL;
	}
	if ((dbo->obj_type != GDA_META_DB_TABLE) && (dbo->obj_type != GDA_META_DB_VIEW)) {
		g_set_error (error, 0, 0,
			     _("The \"%s\" object is not a table"), table_name);
		return NULL;
	}
	return GDA_META_TABLE (dbo);
}

static gboolean
init_from_table_node (DataSource *source, xmlNodePtr node, GError **error)
{
	xmlChar *tname;
	tname = xmlGetProp (node, BAD_CAST "name");
	if (!tname) {
		g_set_error (error, 0, 0,
			     /* Translators: Do not translate "name" */
			     _("Missing attribute \"name\" for table"));
		return FALSE;
	}
	
	if (! source->priv->title)
		source->priv->title = g_strdup_printf (_("Contents of '%s'"), (gchar*) tname);

	/* locate table */
	GdaMetaTable *mtable;
	mtable = get_meta_table (source, (gchar*) tname, error);
	if (!mtable) {
		xmlFree (tname);
		return FALSE;
	}

	/* build statement */
	GSList *list;
	GdaSqlBuilder *b;
	gint i;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	if (! gda_sql_builder_select_add_target (b, (gchar*) tname, NULL)) {
		g_set_error (error, 0, 0,
			     _("Could not build SELECT statement"));
		xmlFree (tname);
		return FALSE;
	}
	gda_sql_builder_select_set_limit (b,
					  gda_sql_builder_add_expr (b, NULL,
								    G_TYPE_INT, DEFAULT_DATA_SELECT_LIMIT),
					  0);

	for (i = 0, list = mtable->columns; list; i++, list = list->next) {
		GdaMetaTableColumn *mcol;
		mcol = GDA_META_TABLE_COLUMN (list->data);
		gda_sql_builder_select_add_field (b, mcol->column_name, NULL, NULL);

		if (mcol->pkey) {
			/* ORDER BY */
			gda_sql_builder_select_order_by (b,
							 gda_sql_builder_add_id (b,
										 mcol->column_name),
							 FALSE, NULL);
		}

		/* export value */
		gchar *tmp;
		if (source->priv->id)
			tmp = g_strdup_printf ("%s@%s", source->priv->id, mcol->column_name);
		else
			tmp = g_strdup_printf ("%s@%s", (gchar*) tname, mcol->column_name);
		if (! source->priv->export_names)
			source->priv->export_names = g_array_new (FALSE, FALSE,
								  sizeof (gchar*));
		if (! source->priv->export_columns)
			source->priv->export_columns =
				g_hash_table_new_full (g_str_hash, g_str_equal,
						       g_free, NULL);
		g_array_append_val (source->priv->export_names, tmp);
		g_hash_table_insert (source->priv->export_columns, tmp,
				     GINT_TO_POINTER (i + 1));
		/*g_print ("EXPORT [%s]\n", tmp);*/

		if (source->priv->id)
			tmp = g_strdup_printf ("%s@%d", source->priv->id, i + 1);
		else
			tmp = g_strdup_printf ("%s@%d", (gchar*) tname, i + 1);
		g_array_append_val (source->priv->export_names, tmp);
		g_hash_table_insert (source->priv->export_columns, tmp,
				     GINT_TO_POINTER (i + 1));
		/*g_print ("EXPORT [%s]\n", tmp);*/
	}
	xmlFree (tname);

	/* linking */
	xmlNodePtr subnode;
	for (subnode = node->children; subnode; subnode = subnode->next) {
		if (!strcmp ((gchar*)subnode->name, "depend")) {
			xmlChar *fk_table, *id;
			GdaMetaTable *mlinked;
			
			fk_table = xmlGetProp (subnode, BAD_CAST "foreign_key_table");
			mlinked = get_meta_table (source, (gchar*) fk_table, error);
			if (!mlinked) {
				xmlFree (fk_table);
				g_object_unref (b);	
				return FALSE;
			}
			id = xmlGetProp (subnode, BAD_CAST "id");

			/* find foreign key to linked table */
			GdaMetaTableForeignKey *fk = NULL;
			for (list = mtable->fk_list; list; list = list->next) {
				if (GDA_META_TABLE_FOREIGN_KEY (list->data)->depend_on == GDA_META_DB_OBJECT (mlinked)) {
					fk = GDA_META_TABLE_FOREIGN_KEY (list->data);
					break;
				}
			}
			if (!fk) {
				g_set_error (error, 0, 0,
					     _("Could not find any foreign key to \"%s\""), (gchar*) fk_table);
				xmlFree (fk_table);
				if (id) xmlFree (id);
				g_object_unref (b);	
				return FALSE;
			}
			else if (fk->cols_nb <= 0) {
				g_set_error (error, 0, 0,
					     _("The fields involved in the foreign key to \"%s\" are not known"),
					     (gchar*) fk_table);
				xmlFree (fk_table);
				if (id) xmlFree (id);
				g_object_unref (b);	
				return FALSE;
			}
			else if (fk->cols_nb == 1) {
				gchar *tmp;
				GdaMetaTableColumn *col;
				col = GDA_META_TABLE_COLUMN (g_slist_nth_data (mlinked->columns, fk->fk_cols_array [0]));
				g_assert (col);
				const guint id1 = gda_sql_builder_add_id (b, fk->fk_names_array [0]);
				tmp = g_strdup_printf ("%s@%s", id ? (gchar*) id : (gchar*) fk_table,
						       fk->ref_pk_names_array [0]);
				const guint id2 = gda_sql_builder_add_param (b, tmp, col->gtype, FALSE);
				g_free (tmp);
				const guint id_cond = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ, id1, id2, 0);
				gda_sql_builder_set_where (b, id_cond);
			}
			else {
				TO_IMPLEMENT;
			}

			xmlFree (fk_table);
			if (id)
				xmlFree (id);
			break;
		}
	}

	source->priv->stmt = gda_sql_builder_get_statement (b, error);

	/* compute parameters */
	gda_statement_get_parameters (source->priv->stmt, &source->priv->params,
				      &source->priv->init_error);
	if (source->priv->params) {
		GSList *list;
		for (list = source->priv->params->holders; list; list = list->next)
			gda_holder_set_not_null (GDA_HOLDER (list->data), FALSE);

		g_signal_connect (source->priv->params, "holder-changed",
				  G_CALLBACK (params_changed_cb), source);
	}

	/*g_print ("SQL [%s]\n", gda_statement_to_sql (source->priv->stmt, NULL, NULL));*/
	g_object_unref (b);

	return source->priv->stmt ? TRUE : FALSE;
}


/**
 * data_source_to_xml_node
 */
xmlNodePtr
data_source_to_xml_node (DataSource *source)
{
	TO_IMPLEMENT;
	return NULL;
}

static gboolean
exec_end_timeout_cb (DataSource *source)
{
	GObject *obj;

	g_return_val_if_fail (source->priv->exec_id > 0, FALSE);

	g_clear_error (&source->priv->exec_error);
	obj = browser_connection_execution_get_result (source->priv->bcnc,
						       source->priv->exec_id,
						       NULL,
						       &source->priv->exec_error);
	if (obj) {
		if (GDA_IS_DATA_MODEL (obj)) {
			if (source->priv->model != GDA_DATA_MODEL (obj)) {
				if (source->priv->model)
					g_object_unref (source->priv->model);
				source->priv->model = GDA_DATA_MODEL (obj);
				g_object_set (source->priv->model, "auto-reset", FALSE, NULL);
			}
			else {
				gda_data_model_thaw (source->priv->model);
				gda_data_model_reset (source->priv->model);
			}
			/*gda_data_model_dump (source->priv->model, NULL);*/
		}
		else {
			g_object_unref (obj);
			g_set_error (&source->priv->exec_error, 0, 0,
				     _("Statement to execute is not a selection statement"));
		}

		source->priv->exec_id = 0;
		g_signal_emit (source, data_source_signals [EXEC_FINISHED], 0, source->priv->exec_error);
		if (source->priv->exec_again) {
			source->priv->exec_again = FALSE;
			data_source_execute (source, NULL);
		}
		return FALSE;
	}
	else if (source->priv->exec_error) {
		source->priv->exec_id = 0;
		g_signal_emit (source, data_source_signals [EXEC_FINISHED], 0, source->priv->exec_error);
		if (source->priv->exec_again) {
			source->priv->exec_again = FALSE;
			data_source_execute (source, NULL);
		}
		return FALSE;
	}
	else
		return TRUE; /* keep timer */
}

/**
 * data_source_execution_going_on
 */
gboolean
data_source_execution_going_on (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), FALSE);
	return source->priv->executing || (source->priv->exec_id > 0);
}

/**
 * data_source_get_import
 *
 * Returns: a pointer to a read-only #GdaSet, or %NULL (must not be modified)
 */
GdaSet *
data_source_get_import (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	return source->priv->params;
}

/**
 * data_source_set_external_import
 */
void
data_source_set_params (DataSource *source, GdaSet *params)
{
	gboolean bound = FALSE;
	g_return_if_fail (IS_DATA_SOURCE (source));
	g_return_if_fail (!params || GDA_IS_SET (params));

	if (source->priv->ext_params) {
		g_signal_handlers_disconnect_by_func (source->priv->ext_params,
						      G_CALLBACK (ext_params_changed_cb), source);
		g_object_unref (source->priv->ext_params);
		source->priv->ext_params = NULL;
	}

	if (source->priv->params) {
		GSList *list;
		for (list = source->priv->params->holders; list; list = list->next) {
			GdaHolder *holder = GDA_HOLDER (list->data);
			GdaHolder *bind = NULL;
			if (params)
				bind = gda_set_get_holder (params, gda_holder_get_id (holder));
			if (gda_holder_set_bind (holder, bind, NULL))
				bound = TRUE;
		}
	}

	if (params && bound) {
		source->priv->ext_params = g_object_ref (params);
		g_signal_connect (params, "holder-changed",
				  G_CALLBACK (ext_params_changed_cb), source);
	}
}

/**
 * data_source_get_export_template
 *
 * Returns: an array of strings, or %NULL
 */
GArray *
data_source_get_export_names (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	return source->priv->export_names;
}

/**
 * data_source_get_export_columns
 *
 * Returns: a #GHashTable where key is an export name and value is its column number (use GPOINTER_TO_INT)
 */
GHashTable *
data_source_get_export_columns (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	return source->priv->export_columns;	
}

/**
 * data_source_execute
 */
void
data_source_execute (DataSource *source, GError **error)
{
	GError *lerror = NULL;
	gboolean has_exec = TRUE;
	guint exec_id = 0;
	g_return_if_fail (IS_DATA_SOURCE (source));

	if (source->priv->exec_again)
		return;
	if (source->priv->executing || (source->priv->exec_id > 0)) {
		source->priv->exec_again = TRUE;
		return;
	}

	source->priv->executing = TRUE;
	if (! source->priv->stmt) {
		if (source->priv->init_error)
			g_propagate_error (error, source->priv->init_error);
		else
			g_set_error (error, 0, 0,
				     _("No SELECT statement to execute"));
	}

	if (source->priv->model) {
		if (source->priv->need_rerun) {
			/* freeze source->priv->model to avoid that it emits signals while being in the
			 * wrong thread */
			source->priv->need_rerun = FALSE;
			gda_data_model_freeze (source->priv->model);
			exec_id = browser_connection_rerun_select (source->priv->bcnc,
								   source->priv->model, &lerror);
		}
		else
			has_exec = FALSE;
	}
	else
		exec_id = browser_connection_execute_statement (source->priv->bcnc,
								source->priv->stmt,
								source->priv->params,
								GDA_STATEMENT_MODEL_RANDOM_ACCESS |
								GDA_STATEMENT_MODEL_ALLOW_NOPARAM,
								FALSE, &lerror);

	if (has_exec) {
		g_signal_emit (source, data_source_signals [EXEC_STARTED], 0);
		if (! exec_id) {
			gda_data_model_thaw (source->priv->model);
			gda_data_model_reset (source->priv->model);
			g_signal_emit (source, data_source_signals [EXEC_FINISHED], 0, lerror);
			g_propagate_error (error, lerror);
		}
		else {
			/* monitor the end of execution */
			source->priv->exec_id = exec_id;
			g_timeout_add (50, (GSourceFunc) exec_end_timeout_cb, source);
		}
	}
	source->priv->executing = FALSE;
}

/*
 * creates a new string where double underscores '__' are replaced by a single underscore '_'
 */
static gchar *
replace_double_underscores (const gchar *str)
{
        gchar **arr;
        gchar *ret;

        arr = g_strsplit (str, "__", 0);
        ret = g_strjoinv ("_", arr);
        g_strfreev (arr);

        return ret;
}

/**
 * data_source_create_grid
 *
 * Returns: a new #GdauiRawGrid, or %NULL if an error occurred
 */
GdauiRawGrid *
data_source_create_grid (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);

	if (! source->priv->model)
		return NULL;

	GdauiRawGrid *grid;
	grid = (GdauiRawGrid*) gdaui_raw_grid_new (source->priv->model);
	
	GList *columns, *list;
	columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (grid));
	for (list = columns; list; list = list->next) {
		/* reduce column's title */
		const gchar *title;
		GtkWidget *header;
		title = gtk_tree_view_column_get_title (GTK_TREE_VIEW_COLUMN (list->data));
		header = gtk_label_new ("");
		if (title) {
			gchar *tmp, *str;
			str = replace_double_underscores (title);
			tmp = g_markup_printf_escaped ("<small>%s</small>", str);
			g_free (str);
			gtk_label_set_markup (GTK_LABEL (header), tmp);
			g_free (tmp);
		}
		else
			gtk_label_set_markup (GTK_LABEL (header), "<small></small>");
		gtk_widget_show (header);
		gtk_tree_view_column_set_widget (GTK_TREE_VIEW_COLUMN (list->data),
						 header);
		
		/* reduce text's size */
		GList *renderers, *list2;
		renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (list->data));
		for (list2 = renderers; list2; list2 = list2->next) {
			if (GTK_IS_CELL_RENDERER_TEXT (list2->data))
				g_object_set ((GObject*) list2->data,
					      "scale", 0.7, NULL);
		}
		g_list_free (renderers);
	}
	
	/*if (!columns || !columns->next)*/
		gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (grid), FALSE);
	g_list_free (columns);
	return grid;
}

/**
 * data_source_get_title
 */
const gchar *
data_source_get_title (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);

	if (source->priv->title)
		return source->priv->title;
	else if (source->priv->id)
		return source->priv->id;
	else
		return _("No name");
}
