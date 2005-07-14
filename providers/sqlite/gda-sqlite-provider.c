/* GDA SQLite provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Carlos Perelló Marín <carlos@gnome-db.org>
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

#if defined(HAVE_CONFIG_H)
#endif

#include <stdlib.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-util.h>
#include "gda-sqlite.h"
#include "gda-sqlite-provider.h"
#include "gda-sqlite-recordset.h"
#include "sqliteInt.h"

static void gda_sqlite_provider_class_init (GdaSqliteProviderClass *klass);
static void gda_sqlite_provider_init       (GdaSqliteProvider *provider,
					    GdaSqliteProviderClass *klass);
static void gda_sqlite_provider_finalize   (GObject *object);

static const gchar *gda_sqlite_provider_get_version (GdaServerProvider *provider);
static gboolean gda_sqlite_provider_open_connection (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaQuarkList *params,
						     const gchar *username,
						     const gchar *password);
static gboolean gda_sqlite_provider_close_connection (GdaServerProvider *provider,
						      GdaConnection *cnc);
static const gchar *gda_sqlite_provider_get_server_version (GdaServerProvider *provider,
							    GdaConnection *cnc);
static const gchar *gda_sqlite_provider_get_database (GdaServerProvider *provider,
						  GdaConnection *cnc);
static gboolean gda_sqlite_provider_change_database (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     const gchar *name);
static gchar *gda_sqlite_provider_get_specs_create_database (GdaServerProvider *provider);
static gboolean gda_sqlite_provider_create_database (GdaServerProvider *provider,
						     GdaParameterList *params, GError **error);
static gboolean gda_sqlite_provider_drop_database (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   const gchar *name);

static GList *gda_sqlite_provider_execute_command (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   GdaCommand *cmd,
						   GdaParameterList *params);

static gboolean gda_sqlite_provider_begin_transaction (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaTransaction *xaction);
static gboolean gda_sqlite_provider_commit_transaction (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaTransaction *xaction);
static gboolean gda_sqlite_provider_rollback_transaction (GdaServerProvider *provider,
							  GdaConnection * cnc,
							  GdaTransaction *xaction);

static gboolean gda_sqlite_provider_single_command (const GdaSqliteProvider *provider,
						    GdaConnection *cnc,
						    const gchar *command);

static gboolean gda_sqlite_provider_supports (GdaServerProvider *provider,
					      GdaConnection *cnc,
					      GdaConnectionFeature feature);

static GdaDataModel *gda_sqlite_provider_get_schema (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaConnectionSchema schema,
						     GdaParameterList *params);

static GObjectClass *parent_class = NULL;

/*
 * GdaSqliteProvider class implementation
 */

static void
gda_sqlite_provider_class_init (GdaSqliteProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_sqlite_provider_finalize;
	provider_class->get_version = gda_sqlite_provider_get_version;
	provider_class->open_connection = gda_sqlite_provider_open_connection;
	provider_class->close_connection = gda_sqlite_provider_close_connection;
	provider_class->get_server_version = gda_sqlite_provider_get_server_version;
	provider_class->get_database = gda_sqlite_provider_get_database;
	provider_class->change_database = gda_sqlite_provider_change_database;
	provider_class->get_specs_create_database = gda_sqlite_provider_get_specs_create_database;
	provider_class->create_database_params = gda_sqlite_provider_create_database;
	provider_class->drop_database = gda_sqlite_provider_drop_database;
	provider_class->execute_command = gda_sqlite_provider_execute_command;
	provider_class->begin_transaction = gda_sqlite_provider_begin_transaction;
	provider_class->commit_transaction = gda_sqlite_provider_commit_transaction;
	provider_class->rollback_transaction = gda_sqlite_provider_rollback_transaction;
	provider_class->supports = gda_sqlite_provider_supports;
	provider_class->get_schema = gda_sqlite_provider_get_schema;
}

static void
gda_sqlite_provider_init (GdaSqliteProvider *sqlite_prv, GdaSqliteProviderClass *klass)
{
}

