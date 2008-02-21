/* GDA Library
 * Copyright (C) 2008 The GNOME Foundation.
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
#include <libgda/gda-easy.h>
#include <libgda/gda-server-provider.h>
#include <sql-parser/gda-sql-parser.h>
#include <libgda/gda-config.h>

static GdaSqlParser *internal_parser = NULL;

/**
 * gda_prepare_create_database
 * @provider: the database provider to use
 * @db_name: the name of the database to create, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #GdaServerOperation object which contains the specifications required
 * to create a database. Once these specifications provided, use 
 * gda_perform_create_database() to perform the database creation.
 *
 * If @db_name is left %NULL, then the name of the database to create will have to be set in the
 * returned #GdaServerOperation using gda_server_operation_set_value_at().
 *
 * Returns: new #GdaServerOperation object, or %NULL if the provider does not support database
 * creation
 */
GdaServerOperation *
gda_prepare_create_database (const gchar *provider, const gchar *db_name, GError **error)
{
	GdaServerProvider *prov;

	g_return_val_if_fail (provider && *provider, NULL);
	g_return_val_if_fail (db_name && *db_name, NULL);

	prov = gda_config_get_provider_object (provider, error);
	if (prov) {
		GdaServerOperation *op;
		op = gda_server_provider_create_operation (prov, NULL, GDA_SERVER_OPERATION_CREATE_DB, 
							   NULL, error);
		if (op) {
			g_object_set_data (G_OBJECT (op), "_gda_provider_obj", prov);
			if (db_name)
				gda_server_operation_set_value_at (op, db_name, NULL, "/DB_DEF_P/DB_NAME");
		}
		return op;
	}
	else
		return NULL;
}

/**
 * gda_perform_create_database
 * @op: a #GdaServerOperation object obtained using gda_prepare_create_database()
 * @error: a place to store en error, or %NULL
 *
 * Creates a new database using the specifications in @op, which must have been obtained using
 * gda_prepare_create_database ()
 *
 * Returns: TRUE if no error occurred and the database has been created
 */
gboolean
gda_perform_create_database (GdaServerOperation *op, GError **error)
{
	GdaServerProvider *provider;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);

	provider = g_object_get_data (G_OBJECT (op), "_gda_provider_obj");
	if (provider) 
		return gda_server_provider_perform_operation (provider, NULL, op, error);
	else {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_NOT_FOUND_ERROR, 
			     _("Could not find operation's associated provider, "
			       "did you use gda_prepare_create_database() ?")); 
		return FALSE;
	}
}

/**
 * gda_prepare_drop_database
 * @provider: the database provider to use
 * @db_name: the name of the database to drop, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #GdaServerOperation object which contains the specifications required
 * to drop a database. Once these specifications provided, use 
 * gda_perform_drop_database() to perform the database creation.
 *
 * If @db_name is left %NULL, then the name of the database to drop will have to be set in the
 * returned #GdaServerOperation using gda_server_operation_set_value_at().
 *
 * Returns: new #GdaServerOperation object, or %NULL if the provider does not support database
 * destruction
 */
GdaServerOperation *
gda_prepare_drop_database (const gchar *provider, const gchar *db_name, GError **error)
{
	GdaServerProvider *prov;
	
	g_return_val_if_fail (provider && *provider, NULL);
	g_return_val_if_fail (db_name && *db_name, NULL);

	prov = gda_config_get_provider_object (provider, error);
	if (prov) {
		GdaServerOperation *op;
		op = gda_server_provider_create_operation (prov, NULL, GDA_SERVER_OPERATION_DROP_DB, 
							   NULL, error);
		if (op) {
			g_object_set_data (G_OBJECT (op), "_gda_provider_obj", prov);
			if (db_name)
				gda_server_operation_set_value_at (op, db_name, NULL, "/DB_DESC_P/DB_NAME");
		}
		return op;
	}
	else
		return NULL;
}

