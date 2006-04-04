/* GDA SQLite provider
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
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
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-parameter-list.h>
#include <libgda/gda-server-provider-extra.h>
#include "gda-sqlite.h"
#include "gda-sqlite-provider.h"
#include "gda-sqlite-recordset.h"
#include "sqliteInt.h"
#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-bin.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-type.h>

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
static gchar *gda_sqlite_provider_get_specs (GdaServerProvider *provider, GdaClientSpecsType type);
static gboolean gda_sqlite_provider_perform_action_params (GdaServerProvider *provider,
							   GdaParameterList *params, 
							   GdaClientSpecsType type, GError **error);
static gboolean gda_sqlite_provider_drop_database_cnc (GdaServerProvider *provider,
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

static GdaServerProviderInfo *gda_sqlite_provider_get_info (GdaServerProvider *provider,
							    GdaConnection *cnc);

static GdaDataModel *gda_sqlite_provider_get_schema (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaConnectionSchema schema,
						     GdaParameterList *params);

static GdaDataHandler *gda_sqlite_provider_get_data_handler (GdaServerProvider *provider,
							     GdaConnection *cnc,
							     GdaValueType gda_type,
							     const gchar *dbms_type);

static GObjectClass *parent_class = NULL;

typedef struct {
        gchar        *col_name;
        GdaValueType  data_type;
} GdaSqliteColData;

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
	provider_class->get_server_version = gda_sqlite_provider_get_server_version;
	provider_class->get_info = gda_sqlite_provider_get_info;
	provider_class->supports = gda_sqlite_provider_supports;
	provider_class->get_schema = gda_sqlite_provider_get_schema;

	provider_class->get_data_handler = gda_sqlite_provider_get_data_handler;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = NULL;

	provider_class->open_connection = gda_sqlite_provider_open_connection;
	provider_class->close_connection = gda_sqlite_provider_close_connection;
	provider_class->get_database = gda_sqlite_provider_get_database;
	provider_class->change_database = gda_sqlite_provider_change_database;

	provider_class->get_specs = gda_sqlite_provider_get_specs;
	provider_class->perform_action_params = gda_sqlite_provider_perform_action_params;

	provider_class->create_database_cnc = NULL;
	provider_class->drop_database_cnc = gda_sqlite_provider_drop_database_cnc;
	provider_class->create_table = NULL;
	provider_class->drop_table = NULL;
	provider_class->create_index = NULL;
	provider_class->drop_index = NULL;

	provider_class->execute_command = gda_sqlite_provider_execute_command;
	provider_class->get_last_insert_id = NULL;

	provider_class->begin_transaction = gda_sqlite_provider_begin_transaction;
	provider_class->commit_transaction = gda_sqlite_provider_commit_transaction;
	provider_class->rollback_transaction = gda_sqlite_provider_rollback_transaction;
	
	provider_class->create_blob = NULL;
	provider_class->fetch_blob = NULL;
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
		gda_connection_add_event_string (cnc,
						 _("A full path must be specified on the "
						   "connection string to open a database."));
		return FALSE;
	}

	scnc = g_new0 (SQLITEcnc, 1);

	errmsg = sqlite3_open (t_filename, &scnc->connection);
	scnc->file = g_strdup (t_filename);

	if (errmsg != SQLITE_OK) {
		printf("error %s", sqlite3_errmsg (scnc->connection));
		gda_connection_add_event_string (cnc, sqlite3_errmsg (scnc->connection));
		sqlite3_close (scnc->connection);
		g_free (scnc->file);
		g_free (scnc);
			
		return FALSE;
	}

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE, scnc);

	/* set SQLite library options */
	if (!gda_sqlite_provider_single_command (sqlite_prv, cnc, "PRAGMA empty_result_callbacks = ON"))
		gda_connection_add_event_string (cnc, _("Could not set empty_result_callbacks SQLite option"));

	/* make sure the internals are completely initialized now */
	{
		gchar **data = NULL;
		gint ncols;
		gint nrows;
		gint status;
		gchar *errmsg;
		
		status = sqlite3_get_table (scnc->connection, "SELECT name"
					    " FROM (SELECT * FROM sqlite_master UNION ALL "
					    "       SELECT * FROM sqlite_temp_master)",
					    &data,
					    &nrows,
					    &ncols,
					    &errmsg);
		if (status == SQLITE_OK) 
			sqlite3_free_table (data);
		else {
			g_print ("error: %s", errmsg);
			gda_connection_add_event_string (cnc, errmsg);
			sqlite3_free (errmsg);
			sqlite3_close (scnc->connection);
			g_free (scnc->file);
			g_free (scnc);

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
		gda_connection_add_event_string (cnc, _("Invalid SQLite handle"));
		return FALSE;
	}

	gda_sqlite_free_cnc (scnc);
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
		if (sqlite3_complete (tmp)) {
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
	gchar **arr;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = sql_split (sql);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			SQLITEresult *sres;
			GdaDataModel *recset;
			gint status;
			int changes;
			const char *left;

			sres = g_new0 (SQLITEresult, 1);
			changes = sqlite3_total_changes (scnc->connection);

			status = sqlite3_prepare (scnc->connection, arr [n], -1, &(sres->stmt), &left);
			/*g_print ("SQlite SQL: %s\n", arr [n]);*/
			if (options & GDA_COMMAND_OPTION_IGNORE_ERRORS ||
			    status == SQLITE_OK) {
				gchar *tststr;

				g_strchug (arr[n]);
				tststr = arr[n];
				if (! g_ascii_strncasecmp (tststr, "SELECT", 6) ||
				    ! g_ascii_strncasecmp (tststr, "PRAGMA", 6) ||
				    ! g_ascii_strncasecmp (tststr, "EXPLAIN", 7)) {
					recset = gda_sqlite_recordset_new (cnc, sres);
					g_object_set (G_OBJECT (recset), 
						      "command_text", arr[n],
						      "command_type", GDA_COMMAND_TYPE_SQL, NULL);
					reclist = g_list_append (reclist, recset);
				}
				else {
					int newchanges;
					GdaConnectionEvent *event;
					gchar *str, *tmp, *ptr;
					
					/* actually execute the command */
					status = sqlite3_step (sres->stmt);
					if (status != SQLITE_DONE) {
						GdaConnectionEvent *error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
						gda_connection_event_set_description (error, sqlite3_errmsg (scnc->connection));
						gda_connection_add_event (cnc, error);
						gda_sqlite_free_result (sres);
						break;
					}
					else {
						/* don't return a data model */
						reclist = g_list_append (reclist, NULL);
					}

					gda_sqlite_free_result (sres);

					/* generate a notice about changes */
					newchanges = sqlite3_total_changes (scnc->connection);
					event = gda_connection_event_new (GDA_CONNECTION_EVENT_NOTICE);
					ptr = tststr;
					while (*ptr && (*ptr != ' ') && (*ptr != '\t') &&
					       (*ptr != '\n'))
						ptr++;
					*ptr = 0;
					tmp = g_ascii_strup (tststr, -1);
					if (!strcmp (tmp, "DELETE"))
						str = g_strdup_printf ("%s %d (see SQLite documentation for a \"DELETE * FROM table\" query)", 
								       tmp, (newchanges - changes));
					else {
						if (!strcmp (tmp, "INSERT"))
							str = g_strdup_printf ("%s %lld %d", tmp, 
									       sqlite3_last_insert_rowid (scnc->connection),
									       (newchanges - changes));
						else
							str = g_strdup_printf ("%s %d", tmp, (newchanges - changes));
					}
					gda_connection_event_set_description (event, str);
					g_free (str);
					gda_connection_add_event (cnc, event);
				}
			} 
			else {
				GdaConnectionEvent *error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (error, sqlite3_errmsg (scnc->connection));
				gda_connection_add_event (cnc, error);
				gda_sqlite_free_result (sres);

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
		gda_connection_add_event_string (cnc, _("Invalid SQLite handle"));
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

	gda_connection_add_event_string (cnc, _("Only one database per connection is allowed"));
	return FALSE;
}

/* get_specs handler for the GdaSqliteProvider class */
static gchar *
gda_sqlite_provider_get_specs (GdaServerProvider *provider, GdaClientSpecsType type)
{
	gchar *specs, *file;
        gint len;
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);

	switch (type) {
	case GDA_CLIENT_SPECS_CREATE_DATABASE:
		file = g_build_filename (LIBGDA_DATA_DIR, "sqlite_specs_create_db.xml", NULL);
		if (g_file_get_contents (file, &specs, &len, NULL))
			return specs;
		else
			return NULL;
		break;
	default:
		return NULL;
	}
}

/* create_database handler for the GdaSqliteProvider class */
static gboolean
gda_sqlite_provider_perform_action_params (GdaServerProvider *provider,
					   GdaParameterList *params, 
					   GdaClientSpecsType type,
					   GError **error)
{
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;
	gboolean retval;
	GdaParameter *param = NULL;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	
	switch (type) {
	case GDA_CLIENT_SPECS_CREATE_DATABASE:
		if (params)
			param = gda_parameter_list_find_param (params, "URI");
		if (!param) {
			g_set_error (error, 0, 0,
				     _("Missing parameter 'URI'"));
			retval = FALSE;
		}
		else {
			SQLITEcnc *scnc;
			gint errmsg;
			gchar *filename;
			
			filename = (gchar *) gda_value_get_string ((GdaValue *) gda_parameter_get_value (param));
			
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
		break;
	default:
		g_set_error (error, 0, 0,
			     _("Method not handled by this provider"));
		return FALSE;
	}

	return retval;
}

static gboolean
gda_sqlite_provider_drop_database_cnc (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       const gchar *name)
{
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	gda_connection_add_event_string (cnc, _("To remove a SQLite database you should remove the database file by hand"));
	return FALSE;
}

/* execute_command handler for the GdaSqliteProvider class */
static GList *
gda_sqlite_provider_execute_command (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaCommand *cmd,
				     GdaParameterList *params)
{
	GList *reclist = NULL;
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
	const gchar *name;
	GdaSqliteProvider *sqlite_prv = (GdaSqliteProvider *) provider;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (sqlite_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
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
	const gchar *name;
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
	const gchar *name;
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
		gda_connection_add_event_string (cnc, _("Invalid SQLite handle"));
		return FALSE;
	}
	
	status = sqlite3_exec (scnc->connection, command, NULL, NULL, &errmsg);
	if (status == SQLITE_OK)
		result = TRUE;
	else {
		GdaConnectionEvent *error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
		gda_connection_event_set_description (error, errmsg);
		gda_connection_add_event (cnc, error);

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

static GdaServerProviderInfo *
gda_sqlite_provider_get_info (GdaServerProvider *provider, GdaConnection *cnc)
{
	static GdaServerProviderInfo info = {
		"SQLite",
		TRUE, 
		TRUE,
		TRUE,
	};
	
	return &info;
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

	gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

	g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
	g_list_free (value_list);
}

static void
add_g_list_row (gpointer data, GdaDataModelArray *recset)
{
	GList *rowlist = data;

	gda_data_model_append_values (GDA_DATA_MODEL (recset), rowlist, NULL);
	g_list_foreach (rowlist, (GFunc) gda_value_free, NULL);
	g_list_free (rowlist);
}

/*
 * Returns TRUE if there is a unique index on the @tblname.@field_name field
 */
static gboolean
sqlite_find_field_unique_index (GdaConnection *cnc, const gchar *tblname, const gchar *field_name)
{
	GdaDataModel *model = NULL;
	gchar *sql;
	gboolean retval = FALSE;
	GList *reclist;

	/* use the SQLite PRAGMA command to get the list of unique indexes for the table */
	sql = g_strdup_printf ("PRAGMA index_list('%s')", tblname);
	reclist = process_sql_commands (NULL, cnc, sql, 0);
	g_free (sql);
	if (reclist) 
		model = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	if (model) {
		int i, nrows;
		const GdaValue *value;
		const gchar *str;

		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; !retval && (i < nrows) ; i++) {
			value = gda_data_model_get_value_at (model, 2, i);
			
			str = gda_value_get_string ((GdaValue *) value);
			if (str && (*str == '1')) {
				const gchar *uidx_name;
				GdaDataModel *contents_model = NULL;

				value = gda_data_model_get_value_at (model, 1, i);
				uidx_name = gda_value_get_string ((GdaValue *) value);
				
				sql = g_strdup_printf ("PRAGMA index_info('%s')", uidx_name);
				reclist = process_sql_commands (NULL, cnc, sql, 0);
				g_free (sql);
				if (reclist) 
					contents_model = GDA_DATA_MODEL (reclist->data);
				g_list_free (reclist);
				if (contents_model) {
					if (gda_data_model_get_n_rows (contents_model) == 1) {
						const gchar *str;

						value = gda_data_model_get_value_at (contents_model, 2, 0);
						str = gda_value_get_string ((GdaValue *) value);
						if (!strcmp (str, field_name))
							retval = TRUE;
					}
					g_object_unref (contents_model);
				}
			}
		}
		g_object_unref (model);
	}

	return retval;
}

/*
 * finds the table.field which @tblname.@field_name references
 */
static gchar *
sqlite_find_field_reference (GdaConnection *cnc, const gchar *tblname, const gchar *field_name)
{
	GdaDataModel *model = NULL;
	gchar *sql;
	gchar *reference = NULL;
	GList *reclist;

	/* use the SQLite PRAGMA command to get the list of FK keys for the table */
	sql = g_strdup_printf ("PRAGMA foreign_key_list('%s')", tblname);
	reclist = process_sql_commands (NULL, cnc, sql, 0);
	g_free (sql);
	if (reclist) 
		model = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	if (model) {
		int i, nrows;
		const GdaValue *value;
		const gchar *str;

		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; !reference && (i < nrows) ; i++) {
			value = gda_data_model_get_value_at (model, 3, i);
			str = gda_value_get_string ((GdaValue *) value);
			if (!strcmp (str, field_name)) {
				const gchar *ref_table;

				value = gda_data_model_get_value_at (model, 2, i);
				ref_table = gda_value_get_string ((GdaValue *) value);
				value = gda_data_model_get_value_at (model, 4, i);
				str = gda_value_get_string ((GdaValue *) value);
				reference = g_strdup_printf ("%s.%s", ref_table, str);
			}
		}
		g_object_unref (model);
	}

	return reference;	
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
	GdaDataModel *selmodel = NULL, *pragmamodel = NULL;
	int i, nrows;
	GList *list = NULL;
	GdaColumn *fa = NULL;


	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}

	/* find table name */
	par = gda_parameter_list_find_param (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	tblname = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname != NULL, NULL);

	/* does the table exist? */
	/* FIXME */

	/* use the SQLite PRAGMA for table information command */
	sql = g_strdup_printf ("PRAGMA table_info('%s');", tblname);
	reclist = process_sql_commands (NULL, cnc, sql, 0);
	g_free (sql);
	if (reclist) 
		pragmamodel = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	if (!pragmamodel) {
		gda_connection_add_event_string (cnc, _("Can't execute PRAGMA table_info()"));
		return NULL;
	}

	/* run the "SELECT * from table" to fetch information */
	sql = g_strdup_printf ("SELECT * FROM %s LIMIT 1", tblname);
        reclist = process_sql_commands (NULL, cnc, sql, 0);
        g_free (sql);
	if (reclist) 
		selmodel = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	if (!selmodel) 
		return NULL;
	
	/* create the model */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_FIELDS)));
	g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_FIELDS));

	nrows = gda_data_model_get_n_rows (pragmamodel);
	for (i = 0; i < nrows; i++) {
		GList *rowlist = NULL;
		GdaValue *nvalue;
		GdaRow *row;
		const GdaValue *value;
		const gchar *field_name;
		gchar *str;
		gboolean is_pk, found;
			
		row = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (pragmamodel), i, NULL);
		g_assert (row);
		fa = gda_data_model_describe_column (selmodel, i);

		/* col name */
		value = gda_row_get_value (row, 1);
		nvalue = gda_value_copy ((GdaValue *) value);
		rowlist = g_list_append (rowlist, nvalue);
		field_name = gda_value_get_string (nvalue);

		/* data type */
		value = gda_row_get_value (row, 2);
		nvalue = gda_value_copy ((GdaValue *) value);
		rowlist = g_list_append (rowlist, nvalue);

		/* size */
		if (fa)
			nvalue = gda_value_new_integer (gda_column_get_defined_size (fa));
		else
			nvalue = gda_value_new_integer (-1);
		rowlist = g_list_append (rowlist, nvalue);

		/* scale */
		if (fa)
			nvalue = gda_value_new_integer (gda_column_get_scale (fa));
		else
			nvalue = gda_value_new_integer (-1);
		rowlist = g_list_append (rowlist, nvalue);

		/* not null */
		value = gda_row_get_value (row, 5);
		str = (gchar *) gda_value_get_string ((GdaValue *) value);
		is_pk = str && (*str == '1') ? TRUE : FALSE;
		if (is_pk)
			nvalue = gda_value_new_boolean (TRUE);
		else {
			value = gda_row_get_value (row, 3);
			str = (gchar *) gda_value_get_string ((GdaValue *) value);
			nvalue = gda_value_new_boolean (str && (*str == '0') ? FALSE : TRUE);
		}
		rowlist = g_list_append (rowlist, nvalue);

		/* PK */
		nvalue = gda_value_new_boolean (is_pk);
		rowlist = g_list_append (rowlist, nvalue);

		/* unique index */
		found = sqlite_find_field_unique_index (cnc, tblname, field_name);
		nvalue = gda_value_new_boolean (found);
		rowlist = g_list_append (rowlist, nvalue);

		/* FK */
		str = sqlite_find_field_reference (cnc, tblname, field_name);
		if (str && *str)
			nvalue = gda_value_new_string (str);
		else
			nvalue = gda_value_new_null ();
		g_free (str);
		rowlist = g_list_append (rowlist, nvalue);

		/* default value */
		value = gda_row_get_value (row, 4);
		if (value && !gda_value_is_null ((GdaValue *) value))
			str = gda_value_stringify ((GdaValue *) value);
		else
			str = NULL;
		if (str && *str)
			nvalue = gda_value_new_string (str);
		else
			nvalue = gda_value_new_null ();
		g_free (str);
		rowlist = g_list_append (rowlist, nvalue);

		/* extra attributes */
		nvalue = NULL;
		if (SQLITE_VERSION_NUMBER >= 3000000) {
			Table *table;
				
			table = sqlite3FindTable (scnc->connection, tblname, NULL); /* FIXME: database name as 3rd arg */
			if (!table) {
				gda_connection_add_event_string (cnc, _("Can't find table %s"), tblname);
				return GDA_DATA_MODEL (recset);
			}
				
			if ((table->iPKey == i) && table->autoInc)
				nvalue = gda_value_new_string ("AUTO_INCREMENT");
		}

		if (!nvalue)
			nvalue = gda_value_new_null ();
		rowlist = g_list_append (rowlist, nvalue);

		list = g_list_append (list, rowlist);
	}

	g_list_foreach (list, (GFunc) add_g_list_row, recset);
	g_list_free (list);
		

	g_object_unref (pragmamodel);
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
		gda_connection_add_event_string (cnc, _("Invalid SQLite handle"));
		return NULL;
	}

	sql = g_strdup_printf ("SELECT name as 'Table', 'system' as Owner, ' ' as Description, sql as Definition "
			       " FROM (SELECT * FROM sqlite_master UNION ALL "
			       "       SELECT * FROM sqlite_temp_master) "
			       " WHERE type = '%s' AND name not like 'sqlite_%%'"
			       " ORDER BY name", views ? "view": "table");

	reclist = process_sql_commands (NULL, cnc, sql, 0);
	g_free (sql);

	if (!reclist)
		return NULL;

	model = GDA_DATA_MODEL (reclist->data);
	g_object_ref (G_OBJECT (model));

	if (views) 
		g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (model), GDA_CONNECTION_SCHEMA_VIEWS));
	else 
		g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (model), GDA_CONNECTION_SCHEMA_TABLES));

	g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
	g_list_free (reclist);

	return model;
}