static void
gda_sqlite_provider_finalize (GObject *object)
{
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) object;

	g_return_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_sqlite_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaSqliteProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_sqlite_provider_class_init,
			NULL, NULL,
			sizeof (GdaSqliteProvider),
			0,
			(GInstanceInitFunc) gda_sqlite_provider_init
		};
		type = g_type_register_static (PARENT_TYPE,
					       "GdaSqliteProvider",
					       &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_sqlite_provider_new (void)
{
	GdaSqliteProvider *provider;

	provider = g_object_new (gda_sqlite_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

/* get_version handler for the GdaSqliteProvider class */
static const gchar *
gda_sqlite_provider_get_version (GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/* open_connection handler for the GdaSqliteProvider class */
static gboolean
gda_sqlite_provider_open_connection (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaQuarkList *params,
				     const gchar *username,
				     const gchar *password)
{
	const gchar *t_filename = NULL;
	gint errmsg;
	SQLITEcnc *scnc;
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* get all parameters received */
	t_filename = gda_quark_list_find (params, "URI");

	if (!t_filename || *t_filename != '/') {
		gda_connection_add_error_string (cnc,
						 _("A full path must be specified on the "
						   "connection string to open a database."));
		return FALSE;
	}

	scnc = g_new0 (SQLITEcnc, 1);

	errmsg = sqlite3_open (t_filename, &scnc->connection);
	scnc->file = g_strdup (t_filename);

	if (errmsg != SQLITE_OK) {
		printf("error %s", sqlite3_errmsg (scnc->connection));
		gda_connection_add_error_string (cnc, sqlite3_errmsg (scnc->connection));
		sqlite3_close (scnc->connection);
		g_free (scnc->file);
		g_free (scnc);
			
		return FALSE;
	}

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE, scnc);

	/* set SQLite library options */
	if (!gda_sqlite_provider_single_command (sqlite_prv, cnc, "PRAGMA empty_result_callbacks = ON"))
		gda_connection_add_error_string (cnc, _("Could not set empty_result_callbacks SQLite option"));

	/* make sure the internals are completely initialized now */
	{
		SQLITEresult *sres;
		gint status, i;
		gchar *errmsg;
		
		sres = g_new0 (SQLITEresult, 1);
		status = sqlite3_get_table (scnc->connection, "SELECT name"
					    " FROM (SELECT * FROM sqlite_master UNION ALL "
					    "       SELECT * FROM sqlite_temp_master)",
					    &sres->data,
					    &sres->nrows,
					    &sres->ncols,
					    &errmsg);
		if (status == SQLITE_OK) {
			sqlite3_free_table (sres->data);
			g_free (sres);
		}
		else {
			g_print ("error: %s", errmsg);
			gda_connection_add_error_string (cnc, errmsg);
			sqlite3_free (errmsg);
			sqlite3_close (scnc->connection);
			g_free (scnc->file);
			g_free (scnc);
			g_free (sres);

			return FALSE;
		}
	}

	if (0) {
		/* show all databases in this handle */
		Db *db;
		gint i;

		for (i=0; i < scnc->connection->nDb; i++) {
			db = &(scnc->connection->aDb[i]);
			g_print ("/// %s\n", db->zName);
		}
	}

	return TRUE;
}

/* close_connection handler for the GdaSqliteProvider class */
static gboolean
gda_sqlite_provider_close_connection (GdaServerProvider *provider,
				      GdaConnection *cnc)
{
	SQLITEcnc *scnc;
	
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_error_string (cnc, _("Invalid SQLite handle"));
		return FALSE;
	}

	sqlite3_close (scnc->connection);
	g_free (scnc->file);
	g_free (scnc);
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE, NULL);

	return TRUE;
}

static gchar **
sql_split (const gchar *sql)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array;
	guint n = 0;
	const gchar *remainder = sql, *next = sql;

	while ((next = (const gchar *) strchr (next, ';')) != NULL) {
		gchar *tmp = g_strndup (remainder, next - remainder + 1);
		if (sqlite_complete (tmp)) {
			string_list = g_slist_prepend (string_list, tmp);
			n++;
			remainder = (const gchar *) (next + 1);
		} else {
			g_free (tmp);
		}
		next++;
	}

	if (*remainder) {
		n++;
		string_list = g_slist_prepend (string_list, g_strdup (remainder));
	}

	str_array = g_new (gchar*, n + 1);

	str_array[n--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
		str_array[n--] = slist->data;

	g_slist_free (string_list);

	return str_array;
}
  
static GList *
process_sql_commands (GList *reclist, GdaConnection *cnc,
		      const gchar *sql, GdaCommandOptions options)
{
	SQLITEcnc *scnc;
	gchar  *errmsg;
	gchar **arr;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_error_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = sql_split (sql);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			SQLITEresult *sres;
			GdaDataModel *recset;
			gint status, i;

			sres = g_new0 (SQLITEresult, 1);
			status = sqlite3_get_table (scnc->connection, arr[n],
						   &sres->data,
						   &sres->nrows,
						   &sres->ncols,
						   &errmsg);
			if (options & GDA_COMMAND_OPTION_IGNORE_ERRORS ||
			    status == SQLITE_OK) {

				recset = gda_sqlite_recordset_new (cnc, sres);
				if (GDA_IS_DATA_MODEL (recset)) {
					gda_data_model_set_command_text (recset, arr[n]);
					gda_data_model_set_command_type (recset, GDA_COMMAND_TYPE_SQL);
					for (i = sres->ncols; i >= 0; i--)
						gda_data_model_set_column_title (recset, i,
								sres->data[i]);
					reclist = g_list_append (reclist, recset);
				}
			} else {
				GdaError *error = gda_error_new ();
				gda_error_set_description (error, errmsg);
				gda_connection_add_error (cnc, error);

				g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
				g_list_free (reclist);
				free (errmsg);
				reclist = NULL;

				break;
			}

			n++;
		}

		g_strfreev (arr);
	}

	return reclist;
}

