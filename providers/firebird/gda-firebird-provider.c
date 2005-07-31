/* GDA FireBird Provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <libgda/gda-intl.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-command.h>
#include <glib/gprintf.h>
#include <string.h>
#include "gda-firebird-provider.h"
#include "gda-firebird-recordset.h"

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
static gboolean 	gda_firebird_provider_create_database_cnc (GdaServerProvider *provider,
					  		           GdaConnection *cnc,
							           const gchar *name);
static gboolean 	gda_firebird_provider_drop_database_cnc (GdaServerProvider *provider,
								 GdaConnection *cnc,
								 const gchar *name);
static GList 		*gda_firebird_provider_execute_command (GdaServerProvider *provider,
								GdaConnection *cnc,
								GdaCommand *cmd,
								GdaParameterList *params);
static gboolean 	gda_firebird_provider_begin_transaction (GdaServerProvider *provider,
								 GdaConnection *cnc,
								 GdaTransaction *xaction);
static gboolean 	gda_firebird_provider_commit_transaction (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GdaTransaction *xaction);
static gboolean 	gda_firebird_provider_rollback_transaction (GdaServerProvider *provider,
								    GdaConnection *cnc,
								    GdaTransaction *xaction);
static gboolean 	gda_firebird_provider_supports (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaConnectionFeature feature);
static GdaDataModel	*gda_firebird_provider_get_schema (GdaServerProvider *provider,
							   GdaConnection *cnc,
							   GdaConnectionSchema schema,
							   GdaParameterList *params);


static GObjectClass *parent_class = NULL;

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
	provider_class->open_connection = gda_firebird_provider_open_connection;
	provider_class->close_connection = gda_firebird_provider_close_connection;
	provider_class->get_server_version = gda_firebird_provider_get_server_version;
	provider_class->get_database = gda_firebird_provider_get_database;
	provider_class->change_database = gda_firebird_provider_change_database;
	provider_class->create_database_cnc = gda_firebird_provider_create_database_cnc;
	provider_class->drop_database_cnc = gda_firebird_provider_drop_database_cnc;
	provider_class->execute_command = gda_firebird_provider_execute_command;
	provider_class->begin_transaction = gda_firebird_provider_begin_transaction;
	provider_class->commit_transaction = gda_firebird_provider_commit_transaction;
	provider_class->rollback_transaction = gda_firebird_provider_rollback_transaction;
	provider_class->supports = gda_firebird_provider_supports;
	provider_class->get_schema = gda_firebird_provider_get_schema;
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

	if (!type) {
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
		GdaValueType type;
	} types[] = {
		{ "blob", "", "Binary large object (blob)", GDA_VALUE_TYPE_BINARY },
		{ "char", "", "Fixed length string", GDA_VALUE_TYPE_STRING },
		{ "varchar", "", "Variable length string", GDA_VALUE_TYPE_STRING },
		{ "date", "", "Date", GDA_VALUE_TYPE_DATE },
		{ "time", "", "Time", GDA_VALUE_TYPE_TIME },
		{ "timestamp", "", "Time stamp", GDA_VALUE_TYPE_TIMESTAMP },
		{ "smallint", "", "Signed short integer", GDA_VALUE_TYPE_SMALLINT },
		{ "integer", "", "Signed long integer (longword)", GDA_VALUE_TYPE_INTEGER },
		{ "decimal", "", "Decimal number", GDA_VALUE_TYPE_NUMERIC },
		{ "numeric", "", "Decimal number", GDA_VALUE_TYPE_NUMERIC },
		{ "float", "", "Single precision number", GDA_VALUE_TYPE_SINGLE },
		{ "double precision", "", "Double precision number", GDA_VALUE_TYPE_DOUBLE }
	};

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	
	/* Create the recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (4);
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Type"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 1, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 2, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 3, _("GDA type"));
	
	/* Fill the recordset with each of the values from the array defined above */
	for (i = 0; i < (sizeof (types) / sizeof (types[0])); i++) {
		value_list = NULL;
		
		value_list = g_list_append (value_list, gda_value_new_string (types[i].name));
		value_list = g_list_append (value_list, gda_value_new_string (types[i].owner));
		value_list = g_list_append (value_list, gda_value_new_string (types[i].comments));
		value_list = g_list_append (value_list, gda_value_new_gdatype (types[i].type));

		/* Add values to row */
		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list);

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
	GList *reclist, *value_list;
	GdaDataModel *recset = NULL;
	GdaTransaction *transaction;
	GdaCommand *command;
	GdaValue *value;
	GdaRow *row;
	GdaParameter *par = NULL;
	gchar *sql, *sql_cond;
	gint i;
	gboolean systables = FALSE;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	transaction = gda_transaction_new ("temp_transaction");
	if (gda_connection_begin_transaction (cnc, transaction)) {

		/* Find parameters */
		if (params)
			par = gda_parameter_list_find (params, "systables");

		/* Initialize parameter values */
		if (par)
			systables = gda_value_get_boolean ((GdaValue *) gda_parameter_get_value (par));
	
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
		sql = g_strconcat ("SELECT RDB$RELATION_NAME,RDB$OWNER_NAME FROM RDB$RELATIONS ", sql_cond, NULL);
		
		/* Execute statement */
		command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		gda_command_set_transaction (command, transaction);		
		reclist = gda_connection_execute_command (cnc, command, NULL);

		if (reclist) {
			recset = gda_data_model_array_new (4);
			gda_data_model_set_column_title (recset, 0, _("Table"));
			gda_data_model_set_column_title (recset, 1, _("Owner"));
			gda_data_model_set_column_title (recset, 2, _("Description"));
			gda_data_model_set_column_title (recset, 3, _("Definition"));			
			for (i = 0; i < gda_data_model_get_n_rows (GDA_DATA_MODEL (reclist->data)); i++) {
				value_list = NULL;

				/* Get name of table */
				row = (GdaRow *) gda_data_model_get_row (GDA_DATA_MODEL (reclist->data), i);
				value = gda_row_get_value (row, 0);
				value_list = g_list_append (
						value_list,
						gda_value_new_string ((gchar *) gda_value_get_string (value)));

				/* Get owner of table */
				value = gda_row_get_value (row, 1);
				value_list = g_list_append (
						value_list,
						gda_value_new_string ((gchar *) gda_value_get_string (value)));
				
				/* FIXME: Set correct values */
				value_list = g_list_append (value_list, gda_value_new_string (""));
				value_list = g_list_append (value_list, gda_value_new_string (""));

				gda_data_model_append_values (recset, value_list);
				g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
				g_list_free (value_list);
			}

			g_list_free (reclist);
		}

		g_free (sql);
		gda_connection_rollback_transaction (cnc, transaction);
		gda_command_free (command);
	}
	
	g_object_unref (transaction);
	
	return recset;
}