/*
 * Data types
 */
static void
get_types_foreach (gchar *key, gpointer value, GdaDataModelArray *recset)
{
	if (strcmp (key, "integer") &&
	    strcmp (key, "real") &&
	    strcmp (key, "string") && 
	    strcmp (key, "blob"))
		add_type_row (recset, key, "system", NULL, GPOINTER_TO_INT (value));
}

static GdaDataModel *
get_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	SQLITEcnc *scnc;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}
	g_assert (! scnc->connection->init.busy);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TYPES)));
	g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_TYPES));
	
	/* basic data types */
	add_type_row (recset, "integer", "system", "Signed integer, stored in 1, 2, 3, 4, 6, or 8 bytes depending on the magnitude of the value", GDA_VALUE_TYPE_INTEGER);
	add_type_row (recset, "real", "system", "Floating point value, stored as an 8-byte IEEE floating point number", GDA_VALUE_TYPE_DOUBLE);
	add_type_row (recset, "string", "system", "Text string, stored using the database encoding", GDA_VALUE_TYPE_STRING);
	add_type_row (recset, "blob", "system", "Blob of data, stored exactly as it was input", GDA_VALUE_TYPE_BLOB);


	/* scan the data types of all the columns of all the tables */
	gda_sqlite_update_types_hash (scnc);
	g_hash_table_foreach (scnc->types, (GHFunc) get_types_foreach, recset);

	return GDA_DATA_MODEL (recset);
}



