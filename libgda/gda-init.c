/* GDA Library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
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

static GMainLoop *main_loop = NULL;

/* global variables */
GdaDict        *default_dict = NULL; /* available in all libgda, always NOT NULL */
xmlDtdPtr       gda_dict_dtd = NULL;
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

	if (initialized) {
		gda_log_error (_("Attempt to re-initialize GDA library. ignored."));
		return;
	}

	bindtextdomain (GETTEXT_PACKAGE, LIBGDA_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* Threading support if possible */
#ifdef G_THREADS_ENABLED
#ifndef G_THREADS_IMPL_NONE
	if (! g_thread_supported ())
		g_thread_init (NULL);
#endif
#endif
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

	/* create a default dictionary */
	default_dict = gda_dict_new ();

#define LIBGDA_DICT_DTD_FILE DTDINSTALLDIR"/libgda-dict.dtd"
	gda_dict_dtd = xmlParseDTD (NULL, LIBGDA_DICT_DTD_FILE);
	if (gda_dict_dtd)
		gda_dict_dtd->name = xmlStrdup((xmlChar*) "gda_dict");
	else
		g_warning (_("Could not parse " LIBGDA_DICT_DTD_FILE ": "
			     "XML dictionaries validation will not be performed (some weird errors may occur)"));
#define LIBGDA_PARAMLIST_DTD_FILE DTDINSTALLDIR"/libgda-paramlist.dtd"
	gda_paramlist_dtd = xmlParseDTD (NULL, LIBGDA_PARAMLIST_DTD_FILE);
	if (gda_paramlist_dtd)
		gda_paramlist_dtd->name = xmlStrdup((xmlChar*) "data-set-spec");
	else
		g_warning (_("Could not parse " LIBGDA_PARAMLIST_DTD_FILE ": "
			     "XML data import validation will not be performed (some weird errors may occur)"));
#define LIBGDA_ARRAY_DTD_FILE DTDINSTALLDIR"/libgda-array.dtd"
	gda_array_dtd = xmlParseDTD (NULL, LIBGDA_ARRAY_DTD_FILE);
	if (gda_array_dtd)
		gda_array_dtd->name = xmlStrdup((xmlChar*) "gda_array");
	else
		g_warning (_("Could not parse " LIBGDA_ARRAY_DTD_FILE ": "
			     "XML data import validation will not be performed (some weird errors may occur)"));

#define LIBGDA_SERVER_OP_DTD_FILE DTDINSTALLDIR"/libgda-server-operation.dtd"
	gda_server_op_dtd = xmlParseDTD (NULL, LIBGDA_SERVER_OP_DTD_FILE);
	if (gda_server_op_dtd)
		gda_server_op_dtd->name = xmlStrdup((xmlChar*) "serv_op");
	else
		g_warning (_("Could not parse " LIBGDA_SERVER_OP_DTD_FILE ": "
			     "Validation for XML files for server operations will not be performed (some weird errors may occur)"));

	initialized = TRUE;
}

/**
 * gda_get_default_dict
 *
 * Get the default dictionary.
 *
 * Returns: a not %NULL pointer to the default #GdaDict dictionary
 */
GdaDict *
gda_get_default_dict (void)
{
        return default_dict;
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
	if (main_loop)
		return;

	if (init_func) {
		InitCbData *cb_data;

		cb_data = g_new (InitCbData, 1);
		cb_data->init_func = init_func;
		cb_data->user_data = user_data;

		g_idle_add ((GSourceFunc) idle_cb, cb_data);
	}

	main_loop = g_main_loop_new (g_main_context_default (), FALSE);
	g_main_loop_run (main_loop);
}

/**
 * gda_main_quit
 * 
 * Exits the main loop.
 */
