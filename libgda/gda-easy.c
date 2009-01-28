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
#include <libgda/gda-config.h>
#include <libgda/gda-set.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-sql-statement.h>
#include <libgda/gda-holder.h>

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
 * Returns: a new #GdaDataModel if succesfull, NULL otherwise
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
 * This is a convenience function to execute a SQL command over the opened connection.
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
			     "%s", _("Unspecified table name"));
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
					     "%s", _("Invalid type"));
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
			     "%s", _("CREATE TABLE operation is not supported by the database server"));
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
 * @table_name: name of the table to drop
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

/**
 * gda_insert_row_into_table
 * @cnc: an opened connection
 * @table: table's name to insert into
 * @error: a place to store errors, or %NULL
 * @...: a list of string/GValue pairs with the name of the column to use and the
 * GValue pointer containing the value to insert for the column (value can be %NULL), finished by a %NULL. There must be
 * at least one column name and value
 * 
 * This is a convenience function, which creates an INSERT statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: INSERT INTO &lt;table&gt; (&lt;column_name&gt; [,...]) VALUES (&lt;column_name&gt; = &lt;new_value&gt; [,...]).
 * 
 * Returns: TRUE if no error occurred
 */
gboolean
gda_insert_row_into_table (GdaConnection *cnc, const gchar *table, GError **error, ...)
{
	GSList *clist = NULL;
	GSList *vlist = NULL;
	gboolean retval;
	va_list  args;
	gchar *col_name;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table && *table, FALSE);
	
	va_start (args, error);	
	while ((col_name = va_arg (args, gchar*))) {
		clist = g_slist_prepend (clist, col_name);
		GValue *value;
		value = va_arg (args, GValue *);
		vlist = g_slist_prepend (vlist, value);
	}
	
	va_end (args);

	if (!clist) {
		g_warning ("No specified column or value");
		return FALSE;
	}

	clist = g_slist_reverse (clist);
	vlist = g_slist_reverse (vlist);
	retval = gda_insert_row_into_table_v (cnc, table, clist, vlist, error);
	g_slist_free (clist);
	g_slist_free (vlist);

	return retval;
}

/**
 * gda_insert_row_into_table_v
 * @cnc: an opened connection
 * @table: table's name to insert into
 * @col_names: a list of column names (as const gchar *)
 * @values: a list of values (as #GValue)
 * @error: a place to store errors, or %NULL
 *
 * @col_names and @values must have length (&gt;= 1).
 *
 * This is a convenience function, which creates an INSERT statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: INSERT INTO &lt;table&gt; (&lt;column_name&gt; [,...]) VALUES (&lt;column_name&gt; = &lt;new_value&gt; [,...]).
 * 
 * Returns: TRUE if no error occurred
 */
