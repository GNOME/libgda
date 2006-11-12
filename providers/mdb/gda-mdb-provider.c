/* GDA MDB provider
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-data-model-private.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-parameter-list.h>
#include "gda-mdb.h"

#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-type.h>
#include <libgda/handlers/gda-handler-bin.h>

#include <libgda/sql-delimiter/gda-sql-delimiter.h>

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
static GList *gda_mdb_provider_execute_command (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  GdaCommand *cmd,
						  GdaParameterList *params);
static gboolean gda_mdb_provider_supports (GdaServerProvider *provider,
					     GdaConnection *cnc,
					     GdaConnectionFeature feature);

static GdaServerProviderInfo *gda_mdb_provider_get_info (GdaServerProvider *provider,
							 GdaConnection *cnc);

static GdaDataModel *gda_mdb_provider_get_schema (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaConnectionSchema schema,
						    GdaParameterList *params);

static GdaDataHandler *gda_mdb_provider_get_data_handler (GdaServerProvider *provider,
							  GdaConnection *cnc,
							  GType g_type,
							  const gchar *dbms_type);

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
	provider_class->get_server_version = gda_mdb_provider_get_server_version;
	provider_class->get_info = gda_mdb_provider_get_info;
	provider_class->supports_feature = gda_mdb_provider_supports;
	provider_class->get_schema = gda_mdb_provider_get_schema;

	provider_class->get_data_handler = gda_mdb_provider_get_data_handler;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = NULL;

	provider_class->open_connection = gda_mdb_provider_open_connection;
	provider_class->close_connection = gda_mdb_provider_close_connection;
	provider_class->get_database = gda_mdb_provider_get_database;
	provider_class->change_database = gda_mdb_provider_change_database;

	provider_class->supports_operation = NULL;
        provider_class->create_operation = NULL;
        provider_class->render_operation = NULL;
        provider_class->perform_operation = NULL;

	provider_class->execute_command = gda_mdb_provider_execute_command;
	provider_class->get_last_insert_id = NULL;

	provider_class->begin_transaction = NULL;
	provider_class->commit_transaction = NULL;
	provider_class->rollback_transaction = NULL;
	provider_class->add_savepoint = NULL;
	provider_class->rollback_savepoint = NULL;
	provider_class->delete_savepoint = NULL;
	
	provider_class->create_blob = NULL;
	provider_class->fetch_blob = NULL;
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
		gda_connection_add_event_string (
			cnc,
			_("A database FILENAME must be specified in the connection_string"));
		return FALSE;
	}

	mdb_cnc = g_new0 (GdaMdbConnection, 1);
	mdb_cnc->cnc = cnc;
	mdb_cnc->server_version = NULL;
	mdb_cnc->mdb = mdb_open (filename, MDB_WRITABLE);
	if (!mdb_cnc->mdb) {
		gda_connection_add_event_string (cnc, _("Could not open file %s"), filename);
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
		gda_connection_add_event_string (cnc, _("Invalid MDB handle"));
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
		gda_connection_add_event_string (cnc, _("Invalid MDB handle"));
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
		gda_connection_add_event_string (cnc, _("Invalid MDB handle"));
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
		gda_connection_add_event_string (cnc, _("Invalid MDB handle"));
		return NULL;
	}

	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL :
		arr = gda_delimiter_split_sql (cmd->text);
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

static GdaServerProviderInfo *
gda_mdb_provider_get_info (GdaServerProvider *provider,
			   GdaConnection *cnc)
{
	static GdaServerProviderInfo info = {
		"Mdb",
		TRUE, 
		TRUE,
		FALSE,
		FALSE,
		FALSE,
		TRUE
	};
	
	return &info;
}


static GdaDataModel *
get_mdb_databases (GdaMdbConnection *mdb_cnc)
{
	GdaDataModel *model;
	GValue *value;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (mdb_cnc->mdb != NULL, NULL);

	model = gda_data_model_array_new (1);
	gda_data_model_set_column_title (model, 0, _("Name"));

	g_value_set_string (value = gda_value_new (G_TYPE_STRING), mdb_cnc->mdb->f->filename);
	gda_data_model_set_value_at (model, 0, 0, value, NULL);
	gda_value_free (value);

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
	MdbIndex *mdb_idx;
	gint i, j;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (mdb_cnc->mdb != NULL, NULL);

	par = gda_parameter_list_find_param (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	table_name = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (table_name != NULL, NULL);

	/* create the data model */
	model = gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_FIELDS));
	gda_server_provider_init_schema_model (model, GDA_CONNECTION_SCHEMA_FIELDS);
	
	/* fill in the data model with the information for the table */
	for (i = 0; i < mdb_cnc->mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb_cnc->mdb->catalog, i);
		if (entry->object_type == MDB_TABLE &&
		    !strcmp (entry->object_name, table_name)) {
			mdb_table = mdb_read_table (entry);
			mdb_read_columns (mdb_table);
			mdb_read_indices (mdb_table);

			mdb_idx = NULL;
			for (j = 0; !mdb_idx && (j < mdb_table->num_idxs); j++) {
				mdb_idx = g_ptr_array_index (mdb_table->indices, j);
				/*g_print ("=======\n");
				  mdb_index_dump (mdb_table, mdb_idx);*/
				if (mdb_idx->index_type != 1)
					mdb_idx = NULL;
			}

			for (j = 0; j < mdb_table->num_cols; j++) {
				GList *value_list = NULL;
				GValue *tmpval;
				
				mdb_col = g_ptr_array_index (mdb_table->columns, j);

				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), mdb_col->name);
				value_list = g_list_append (value_list, tmpval);

				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
						    mdb_get_coltype_string (mdb_cnc->mdb->default_backend,
									    mdb_col->col_type));
				value_list = g_list_append (value_list, tmpval);

				g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), mdb_col->col_size);
				value_list = g_list_append (value_list, tmpval);

				g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), mdb_col->col_scale);
				value_list = g_list_append (value_list, tmpval);

				g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), 
						     mdb_col->is_fixed ? TRUE : FALSE);
				value_list = g_list_append (value_list, tmpval);

				/* PK */
				tmpval = gda_value_new (G_TYPE_BOOLEAN);
				if (mdb_idx) {
					int k;
					gboolean ispk = FALSE;

					for (k = 0; !ispk && (k < mdb_idx->num_keys) ; k++) {
						if (j == mdb_idx->key_col_num[k]-1)
							ispk = TRUE;
					}
					g_value_set_boolean (tmpval, ispk);
				}
				else
					g_value_set_boolean (tmpval, FALSE);
				value_list = g_list_append (value_list, tmpval);

				g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
				value_list = g_list_append (value_list, tmpval);

				value_list = g_list_append (value_list, gda_value_new_null ());
				value_list = g_list_append (value_list, gda_value_new_null ());
				value_list = g_list_append (value_list, gda_value_new_null ());

				gda_data_model_append_values (model, value_list, NULL);

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
	GdaDataModel *model;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (mdb_cnc->mdb != NULL, NULL);

	/* create the data model */
	model =  gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_PROCEDURES));
	gda_server_provider_init_schema_model (model, GDA_CONNECTION_SCHEMA_PROCEDURES);

	for (i = 0; i < mdb_cnc->mdb->num_catalog; i++) {
		MdbCatalogEntry *entry;

		entry = g_ptr_array_index (mdb_cnc->mdb->catalog, i);

 		/* if it's a table */
		if (entry->object_type == MDB_MODULE) {
			GList *value_list = NULL;
			GValue *tmpval;
			
			g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), entry->object_name);
			value_list = g_list_append (value_list, tmpval);

			value_list = g_list_append (value_list, gda_value_new_null ());
			value_list = g_list_append (value_list, gda_value_new_null ());
			value_list = g_list_append (value_list, gda_value_new_null ());
			value_list = g_list_append (value_list, gda_value_new_null ());
			value_list = g_list_append (value_list, gda_value_new_null ());
			value_list = g_list_append (value_list, gda_value_new_null ());
			value_list = g_list_append (value_list, gda_value_new_null ());

			gda_data_model_append_values (model, value_list, NULL);

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
	GdaDataModel *model;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (mdb_cnc->mdb != NULL, NULL);

	model = gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TABLES));
	gda_server_provider_init_schema_model (model, GDA_CONNECTION_SCHEMA_TABLES);

	for (i = 0; i < mdb_cnc->mdb->num_catalog; i++) {
		MdbCatalogEntry *entry;

		entry = g_ptr_array_index (mdb_cnc->mdb->catalog, i);

 		/* if it's a table */
		if (entry->object_type == MDB_TABLE) {
			/* skip the MSys tables */
			if (strncmp (entry->object_name, "MSys", 4)) {
				GList *value_list = NULL;
				GValue *tmpval;

				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), entry->object_name);
				value_list = g_list_append (value_list, tmpval);

				value_list = g_list_append (value_list, gda_value_new_null ());
				value_list = g_list_append (value_list, gda_value_new_null ());
				value_list = g_list_append (value_list, gda_value_new_null ());

				gda_data_model_append_values (model, value_list, NULL);

				g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
				g_list_free (value_list);
			}
		}
	}

	return GDA_DATA_MODEL (model);
}

