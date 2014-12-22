/*
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2006 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Daniel Espinosa <esodan@gmail.com>
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

#include <string.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-server-provider-private.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-util.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <libgda/sql-parser/gda-sql-parser.h>

#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-type.h>

/**
 * gda_server_provider_internal_get_parser:
 * @prov: a #GdaServerProvider
 *
 * This is a factory method to get a unique instance of a #GdaSqlParser object
 * for each #GdaServerProvider object
 * Don't unref it.
 *
 * Returns: (transfer none): a #GdaSqlParser
 */
GdaSqlParser *
gda_server_provider_internal_get_parser (GdaServerProvider *prov)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (prov), NULL);
	if (prov->priv->parser)
		return prov->priv->parser;
	prov->priv->parser = gda_server_provider_create_parser (prov, NULL);
	if (!prov->priv->parser)
		prov->priv->parser = gda_sql_parser_new ();
	return prov->priv->parser;
}

/**
 * gda_server_provider_perform_operation_default:
 * @provider: a #GdaServerProvider object
 * @cnc: (allow-none): a #GdaConnection object which will be used to perform an action, or %NULL
 * @op: a #GdaServerOperation object
 * @error: a place to store an error, or %NULL
 *
 * Performs the operation described by @op, using the SQL from the rendering of the operation
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_server_provider_perform_operation_default (GdaServerProvider *provider, GdaConnection *cnc,
					       GdaServerOperation *op, GError **error)
{
	gchar *sql;
	GdaBatch *batch;
	const GSList *list;
	gboolean retval = TRUE;

	sql = gda_server_provider_render_operation (provider, cnc, op, error);
	if (!sql)
		return FALSE;

	GdaSqlParser *parser;
	parser = gda_server_provider_internal_get_parser (provider); /* no ref held! */
	batch = gda_sql_parser_parse_string_as_batch (parser, sql, NULL, error);
	g_free (sql);
	if (!batch)
		return FALSE;

	for (list = gda_batch_get_statements (batch); list; list = list->next) {
		if (gda_connection_statement_execute_non_select (cnc, GDA_STATEMENT (list->data), NULL, NULL, error) == -1) {
			retval = FALSE;
			break;
		}
	}
	g_object_unref (batch);

	return retval;
}

/**
 * gda_server_provider_handler_use_default: (skip)
 * @provider: a server provider
 * @type: a #GType
 *
 * Reserved to database provider's implementations. This method defines a default data handler for
 * @provider, and returns that #GdaDataHandler.
 *
 * Returns: (transfer none): a #GdaDataHandler, or %NULL
 *
 * Since: 5.2
 */
GdaDataHandler *
gda_server_provider_handler_use_default (GdaServerProvider *provider, GType type)
{
	GdaDataHandler *dh = NULL;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_UINT64) ||
	    (type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == GDA_TYPE_USHORT) ||
	    (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) ||
	    (type == G_TYPE_UINT) ||
	    (type == G_TYPE_LONG) ||
	    (type == G_TYPE_ULONG)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_numerical_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT64, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UINT64, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DOUBLE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_NUMERIC, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_FLOAT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_SHORT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_USHORT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_CHAR, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UCHAR, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UINT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_ULONG, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_LONG, NULL);
			g_object_unref (dh);
		}
	}
        else if ((type == GDA_TYPE_BINARY) ||
		 (type == GDA_TYPE_BLOB)) {
		/* no default binary data handler, it's too database specific */
		dh = NULL;
	}
        else if (type == G_TYPE_BOOLEAN) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_boolean_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_BOOLEAN, NULL);
			g_object_unref (dh);
		}
	}
	else if ((type == GDA_TYPE_TIME) ||
		 (type == GDA_TYPE_TIMESTAMP) ||
		 (type == G_TYPE_DATE)) {
		/* no default time related data handler, it's too database specific */
		dh = NULL;
	}
	else if (type == G_TYPE_STRING) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_string_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_STRING, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == G_TYPE_GTYPE) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_type_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_GTYPE, NULL);
			g_object_unref (dh);
		}
	}

	return dh;
}

/**
 * gda_server_provider_get_data_handler_default:
 * @provider: a server provider.
 * @cnc: (allow-none): a #GdaConnection object, or %NULL
 * @type: a #GType
 * @dbms_type: a DBMS type definition
 *
 * Provides the implementation when the default Libgda's data handlers must be used
 * 
 * Returns: (transfer none): a #GdaDataHandler, or %NULL
 *
 * Deprecated: 5.2: use gda_server_provider_handler_use_default() instead
 */
