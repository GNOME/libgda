/* GDA Default provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Carlos Perelló Marín <carlos@gnome-db.org>
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

#include <stdlib.h>
#include <libgda/gda-row.h>
#include <libgda/gda-server-recordset.h>
#include <libgda/gda-server-recordset-model.h>
#include <libgda/gda-util.h>
#include "gda-default.h"
#include "gda-default-recordset.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_DEFAULT_HANDLE "GDA_Default_DefaultHandle"
#define OBJECT_DATA_DEFAULT_RECORDSET "GDA_Default_DefaultRecordset"

static void gda_default_provider_class_init (GdaDefaultProviderClass *klass);
static void gda_default_provider_init       (GdaDefaultProvider *provider,
					     GdaDefaultProviderClass *klass);
static void gda_default_provider_finalize   (GObject *object);

static gboolean gda_default_provider_open_connection (GdaServerProvider *provider,
						      GdaServerConnection *cnc,
						      GdaQuarkList *params,
						      const gchar *username,
						      const gchar *password);
static gboolean gda_default_provider_close_connection (GdaServerProvider *provider,
						       GdaServerConnection *cnc);
static GList *gda_default_provider_execute_command (GdaServerProvider *provider,
						    GdaServerConnection *cnc,
						    GdaCommand *cmd,
						    GdaParameterList *params);
static gboolean gda_default_provider_begin_transaction (GdaServerProvider *provider,
							GdaServerConnection *cnc,
							const gchar *trans_id);
static gboolean gda_default_provider_commit_transaction (GdaServerProvider *provider,
							 GdaServerConnection *cnc,
							 const gchar *trans_id);
static gboolean gda_default_provider_rollback_transaction (GdaServerProvider *provider,
							   GdaServerConnection *cnc,
							   const gchar *trans_id);
static gboolean gda_default_provider_supports (GdaServerProvider *provider,
					       GdaServerConnection *cnc,
					       GNOME_Database_Feature feature);
static GdaServerRecordset *gda_default_provider_get_schema (GdaServerProvider *provider,
							    GdaServerConnection *cnc,
							    GNOME_Database_Connection_Schema schema,
							    GdaParameterList *params);

static GObjectClass *parent_class = NULL;

/*
 * GdaDefaultProvider class implementation
 */

static void
gda_default_provider_class_init (GdaDefaultProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_default_provider_finalize;
	provider_class->open_connection = gda_default_provider_open_connection;
	provider_class->close_connection = gda_default_provider_close_connection;
	provider_class->execute_command = gda_default_provider_execute_command;
	provider_class->begin_transaction = gda_default_provider_begin_transaction;
	provider_class->commit_transaction = gda_default_provider_commit_transaction;
	provider_class->rollback_transaction = gda_default_provider_rollback_transaction;
	provider_class->supports = gda_default_provider_supports;
	provider_class->get_schema = gda_default_provider_get_schema;
}

static void
gda_default_provider_init (GdaDefaultProvider *myprv,
			   GdaDefaultProviderClass *klass)
{
}

static void
gda_default_provider_finalize (GObject *object)
{
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) object;

	g_return_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_default_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaDefaultProviderClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_default_provider_class_init,
				NULL, NULL,
				sizeof (GdaDefaultProvider),
				0,
				(GInstanceInitFunc) gda_default_provider_init
			};
			type = g_type_register_static (PARENT_TYPE,
						       "GdaDefaultProvider",
						       &info, 0);
		}
	}

	return type;
}

/* open_connection handler for the GdaDefaultProvider class */
static gboolean
gda_default_provider_open_connection (GdaServerProvider *provider,
				      GdaServerConnection *cnc,
				      GdaQuarkList *params,
				      const gchar *username,
				      const gchar *password)
{
	const gchar *t_uri = NULL;
	GdaXmlDatabase *xmldb;
	
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) provider;

	g_return_val_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	/* get all parameters received */
	t_uri = gda_quark_list_find (params, "URI");

	if (!t_uri) {
		gda_server_connection_add_error_string (
			cnc,
			_("A full path must be specified on the "
			  "connection string to open a database."));
		return FALSE;
	}

	/* open the given filename */
	xmldb = gda_xml_database_new_from_uri ((const gchar *) t_uri);
	if (!xmldb) {
		xmldb = gda_xml_database_new ();
		gda_xml_database_set_uri (xmldb, t_uri);
	}

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_DEFAULT_HANDLE, xmldb);
	
	return TRUE;
}