static void
add_type (GdaDataModel *model, const gchar *typname, const gchar *owner,
	  const gchar *comments, GType type, const gchar *synonyms)
{
	GList *value_list = NULL;
	GValue *tmpval;

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), typname);
	value_list = g_list_append (value_list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), owner);
	value_list = g_list_append (value_list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), comments);
	value_list = g_list_append (value_list, tmpval);

	g_value_set_ulong (tmpval = gda_value_new (G_TYPE_ULONG), type);
	value_list = g_list_append (value_list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), synonyms);
	value_list = g_list_append (value_list, tmpval);

	gda_data_model_append_values (model, value_list, NULL);

	g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
	g_list_free (value_list);
}

static GdaDataModel *
get_mdb_types (GdaMdbConnection *mdb_cnc)
{
	GdaDataModel *model;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (mdb_cnc->mdb != NULL, NULL);

	model = gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TYPES));
	gda_server_provider_init_schema_model (model, GDA_CONNECTION_SCHEMA_TYPES);

	add_type (model, "boolean", NULL, _("Boolean type"), G_TYPE_BOOLEAN, NULL);
	add_type (model, "byte", NULL, _("1-byte integers"), G_TYPE_CHAR, NULL);
	add_type (model, "integer", NULL, _("32-bit integers"), G_TYPE_INT, "int");
	add_type (model, "long integer", NULL, _("64-bit integers"), G_TYPE_INT64, "longint");
	add_type (model, "currency", NULL, _("Money amounts"), G_TYPE_DOUBLE, "money");
	add_type (model, "single", NULL, _("Single precision values"), G_TYPE_FLOAT, "float");
	add_type (model, "double", NULL, _("Double precision values"), G_TYPE_DOUBLE, NULL);
	add_type (model, "datetime", NULL, _("Date/time value"), GDA_TYPE_TIMESTAMP, "dateTime (short)");
	add_type (model, "text", NULL, _("Character strings"), G_TYPE_STRING, NULL);
	add_type (model, "ole", NULL, _("OLE object"), GDA_TYPE_BINARY, NULL);
	add_type (model, "memo", NULL, _("Variable length character strings"), G_TYPE_STRING, "memo/hyperlink,hyperlink");
	add_type (model, "repid", NULL, _("Replication ID"), GDA_TYPE_BINARY, NULL);
	add_type (model, "numeric", NULL, _("Numeric"), GDA_TYPE_NUMERIC, NULL);

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
		gda_connection_add_event_string (cnc, _("Invalid MDB handle"));
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
	GdaDataModel *model;
	GType *coltypes;

	g_return_val_if_fail (GDA_IS_MDB_PROVIDER (mdbprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (sql != NULL, NULL);

	mdb_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MDB_HANDLE);
	if (!mdb_cnc) {
		gda_connection_add_event_string (cnc, _("Invalid MDB handle"));
		return NULL;
	}

	/* parse the SQL command */
	mdb_SQL->mdb = mdb_cnc->mdb;
	g_input_ptr = (char *) sql;
	/* begin unsafe */
	_mdb_sql (mdb_SQL);
	if (yyparse ()) {
		/* end unsafe */
		gda_connection_add_event_string (cnc, _("Could not parse '%s' command"), sql);
		mdb_sql_reset (mdb_SQL);
		return NULL;
	}
	if (!mdb_SQL->cur_table) {
		/* parsing went fine, but there is no result
		   (e.g. because of an invalid column name) */
		gda_connection_add_event_string (cnc, _("Got no result for '%s' command"), sql);
		return NULL;
	}

	model = gda_data_model_array_new (mdb_SQL->num_columns);
	g_object_set (G_OBJECT (model), 
		      "command_text", sql, NULL);

	MdbTableDef *mdb_table;
	
	mdb_table = mdb_SQL->cur_table;
	mdb_read_columns (mdb_table);
	
	coltypes = g_new0 (GType, mdb_table->num_cols);
	for (c = 0; c < mdb_table->num_cols; c++) {
		MdbColumn *mdb_col;
		GdaColumn *fa;
		
		/* column type */
		mdb_col = g_ptr_array_index (mdb_table->columns, c);
		coltypes [c] = gda_mdb_type_to_gda (mdb_col->col_type);
		
		/* allocate bound data */
		bound_data[c] = (gchar *) malloc (MDB_BIND_SIZE);
		bound_data[c][0] = '\0';
		mdb_sql_bind_column (mdb_SQL, c + 1, bound_data[c], NULL);
		
		/* set description for the field */
		fa = gda_data_model_describe_column (model, c);
		gda_column_set_name (fa, mdb_col->name);
		gda_column_set_g_type (fa, coltypes [c]);
		gda_column_set_defined_size (fa, mdb_col->col_size);
	}

	/* read data */
	r = 0;
	while (mdb_fetch_row (mdb_SQL->cur_table)) {
		GList *value_list = NULL;
		GValue *tmpval;

		r++;
		for (c = 0; c < mdb_SQL->num_columns; c++) {
			gda_value_set_from_string ((tmpval = gda_value_new (coltypes [c])), bound_data[c], coltypes [c]);
			value_list = g_list_append (value_list, tmpval);
		}

		gda_data_model_append_values (GDA_DATA_MODEL (model), value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	/* free memory */
	g_free (coltypes);
	for (c = 0; c < mdb_SQL->num_columns; c++)
		free (bound_data[c]);
	mdb_sql_reset (mdb_SQL);
	return model;
}

static GdaDataHandler *
gda_mdb_provider_get_data_handler (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   GType type,
				   const gchar *dbms_type)
{
	GdaDataHandler *dh = NULL;
	GdaMdbProvider *mdb_prv = GDA_MDB_PROVIDER (provider);

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
	    (type == G_TYPE_UINT)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_numerical_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT64, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UINT64, NULL);
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
        else if ((type == GDA_TYPE_BINARY) ||
		 (type == GDA_TYPE_BLOB)) {
		dh = gda_server_provider_handler_find (provider, cnc, type, NULL);
		if (!dh) {
			dh = gda_handler_bin_new_with_prov (provider, cnc);
			if (dh) {
				gda_server_provider_handler_declare (provider, dh, cnc, GDA_TYPE_BINARY, NULL);
				gda_server_provider_handler_declare (provider, dh, cnc, GDA_TYPE_BLOB, NULL);
				g_object_unref (dh);
			}
		}
	}
        else if (type == G_TYPE_BOOLEAN) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_boolean_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_BOOLEAN, NULL);
			g_object_unref (dh);
		}
	}
	else if ((type == G_TYPE_DATE) ||
		 (type == GDA_TYPE_TIME) ||
		 (type == GDA_TYPE_TIMESTAMP)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_time_new_no_locale ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DATE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIME, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIMESTAMP, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == G_TYPE_STRING) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_string_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_STRING, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == G_TYPE_ULONG) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_type_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_ULONG, NULL);
			g_object_unref (dh);
		}
	}
	else {
		if (dbms_type) {
			TO_IMPLEMENT;
		}
	}

	return dh;
}