static const gchar *
gda_sqlite_provider_get_server_version (GdaServerProvider *provider,
					GdaConnection *cnc)
{
	static gchar *version_string = NULL;

	if (!version_string)
		version_string = g_strdup_printf ("SQLite version %s", SQLITE_VERSION);

	return (const gchar *) version_string;
}

static const gchar *
gda_sqlite_provider_get_database (GdaServerProvider *provider,
				  GdaConnection *cnc)
{
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;
	SQLITEcnc *scnc;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);

	if (!scnc) {
		gda_connection_add_error_string (cnc, _("Invalid SQLite handle"));
		return NULL;
	}

	return scnc->file;
}

/* change_database handler for the GdaSqliteProvider class */
static gboolean
gda_sqlite_provider_change_database (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     const gchar *name)
{
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	gda_connection_add_error_string (cnc, _("Only one database per connection is allowed"));
	return FALSE;
}

/* get_specs_create_database handler for the GdaSqliteProvider class */
static gchar *
gda_sqlite_provider_get_specs_create_database (GdaServerProvider *provider)
{
	gchar *specs, *file;
        gint len;
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);

        file = g_build_filename (LIBGDA_DATA_DIR, "sqlite_specs_create_db.xml", NULL);
        if (g_file_get_contents (file, &specs, &len, NULL))
                return specs;
        else
                return NULL;
}

/* create_database handler for the GdaSqliteProvider class */
static gboolean
gda_sqlite_provider_create_database (GdaServerProvider *provider,
				     GdaParameterList *params, GError **error)
{
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;
	gboolean retval;
	GdaParameter *param = NULL;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	
	if (params)
		param = gda_parameter_list_find (params, "URI");
	if (!param) {
		g_set_error (error, 0, 0,
			     _("Missing parameter 'URI'"));
		retval = FALSE;
	}
	else {
		SQLITEcnc *scnc;
		gint errmsg;
		gchar *filename;

		filename = gda_value_get_string (gda_parameter_get_value (param));

		scnc = g_new0 (SQLITEcnc, 1);
		errmsg = sqlite3_open (filename, &scnc->connection);
		
		if (errmsg != SQLITE_OK) {
			g_set_error (error, 0, 0,
				     sqlite3_errmsg (scnc->connection));
			retval = FALSE;
		}
		else
			retval = TRUE;
		sqlite3_close (scnc->connection);
		g_free (scnc);
	}

	return retval;
}

static gboolean
gda_sqlite_provider_drop_database (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   const gchar *name)
{
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	gda_connection_add_error_string (cnc, _("To remove a SQLite database you should remove the database file by hand"));
	return FALSE;
}