/*
 *  fb_type_name_to_gda_type
 *
 *  Function convert a string containing the name of Firebird's
 *  data type to it's corresponding gda data type.
 *
 *  Returns: The GdaValueType corresponding to @name Firebird's
 *           type
 */
static GdaValueType
fb_type_name_to_gda_type (const gchar *name)
{
	if (!strcmp (name, "CHAR") || !strcmp (name, "VARCHAR"))
		return GDA_VALUE_TYPE_STRING;
	else if (!strcmp (name, "SMALLINT"))
		return GDA_VALUE_TYPE_SMALLINT;
	else if (!strcmp (name, "NUMERIC") || !strcmp (name, "DECIMAL"))
		return GDA_VALUE_TYPE_NUMERIC;
	else if (!strcmp (name, "INTEGER"))
		return GDA_VALUE_TYPE_INTEGER;
	else if (!strcmp (name, "BLOB"))
		return GDA_VALUE_TYPE_BLOB;
	else if (!strcmp (name, "TIMESTAMP"))
		return GDA_VALUE_TYPE_TIMESTAMP;
	else if (!strcmp (name, "TIME"))
		return GDA_VALUE_TYPE_TIME;
	else if (!strcmp (name, "DATE"))
		return GDA_VALUE_TYPE_DATE;
	else if (!strcmp (name, "INT64"))
		return GDA_VALUE_TYPE_NUMERIC;
	else if (!strcmp (name, "FLOAT"))
		return GDA_VALUE_TYPE_SINGLE;
	else if (!strcmp (name, "DOUBLE"))
		return GDA_VALUE_TYPE_DOUBLE;

	return GDA_VALUE_TYPE_STRING;
}


