/* GDA MDB provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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
#include <glib/gbacktrace.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-list.h>
#include <libgda/gda-table.h>
#include <libgda/gda-intl.h>
#include "gda-mdb.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_MDB_HANDLE "GDA_Mdb_MdbHandle"

static void gda_mdb_provider_class_init (GdaMdbProviderClass *klass);
static void gda_mdb_provider_init       (GdaMdbProvider *provider,
					   GdaMdbProviderClass *klass);
static void gda_mdb_provider_finalize   (GObject *object);

static const gchar *gda_mdb_provider_get_version (GdaServerProvider *provider);
static gboolean gda_mdb_provider_open_connection (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaQuarkList *params,
						    const gchar *username,
						    const gchar *password);
static gboolean gda_mdb_provider_close_connection (GdaServerProvider *provider,
						     GdaConnection *cnc);
static const gchar *gda_mdb_provider_get_server_version (GdaServerProvider *provider,
							 GdaConnection *cnc);
static const gchar *gda_mdb_provider_get_database (GdaServerProvider *provider,
						   GdaConnection *cnc);
static gboolean gda_mdb_provider_change_database (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  const gchar *name);
static gboolean gda_mdb_provider_create_database (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  const gchar *name);
static gboolean gda_mdb_provider_drop_database (GdaServerProvider *provider,
						GdaConnection *cnc,
						const gchar *name);
static GList *gda_mdb_provider_execute_command (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  GdaCommand *cmd,
						  GdaParameterList *params);
static gboolean gda_mdb_provider_begin_transaction (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaTransaction *xaction);
static gboolean gda_mdb_provider_commit_transaction (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaTransaction *xaction);
static gboolean gda_mdb_provider_rollback_transaction (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaTransaction *xaction);
static gboolean gda_mdb_provider_supports (GdaServerProvider *provider,
					     GdaConnection *cnc,
					     GdaConnectionFeature feature);
static GdaDataModel *gda_mdb_provider_get_schema (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaConnectionSchema schema,
						    GdaParameterList *params);

static GObjectClass *parent_class = NULL;
static gint loaded_providers = 0;
extern MdbSQL *mdb_SQL;
char *g_input_ptr;

/*
 * Private functions
 */

/*
 * GdaMdbProvider class implementation
 */

static void
gda_mdb_provider_class_init (GdaMdbProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_mdb_provider_finalize;
	provider_class->get_version = gda_mdb_provider_get_version;
	provider_class->open_connection = gda_mdb_provider_open_connection;
	provider_class->close_connection = gda_mdb_provider_close_connection;
	provider_class->get_server_version = gda_mdb_provider_get_server_version;
	provider_class->get_database = gda_mdb_provider_get_database;
	provider_class->change_database = gda_mdb_provider_change_database;
	provider_class->create_database = gda_mdb_provider_create_database;
	provider_class->drop_database = gda_mdb_provider_drop_database;
	provider_class->execute_command = gda_mdb_provider_execute_command;
	provider_class->begin_transaction = gda_mdb_provider_begin_transaction;
	provider_class->commit_transaction = gda_mdb_provider_commit_transaction;
	provider_class->rollback_transaction = gda_mdb_provider_rollback_transaction;
	provider_class->supports = gda_mdb_provider_supports;
	provider_class->get_schema = gda_mdb_provider_get_schema;
}

static void
gda_mdb_provider_init (GdaMdbProvider *myprv, GdaMdbProviderClass *klass)
{
}

static void
gda_mdb_provider_finalize (GObject *object)
{
	GdaMdbProvider *myprv = (GdaMdbProvider *) object;

	g_return_if_fail (GDA_IS_MDB_PROVIDER (myprv));

	/* chain to parent class */
	parent_class->finalize (object);

	/* call MDB exit function if there are no more providers */
	loaded_providers--;
	if (loaded_providers == 0)
		mdb_exit ();
}