/* execute_command handler for the GdaSqliteProvider class */
static GList *
gda_sqlite_provider_execute_command (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaCommand *cmd,
				     GdaParameterList *params)
{
	GList  *reclist = NULL;
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;
	GdaCommandOptions options;
	gchar **arr;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	options = gda_command_get_options (cmd);
	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL:
		reclist = process_sql_commands (reclist, cnc,
						gda_command_get_text (cmd),
						options);
		break;
	case GDA_COMMAND_TYPE_XML:
		/* FIXME: Implement me */
		return NULL;
	case GDA_COMMAND_TYPE_PROCEDURE:
		/* FIXME: Implement me */
		return NULL;
	case GDA_COMMAND_TYPE_TABLE:
		/* there can be multiple table names */
		arr = g_strsplit (gda_command_get_text (cmd), ";", 0);
		if (arr) {
			GString *str = NULL;
			gint n = 0;

			while (arr[n]) {
				if (!str)
					str = g_string_new ("SELECT * FROM ");
				else
					str = g_string_append (str, "; SELECT * FROM ");

				str = g_string_append (str, arr[n]);

				n++;
			}

			reclist = process_sql_commands (reclist, cnc, str->str, options);

			g_string_free (str, TRUE);
			g_strfreev (arr);
		}
		break;
	case GDA_COMMAND_TYPE_SCHEMA:
		/* FIXME: Implement me */
		return NULL;
	case GDA_COMMAND_TYPE_INVALID:
		return NULL;
	}

	return reclist;
}

static gboolean
gda_sqlite_provider_begin_transaction (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaTransaction *xaction)
{
	gboolean status;
	gchar *sql;
	gchar *name;
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_error_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	name = gda_transaction_get_name (xaction);
	if (name)
		sql = g_strdup_printf ("BEGIN TRANSACTION %s", name);
	else
		sql = g_strdup_printf ("BEGIN TRANSACTION");

	status = gda_sqlite_provider_single_command (sqlite_prv, cnc, sql);
	g_free (sql);

	return status;
}

static gboolean
gda_sqlite_provider_commit_transaction (GdaServerProvider *provider,
					GdaConnection *cnc,
					GdaTransaction *xaction)
{
	gboolean status;
	gchar *sql;
	gchar *name;
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	name = gda_transaction_get_name (xaction);
	if (name)
		sql = g_strdup_printf ("COMMIT TRANSACTION %s", name);
	else
		sql = g_strdup_printf ("COMMIT TRANSACTION");

	status = gda_sqlite_provider_single_command (sqlite_prv, cnc, sql);
	g_free (sql);

	return status;
}

static gboolean
gda_sqlite_provider_rollback_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GdaTransaction *xaction)
{
	gboolean status;
	gchar *sql;
	gchar *name;
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	name = gda_transaction_get_name (xaction);
	if (name)
		sql = g_strdup_printf ("ROLLBACK TRANSACTION %s", name);
	else
		sql = g_strdup_printf ("ROLLBACK TRANSACTION");

	status = gda_sqlite_provider_single_command (sqlite_prv, cnc, sql);
	g_free (sql);

	return status;
}

static gboolean
gda_sqlite_provider_single_command (const GdaSqliteProvider *provider,
				    GdaConnection *cnc,
				    const gchar *command)
{

	SQLITEcnc *scnc;
	gboolean result;
	gint status;
	gchar *errmsg = NULL;
	
	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);

	if (!scnc) {
		gda_connection_add_error_string (cnc, _("Invalid SQLite handle"));
		return FALSE;
	}
	
	status = sqlite3_exec (scnc->connection, command, NULL, NULL, &errmsg);
	if (status == SQLITE_OK)
		result = TRUE;
	else {
		GdaError *error = gda_error_new ();
		gda_error_set_description (error, errmsg);
		gda_connection_add_error (cnc, error);

		result = FALSE;
	}
	free (errmsg);

	return result;
}

static gboolean
gda_sqlite_provider_supports (GdaServerProvider *provider,
			      GdaConnection *cnc,
			      GdaConnectionFeature feature)
{
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);

	switch (feature) {
	case GDA_CONNECTION_FEATURE_SQL :
	case GDA_CONNECTION_FEATURE_TRANSACTIONS :
	case GDA_CONNECTION_FEATURE_AGGREGATES :
	case GDA_CONNECTION_FEATURE_INDEXES :
	case GDA_CONNECTION_FEATURE_TRIGGERS :
	case GDA_CONNECTION_FEATURE_VIEWS :
		return TRUE;
	case GDA_CONNECTION_FEATURE_PROCEDURES :
	default: ;
	}

	return FALSE;
}

