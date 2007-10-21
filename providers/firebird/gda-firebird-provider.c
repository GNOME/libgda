/* GDA FireBird Provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-command.h>
#include <libgda/gda-parameter-list.h>
#include <glib/gprintf.h>
#include <string.h>
#include "gda-firebird-provider.h"
#include "gda-firebird-recordset.h"

#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-type.h>
#include <libgda/handlers/gda-handler-bin.h>

#include <libgda/sql-delimiter/gda-sql-delimiter.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-blob-op.h>

static void 		gda_firebird_provider_class_init (GdaFirebirdProviderClass *klass);
static void 		gda_firebird_provider_init (GdaFirebirdProvider *provider,
						    GdaFirebirdProviderClass *klass);
static void 		gda_firebird_provider_finalize (GObject *object);

static const gchar 	*gda_firebird_provider_get_version (GdaServerProvider *provider);
static gboolean 	gda_firebird_provider_open_connection (GdaServerProvider *provider,
							       GdaConnection *cnc,
							       GdaQuarkList *params,
							       const gchar *username,
							       const gchar *password);
static gboolean		gda_firebird_provider_close_connection (GdaServerProvider *provider,
								GdaConnection *cnc);
static const gchar	*gda_firebird_provider_get_server_version (GdaServerProvider *provider,
								   GdaConnection *cnc);
static const gchar 	*gda_firebird_provider_get_database (GdaServerProvider *provider,
							     GdaConnection *cnc);
static gboolean		gda_firebird_provider_change_database (GdaServerProvider *provider,
							       GdaConnection *cnc,
							       const gchar *name);
static GList 		*gda_firebird_provider_execute_command (GdaServerProvider *provider,
								GdaConnection *cnc,
								GdaCommand *cmd,
								GdaParameterList *params);
static gboolean 	gda_firebird_provider_begin_transaction (GdaServerProvider *provider,
								 GdaConnection *cnc,
								 const gchar *name, GdaTransactionIsolation level,
								 GError **error);
static gboolean 	gda_firebird_provider_commit_transaction (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  const gchar *name, GError **error);
static gboolean 	gda_firebird_provider_rollback_transaction (GdaServerProvider *provider,
								    GdaConnection *cnc,
								    const gchar *name, GError **error);
static gboolean 	gda_firebird_provider_supports (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaConnectionFeature feature);

static GdaServerProviderInfo *gda_firebird_provider_get_info (GdaServerProvider *provider,
							      GdaConnection *cnc);

static GdaDataModel	*gda_firebird_provider_get_schema (GdaServerProvider *provider,
							   GdaConnection *cnc,
							   GdaConnectionSchema schema,
							   GdaParameterList *params);

static GdaDataHandler *gda_firebird_provider_get_data_handler (GdaServerProvider *provider,
							       GdaConnection *cnc,
							       GType g_type,
							       const gchar *dbms_type);
static const gchar* gda_firebird_provider_get_default_dbms_type (GdaServerProvider *provider,
								 GdaConnection *cnc,
								 GType type);

static gboolean          begin_transaction_if_not_current (GdaServerProvider *provider,
							   GdaConnection *cnc,
							   const gchar *name, GdaTransactionIsolation level,
							   GError **error, gboolean *has_error);


static GObjectClass *parent_class = NULL;

static const gchar *read_string_in_blob (const GValue *value);


/*
 * GdaFirebirdProvider class implementation
 */
static void
gda_firebird_provider_class_init (GdaFirebirdProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_firebird_provider_finalize;
	provider_class->get_version = gda_firebird_provider_get_version;
	provider_class->get_server_version = gda_firebird_provider_get_server_version;
	provider_class->get_info = gda_firebird_provider_get_info;
	provider_class->supports_feature = gda_firebird_provider_supports;
	provider_class->get_schema = gda_firebird_provider_get_schema;

	provider_class->get_data_handler = gda_firebird_provider_get_data_handler;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = gda_firebird_provider_get_default_dbms_type;

	provider_class->create_connection = NULL;
	provider_class->open_connection = gda_firebird_provider_open_connection;
	provider_class->close_connection = gda_firebird_provider_close_connection;
	provider_class->get_database = gda_firebird_provider_get_database;
	provider_class->change_database = gda_firebird_provider_change_database;

	provider_class->supports_operation = NULL;
        provider_class->create_operation = NULL;
        provider_class->render_operation = NULL;
        provider_class->perform_operation = NULL;

	provider_class->execute_command = gda_firebird_provider_execute_command;
	provider_class->execute_query = NULL;
	provider_class->get_last_insert_id = NULL;

	provider_class->begin_transaction = gda_firebird_provider_begin_transaction;
	provider_class->commit_transaction = gda_firebird_provider_commit_transaction;
	provider_class->rollback_transaction = gda_firebird_provider_rollback_transaction;
	provider_class->add_savepoint = NULL;
	provider_class->rollback_savepoint = NULL;
	provider_class->delete_savepoint = NULL;
}

static void
gda_firebird_provider_init (GdaFirebirdProvider *provider, GdaFirebirdProviderClass *klass)
{
}

