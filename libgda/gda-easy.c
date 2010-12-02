/* GDA Library
 * Copyright (C) 2008 - 2010 The GNOME Foundation.
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
#include "gda-easy.h"
#include "gda-server-operation.h"
#include "gda-server-provider.h"
#include "gda-data-handler.h"

/* module error */
GQuark gda_easy_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_easy_error");
	return quark;
}

/**
 * gda_prepare_create_database:
 * @provider: the database provider to use
 * @db_name: the name of the database to create, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #GdaServerOperation object which contains the specifications required
 * to create a database. Once these specifications provided, use 
 * gda_server_operation_perform_create_database() to perform the database creation.
 *
 * If @db_name is left %NULL, then the name of the database to create will have to be set in the
 * returned #GdaServerOperation using gda_server_operation_set_value_at().
 *
 * Returns: (transfer full) (allow-none): new #GdaServerOperation object, or %NULL if the provider does not support database
 * creation
 *
 * Deprecated: 4.2.3: Use gda_server_operation_prepare_create_database() instead.
 */
GdaServerOperation *
gda_prepare_create_database (const gchar *provider, const gchar *db_name, GError **error)
{
	return gda_server_operation_prepare_create_database(provider, db_name, error);
}

/**
 * gda_perform_create_database:
 * @provider: the database provider to use, or %NULL if @op has been created using gda_server_operation_prepare_create_database()
 * @op: a #GdaServerOperation object obtained using gda_prepare_create_database()
 * @error: a place to store en error, or %NULL
 *
 * Creates a new database using the specifications in @op. @op can be obtained using
 * gda_server_provider_create_operation(), or gda_prepare_create_database().
 *
 * Returns: TRUE if no error occurred and the database has been created, FALSE otherwise
 *
 * Deprecated: 4.2.3: Use gda_server_operation_perform_create_database() instead.
 */
gboolean
gda_perform_create_database (const gchar *provider, GdaServerOperation *op, GError **error)
{
	return gda_server_operation_perform_create_database(op, provider, error);
}

/**
 * gda_prepare_drop_database:
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
 * Returns: (transfer full) (allow-none): new #GdaServerOperation object, or %NULL if the provider does not support database
 * destruction
 *
 * Deprecated: 4.2.3: Use gda_server_operation_prepare_drop_database() instead.
 */
GdaServerOperation *
gda_prepare_drop_database (const gchar *provider, const gchar *db_name, GError **error)
{
	return gda_server_operation_prepare_drop_database(provider, db_name, error);
}

/**
 * gda_perform_drop_database:
 * @provider: the database provider to use, or %NULL if @op has been created using gda_server_operation_prepare_drop_database()
 * @op: a #GdaServerOperation object obtained using gda_server_operation_prepare_drop_database()
 * @error: a place to store en error, or %NULL
 *
 * Destroys an existing database using the specifications in @op.  @op can be obtained using
 * gda_server_provider_create_operation(), or gda_server_operation_prepare_drop_database().
 *
 * Returns: TRUE if no error occurred and the database has been destroyed, FALSE otherwise
 *
 * Deprecated: 4.2.3: Use gda_server_operation_perform_drop_database() instead.
 */
gboolean
gda_perform_drop_database (const gchar *provider, GdaServerOperation *op, GError **error)
{
	return gda_server_operation_perform_drop_database(op, provider, error);
}

/**
 * gda_execute_select_command:
 * @cnc: an opened connection
 * @sql: a query statement must begin with "SELECT"
 * @error: a place to store errors, or %NULL
 * 
 * Execute a SQL SELECT command over an opened connection.
 *
 * Returns: (transfer full) (allow-none): a new #GdaDataModel if successful, NULL otherwise
 *
 * Deprecated: 4.2.3: Use gda_connection_execute_select_command() instead.
 */
GdaDataModel *          
gda_execute_select_command (GdaConnection *cnc, const gchar *sql, GError **error)
{
	return gda_connection_execute_select_command(cnc, sql, error);
}

/**
 * gda_execute_non_select_command:
 * @cnc: an opened connection
 * @sql: a query statement must begin with "SELECT"
 * @error: a place to store errors, or %NULL
 *
 * This is a convenience function to execute a SQL command over the opened connection.
 *
 * Returns: the number of rows affected or -1.
 *
 * Deprecated: 4.2.3: Use gda_connection_execute_non_select_command() instead.
 */
gint
gda_execute_non_select_command (GdaConnection *cnc, const gchar *sql, GError **error)
{
	return gda_connection_execute_non_select_command(cnc, sql, error);
}

/**
 * gda_prepare_create_table:
 * @cnc: an opened connection
 * @table_name: name of the table to create
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
 * Returns: (transfer full) (allow-none): a #GdaServerOperation if no errors; NULL and set @error otherwise
 *
 * Deprecated: 4.2.3: Use gda_server_operation_prepare_create_table() instead.
 */