static void
add_type_row (GdaDataModelArray *recset, const gchar *name,
	      const gchar *owner, const gchar *comments,
	      GdaValueType type)
{
	GList *value_list;

	value_list = g_list_append (NULL, gda_value_new_string (name));
	value_list = g_list_append (value_list, gda_value_new_string (owner));
	value_list = g_list_append (value_list, gda_value_new_string (comments));
	value_list = g_list_append (value_list, gda_value_new_gdatype (type));
	value_list = g_list_append (value_list, gda_value_new_string (NULL));

	gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list);

	g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
	g_list_free (value_list);
}

typedef struct {
        gchar *col_name;
        GdaValueType data_type;
} GdaSqliteColData;


static void
add_g_list_row (gpointer data, gpointer user_data)
{
	GList *rowlist = data;
	GdaDataModelArray *recset = user_data;

	gda_data_model_append_values (GDA_DATA_MODEL (recset), rowlist);
	g_list_foreach (rowlist, (GFunc) gda_value_free, NULL);
	g_list_free (rowlist);
}


/*
 * Tables' fields
 */
static GdaDataModelArray *
init_table_fields_recset (GdaConnection *cnc)
{
	GdaDataModelArray *recset;
	gint i;
	GdaSqliteColData cols[] = {
		{ N_("Field name")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Data type")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Size")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Scale")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Not null?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Primary key?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Unique index?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("References")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Default value")   , GDA_VALUE_TYPE_STRING  },
		{ N_("Extra attributes"), GDA_VALUE_TYPE_STRING  }		
	};

	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (sizeof cols / sizeof cols[0]));
	for (i = 0; i < sizeof cols / sizeof cols[0]; i++)
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i, _(cols[i].col_name));

	return recset;
}