/**
 * gda_perform_drop_database
 * @op: a #GdaServerOperation object obtained using gda_prepare_drop_database()
 * @error: a place to store en error, or %NULL
 *
 * Destroys an existing database using the specifications in @op,  which must have been obtained using
 * gda_prepare_drop_database ()
 *
 * Returns: TRUE if no error occurred and the database has been destroyed
 */
gboolean
gda_perform_drop_database (GdaServerOperation *op, GError **error)
{
	GdaServerProvider *provider;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);

	provider = g_object_get_data (G_OBJECT (op), "_gda_provider_obj");
	if (provider) 
		return gda_server_provider_perform_operation (provider, NULL, op, error);
	else {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_NOT_FOUND_ERROR, 
			     _("Could not find operation's associated provider, "
			       "did you use gda_prepare_drop_database() ?")); 
		return FALSE;
	}
}

/**
 * gda_execute_select_command
 * @cnc: an opened connection
 * @sql: a query statament must begin with "SELECT"
 * @error: a place to store errors, or %NULL
 * 
 * Execute a SQL SELECT command over an opened connection.
 *
 * Return: a new #GdaDataModel if succesfull, NULL otherwise
 */
GdaDataModel *          
gda_execute_select_command (GdaConnection *cnc, const gchar *sql, GError **error)
{
	GdaStatement *stmt;
	GdaDataModel *model;
    
	g_return_val_if_fail (sql != NULL 
			      || GDA_IS_CONNECTION (cnc) 
			      || !gda_connection_is_opened (cnc)
			      || g_str_has_prefix (sql, "SELECT"),
			      NULL);
    
	if (!internal_parser)
		internal_parser = gda_sql_parser_new ();

	stmt = gda_sql_parser_parse_string (internal_parser, sql, NULL, error);
	if (!stmt) 
		return NULL;
	model = gda_connection_statement_execute_select (cnc, stmt, NULL, error);
	g_object_unref (stmt);

	return model;
}

/**
 * gda_execute_sql_command
 * @cnc: an opened connection
 * @sql: a query statament must begin with "SELECT"
 * @error: a place to store errors, or %NULL
 *
 * This is a convenient function to execute a SQL command over the opened connection.
 *
 * Returns: the number of rows affected or -1.
 */
gint
gda_execute_non_select_command (GdaConnection *cnc, const gchar *sql, GError **error)
{
	GdaStatement *stmt;
	gint retval;

	g_return_val_if_fail (sql != NULL 
			      || GDA_IS_CONNECTION (cnc) 
			      || !gda_connection_is_opened (cnc), -1);
    
	if (!internal_parser)
		internal_parser = gda_sql_parser_new ();

	stmt = gda_sql_parser_parse_string (internal_parser, sql, NULL, error);
	if (!stmt) 
		return -1;
    
	retval = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, error);
	g_object_unref (stmt);
	return retval;
}

/**
 * gda_create_table
 * @cnc: an opened connection
 * @table_name:
 * @num_columns
 * @error: a place to store errors, or %NULL
 * @...: pairs of column name and #GType, finish with NULL
 * 
 * Create a Table over an opened connection using a pair list of colum name and 
 * GType as arguments, you need to finish the list using NULL.
 *
 * This is just a convenient function to create tables quickly, 
 * using defaults for the provider and converting the #GType passed to the corresponding 
 * type in the provider; to use a custom type or more advanced characteristics in a 
 * specific provider use the #GdaServerOperation framework.
 *
 * Returns: TRUE if the table was created; FALSE and set @error otherwise
 */
