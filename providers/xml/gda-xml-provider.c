/* GDA Xml provider
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
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-row.h>
#include <libgda/gda-select.h>
#include <libgda/gda-util.h>
#include "gda-xml.h"
#include "gda-xml-recordset.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_XML_HANDLE "GDA_Xml_XmlHandle"
#define OBJECT_DATA_XML_RECORDSET "GDA_Xml_XmlRecordset"

static void gda_xml_provider_class_init (GdaXmlProviderClass *klass);
static void gda_xml_provider_init       (GdaXmlProvider *provider,
					 GdaXmlProviderClass *klass);
static void gda_xml_provider_finalize   (GObject *object);

static const gchar *gda_xml_provider_get_version (GdaServerProvider *provider);
static gboolean gda_xml_provider_open_connection (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  GdaQuarkList *params,
						  const gchar *username,
						  const gchar *password);
static gboolean gda_xml_provider_close_connection (GdaServerProvider *provider,
						   GdaConnection *cnc);
static const gchar *gda_xml_provider_get_server_version (GdaServerProvider *provider,
							 GdaConnection *cnc);
static const gchar *gda_xml_provider_get_database (GdaServerProvider *provider,
						   GdaConnection *cnc);
static gboolean gda_xml_provider_create_database (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  const gchar *name);
static GList *gda_xml_provider_execute_command (GdaServerProvider *provider,
						GdaConnection *cnc,
						GdaCommand *cmd,
						GdaParameterList *params);
static gboolean gda_xml_provider_begin_transaction (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaTransaction *xaction);
static gboolean gda_xml_provider_commit_transaction (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaTransaction *xaction);
static gboolean gda_xml_provider_rollback_transaction (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaTransaction *xaction);
static gboolean gda_xml_provider_supports (GdaServerProvider *provider,
					   GdaConnection *cnc,
					   GdaConnectionFeature feature);
static GdaDataModel *gda_xml_provider_get_schema (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  GdaConnectionSchema schema,
						  GdaParameterList *params);

static GObjectClass *parent_class = NULL;

/*
 * GdaXmlProvider class implementation
 */

static void
gda_xml_provider_class_init (GdaXmlProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_xml_provider_finalize;
	provider_class->get_version = gda_xml_provider_get_version;
	provider_class->open_connection = gda_xml_provider_open_connection;
	provider_class->close_connection = gda_xml_provider_close_connection;
	provider_class->get_server_version = gda_xml_provider_get_server_version;
	provider_class->get_database = gda_xml_provider_get_database;
	provider_class->create_database = gda_xml_provider_create_database;
	provider_class->execute_command = gda_xml_provider_execute_command;
	provider_class->begin_transaction = gda_xml_provider_begin_transaction;
	provider_class->commit_transaction = gda_xml_provider_commit_transaction;
	provider_class->rollback_transaction = gda_xml_provider_rollback_transaction;
	provider_class->supports = gda_xml_provider_supports;
	provider_class->get_schema = gda_xml_provider_get_schema;
}

static void
gda_xml_provider_init (GdaXmlProvider *myprv,
			   GdaXmlProviderClass *klass)
{
}

static void
gda_xml_provider_finalize (GObject *object)
{
	GdaXmlProvider *dfprv = (GdaXmlProvider *) object;

	g_return_if_fail (GDA_IS_XML_PROVIDER (dfprv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_xml_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaXmlProviderClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_xml_provider_class_init,
				NULL, NULL,
				sizeof (GdaXmlProvider),
				0,
				(GInstanceInitFunc) gda_xml_provider_init
			};
			type = g_type_register_static (PARENT_TYPE,
						       "GdaXmlProvider",
						       &info, 0);
		}
	}

	return type;
}

GdaServerProvider *
gda_xml_provider_new (void)
{
	GdaXmlProvider *provider;

	provider = g_object_new (gda_xml_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

/* get_version handler for the GdaXmlProvider class */
static const gchar *
gda_xml_provider_get_version (GdaServerProvider *provider)
{
	GdaXmlProvider *dfprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (dfprv), NULL);
	return PACKAGE_VERSION;
}

/* open_connection handler for the GdaXmlProvider class */
static gboolean
gda_xml_provider_open_connection (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      GdaQuarkList *params,
				      const gchar *username,
				      const gchar *password)
{
	const gchar *t_uri = NULL;
	GdaXmlDatabase *xmldb;
	
	GdaXmlProvider *dfprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (dfprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* get all parameters received */
	t_uri = gda_quark_list_find (params, "URI");

	if (!t_uri) {
		gda_connection_add_error_string (cnc,
						 _("A full path must be specified on the "
						   "connection string to open a database "
						   "on the XML provider."));
		return FALSE;
	}

	/* open the given filename */
	xmldb = gda_xml_database_new_from_uri ((const gchar *) t_uri);
	if (!xmldb) {
		xmldb = gda_xml_database_new ();
		gda_xml_database_set_uri (xmldb, t_uri);
	}

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE, xmldb);
	
	return TRUE;
}

/* close_connection handler for the GdaXmlProvider class */
static gboolean
gda_xml_provider_close_connection (GdaServerProvider *provider,
				   GdaConnection *cnc)
{
	GdaXmlDatabase *xmldb;
	GdaXmlProvider *xmlprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (xmlprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE);
	if (!xmldb) {
		gda_connection_add_error_string (cnc, _("Invalid XML handle"));
		return FALSE;
	}

	g_object_unref (G_OBJECT (xmldb));
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE, NULL);

	return TRUE;
}