static GdaDataModel *
get_table_fields (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	SQLITEcnc *scnc;
	GdaParameter *par;
	const gchar *tblname;
	GList *reclist;
        gchar *sql;
	GdaDataModel *selmodel = NULL;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_error_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}

	recset = init_table_fields_recset (cnc);

	/* find table name */
	par = gda_parameter_list_find (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	tblname = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname != NULL, NULL);

	/* run the "SELECT * from table" to fetch information */
	sql = g_strdup_printf ("SELECT * FROM %s", tblname);
        reclist = process_sql_commands (NULL, cnc, sql, 0);
        g_free (sql);
	if (reclist) 
		selmodel = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	g_assert ((scnc->connection->flags & SQLITE_Initialized) || scnc->connection->init.busy);

	/* Fetch more information in memory */
	if (SQLITE_VERSION_NUMBER >= 3000000) {
		Table *table;
		gint i;
		GList *list = NULL;

		table = sqlite3FindTable (scnc->connection, tblname, NULL); /* FIXME: database name as 3rd arg */
		if (!table) {
			gda_connection_add_error_string (cnc, _("Can't find table %s"), tblname);
			return GDA_DATA_MODEL (recset);
		}

		for (i = 0; i < table->nCol; i++) {
			GList *rowlist = NULL;
			GdaValue *value;
			Column *column = & ((table->aCol)[i]);
			GdaColumn *fa = NULL;
			
			if (selmodel)
				fa = gda_data_model_describe_column (selmodel, i);

			/* col name */
			value = gda_value_new_string (column->zName);
			rowlist = g_list_append (rowlist, value);

			/* data type (column->affinity = SQLITE_AFF_... */
			value = gda_value_new_string (column->zType);
			rowlist = g_list_append (rowlist, value);

			/* size */
			if (fa)
				value = gda_value_new_integer (gda_column_get_defined_size (fa));
			else
				value = gda_value_new_integer (-1);
			rowlist = g_list_append (rowlist, value);

			/* scale */
			if (fa)
				value = gda_value_new_integer (gda_column_get_scale (fa));
			else
				value = gda_value_new_integer (-1);
			rowlist = g_list_append (rowlist, value);

			/* not null */
			value = gda_value_new_boolean (column->notNull ? TRUE : FALSE);
			rowlist = g_list_append (rowlist, value);

			/* PK */
			value = gda_value_new_boolean (column->isPrimKey ? TRUE : FALSE);
			rowlist = g_list_append (rowlist, value);

			/* unique index */
			if (column->isPrimKey)
				value = gda_value_new_boolean (TRUE);
			else {
				gboolean found = FALSE;
				Index *index = NULL;
				index = table->pIndex;
				while (index && !found) {
					if (index->pTable == table) {
						if ((index->nColumn == 1) && 
						    (index->aiColumn == i) && index->autoIndex)
							found = TRUE;
					}
					index = index->pNext;
				}
				value = gda_value_new_boolean (found);
			}
			rowlist = g_list_append (rowlist, value);

			/* FK */
			if (table->pFKey) {
				FKey *fk = table->pFKey;
				gboolean found = FALSE;
				
				while (fk && !found) {
					gint ifk;
					g_assert (fk->pFrom == table);
					
					ifk = 0;
					while ((ifk < fk->nCol) && !found) {
						if ((fk->aCol [ifk]).iFrom == i) {
							gchar *str;
							found = TRUE;

							str = g_strdup_printf ("%s.%s", fk->zTo, (fk->aCol [ifk]).zCol);
							value = gda_value_new_string (str);
							g_free (str);
						}
						ifk++;
					}

					fk = fk->pNextFrom;
				}
				if (!found)
					value = gda_value_new_string ("");
			}
			else
				value = gda_value_new_string ("");
			rowlist = g_list_append (rowlist, value);

			/* default value */
			if (column->pDflt)
				value = gda_value_new_string (column->pDflt->token.z);
			else
				value = gda_value_new_string ("");
			rowlist = g_list_append (rowlist, value);

			/* extra attributes */
			if ((table->iPKey == i) && table->autoInc)
				value = gda_value_new_string ("AUTO_INCREMENT");
			else
				value = gda_value_new_string ("");
			rowlist = g_list_append (rowlist, value);

			list = g_list_append (list, rowlist);
		}
		
		g_list_foreach (list, add_g_list_row, recset);
		g_list_free (list);
	}
	else {
		/* 2.x versions */
		GList *list = NULL;
		gint i;

		for (i = 0; i < gda_data_model_get_n_columns (GDA_DATA_MODEL (reclist->data)); i++) {
			GdaColumn *fa;
			GList *rowlist = NULL;
			
			fa = gda_data_model_describe_column (GDA_DATA_MODEL (reclist->data), i);
			if (!fa) {
				gda_connection_add_error_string (cnc, _("Could not retrieve information for field"));
				g_object_unref (G_OBJECT (recset));
				recset = NULL;
				break;
			}
			
			rowlist = g_list_append (rowlist,
						 gda_value_new_string (gda_column_get_name (fa)));
			rowlist = g_list_append (rowlist,
						 gda_value_new_string (gda_type_to_string (gda_column_get_gdatype (fa))));
			rowlist = g_list_append (rowlist,
						 gda_value_new_integer (gda_column_get_defined_size (fa)));
			rowlist = g_list_append (rowlist,
						 gda_value_new_integer (gda_column_get_scale (fa)));
			rowlist = g_list_append (rowlist,
						 gda_value_new_boolean (!gda_column_get_allow_null (fa)));
			rowlist = g_list_append (rowlist,
						 gda_value_new_boolean (gda_column_get_primary_key (fa)));
			rowlist = g_list_append (rowlist,
						 gda_value_new_boolean (gda_column_get_unique_key (fa)));
			rowlist = g_list_append (rowlist,
						 gda_value_new_string (gda_column_get_references (fa)));
			rowlist = g_list_append (rowlist, gda_value_new_string (NULL));

			list = g_list_append (list, rowlist);
		}

		g_list_foreach (list, add_g_list_row, recset);
		g_list_free (list);
	}

	if (selmodel)
		g_object_unref (selmodel);

	return GDA_DATA_MODEL (recset);
}

/*
 * Tables and views
 */
static GdaDataModel *
get_tables (GdaConnection *cnc, GdaParameterList *params, gboolean views)
{
	SQLITEcnc *scnc;
	GList *reclist;
	gchar *sql;
	GdaDataModel *model;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_error_string (cnc, _("Invalid SQLite handle"));
		return NULL;
	}

	sql = g_strdup_printf ("SELECT name as 'Table', 'system' as Owner, ' ' as Description, sql as Definition "
			       " FROM (SELECT * FROM sqlite_master UNION ALL "
			       "       SELECT * FROM sqlite_temp_master) "
			       " WHERE type = '%s' "
			       " ORDER BY name", views ? "view": "table");

	reclist = process_sql_commands (NULL, cnc, sql, 0);
	g_free (sql);

	if (!reclist)
		return NULL;

	model = GDA_DATA_MODEL (reclist->data);
	g_object_ref (G_OBJECT (model));

	g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
	g_list_free (reclist);

	return model;
}


