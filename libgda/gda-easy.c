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

static GStaticMutex parser_mutex = G_STATIC_MUTEX_INIT;
static GdaSqlParser *internal_parser = NULL;

/* module error */
GQuark gda_easy_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_easy_error");
	return quark;
}

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

	prov = gda_config_get_provider (provider, error);
	if (prov) {
		GdaServerOperation *op;
		op = gda_server_provider_create_operation (prov, NULL, GDA_SERVER_OPERATION_CREATE_DB, 
							   NULL, error);
		if (op) {
			g_object_set_data_full (G_OBJECT (op), "_gda_provider_obj", g_object_ref (prov), g_object_unref);
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
 * @provider: the database provider to use, or %NULL if @op has been created using gda_prepare_create_database()
 * @op: a #GdaServerOperation object obtained using gda_prepare_create_database()
 * @error: a place to store en error, or %NULL
 *
 * Creates a new database using the specifications in @op. @op can be obtained using
 * gda_server_provider_create_operation(), or gda_prepare_create_database().
 *
 * Returns: TRUE if no error occurred and the database has been created
 */
gboolean
gda_perform_create_database (const gchar *provider, GdaServerOperation *op, GError **error)
{
	GdaServerProvider *prov;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	if (provider)
		prov = gda_config_get_provider (provider, error);
	else
		prov = g_object_get_data (G_OBJECT (op), "_gda_provider_obj");
	if (prov) 
		return gda_server_provider_perform_operation (prov, NULL, op, error);
	else {
		g_warning ("Could not find operation's associated provider, "
			   "did you use gda_prepare_create_database() ?");
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

	prov = gda_config_get_provider (provider, error);
	if (prov) {
		GdaServerOperation *op;
		op = gda_server_provider_create_operation (prov, NULL, GDA_SERVER_OPERATION_DROP_DB, 
							   NULL, error);
		if (op) {
			g_object_set_data_full (G_OBJECT (op), "_gda_provider_obj", g_object_ref (prov), g_object_unref);
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
 * @provider: the database provider to use, or %NULL if @op has been created using gda_prepare_drop_database()
 * @op: a #GdaServerOperation object obtained using gda_prepare_drop_database()
 * @error: a place to store en error, or %NULL
 *
 * Destroys an existing database using the specifications in @op.  @op can be obtained using
 * gda_server_provider_create_operation(), or gda_prepare_drop_database().
 *
 * Returns: TRUE if no error occurred and the database has been destroyed
 */
gboolean
gda_perform_drop_database (const gchar *provider, GdaServerOperation *op, GError **error)
{
	GdaServerProvider *prov;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	if (provider)
		prov = gda_config_get_provider (provider, error);
	else
		prov = g_object_get_data (G_OBJECT (op), "_gda_provider_obj");
	if (prov) 
		return gda_server_provider_perform_operation (prov, NULL, op, error);
	else {
		g_warning ("Could not find operation's associated provider, "
			   "did you use gda_prepare_drop_database() ?"); 
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
    
	g_static_mutex_lock (&parser_mutex);
	if (!internal_parser)
		internal_parser = gda_sql_parser_new ();
	g_static_mutex_unlock (&parser_mutex);

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
    
	g_static_mutex_lock (&parser_mutex);
	if (!internal_parser)
		internal_parser = gda_sql_parser_new ();
	g_static_mutex_unlock (&parser_mutex);

	stmt = gda_sql_parser_parse_string (internal_parser, sql, NULL, error);
	if (!stmt) 
		return -1;
    
	retval = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, error);
	g_object_unref (stmt);
	return retval;
}

/**
 * gda_prepare_create_table
 * @cnc: an opened connection
 * @table_name:
 * @num_columns
 * @error: a place to store errors, or %NULL
 * @...: group of three arguments for column's name, column's #GType 
 * and a #GdaEasyCreateTableFlag flag, finished with NULL
 * 
 * Add more arguments if the flag needs then: 
 *
 * GDA_EASY_CREATE_TABLE_FKEY_FLAG:
 * <itemizedlist>
 *   <listitem><para>string with the table's name referenced</para></listitem>
 *   <listitem><para>an integer with the number pairs "local_field", "referenced_field" 
 *   used in the reference</para></listitem>
 *   <listitem><para>Pairs of "local_field", "referenced_field" to use, must match
 *    the number specified above.</para></listitem>
 *   <listitem><para>a string with the action for ON DELETE; can be: "RESTRICT", "CASCADE", 
 *    "NO ACTION", "SET NULL" and "SET DEFAULT". Example: "ON UPDATE CASCADE".</para></listitem>
 *   <listitem><para>a string with the action for ON UPDATE (see above).</para></listitem>
 * </itemizedlist> 
 * 
 * Create a #GdaServerOperation object using an opened connection, taking three 
 * arguments, a colum's name the column's GType and #GdaEasyCreateTableFlag 
 * flag, you need to finish the list using NULL.
 *
 * You'll be able to modify the #GdaServerOperation object to add custom options
 * to the operation. When finish call #gda_perform_create_table 
 * or #gda_server_operation_perform_operation
 * in order to execute the operation.
 * 
 * Returns: a #GdaServerOperation if no errors; NULL and set @error otherwise
 */
GdaServerOperation*
gda_prepare_create_table (GdaConnection *cnc, const gchar *table_name, GError **error, ...)
{
	GdaServerOperation *op;
	GdaServerProvider *server;
		
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);	
	
	server = gda_connection_get_provider (cnc);
	
	if (!table_name) {
		g_set_error (error, GDA_EASY_ERROR, GDA_EASY_OBJECT_NAME_ERROR, 
			     _("Unspecified table name"));
		return NULL;
	}
	
	if (gda_server_provider_supports_operation (server, cnc, GDA_SERVER_OPERATION_CREATE_TABLE, NULL)) {	
		va_list  args;
		gchar   *arg;
		GType    type;
		gchar   *dbms_type;
		GdaEasyCreateTableFlag flag;
		gint i;
		gint refs;

		op = gda_server_provider_create_operation (server, cnc, 
							   GDA_SERVER_OPERATION_CREATE_TABLE, NULL, error);
		gda_server_operation_set_value_at (op, table_name, error, "/TABLE_DEF_P/TABLE_NAME");
				
		va_start (args, error);
		type = 0;
		arg = NULL;
		i = 0;
		refs = -1;
		
		while ((arg = va_arg (args, gchar*))) {
			/* First argument for Column's name */			
			gda_server_operation_set_value_at (op, arg, error, "/FIELDS_A/@COLUMN_NAME/%d", i);
		
			/* Second to Define column's type */
			type = va_arg (args, GType);
			if (type == 0) {
				g_set_error (error, GDA_EASY_ERROR, GDA_EASY_INCORRECT_VALUE_ERROR, 
					     _("Invalid type"));
				g_object_unref (op);
				return NULL;
			}
			dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (server, 
											 cnc, type);
			gda_server_operation_set_value_at (op, dbms_type, error, "/FIELDS_A/@COLUMN_TYPE/%d", i);
		
			/* Third for column's flags */
			flag = va_arg (args, GdaEasyCreateTableFlag);
			if (flag & GDA_EASY_CREATE_TABLE_PKEY_FLAG)
				gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_PKEY/%d", i);
			if (flag & GDA_EASY_CREATE_TABLE_NOT_NULL_FLAG)
				gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_NNUL/%d", i);
			if (flag & GDA_EASY_CREATE_TABLE_AUTOINC_FLAG)
				gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_AUTOINC/%d", i);
			if (flag & GDA_EASY_CREATE_TABLE_FKEY_FLAG) {
				gint j;
				gint fields;
				gchar *fkey_table;
				gchar *fkey_ondelete;
				gchar *fkey_onupdate;
				
				refs++;
				
				fkey_table = va_arg (args, gchar*);				
				gda_server_operation_set_value_at (op, fkey_table, error, 
								   "/FKEY_S/%d/FKEY_REF_TABLE", refs);
				
				fields = va_arg (args, gint);
				
				for (j = 0; j < fields; j++) {
					gchar *field, *rfield;
					
					field = va_arg (args, gchar*);				
					gda_server_operation_set_value_at (op, field, error, 
									   "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d", refs, j);
					
					rfield = va_arg (args, gchar*);
					gda_server_operation_set_value_at (op, rfield, error, 
									   "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d", refs, j);
					
				}
				
				fkey_ondelete = va_arg (args, gchar*);
				gda_server_operation_set_value_at (op, fkey_ondelete, error, 
								   "/FKEY_S/%d/FKEY_ONDELETE", refs);
				fkey_onupdate = va_arg (args, gchar*);
				gda_server_operation_set_value_at (op, fkey_onupdate, error, 
								   "/FKEY_S/%d/FKEY_ONUPDATE", refs);
			}
			
			i++;
		}
	
		va_end (args);
		
		g_object_set_data_full (G_OBJECT (op), "_gda_connection", g_object_ref (cnc), g_object_unref);
		
		return op;		
	}
	else {
		g_set_error (error, GDA_EASY_ERROR, GDA_EASY_OBJECT_NAME_ERROR, 
			     _("CREATE TABLE operation is not supported by the database server"));
		return NULL;
	}
}

/**
 * gda_perform_create_table
 * @op: a valid #GdaServerOperation
 * @error: a place to store errors, or %NULL
 * 
 * Performs a prepared #GdaServerOperation to create a table. This could perform
 * an operation created by #gda_prepare_create_table or any other using the
 * the #GdaServerOperation API.
 *
 * Returns: TRUE if the table was created; FALSE and set @error otherwise
 */
gboolean
gda_perform_create_table (GdaServerOperation *op, GError **error)
{
	GdaConnection *cnc;
	
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);	
	g_return_val_if_fail (gda_server_operation_get_op_type (op) == GDA_SERVER_OPERATION_CREATE_TABLE, FALSE);
	
	cnc = g_object_get_data (G_OBJECT (op), "_gda_connection");
	if (cnc) 
		return gda_server_provider_perform_operation (gda_connection_get_provider (cnc), cnc, op, error);
	else {
		g_warning ("Could not find operation's associated connection, "
			   "did you use gda_prepare_create_table() ?"); 
		return FALSE;
	}
}

/**
 * gda_prepare_drop_table
 * @cnc: an opened connection
 * @table_name:
 * @error: a place to store errors, or %NULL
 * 
 * This is just a convenient function to create a #GdaServerOperation to drop a 
 * table in an opened connection.
 *
 * Returns: a new #GdaServerOperation or NULL if couldn't create the opereration.
 */
GdaServerOperation*
gda_prepare_drop_table (GdaConnection *cnc, const gchar *table_name, GError **error)
{
	GdaServerOperation *op;
	GdaServerProvider *server;

	server = gda_connection_get_provider (cnc);
	
	op = gda_server_provider_create_operation (server, cnc, 
						   GDA_SERVER_OPERATION_DROP_TABLE, NULL, error);
	
	if (GDA_IS_SERVER_OPERATION (op)) {
		g_return_val_if_fail (table_name != NULL 
				      || GDA_IS_CONNECTION (cnc) 
				      || !gda_connection_is_opened (cnc), NULL);
		
		if (gda_server_operation_set_value_at (op, table_name, 
						       error, "/TABLE_DESC_P/TABLE_NAME")) {
			g_object_set_data_full (G_OBJECT (op), "_gda_connection", g_object_ref (cnc), g_object_unref);
			return op;
		}
		else 
			return NULL;
	}
	else
		return NULL;
}

/**
 * gda_perform_drop_table
 * @op: a #GdaServerOperation object
 * @error: a place to store errors, or %NULL
 * 
 * This is just a convenient function to perform a drop a table operation.
 *
 * Returns: TRUE if the table was dropped
 */
gboolean
gda_perform_drop_table (GdaServerOperation *op, GError **error)
{
	GdaConnection *cnc;
	
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);	
	g_return_val_if_fail (gda_server_operation_get_op_type (op) == GDA_SERVER_OPERATION_DROP_TABLE, FALSE);
	
	cnc = g_object_get_data (G_OBJECT (op), "_gda_connection");
	if (cnc) 
		return gda_server_provider_perform_operation (gda_connection_get_provider (cnc), cnc, op, error);
	else {
		g_warning ("Could not find operation's associated connection, "
			   "did you use gda_prepare_drop_table() ?"); 
		return FALSE;
	}
}

static guint
gtype_hash (gconstpointer key)
{
        return GPOINTER_TO_UINT (key);
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
 * Obtain a pointer to a #GdaDataHandler which can manage #GValue values of type @for_type. The returned
 * data handler will be adapted to use the current locale information (for example dates will be formatted
 * taking into accoutn the locale).
 *
 * The returned pointer is %NULL if there is no default data handler available for the @for_type data type
 *
 * Returns: a #GdaDataHandler which must not be modified or destroyed.
 */
GdaDataHandler *
gda_get_default_handler (GType for_type)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
	static GHashTable *hash = NULL;
	GdaDataHandler *dh;

	g_static_mutex_lock (&mutex);
	if (!hash) {
		hash = g_hash_table_new_full (gtype_hash, gtype_equal,
					      NULL, (GDestroyNotify) g_object_unref);

                g_hash_table_insert (hash, (gpointer) G_TYPE_INT64, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_UINT64, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_BINARY, gda_handler_bin_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_BLOB, gda_handler_bin_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_BOOLEAN, gda_handler_boolean_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_DATE, gda_handler_time_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_DOUBLE, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_INT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_NUMERIC, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_FLOAT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_SHORT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_USHORT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_STRING, gda_handler_string_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_TIME, gda_handler_time_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_TIMESTAMP, gda_handler_time_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_CHAR, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_UCHAR, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_ULONG, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_LONG, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_GTYPE, gda_handler_type_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_UINT, gda_handler_numerical_new ());
	}
	g_static_mutex_unlock (&mutex);
	
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
