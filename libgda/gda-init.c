/* GDA Library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
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

#include <glib/gmain.h>
#include <gmodule.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>

#include <libgda/binreloc/gda-binreloc.h>
#include <sql-parser/gda-sql-parser.h>

static GStaticRecMutex gda_mutex = G_STATIC_REC_MUTEX_INIT;
#define GDA_LOCK() g_static_rec_mutex_lock(&gda_mutex)
#define GDA_UNLOCK() g_static_rec_mutex_unlock(&gda_mutex)

static GMainLoop *main_loop = NULL;
static GdaSqlParser *internal_parser;

/* global variables */
xmlDtdPtr       gda_array_dtd = NULL;
xmlDtdPtr       gda_paramlist_dtd = NULL;
xmlDtdPtr       gda_server_op_dtd = NULL;


/**
 * gda_init
 * @app_id: name of the program.
 * @version: revision number of the program.
 * @nargs: number of arguments, usually argc from main().
 * @args: list of arguments, usually argv from main().
 * 
 * Initializes the GDA library. 
 */
void
gda_init (const gchar *app_id, const gchar *version, gint nargs, gchar *args[])
{
	static gboolean initialized = FALSE;
	GType type;
	gchar *file;

	if (initialized) {
		gda_log_error (_("Attempt to re-initialize GDA library. ignored."));
		return;
	}

	file = gda_gbr_get_file_path (GDA_LOCALE_DIR, NULL);
	bindtextdomain (GETTEXT_PACKAGE, file);
	g_free (file);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* Threading support if possible */
	if (!getenv ("LIBGDA_NO_THREADS")) {
#ifdef G_THREADS_ENABLED
#ifndef G_THREADS_IMPL_NONE
		if (! g_thread_supported ())
			g_thread_init (NULL);
#endif
#endif
	}

	g_type_init ();
	g_set_prgname (app_id);

	if (!g_module_supported ())
		g_error (_("libgda needs GModule. Finishing..."));

	/* create the required Gda types */
	type = G_TYPE_DATE;
	g_assert (type);
	type = GDA_TYPE_BINARY;
	g_assert (type);
	type = GDA_TYPE_BLOB;
	g_assert (type);
	type = GDA_TYPE_GEOMETRIC_POINT;
	g_assert (type);
	type = GDA_TYPE_LIST;
	g_assert (type);
	type = GDA_TYPE_NUMERIC;
	g_assert (type);
	type = GDA_TYPE_SHORT;
	g_assert (type);
	type = GDA_TYPE_USHORT;
	g_assert (type);
	type = GDA_TYPE_TIME;
	g_assert (type);
	type = G_TYPE_DATE;
	g_assert (type);
	type = GDA_TYPE_TIMESTAMP;
	g_assert (type);

	/* binreloc */
	gda_gbr_init ();

	/* internal parser for easy API */
	internal_parser = gda_sql_parser_new ();

	/* array DTD */
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-array.dtd", NULL);
	gda_array_dtd = xmlParseDTD (NULL, (xmlChar*)file);
	if (gda_array_dtd)
		gda_array_dtd->name = xmlStrdup((xmlChar*) "gda_array");
	else
		g_message (_("Could not parse '%s': "
			     "XML data import validation will not be performed (some weird errors may occur)"),
			   file);
	g_free (file);

	/* paramlist DTD */
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-paramlist.dtd", NULL);
	gda_paramlist_dtd = xmlParseDTD (NULL, (xmlChar*)file);
	if (gda_paramlist_dtd)
		gda_paramlist_dtd->name = xmlStrdup((xmlChar*) "data-set-spec");
	else
		g_message (_("Could not parse '%s': "
			     "XML data import validation will not be performed (some weird errors may occur)"),
			   file);
	g_free (file);

	/* server operation DTD */
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-server-operation.dtd", NULL);
	gda_server_op_dtd = xmlParseDTD (NULL, (xmlChar*)file);
	if (gda_server_op_dtd)
		gda_server_op_dtd->name = xmlStrdup((xmlChar*) "serv_op");
	else
		g_message (_("Could not parse '%s': "
			     "Validation for XML files for server operations will not be performed (some weird errors may occur)"),
			   file);
	g_free (file);

	initialized = TRUE;
}

typedef struct {
	GdaInitFunc init_func;
	gpointer user_data;
} InitCbData;

static gboolean
idle_cb (gpointer user_data)
{
	InitCbData *cb_data = (InitCbData *) user_data;

	g_return_val_if_fail (cb_data != NULL, FALSE);

	if (cb_data->init_func)
		cb_data->init_func (cb_data->user_data);

	g_free (cb_data);

	return FALSE;
}

/**
 * gda_main_run
 * @init_func: function to be called when everything has been initialized.
 * @user_data: data to be passed to the init function.
 *
 * Runs the GDA main loop, which is nothing more than the glib main
 * loop, but with internally added stuff specific for applications using
 * libgda.
 *
 * You can specify a function to be called after everything has been correctly
 * initialized (that is, for initializing your own stuff).
 */