/*
 *  fb_set_index_field_metad
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
	GdaTransaction *transaction;
	GdaCommand *command;
	gchar *sql, *field_name, *recset_field_name;
	gint i,j;
	GList *reclist;
	GdaValue *value, *recset_value;
	
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_DATA_MODEL (recset));

	transaction = gda_transaction_new ("tmp_transaction_mdata");
	if (gda_connection_begin_transaction (cnc, transaction)) {
		sql = g_strdup_printf (
				"SELECT A.RDB$FIELD_NAME, I.RDB$UNIQUE_FLAG, B.RDB$CONSTRAINT_TYPE "
				"FROM RDB$INDEX_SEGMENTS A, RDB$RELATION_CONSTRAINTS B, RDB$INDICES I "
				"WHERE ((B.RDB$CONSTRAINT_TYPE = 'PRIMARY KEY') "
				"OR (I.RDB$UNIQUE_FLAG = 1)) AND (A.RDB$INDEX_NAME = B.RDB$INDEX_NAME) "
				"AND (B.RDB$RELATION_NAME = '%s') AND (A.RDB$INDEX_NAME = I.RDB$INDEX_NAME) "
				"ORDER BY A.RDB$FIELD_POSITION",
				table_name);
		command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		gda_command_set_transaction (command, transaction);
		reclist = gda_connection_execute_command (cnc, command, NULL);
		if (reclist) {
			gda_data_model_freeze (recset);
			
			/* For each index or unique field in statement */
			for (i = 0; i < gda_data_model_get_n_rows (GDA_DATA_MODEL (reclist->data)); i++) {
				value = (GdaValue *) gda_data_model_get_value_at (GDA_DATA_MODEL (reclist->data), 0, i);
				field_name = (gchar *) gda_value_get_string (value);
				j = -1;

				/* Find column in @recset by name */
				do {
					j++;
					recset_value = (GdaValue *) gda_data_model_get_value_at (recset, 0, j);
					recset_field_name = (gchar *) gda_value_get_string (recset_value);
				} while (strcmp (recset_field_name, field_name) && (j < gda_data_model_get_n_rows (recset)));
				
				/* Set recset values */
				if (!strcmp (recset_field_name, field_name)) {
					
					/* Get primary key value from statement */
					value = (GdaValue *) gda_data_model_get_value_at (GDA_DATA_MODEL (reclist->data),
											  2,
											  i);
					
					/* Get recorset GdaValue for primary key (position 5) */
					recset_value = (GdaValue *) gda_data_model_get_value_at (recset, 5, j);

					/* Set Primary Key mark */
					gda_value_set_boolean (recset_value, (!strcmp (
										gda_value_get_string (value),
										"PRIMARY KEY")));
					
					/* Get unique index value from statement */
					value = (GdaValue *) gda_data_model_get_value_at (GDA_DATA_MODEL (reclist->data),
											  1,
											  i);
					
					/* Get recorset GdaValue for unique index (position 6) */
					recset_value = (GdaValue *) gda_data_model_get_value_at (recset, 6, j);

					/* Set Unique Index mark */
					gda_value_set_boolean (recset_value, (gda_value_get_smallint (value) == 1));
				}
			}
			
			gda_data_model_thaw (recset);
			g_list_free (reclist);
		}
		
		g_free (sql);
		gda_connection_rollback_transaction (cnc, transaction);
		gda_command_free (command);						
	}

	g_object_unref (transaction);
}


/*
 *  fb_set_field_metad
 *
 *  Fill a list with metadata values from @row.
 *  This function is called from #fb_get_fields_metadata.
 *
 *  Returns: A GList of GdaValues
 */