/*
 * Data types
 */
static GdaDataModel *
get_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	SQLITEcnc *scnc;
	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_error_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}
	g_assert ((scnc->connection->flags & SQLITE_Initialized) || scnc->connection->init.busy);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (5));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Type"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 1, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 2, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 3, _("GDA type"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 4, _("Synonyms"));

	/* basic data types */
	add_type_row (recset, "integer", "system", "Signed integer, stored in 1, 2, 3, 4, 6, or 8 bytes depending on the magnitude of the value", GDA_VALUE_TYPE_INTEGER);
	add_type_row (recset, "real", "system", "Floating point value, stored as an 8-byte IEEE floating point number", GDA_VALUE_TYPE_DOUBLE);
	add_type_row (recset, "string", "system", "Text string, stored using the database encoding", GDA_VALUE_TYPE_STRING);
	add_type_row (recset, "blob", "system", "Blob of data, stored exactly as it was input", GDA_VALUE_TYPE_BLOB);


	/* scan the data types of all the columns of all the tables */
	if (SQLITE_VERSION_NUMBER >= 3000000) {
		GHashTable *names;
		Db *db;
		gint i;

		names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL); /* key= type name, value=1 */
		g_hash_table_insert (names, g_strdup ("integer"), GINT_TO_POINTER (1));
		g_hash_table_insert (names, g_strdup ("real"), GINT_TO_POINTER (1));
		g_hash_table_insert (names, g_strdup ("string"), GINT_TO_POINTER (1));
		g_hash_table_insert (names, g_strdup ("blob"), GINT_TO_POINTER (1));

		for (i = OMIT_TEMPDB; i < scnc->connection->nDb; i++) {
			Hash *tab_hash;
			HashElem *tab_elem;
			Table *table;

			db = &(scnc->connection->aDb[i]);
			tab_hash = &(db->tblHash);
			for (tab_elem = sqliteHashFirst (tab_hash); tab_elem ; tab_elem = sqliteHashNext (tab_elem)) {
				GList *rowlist = NULL;
				GdaValue *value;
				gint j;
				
				table = sqliteHashData (tab_elem);
				g_print ("table: %s\n", table->zName);
				for (j = 0; j < table->nCol; j++) {
					Column *column = &(table->aCol[j]);
					
					if (column->zType && !g_hash_table_lookup (names, column->zType)) {
						g_hash_table_insert (names, g_strdup (column->zType), GINT_TO_POINTER (1));
						GdaValueType type;
						switch (column->affinity) {
						case SQLITE_AFF_INTEGER:
							type = GDA_VALUE_TYPE_INTEGER;
							break;
						case SQLITE_AFF_NUMERIC:
							type = GDA_VALUE_TYPE_NUMERIC;
							break;
						case SQLITE_AFF_TEXT:
						case SQLITE_AFF_NONE:
						default:
							type = GDA_VALUE_TYPE_STRING;
							break;
						}
						add_type_row (recset, column->zType, "system", NULL, type);
					}
				}
			}
		}

		g_hash_table_destroy (names);
	}

	return GDA_DATA_MODEL (recset);
}



/*
 * Procedures and aggregates
 */
static GdaDataModelArray *
init_procs_recset (GdaConnection *cnc, gboolean aggs)
{
	GdaDataModelArray *recset;
	gint i;
	GdaSqliteColData cols_func[8] = {
		{ N_("Procedure")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Id")              , GDA_VALUE_TYPE_STRING  },
		{ N_("Owner")		, GDA_VALUE_TYPE_STRING  },
		{ N_("Comments")       	, GDA_VALUE_TYPE_STRING  },
		{ N_("Return type")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Nb args")	        , GDA_VALUE_TYPE_INTEGER },
		{ N_("Args types")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Definition")	, GDA_VALUE_TYPE_STRING  }
		};
	GdaSqliteColData cols_agg[7] = {
		{ N_("Procedure")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Id")              , GDA_VALUE_TYPE_STRING  },
		{ N_("Owner")		, GDA_VALUE_TYPE_STRING  },
		{ N_("Comments")       	, GDA_VALUE_TYPE_STRING  },
		{ N_("Return type")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Arg type")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Definition")	, GDA_VALUE_TYPE_STRING  }
		};
	
	if (aggs) {
		recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (sizeof cols_agg / sizeof cols_agg[0]));
		for (i = 0; i < sizeof cols_agg / sizeof cols_agg[0]; i++)
			gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i, _(cols_agg[i].col_name));
	}
	else {
		recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (sizeof cols_func / sizeof cols_func[0]));
		for (i = 0; i < sizeof cols_func / sizeof cols_func[0]; i++)
			gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i, _(cols_func[i].col_name));
	}

	return recset;
}