/* close_connection handler for the GdaDefaultProvider class */
static gboolean
gda_default_provider_close_connection (GdaServerProvider *provider,
				       GdaServerConnection *cnc)
{
	GdaXmlDatabase *xmldb;
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) provider;

	g_return_val_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_DEFAULT_HANDLE);
	if (!xmldb)
		return FALSE;

	g_object_unref (G_OBJECT (xmldb));
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_DEFAULT_HANDLE, NULL);

	return TRUE;
}

static GList *
process_sql_commands (GList *reclist, GdaServerConnection *cnc, const gchar *sql)
{
	gchar **arr;

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			n++;
		}

		g_strfreev (arr);
	}

	return reclist;
}


/* execute_command handler for the GdaDefaultProvider class */
static GList *
gda_default_provider_execute_command (GdaServerProvider *provider,
				      GdaServerConnection *cnc,
				      GdaCommand *cmd,
				      GdaParameterList *params)
{
	GList *reclist = NULL;
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) provider;

	g_return_val_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv), NULL);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	switch (gda_command_get_command_type (cmd)) {
		case GDA_COMMAND_TYPE_SQL:
			reclist = process_sql_commands (reclist, cnc,
							gda_command_get_text (cmd));
			break;
		case GDA_COMMAND_TYPE_XML:
			/* FIXME: Implement */
			return NULL;
		case GDA_COMMAND_TYPE_PROCEDURE:
			/* FIXME: Implement */
			return NULL;
		case GDA_COMMAND_TYPE_TABLE:
			break;
		case GDA_COMMAND_TYPE_INVALID:
			return NULL;
	}

	return reclist;
}

/* begin_transaction handler for the GdaDefaultProvider class */
static gboolean
gda_default_provider_begin_transaction (GdaServerProvider *provider,
					GdaServerConnection *cnc,
					const gchar *trans_id)
{
	GdaXmlDatabase *xmldb;
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) provider;

	g_return_val_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv), FALSE);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_DEFAULT_HANDLE);
	if (!xmldb)
		return FALSE;

	return gda_xml_database_save (xmldb, NULL);
}

/* commit_transaction handler for the GdaDefaultProvider class */
static gboolean
gda_default_provider_commit_transaction (GdaServerProvider *provider,
					 GdaServerConnection *cnc,
					 const gchar *trans_id)
{
	GdaXmlDatabase *xmldb;
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) provider;

	g_return_val_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv), FALSE);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_DEFAULT_HANDLE);
	if (!xmldb)
		return FALSE;

	return gda_xml_database_save (xmldb, NULL);
}

/* rollback_transaction handler for the GdaDefaultProvider class */
static gboolean
gda_default_provider_rollback_transaction (GdaServerProvider *provider,
					   GdaServerConnection *cnc,
					   const gchar *trans_id)
{
	GdaXmlDatabase *xmldb;
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) provider;

	g_return_val_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv), FALSE);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_DEFAULT_HANDLE);
	if (!xmldb)
		return FALSE;

	gda_xml_database_reload (xmldb);
	return TRUE;
}

/* supports handler for the GdaDefaultProvider class */
static gboolean
gda_default_provider_supports (GdaServerProvider *provider,
			       GdaServerConnection *cnc,
			       GNOME_Database_Feature feature)
{
	if (feature == GNOME_Database_FEATURE_TRANSACTIONS
	    && feature == GNOME_Database_FEATURE_XML_QUERIES)
		return TRUE;

	return FALSE;
}