GdaDataHandler *
gda_server_provider_get_data_handler_default (GdaServerProvider *provider, G_GNUC_UNUSED GdaConnection *cnc,
					      GType type, G_GNUC_UNUSED const gchar *dbms_type)
{
	return gda_server_provider_handler_use_default (provider, type);
}

static gboolean
param_to_null_foreach (GdaSqlAnyPart *part, G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
	if (part->type == GDA_SQL_ANY_EXPR) {
		GdaSqlExpr *expr = (GdaSqlExpr*) part;
		if (expr->param_spec) {
			GType type = expr->param_spec->g_type;
			gda_sql_param_spec_free (expr->param_spec);
			expr->param_spec = NULL;

			if (!expr->value) {
				if (type != GDA_TYPE_NULL)
					expr->value = gda_value_new_from_string ("0", type);
				else
					g_value_set_int ((expr->value = gda_value_new (G_TYPE_INT)), 0);
			}
		}
	}
	return TRUE;
}

/**
 * gda_select_alter_select_for_empty:
 * @stmt: a SELECT #GdaStatement
 * @error: (allow-none): a place to store errors, or %NULL
 *
 * Creates a new #GdaStatement, selecting the same data as @stmt, but which always returns an
 * empty (no row) data model. This is use dy database providers' implementations.
 *
 * Returns: (transfer full): a new #GdaStatement
 */
GdaStatement *
gda_select_alter_select_for_empty (GdaStatement *stmt, G_GNUC_UNUSED GError **error)
{
	GdaStatement *estmt;
	GdaSqlStatement *sqlst;
	GdaSqlStatementSelect *stsel;

	g_assert (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT);
	g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
	g_assert (sqlst);

	if (sqlst->sql) {
		g_free (sqlst->sql);
		sqlst->sql = NULL;
	}
	stsel = (GdaSqlStatementSelect*) sqlst->contents;

	/* set the WHERE condition to "1 = 0" */
	GdaSqlExpr *expr, *cond = stsel->where_cond;
	GdaSqlOperation *op;
	if (cond)
		gda_sql_expr_free (cond);
	cond = gda_sql_expr_new (GDA_SQL_ANY_PART (stsel));
	stsel->where_cond = cond;
	op = gda_sql_operation_new (GDA_SQL_ANY_PART (cond));
	cond->cond = op;
	op->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;
	expr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
	op->operands = g_slist_prepend (NULL, expr);
	g_value_set_int ((expr->value = gda_value_new (G_TYPE_INT)), 1);
	expr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
	op->operands = g_slist_prepend (op->operands, expr);
	g_value_set_int ((expr->value = gda_value_new (G_TYPE_INT)), 0);

	/* replace any selected field which has a parameter with NULL */
	gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stsel), (GdaSqlForeachFunc) param_to_null_foreach,
				  NULL, NULL);

	/* create new statement */
	estmt = g_object_new (GDA_TYPE_STATEMENT, "structure", sqlst, NULL);
	gda_sql_statement_free (sqlst);
	return estmt;
}

/**
 * gda_server_provider_handler_find:
 * @prov: a #GdaServerProvider
 * @cnc: (allow-none): a #GdaConnection
 * @g_type: a #GType
 * @dbms_type: (allow-none): a database type
 *
 * Reserved to database provider's implementations: get the #GdaDataHandler associated to @prov
 * for connection @cnc. You probably want to use gda_server_provider_get_data_handler_g_type().
 *
 * Returns: (transfer none): the requested #GdaDataHandler, or %NULL if none found
 */
GdaDataHandler *
gda_server_provider_handler_find (GdaServerProvider *prov, GdaConnection *cnc, 
				  GType g_type, const gchar *dbms_type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (prov), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	GdaDataHandler *dh;
	GdaServerProviderHandlerInfo info;

	info.cnc = cnc;
	info.g_type = g_type;
	info.dbms_type = (gchar *) dbms_type;
	dh = g_hash_table_lookup (prov->priv->data_handlers, &info);
	if (!dh) {
		/* try without the connection specification */
		info.cnc = NULL;
		dh = g_hash_table_lookup (prov->priv->data_handlers, &info);
	}

	return dh;
}

void
gda_server_provider_handler_declare (GdaServerProvider *prov, GdaDataHandler *dh,
				     GdaConnection *cnc, 
				     GType g_type, const gchar *dbms_type)
{
	GdaServerProviderHandlerInfo *info;
	g_return_if_fail (GDA_IS_SERVER_PROVIDER (prov));
	g_return_if_fail (GDA_IS_DATA_HANDLER (dh));
	
	info = g_new (GdaServerProviderHandlerInfo, 1);
	info->cnc = cnc;
	info->g_type = g_type;
	info->dbms_type = dbms_type ? g_strdup (dbms_type) : NULL;

	g_hash_table_insert (prov->priv->data_handlers, info, dh);
	g_object_ref (dh);
}