GType
gda_mdb_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaMdbProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mdb_provider_class_init,
			NULL, NULL,
			sizeof (GdaMdbProvider),
			0,
			(GInstanceInitFunc) gda_mdb_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaMdbProvider", &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_mdb_provider_new (void)
{
	static gboolean mdb_initialized = FALSE;
	GdaMdbProvider *provider;

	if (!mdb_initialized)
		mdb_init ();

	loaded_providers++;

	provider = g_object_new (gda_mdb_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

/* get_version handler for the GdaMdbProvider class */
static const gchar *
gda_mdb_provider_get_version (GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/* open_connection handler for the GdaMdbProvider class */
static gboolean
gda_mdb_provider_open_connection (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  GdaQuarkList *params,
				  const gchar *username,
				  const gchar *password)
{
	const gchar *filename;
	GdaMdbConnection *mdb_cnc;
	GdaMdbProvider *mdb_prv = (GdaMdbProvider *) provider;

	g_return_val_if_fail (GDA_IS_MDB_PROVIDER (mdb_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* look for parameters */
	filename = gda_quark_list_find (params, "FILENAME");
	if (!filename) {
		gda_connection_add_error_string (
			cnc,
			_("A database FILENAME must be specified in the connection_string"));
		return FALSE;
	}

	mdb_cnc = g_new0 (GdaMdbConnection, 1);
	mdb_cnc->cnc = cnc;
	mdb_cnc->server_version = NULL;
	mdb_cnc->mdb = mdb_open (filename);
	if (!mdb_cnc->mdb) {
		gda_connection_add_error_string (cnc, _("Could not open file %s"), filename);
		g_free (mdb_cnc);
		return FALSE;
	}

	mdb_read_catalog (mdb_cnc->mdb, MDB_ANY);

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_MDB_HANDLE, mdb_cnc);

	return TRUE;
}

/* close_connection handler for the GdaMdbProvider class */
static gboolean
gda_mdb_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaMdbConnection *mdb_cnc;
	GdaMdbProvider *mdb_prv = (GdaMdbProvider *) provider;

	g_return_val_if_fail (GDA_IS_MDB_PROVIDER (mdb_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	mdb_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MDB_HANDLE);
	if (!mdb_cnc) {
		gda_connection_add_error_string (cnc, _("Invalid MDB handle"));
		return FALSE;
	}

	if (mdb_cnc->server_version != NULL) {
		g_free (mdb_cnc->server_version);
		mdb_cnc->server_version = NULL;
	}

	/* mdb_free_handle (mdb_cnc->mdb); */

	g_free (mdb_cnc);
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_MDB_HANDLE, NULL);

	return TRUE;
}

/* get_server_version handler for the GdaMdbProvider class */
static const gchar *
gda_mdb_provider_get_server_version (GdaServerProvider *provider,
				     GdaConnection *cnc)
{
	GdaMdbConnection *mdb_cnc;
	GdaMdbProvider *mdb_prv = (GdaMdbProvider *) provider;

	g_return_val_if_fail (GDA_IS_MDB_PROVIDER (mdb_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	mdb_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MDB_HANDLE);
	if (!mdb_cnc) {
		gda_connection_add_error_string (cnc, _("Invalid MDB handle"));
		return NULL;
	}

	if (!mdb_cnc->server_version)
		mdb_cnc->server_version = g_strdup_printf ("Microsoft Jet %d", mdb_cnc->mdb->f->jet_version);

	return (const gchar *) mdb_cnc->server_version;
}

/* get_database handler for the GdaMdbProvider class */
static const gchar *
gda_mdb_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaMdbConnection *mdb_cnc;
	GdaMdbProvider *mdb_prv = (GdaMdbProvider *) provider;

	g_return_val_if_fail (GDA_IS_MDB_PROVIDER (mdb_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	mdb_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MDB_HANDLE);
	if (!mdb_cnc) {
		gda_connection_add_error_string (cnc, _("Invalid MDB handle"));
		return NULL;
	}

	return (const gchar *) mdb_cnc->mdb->f->filename;
}

/* change_database handler for the GdaMdbProvider class */
static gboolean
gda_mdb_provider_change_database (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  const gchar *name)
{
	return FALSE;
}

/* create_database handler for the GdaMdbProvider class */
static gboolean
gda_mdb_provider_create_database (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  const gchar *name)
{
	return FALSE;
}

/* drop_database handler for the GdaMdbProvider class */
static gboolean
gda_mdb_provider_drop_database (GdaServerProvider *provider,
				GdaConnection *cnc,
				const gchar *name)
{
	return FALSE;
}

/* execute_command handler for the GdaMdbProvider class */
static GList *
gda_mdb_provider_execute_command (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  GdaCommand *cmd,
				  GdaParameterList *params)
{
	GList *reclist = NULL;
	gchar **arr;
	GdaMdbConnection *mdb_cnc;
	GdaMdbProvider *mdb_prv = (GdaMdbProvider *) provider;

	g_return_val_if_fail (GDA_IS_MDB_PROVIDER (mdb_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	mdb_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MDB_HANDLE);
	if (!mdb_cnc) {
		gda_connection_add_error_string (cnc, _("Invalid MDB handle"));
		return NULL;
	}

	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL :
		arr = g_strsplit (cmd->text, ";", 0);
		if (arr) {
			gint i = 0;

			while (arr[i]) {
				GdaDataModel *model;

				model = gda_mdb_provider_execute_sql (mdb_prv, cnc, arr[i]);
				if (model)
					reclist = g_list_append (reclist, model);
				else {
					if (cmd->options & GDA_COMMAND_OPTION_STOP_ON_ERRORS)
						break;
				}

				i++;
			}

			g_strfreev (arr);
		}
		break;
	case GDA_COMMAND_TYPE_TABLE :
		arr = g_strsplit (cmd->text, ";", 0);
		if (arr) {
			gint i = 0;

			while (arr[i]) {
				GdaDataModel *model;
				gchar *rsql;

				rsql = g_strdup_printf ("select * from %s", arr[i]);
				model = gda_mdb_provider_execute_sql (mdb_prv, cnc, rsql);
				g_free (rsql);
				if (model)
					reclist = g_list_append (reclist, model);
				else {
					if (cmd->options & GDA_COMMAND_OPTION_STOP_ON_ERRORS)
						break;
				}
			}

			g_strfreev (arr);
		}
		break;
	default : ;
	}

	return reclist;
}

/* begin_transaction handler for the GdaMdbProvider class */
static gboolean
gda_mdb_provider_begin_transaction (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    GdaTransaction *xaction)
{
	gda_connection_add_error_string (cnc, _("Transactions not supported in MDB provider"));
	return FALSE;
}

/* commit_transaction handler for the GdaMdbProvider class */
static gboolean
gda_mdb_provider_commit_transaction (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaTransaction *xaction)
{
	gda_connection_add_error_string (cnc, _("Transactions not supported in MDB provider"));
	return FALSE;
}

/* rollback_transaction handler for the GdaMdbProvider class */
static gboolean
gda_mdb_provider_rollback_transaction (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaTransaction *xaction)
{
	gda_connection_add_error_string (cnc, _("Transactions not supported in MDB provider"));
	return FALSE;
}

/* supports handler for the GdaMdbProvider class */
static gboolean
gda_mdb_provider_supports (GdaServerProvider *provider,
			   GdaConnection *cnc,
			   GdaConnectionFeature feature)
{
	g_return_val_if_fail (GDA_IS_MDB_PROVIDER (provider), FALSE);

	switch (feature) {
	case GDA_CONNECTION_FEATURE_INDEXES :
	case GDA_CONNECTION_FEATURE_PROCEDURES :
	case GDA_CONNECTION_FEATURE_SQL :
		return TRUE;
	default : ;
	}

	return FALSE;
}

static GdaDataModel *
get_mdb_databases (GdaMdbConnection *mdb_cnc)
{
	GdaDataModel *model;
	GList l;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (mdb_cnc->mdb != NULL, NULL);

	model = gda_data_model_list_new ();
	gda_data_model_set_column_title (model, 0, _("Name"));

	l.prev = l.next = NULL;
	l.data = gda_value_new_string (mdb_cnc->mdb->f->filename);
	gda_data_model_append_row (model, &l);

	gda_value_free (l.data);

	return model;
}

static GdaDataModel *
get_mdb_fields (GdaMdbConnection *mdb_cnc, GdaParameterList *params)
{
	GdaParameter *par;
	const gchar *table_name;
	GdaDataModel *model;
	MdbCatalogEntry *entry;
	MdbTableDef *mdb_table;
	MdbColumn *mdb_col;
	gint i, j;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (mdb_cnc->mdb != NULL, NULL);

	par = gda_parameter_list_find (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	table_name = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (table_name != NULL, NULL);

	/* create the data model */
	model = gda_data_model_array_new (9);
	gda_data_model_set_column_title (model, 0, _("Field name"));
	gda_data_model_set_column_title (model, 1, _("Data type"));
	gda_data_model_set_column_title (model, 2, _("Size"));
	gda_data_model_set_column_title (model, 3, _("Scale"));
	gda_data_model_set_column_title (model, 4, _("Not null?"));
	gda_data_model_set_column_title (model, 5, _("Primary key?"));
	gda_data_model_set_column_title (model, 6, _("Unique index?"));
	gda_data_model_set_column_title (model, 7, _("References"));
	gda_data_model_set_column_title (model, 8, _("Default value"));

	/* fill in the data model with the information for the table */
	for (i = 0; i < mdb_cnc->mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb_cnc->mdb->catalog, i);
		if (entry->object_type == MDB_TABLE &&
		    !strcmp (entry->object_name, table_name)) {
			mdb_table = mdb_read_table (entry);
			mdb_read_columns (mdb_table);

			for (j = 0; j < mdb_table->num_cols; j++) {
				GList *value_list = NULL;

				mdb_col = g_ptr_array_index (mdb_table->columns, j);

				value_list = g_list_append (value_list, gda_value_new_string (mdb_col->name));
				value_list = g_list_append (
					value_list,
					gda_value_new_string (mdb_get_objtype_string (mdb_col->col_type)));
				value_list = g_list_append (value_list, gda_value_new_integer (mdb_col->col_size));
				value_list = g_list_append (value_list, gda_value_new_integer (mdb_col->col_scale));
				value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));
				value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));
				value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));
				value_list = g_list_append (value_list, gda_value_new_string (NULL));
				value_list = g_list_append (value_list, gda_value_new_string (NULL));

				gda_data_model_append_row (model, value_list);

				g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
				g_list_free (value_list);
			}
		}
	}

	return model;
}