static GdaServerRecordset *
get_table_fields (GdaServerConnection *cnc, GdaXmlDatabase *xmldb, GdaParameterList *params)
{
	GdaServerRecordsetModel *recset;
	GdaParameter *par;
	GdaTable *table;
	gint i;
	gint cols;
	const gchar *table_name;
	struct {
		const gchar *name;
		GdaType type;
	} fields_desc[8] = {
		{ N_("Field name")	, GDA_TYPE_STRING  },
		{ N_("Data type")	, GDA_TYPE_STRING  },
		{ N_("Size")		, GDA_TYPE_INTEGER },
		{ N_("Scale")		, GDA_TYPE_INTEGER },
		{ N_("Not null?")	, GDA_TYPE_BOOLEAN },
		{ N_("Primary key?")	, GDA_TYPE_BOOLEAN },
		{ N_("Unique index?")	, GDA_TYPE_BOOLEAN },
		{ N_("References")	, GDA_TYPE_STRING  }
	};

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	/* get parameters sent by client */
	par = gda_parameter_list_find (params, "name");
	if (!par) {
		gda_server_connection_add_error_string (cnc, _("Invalid argument"));
		return NULL;
	}

	table_name = gda_value_get_string (gda_parameter_get_value (par));
	if (!table_name) {
		gda_server_connection_add_error_string (cnc, _("Invalid argument"));
		return NULL;
	}

	/* find the table */
	table = gda_xml_database_find_table (xmldb, table_name);
	if (!table) {
		gda_server_connection_add_error_string (cnc, _("Table %s not found"), table_name);
		return NULL;
	}

	/* fill in the recordset to be returned */
	recset = GDA_SERVER_RECORDSET_MODEL (gda_server_recordset_model_new (cnc, 8));
	for (i = 0; i < sizeof (fields_desc) / sizeof (fields_desc[0]); i++) {
		gint defined_size =  (fields_desc[i].type == GDA_TYPE_STRING) ? 64 : 
			(fields_desc[i].type == GDA_TYPE_INTEGER) ? sizeof (gint) : 1;

		gda_server_recordset_model_set_field_defined_size (recset, i, defined_size);
		gda_server_recordset_model_set_field_name (recset, i, fields_desc[i].name);
		gda_server_recordset_model_set_field_scale (recset, i, 0);
		gda_server_recordset_model_set_field_gdatype (recset, i, fields_desc[i].type);
	}

	cols = gda_data_model_get_n_columns (GDA_DATA_MODEL (table));
	for (i = 0; i < cols; i++) {
		GdaFieldAttributes *fa;
		GList *value_list = NULL;

		fa = gda_data_model_describe_column (GDA_DATA_MODEL (table), i);
		if (!fa) {
			bonobo_object_unref (BONOBO_OBJECT (recset));
			gda_server_connection_add_error_string (
				cnc, _("Could not get description for column"));

			return NULL;
		}

		value_list = g_list_append (value_list, gda_value_new_string (gda_field_attributes_get_name (fa)));
		value_list = g_list_append (value_list, gda_value_new_string (gda_type_to_string (gda_field_attributes_get_gdatype (fa))));
		value_list = g_list_append (value_list, gda_value_new_integer (gda_field_attributes_get_defined_size (fa)));
		value_list = g_list_append (value_list, gda_value_new_integer (gda_field_attributes_get_scale (fa)));
		value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));
		value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));
		value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));
		value_list = g_list_append (value_list, gda_value_new_string (""));

		gda_field_attributes_free (fa);
	}

	return GDA_SERVER_RECORDSET (recset);
}

static void
add_string_row (GdaServerRecordsetModel *recset, const gchar *str)
{
	GdaValue *value;
	GList list;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset));
	g_return_if_fail (str != NULL);

	value = gda_value_new_string (str);
	list.data = value;
	list.next = NULL;
	list.prev = NULL;

	gda_server_recordset_model_append_row (recset, &list);

	gda_value_free (value);
}