static GList *
fb_set_field_metadata (GdaRow *row)
{
	GList *value_list = NULL;
	GdaValue *value;
	gchar *the_value, *tmp_value;
	gshort scale, short_value;

	g_return_val_if_fail (row != NULL, NULL);

	/* Set field name  */
	value = gda_row_get_value (row, 0);
	the_value = (gchar *) gda_value_get_string (value);
	value_list = g_list_append (value_list,	gda_value_new_string (the_value));
	
	/* Get Scale (number of decimals) */
	value = gda_row_get_value (row, 4);
	if (!gda_value_is_null (value))
		scale = gda_value_get_smallint (value);
	else
		scale = 0;
	
	/* Set data type */
	value = gda_row_get_value (row, 1);
	tmp_value = (gchar *) gda_value_get_string (value);
	tmp_value = g_strchomp (tmp_value);
	the_value = g_ascii_strdown (tmp_value, -1);
	g_free (tmp_value);
	
	/* Set correct value name */
	if (!strcmp (the_value, "long") && (scale < 0))
		value_list = g_list_append (value_list, gda_value_new_string ("decimal"));
	else if (!strcmp (the_value, "long"))
		value_list = g_list_append (value_list, gda_value_new_string ("integer"));
	else if (!strcmp (the_value, "int64"))
		value_list = g_list_append (value_list, gda_value_new_string ("numeric"));
	else if (!strcmp (the_value, "varying"))
		value_list = g_list_append (value_list, gda_value_new_string ("varchar"));
	else if (!strcmp (the_value, "text"))
		value_list = g_list_append (value_list, gda_value_new_string ("char"));
	else if (!strcmp (the_value, "short"))
		value_list = g_list_append (value_list, gda_value_new_string ("smallint"));
	else if (!strcmp (the_value, "double"))
		value_list = g_list_append (value_list, gda_value_new_string ("double precision"));
	else
		value_list = g_list_append (value_list, gda_value_new_string (the_value));

	/* Set size */
	short_value = 0;
	if (fb_type_name_to_gda_type (the_value) == GDA_VALUE_TYPE_STRING) {
		value = gda_row_get_value (row, 2);
		short_value = gda_value_get_smallint (value);
	}
	else if (fb_type_name_to_gda_type (the_value) == GDA_VALUE_TYPE_NUMERIC) {
		value = gda_row_get_value (row, 3);
		short_value = gda_value_get_smallint (value);
	}
	value_list = g_list_append (value_list, gda_value_new_integer (short_value));

	/* Set scale */	
	value_list = g_list_append (value_list, gda_value_new_integer ((scale * -1)));

	/* Set not null indicator */
	value = gda_row_get_value (row, 5);
	if (!gda_value_is_null (value))
		value_list = g_list_append (
				value_list,
				gda_value_new_boolean ((gda_value_get_smallint (value) == 1)));
	else
		value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));

	/* Primary key ? (real value set in #fb_set_index_field_metadata) */
	value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));

	/* Unique key ? (real value set in #fb_set_index_field_metadata) */
	value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));

	/* FIXME: References */
	value_list = g_list_append (value_list, gda_value_new_string (""));

	/* FIXME: Default value */
	value_list = g_list_append (value_list, gda_value_new_string (""));

	return value_list;
}