static void
gda_firebird_provider_finalize (GObject *object)
{
	GdaFirebirdProvider *fbprv = (GdaFirebirdProvider *) object;

	g_return_if_fail (GDA_IS_FIREBIRD_PROVIDER (fbprv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_firebird_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GTypeInfo info = {
			sizeof (GdaFirebirdProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_firebird_provider_class_init,
			NULL,
			NULL,
			sizeof (GdaFirebirdProvider),
			0,
			(GInstanceInitFunc) gda_firebird_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaFirebirdProvider", &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_firebird_provider_new (void)
{
	GdaFirebirdProvider *fprv;
	
	fprv = g_object_new (GDA_TYPE_FIREBIRD_PROVIDER, NULL);
	
	return GDA_SERVER_PROVIDER (fprv);
}


/*
 *  fb_server_get_version
 *
 *  Gets Firebird connection's server version number
 *
 *  Returns: A string containing server version, or NULL if error
 *           String must be released after use
 */
static gchar *
fb_server_get_version (GdaFirebirdConnection *fcnc)
{
	gchar buffer[254], item, *p_buffer;
	gint length;
	gchar fdb_info[] = {
		isc_info_isc_version,
		isc_info_end
	};
	
	/* Try to get datbase version */
	if (! isc_database_info (fcnc->status, &(fcnc->handle), sizeof (fdb_info), fdb_info,
				 sizeof (buffer), buffer)) {
		p_buffer = buffer;
		if (*p_buffer != isc_info_end) {
			item = *p_buffer++;
			length = isc_vax_integer (p_buffer, 2);
			p_buffer += 2;
			if (item == isc_info_isc_version)
				return g_strndup ((guchar *) &p_buffer[2], length-2);
				
		}		
	}

	return NULL;
}


/*
 *  fb_get_types
 *
 *  Returns: A GdaDataModel containing Firebird's data types
 */
static GdaDataModel *
fb_get_types (GdaConnection *cnc,
	      GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GList *value_list;
	gint i;
	struct {
		const gchar *name;
		const gchar *owner;
		const gchar *comments;
		GType type;
		const gchar *syns;
	} types[] = {
		{ "blob", "", "Binary large object (blob)", GDA_TYPE_BLOB, NULL },
		{ "char", "", "Fixed length string", G_TYPE_STRING, "character" },
		{ "varchar", "", "Variable length string", G_TYPE_STRING, "character varying,varying,char varying" },
		{ "text", "", "Text", G_TYPE_STRING, NULL },
		{ "date", "", "Date", G_TYPE_DATE, NULL },
		{ "time", "", "Time", GDA_TYPE_TIME, NULL },
		{ "timestamp", "", "Time stamp", GDA_TYPE_TIMESTAMP, NULL },
		{ "smallint", "", "Signed short integer", GDA_TYPE_SHORT, "short" },
		{ "integer", "", "Signed long integer", G_TYPE_INT, "long" },
		{ "int64", "", "Signed integer (8 bytes)", G_TYPE_INT64, NULL },
		{ "decimal", "", "Decimal number", GDA_TYPE_NUMERIC, NULL },
		{ "numeric", "", "Decimal number", GDA_TYPE_NUMERIC, NULL },
		{ "float", "", "Single precision number", G_TYPE_FLOAT, NULL },
		{ "double", "", "Double precision number", G_TYPE_DOUBLE, "double precision" }
	};

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	
	/* create the recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TYPES));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_TYPES);
	
	/* Fill the recordset with each of the values from the array defined above */
	for (i = 0; i < (sizeof (types) / sizeof (types[0])); i++) {
		value_list = NULL;
		GValue *tmpval;
		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].name);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].owner);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].comments);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_ulong (tmpval = gda_value_new (G_TYPE_ULONG), types[i].type);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].syns);
		value_list = g_list_append (value_list, tmpval);

		/* Add values to row */
		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

		/* Free temporary list of values */
		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}
	
	return GDA_DATA_MODEL (recset);
}


/*
 *  fb_get_tables
 *
 *  @show_views: If TRUE only return views, else return tables
 *
 *  Function looks for the following parameters in list:
 *    systables (gboolean)
 *
 *  Returns: A GdaDataModel containing Firebird's tables/views
 */
static GdaDataModel *
fb_get_tables (GdaConnection *cnc,
	       GdaParameterList *params,
	       gboolean show_views)
{
	GList *value_list;
	GdaDataModel *recset = NULL;
	GdaCommand *command;
	const GValue *value;
	GdaParameter *par = NULL;
	gchar *sql, *sql_cond;
	gint i;
	gboolean systables = FALSE;
	gboolean trans_started, trans_error;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* Begin transaction if it was not already started  */
	trans_started = begin_transaction_if_not_current (gda_connection_get_provider_obj (cnc), cnc, "temp_transaction", 
							  GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL, &trans_error);

	if (!trans_error) {
		GdaDataModel *recmodel;
		
		/* Find parameters */
		if (params)
			par = gda_parameter_list_find_param (params, "systables");

		/* Initialize parameter values */
		if (par)
			systables = g_value_get_boolean ((GValue *) gda_parameter_get_value (par));
	
		/* Evaluate statement conditions */	
		if (show_views) {
			/* Show only views */
			sql_cond = "WHERE RDB$SYSTEM_FLAG = 0 AND RDB$VIEW_BLR IS NOT NULL";
		}
		else {
			if (systables)
				/* Only show system tables */
				sql_cond = "WHERE RDB$SYSTEM_FLAG = 1 AND RDB$VIEW_BLR IS NULL";
			else
				/* Only show user tables */
				sql_cond = "WHERE RDB$SYSTEM_FLAG = 0 AND RDB$VIEW_BLR IS NULL";
		}
	
		/* Generate complete statement */
		sql = g_strconcat ("SELECT RDB$RELATION_NAME, RDB$OWNER_NAME, RDB$VIEW_SOURCE "
				   "FROM RDB$RELATIONS ", 
				   sql_cond, NULL);
		
		/* Execute statement */
		command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		recmodel = gda_connection_execute_select_command (cnc, command, NULL, NULL);
		if (recmodel) {
			recset = (GdaDataModel *) gda_data_model_array_new 
				(gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TABLES));
			gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_TABLES);
			for (i = 0; i < gda_data_model_get_n_rows (recmodel); i++) {
				GValue *tmpval;

				/* Get name of table */
				value = gda_data_model_get_value_at (recmodel, 0, i);
				value_list = g_list_append (NULL, gda_value_copy (value));

				/* Get owner of table */
				value = gda_data_model_get_value_at (recmodel, 1, i);
				value_list = g_list_append (value_list, gda_value_copy (value));
				
				/* Comments FIXME */
				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
				value_list = g_list_append (value_list, tmpval);

				/* description */
				value = gda_data_model_get_value_at (recmodel, 2, i);
				const gchar *cstr;
				cstr = read_string_in_blob (value);
				if (cstr)
					g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), cstr);
				else
					g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
				value_list = g_list_append (value_list, tmpval);

				gda_data_model_append_values (recset, value_list, NULL);
				g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
				g_list_free (value_list);
			}

			g_object_unref (recmodel);
		}

		g_free (sql);
		if (trans_started)
			gda_connection_rollback_transaction (cnc, "temp_transaction", NULL);
		gda_command_free (command);
	}
	
	return recset;
}


/*
 *  fb_type_name_to_g_type
 *
 *  Function convert a string containing the name of Firebird's
 *  data type to it's corresponding gda data type.
 *
 *  Returns: The GType corresponding to @name Firebird's
 *           type
 */