void
gda_main_run (GdaInitFunc init_func, gpointer user_data)
{
	GDA_LOCK ();
	if (main_loop) {
		GDA_UNLOCK ();
		return;
	}

	if (init_func) {
		InitCbData *cb_data;

		cb_data = g_new (InitCbData, 1);
		cb_data->init_func = init_func;
		cb_data->user_data = user_data;

		g_idle_add ((GSourceFunc) idle_cb, cb_data);
	}

	main_loop = g_main_loop_new (g_main_context_default (), FALSE);
	g_main_loop_run (main_loop);
	GDA_UNLOCK ();
}

/**
 * gda_main_quit
 * 
 * Exits the main loop.
 */
void
gda_main_quit (void)
{
	GDA_LOCK ();
	g_main_loop_quit (main_loop);
	g_main_loop_unref (main_loop);

	main_loop = NULL;
	GDA_UNLOCK ();
}

/*
 * Convenient Functions
 */


/* module error */
GQuark gda_general_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_general_error");
	return quark;
}

/**
 * gda_execute_select_command
 * @cnn: an opened connection
 * @sql: a query statament must begin with "SELECT"
 * @error: a place to store errors, or %NULL
 * 
 * Execute a SQL SELECT command over an opened connection.
 *
 * Return: a new #GdaDataModel if succesfull, NULL otherwise
 */
GdaDataModel *          
gda_execute_select_command (GdaConnection *cnn, const gchar *sql, GError **error)
{
	GdaStatement *stmt;
	GdaDataModel *model;
    
	g_return_val_if_fail (sql != NULL 
			      || GDA_IS_CONNECTION (cnn) 
			      || !gda_connection_is_opened (cnn)
			      || g_str_has_prefix (sql, "SELECT"),
			      NULL);
    
	stmt = gda_sql_parser_parse_string (internal_parser, sql, NULL, error);
	if (!stmt) 
		return NULL;
	model = gda_connection_statement_execute_select (cnn, stmt, NULL, error);
	g_object_unref (stmt);

	return model;
}

/**
 * gda_execute_sql_command
 * @cnn: an opened connection
 * @sql: a query statament must begin with "SELECT"
 * @error: a place to store errors, or %NULL
 *
 * This is a convenient function to execute a SQL command over the opened connection.
 *
 * Returns: the number of rows affected or -1.
 */
gint
gda_execute_sql_command (GdaConnection *cnn, const gchar *sql, GError **error)
{
	GdaStatement *stmt;
	gint retval;

	g_return_val_if_fail (sql != NULL 
			      || GDA_IS_CONNECTION (cnn) 
			      || !gda_connection_is_opened (cnn), -1);
    
	stmt = gda_sql_parser_parse_string (internal_parser, sql, NULL, error);
	if (!stmt) 
		return -1;
    
	retval = gda_connection_statement_execute_non_select (cnn, stmt, NULL, NULL, error);
	g_object_unref (stmt);
	return retval;
}

/**
 * gda_create_table
 * @cnn: an opened connection
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
gda_create_table (GdaConnection *cnn, const gchar *table_name, GError **error, ...)
{
	GdaServerOperation *op;
	GdaServerProvider *server;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnn), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnn), FALSE);
	
	server = gda_connection_get_provider_obj(cnn);
	
	op = gda_server_provider_create_operation (server, cnn, 
						   GDA_SERVER_OPERATION_CREATE_TABLE, NULL, error);
	if (GDA_IS_SERVER_OPERATION (op)) {
		va_list  args;
		gchar   *arg;
		GType    type;
		gchar   *dbms_type;
		xmlDocPtr parameters;
		xmlNodePtr root;
		xmlNodePtr table, op_data, array_data, array_row, array_value;
		
		if (table_name == NULL) {
			g_message("Table name is NULL!");      
			g_set_error (error, GDA_GENERAL_ERROR, GDA_GENERAL_OBJECT_NAME_ERROR, 
				    "Couldn't create table with a NULL string");
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
			g_message("Getting the arguments...");			
			type = va_arg (args, GType);
			if (type == 0) {
				g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_INCORRECT_VALUE_ERROR, 
					    "Error the number of arguments are incorrect; couldn't create the table");
				g_object_unref (op);
				xmlFreeDoc(parameters);
				return FALSE;
			}
			
			dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (server, 
											 cnn, type);
			array_row = xmlNewChild (array_data, NULL, (xmlChar*)"gda_array_row", NULL);
			array_value = xmlNewChild (array_row, NULL, (xmlChar*)"gda_array_value", (xmlChar*)arg);
			xmlSetProp(array_value, (xmlChar*)"colid", (xmlChar*)"COLUMN_NAME");
		
			array_value = xmlNewChild(array_row, NULL, (xmlChar*)"gda_array_value", (xmlChar*)dbms_type);
			xmlSetProp(array_value, (xmlChar*)"colid", (xmlChar*)"COLUMN_TYPE");
			
		}
		
		va_end(args);
		
		if (!gda_server_operation_load_data_from_xml (op, root, error)) {
			/* error */
			g_set_error (error, GDA_GENERAL_ERROR, GDA_GENERAL_OPERATION_ERROR, 
				     "The XML operation doesn't exist or could't be loaded");
			g_object_unref (op);
			xmlFreeDoc(parameters);
			return FALSE;
		}
		else {
			if (gda_server_provider_perform_operation (server, cnn, op, error)) {
				/* error */
				g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_OPERATION_ERROR, 
					    "The Server couldn't perform the CREATE TABLE operation!");
				g_object_unref (op);
				xmlFreeDoc(parameters);
				return FALSE;
			}
		}
		
		g_object_unref (op);
		xmlFreeDoc(parameters);
	}
	else {
		g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_OBJECT_NAME_ERROR, 
			    "The Server doesn't support the CREATE TABLE operation!");
		return FALSE;
	}
	return TRUE;
}