static GdaServerRecordset *
get_databases (GdaServerConnection *cnc, GdaXmlDatabase *xmldb)
{
	GdaServerRecordsetModel *recset;

	recset = GDA_SERVER_RECORDSET_MODEL (gda_server_recordset_model_new (cnc, 1));
	gda_server_recordset_model_set_field_defined_size (recset, 0, 256);
	gda_server_recordset_model_set_field_name (recset, 0, _("Name"));
	gda_server_recordset_model_set_field_scale (recset, 0, 0);
	gda_server_recordset_model_set_field_gdatype (recset, 0, GDA_TYPE_STRING);

	add_string_row (recset, gda_xml_database_get_name (xmldb));

	return recset;
}

static GdaServerRecordset *
get_tables (GdaServerConnection *cnc, GdaXmlDatabase *xmldb)
{
	GdaServerRecordsetModel *recset;
	GList *tables;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);

	recset = GDA_SERVER_RECORDSET_MODEL (gda_server_recordset_model_new (cnc, 1));
	gda_server_recordset_model_set_field_defined_size (recset, 0, 256);
	gda_server_recordset_model_set_field_name (recset, 0, _("Name"));
	gda_server_recordset_model_set_field_scale (recset, 0, 0);
	gda_server_recordset_model_set_field_gdatype (recset, 0, GDA_TYPE_STRING);

	tables = gda_xml_database_get_tables (xmldb);
	if (tables != NULL) {
		GList *l;

		for (l = tables; l != NULL; l = l->next) {
			GList *value_list;
			GdaValue *value;

			value = gda_value_new_string ((const gchar *) l->data);
			value_list = g_list_append (NULL, value);
			gda_server_recordset_model_append_row (recset,
							       (const GList *) value_list);

			gda_value_free (value);
			g_list_free (value_list);
		}

		gda_xml_database_free_table_list (tables);
	}

	return GDA_SERVER_RECORDSET (recset);
}

static GdaServerRecordset *
get_types (GdaServerConnection *cnc)
{
	GdaServerRecordsetModel *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaServerRecordsetModel *) gda_server_recordset_model_new (cnc, 1);
	gda_server_recordset_model_set_field_defined_size (recset, 0, 32);
	gda_server_recordset_model_set_field_name (recset, 0, _("Type"));
	gda_server_recordset_model_set_field_scale (recset, 0, 0);
	gda_server_recordset_model_set_field_gdatype (recset, 0, GDA_TYPE_STRING);

	/* fill the recordset */
	add_string_row (recset, "bigint");
	add_string_row (recset, "binary");
	add_string_row (recset, "boolean");
	add_string_row (recset, "date");
	add_string_row (recset, "double");
	add_string_row (recset, "integer");
	add_string_row (recset, "list");
	add_string_row (recset, "numeric");
	add_string_row (recset, "point");
	add_string_row (recset, "single");
	add_string_row (recset, "smallint");
	add_string_row (recset, "string");
	add_string_row (recset, "time");
	add_string_row (recset, "timestamp");
	add_string_row (recset, "tinyint");

	return GDA_SERVER_RECORDSET (recset);
}

/* get_schema handler for the GdaDefaultProvider class */
static GdaServerRecordset *
gda_default_provider_get_schema (GdaServerProvider *provider,
				 GdaServerConnection *cnc,
				 GNOME_Database_Connection_Schema schema,
				 GdaParameterList *params)
{
	GdaXmlDatabase *xmldb;
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) provider;

	g_return_val_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv), NULL);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_DEFAULT_HANDLE);
	if (!xmldb)
		return NULL;

	switch (schema) {
	case GNOME_Database_Connection_SCHEMA_DATABASES :
		return get_databases (cnc, xmldb);
	case GNOME_Database_Connection_SCHEMA_FIELDS :
		return get_table_fields (cnc, xmldb, params);
	case GNOME_Database_Connection_SCHEMA_TABLES :
		return get_tables (cnc, xmldb);
	case GNOME_Database_Connection_SCHEMA_TYPES :
		return get_types (cnc);
	default :
	}

	return NULL;
}