gboolean
gda_create_table (GdaConnection *cnc, const gchar *table_name, GError **error, ...)
{
	GdaServerOperation *op;
	GdaServerProvider *server;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);
	
	server = gda_connection_get_provider_obj(cnc);
	
	op = gda_server_provider_create_operation (server, cnc, GDA_SERVER_OPERATION_CREATE_TABLE, NULL, error);
	if (GDA_IS_SERVER_OPERATION (op)) {
		va_list  args;
		gchar   *arg;
		GType    type;
		gchar   *dbms_type;
		xmlDocPtr parameters;
		xmlNodePtr root;
		xmlNodePtr table, op_data, array_data, array_row, array_value;
		
		if (table_name == NULL) {
			g_set_error (error, GDA_GENERAL_ERROR, GDA_GENERAL_OBJECT_NAME_ERROR, 
				     _("Couldn't create table with a NULL string"));
			return FALSE;    
		}
		
	
		/* Initation of the xmlDoc */
		parameters = xmlNewDoc ((xmlChar*)"1.0");
		
		root = xmlNewDocNode (parameters, NULL, (xmlChar*)"serv_op_data", NULL);
		xmlDocSetRootElement (parameters, root);
		table = xmlNewChild (root, NULL, (xmlChar*)"op_data", (xmlChar*)table_name);
		xmlSetProp(table, (xmlChar*)"path", (xmlChar*)"/TABLE_DEF_P/TABLE_NAME");

		op_data = xmlNewChild (root, NULL, (xmlChar*)"op_data", NULL);
		xmlSetProp(op_data, (xmlChar*)"path", (xmlChar*)"/FIELDS_A");
		array_data = xmlNewChild (op_data, NULL, (xmlChar*)"gda_array_data", NULL);
			
		va_start (args, error);
		type = 0;
		arg = NULL;
		
		while ((arg = va_arg (args, gchar*))) {
			type = va_arg (args, GType);
			if (type == 0) {
				g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_INCORRECT_VALUE_ERROR, 
					    _("Error the number of arguments are incorrect; couldn't create the table"));
				g_object_unref (op);
				xmlFreeDoc(parameters);
				return FALSE;
			}
			
			dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (server, 
											 cnc, type);
			array_row = xmlNewChild (array_data, NULL, (xmlChar*)"gda_array_row", NULL);
			array_value = xmlNewChild (array_row, NULL, (xmlChar*)"gda_array_value", (xmlChar*)arg);
			xmlSetProp(array_value, (xmlChar*)"colid", (xmlChar*)"COLUMN_NAME");
		
			array_value = xmlNewChild(array_row, NULL, (xmlChar*)"gda_array_value", (xmlChar*)dbms_type);
			xmlSetProp(array_value, (xmlChar*)"colid", (xmlChar*)"COLUMN_TYPE");
			
		}
		
		va_end(args);
		
		if (!gda_server_operation_load_data_from_xml (op, root, error)) {
			g_set_error (error, GDA_GENERAL_ERROR, GDA_GENERAL_OPERATION_ERROR, 
				     _("The XML operation doesn't exist or could't be loaded"));
			g_object_unref (op);
			xmlFreeDoc(parameters);
			return FALSE;
		}
		else {
			if (!gda_server_provider_perform_operation (server, cnc, op, error)) {
				g_object_unref (op);
				xmlFreeDoc(parameters);
				return FALSE;
			}
		}
		
		g_object_unref (op);
		xmlFreeDoc(parameters);
	}
	
	return TRUE;
}

/**
 * gda_drop_table
 * @cnc: an opened connection
 * @table_name:
 * @error: a place to store errors, or %NULL
 * 
 * This is just a convenient function to drop a table in an opened connection.
 *
 * Returns: TRUE if the table was dropped
 */