static GdaDataModel *
get_mdb_procedures (GdaMdbConnection *mdb_cnc)
{
	gint i;
	GdaDataModelArray *model;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (mdb_cnc->mdb != NULL, NULL);

	model = (GdaDataModelArray *) gda_data_model_array_new (8);
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 0, _("Procedure"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 1, _("ID"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 2, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 3, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 4, _("Return type"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 5, _("# of args"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 6, _("Args types"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 7, _("Definition"));

	for (i = 0; i < mdb_cnc->mdb->num_catalog; i++) {
		GdaValue *value;
		MdbCatalogEntry *entry;

		entry = g_ptr_array_index (mdb_cnc->mdb->catalog, i);

 		/* if it's a table */
		if (entry->object_type == MDB_MODULE) {
			GList *value_list = NULL;

			value_list = g_list_append (value_list, gda_value_new_string (entry->object_name));
			value_list = g_list_append (value_list, gda_value_new_string (NULL));
			value_list = g_list_append (value_list, gda_value_new_string (NULL));
			value_list = g_list_append (value_list, gda_value_new_string (NULL));
			value_list = g_list_append (value_list, gda_value_new_string (NULL));
			value_list = g_list_append (value_list, gda_value_new_integer (0));
			value_list = g_list_append (value_list, gda_value_new_string (NULL));
			value_list = g_list_append (value_list, gda_value_new_string (NULL));

			gda_data_model_append_row (GDA_DATA_MODEL (model), value_list);

			g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
			g_list_free (value_list);
		}
	}

	return GDA_DATA_MODEL (model);
}

static GdaDataModel *
get_mdb_tables (GdaMdbConnection *mdb_cnc)
{
	gint i;
	GdaDataModelArray *model;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (mdb_cnc->mdb != NULL, NULL);

	model = (GdaDataModelArray *) gda_data_model_array_new (4);
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 0, _("Name"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 1, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 2, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (model), 3, "SQL");

	for (i = 0; i < mdb_cnc->mdb->num_catalog; i++) {
		GdaValue *value;
		MdbCatalogEntry *entry;

		entry = g_ptr_array_index (mdb_cnc->mdb->catalog, i);

 		/* if it's a table */
		if (entry->object_type == MDB_TABLE) {
			/* skip the MSys tables */
			if (strncmp (entry->object_name, "MSys", 4)) {
				GList *value_list = NULL;

				value_list = g_list_append (value_list,
							    gda_value_new_string (entry->object_name));
				value_list = g_list_append (value_list, gda_value_new_string (""));
				value_list = g_list_append (value_list, gda_value_new_string (""));
				value_list = g_list_append (value_list, gda_value_new_string (""));

				gda_data_model_append_row (model, value_list);

				g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
				g_list_free (value_list);
			}
		}
	}

	return GDA_DATA_MODEL (model);
}

static void
add_type (GdaDataModel *model, const gchar *typname, const gchar *owner,
	  const gchar *comments, GdaValueType type)
{
	GList *value_list = NULL;

	value_list = g_list_append (value_list, gda_value_new_string (typname));
	value_list = g_list_append (value_list, gda_value_new_string (owner));
	value_list = g_list_append (value_list, gda_value_new_string (comments));
	value_list = g_list_append (value_list, gda_value_new_type (type));

	gda_data_model_append_row (model, value_list);

	g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
	g_list_free (value_list);
}

static GdaDataModel *
get_mdb_types (GdaMdbConnection *mdb_cnc)
{
	GdaDataModel *model;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (mdb_cnc->mdb != NULL, NULL);

	model = gda_data_model_array_new (4);
	gda_data_model_set_column_title (model, 0, _("Name"));
	gda_data_model_set_column_title (model, 1, _("Owner"));
	gda_data_model_set_column_title (model, 2, _("Comments"));
	gda_data_model_set_column_title (model, 3, _("GDA Type"));

	add_type (model, "boolean", NULL, _("Boolean type"), GDA_VALUE_TYPE_BOOLEAN);
	add_type (model, "byte", NULL, _("1-byte integers"), GDA_VALUE_TYPE_TINYINT);
	add_type (model, "double", NULL, _("Double precision values"), GDA_VALUE_TYPE_DOUBLE);
	add_type (model, "float", NULL, _("Single precision values"), GDA_VALUE_TYPE_SINGLE);
	add_type (model, "int", NULL, _("32-bit integers"), GDA_VALUE_TYPE_INTEGER);
	add_type (model, "longint", NULL, _("64-bit integers"), GDA_VALUE_TYPE_BIGINT);
	add_type (model, "memo", NULL, _("Variable length character strings"), GDA_VALUE_TYPE_BINARY);
	add_type (model, "money", NULL, _("Money amounts"), GDA_VALUE_TYPE_DOUBLE);
	add_type (model, "ole", NULL, _("OLE object"), GDA_VALUE_TYPE_BINARY);
	add_type (model, "repid", NULL, _("FIXME"), GDA_VALUE_TYPE_BINARY);
	add_type (model, "sdatetime", NULL, _("Date/time value"), GDA_VALUE_TYPE_TIMESTAMP);
	add_type (model, "text", NULL, _("Character strings"), GDA_VALUE_TYPE_STRING);

	return model;
}

/* get_schema handler for the GdaMdbProvider class */
static GdaDataModel *
gda_mdb_provider_get_schema (GdaServerProvider *provider,
			     GdaConnection *cnc,
			     GdaConnectionSchema schema,
			     GdaParameterList *params)
{
	GdaMdbConnection *mdb_cnc;
	GdaMdbProvider *mdb_prv = (GdaMdbProvider *) provider;

	g_return_val_if_fail (GDA_IS_MDB_PROVIDER (mdb_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	mdb_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MDB_HANDLE);
	if (!mdb_cnc) {
		gda_connection_add_error_string (cnc, _("Invalid MDB handle"));
		return NULL;
	}

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_DATABASES :
		return get_mdb_databases (mdb_cnc);
	case GDA_CONNECTION_SCHEMA_FIELDS :
		return get_mdb_fields (mdb_cnc, params);
	case GDA_CONNECTION_SCHEMA_PROCEDURES :
		return get_mdb_procedures (mdb_cnc);
	case GDA_CONNECTION_SCHEMA_TABLES :
		return get_mdb_tables (mdb_cnc);
	case GDA_CONNECTION_SCHEMA_TYPES :
		return get_mdb_types (mdb_cnc);
	default :
		break;
	}

	return NULL;
}

GdaDataModel *
gda_mdb_provider_execute_sql (GdaMdbProvider *mdbprv, GdaConnection *cnc, const gchar *sql)
{
	gchar *bound_data[256];
	gint c, r;
	GdaMdbConnection *mdb_cnc;
	GdaTable *model;

	g_return_val_if_fail (GDA_IS_MDB_PROVIDER (mdbprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (sql != NULL, NULL);

	mdb_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MDB_HANDLE);
	if (!mdb_cnc) {
		gda_connection_add_error_string (cnc, _("Invalid MDB handle"));
		return NULL;
	}

	/* parse the SQL command */
	mdb_SQL->mdb = mdb_cnc->mdb;
	g_input_ptr = sql;
	/* begin unsafe */
	_mdb_sql (mdb_SQL);
	if (yyparse ()) {
		/* end unsafe */
		gda_connection_add_error_string (cnc, _("Could not parse '%s' command"), sql);
		mdb_sql_reset (mdb_SQL);
		return NULL;
	}
	if (!mdb_SQL->cur_table) {
		/* parsing went fine, but there is no result
		   (e.g. because of an invalid column name) */
		gda_connection_add_error_string (cnc, _("Got no result for '%s' command"), sql);
		return NULL;
	}

	model = gda_table_new (sql);

	/* allocate bound data */
	for (c = 0; c < mdb_SQL->num_columns; c++) {
		MdbSQLColumn *sqlcol;
		GdaFieldAttributes *fa;

		bound_data[c] = (gchar *) malloc (MDB_BIND_SIZE);
		bound_data[c][0] = '\0';
		mdbsql_bind_column (mdb_SQL, c + 1, bound_data[c]);

		/* set description for the field */
		sqlcol = g_ptr_array_index (mdb_SQL->columns, c);
		fa = gda_field_attributes_new ();
		gda_field_attributes_set_name (fa, sqlcol->name);
		gda_field_attributes_set_defined_size (fa, sqlcol->disp_size);
		gda_field_attributes_set_gdatype (fa, gda_mdb_type_to_gda (sqlcol->bind_type));

		/* and add it to the table */
		gda_table_add_field (model, (const GdaFieldAttributes *) fa);
		gda_field_attributes_free (fa);
	}

	/* read data */
	r = 0;
	while (mdb_fetch_row (mdb_SQL->cur_table)) {
		GList *value_list = NULL;

		r++;
		for (c = 0; c < mdb_SQL->num_columns; c++)
			value_list = g_list_append (value_list, gda_value_new_string (bound_data[c]));

		gda_data_model_append_row (GDA_DATA_MODEL (model), value_list);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	/* free memory */
	for (c = 0; c < mdb_SQL->num_columns; c++)
		free (bound_data[c]);
	mdb_sql_reset (mdb_SQL);

	return GDA_DATA_MODEL (model);
}