gboolean
gda_insert_row_into_table_v (GdaConnection *cnc, const gchar *table,
			     GSList *col_names, GSList *values,
			     GError **error)
{
	gboolean retval;
	GSList *fields = NULL;
	GSList *expr_values = NULL;
	GdaSqlStatement *sql_stm;
	GdaSqlStatementInsert *ssi;
	GdaStatement *insert;
	gint i;

	GSList *holders = NULL;
	GSList *l1, *l2;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table && *table, FALSE);
	g_return_val_if_fail (col_names, FALSE);
	g_return_val_if_fail (g_slist_length (col_names) == g_slist_length (values), FALSE);
	
	/* Construct insert query and list of GdaHolders */
	sql_stm = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);
	ssi = (GdaSqlStatementInsert*) sql_stm->contents;
	g_assert (GDA_SQL_ANY_PART (ssi)->type == GDA_SQL_ANY_STMT_INSERT);

	ssi->table = gda_sql_table_new (GDA_SQL_ANY_PART (ssi));
	if (gda_sql_identifier_needs_quotes (table))
		ssi->table->table_name = gda_sql_identifier_add_quotes (table);
	else
		ssi->table->table_name = g_strdup (table);

	i = 0;
	for (l1 = col_names, l2 = values;
	     l1;
	     l1 = l1->next, l2 = l2->next) {
		GdaSqlField *field;
		GdaSqlExpr *expr;
		GValue *value = (GValue *) l2->data;
		const gchar *col_name = (const gchar*) l1->data; 
		
		/* field */
		field = gda_sql_field_new (GDA_SQL_ANY_PART (ssi));
		if (gda_sql_identifier_needs_quotes (col_name))
			field->field_name = gda_sql_identifier_add_quotes (col_name);
		else
			field->field_name = g_strdup (col_name);
		fields = g_slist_prepend (fields, field);

		/* value */
		expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ssi));
		if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
			/* create a GdaSqlExpr with a parameter */
			GdaSqlParamSpec *param;
			param = g_new0 (GdaSqlParamSpec, 1);
			param->name = g_strdup_printf ("+%d", i);
			param->g_type = G_VALUE_TYPE (value);
			param->is_param = TRUE;
			expr->param_spec = param;

			GdaHolder *holder;
			holder = (GdaHolder*)  g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (value),
							     "id", param->name, NULL);
			g_assert (gda_holder_set_value (holder, value, NULL));
			holders = g_slist_prepend (holders, holder);
		}
		else {
			/* create a NULL GdaSqlExpr => nothing to do */
		}
		expr_values = g_slist_prepend (expr_values, expr);
		
		i++;
	}
	
	ssi->fields_list = g_slist_reverse (fields);
	ssi->values_list = g_slist_prepend (NULL, g_slist_reverse (expr_values));
	
	insert = gda_statement_new ();
	g_object_set (G_OBJECT (insert), "structure", sql_stm, NULL);
	gda_sql_statement_free (sql_stm);

	/* execute statement */
	GdaSet *set = NULL;
	if (holders) {
		set = gda_set_new (holders);
		g_slist_foreach (holders, (GFunc) g_object_unref, NULL);
		g_slist_free (holders);
	}

	retval = (gda_connection_statement_execute_non_select (cnc, insert, set, NULL, error) == -1) ? FALSE : TRUE;

	if (set)
		g_object_unref (set);
	g_object_unref (insert);

	return retval;	
}