gboolean
gda_drop_table (GdaConnection *cnc, const gchar *table_name, GError **error)
{
	GdaServerOperation *op;
	GdaServerProvider *server;
	gboolean retval = TRUE;

	server = gda_connection_get_provider_obj (cnc);
	
	op = gda_server_provider_create_operation (server, cnc, 
						   GDA_SERVER_OPERATION_DROP_TABLE, NULL, error);
	
	if (GDA_IS_SERVER_OPERATION (op)) {
		xmlDocPtr parameters;
		xmlNodePtr root;
		xmlNodePtr table;
		
		g_return_val_if_fail (table_name != NULL 
				      || GDA_IS_CONNECTION (cnc) 
				      || !gda_connection_is_opened (cnc), FALSE);
    
		parameters = xmlNewDoc ((xmlChar*)"1.0");
		root = xmlNewDocNode (parameters, NULL, (xmlChar*)"serv_op_data", NULL);
		xmlDocSetRootElement (parameters, root);
		table = xmlNewChild (root, NULL, (xmlChar*)"op_data", (xmlChar*)table_name);
		xmlSetProp(table, (xmlChar*)"path", (xmlChar*)"/TABLE_DESC_P/TABLE_NAME");
	    
		if (!gda_server_operation_load_data_from_xml (op, root, error)) {
			/* error */
			g_object_unref (op);
			xmlFreeDoc(parameters);
			retval = FALSE;
		}
		else {
			if (gda_server_provider_perform_operation (server, cnc, op, error))
				/* error */
				g_object_unref (op);
		        xmlFreeDoc(parameters);
			return FALSE;
		}
		g_object_unref (op);
		xmlFreeDoc(parameters);
	}
	else {
		g_message("The Server doesn't support the DROP TABLE operation!\n\n");
		retval = FALSE;
	}
    
	return retval;
}

static guint
gtype_hash (gconstpointer key)
{
        return (guint) key;
}

static gboolean 
gtype_equal (gconstpointer a, gconstpointer b)
{
        return (GType) a == (GType) b ? TRUE : FALSE;
}

/**
 * gda_get_default_handler
 * @for_type: a #GType type
 * 
 * Obtain a pointer to a #GdaDataHandler which can manage #GValue values of type @for_type
 *
 * The returned pointer is %NULL if there is no default data handler available for the @for_type data type
 *
 * Returns: a #GdaDataHandler which must not be modified or destroyed.
 */
GdaDataHandler *
gda_get_default_handler (GType for_type)
{
	static GHashTable *hash = NULL;
	GdaDataHandler *dh;

	if (!hash) {
		hash = g_hash_table_new_full (gtype_hash, gtype_equal,
					      NULL, (GDestroyNotify) g_object_unref);

                g_hash_table_insert (hash, (gpointer) G_TYPE_INT64, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_UINT64, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_BINARY, gda_handler_bin_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_BLOB, gda_handler_bin_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_BOOLEAN, gda_handler_boolean_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_DATE, gda_handler_time_new_no_locale ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_DOUBLE, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_INT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_NUMERIC, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_FLOAT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_SHORT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_USHORT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_STRING, gda_handler_string_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_TIME, gda_handler_time_new_no_locale ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_TIMESTAMP, gda_handler_time_new_no_locale ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_CHAR, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_UCHAR, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_ULONG, gda_handler_type_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_UINT, gda_handler_numerical_new ());
	}
	
	dh = g_hash_table_lookup (hash, (gpointer) for_type);
	return dh;
}

/* Auxiliary struct to save and get the varian arguments */
typedef struct {
	gchar  *column_name;
	GValue *value;
} GdaValueArgument;

/**
 * gda_insert_row_into_table_from_string
 * @cnc: an opened connection
 * @table_name:
 * @error: a place to store errors, or %NULL
 * @...: a list of strings to be converted as value, finished by %NULL
 * 
 * This is just a convenient function to insert a row with the values given as arguments.
 * The values must be strings that could be converted to the type in the corresponding
 * column. Finish the list with NULL.
 * 
 * The arguments must be pairs of column name followed by his value.
 *
 * The SQL command is like: 
 * INSERT INTO table_name (column1, column2, ...) VALUES (value1, value2, ...)
 *
 * Returns: TRUE if no error occurred, and FALSE and set error otherwise
 */