static GType
fb_type_name_to_g_type (const gchar *name)
{
	if (!strcmp (name, "CHAR") || !strcmp (name, "VARCHAR"))
		return G_TYPE_STRING;
	else if (!strcmp (name, "SMALLINT"))
		return GDA_TYPE_SHORT;
	else if (!strcmp (name, "NUMERIC") || !strcmp (name, "DECIMAL"))
		return GDA_TYPE_NUMERIC;
	else if (!strcmp (name, "INTEGER"))
		return G_TYPE_INT;
	else if (!strcmp (name, "BLOB"))
		return GDA_TYPE_BLOB;
	else if (!strcmp (name, "TIMESTAMP"))
		return GDA_TYPE_TIMESTAMP;
	else if (!strcmp (name, "TIME"))
		return GDA_TYPE_TIME;
	else if (!strcmp (name, "DATE"))
		return G_TYPE_DATE;
	else if (!strcmp (name, "INT64"))
		return GDA_TYPE_NUMERIC;
	else if (!strcmp (name, "FLOAT"))
		return G_TYPE_FLOAT;
	else if (!strcmp (name, "DOUBLE"))
		return G_TYPE_DOUBLE;

	return G_TYPE_STRING;
}


/*
 *  fb_set_index_field_metadata
 *
 *  Sets primary key and unique key properties for fields
 *  in @recset.
 *  This function is called from #fb_get_fields_metadata.
 */
static void
fb_set_index_field_metadata (GdaConnection *cnc,
			     GdaDataModel *recset,
			     const gchar *table_name)
{
	GdaCommand *command;
	gchar *sql, *field_name, *recset_field_name;
	gint i,j;
	GValue *value, *recset_value;
	gboolean trans_started, trans_error;
	
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_DATA_MODEL (recset));

	trans_started = begin_transaction_if_not_current (gda_connection_get_provider_obj (cnc), cnc, "tmp_transaction_mdata", 
							  GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL, &trans_error);

	if (!trans_error) {
		GdaDataModel *recmodel;
		sql = g_strdup_printf (
				"SELECT A.RDB$FIELD_NAME, I.RDB$UNIQUE_FLAG, B.RDB$CONSTRAINT_TYPE "
				"FROM RDB$INDEX_SEGMENTS A, RDB$RELATION_CONSTRAINTS B, RDB$INDICES I "
				"WHERE ((B.RDB$CONSTRAINT_TYPE = 'PRIMARY KEY') "
				"OR (I.RDB$UNIQUE_FLAG = 1)) AND (A.RDB$INDEX_NAME = B.RDB$INDEX_NAME) "
				"AND (B.RDB$RELATION_NAME = '%s') AND (A.RDB$INDEX_NAME = I.RDB$INDEX_NAME) "
				"ORDER BY A.RDB$FIELD_POSITION",
				table_name);
		command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		recmodel = gda_connection_execute_select_command (cnc, command, NULL, NULL);
		if (recmodel) {
			gda_data_model_freeze (recset);
			
			/* For each index or unique field in statement */
			for (i = 0; i < gda_data_model_get_n_rows (recmodel); i++) {
				value = (GValue *) gda_data_model_get_value_at (recmodel, 0, i);
				field_name = (gchar *) g_value_get_string (value);
				j = -1;

				/* Find column in @recset by name */
				do {
					j++;
					recset_value = (GValue *) gda_data_model_get_value_at (recset, 0, j);
					recset_field_name = (gchar *) g_value_get_string (recset_value);
				} while (strcmp (recset_field_name, field_name) && 
					 (j < gda_data_model_get_n_rows (recset)));
				
				/* Set recset values */
				if (!strcmp (recset_field_name, field_name)) {
					
					/* Get primary key value from statement */
					value = (GValue *) gda_data_model_get_value_at (recmodel,
											  2,
											  i);
					
					/* Get recorset GValue for primary key (position 5) */
					recset_value = (GValue *) gda_data_model_get_value_at (recset, 5, j);

					/* Set Primary Key mark */
					g_value_set_boolean (recset_value, (!strcmp (
										g_value_get_string (value),
										"PRIMARY KEY")));
					
					/* Get unique index value from statement */
					value = (GValue *) gda_data_model_get_value_at (recmodel,
											  1,
											  i);
					
					/* Get recorset GValue for unique index (position 6) */
					recset_value = (GValue *) gda_data_model_get_value_at (recset, 6, j);

					/* Set Unique Index mark */
					g_value_set_boolean (recset_value, (gda_value_get_short (value) == 1));
				}
			}
			
			gda_data_model_thaw (recset);
			g_object_unref (recmodel);
		}
		
		g_free (sql);
		if (trans_started)
			gda_connection_rollback_transaction (cnc, "tmp_transaction_mdata", NULL);
		gda_command_free (command);
	}
}

static const gchar *
read_string_in_blob (const GValue *value)
{
	gchar *str = NULL;
	
	if (!gda_value_is_null (value)) {
		GdaBlob *blob;
		blob = (GdaBlob*) gda_value_get_blob (value);
		if (!blob)
			g_warning ("Blob created by Firebird provider without a GdaBlob!");
		else if (! blob->op)
			g_warning ("Blob created by Firebird provider without a GdaBlobOp!");
		else {
			if (gda_blob_op_read_all (blob->op, blob)) 
				str = (gchar *) ((GdaBinary *)blob)->data;
			else
				g_warning ("Can't read BLOB contents");
		}
	}
	return str;
}

static GdaDataModel *
fb_get_field_dimensions (GdaConnection *cnc, const gchar *field_name)
{
	gboolean trans_started, trans_error;
	GdaDataModel *model = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (field_name, NULL);
	
	/* Begin transaction if it was not already started  */
	trans_started = begin_transaction_if_not_current (gda_connection_get_provider_obj (cnc), cnc, "temp_transaction", 
							  GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL, &trans_error);

	if (!trans_error) {
		gchar *sql;
		GdaCommand *command;
		sql = g_strdup_printf (
				"SELECT RDB$DIMENSION, RDB$LOWER_BOUND, RDB$UPPER_BOUND "
				"FROM RDB$FIELD_DIMENSIONS WHERE RDB$FIELD_NAME='%s' ORDER BY RDB$DIMENSION",
				field_name);
		command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		model = gda_connection_execute_select_command (cnc, command, NULL, NULL);
		g_free (sql);
		if (trans_started)
			gda_connection_rollback_transaction (cnc, "temp_transaction", NULL);
		gda_command_free (command);						
	}

	return model;
}