/* get_server_version handler for the GdaXmlProvider class */
static const gchar *
gda_xml_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaXmlDatabase *xmldb;
	GdaXmlProvider *xmlprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (xmlprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE);
	if (!xmldb) {
		gda_connection_add_error_string (cnc, _("Invalid XML handle"));
		return NULL;
	}

	return gda_xml_database_get_version (xmldb);
}

/* get_database handler for the GdaXmlProvider class */
static const gchar *
gda_xml_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaXmlDatabase *xmldb;
	GdaXmlProvider *dfprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (dfprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE);
	if (!xmldb) {
		gda_connection_add_error_string (cnc, _("Invalid internal handle"));
		return NULL;
	}

	return gda_xml_database_get_name (xmldb);
}

static GList *
process_sql_commands (GList *reclist, GdaConnection *cnc, const gchar *sql)
{
	gchar **arr;
	GdaXmlDatabase *xmldb;

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE);
	if (!xmldb) {
		gda_connection_add_error_string (cnc, _("Invalid internal handle"));
		return reclist;
	}

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n = 0;
		GList *tables, *l;

		tables = gda_xml_database_get_tables (xmldb);
		while (arr[n]) {
			GdaSelect *select;

			/* create the GdaSelect and add the sources */
			select = (GdaSelect *) gda_select_new ();
			gda_select_set_sql (select, arr[n]);

			for (l = tables; l != NULL; l = l->next) {
				GdaTable *table;

				table = gda_xml_database_find_table (xmldb, l->data);
				if (table) {
					gda_select_add_source (select,
							       l->data,
							       (const GdaDataModel *) table);
				}
			}

			/* run the SQL */
			if (gda_select_run (select))
				reclist = g_list_append (reclist, select);

			n++;
		}

		g_strfreev (arr);
		gda_xml_database_free_table_list (tables);
	}

	return reclist;
}

static GList *
process_table_commands (GList *reclist, GdaConnection *cnc, const gchar *str)
{
	gchar **arr;
	GdaXmlDatabase *xmldb;

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE);
	if (!xmldb) {
		gda_connection_add_error_string (cnc, _("Invalid internal handle"));
		return reclist;
	}

	/* parse string, which can contain several table names */
	arr = g_strsplit (str, ";", 0);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			GdaTable *table;

			table = gda_xml_database_find_table (xmldb, arr[n]);
			if (GDA_IS_TABLE (table))
				reclist = g_list_append (reclist, table);
			else {
				/* FIXME: what to do on errors? */
			}

			n++;
		}

		g_strfreev (arr);
	}

	return reclist;
}

/* create_database handler for the GdaXmlProvider class */
static gboolean
gda_xml_provider_create_database (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      const gchar *name)
{
	GdaXmlProvider *dfprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (dfprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	gda_connection_add_error_string (cnc, _("Not Implemented"));
	return FALSE;
}

/* execute_command handler for the GdaXmlProvider class */
static GList *
gda_xml_provider_execute_command (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      GdaCommand *cmd,
				      GdaParameterList *params)
{
	GList *reclist = NULL;
	GdaXmlProvider *dfprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (dfprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
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
		reclist = process_table_commands (reclist, cnc,
						  gda_command_get_text (cmd));
		break;
	default:
		return NULL;
	}

	return reclist;
}

/* begin_transaction handler for the GdaXmlProvider class */
static gboolean
gda_xml_provider_begin_transaction (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    GdaTransaction *xaction)
{
	GdaXmlDatabase *xmldb;
	GdaXmlProvider *dfprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (dfprv), FALSE);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE);
	if (!xmldb)
		return FALSE;

	return gda_xml_database_save (xmldb, NULL);
}