gboolean
gda_insert_row_into_table_from_string (GdaConnection *cnc, const gchar *table_name, GError **error, ...)
{        
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	TO_IMPLEMENT;
	return FALSE;
}

/**
 * gda_insert_row_into_table
 * @cnc: an opened connection
 * @table_name:
 * @error: a place to store errors, or %NULL
 * @...: a list of string/@GValue pairs where the string is the name of the column
 * followed by its @GValue to set in the insert operation, finished by %NULL
 * 
 * This is just a convenient function to insert a row with the values given as argument.
 * The values must correspond with the GType of the column to set, otherwise throw to 
 * an error. Finish the list with NULL.
 * 
 * The arguments must be pairs of column name followed by his value.
 * 
 * Returns: TRUE if no error occurred, and FALSE and set error otherwise
 */
gboolean
gda_insert_row_into_table (GdaConnection *cnc, const gchar *table_name, GError **error, ...)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	TO_IMPLEMENT;
	return FALSE;
}

/**
 * gda_delete_row_from_table
 * @cnc: an opened connection
 * @table_name:
 * @condition_column_name: the name of the column to used in the WHERE condition clause
 * @condition: a GValue to used to find the row to be deleted 
 * @error: a place to store errors, or %NULL
 *
 * This is just a convenient function to delete the row fitting the given condition
 * from the given table.
 *
 * @condition must be a valid GValue and must correspond with the GType of the column to use
 * in the WHERE clause.
 *
 * The SQL command is like: DELETE FROM table_name WHERE contition_column_name = condition
 *
 * Returns: TRUE if no error occurred, and FALSE and set error otherwise
 */
gboolean              
gda_delete_row_from_table (GdaConnection *cnc, const gchar *table_name, const gchar *condition_column_name, 
			   const GValue *condition, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	TO_IMPLEMENT;
	return FALSE;
}

/**
 * gda_update_value_in_table
 * @cnc: an opened connection
 * @table_name:
 * @search_for_column: the name of the column to used in the WHERE condition clause
 * @condition: a GValue to used to find the value to be updated; it must correspond with the GType
 * of the column used to search
 * @column_name: the column containing the value to be updated
 * @new_value: the new value to update to; the @GValue must correspond with the GType of the column to update
 * @error: a place to store errors, or %NULL
 * 
 * This is just a convenient function to update values in a table on a given column where
 * the row is fitting the given condition.
 *
 * The SQL command is like: UPDATE INTO table_name SET column_name = new_value WHERE search_for_column = condition
 *
 * Returns: TRUE if no error occurred
 */
gboolean              
gda_update_value_in_table (GdaConnection *cnc, 
			   const gchar *table_name, const gchar *search_for_column, 
			   const GValue *condition, 
			   const gchar *column_name, const GValue *new_value, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	TO_IMPLEMENT;
	return FALSE;
}

/**
 * gda_update_values_in_table
 * @cnc: an opened connection
 * @table_name: the name of the table where the update will be done
 * @condition_column_name: the name of the column to used in the WHERE condition clause
 * @condition: a GValue to used to find the values to be updated; it must correspond with the
 * column's @GType
 * @error: a place to store errors, or %NULL
 * @...: a list of string/@GValue pairs where the string is the name of the column to be 
 * updated followed by the new @GValue to set, finished by %NULL
 * 
 * This is just a convenient function to update values in a table on a given column where
 * the row is fitting the given condition.
 *
 * The SQL command is like: 
 * UPDATE INTO table_name SET column1 = new_value1, column2 = new_value2 ... WHERE condition_column_name = condition
 *
 * Returns: TRUE if no error occurred
 */
gboolean              
gda_update_values_in_table (GdaConnection *cnc, const gchar *table_name, 
			    const gchar *condition_column_name, const GValue *condition, 
			    GError **error, ...)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	TO_IMPLEMENT;
	return FALSE;
}