/*
 *  fb_set_field_metadata
 *
 *  Fill a list with metadata values from @row.
 *  This function is called from #fb_get_fields_metadata.
 *
 *  Returns: A GList of GValues
 */
static GList *
fb_set_field_metadata (GdaConnection *cnc, GdaDataModel *model, gint row)
{
	GList *value_list = NULL;
	const GValue *value;
	GType field_gtype;
	gshort scale, short_value;
	GValue *tmpval;
	const gchar *str;

	/* Set field name  */
	value = gda_data_model_get_value_at (model, 0, row);
	value_list = g_list_append (value_list,	gda_value_copy (value));
		
	/* Set data type */
	value = gda_data_model_get_value_at (model, 7, row);
	tmpval = NULL;
	if (!gda_value_is_null (value)) {
		const GValue *value_type;

		value_type = gda_data_model_get_value_at (model, 6, row);
		if ((g_value_get_int (value_type) != 261) && (g_value_get_int (value_type) != 14)) { 
			/* Not for BLOB or Char types */
			if (g_value_get_int (value) == 1) {
				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "numeric");
				field_gtype = GDA_TYPE_NUMERIC;
			}
			else if (g_value_get_int (value) == 2) {
				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "decimal");
				field_gtype = GDA_TYPE_NUMERIC;
			}
		}
	}
	if (!tmpval) {
		gchar *the_value;
		value = gda_data_model_get_value_at (model, 1, row);
		the_value = g_ascii_strdown (g_value_get_string (value), -1);
		g_value_take_string (tmpval = gda_value_new (G_TYPE_STRING), g_strchomp (the_value));
		field_gtype = fb_type_name_to_g_type (the_value);
	}
	value_list = g_list_append (value_list, tmpval);

	/* Set size */
	short_value = 0;
	if (field_gtype == G_TYPE_STRING) {
		value = gda_data_model_get_value_at (model, 2, row);
		short_value = gda_value_get_short (value);
	}
	else if (field_gtype == GDA_TYPE_NUMERIC) {
		value = gda_data_model_get_value_at (model, 3, row);
		short_value = gda_value_get_short (value);
	}
	g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), short_value);
	value_list = g_list_append (value_list, tmpval);

	/* Set scale (number of decimals) */
	value = gda_data_model_get_value_at (model, 4, row);
	if (!gda_value_is_null (value))
		scale = gda_value_get_short (value);
	else
		scale = 0;
	g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), scale * -1);
	value_list = g_list_append (value_list, tmpval);

	/* Set not null indicator */
	value = gda_data_model_get_value_at (model, 5, row);
	if (!gda_value_is_null (value))
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), (gda_value_get_short (value) == 1));
	else
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
	value_list = g_list_append (value_list, tmpval);

	/* Primary key ? (real value set in #fb_set_index_field_metadata) */
	g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
	value_list = g_list_append (value_list, tmpval);

	/* Unique key ? (real value set in #fb_set_index_field_metadata) */
	g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
	value_list = g_list_append (value_list, tmpval);

	/* FIXME: References */
	tmpval = gda_value_new_null ();
	value_list = g_list_append (value_list, tmpval);

	/* Default value */
	tmpval = NULL;
	value = gda_data_model_get_value_at (model, 8, row);
	str = read_string_in_blob (value);
	if (str) {
		if (!strncmp (str, "DEFAULT ", 8))
			g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), str + 8);
		else
			g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), str);
	}
	if (!tmpval) {
		value = gda_data_model_get_value_at (model, 9, row);
		str = read_string_in_blob (value);
		if (str) {
			if (!strncmp (str, "DEFAULT ", 8))
				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), str + 8);
			else
				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), str);
		}
	}
	if (!tmpval) 
		tmpval = gda_value_new_null ();
	value_list = g_list_append (value_list, tmpval);

	/* extra attributes FIXME: association GENERATOR <-> field name done through triggers :( */
	tmpval = NULL;
	value = gda_data_model_get_value_at (model, 10, row);
	if (!gda_value_is_null (value) && (g_value_get_int (value) > 0)) {
		/* here is an array */
		GString *string = NULL;

		tmpval = (GValue*) gda_data_model_get_value_at (model, 11, row);
		if (gda_value_is_null (tmpval))
			g_warning ("Firebird provider unexpected NULL RDB$FIELD_NAME");
		else {
			GdaDataModel *dims;
			gint i, nrows;

			string = g_string_new ("");
			dims = fb_get_field_dimensions (cnc, g_value_get_string (tmpval));
			nrows = gda_data_model_get_n_rows (dims);
			for (i = 0; i < nrows; i++) {
				if (i > 0)
					g_string_append_c (string, ',');
				value = gda_data_model_get_value_at (dims, 1, i);
				if (!gda_value_is_null (value) && (g_value_get_long (value) > 1))
					g_string_append_printf (string, "%ld:", g_value_get_long (value));
				value = gda_data_model_get_value_at (dims, 2, i);
				if (!gda_value_is_null (value))
					g_string_append_printf (string, "%ld", g_value_get_long (value));
			}
			g_object_unref (dims);
		}
		if (string) {
			tmpval = gda_value_new (G_TYPE_STRING);
			gchar *str = g_strdup_printf ("ARRAY [%s]", string->str);
			g_value_take_string (tmpval, str);
			g_string_free (string, TRUE);
		}
		else
			tmpval = NULL;
	}
	if (!tmpval)
		tmpval = gda_value_new_null ();
	value_list = g_list_append (value_list, tmpval);

	return value_list;
}

/*
 *  fb_get_fields_metadata
 *
 *  @params must contain a parameter named "name" with
 *  name of table to describe. ... :-)
 *
 *  Returns: A GdaDataModel containing table metad
 */