G_GNUC_NULL_TERMINATED
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
		if (!GDA_IS_SERVER_OPERATION(op))
			return NULL;
		if(!gda_server_operation_set_value_at (op, table_name, error, "/TABLE_DEF_P/TABLE_NAME")) {
			g_object_unref (op);
			return NULL;
		}
				
		va_start (args, error);
		type = 0;
		arg = NULL;
		i = 0;
		refs = -1;
		
		while ((arg = va_arg (args, gchar*))) {
			/* First argument for Column's name */			
			if(!gda_server_operation_set_value_at (op, arg, error, "/FIELDS_A/@COLUMN_NAME/%d", i)){
				g_object_unref (op);
				return NULL;
			}
		
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
			if (!gda_server_operation_set_value_at (op, dbms_type, error, "/FIELDS_A/@COLUMN_TYPE/%d", i)){
				g_object_unref (op);
				return NULL;
			}
		
			/* Third for column's flags */
			flag = va_arg (args, GdaEasyCreateTableFlag);
			if (flag & GDA_EASY_CREATE_TABLE_PKEY_FLAG)
				if(!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_PKEY/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_EASY_CREATE_TABLE_NOT_NULL_FLAG)
				if(!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_NNUL/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_EASY_CREATE_TABLE_AUTOINC_FLAG)
				if (!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_AUTOINC/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_EASY_CREATE_TABLE_UNIQUE_FLAG)
				if(!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_UNIQUE/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_EASY_CREATE_TABLE_FKEY_FLAG) {
				gint j;
				gint fields;
				gchar *fkey_table;
				gchar *fkey_ondelete;
				gchar *fkey_onupdate;
				
				refs++;
				
				fkey_table = va_arg (args, gchar*);				
				if (!gda_server_operation_set_value_at (op, fkey_table, error, 
								   "/FKEY_S/%d/FKEY_REF_TABLE", refs)){
					g_object_unref (op);
					return NULL;
				}
				
				fields = va_arg (args, gint);
				
				for (j = 0; j < fields; j++) {
					gchar *field, *rfield;
					
					field = va_arg (args, gchar*);				
					if(!gda_server_operation_set_value_at (op, field, error, 
									   "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d", refs, j)){
						g_object_unref (op);
						return NULL;
					}
					
					rfield = va_arg (args, gchar*);
					if(!gda_server_operation_set_value_at (op, rfield, error, 
									   "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d", refs, j)){
						g_object_unref (op);
						return NULL;
					}
					
				}
				
				fkey_ondelete = va_arg (args, gchar*);
				if (!gda_server_operation_set_value_at (op, fkey_ondelete, error, 
								   "/FKEY_S/%d/FKEY_ONDELETE", refs)){
					g_object_unref (op);
					return NULL;
				}
				fkey_onupdate = va_arg (args, gchar*);
				if(!gda_server_operation_set_value_at (op, fkey_onupdate, error, 
								   "/FKEY_S/%d/FKEY_ONUPDATE", refs)){
					g_object_unref (op);
					return NULL;
				}
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
 * gda_perform_create_table:
 * @op: a valid #GdaServerOperation
 * @error: a place to store errors, or %NULL
 * 
 * Performs a prepared #GdaServerOperation to create a table. This could perform
 * an operation created by gda_server_operation_prepare_create_table() or any other using the
 * the #GdaServerOperation API.
 *
 * Returns: TRUE if the table was created; FALSE and set @error otherwise
 *
 * Deprecated: 4.2.3: Use gda_server_operation_perform_create_table() instead.
 */
gboolean
gda_perform_create_table (GdaServerOperation *op, GError **error)
{
	return gda_server_operation_perform_create_table (op, error);
}

/**
 * gda_prepare_drop_table:
 * @cnc: an opened connection
 * @table_name: name of the table to drop
 * @error: a place to store errors, or %NULL
 * 
 * This is just a convenient function to create a #GdaServerOperation to drop a 
 * table in an opened connection.
 *
 * Returns: (transfer full) (allow-none): a new #GdaServerOperation or NULL if couldn't create the opereration.
 *
 * Deprecated: 4.2.3: Use gda_server_operation_prepare_drop_table() instead.
 */
GdaServerOperation*
gda_prepare_drop_table (GdaConnection *cnc, const gchar *table_name, GError **error)
{
	return gda_server_operation_prepare_drop_table (cnc, table_name, error);
}

/**
 * gda_perform_drop_table:
 * @op: a #GdaServerOperation object
 * @error: a place to store errors, or %NULL
 * 
 * This is just a convenient function to perform a drop a table operation.
 *
 * Returns: TRUE if the table was dropped, FALSE otherwise
 *
 * Deprecated: 4.2.3: Use gda_server_operation_perform_drop_table() instead.
 */
gboolean
gda_perform_drop_table (GdaServerOperation *op, GError **error)
{
	return gda_server_operation_perform_drop_table(op, error);
}

/**
 * gda_get_default_handler:
 * @for_type: a #GType type
 * 
 * Obtain a pointer to a #GdaDataHandler which can manage #GValue values of type @for_type. The returned
 * data handler will be adapted to use the current locale information (for example dates will be formatted
 * taking into account the locale).
 *
 * The returned pointer is %NULL if there is no default data handler available for the @for_type data type
 *
 * Returns: (transfer none): a #GdaDataHandler which must not be modified or destroyed.
 *
 * Deprecated: 4.2.3: Use gda_data_handler_get_default() instead.
 */
GdaDataHandler *
gda_get_default_handler (GType for_type)
{
	return gda_data_handler_get_default(for_type);
}

/**
 * gda_insert_row_into_table:
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
 * Returns: TRUE if no error occurred, FALSE otherwise
 *
 * Deprecated: 4.2.3: Use gda_connection_insert_row_into_table() instead.
 */
G_GNUC_NULL_TERMINATED
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
	retval = gda_connection_insert_row_into_table_v (cnc, table, clist, vlist, error);
	g_slist_free (clist);
	g_slist_free (vlist);

	return retval;
}

/**
 * gda_insert_row_into_table_v:
 * @cnc: an opened connection
 * @table: table's name to insert into
 * @col_names: (element-type utf8): a list of column names (as const gchar *)
 * @values: (element-type GValue): a list of values (as #GValue)
 * @error: a place to store errors, or %NULL
 *
 * @col_names and @values must have length (&gt;= 1).
 *
 * This is a convenience function, which creates an INSERT statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: INSERT INTO &lt;table&gt; (&lt;column_name&gt; [,...]) VALUES (&lt;column_name&gt; = &lt;new_value&gt; [,...]).
 * 
 * Returns: TRUE if no error occurred, FALSE otherwise
 *
 * Deprecated: 4.2.3: Use gda_connection_insert_row_into_table_v() instead.
 */
gboolean
gda_insert_row_into_table_v (GdaConnection *cnc, const gchar *table,
			     GSList *col_names, GSList *values,
			     GError **error)
{
	return gda_connection_insert_row_into_table_v(cnc, table, col_names, values, error);
}


/**
 * gda_update_row_in_table:
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
 * Returns: TRUE if no error occurred, FALSE otherwise
 *
 * Deprecated: 4.2.3: Use gda_connection_update_row_in_table() instead.
 */
G_GNUC_NULL_TERMINATED
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
	retval = gda_connection_update_row_in_table_v (cnc, table, condition_column_name, condition_value, clist, vlist, error);
	g_slist_free (clist);
	g_slist_free (vlist);

	return retval;
}