/* commit_transaction handler for the GdaXmlProvider class */
static gboolean
gda_xml_provider_commit_transaction (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 GdaTransaction *xaction)
{
	GdaXmlDatabase *xmldb;
	GdaXmlProvider *dfprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (dfprv), FALSE);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE);
	if (!xmldb)
		return FALSE;

	return gda_xml_database_save (xmldb, NULL);
}

/* rollback_transaction handler for the GdaXmlProvider class */
static gboolean
gda_xml_provider_rollback_transaction (GdaServerProvider *provider,
					   GdaConnection *cnc,
					   GdaTransaction *xaction)
{
	GdaXmlDatabase *xmldb;
	GdaXmlProvider *dfprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (dfprv), FALSE);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE);
	if (!xmldb)
		return FALSE;

	gda_xml_database_reload (xmldb);
	return TRUE;
}

/* supports handler for the GdaXmlProvider class */
static gboolean
gda_xml_provider_supports (GdaServerProvider *provider,
			       GdaConnection *cnc,
			       GdaConnectionFeature feature)
{
	if (feature == GDA_CONNECTION_FEATURE_TRANSACTIONS
	    || feature == GDA_CONNECTION_FEATURE_XML_QUERIES)
		return TRUE;

	return FALSE;
}

static GdaDataModel *
get_table_fields (GdaConnection *cnc, GdaXmlDatabase *xmldb, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaParameter *par;
	GdaTable *table;
	gint i;
	gint cols;
	const gchar *table_name;
	struct {
		const gchar *name;
		GdaValueType type;
	} fields_desc[8] = {
		{ N_("Field name")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Data type")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Size")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Scale")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Not null?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Primary key?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Unique index?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("References")	, GDA_VALUE_TYPE_STRING  }
	};

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	/* get parameters sent by client */
	par = gda_parameter_list_find (params, "name");
	if (!par) {
		gda_connection_add_error_string (cnc, _("Invalid argument"));
		return NULL;
	}

	table_name = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
	if (!table_name) {
		gda_connection_add_error_string (cnc, _("Invalid argument"));
		return NULL;
	}

	/* find the table */
	table = gda_xml_database_find_table (xmldb, table_name);
	if (!table) {
		gda_connection_add_error_string (cnc, _("Table %s not found"), table_name);
		return NULL;
	}

	/* fill in the recordset to be returned */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (8));
	for (i = 0; i < sizeof (fields_desc) / sizeof (fields_desc[0]); i++) {
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i, _(fields_desc[i].name));
	}

	cols = gda_data_model_get_n_columns (GDA_DATA_MODEL (table));
	for (i = 0; i < cols; i++) {
		GdaDataModelColumnAttributes *fa;
		GList *value_list = NULL;

		fa = gda_data_model_describe_column (GDA_DATA_MODEL (table), i);
		if (!fa) {
			g_object_unref (G_OBJECT (recset));
			gda_connection_add_error_string (
				cnc, _("Could not get description for column"));

			return NULL;
		}

		value_list = g_list_append (value_list, gda_value_new_string (gda_data_model_column_attributes_get_name (fa)));
		value_list = g_list_append (value_list, gda_value_new_string (gda_type_to_string (gda_data_model_column_attributes_get_gdatype (fa))));
		value_list = g_list_append (value_list, gda_value_new_integer (gda_data_model_column_attributes_get_defined_size (fa)));
		value_list = g_list_append (value_list, gda_value_new_integer (gda_data_model_column_attributes_get_scale (fa)));
		value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));
		value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));
		value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));
		value_list = g_list_append (value_list, gda_value_new_string (""));

		gda_data_model_column_attributes_free (fa);
	}

	return GDA_DATA_MODEL (recset);
}

static void
add_string_row (GdaDataModelArray *recset, const gchar *str)
{
	GdaValue *value;
	GList list;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (recset));
	g_return_if_fail (str != NULL);

	value = gda_value_new_string (str);
	list.data = value;
	list.next = NULL;
	list.prev = NULL;

	gda_data_model_append_values (GDA_DATA_MODEL (recset), &list);

	gda_value_free (value);
}

static GdaDataModel *
get_databases (GdaConnection *cnc, GdaXmlDatabase *xmldb)
{
	GdaDataModelArray *recset;
	const gchar *dbname;

	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (1));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Name"));

	dbname = gda_xml_database_get_name (xmldb);
	if (dbname != NULL)
		add_string_row (recset, dbname);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_tables (GdaConnection *cnc, GdaXmlDatabase *xmldb)
{
	GdaDataModelArray *recset;
	GList *tables;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);

	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (1));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Name"));

	tables = gda_xml_database_get_tables (xmldb);
	if (tables != NULL) {
		GList *l;

		for (l = tables; l != NULL; l = l->next) {
			GList *value_list;
			GdaValue *value;

			value = gda_value_new_string ((const gchar *) l->data);
			value_list = g_list_append (NULL, value);
			gda_data_model_append_values (GDA_DATA_MODEL (recset),
						   (const GList *) value_list);

			gda_value_free (value);
			g_list_free (value_list);
		}

		gda_xml_database_free_table_list (tables);
	}

	return GDA_DATA_MODEL (recset);
}