static GdaDataModel *
fb_get_fields_metadata (GdaConnection *cnc,
			GdaParameterList *params)
{
	GList *value_list;
	GdaDataModelArray *recset = NULL;
	GdaParameter *par;
	GdaCommand *command;
	gint i;
	gchar *sql;
	const gchar *table_name;
	gboolean trans_started, trans_error;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);
	
	par = gda_parameter_list_find_param (params, "name");
	if (!par) {
		gda_connection_add_event_string (cnc,
				_("Table name is needed but none specified in parameter list"));
		return NULL;
	}

	table_name = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	if (!table_name) {
		gda_connection_add_event_string (cnc,
				_("Table name is needed but none specified in parameter list"));
		return NULL;
	}
	
	/* Begin transaction if it was not already started  */
	trans_started = begin_transaction_if_not_current (gda_connection_get_provider_obj (cnc), cnc, "temp_transaction", 
							  GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL, &trans_error);

	if (!trans_error) {
		GdaDataModel *recmodel;
		sql = g_strdup_printf (
				"SELECT A.RDB$FIELD_NAME, C.RDB$TYPE_NAME, B.RDB$FIELD_LENGTH, "
				"B.RDB$FIELD_PRECISION, B.RDB$FIELD_SCALE, A.RDB$NULL_FLAG, "
				"B.RDB$FIELD_TYPE, B.RDB$FIELD_SUB_TYPE, A.RDB$DEFAULT_SOURCE, B.RDB$DEFAULT_SOURCE, "
				"B.RDB$DIMENSIONS, B.RDB$FIELD_NAME "
				"FROM RDB$RELATION_FIELDS A, RDB$FIELDS B, RDB$TYPES C "
				"WHERE (A.RDB$RELATION_NAME = '%s') AND (A.RDB$FIELD_SOURCE = B.RDB$FIELD_NAME)"
				" AND (C.RDB$FIELD_NAME = 'RDB$FIELD_TYPE') AND (C.RDB$TYPE = B.RDB$FIELD_TYPE)"
				" ORDER BY A.RDB$FIELD_POSITION",
				table_name);
		command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		recmodel = gda_connection_execute_select_command (cnc, command, NULL, NULL);
		if (recmodel) {
			/* Create and fill recordset to be returned */
			recset = (GdaDataModelArray *) gda_data_model_array_new 
				(gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_FIELDS));
			gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_FIELDS);
			
			gint nrows;
			nrows = gda_data_model_get_n_rows (recmodel);
			for (i = 0; i < nrows; i++) {
				/* Set field metdata for row, and append to recordset */
				value_list = fb_set_field_metadata (cnc, recmodel, i);
				GError *er = NULL;
				if (gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, &er) < 0)
					g_print ("==ERR: %s\n", er->message);
				
				/* Free temp list of values */
				g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
				g_list_free (value_list);
			}
			
			fb_set_index_field_metadata (cnc, (GdaDataModel *) recset, table_name);
			g_object_unref (recmodel);
		}

		g_free (sql);
		if (trans_started)
			gda_connection_rollback_transaction (cnc, "temp_transaction", NULL);
		gda_command_free (command);						
	}

	return GDA_DATA_MODEL (recset);
}

static void
fb_add_aggregate_row (GdaDataModelArray *recset,
		      const gchar *str,
		      const gchar *comments)
{
	GList *list;
	GValue *tmpval;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (recset));

	/* 1st the name */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), str);
	list = g_list_append (NULL, tmpval);
	/* 2nd the unique id */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), str);
	list = g_list_append (list, tmpval);
	/* 3rd the owner */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "SYSDBA");
	list = g_list_append (list, tmpval);
	/* 4th the comments */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), comments);
	list = g_list_append (list, tmpval);
	/* 5th the return type */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), _("UNKNOWN"));
	list = g_list_append (list, tmpval);
	/* 6th the argument type */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), _("UNKNOWN"));
	list = g_list_append (list, tmpval);
	/* 7th the SQL definition */
	tmpval = gda_value_new_null ();
	list = g_list_append (list, tmpval);

	gda_data_model_append_values (GDA_DATA_MODEL (recset), list, NULL);

	g_list_foreach (list, (GFunc) gda_value_free, NULL);
	g_list_free (list);
}

static GdaDataModel *
fb_get_aggregates (GdaConnection *cnc,
		   GdaParameterList *params)
{
	GdaDataModelArray *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* Create recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_AGGREGATES));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_AGGREGATES);

	/* Fill recordset */
	/*FIXME: Add user defined fuctions (Stored in systables) and set parameters */
	fb_add_aggregate_row (recset, "abs", "comments");
	fb_add_aggregate_row (recset, "acos","comments");
	fb_add_aggregate_row (recset, "ascii_char", "comments");
	fb_add_aggregate_row (recset, "ascii_val", "comments");
	fb_add_aggregate_row (recset, "asin", "comments");
	fb_add_aggregate_row (recset, "atan", "comments");
	fb_add_aggregate_row (recset, "atan2", "comments");
	fb_add_aggregate_row (recset, "bin_and", "comments");
	fb_add_aggregate_row (recset, "bin_or", "comments");
	fb_add_aggregate_row (recset, "bin_xor", "comments");
	fb_add_aggregate_row (recset, "ceiling", "comments");
	fb_add_aggregate_row (recset, "cos", "comments");
	fb_add_aggregate_row (recset, "cosh", "comments");
	fb_add_aggregate_row (recset, "cot", "comments");
	fb_add_aggregate_row (recset, "div", "comments");
	fb_add_aggregate_row (recset, "floor", "comments");
	fb_add_aggregate_row (recset, "ln", "comments");
	fb_add_aggregate_row (recset, "log", "comments");
	fb_add_aggregate_row (recset, "log10", "comments");
	fb_add_aggregate_row (recset, "lower", "comments");
	fb_add_aggregate_row (recset, "ltrim", "comments");
	fb_add_aggregate_row (recset, "mod", "comments");
	fb_add_aggregate_row (recset, "pi", "comments");
	fb_add_aggregate_row (recset, "rand", "comments");
	fb_add_aggregate_row (recset, "rtrim", "comments");
	fb_add_aggregate_row (recset, "sign", "comments");
	fb_add_aggregate_row (recset, "sin", "comments");
	fb_add_aggregate_row (recset, "sinh", "comments");
	fb_add_aggregate_row (recset, "sqrt", "comments");
	fb_add_aggregate_row (recset, "strlen", "comments");
	fb_add_aggregate_row (recset, "substr", "comments");
	fb_add_aggregate_row (recset, "tan", "comments");
	fb_add_aggregate_row (recset, "tanh", "comments");	

	return GDA_DATA_MODEL (recset);
}

/*
 *  fb_sqlerror_get_description
 *
 *  Returns: A message containing a description from a Firebird error
 */