/**
 * gda_update_row_in_table_v:
 * @cnc: an opened connection
 * @table: the table's name with the row's values to be updated
 * @condition_column_name: the name of the column to used in the WHERE condition clause
 * @condition_value: the @condition_column_type's GType
 * @col_names: (element-type utf8): a list of column names (as const gchar *)
 * @values: (element-type GValue): a list of values (as #GValue)
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
 *
 * Deprecated: 4.2.3: Use gda_connection_update_row_in_table_v() instead.
 */
gboolean
gda_update_row_in_table_v (GdaConnection *cnc, const gchar *table, 
			   const gchar *condition_column_name, 
			   GValue *condition_value, 
			   GSList *col_names, GSList *values,
			   GError **error)
{
	return gda_connection_update_row_in_table_v (cnc, table, condition_column_name, condition_value,
						     col_names, values, error);
}

/**
 * gda_delete_row_from_table:
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
 * Returns: TRUE if no error occurred, FALSE otherwise
 *
 * Deprecated: 4.2.3: Use gda_connection_delete_row_from_table() instead.
 */
gboolean
gda_delete_row_from_table (GdaConnection *cnc, const gchar *table, 
			   const gchar *condition_column_name, 
			   GValue *condition_value, GError **error)
{
	return gda_connection_delete_row_from_table (cnc, table, condition_column_name, condition_value, error);
}

/**
 * gda_parse_sql_string:
 * @cnc: (allow-none): a #GdaConnection object, or %NULL
 * @sql: an SQL command to parse, not %NULL
 * @params: (out) (allow-none) (transfer full): a place to store a new #GdaSet, for parameters used in SQL command, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * This function helps to parse a SQL witch use paramenters and store them at @params.
 *
 * Returns: (transfer full) (allow-none): a #GdaStatement representing the SQL command, or %NULL if an error occurred
 *
 * Since: 4.2
 *
 * Deprecated: 4.2.3: Use gda_connection_parse_sql_string() instead.
 */
GdaStatement*
gda_parse_sql_string (GdaConnection *cnc, const gchar *sql, GdaSet **params, GError **error)
{
	return gda_connection_parse_sql_string (cnc, sql, params, error);
}