/**
 * gda_update_row_in_table
 * @cnc: an opened connection
 * @table: the table's name with the row's values to be updated
 * @condition_column_name: the name of the column to used in the WHERE condition clause
 * @condition_value: the @condition_column_type's GType
 * @error: a place to store errors, or %NULL
 * @...: a list of string/GValue pairs with the name of the column to use and the
 * GValue pointer containing the value to update the column to (value can be %NULL), finished by a %NULL. There must be
 * at least one column name and value
 * 
 * This is a convenience function, which creates an UPDATE statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: UPDATE &lt;table&gt; SET &lt;column_name&gt; = &lt;new_value&gt; [,...] WHERE &lt;condition_column_name&gt; = &lt;condition_value&gt;.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_update_row_in_table (GdaConnection *cnc, const gchar *table, 
			 const gchar *condition_column_name, 
			 GValue *condition_value, GError **error, ...)
{
	GSList *clist = NULL;
	GSList *vlist = NULL;
	gboolean retval;
	va_list  args;
	gchar *col_name;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table && *table, FALSE);
	
	va_start (args, error);	
	while ((col_name = va_arg (args, gchar*))) {
		clist = g_slist_prepend (clist, col_name);
		GValue *value;
		value = va_arg (args, GValue *);
		vlist = g_slist_prepend (vlist, value);
	}
	
	va_end (args);

	if (!clist) {
		g_warning ("No specified column or value");
		return FALSE;
	}

	clist = g_slist_reverse (clist);
	vlist = g_slist_reverse (vlist);
	retval = gda_update_row_in_table_v (cnc, table, condition_column_name, condition_value, clist, vlist, error);
	g_slist_free (clist);
	g_slist_free (vlist);

	return retval;
}

/**
 * gda_update_row_in_table_v
 * @cnc: an opened connection
 * @table: the table's name with the row's values to be updated
 * @condition_column_name: the name of the column to used in the WHERE condition clause
 * @condition_value: the @condition_column_type's GType
 * @col_names: a list of column names (as const gchar *)
 * @values: a list of values (as #GValue)
 * @error: a place to store errors, or %NULL
 *
 * @col_names and @values must have length (&gt;= 1).
 * 
 * This is a convenience function, which creates an UPDATE statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: UPDATE &lt;table&gt; SET &lt;column_name&gt; = &lt;new_value&gt; [,...] WHERE &lt;condition_column_name&gt; = &lt;condition_value&gt;.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_update_row_in_table_v (GdaConnection *cnc, const gchar *table, 
			   const gchar *condition_column_name, 
			   GValue *condition_value, 
			   GSList *col_names, GSList *values,
			   GError **error)
{
	gboolean retval;
	GSList *fields = NULL;
	GSList *expr_values = NULL;
	GdaSqlStatement *sql_stm;
	GdaSqlStatementUpdate *ssu;
	GdaStatement *update;
	gint i;

	GSList *holders = NULL;
	GSList *l1, *l2;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table && *table, FALSE);
	g_return_val_if_fail (col_names, FALSE);
	g_return_val_if_fail (g_slist_length (col_names) == g_slist_length (values), FALSE);
	
	/* Construct update query and list of GdaHolders */
	sql_stm = gda_sql_statement_new (GDA_SQL_STATEMENT_UPDATE);
	ssu = (GdaSqlStatementUpdate*) sql_stm->contents;
	g_assert (GDA_SQL_ANY_PART (ssu)->type == GDA_SQL_ANY_STMT_UPDATE);

	ssu->table = gda_sql_table_new (GDA_SQL_ANY_PART (ssu));
	if (gda_sql_identifier_needs_quotes (table))
		ssu->table->table_name = gda_sql_identifier_add_quotes (table);
	else
		ssu->table->table_name = g_strdup (table);

	if (condition_column_name) {
		GdaSqlExpr *where, *op;
		where = gda_sql_expr_new (GDA_SQL_ANY_PART (ssu));
		ssu->cond = where;

		where->cond = gda_sql_operation_new (GDA_SQL_ANY_PART (where));
		where->cond->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;

		op = gda_sql_expr_new (GDA_SQL_ANY_PART (where->cond));
		where->cond->operands = g_slist_prepend (NULL, op);
		op->value = gda_value_new (G_TYPE_STRING);
		if (gda_sql_identifier_needs_quotes (condition_column_name))
			g_value_take_string (op->value, gda_sql_identifier_add_quotes (condition_column_name));
		else
			g_value_set_string (op->value, condition_column_name);

		op = gda_sql_expr_new (GDA_SQL_ANY_PART (where->cond));
		where->cond->operands = g_slist_append (where->cond->operands, op);
		if (condition_value) {
			GdaSqlParamSpec *param;
			param = g_new0 (GdaSqlParamSpec, 1);
			param->name = g_strdup ("cond");
			param->g_type = G_VALUE_TYPE (condition_value);
			param->is_param = TRUE;
			op->param_spec = param;

			GdaHolder *holder;
			holder = (GdaHolder*)  g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (condition_value),
							     "id", param->name, NULL);
			g_assert (gda_holder_set_value (holder, condition_value, NULL));
			holders = g_slist_prepend (holders, holder);
		}
		else {
			/* nothing to do: NULL */
		}
	}

	i = 0;
	for (l1 = col_names, l2 = values;
	     l1;
	     l1 = l1->next, l2 = l2->next) {
		GValue *value = (GValue *) l2->data;
		const gchar *col_name = (const gchar*) l1->data;
		GdaSqlField *field;
		GdaSqlExpr *expr;
		
		/* field */
		field = gda_sql_field_new (GDA_SQL_ANY_PART (ssu));
		if (gda_sql_identifier_needs_quotes (col_name))
			field->field_name = gda_sql_identifier_add_quotes (col_name);
		else
			field->field_name = g_strdup (col_name);
		fields = g_slist_prepend (fields, field);

		/* value */
		expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ssu));
		if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
			/* create a GdaSqlExpr with a parameter */
			GdaSqlParamSpec *param;
			param = g_new0 (GdaSqlParamSpec, 1);
			param->name = g_strdup_printf ("+%d", i);
			param->g_type = G_VALUE_TYPE (value);
			param->is_param = TRUE;
			expr->param_spec = param;

			GdaHolder *holder;
			holder = (GdaHolder*)  g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (value),
							     "id", param->name, NULL);
			g_assert (gda_holder_set_value (holder, value, NULL));
			holders = g_slist_prepend (holders, holder);
		}
		else {
			/* create a NULL GdaSqlExpr => nothing to do */
		}
		expr_values = g_slist_prepend (expr_values, expr);
		
		i++;
	}

	ssu->fields_list = g_slist_reverse (fields);
	ssu->expr_list = g_slist_reverse (expr_values);
	
	update = gda_statement_new ();
	g_object_set (G_OBJECT (update), "structure", sql_stm, NULL);
	gda_sql_statement_free (sql_stm);

	/* execute statement */
	GdaSet *set = NULL;
	if (holders) {
		set = gda_set_new (holders);
		g_slist_foreach (holders, (GFunc) g_object_unref, NULL);
		g_slist_free (holders);
	}

	retval = (gda_connection_statement_execute_non_select (cnc, update, set, NULL, error) == -1) ? FALSE : TRUE;

	if (set)
		g_object_unref (set);
	g_object_unref (update);

	return retval;
}