static void
add_type_row (GdaDataModelArray *recset, const gchar *type, const gchar *owner,
	      const gchar *comments, GdaValueType gda_type)
{
	GList *value_list = NULL;

	value_list = g_list_append (value_list, gda_value_new_string (type));
	value_list = g_list_append (value_list, gda_value_new_string (owner));
	value_list = g_list_append (value_list, gda_value_new_string (comments));
	value_list = g_list_append (value_list, gda_value_new_type (gda_type));

	gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list);

	g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
	g_list_free (value_list);
}

static GdaDataModel *
get_types (GdaConnection *cnc)
{
	GdaDataModelArray *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (4));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Type"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 1, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 2, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 3, _("GDA type"));

	/* fill the recordset */
	add_type_row (recset, "bigint", "", _("Big integer"), GDA_VALUE_TYPE_BIGINT);
	add_type_row (recset, "biguint", "", _("Big unsigned integer"), GDA_VALUE_TYPE_BIGUINT);
	add_type_row (recset, "binary", "", _("Binary"), GDA_VALUE_TYPE_BINARY);
	add_type_row (recset, "boolean", "", _("Boolean"), GDA_VALUE_TYPE_BOOLEAN);
	add_type_row (recset, "date", "", _("Date"), GDA_VALUE_TYPE_DATE);
	add_type_row (recset, "double", "", _("Double precision"), GDA_VALUE_TYPE_DOUBLE);
	add_type_row (recset, "integer", "", _("Integer"), GDA_VALUE_TYPE_INTEGER);
	add_type_row (recset, "list", "", _("List"), GDA_VALUE_TYPE_LIST);
	add_type_row (recset, "money", "", _("Money"), GDA_VALUE_TYPE_MONEY);
	add_type_row (recset, "numeric", "", _("Numeric"), GDA_VALUE_TYPE_NUMERIC);
	add_type_row (recset, "point", "", _("Point"), GDA_VALUE_TYPE_GEOMETRIC_POINT);
	add_type_row (recset, "single", "", _("Single precision"), GDA_VALUE_TYPE_SINGLE);
	add_type_row (recset, "smallint", "", _("Small integer"), GDA_VALUE_TYPE_SMALLINT);
	add_type_row (recset, "smalluint", "", _("Small unsigned integer"), GDA_VALUE_TYPE_SMALLUINT);
	add_type_row (recset, "string", "", _("String"), GDA_VALUE_TYPE_STRING);
	add_type_row (recset, "time", "", _("Time"), GDA_VALUE_TYPE_TIME);
	add_type_row (recset, "timestamp", "", _("Timestamp"), GDA_VALUE_TYPE_TIMESTAMP);
	add_type_row (recset, "tinyint", "", _("Tiny integer"), GDA_VALUE_TYPE_TINYINT);
	add_type_row (recset, "tinyuint", "", _("Tiny unsigned integer"), GDA_VALUE_TYPE_TINYUINT);
	add_type_row (recset, "type", "", _("Type"), GDA_VALUE_TYPE_TYPE);
	add_type_row (recset, "uinteger", "", _("Unsigned integer"), GDA_VALUE_TYPE_UINTEGER);

	return GDA_DATA_MODEL (recset);
}

/* get_schema handler for the GdaXmlProvider class */
static GdaDataModel *
gda_xml_provider_get_schema (GdaServerProvider *provider,
				 GdaConnection *cnc,
				 GdaConnectionSchema schema,
				 GdaParameterList *params)
{
	GdaXmlDatabase *xmldb;
	GdaXmlProvider *dfprv = (GdaXmlProvider *) provider;

	g_return_val_if_fail (GDA_IS_XML_PROVIDER (dfprv), NULL);

	xmldb = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_XML_HANDLE);
	if (!xmldb)
		return NULL;

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_DATABASES :
		return get_databases (cnc, xmldb);
	case GDA_CONNECTION_SCHEMA_FIELDS :
		return get_table_fields (cnc, xmldb, params);
	case GDA_CONNECTION_SCHEMA_TABLES :
		return get_tables (cnc, xmldb);
	case GDA_CONNECTION_SCHEMA_TYPES :
		return get_types (cnc);
	default : break;
	}

	return NULL;
}