static gchar *
fb_sqlerror_get_description (GdaFirebirdConnection *fcnc,
			     const gint statement_type)
{
	gchar *tmp_msg = NULL;
	gchar *tmp_sql_msg = NULL;
	char buffer[512], *pmsg;
	short sql_code;
	long * ptr_status_vector = fcnc->status;

	pmsg = buffer;
	if ((sql_code = isc_sqlcode (ptr_status_vector)) != 0) {
		isc_sql_interprete ((short) sql_code, pmsg, sizeof (buffer));
		if (strncmp (buffer, "Invalid token", 13))
			tmp_sql_msg = g_strdup (pmsg);
        }

	while (fb_interpret (buffer, sizeof (buffer), (const ISC_STATUS**) &ptr_status_vector)) {
		if (strncmp (buffer, "Dynamic SQL Error", 17) &&
		    strncmp (buffer, "SQL error code =", 16)) {
			gchar *tmp = tmp_msg;
			if (tmp) {
				tmp_msg = g_strconcat (tmp, ": ", pmsg, NULL);
				g_free (tmp);
			}
			else
				tmp_msg = g_strdup (pmsg);
		}
	}
	
	if (tmp_sql_msg) {
		gchar *tmp = tmp_msg;
		if (tmp) {
			tmp_msg = g_strconcat (tmp, ": ", tmp_sql_msg, NULL);
			g_free (tmp);
			g_free (tmp_sql_msg);
		}
		else
			tmp_msg = tmp_sql_msg;
	}
						
	return tmp_msg;
}

GdaConnectionEvent *
gda_firebird_connection_make_error (GdaConnection *cnc,
				    const gint statement_type)
{
	GdaConnectionEvent *error;
	GdaFirebirdConnection *fcnc;
	gchar *description;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) 
		return gda_connection_add_event_string (cnc, _("Invalid Firebird handle"));
	
	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	gda_connection_event_set_code (error, isc_sqlcode (fcnc->status));
	description = fb_sqlerror_get_description (fcnc, statement_type);
	gda_connection_event_set_source (error, "[GDA Firebird]");
	gda_connection_event_set_description (error, description);
	gda_connection_add_event (cnc, error);	
	g_free (description);

	return error;
}
																								
/* get_version handler for the GdaFirebirdProvider class */
static const gchar *
gda_firebird_provider_get_version (GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/* open_connection handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_open_connection (GdaServerProvider *provider,
					GdaConnection *cnc,
					GdaQuarkList *params,
					const gchar *username,
					const gchar *password)
{
	GdaFirebirdConnection *fcnc;
	gchar *fb_db, *fb_user, *fb_password, *fb_charset;
	gchar *dpb;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* get parameters */
	fb_db = (gchar *) gda_quark_list_find (params, "DATABASE");
	if (!fb_db) {
		gda_connection_add_event_string (cnc, _("No database specified"));
		return FALSE;
	}

	if (username)
		fb_user = (gchar *) username;
	else
		fb_user = (gchar *) gda_quark_list_find (params, "USERNAME");

	if (password)
		fb_password = (gchar *) password;
	else
		fb_password = (gchar *) gda_quark_list_find (params, "PASSWORD");

	fb_charset = (gchar *) gda_quark_list_find (params, "CHARACTER_SET");

	fcnc = g_new0 (GdaFirebirdConnection, 1);

	/* prepare connection to firebird server */
	/* Initialize dpb_buffer */
	dpb = fcnc->dpb_buffer;
	*dpb++ = isc_dpb_version1;

	/* Set user name */
	if (fb_user) {
		*dpb++ = isc_dpb_user_name;
		*dpb++ = strlen (fb_user);
		strcpy (dpb, fb_user);
		dpb += strlen (fb_user);
	}

	/* Set password */
	if (fb_password) {
		*dpb++ = isc_dpb_password;
		*dpb++ = strlen (fb_password);
		strcpy (dpb, fb_password);
		dpb += strlen (fb_password);
	}

	/* Set character set */
	if (fb_charset) {
		*dpb++ = isc_dpb_lc_ctype;
		*dpb++ = strlen (fb_charset);
		strcpy (dpb, fb_charset);
		dpb += strlen (fb_charset);
	}

	/* Save dpb length */
	fcnc->dpb_length = dpb - fcnc->dpb_buffer;

	if (isc_attach_database (fcnc->status, strlen (fb_db), fb_db, &(fcnc->handle), fcnc->dpb_length,
				 fcnc->dpb_buffer)) {
		g_object_set_data (G_OBJECT (cnc), CONNECTION_DATA, fcnc);
		gda_firebird_connection_make_error (cnc, 0);
		g_object_set_data (G_OBJECT (cnc), CONNECTION_DATA, NULL);
		g_free (fcnc);
		
		return FALSE;
	}

	fcnc->dbname = g_strdup (fb_db);
	fcnc->server_version = fb_server_get_version (fcnc);

	/* attach the GdaFirebirdConnection struct to the connection */
	g_object_set_data (G_OBJECT (cnc), CONNECTION_DATA, fcnc);

	return TRUE;
}

/* close_connection handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_close_connection (GdaServerProvider *provider,
					GdaConnection *cnc)
{
	GdaFirebirdConnection *fcnc;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_event_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}

	/* detach from database */
	isc_detach_database (fcnc->status, &(fcnc->handle));

	/* free memory */
	g_free (fcnc->dbname);
	g_free (fcnc);
	g_object_set_data (G_OBJECT (cnc), CONNECTION_DATA, NULL);

	return TRUE;
}

/* get_server_version handler for the GdaFirebirdProvider class */
static const gchar *
gda_firebird_provider_get_server_version (GdaServerProvider *provider,
					  GdaConnection *cnc)
{
	GdaFirebirdConnection *fcnc;
	
	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_event_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}

	return ((const gchar *) fcnc->server_version); 	
}

/* get_database handler for the GdaFirebirdProvider class */
static const gchar *
gda_firebird_provider_get_database (GdaServerProvider *provider,
				    GdaConnection *cnc)
{
	GdaFirebirdConnection *fcnc;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_event_string (cnc, _("Invalid Firebird handle"));
		return NULL;
	}

	return (const gchar *) fcnc->dbname;
}