static GdaDataModel *
get_procs (GdaConnection *cnc, GdaParameterList *params, gboolean aggs)
{
	GdaDataModelArray *recset;
	SQLITEcnc *scnc;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_error_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}

	recset = init_procs_recset (cnc, aggs);
	if (SQLITE_VERSION_NUMBER >= 3000000) {
		Hash *func_hash;
		HashElem *func_elem;
		FuncDef *func;
		gint i = 0;
		gchar *str;
		gint nbargs;
		GList *list = NULL;
		gboolean is_agg;
		
		func_hash = &(scnc->connection->aFunc);
		for (func_elem = sqliteHashFirst (func_hash); func_elem ; func_elem = sqliteHashNext (func_elem)) {
			GList *rowlist = NULL;
			GdaValue *value;

			func = sqliteHashData (func_elem);
			is_agg = func->xFinalize ? TRUE : FALSE;
			if ((is_agg && !aggs) ||
			    (!is_agg && aggs))
				continue;

			/* Proc name */
			value = gda_value_new_string (func->zName);
			rowlist = g_list_append (rowlist, value);

			/* Proc_Id */
			if (! is_agg)
				str = g_strdup_printf ("p%d", i);
			else
				str = g_strdup_printf ("a%d", i);
			value = gda_value_new_string (str);
			g_free (str);
			rowlist = g_list_append (rowlist, value);

			/* Owner */
			value = gda_value_new_string ("system");
			rowlist = g_list_append (rowlist, value);

			/* Comments */ 
			value = gda_value_new_string ("");
			rowlist = g_list_append (rowlist, value);
			
			/* Out type */ 
			value = gda_value_new_string ("string");
			rowlist = g_list_append (rowlist, value);

			if (! is_agg) {
				/* Number of args */
				nbargs = func->nArg;
				value = gda_value_new_integer (nbargs);
				rowlist = g_list_append (rowlist, value);
			}
			
			/* In types */
			if (! is_agg) {
				if (nbargs > 0) {
					GString *string;
					gint j;
					
					string = g_string_new ("");
					for (j = 0; j < nbargs; j++) {
						if (j > 0)
							g_string_append_c (string, ',');
						g_string_append_c (string, '-');
					}
					value = gda_value_new_string (string->str);
					g_string_free (string, TRUE);
				}
				else
					value = gda_value_new_string ("");
			}
			else
				value = gda_value_new_string ("-");	
			rowlist = g_list_append (rowlist, value);

			/* Definition */
			value = gda_value_new_string ("");
			rowlist = g_list_append (rowlist, value);
			
			list = g_list_append (list, rowlist);
			i++;
		}

		g_list_foreach (list, add_g_list_row, recset);
		g_list_free (list);
	}

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
gda_sqlite_provider_get_schema (GdaServerProvider *provider,
				GdaConnection *cnc,
				GdaConnectionSchema schema,
				GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_FIELDS :
		return get_table_fields (cnc, params);
	case GDA_CONNECTION_SCHEMA_TABLES :
		return get_tables (cnc, params, FALSE);
	case GDA_CONNECTION_SCHEMA_VIEWS:
		return get_tables (cnc, params, TRUE);
	case GDA_CONNECTION_SCHEMA_TYPES :
		return get_types (cnc, params);
	case GDA_CONNECTION_SCHEMA_PROCEDURES:
		return get_procs (cnc, params, FALSE);
	case GDA_CONNECTION_SCHEMA_AGGREGATES:
		return get_procs (cnc, params, TRUE);
	default: ;
	}

	return NULL;
}