/**
 * gda_delete_row_from_table
 * @cnc: an opened connection
 * @table: the table's name with the row's values to be updated
 * @condition_column_name: the name of the column to used in the WHERE condition clause
 * @condition_value: the @condition_column_type's GType
 * @error: a place to store errors, or %NULL
 * 
 * This is a convenience function, which creates a DELETE statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: DELETE FROM &lt;table&gt; WHERE &lt;condition_column_name&gt; = &lt;condition_value&gt;.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_delete_row_from_table (GdaConnection *cnc, const gchar *table, 
			   const gchar *condition_column_name, 
			   GValue *condition_value, GError **error)
{
	gboolean retval;
	gchar *col_name;
	GdaSqlStatement *sql_stm;
	GdaSqlStatementDelete *ssd;
	GdaStatement *delete;

	GSList *holders = NULL;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table && *table, FALSE);
	
	/* Construct delete query and list of GdaHolders */
	sql_stm = gda_sql_statement_new (GDA_SQL_STATEMENT_DELETE);
	ssd = (GdaSqlStatementDelete*) sql_stm->contents;
	g_assert (GDA_SQL_ANY_PART (ssd)->type == GDA_SQL_ANY_STMT_DELETE);

	ssd->table = gda_sql_table_new (GDA_SQL_ANY_PART (ssd));
	if (gda_sql_identifier_needs_quotes (table))
		ssd->table->table_name = gda_sql_identifier_add_quotes (table);
	else
		ssd->table->table_name = g_strdup (table);

	if (condition_column_name) {
		GdaSqlExpr *where, *op;
		where = gda_sql_expr_new (GDA_SQL_ANY_PART (ssd));
		ssd->cond = where;

		where->cond = gda_sql_operation_new (GDA_SQL_ANY_PART (where));
		where->cond->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;

		op = gda_sql_expr_new (GDA_SQL_ANY_PART (where->cond));
		where->cond->operands = g_slist_prepend (NULL, op);
		op->value = gda_value_new (G_TYPE_STRING);
		if (gda_sql_identifier_needs_quotes (condition_column_name))
			g_value_take_string (op->value, gda_sql_identifier_add_quotes (condition_column_name));
		else
			g_value_set_string (op->value, condition_column_name);

		op = gda_sql_expr_new (GDA_SQL_ANY_PART (where->cond));
		where->cond->operands = g_slist_append (where->cond->operands, op);
		if (condition_value) {
			GdaSqlParamSpec *param;
			param = g_new0 (GdaSqlParamSpec, 1);
			param->name = g_strdup ("cond");
			param->g_type = G_VALUE_TYPE (condition_value);
			param->is_param = TRUE;
			op->param_spec = param;

			GdaHolder *holder;
			holder = (GdaHolder*)  g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (condition_value),
							     "id", param->name, NULL);
			g_assert (gda_holder_set_value (holder, condition_value, NULL));
			holders = g_slist_prepend (holders, holder);
		}
		else {
			/* nothing to do: NULL */
		}
	}

	delete = gda_statement_new ();
	g_object_set (G_OBJECT (delete), "structure", sql_stm, NULL);
	gda_sql_statement_free (sql_stm);

	/* execute statement */
	GdaSet *set = NULL;
	if (holders) {
		set = gda_set_new (holders);
		g_slist_foreach (holders, (GFunc) g_object_unref, NULL);
		g_slist_free (holders);
	}

	retval = (gda_connection_statement_execute_non_select (cnc, delete, set, NULL, error) == -1) ? FALSE : TRUE;

	if (set)
		g_object_unref (set);
	g_object_unref (delete);

	return retval;
}