/* change_database handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_change_database (GdaServerProvider *provider,
					GdaConnection *cnc,
					const gchar *name)
{
	return FALSE;
}

/* create_database_cnc handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_create_database_cnc (GdaServerProvider *provider,
					   GdaConnection *cnc,
					   const gchar *name)
{
	gchar *sql;
	GdaFirebirdConnection *fcnc;
	isc_tr_handle ftr = NULL;
	isc_db_handle fdb = NULL;	
	GdaQuarkList *params;
	gchar *fb_sql_dialect, *fb_user, *fb_password;
	gchar *fb_page_size, *fb_char_set;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	
	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_event_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}
	
	/* Get conection parameter list */
	params = gda_quark_list_new_from_string (gda_connection_get_cnc_string (cnc));

	/* Get parameters for database creation */	
	fb_user = (gchar *) gda_connection_get_username (cnc);
	fb_password = (gchar *) gda_connection_get_password (cnc);
	fb_sql_dialect = (gchar *) gda_quark_list_find (params, "SQL_DIALECT");
	
	if (!fb_sql_dialect)
		fb_sql_dialect = "3";
		
	fb_page_size = (gchar *) gda_quark_list_find (params, "PAGE_SIZE");
	
	/* Valid values = 1024, 2048, 4096 and 8192 */
	if (!fb_page_size)
		fb_page_size = "4096";
		
	fb_char_set = (gchar *) gda_quark_list_find (params, "CHARACTER_SET");
	
	if (!fb_char_set)
		fb_char_set = "NONE";
	
	/* Create database */	
	sql = g_strdup_printf ("CREATE DATABASE '%s' USER '%s' PASSWORD '%s' PAGE_SIZE %s DEFAULT CHARACTER SET %s;",
				name, fb_user, fb_password, fb_page_size, fb_char_set);
				
	/* NOTE: Transaction and Database handles should be NULL in this case */
	if (isc_dsql_execute_immediate (fcnc->status, &fdb, &ftr, 0, sql, (gushort) atoi (fb_sql_dialect), NULL)) {
		gda_firebird_connection_make_error (cnc, 0);
		return FALSE;
	}

	gda_quark_list_free (params);
	
	/* we need to detach from the newly created database */
	isc_detach_database (fcnc->status, &fdb);

	return TRUE;
}

static GList *
gda_firebird_provider_run_sql (GList *reclist,
			       GdaConnection *cnc,
			       isc_tr_handle *ftr,
			       const gchar *sql)
{
	GObject *res = NULL;
	gchar **arr;
	gint n = 0;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = gda_delimiter_split_sql (sql);
	if (arr) {
		while (arr[n]) {
			GdaDataModel *recset;
			GdaConnectionEvent *event = NULL;

			recset = (GdaDataModel *) gda_firebird_recordset_new (cnc, ftr, arr [n], &res, &event);
  			if (GDA_IS_FIREBIRD_RECORDSET (recset)) {
				g_assert (!res);
				g_object_set (G_OBJECT (recset), "command_text", arr[n],
					      "command_type", GDA_COMMAND_TYPE_SQL, NULL);
				reclist = g_list_append (reclist, recset);
			}
			else if (res) 
				reclist = g_list_append (reclist, res);
			else
				reclist = g_list_append (reclist, NULL);

			gda_connection_internal_treat_sql (cnc, sql, event);
			n++;
		}
		
		g_strfreev(arr);
	}

	return reclist;
}

/* drop_database_cnc handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_drop_database_cnc (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 const gchar *name)
{
	GdaFirebirdConnection *fcnc;
	isc_db_handle db_handle = NULL;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	/* Get current firebird connection */
	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_event_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}
	
	/* Make connection to database */
	if (isc_attach_database (fcnc->status, strlen (name), (gchar *) name, &(db_handle),
				 fcnc->dpb_length, fcnc->dpb_buffer)) {
		gda_firebird_connection_make_error (cnc, 0);
		return FALSE;
	}
	
	/* Drop database */
	if (isc_drop_database (fcnc->status, &(db_handle))) {
		gda_firebird_connection_make_error (cnc, 0);
		return FALSE;
	}
	
	return TRUE;
}

/* execute_command handler for the GdaFirebirdProvider class */
static GList *
gda_firebird_provider_execute_command (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaCommand *cmd,
				       GdaParameterList *params)
{
	isc_tr_handle *ftr;
	GList *reclist = NULL;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);
	
	/* Begin transaction if it was not already started 
	 * REM: don't close the transaction because otherwise blobs won't be readable anymore */
	begin_transaction_if_not_current (provider, cnc, 
					  "local_tr", 
					  GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL, NULL);
	ftr = g_object_get_data (G_OBJECT (cnc), TRANSACTION_DATA);
	
	/* Parse command */
	switch (gda_command_get_command_type (cmd)) {
		case GDA_COMMAND_TYPE_SQL:
			reclist = gda_firebird_provider_run_sql (reclist, cnc, ftr, gda_command_get_text (cmd));
			break;
		default:
			break;
	}
	
	return reclist;
}

/*
 * Starts a new transaction is one is not yet running
 * @has_error: set to TRUE if there was an error while starting a transaction
 * Returns: TRUE if a new transaction was just started
 */
static gboolean
begin_transaction_if_not_current (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  const gchar *name, GdaTransactionIsolation level,
				  GError **error, gboolean *has_error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	if (g_object_get_data (G_OBJECT (cnc), TRANSACTION_DATA)) {
		/* aalready in a transaction */
		if (has_error)
			*has_error = FALSE;
		return FALSE;
	}
	else {
		gboolean newtrans;
		newtrans = gda_firebird_provider_begin_transaction (provider, cnc, name, level, error);
		if (has_error)
			*has_error = !newtrans;
		return newtrans;
	}
}


/* begin_transaction handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_begin_transaction (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 const gchar *name, GdaTransactionIsolation level,
					 GError **error)
{
	GdaFirebirdConnection *fcnc;
	isc_tr_handle *ftr;
	static char tpb[] = {
		isc_tpb_version3,
		isc_tpb_write,
		isc_tpb_read_committed,
		isc_tpb_rec_version,
		isc_tpb_wait
	};

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_event_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}

	/* start the transaction */
	ftr = g_new0 (isc_tr_handle, 1);
	if (isc_start_transaction (fcnc->status, ftr, 1, &(fcnc->handle),
				   (unsigned short) sizeof (tpb), &tpb)) {
		gda_firebird_connection_make_error (cnc, 0);
		g_free (ftr);
		
		return FALSE;
	}
	
	g_object_set_data (G_OBJECT (cnc), TRANSACTION_DATA, ftr);
	gda_connection_internal_transaction_started (cnc, NULL, name, level);

	return TRUE;
}