/*
 * Procedures and aggregates
 */
static GdaDataModel *
get_procs (GdaConnection *cnc, GdaParameterList *params, gboolean aggs)
{
	GdaDataModelArray *recset;
	SQLITEcnc *scnc;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}

	if (aggs) {
		recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
					       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_AGGREGATES)));
		g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_AGGREGATES));
	}
	else {
		recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
					       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_PROCEDURES)));
		g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_PROCEDURES));
	}

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

		g_list_foreach (list, (GFunc) add_g_list_row, recset);
		g_list_free (list);
	}

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_constraints (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	SQLITEcnc *scnc;
	GdaParameter *par;
	const gchar *tblname;
	GList *reclist;
        gchar *sql;
	GdaDataModel *pragmamodel = NULL;
	int i, nrows;

	/* current constraint */
	gchar *cid = NULL;
	GString *cfields = NULL;
	GString *cref_fields = NULL;
	GdaRow *crow = NULL;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}

	/* find table name */
	par = gda_parameter_list_find_param (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	tblname = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname != NULL, NULL);

	/* does the table exist? */
	/* FIXME */

	/* create the model */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_CONSTRAINTS)));
	g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_CONSTRAINTS));

	/* 
	 * PRMARY KEY
	 * use the SQLite PRAGMA for table information command 
	 */
	sql = g_strdup_printf ("PRAGMA table_info ('%s');", tblname);
	reclist = process_sql_commands (NULL, cnc, sql, 0);
	g_free (sql);
	if (reclist) 
		pragmamodel = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	if (!pragmamodel) {
		gda_connection_add_event_string (cnc, _("Can't execute PRAGMA table_info()"));
		return NULL;
	}

	/* analysing the returned data model for the PRAGMA command */
	crow = NULL;
	cfields = NULL;
	nrows = gda_data_model_get_n_rows (pragmamodel);
	for (i = 0; i < nrows; i++) {
		GdaRow *row;
		GdaValue *value;
		gchar *str;
		row = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (pragmamodel), i, NULL);
		g_assert (row);

		value = gda_row_get_value (row, 5);
		str = (gchar *) gda_value_get_string ((GdaValue *) value);
		if (str && (*str == '1')) {
			if (!crow) {
				gint new_row_num;
				
				new_row_num = gda_data_model_append_row (GDA_DATA_MODEL (recset), NULL);
				crow = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (recset), new_row_num, NULL);
				
				value = gda_value_new_string ("");
				gda_row_set_value (crow, 0, value);
				gda_value_free (value);
				
				value = gda_value_new_string ("PRIMARY_KEY");
				gda_row_set_value (crow, 1, value);
				gda_value_free (value);
			}
			
			value = gda_row_get_value (row, 1);
			if (!cfields)
				cfields = g_string_new (gda_value_get_string (value));
			else {
				g_string_append_c (cfields, ',');
				g_string_append (cfields, gda_value_get_string (value));
			}
		}
	}

	if (crow) {
		GdaValue *value;

		value = gda_value_new_string (cfields->str);
		gda_row_set_value (crow, 2, value);
		gda_value_free (value);
		g_string_free (cfields, TRUE);
		
		value = gda_value_new_null ();
		gda_row_set_value (crow, 3, value);
		gda_value_free (value);

		value = gda_value_new_null ();
		gda_row_set_value (crow, 4, value);
		gda_value_free (value);
	}

	g_object_unref (pragmamodel);

	/* 
	 * FOREIGN KEYS
	 * use the SQLite PRAGMA for table information command 
	 */
	sql = g_strdup_printf ("PRAGMA foreign_key_list ('%s');", tblname);
	reclist = process_sql_commands (NULL, cnc, sql, 0);
	g_free (sql);
	if (reclist) 
		pragmamodel = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	if (!pragmamodel) {
		gda_connection_add_event_string (cnc, _("Can't execute PRAGMA foreign_key_list()"));
		return NULL;
	}

	/* analysing the returned data model for the PRAGMA command */
	cid = NULL;
	cfields = NULL;
	cref_fields = NULL;
	crow = NULL;
	nrows = gda_data_model_get_n_rows (pragmamodel);
	for (i = 0; i < nrows; i++) {
		GdaRow *row;
		GdaValue *value;
		gchar *str;
		row = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (pragmamodel), i, NULL);
		g_assert (row);

		value = gda_row_get_value (row, 0);
		str = (gchar *) gda_value_get_string (value);
		if (!cid || strcmp (cid, str)) {
			gint new_row_num;
			cid = str;

			/* record current constraint */
			if (crow) {
				value = gda_value_new_string (cfields->str);
				gda_row_set_value (crow, 2, value);
				gda_value_free (value);
				g_string_free (cfields, TRUE);

				g_string_append_c (cref_fields, ')');
				value = gda_value_new_string (cref_fields->str);
				gda_row_set_value (crow, 3, value);
				gda_value_free (value);
				g_string_free (cref_fields, TRUE);
			}

			/* start a new constraint */
			new_row_num = gda_data_model_append_row (GDA_DATA_MODEL (recset), NULL);
			crow = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (recset), new_row_num, NULL);
			
			value = gda_value_new_string ("");
			gda_row_set_value (crow, 0, value);
			gda_value_free (value);

			value = gda_value_new_string ("FOREIGN_KEY");
			gda_row_set_value (crow, 1, value);
			gda_value_free (value);

			
			value = gda_row_get_value (row, 3);
			cfields = g_string_new (gda_value_get_string (value));
			
			value = gda_row_get_value (row, 2);
			cref_fields = g_string_new (gda_value_get_string (value));
			g_string_append_c (cref_fields, '(');
			value = gda_row_get_value (row, 4);
			g_string_append (cref_fields, (gchar *) gda_value_get_string (value));

			value = gda_value_new_null ();
			gda_row_set_value (crow, 4, value);
			gda_value_free (value);
		}
		else {
			/* "augment" the current constraint */
			g_string_append_c (cfields, ',');
			value = gda_row_get_value (row, 3);
			g_string_append (cfields, (gchar *) gda_value_get_string (value));

			g_string_append_c (cref_fields, ',');
			value = gda_row_get_value (row, 4);
			g_string_append (cref_fields, (gchar *) gda_value_get_string (value));
		}
	       
	}

	if (crow) {
		GdaValue *value;

		value = gda_value_new_string (cfields->str);
		gda_row_set_value (crow, 2, value);
		gda_value_free (value);
		g_string_free (cfields, TRUE);
		
		g_string_append_c (cref_fields, ')');
		value = gda_value_new_string (cref_fields->str);
		gda_row_set_value (crow, 3, value);
		gda_value_free (value);
		g_string_free (cref_fields, TRUE);
	}

	g_object_unref (pragmamodel);

	return GDA_DATA_MODEL (recset);
}