/**
 * gda_drop_table
 * @cnn: an opened connection
 * @table_name:
 * @error: a place to store errors, or %NULL
 * 
 * This is just a convenient function to drop a table in an opened connection.
 *
 * Returns: TRUE if the table was dropped
 */
gboolean
gda_drop_table (GdaConnection *cnn, const gchar *table_name, GError **error)
{
	GdaServerOperation *op;
	GdaServerProvider *server;
	gboolean retval = TRUE;

	server = gda_connection_get_provider_obj (cnn);
	
	op = gda_server_provider_create_operation (server, cnn, 
						   GDA_SERVER_OPERATION_DROP_TABLE, NULL, error);
	
	if (GDA_IS_SERVER_OPERATION (op)) {
		xmlDocPtr parameters;
		xmlNodePtr root;
		xmlNodePtr table;
		
		g_return_val_if_fail (table_name != NULL 
				      || GDA_IS_CONNECTION (cnn) 
				      || !gda_connection_is_opened (cnn), FALSE);
    
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
			if (gda_server_provider_perform_operation (server, cnn, op, error))
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

/**
 * gda_open_connection
 * @dsn: a data source name
 * @username:
 * @password:
 * @options:
 * @error: a place to store errors, or %NULL
 * 
 * Convenient function to open a connection to a Data Source, 
 * see also gda_client_open_connection().
 *
 * Returns: a #GdaConnection object if the connection was sucessfully opened, %NULL otherwise
 */
GdaConnection *
gda_open_connection (const gchar *dsn, const gchar *username, const gchar *password, 
		     GdaConnectionOptions options, GError **error)
{
	static GdaClient *client = NULL;
	GdaConnection *cnn;
	
	g_return_val_if_fail (dsn != NULL, NULL);
	
	if (!client)
		client = gda_client_new ();
	
	cnn = gda_client_open_connection (client, dsn, username, password, options, error);
	
	return cnn;
    
}


/* Auxiliary struct to save and get the varian arguments */
typedef struct {
	gchar  *column_name;
	GValue *value;
} GdaValueArgument;

/**
 * gda_insert_row_into_table_from_string
 * @cnn: an opened connection
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
gda_insert_row_into_table_from_string (GdaConnection *cnn, const gchar *table_name, GError **error, ...)
{        
	g_return_val_if_fail (GDA_IS_CONNECTION (cnn), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnn), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	TO_IMPLEMENT;
	return FALSE;
}

/**
 * gda_insert_row_into_table
 * @cnn: an opened connection
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
gda_insert_row_into_table (GdaConnection *cnn, const gchar *table_name, GError **error, ...)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnn), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnn), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	TO_IMPLEMENT;
	return FALSE;
}

/**
 * gda_delete_row_from_table
 * @cnn: an opened connection
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
gda_delete_row_from_table (GdaConnection *cnn, const gchar *table_name, const gchar *condition_column_name, 
			   const GValue *condition, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnn), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnn), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	TO_IMPLEMENT;
	return FALSE;
}

/**
 * gda_update_value_in_table
 * @cnn: an opened connection
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
gda_update_value_in_table (GdaConnection *cnn, 
			   const gchar *table_name, const gchar *search_for_column, 
			   const GValue *condition, 
			   const gchar *column_name, const GValue *new_value, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnn), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnn), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	TO_IMPLEMENT;
	return FALSE;
}

/**
 * gda_update_values_in_table
 * @cnn: an opened connection
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
gda_update_values_in_table (GdaConnection *cnn, const gchar *table_name, 
			    const gchar *condition_column_name, const GValue *condition, 
			    GError **error, ...)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnn), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnn), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	TO_IMPLEMENT;
	return FALSE;
}