void
gda_main_quit (void)
{
	g_main_loop_quit (main_loop);
	g_main_loop_unref (main_loop);

	main_loop = NULL;
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
    GdaCommand *cmd;
    GdaDataModel *model;
    
    g_return_val_if_fail (sql != NULL 
			  || GDA_IS_CONNECTION (cnn) 
			  || !gda_connection_is_opened (cnn)
			  || g_str_has_prefix (sql, "SELECT"),
			  NULL);
    
    cmd = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
    
    model = gda_connection_execute_select_command (cnn, cmd, NULL, error);
    gda_command_free (cmd);

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
 * Returns: the number of rows impacted or -1.
 */
gint
gda_execute_sql_command (GdaConnection *cnn, const gchar *sql, GError **error)
{
    GdaCommand *cmd;
    
    g_return_val_if_fail (sql != NULL 
			  || GDA_IS_CONNECTION (cnn) 
			  || !gda_connection_is_opened (cnn), -1);
    
    cmd = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
    
    return gda_connection_execute_non_select_command(cnn, cmd, NULL, error);
    
}

/**
 * gda_create_table
 * @cnn: an opened connection
 * @table_name:
 * @num_columns
 * @error: a place to store errors, or %NULL
 * @...: pairs of column name and GType, finish with -1
 * 
 * Create a Table over an opened connection using a pair list of colum name and 
 * GType as arguments, you need to finish the list using -1.
 *
 * This is just a convenient function to create tables quickly, 
 * using defaults for the provider and convertin the GType passed to corresponding 
 * type in the provider; to use a custom type or more
 * advanced characteristics in a specific provider use the #GdaServerOperation framework.
 *
 * Returns: TRUE if the table was created
 */
gboolean
gda_create_table(GdaConnection *cnn, const gchar *table_name, gint num_columns, 
		 GError **error, ...)
{
	GdaServerOperation *op;
	GdaServerProvider *server;
	gboolean retval = TRUE;
	
	server = gda_connection_get_provider_obj(cnn);
	
	op = gda_server_provider_create_operation (server, cnn, 
						   GDA_SERVER_OPERATION_CREATE_TABLE, NULL, error);
	if (GDA_IS_SERVER_OPERATION (op)) {
		va_list  args;
		gchar   *arg;
		GType    type;
		gchar   *dbms_type;
		GError 	*error = NULL;
		xmlDocPtr parameters;
		xmlNodePtr root;
		xmlNodePtr table, op_data, array_data, array_row, array_value;
		
		gint ncol;
		
		g_return_val_if_fail (table_name != NULL 
				      || num_columns > 0 
				      || !gda_connection_is_opened (cnn), FALSE);
		
		
		
		/* Initation of the xmlDoc */
		parameters = xmlNewDoc ("1.0");
		
		root = xmlNewDocNode (parameters, NULL, "serv_op_data", NULL);
		xmlDocSetRootElement (parameters, root);
		table = xmlNewChild (root, NULL, "op_data", table_name);
		xmlSetProp (table, "path", "/TABLE_DEF_P/TABLE_NAME");

		op_data = xmlNewChild (root, NULL, "op_data", NULL);
		xmlSetProp (op_data, "path", "/FIELDS_A");
		array_data = xmlNewChild (op_data, NULL, "gda_array_data", NULL);
			
		va_start (args, 0);
		type = 0;
		arg = NULL;
		
		for (ncol = 0; ncol < num_columns; ncol++) {
			arg = va_arg (args, gchar*);

			if (arg == NULL) {
				g_message ("Error the number of columns are incorrect; couldn't create the table");
				break;
			}
			
			type = va_arg (args, GType);
			if (type == 0) {
				g_message("Error the number of arguments are incorrect; couldn't create the table");
				break;
			}
			
			dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (server, 
											 cnn, type);
			
#ifdef GDA_DEBUG_NO
			g_message("create_table: argument pair #%d 'column name' = '%s'\n   'type' = '%d' dbms_type = '%s'", ncol, arg, (int) type, dbms_type);
#endif			
		    
			array_row = xmlNewChild (array_data, NULL, "gda_array_row", NULL);
			array_value = xmlNewChild (array_row, NULL, "gda_array_value", arg);
			xmlSetProp (array_value, "colid", "COLUMN_NAME");
		
			array_value = xmlNewChild(array_row, NULL, "gda_array_value", dbms_type);
			xmlSetProp(array_value, "colid", "COLUMN_TYPE");
			
		}
		
		va_end(args);
		
#ifdef GDA_DEBUG_NO
		gchar* buffer; 
		gint   buffer_s ize;
		xmlKeepBlanksDefault (0);
		xmlDocDumpFormatMemory (parameters, &buffer, &buffer_size, 1);
		g_message ("XML to Create New Table:\n%s", buffer);
		xmlFree (buffer);
#endif		
		
		if (!gda_server_operation_load_data_from_xml (op, root, &error))
			/* error */
			retval = FALSE;
		else {
			if (gda_server_provider_perform_operation (server, cnn, op, &error))
				/* error */
				retval = FALSE;
		}
		g_object_unref (op);
	}
	else {
		g_message("The Server doesn't support the CREATE TABLE operation!\n\n");
		retval = FALSE;
	}

	return retval;
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
    
		parameters = xmlNewDoc ("1.0");
		root = xmlNewDocNode (parameters, NULL, "serv_op_data", NULL);
		xmlDocSetRootElement (parameters, root);
		table = xmlNewChild (root, NULL, "op_data", table_name);
		xmlSetProp (table, "path", "/TABLE_DESC_P/TABLE_NAME");
	    
#ifdef GDA_DEBUG
		gchar *buffer;
		gint   buffer_size;
		xmlKeepBlanksDefault (0);
		xmlDocDumpFormatMemory (parameters, &buffer, &buffer_size, 1);
		g_message ("XML to Create New Table:\n%s", buffer);
		xmlFree (buffer);
#endif
		if (!gda_server_operation_load_data_from_xml (op, root, error))
			/* error */
			retval = FALSE;
		else {
			if (gda_server_provider_perform_operation (server, cnn, op, error))
				/* error */
				retval = FALSE;
		}
		g_object_unref (op);
	}
	else {
		g_message("The Server doesn't support the DROP TABLE operation!\n\n");
		retval = FALSE;
	}
    
	return retval;
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
 * gda_insert_row_into_table
 * @cnn: an opened connection
 * @table_name:
 * @error: a place to store errors, or %NULL
 * @...: a list of strings to be converted as value, finished by %NULL
 * 
 * This is just a convenient function to insert a row with the values given as argument.
 * The values must be strings that could be converted to the type in the corresponding
 * column. Finish the list with NULL.
 * 
 * The arguments must be pairs of column name followed by his value.
 * 
 * Returns: TRUE if no error occurred, and FALSE and set error otherwise
 */
gboolean
gda_insert_row_into_table (GdaConnection *cnn, const gchar *table_name, GError **error, ...)
{
	GdaDict         *dict;
	GdaDictDatabase *db;
	GdaDictTable    *table;
        
	g_return_val_if_fail (GDA_IS_CONNECTION (cnn), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnn), FALSE);

	/* Setting up the Dictionary for the connection */	
	dict = gda_dict_new ();
	gda_dict_set_connection (dict, cnn);
	db = gda_dict_get_database (dict);
	gda_dict_update_dbms_meta_data (dict, 0, NULL, NULL);
	
	/* Getting the Table from the Dictionary */
	table = gda_dict_database_get_table_by_name (db, table_name);
	if(!GDA_IS_DICT_TABLE(table)) {
		g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_OBJECT_NAME_ERROR, 
			    "The table '%s' doesn't exist", table_name);
		return FALSE;
	}
    
	/* Getting the field's values given as function's parameters */    
	va_list args;
	gchar* arg;
	GSList *table_fields = NULL, *flist = NULL;
	GList *values = NULL;
	GdaValueArgument *ra;
    
	va_start (args, error);
	table_fields = gda_entity_get_fields (GDA_ENTITY(table));
    	while ((arg = va_arg (args, gchar*))) {
		GdaDictField *tfield;
        
		ra = g_new0 (GdaValueArgument, 1);
		ra->column_name = g_strdup (arg);
		flist = table_fields;
		while(flist) {
			tfield = GDA_DICT_FIELD (flist->data);
			if (g_str_equal(arg, gda_object_get_name (GDA_OBJECT(tfield))))
				break;
			flist = g_slist_next(flist);
		}
        
		if(flist) {
			arg = va_arg(args, gchar*);
			ra->value = gda_value_new_from_string (arg, 
					  gda_entity_field_get_g_type(GDA_ENTITY_FIELD(tfield)));
			values = g_list_prepend(values, ra);
		}
		else {
			g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_OBJECT_NAME_ERROR, 
				    "The column '%s' doesn't exist in the table '%s'", arg, table_name);
			return -1;
		}
		values = g_list_prepend(values, ra);
	}

	/* Constructing the INSERT GdaQuery */
	GdaQuery        *query;
	GdaQueryTarget  *target;
	gint i;
    
	query = gda_query_new (dict);
	gda_query_set_query_type (query, GDA_QUERY_TYPE_INSERT);
	target = gda_query_target_new (query, table_name);
	gda_query_add_target (query, target, NULL);
    
	flist = table_fields;
	i = 0;
    
	while (flist) {
		GdaDictField *tfield;
		GList *vlist = NULL;
		GdaValueArgument *ar;
		
		tfield = GDA_DICT_FIELD (flist->data);
		vlist = g_list_first (values);
        
		while (vlist) {
			ar = (GdaValueArgument*) vlist->data;
			if (g_str_equal (ar->column_name, gda_object_get_name (GDA_OBJECT(tfield))))
				break;
			vlist = g_list_next(vlist);
		}
		if (vlist) {
			GdaQueryFieldField *qfieldfield;
			GdaQueryFieldValue *qfieldvalue;
			gchar *str;
			
			/* Set the field in the INSERT INTO table_name (field) */
			qfieldfield = g_object_new (GDA_TYPE_QUERY_FIELD_FIELD,
						    "dict", dict,
						    "query", query,
						    "target", target,
						    "field", tfield,
						    NULL);
			gda_object_set_name (GDA_OBJECT (qfieldfield), 
					     gda_object_get_name (GDA_OBJECT (tfield)));
		    gda_entity_add_field (GDA_ENTITY (query), GDA_ENTITY_FIELD (qfieldfield));

		    /* Set the value asociated with that field; 
		       part of the VALUES in the INSERT command */
		    qfieldvalue = (GdaQueryFieldValue *) gda_query_field_value_new (query, 
				  gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield)));
		    gda_query_field_set_visible (GDA_QUERY_FIELD (qfieldvalue), FALSE);
		    str = g_strdup_printf ("+%d", i++);
		    gda_object_set_name (GDA_OBJECT (qfieldvalue), str);
		    g_free (str);
		    gda_query_field_value_set_is_parameter (GDA_QUERY_FIELD_VALUE (qfieldvalue), 
							    TRUE);
		    gda_query_field_value_set_not_null (GDA_QUERY_FIELD_VALUE (qfieldvalue), 
							!gda_dict_field_is_null_allowed (tfield));
		    ar = (GdaValueArgument*) vlist->data;
		    
		    if (G_IS_VALUE (ar->value))
			    gda_query_field_value_set_value (GDA_QUERY_FIELD_VALUE (qfieldvalue), 
							     ar->value);
		    else
			    gda_query_field_value_set_default_value (GDA_QUERY_FIELD_VALUE (qfieldvalue), 
								     gda_dict_field_get_default_value (tfield));            
		    gda_entity_add_field (GDA_ENTITY (query), GDA_ENTITY_FIELD (qfieldvalue));
		    g_object_set (qfieldfield, "value-provider", qfieldvalue, NULL);
		    
		    g_object_unref (G_OBJECT (qfieldvalue));
		    g_object_unref (G_OBJECT (qfieldfield));
		}
        
		flist = g_slist_next (flist);
	}
	
	
	/* Execute the GdaQuery */
	gda_query_execute (query, NULL, FALSE, error);
    
	g_object_unref (G_OBJECT(target));
	g_object_unref (G_OBJECT(query));
	g_object_unref (G_OBJECT(dict));
    
	g_list_free (values);
	
	if (error)
		return FALSE;
	else 
		return TRUE;
}