static GdaDataModel *
gda_sqlite_provider_get_schema (GdaServerProvider *provider,
				GdaConnection *cnc,
				GdaConnectionSchema schema,
				GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
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
	case GDA_CONNECTION_SCHEMA_CONSTRAINTS:
		return get_constraints (cnc, params);
	default: ;
	}

	return NULL;
}

static GdaDataHandler *
gda_sqlite_provider_get_data_handler (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      GdaValueType gda_type,
				      const gchar *dbms_type)
{
	GdaDataHandler *dh = NULL;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	if (cnc) 
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	switch (gda_type) {
        case GDA_VALUE_TYPE_BIGINT:
	case GDA_VALUE_TYPE_BIGUINT:
	case GDA_VALUE_TYPE_DOUBLE:
	case GDA_VALUE_TYPE_INTEGER:
	case GDA_VALUE_TYPE_NUMERIC:
	case GDA_VALUE_TYPE_SINGLE:
	case GDA_VALUE_TYPE_SMALLINT:
	case GDA_VALUE_TYPE_SMALLUINT:
        case GDA_VALUE_TYPE_TINYINT:
        case GDA_VALUE_TYPE_TINYUINT:
        case GDA_VALUE_TYPE_UINTEGER:
		dh = gda_server_provider_handler_find (provider, NULL, gda_type, NULL);
		if (!dh) {
			dh = gda_handler_numerical_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_BIGINT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_BIGUINT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_DOUBLE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_INTEGER, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_NUMERIC, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_SINGLE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_SMALLINT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_SMALLUINT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_TINYINT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_TINYUINT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_UINTEGER, NULL);
			g_object_unref (dh);
		}
		break;
        case GDA_VALUE_TYPE_BINARY:
        case GDA_VALUE_TYPE_BLOB:
		dh = gda_server_provider_handler_find (provider, cnc, gda_type, NULL);
		if (!dh) {
			dh = gda_handler_bin_new_with_prov (provider, cnc);
			if (dh) {
				gda_server_provider_handler_declare (provider, dh, cnc, GDA_VALUE_TYPE_BINARY, NULL);
				gda_server_provider_handler_declare (provider, dh, cnc, GDA_VALUE_TYPE_BLOB, NULL);
				g_object_unref (dh);
			}
		}
		break;
        case GDA_VALUE_TYPE_BOOLEAN:
		dh = gda_server_provider_handler_find (provider, NULL, gda_type, NULL);
		if (!dh) {
			dh = gda_handler_boolean_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_BOOLEAN, NULL);
			g_object_unref (dh);
		}
 		break;
	case GDA_VALUE_TYPE_DATE:
	case GDA_VALUE_TYPE_TIME:
	case GDA_VALUE_TYPE_TIMESTAMP:
		dh = gda_server_provider_handler_find (provider, NULL, gda_type, NULL);
		if (!dh) {
			dh = gda_handler_time_new_no_locale ();
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_DATE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_TIME, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_TIMESTAMP, NULL);
			g_object_unref (dh);
		}
 		break;
	case GDA_VALUE_TYPE_STRING:
		dh = gda_server_provider_handler_find (provider, NULL, gda_type, NULL);
		if (!dh) {
			dh = gda_handler_string_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_STRING, NULL);
			g_object_unref (dh);
		}
 		break;
	case GDA_VALUE_TYPE_TYPE:
		dh = gda_server_provider_handler_find (provider, NULL, gda_type, NULL);
		if (!dh) {
			dh = gda_handler_type_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_VALUE_TYPE_TYPE, NULL);
			g_object_unref (dh);
		}
 		break;
	case GDA_VALUE_TYPE_NULL:
	case GDA_VALUE_TYPE_GEOMETRIC_POINT:
	case GDA_VALUE_TYPE_GOBJECT:
	case GDA_VALUE_TYPE_LIST:
	case GDA_VALUE_TYPE_MONEY:
	case GDA_VALUE_TYPE_UNKNOWN:
		/* special case: we take into account the dbms_type argument */
		if (!dbms_type)
			break;

		TO_IMPLEMENT;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return dh;	
}