/*
 *  fb_get_fields_metad
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
	GdaTransaction *transaction;
	GList *value_list, *reclist;
	GdaRow *row;
	GdaDataModelArray *recset = NULL;
	GdaParameter *par;
	GdaCommand *command;
	gint i;
	gchar *sql;
	const gchar *table_name;
	struct {
	  	const gchar *name;
		GdaValueType type;
	} fields_desc[] = {
		{ N_("Field name")      , GDA_VALUE_TYPE_STRING  },
		{ N_("Data type")       , GDA_VALUE_TYPE_STRING  },
		{ N_("Size")            , GDA_VALUE_TYPE_INTEGER },
		{ N_("Scale")           , GDA_VALUE_TYPE_INTEGER },
		{ N_("Not null?")       , GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Primary key?")    , GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Unique index?")   , GDA_VALUE_TYPE_BOOLEAN },
		{ N_("References")      , GDA_VALUE_TYPE_STRING  },
		{ N_("Default value")   , GDA_VALUE_TYPE_STRING  }
	};

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);
	
	par = gda_parameter_list_find (params, "name");
	if (!par) {
		gda_connection_add_event_string (cnc,
				_("Table name is needed but none specified in parameter list"));
		return NULL;
	}

	table_name = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
	if (!table_name) {
		gda_connection_add_event_string (cnc,
				_("Table name is needed but none specified in parameter list"));
		return NULL;
	}
	
	transaction = gda_transaction_new ("temp_transaction");
	if (gda_connection_begin_transaction (cnc, transaction)) {
		sql = g_strdup_printf (
				"SELECT A.RDB$FIELD_NAME, C.RDB$TYPE_NAME, B.RDB$FIELD_LENGTH, "
				"B.RDB$FIELD_PRECISION, B.RDB$FIELD_SCALE, A.RDB$NULL_FLAG "
				"FROM RDB$RELATION_FIELDS A, RDB$FIELDS B, RDB$TYPES C "
				"WHERE (A.RDB$RELATION_NAME = '%s') AND (A.RDB$FIELD_SOURCE = B.RDB$FIELD_NAME)"
				" AND (C.RDB$FIELD_NAME = 'RDB$FIELD_TYPE') AND (C.RDB$TYPE = B.RDB$FIELD_TYPE)"
				" ORDER BY A.RDB$FIELD_POSITION",
				table_name);
		command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		gda_command_set_transaction (command, transaction);
		reclist = gda_connection_execute_command (cnc, command, NULL);
		if (reclist) {
			/* Create and fill recordset to be returned */
			recset = (GdaDataModelArray *) gda_data_model_array_new (9);
			for (i = 0; i < (sizeof (fields_desc) / sizeof (fields_desc[0])); i++)
				gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i,
								 _(fields_desc[i].name));
			
			for (i = 0; i < gda_data_model_get_n_rows (GDA_DATA_MODEL (reclist->data)); i++) {
				value_list = NULL;
				row = (GdaRow *) gda_data_model_get_row (GDA_DATA_MODEL (reclist->data), i);
				
				/* Set field metdata for row, then append to recordset */
				value_list = fb_set_field_metadata (row);
				gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list);

				/* Free temp list of values */
				g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
				g_list_free (value_list);
			}
			
			fb_set_index_field_metadata (cnc, (GdaDataModel *) recset, table_name);
			g_list_free (reclist);
		}

		g_free (sql);
		gda_connection_rollback_transaction (cnc, transaction);
		gda_command_free (command);						
	}

	g_object_unref (transaction);

	return GDA_DATA_MODEL (recset);
}

static void
fb_add_aggregate_row (GdaDataModelArray *recset,
		      const gchar *str,
		      const gchar *comments)
{
	GList *list;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (recset));

	/* 1st the name */
	list = g_list_append (NULL, gda_value_new_string (str));
	/* 2nd the unique id */
	list = g_list_append (list, gda_value_new_string (str));
	/* 3rd the owner */
	list = g_list_append (list, gda_value_new_string (NULL));
	/* 4th the comments */
	list = g_list_append (list, gda_value_new_string (comments));
	/* 5th the return type */
	list = g_list_append (list, gda_value_new_string (_("UNKNOWN")));
	/* 6th the argument type */
	list = g_list_append (list, gda_value_new_string (_("UNKNOWN")));
	/* 7th the SQL definition */
	list = g_list_append (list, gda_value_new_string (NULL));

	gda_data_model_append_values (GDA_DATA_MODEL (recset), list);

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
	recset = (GdaDataModelArray *) gda_data_model_array_new (7);
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Name"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 1, _("ID"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 2, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 3, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 4, _("Return type"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 5, _("Args types"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 6, _("SQL"));

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
	gchar *tmp_msg;
	char buffer[512];
	short sql_code;
	long * ptr_status_vector = fcnc->status;

	/* Get error message */
	isc_interprete (buffer, &ptr_status_vector);
	tmp_msg = g_strdup ((gchar *) buffer);

	/* Look for more messages */
	while (isc_interprete (buffer, &ptr_status_vector))
		tmp_msg = g_strconcat (tmp_msg, "\n", (gchar *) buffer, NULL);

	if (statement_type != 0) {
		sql_code = isc_sqlcode (fcnc->status);
		isc_sql_interprete (sql_code, buffer, sizeof (buffer));
		tmp_msg = g_strconcat  (tmp_msg, "\n", (gchar *) buffer, NULL);
	}
									
	return tmp_msg;
}