/**
 * gda_update_value_in_table
 * @cnn: an opened connection
 * @table_name:
 * @search_for_column:
 * @condition:
 * @column_name:
 * @new_value:
 * @error: a place to store errors, or %NULL
 * 
 * This is just a convenient function to update values in a table on a given column where
 * the row is fitting the given condition.
 *
 * Returns: TRUE if no error occurred
 */
gboolean              
gda_update_value_in_table (GdaConnection *cnn, 
			   const gchar *table_name, const gchar *search_for_column, 
			   const GValue *condition, 
			   const gchar *column_name, const GValue *new_value, GError **error)
{
	GdaDict         *dict;
	GdaDictDatabase *db;
	GdaDictTable    *table;
    
	g_return_val_if_fail (GDA_IS_CONNECTION (cnn), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnn), FALSE);
    
	/* Setting up the Dictionary for the connection */
	dict = gda_dict_new ();
	gda_dict_set_connection (dict, cnn);
	db = gda_dict_get_database (dict);
	gda_dict_update_dbms_meta_data (dict, 0, NULL, NULL);
    
	/* Getting the Table from the Dictionary */
	table = gda_dict_database_get_table_by_name (db, table_name);
    
	if (!GDA_IS_DICT_TABLE (table)) {
		g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_OBJECT_NAME_ERROR, 
			    "The table '%s' doesn't exist", table_name);
		return FALSE;
	}
    
	/* Constructing the UPDATE GdaQuery */
	GdaQuery        *query;
	GdaQueryTarget  *target;
	GSList          *table_fields = NULL, *flist = NULL;
	GdaQueryFieldField *fcond;
	GdaQueryFieldValue *condvalue;
	GdaQueryCondition    *cond, *newcond;
	gint i;
    
	query = gda_query_new (dict);
	gda_query_set_query_type (query, GDA_QUERY_TYPE_UPDATE);
	target = gda_query_target_new (query, table_name);
	gda_query_add_target (query, target, NULL);
	table_fields = gda_entity_get_fields (GDA_ENTITY(table));
	cond = gda_query_condition_new(query, GDA_QUERY_CONDITION_NODE_AND);
	flist = table_fields;
	i = 0;
    
	while (flist) {
		GdaDictField *tfield;
		GdaQueryFieldField *qfieldfield;
		GdaQueryFieldValue *qfieldvalue;
	   	gchar *str;
		
		tfield = GDA_DICT_FIELD (flist->data);

        
		if (g_str_equal (column_name, gda_object_get_name (GDA_OBJECT (tfield)))) {
			if (G_IS_VALUE (new_value)) {
				/* Set the field in the UPDATE table_name SET field */
				qfieldfield = g_object_new (GDA_TYPE_QUERY_FIELD_FIELD,
							    "dict", dict,
							    "query", query,
	    	    					    "target", target,
	    	    					    "field", tfield,
	    	    					    NULL);
				gda_object_set_name (GDA_OBJECT (qfieldfield), 
						     gda_object_get_name (GDA_OBJECT (tfield)));
				gda_entity_add_field (GDA_ENTITY (query), 
						      GDA_ENTITY_FIELD (qfieldfield));
    
				/* Set the value asociated with that field; 
				   part of the SET field = value in the UPDATE command */
				qfieldvalue = (GdaQueryFieldValue *) gda_query_field_value_new (query, 
					      gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield)));
				gda_query_field_set_visible (GDA_QUERY_FIELD (qfieldvalue), FALSE);
				str = g_strdup_printf ("+%d", i++);
				gda_object_set_name (GDA_OBJECT (qfieldvalue), str);
				g_free (str);
				gda_query_field_value_set_is_parameter (GDA_QUERY_FIELD_VALUE (qfieldvalue), TRUE);
				gda_query_field_value_set_not_null (GDA_QUERY_FIELD_VALUE (qfieldvalue), 
	    							    !gda_dict_field_is_null_allowed (tfield));
    
				if(G_VALUE_TYPE(new_value) == gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield)))
					gda_query_field_value_set_value (GDA_QUERY_FIELD_VALUE (qfieldvalue), new_value);
				else {
					g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_INCORRECT_VALUE_ERROR, 
						    "The given New Value's Type '%s', doesn't correspond with the field type: '%s'",
						    g_type_name (G_VALUE_TYPE (new_value)), 
						    g_type_name (gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield))));
					
					g_object_unref (G_OBJECT (qfieldvalue));
					g_object_unref (G_OBJECT (qfieldfield));
					return FALSE;
				}
        		    
				gda_entity_add_field (GDA_ENTITY (query), 
						      GDA_ENTITY_FIELD (qfieldvalue));
				g_object_set (qfieldfield, "value-provider", qfieldvalue, NULL);
				
				g_object_unref (G_OBJECT (qfieldvalue));
				g_object_unref (G_OBJECT (qfieldfield));
			}
			else {
				g_set_error (error, GDA_GENERAL_ERROR, GDA_GENERAL_INCORRECT_VALUE_ERROR, 
					    "The given Argument Value is invalid");
				return FALSE;
			}
		}
        
		if (g_str_equal (search_for_column, gda_object_get_name (GDA_OBJECT (tfield)))) {
			if(G_IS_VALUE(condition)) {
				/* Set the field in the UPDATE table_name WHERE field */
				fcond = g_object_new (GDA_TYPE_QUERY_FIELD_FIELD,
						      "dict", dict,
						      "query", query,
						      "target", target,
						      "field", tfield,
						      NULL);
				gda_object_set_name (GDA_OBJECT (fcond), 
						     gda_object_get_name (GDA_OBJECT (tfield)));
	   	        
				condvalue = GDA_QUERY_FIELD_VALUE (gda_query_field_value_new (query, 
											      gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield))));
				gda_query_field_set_visible (GDA_QUERY_FIELD (condvalue), FALSE);
				str = g_strdup_printf ("-%d", i++);
				gda_object_set_name (GDA_OBJECT (condvalue), str);
				g_free (str);
	               
				
				gda_query_field_value_set_is_parameter (GDA_QUERY_FIELD_VALUE (condvalue), TRUE);
				gda_query_field_value_set_not_null (GDA_QUERY_FIELD_VALUE (condvalue), 
								    !gda_dict_field_is_null_allowed (tfield));
	            								    
				if (G_VALUE_TYPE (condition) == gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield)))
					gda_query_field_value_set_value(GDA_QUERY_FIELD_VALUE(condvalue), condition);
				else {
					g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_INCORRECT_VALUE_ERROR, 
						    "The given Condition Value's Type '%s', doesn't correspond with the field type: '%s'",
						    g_type_name (G_VALUE_TYPE (condition)), 
						    g_type_name (gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield))));
					return FALSE;
				}
			}
			else {
				g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_INCORRECT_VALUE_ERROR, 
					    "The given Condition Value is invalid");
				return FALSE;
			}
	        
			newcond = gda_query_condition_new (query, GDA_QUERY_CONDITION_LEAF_EQUAL);
			
			gda_query_condition_leaf_set_operator (newcond, GDA_QUERY_CONDITION_OP_LEFT, GDA_QUERY_FIELD (fcond));
			gda_query_condition_leaf_set_operator (newcond, GDA_QUERY_CONDITION_OP_RIGHT, GDA_QUERY_FIELD (condvalue));
			
			gda_query_condition_node_add_child (cond, newcond, NULL);
			
			g_object_unref (newcond);
			g_object_unref(condvalue);
			g_object_unref(fcond);
		}
        
		flist = g_slist_next (flist);
	} 
	
	gda_query_set_condition (query, cond);
	g_object_unref (cond);
    
	/* Execute the GdaQuery */
	gda_query_execute (query, NULL, FALSE, error);
    
	g_object_unref(G_OBJECT(target));
	g_object_unref(G_OBJECT(query));
	g_object_unref(G_OBJECT(dict));
	
	if(error)
		return FALSE;
	else
		return TRUE;
}