/* commit_transaction handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_commit_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  const gchar *name, GError **error)
{
	GdaFirebirdConnection *fcnc;
	isc_tr_handle *ftr;
	gboolean result;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_event_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}

	ftr = g_object_get_data (G_OBJECT (cnc), TRANSACTION_DATA);
	if (!ftr) {
		gda_connection_add_event_string (cnc, _("Invalid transaction handle"));
		return FALSE;
	}

	if (isc_commit_transaction (fcnc->status, ftr)) {
		gda_firebird_connection_make_error (cnc, 0);
		result = FALSE;
	}
	else {
		gda_connection_internal_transaction_committed (cnc, name);
		result = TRUE;
	}

	g_free (ftr);
	g_object_set_data (G_OBJECT (cnc), TRANSACTION_DATA, NULL);

	return result;
}

/* rollback_transaction handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_rollback_transaction (GdaServerProvider *provider,
					    GdaConnection *cnc,
					    const gchar *name, GError **error)
{
	GdaFirebirdConnection *fcnc;
	isc_tr_handle *ftr;
	gboolean result;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_event_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}

	ftr = g_object_get_data (G_OBJECT (cnc), TRANSACTION_DATA);
	if (!ftr) {
		gda_connection_add_event_string (cnc, _("Invalid transaction handle"));
		return FALSE;
	}

	if (isc_rollback_transaction (fcnc->status, ftr)) {
		gda_firebird_connection_make_error (cnc, 0);
		result = FALSE;
	}
	else {
		gda_connection_internal_transaction_rolledback (cnc, name);
		result = TRUE;
	}

	g_free (ftr);
	g_object_set_data (G_OBJECT (cnc), TRANSACTION_DATA, NULL);

	return result;
}

/* supports handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_supports (GdaServerProvider *provider,
				GdaConnection *cnc,
				GdaConnectionFeature feature)
{
	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);

	switch (feature) {
		case GDA_CONNECTION_FEATURE_VIEWS:
		case GDA_CONNECTION_FEATURE_SQL:
		case GDA_CONNECTION_FEATURE_TRANSACTIONS:
		case GDA_CONNECTION_FEATURE_AGGREGATES:
		case GDA_CONNECTION_FEATURE_BLOBS:
		case GDA_CONNECTION_FEATURE_TRIGGERS:
		case GDA_CONNECTION_FEATURE_INDEXES:
		case GDA_CONNECTION_FEATURE_PROCEDURES:
		case GDA_CONNECTION_FEATURE_USERS:
			return TRUE;
		default:
			break;
	}

	return FALSE;
}

static GdaServerProviderInfo *
gda_firebird_provider_get_info (GdaServerProvider *provider, GdaConnection *cnc)
{
	static GdaServerProviderInfo info = {
		"Firebird",
		TRUE, 
		TRUE,
		TRUE,
		TRUE,
		TRUE,
		FALSE
	};
	
	return &info;
}

/* get_schema handler for the GdaFirebirdProvider class */
static GdaDataModel *
gda_firebird_provider_get_schema (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  GdaConnectionSchema schema,
				  GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	switch (schema) {
		case GDA_CONNECTION_SCHEMA_TYPES:
			return fb_get_types (cnc, params);
		case GDA_CONNECTION_SCHEMA_TABLES:
			return fb_get_tables (cnc, params, FALSE);
		case GDA_CONNECTION_SCHEMA_VIEWS:
			return fb_get_tables (cnc, params, TRUE);
		case GDA_CONNECTION_SCHEMA_FIELDS:
			return fb_get_fields_metadata (cnc, params);
		case GDA_CONNECTION_SCHEMA_AGGREGATES:
			return fb_get_aggregates (cnc, params);
/*		case GDA_CONNECTION_SCHEMA_INDEXES:
			return get_firebird_indexes (cnc, params);
		case GDA_CONNECTION_SCHEMA_PROCEDURES:
			return get_firebird_procedures (cnc, params);
		case GDA_CONNECTION_SCHEMA_USERS:
			return get_firebird_users (cnc, params);
		case GDA_CONNECTION_SCHEMA_TRIGGERS:
			return get_firebird_triggers (cnc, params);*/
		default:
			break;
	}

	return NULL;
}

static GdaDataHandler *
gda_firebird_provider_get_data_handler (GdaServerProvider *provider,
					GdaConnection *cnc,
					GType type,
					const gchar *dbms_type)
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
	    (type == G_TYPE_ULONG) ||
	    (type == G_TYPE_UINT)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_numerical_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT64, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DOUBLE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_NUMERIC, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_FLOAT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_SHORT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_USHORT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_CHAR, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UCHAR, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UINT, NULL);
			g_object_unref (dh);
		}
	}
	else if ((type == G_TYPE_DATE) ||
		 (type == GDA_TYPE_TIME) ||
		 (type == GDA_TYPE_TIMESTAMP)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_time_new ();
			gda_handler_time_set_sql_spec   ((GdaHandlerTime *) dh, G_DATE_YEAR,
							 G_DATE_MONTH, G_DATE_DAY, '-', FALSE);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DATE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIME, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIMESTAMP, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == G_TYPE_STRING) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_string_new_with_provider (provider, cnc);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_STRING, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == GDA_TYPE_BLOB) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_bin_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_BLOB, NULL);
			g_object_unref (dh);
		}
	}

	return dh;
}
	
static const gchar* 
gda_firebird_provider_get_default_dbms_type (GdaServerProvider *provider,
					     GdaConnection *cnc,
					     GType type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	if (type == G_TYPE_INT64)
		return "int64";
	if (type == G_TYPE_UINT64)
		return "FIXME";
	if (type == GDA_TYPE_BLOB)
		return "blob";
	if (type == G_TYPE_BOOLEAN)
		return "smallint";
	if (type == G_TYPE_DATE)
		return "date";
	if (type == G_TYPE_DOUBLE)
		return "double";
	if (type == G_TYPE_INT)
		return "integer";
	if (type == GDA_TYPE_NUMERIC)
		return "numeric";
	if (type == G_TYPE_FLOAT)
		return "float";
	if (type == GDA_TYPE_SHORT)
		return "smallint";
	if (type == GDA_TYPE_USHORT)
		return "integer";
	if (type == G_TYPE_STRING)
		return "varchar";
	if (type == GDA_TYPE_TIME)
		return "time";
	if (type == GDA_TYPE_TIMESTAMP)
		return "timestamp";
	if (type == G_TYPE_CHAR)
		return "smallint";
	if (type == G_TYPE_UCHAR)
		return "smallint";
	if (type == G_TYPE_ULONG)
		return "int64";
        if (type == G_TYPE_UINT)
		return "int64";

	return "text";
}