void
gda_firebird_connection_make_error (GdaConnection *cnc,
				    const gint statement_type)
{
	GdaConnectionEvent *error;
	GdaFirebirdConnection *fcnc;
	gchar *description;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_event_string (cnc, _("Invalid Firebird handle"));
		return;
	}
	
	error = gda_connection_event_new ();
	gda_connection_event_set_code (error, isc_sqlcode (fcnc->status));
	gda_connection_event_set_source (error, "[GDA Firebird]");
	description = fb_sqlerror_get_description (fcnc, statement_type);
	gda_connection_event_set_description (error, description);
	gda_connection_add_event (cnc, error);	
	g_free (description);
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
		gda_firebird_connection_make_error (cnc, 0);
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
	GdaFirebirdRecordset *recset;
	gchar **arr;
	gint n = 0;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	recset = NULL;

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		while (arr[n]) {
			recset = gda_firebird_recordset_new (cnc, ftr, sql);
  			if (GDA_IS_FIREBIRD_RECORDSET (recset)) {
				gda_data_model_set_command_text ((GdaDataModel*) recset, arr[n]);
				gda_data_model_set_command_type ((GdaDataModel*) recset, GDA_COMMAND_TYPE_SQL);
				reclist = g_list_append (reclist, recset);
			}
			
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
	gboolean commit_tr;
	GdaTransaction *xaction;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);
	
	commit_tr = FALSE;

	/* Get transaction */
	xaction = gda_command_get_transaction (cmd);
	if (!GDA_IS_TRANSACTION (xaction))
		xaction = gda_transaction_new ("local_tr");
	
	/* Get transaction handle */
	ftr = g_object_get_data (G_OBJECT (xaction), TRANSACTION_DATA);
	
	/* Begin transaction if it was not already started  */
	if (!ftr) {

		/* Start transaction */
		commit_tr = gda_firebird_provider_begin_transaction (provider, cnc, xaction);
	
		/* Get transaction handle */
		ftr = g_object_get_data (G_OBJECT (xaction), TRANSACTION_DATA);
	}
	
	/* Parse command */
	switch (gda_command_get_command_type (cmd)) {
		case GDA_COMMAND_TYPE_SQL:
			reclist = gda_firebird_provider_run_sql (reclist, cnc, ftr, gda_command_get_text (cmd));
			break;
		default:
			break;
	}

	/* Commit transaction if it was not started directly by programmer */
	if (commit_tr) {
		gda_firebird_provider_commit_transaction (provider, cnc, xaction);
		g_object_unref (xaction);
	}
	
	return reclist;
}

/* begin_transaction handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_begin_transaction (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 GdaTransaction *xaction)
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
	
	g_object_set_data (G_OBJECT (xaction), TRANSACTION_DATA, ftr);

	return TRUE;
}

/* commit_transaction handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_commit_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GdaTransaction *xaction)
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

	ftr = g_object_get_data (G_OBJECT (xaction), TRANSACTION_DATA);
	if (!ftr) {
		gda_connection_add_event_string (cnc, _("Invalid transaction handle"));
		return FALSE;
	}

	if (isc_commit_transaction (fcnc->status, ftr)) {
		gda_firebird_connection_make_error (cnc, 0);
		result = FALSE;
	}
	else
		result = TRUE;

	g_free (ftr);
	g_object_set_data (G_OBJECT (xaction), TRANSACTION_DATA, NULL);

	return result;
}

/* rollback_transaction handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_rollback_transaction (GdaServerProvider *provider,
					    GdaConnection *cnc,
					    GdaTransaction *xaction)
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

	ftr = g_object_get_data (G_OBJECT (xaction), TRANSACTION_DATA);
	if (!ftr) {
		gda_connection_add_event_string (cnc, _("Invalid transaction handle"));
		return FALSE;
	}

	if (isc_rollback_transaction (fcnc->status, ftr)) {
		gda_firebird_connection_make_error (cnc, 0);
		result = FALSE;
	}
	else
		result = TRUE;

	g_free (ftr);
	g_object_set_data (G_OBJECT (xaction), TRANSACTION_DATA, NULL);

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
			return TRUE;
		case GDA_CONNECTION_FEATURE_TRIGGERS:
		case GDA_CONNECTION_FEATURE_INDEXES:
		case GDA_CONNECTION_FEATURE_PROCEDURES:
		case GDA_CONNECTION_FEATURE_USERS:
		case GDA_CONNECTION_FEATURE_BLOBS:
		default:
			break;
	}

	return FALSE;
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