/**
 * gda_update_values_in_table
 * @cnn: an opened connection
 * @table_name:
 * @search_for_column:
 * @condition:
 * @column_name:
 * @new_value:
 * @error: a place to store errors, or %NULL
 * 
 * This is just a convenient function to update values in a table on a given column where
 * the row is fitting the given condition.
 *
 * Returns: TRUE if no error occurred
 */
gboolean              
gda_update_values_in_table (GdaConnection *cnn, const gchar *table_name, 
			    const gchar *condition_column_name, const GValue *condition, 
			    GError **error, ...)
{
	/* Setting up the table to update */
	GdaDict         *dict;
	GdaDictDatabase *db;
	GdaDictTable    *table;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnn), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnn), FALSE);

	/* Setting up the Dictionary for the connection */
	dict = gda_dict_new ();
	gda_dict_set_connection (dict, cnn);
	db = gda_dict_get_database (dict);    
	gda_dict_update_dbms_meta_data (dict, 0, NULL, NULL);
    
	/* Getting the Table from the Dictionary */
	table = gda_dict_database_get_table_by_name (db, table_name);
        if (!GDA_IS_DICT_TABLE (table)) {
		g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_OBJECT_NAME_ERROR, 
			    "The table '%s' doesn't exist", table_name);
		return FALSE;
	}
    
	/* Getting the field's values given as function's parameters */
	va_list args;
	gchar* arg;
	GValue* val;
	GSList *table_fields = NULL, *flist = NULL;
	GList *values = NULL;
	GdaValueArgument *ra;
    
	va_start (args, error);
	table_fields = gda_entity_get_fields (GDA_ENTITY(table));
	
	while ((arg = va_arg(args, gchar*))) {
		GdaDictField *tfield;
		
		ra = g_new0 (GdaValueArgument, 1);
		ra->column_name = g_strdup (arg);
		flist = table_fields;
		while (flist) {
			tfield = GDA_DICT_FIELD (flist->data);
			if (g_str_equal (arg, gda_object_get_name (GDA_OBJECT(tfield))))
				break;
			flist = g_slist_next(flist);
		}
        
		if (flist) {
			val = va_arg (args, GValue*);
            
			if (G_IS_VALUE (val)) {
	                                    
				if (G_VALUE_TYPE (val) == 
				    gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield))) {
					ra->value = gda_value_copy (val);
					values = g_list_prepend (values, ra);
				}
				else {
					g_set_error (error, GDA_GENERAL_ERROR, GDA_GENERAL_INCORRECT_VALUE_ERROR, 
						     "The given Argument Value's Type '%s', doesn't correspond with the field type: '%s'",
						     g_type_name (G_VALUE_TYPE (val)), 
						     g_type_name (gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield))));
					return FALSE;
				}
			}
			else {
				g_set_error (error, GDA_GENERAL_ERROR, GDA_GENERAL_INCORRECT_VALUE_ERROR, 
					    "The Given Argument Value is invalid");
				return FALSE;
			}
		}
		else {
			g_set_error (error, GDA_GENERAL_ERROR, GDA_GENERAL_OBJECT_NAME_ERROR, 
				     "The column '%s' doesn't exist in the table '%s'", arg, table_name);
			return FALSE;
		}
	}

	/* Constructing the UPDATE GdaQuery */
	GdaQuery        *query;
	GdaQueryTarget  *target;
	GdaQueryFieldField *fcond;
	GdaQueryFieldValue *condvalue;
	GdaQueryCondition    *cond, *newcond;
	gint i;
    
	query = gda_query_new (dict);
	gda_query_set_query_type (query, GDA_QUERY_TYPE_UPDATE);
	target = gda_query_target_new (query, table_name);
	gda_query_add_target (query, target, NULL);
	table_fields = gda_entity_get_fields (GDA_ENTITY(table));
	cond = gda_query_condition_new(query, GDA_QUERY_CONDITION_NODE_AND);
	flist = table_fields;
	i = 0;
    
	while (flist) {
		GdaDictField *tfield;
		GdaValueArgument *ar;
		GdaQueryFieldField *qfieldfield;
		GdaQueryFieldValue *qfieldvalue;
	   	gchar *str;
	   	GList * vlist = NULL;
		
		tfield = GDA_DICT_FIELD (flist->data);
		vlist = g_list_first (values);
		while (vlist) {
			ar = (GdaValueArgument*) vlist->data;
			if (g_str_equal (ar->column_name,
					 gda_object_get_name (GDA_OBJECT (tfield))))
				break;
			vlist = g_list_next (vlist);
		}
		if (vlist) {
			if (g_str_equal (ar->column_name, 
					 gda_object_get_name (GDA_OBJECT (tfield)))) {   
				/* Set the field in the UPDATE table_name SET field */
				qfieldfield = g_object_new (GDA_TYPE_QUERY_FIELD_FIELD,
							    "dict", dict,
							    "query", query,
		    					    "target", target,
		       					    "field", tfield,
		       					    NULL);
				gda_object_set_name (GDA_OBJECT (qfieldfield), 
						     gda_object_get_name (GDA_OBJECT (tfield)));
				gda_entity_add_field (GDA_ENTITY (query), 
						      GDA_ENTITY_FIELD (qfieldfield));
    
				/* Set the value asociated with that field; 
				   part of the SET field = value in the UPDATE command */
				qfieldvalue = (GdaQueryFieldValue *) gda_query_field_value_new (query, 
												gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield)));
				gda_query_field_set_visible (GDA_QUERY_FIELD (qfieldvalue), FALSE);
				str = g_strdup_printf ("+%d", i++);
				gda_object_set_name (GDA_OBJECT (qfieldvalue), str);
				g_free (str);
				gda_query_field_value_set_is_parameter (GDA_QUERY_FIELD_VALUE (qfieldvalue), TRUE);
				gda_query_field_value_set_not_null (GDA_QUERY_FIELD_VALUE (qfieldvalue), 
		    						    !gda_dict_field_is_null_allowed (tfield));
    
				gda_query_field_value_set_value (GDA_QUERY_FIELD_VALUE (qfieldvalue),
								 ar->value);
				
				gda_entity_add_field (GDA_ENTITY (query), 
						      GDA_ENTITY_FIELD (qfieldvalue));
				g_object_set (qfieldfield, "value-provider", qfieldvalue, NULL);
	        		
				g_object_unref (G_OBJECT (qfieldvalue));
				g_object_unref (G_OBJECT (qfieldfield));
			}
		}
        
		if (g_str_equal (condition_column_name, 
				 gda_object_get_name (GDA_OBJECT (tfield)))) {
			if (G_IS_VALUE (condition)) {
				/* Set the field in the UPDATE table_name WHERE field */
				fcond = g_object_new (GDA_TYPE_QUERY_FIELD_FIELD,
						      "dict", dict,
						      "query", query,
						      "target", target,
						      "field", tfield,
						      NULL);
				gda_object_set_name (GDA_OBJECT (fcond), 
						     gda_object_get_name (GDA_OBJECT (tfield)));
	    	        
				condvalue = GDA_QUERY_FIELD_VALUE (gda_query_field_value_new (query, 
											      gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield))));
				gda_query_field_set_visible (GDA_QUERY_FIELD (condvalue), FALSE);
				str = g_strdup_printf ("-%d", i++);
				gda_object_set_name (GDA_OBJECT (condvalue), str);
				g_free (str);
	                
				gda_query_field_value_set_is_parameter (GDA_QUERY_FIELD_VALUE (condvalue), TRUE);
				gda_query_field_value_set_not_null (GDA_QUERY_FIELD_VALUE (condvalue), 
								    !gda_dict_field_is_null_allowed (tfield));
				
				
				if (G_VALUE_TYPE(condition) == 
				    gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield)))
					gda_query_field_value_set_value(GDA_QUERY_FIELD_VALUE(condvalue), condition);
				else {
					g_set_error (error, GDA_GENERAL_ERROR, GDA_GENERAL_INCORRECT_VALUE_ERROR, 
						     "The given Condition Value's Type '%s', doesn't correspond with the field type: '%s'",
						     g_type_name (G_VALUE_TYPE (condition)), 
						     g_type_name (gda_entity_field_get_g_type (GDA_ENTITY_FIELD (tfield))));
					g_object_unref (condvalue);
					g_object_unref (fcond);
					return FALSE;
				}
				newcond = gda_query_condition_new (query, 
								   GDA_QUERY_CONDITION_LEAF_EQUAL);
            
				gda_query_condition_leaf_set_operator (newcond, 
								       GDA_QUERY_CONDITION_OP_LEFT, GDA_QUERY_FIELD (fcond));
	        
				gda_query_condition_leaf_set_operator (newcond, 
								       GDA_QUERY_CONDITION_OP_RIGHT, GDA_QUERY_FIELD (condvalue));
                    
				gda_query_condition_node_add_child (cond, newcond, NULL);
				
				g_object_unref (newcond);
				g_object_unref (condvalue);
				g_object_unref (fcond);
			}
			else {
				g_set_error(error, GDA_GENERAL_ERROR, GDA_GENERAL_INCORRECT_VALUE_ERROR, 
					    "The given Condition Value is invalid");
				
				return FALSE;
			}
		}
		flist = g_slist_next (flist);
	}
    
	gda_query_set_condition (query, cond);
	g_object_unref (cond);
    
	/* Execute the GdaQuery */
	/*g_print("===> SQL update Values is %s \n", gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, 0, NULL) ); */
	gda_query_execute (query, NULL, FALSE, error);
    
	g_object_unref (G_OBJECT (target));
	g_object_unref (G_OBJECT (query));
	g_object_unref (G_OBJECT (dict));
    
	if(error)
		return FALSE;
	else 
		return TRUE;
}