static gboolean
handlers_clear_for_cnc_fh (GdaServerProviderHandlerInfo *key, GdaDataHandler *value, GdaConnection *cnc)
{
	return (key->cnc == cnc) ? TRUE : FALSE;
}

/*
 * Removes any #GdaServerProviderHandlerInfo associated to @cnc */
void
_gda_server_provider_handlers_clear_for_cnc (GdaServerProvider *prov, GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_SERVER_PROVIDER (prov));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_hash_table_foreach_remove (prov->priv->data_handlers, (GHRFunc) handlers_clear_for_cnc_fh, cnc);
}

/**
 * gda_server_provider_find_file:
 * @prov: a #GdaServerProvider
 * @inst_dir: directory where @prov is installed
 * @filename: name of the file to find
 *
 * Finds the location of a @filename. This function should only be used by database provider's
 * implementations
 *
 * Returns: (transfer full): the complete path to @filename, or %NULL if not found
 */
gchar *
gda_server_provider_find_file (GdaServerProvider *prov, const gchar *inst_dir, const gchar *filename)
{
	gchar *file = NULL;
	const gchar *dirname;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (prov), NULL);
	dirname = g_object_get_data (G_OBJECT (prov), "GDA_PROVIDER_DIR");
	if (dirname)
		file = g_build_filename (dirname, filename, NULL);

	if (!file ||
	    (file && !g_file_test (file, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))) {
		g_free (file);
		file = g_build_filename (inst_dir, filename, NULL);
		if (! g_file_test (file, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			g_free (file);
			file = NULL;
			if (dirname) {
				/* look in the parent dir, to handle the case where the lib is in a .libs dir */
				file = g_build_filename (dirname, "..", filename, NULL);
				if (! g_file_test (file, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
					g_free (file);
					file = NULL;
				}
			}
		}
	}

	return file;
}

/**
 * gda_server_provider_load_file_contents:
 * @inst_dir: directory where the database provider has been installed
 * @data_dir: DATA directory to look for ($prefix/share)
 * @filename: name of the file to load
 *
 * Loads and returns the contents of @filename, which is searched in several places
 * This function should only be used by database provider's
 * implementations
 *
 * Returns: (transfer full): a new string containing @filename's contents, or %NULL if not found or if an error occurred
 */
gchar *
gda_server_provider_load_file_contents (const gchar *inst_dir, const gchar *data_dir, const gchar *filename)
{
	gchar *contents, *file;

	file = g_build_filename (inst_dir, filename, NULL);

	if (g_file_get_contents (file, &contents, NULL, NULL))
		goto theend;

	g_free (file);
	file = g_build_filename (inst_dir, "..", filename, NULL);
	if (g_file_get_contents (file, &contents, NULL, NULL))
		goto theend;

	g_free (file);
	file = g_build_filename (data_dir, filename, NULL);
	if (g_file_get_contents (file, &contents, NULL, NULL))
		goto theend;
	
	g_free (file);
	file = g_build_filename (inst_dir, "..", "..", "..", "share", "libgda-6.0", filename, NULL);
	if (g_file_get_contents (file, &contents, NULL, NULL))
		goto theend;
	contents = NULL;

 theend:
	g_free (file);
	return contents;
}

/**
 * gda_server_provider_load_resource_contents:
 * @prov_name: the provider's name
 * @resource: the name of the resource to load
 *
 * Loads and returns the contents of the specified resource.
 * This function should only be used by database provider's implementations
 *
 * Returns: (transfer full): a new string containing the resource's contents, or %NULL if not found or if an error occurred
 */
gchar *
gda_server_provider_load_resource_contents (const gchar *prov_name, const gchar *resource)
{
	g_return_val_if_fail (prov_name, NULL);
	g_return_val_if_fail (resource, NULL);

	gchar *rname;
	rname = g_strdup_printf ("/spec/%s/%s", prov_name, resource);
	g_print ("Using resource %s\n", rname);

	GBytes *bytes;
	bytes = g_resources_lookup_data (rname, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	g_free (rname);
	if (!bytes)
		return NULL;

	gchar *retval;
	retval = g_strdup ((const gchar*) g_bytes_get_data (bytes, NULL));
	g_bytes_unref (bytes);
	return retval;
}
