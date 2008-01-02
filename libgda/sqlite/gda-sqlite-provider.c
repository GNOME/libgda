/* GDA SQLite provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-parameter-list.h>
#include <libgda/gda-util.h>
#include <libgda/gda-server-provider-extra.h>
#include "gda-sqlite.h"
#include "gda-sqlite-provider.h"
#include "gda-sqlite-recordset.h"
#include "gda-sqlite-ddl.h"
#include <libgda/handlers/gda-handler-numerical.h>
#include "gda-sqlite-handler-bin.h"
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-type.h>
#include <libgda/gda-connection-private.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda/gda-renderer.h>
#include <libgda/gda-set.h>
#include <libgda/gda-statement-extra.h>
#include <sql-parser/gda-sql-parser.h>
#include <stdio.h>
#ifndef G_OS_WIN32
#define __USE_GNU
#include <dlfcn.h>
#endif

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER
#define FILE_EXTENSION ".db"
#ifdef HAVE_SQLITE
static SQLITEcnc *opening_scnc = NULL;
static GHashTable *db_connections_hash = NULL;
#endif

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

static gboolean gda_sqlite_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
                                                          GdaServerOperationType type, GdaParameterList *options);
static GdaServerOperation *gda_sqlite_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
                                                                   GdaServerOperationType type,
                                                                   GdaParameterList *options, GError **error);
static gchar *gda_sqlite_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
                                                      GdaServerOperation *op, GError **error);

static gboolean gda_sqlite_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
                                                      GdaServerOperation *op, GError **error);

static gchar *gda_sqlite_provider_get_last_insert_id (GdaServerProvider *provider,
                                                        GdaConnection *cnc,
                                                        GdaDataModel *recset);

static gboolean gda_sqlite_provider_begin_transaction (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       const gchar *name, GdaTransactionIsolation level,
						       GError **error);
static gboolean gda_sqlite_provider_commit_transaction (GdaServerProvider *provider,
							GdaConnection *cnc,
							const gchar *name, GError **error);
static gboolean gda_sqlite_provider_rollback_transaction (GdaServerProvider *provider,
							  GdaConnection * cnc,
							  const gchar *name, GError **error);

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
							     GType g_type,
							     const gchar *dbms_type);

static const gchar* gda_sqlite_provider_get_default_dbms_type (GdaServerProvider *provider,
							       GdaConnection *cnc,
							       GType type);
static GdaSqlParser *gda_sqlite_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc);
static gchar        *gda_sqlite_provider_statement_to_sql  (GdaServerProvider *provider, GdaConnection *cnc,
							    GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
							    GSList **params_used, GError **error);
static gboolean      gda_sqlite_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
							    GdaStatement *stmt, GError **error);
static GObject      *gda_sqlite_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
							    GdaStatement *stmt, GdaSet *params,
							    GdaStatementModelUsage model_usage, GError **error);

static void gda_sqlite_free_cnc    (SQLITEcnc *scnc);

static GObjectClass *parent_class = NULL;

typedef struct {
        gchar *col_name;
        GType  data_type;
} GdaSqliteColData;

/* extending SQLite with our own functions */
static void scalar_gda_file_exists_func (sqlite3_context *context, int argc, sqlite3_value **argv);
typedef struct {
	char     *name;
	int       nargs;
	gpointer  user_data; /* retreive it in func. implementations using sqlite3_user_data() */
	void    (*xFunc)(sqlite3_context*,int,sqlite3_value**);
} ScalarFunction;
ScalarFunction scalars[] = {
	{"gda_file_exists", 1, NULL, scalar_gda_file_exists_func},
};


/*
 * Prepared statements
 */
static GdaSqlParser *sqlite_parser = NULL;

typedef enum {
	INTERNAL_PRAGMA_INDEX_LIST,
	INTERNAL_PRAGMA_INDEX_INFO,
	INTERNAL_PRAGMA_FK_LIST,
	INTERNAL_PRAGMA_TABLE_INFO,
	INTERNAL_SELECT_A_TABLE_ROW,
	INTERNAL_SELECT_ALL_TABLES,
	INTERNAL_SELECT_A_TABLE,
	INTERNAL_PRAGMA_PROC_LIST,
	INTERNAL_PRAGMA_EMPTY_RESULT,
	INTERNAL_BEGIN,
	INTERNAL_BEGIN_NAMED,
	INTERNAL_COMMIT,
	INTERNAL_COMMIT_NAMED,
	INTERNAL_ROLLBACK,
	INTERNAL_ROLLBACK_NAMED,
} InternalStatementItem;

gchar *internal_sql[] = {
	"PRAGMA index_list(##tblname::string)",
	"PRAGMA index_info(##idxname::string)",
	"PRAGMA foreign_key_list (##tblname::string)",
	"PRAGMA table_info (##tblname::string)",
	"SELECT * FROM ##tblname::string LIMIT 1",
	"SELECT name as 'Table', 'system' as 'Owner', ' ' as 'Description', sql as 'Definition' FROM (SELECT * FROM sqlite_master UNION ALL SELECT * FROM sqlite_temp_master) WHERE type = ##type::string AND name not like 'sqlite_%%' ORDER BY name",
	"SELECT name as 'Table', 'system' as 'Owner', ' ' as 'Description', sql as 'Definition' FROM (SELECT * FROM sqlite_master UNION ALL SELECT * FROM sqlite_temp_master) WHERE type = ##type::string AND name = ##tblname::string AND name not like 'sqlite_%%' ORDER BY name",
	"PRAGMA proc_list",
	"PRAGMA empty_result_callbacks = ON",
	"BEGIN TRANSACTION",
	"BEGIN TRANSACTION ##name::string",
	"COMMIT TRANSACTION",
	"COMMIT TRANSACTION ##name::string",
	"ROLLBACK TRANSACTION",
	"ROLLBACK TRANSACTION ##name::string"
};

GdaStatement *internal_stmt[] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};


void
_gda_sqlite_prepared_statement_free (SQLitePreparedStatement *ps)
{
	ps->ref_count --;
	if (ps->ref_count > 0)
		return;

	sqlite3_finalize (ps->sqlite_stmt);
	if (ps->param_ids) {
		g_slist_foreach (ps->param_ids, (GFunc) g_free, NULL);
		g_slist_free (ps->param_ids);
	}
	g_free (ps->sql);

	if (ps->types)
                g_free (ps->types);
        if (ps->cols_size)
                g_free (ps->cols_size);
	g_free (ps);
}

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
	provider_class->supports_feature = gda_sqlite_provider_supports;
	provider_class->get_schema = gda_sqlite_provider_get_schema;

	provider_class->get_data_handler = gda_sqlite_provider_get_data_handler;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = gda_sqlite_provider_get_default_dbms_type;

	provider_class->create_connection = NULL;
	provider_class->open_connection = gda_sqlite_provider_open_connection;
	provider_class->close_connection = gda_sqlite_provider_close_connection;
	provider_class->get_database = gda_sqlite_provider_get_database;
	provider_class->change_database = gda_sqlite_provider_change_database;

	provider_class->supports_operation = gda_sqlite_provider_supports_operation;
        provider_class->create_operation = gda_sqlite_provider_create_operation;
        provider_class->render_operation = gda_sqlite_provider_render_operation;
        provider_class->perform_operation = gda_sqlite_provider_perform_operation;

	provider_class->execute_command = NULL;
	provider_class->execute_query = NULL;
	provider_class->get_last_insert_id = gda_sqlite_provider_get_last_insert_id;

	provider_class->begin_transaction = gda_sqlite_provider_begin_transaction;
	provider_class->commit_transaction = gda_sqlite_provider_commit_transaction;
	provider_class->rollback_transaction = gda_sqlite_provider_rollback_transaction;
	provider_class->add_savepoint = NULL;
	provider_class->rollback_savepoint = NULL;
	provider_class->delete_savepoint = NULL;

	provider_class->create_parser = gda_sqlite_provider_create_parser;
	provider_class->statement_to_sql = gda_sqlite_provider_statement_to_sql;
	provider_class->statement_prepare = gda_sqlite_provider_statement_prepare;
	provider_class->statement_execute = gda_sqlite_provider_statement_execute;
}

static void
gda_sqlite_provider_init (GdaSqliteProvider *sqlite_prv, GdaSqliteProviderClass *klass)
{
	InternalStatementItem i;
	sqlite_parser = gda_sqlite_provider_create_parser ((GdaServerProvider*) sqlite_prv, NULL);
	for (i = INTERNAL_PRAGMA_INDEX_LIST; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
		internal_stmt[i] = gda_sql_parser_parse_string (sqlite_parser, internal_sql[i], NULL, NULL);
		if (!internal_stmt[i]) 
			g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
	}
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

	if (G_UNLIKELY (type == 0)) {
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

static void
add_g_list_row (gpointer data, GdaDataModelArray *recset)
{
	GList *rowlist = data;
	GError *error = NULL;
	if (gda_data_model_append_values (GDA_DATA_MODEL (recset), rowlist, &error) < 0) {
		g_warning ("Data model append error: %s\n", error && error->message ? error->message : "no detail");
		g_error_free (error);
	}
	g_list_foreach (rowlist, (GFunc) gda_value_free, NULL);
	g_list_free (rowlist);
}

#ifndef G_OS_WIN32
#ifdef HAVE_SQLITE
int sqlite3CreateFunc (sqlite3 *db, const char *name, int nArg, int eTextRep, void *data,
		       void (*xFunc)(sqlite3_context*,int,sqlite3_value **),
		       void (*xStep)(sqlite3_context*,int,sqlite3_value **), void (*xFinal)(sqlite3_context*))
{
	SQLITEcnc *scnc = NULL;
	gboolean is_func = FALSE;
	gboolean is_agg = FALSE;
	GdaDataModelArray *recset = NULL;

	static int (*func) (sqlite3 *, const char *, int, int, void *,
			    void (*)(sqlite3_context*,int,sqlite3_value **),
			    void (*)(sqlite3_context*,int,sqlite3_value **), void (*)(sqlite3_context*)) = NULL;

	if(!func)
		func = (int (*) (sqlite3 *, const char *, int, int, void *,
				 void (*)(sqlite3_context*,int,sqlite3_value **),
				 void (*)(sqlite3_context*,int,sqlite3_value **), 
				 void (*)(sqlite3_context*))) dlsym (RTLD_NEXT, "sqlite3CreateFunc");

	/* try to find which SQLITEcnc this concerns */
	if (db_connections_hash) 
		scnc = g_hash_table_lookup (db_connections_hash, db);
	if (!scnc)
		scnc = opening_scnc;
	if (!scnc)
		return func (db, name, nArg, eTextRep, data, xFunc, xStep, xFinal);

	if (xFunc) {
		/* It's a function */
		recset = (GdaDataModelArray *) scnc->functions_model;
		if (!recset) {
			recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
						       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_PROCEDURES)));
			g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), 
									 GDA_CONNECTION_SCHEMA_PROCEDURES));
			scnc->functions_model = (GdaDataModel *) recset;
		}
		is_func = TRUE;
	}
	else if (xStep && xFinal) {
		/* It's an aggregate */
		recset = (GdaDataModelArray *) scnc->aggregates_model;
		if (!recset) {
			recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
						       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_AGGREGATES)));
			g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), 
									 GDA_CONNECTION_SCHEMA_AGGREGATES));
			scnc->aggregates_model = (GdaDataModel *) recset;
		}
		is_agg = TRUE;
	}
	else if (!xFunc && !xStep && !xFinal) {
		/* remove function or aggregate definition */
		GSList *values;
		GValue *value;
		gint cols_index [] = {0};
		gint row;

		g_value_set_string (value = gda_value_new (G_TYPE_STRING), name);
		values = g_slist_prepend (NULL, value);
		
		if (scnc->functions_model) {
			row = gda_data_model_get_row_from_values (scnc->functions_model, values, cols_index);
			if (row >= 0) {
				g_object_set (G_OBJECT (scnc->functions_model), "read-only", FALSE, NULL);
				g_assert (gda_data_model_remove_row (scnc->functions_model, row, NULL));
				g_object_set (G_OBJECT (scnc->functions_model), "read-only", TRUE, NULL);
			}
		}
		if (scnc->aggregates_model) {
			row = gda_data_model_get_row_from_values (scnc->aggregates_model, values, cols_index);
			if (row >= 0) {
				g_object_set (G_OBJECT (scnc->aggregates_model), "read-only", FALSE, NULL);
				g_assert (gda_data_model_remove_row (scnc->aggregates_model, row, NULL));
				g_object_set (G_OBJECT (scnc->aggregates_model), "read-only", TRUE, NULL);
			}
		}

		gda_value_free (value);
		g_slist_free (values);
	}

	if (is_func || is_agg) {
		gchar *str;
		GValue *value;
		GList *rowlist = NULL;
		
		/* 'unlock' the data model */
		g_object_set (G_OBJECT (recset), "read-only", FALSE, NULL);

		/* Proc name */
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), name);
		rowlist = g_list_append (rowlist, value);
				
		/* Proc_Id */
		if (! is_agg)
			str = g_strdup_printf ("p%d", gda_data_model_get_n_rows ((GdaDataModel*) recset));
		else
			str = g_strdup_printf ("a%d", gda_data_model_get_n_rows ((GdaDataModel*) recset));
		g_value_take_string (value = gda_value_new (G_TYPE_STRING), str);
		rowlist = g_list_append (rowlist, value);
				
		/* Owner */
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), "system");
		rowlist = g_list_append (rowlist, value);
		
		/* Comments */ 
		rowlist = g_list_append (rowlist, gda_value_new_null());
		
		/* Out type */ 
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), "text");
		rowlist = g_list_append (rowlist, value);
				
		if (! is_agg) {
			/* Number of args */
			g_value_set_int (value = gda_value_new (G_TYPE_INT), nArg);
			rowlist = g_list_append (rowlist, value);
		}
				
		/* In types */
		if (! is_agg) {
			if (nArg > 0) {
				GString *string;
				gint j;
				
				string = g_string_new ("");
				for (j = 0; j < nArg; j++) {
					if (j > 0)
						g_string_append_c (string, ',');
					g_string_append_c (string, '-');
				}
				g_value_take_string (value = gda_value_new (G_TYPE_STRING), string->str);
				g_string_free (string, FALSE);
			}
			else
				g_value_set_string (value = gda_value_new (G_TYPE_STRING), "");
		}
		else
			g_value_set_string (value = gda_value_new (G_TYPE_STRING), "");
		rowlist = g_list_append (rowlist, value);

		/* Definition */
		rowlist = g_list_append (rowlist, gda_value_new_null());

		add_g_list_row ((gpointer) rowlist, recset);

		/* 'lock' the data model */
		g_object_set (G_OBJECT (recset), "read-only", TRUE, NULL);

	}

	return func (db, name, nArg, eTextRep, data, xFunc, xStep, xFinal);
}
#endif
#endif

/* open_connection handler for the GdaSqliteProvider class */
static gboolean
gda_sqlite_provider_open_connection (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaQuarkList *params,
				     const gchar *username,
				     const gchar *password)
{
	gchar *filename = NULL;
	const gchar *dirname = NULL, *dbname = NULL;
	const gchar *is_virtual = NULL;
	const gchar *use_extra_functions = NULL;
	gint errmsg;
	SQLITEcnc *scnc;
	gchar *dup = NULL;

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* get all parameters received */
	dirname = gda_quark_list_find (params, "DB_DIR");
	dbname = gda_quark_list_find (params, "DB_NAME");
	is_virtual = gda_quark_list_find (params, "_IS_VIRTUAL");
	use_extra_functions = gda_quark_list_find (params, "LOAD_GDA_FUNCTIONS");

	if (! is_virtual) {
		if (!dirname || !dbname) {
			const gchar *str;
			
			str = gda_quark_list_find (params, "URI");
			if (!str) {
				gda_connection_add_event_string (cnc,
								 _("The connection string must contain DB_DIR and DB_NAME values"));
				return FALSE;
			}
			else {
				gint len = strlen (str);
				gint elen = strlen (FILE_EXTENSION);
				
				if (g_str_has_suffix (str, FILE_EXTENSION)) {
					gchar *ptr;
					
					dup = strdup (str);
					dup [len-elen] = 0;
					for (ptr = dup + (len - elen - 1); (ptr >= dup) && (*ptr != G_DIR_SEPARATOR); ptr--);
					dbname = ptr;
					if (*ptr == G_DIR_SEPARATOR)
						dbname ++;
					
					if ((*ptr == G_DIR_SEPARATOR) && (ptr > dup)) {
						dirname = dup;
						while ((ptr >= dup) && (*ptr != G_DIR_SEPARATOR))
							ptr--;
						*ptr = 0;
					}
				}
				if (!dbname || !dirname) {
					gda_connection_add_event_string (cnc,
									 _("The connection string format has changed: replace URI with "
									   "DB_DIR (the path to the database file) and DB_NAME "
									   "(the database file without the '%s' at the end)."), FILE_EXTENSION);
					g_free (dup);
					return FALSE;
				}
				else
					g_warning (_("The connection string format has changed: replace URI with "
						     "DB_DIR (the path to the database file) and DB_NAME "
						     "(the database file without the '%s' at the end)."), FILE_EXTENSION);
			}
		}	
		
		if (!g_file_test (dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			gda_connection_add_event_string (cnc,
							 _("The DB_DIR part of the connection string must point "
							   "to a valid directory"));
			g_free (dup);
			return FALSE;
		}

		/* try first without the file extension */
		filename = g_build_filename (dirname, dbname, NULL);
		if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
			gchar *tmp;
			g_free (filename);
			tmp = g_strdup_printf ("%s%s", dbname, FILE_EXTENSION);
			filename = g_build_filename (dirname, tmp, NULL);
			g_free (tmp);
			
		}
		g_free (dup);
	}

	scnc = g_new0 (SQLITEcnc, 1);
#ifdef HAVE_SQLITE
	opening_scnc = scnc;
#endif
	errmsg = sqlite3_open (filename, &scnc->connection);
	if (filename)
		scnc->file = g_strdup (filename);

	if (errmsg != SQLITE_OK) {
		printf("error %s", sqlite3_errmsg (scnc->connection));
		gda_connection_add_event_string (cnc, sqlite3_errmsg (scnc->connection));
		gda_sqlite_free_cnc (scnc);
			
#ifdef HAVE_SQLITE
		opening_scnc = NULL;
#endif
		return FALSE;
	}
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE, scnc);

	/* use extended result codes */
	sqlite3_extended_result_codes (scnc->connection, 1);
	
	/* allow a busy timeout of 500 ms */
	sqlite3_busy_timeout (scnc->connection, 500);

	/* initialize prepared statement handling */
	gda_connection_init_prepared_statement_hash (cnc, (GDestroyNotify) _gda_sqlite_prepared_statement_free);

	/* try to prepare all the internal statements */
	InternalStatementItem i;
	for (i = INTERNAL_PRAGMA_INDEX_LIST; i < sizeof (internal_sql) / sizeof (gchar*); i++) 
		gda_connection_statement_prepare (cnc, internal_stmt[i], NULL);

	/* set SQLite library options */
	GObject *obj;
	obj = gda_connection_statement_execute (cnc, internal_stmt[INTERNAL_PRAGMA_EMPTY_RESULT],
						NULL, GDA_STATEMENT_MODEL_CURSOR_FORWARD, NULL);
	if (!obj)
		gda_connection_add_event_string (cnc, _("Could not set empty_result_callbacks SQLite option"));
	g_object_unref (obj);


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
			gda_connection_add_event_string (cnc, errmsg);
			sqlite3_free (errmsg);
			gda_sqlite_free_cnc (scnc);
			g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE, NULL);
#ifdef HAVE_SQLITE
			opening_scnc = NULL;
#endif
			return FALSE;
		}
	}

	if (use_extra_functions && ((*use_extra_functions == 't') || (*use_extra_functions == 'T'))) {
		int i;

		for (i = 0; i < sizeof (scalars) / sizeof (ScalarFunction); i++) {
			ScalarFunction *func = (ScalarFunction *) &(scalars [i]);
			gint res = sqlite3_create_function (scnc->connection, 
							    func->name, func->nargs, SQLITE_UTF8, func->user_data, 
							    func->xFunc, NULL, NULL);
			if (res != SQLITE_OK) {
				gda_sqlite_free_cnc (scnc);
				g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE, NULL);
#ifdef HAVE_SQLITE
				opening_scnc = NULL;
#endif
				return FALSE;
			}
		}
	}

#ifdef HAVE_SQLITE
	/* add the (scnc->connection, scnc) to db_connections_hash */
	if (!db_connections_hash)
		db_connections_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	g_hash_table_insert (db_connections_hash, scnc->connection, scnc);
#endif

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

static gboolean
gda_sqlite_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
                                       GdaServerOperationType type, GdaParameterList *options)
{
        switch (type) {
        case GDA_SERVER_OPERATION_CREATE_DB:
        case GDA_SERVER_OPERATION_DROP_DB:

        case GDA_SERVER_OPERATION_CREATE_TABLE:
        case GDA_SERVER_OPERATION_DROP_TABLE:
        case GDA_SERVER_OPERATION_RENAME_TABLE:

        case GDA_SERVER_OPERATION_ADD_COLUMN:

        case GDA_SERVER_OPERATION_CREATE_INDEX:
        case GDA_SERVER_OPERATION_DROP_INDEX:
                return TRUE;
        default:
                return FALSE;
        }
}

static GdaServerOperation *
gda_sqlite_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
                                     GdaServerOperationType type,
                                     GdaParameterList *options, GError **error)
{
        gchar *file;
        GdaServerOperation *op;
        gchar *str;
	gchar *dir;

        file = g_utf8_strdown (gda_server_operation_op_type_to_string (type), -1);
        str = g_strdup_printf ("sqlite_specs_%s.xml", file);
        g_free (file);

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        file = gda_server_provider_find_file (provider, dir, str);
	g_free (dir);
        g_free (str);

        if (! file) {
                g_set_error (error, 0, 0, _("Missing spec. file '%s'"), file);
                return NULL;
        }

        op = gda_server_operation_new (type, file);
        g_free (file);

        return op;
}

static gchar *
gda_sqlite_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
                                     GdaServerOperation *op, GError **error)
{
        gchar *sql = NULL;
        gchar *file;
        gchar *str;
	gchar *dir;

        file = g_utf8_strdown (gda_server_operation_op_type_to_string (gda_server_operation_get_op_type (op)), -1);
        str = g_strdup_printf ("sqlite_specs_%s.xml", file);
        g_free (file);

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        file = gda_server_provider_find_file (provider, dir, str);
	g_free (dir);
        g_free (str);

        if (! file) {
                g_set_error (error, 0, 0, _("Missing spec. file '%s'"), file);
                return NULL;
        }
        if (!gda_server_operation_is_valid (op, file, error)) {
                g_free (file);
                return NULL;
        }
        g_free (file);

        switch (gda_server_operation_get_op_type (op)) {
        case GDA_SERVER_OPERATION_CREATE_DB:
        case GDA_SERVER_OPERATION_DROP_DB:
		sql = NULL;
                break;
        case GDA_SERVER_OPERATION_CREATE_TABLE:
                sql = gda_sqlite_render_CREATE_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_TABLE:
                sql = gda_sqlite_render_DROP_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_RENAME_TABLE:
                sql = gda_sqlite_render_RENAME_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_ADD_COLUMN:
                sql = gda_sqlite_render_ADD_COLUMN (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_CREATE_INDEX:
                sql = gda_sqlite_render_CREATE_INDEX (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_INDEX:
                sql = gda_sqlite_render_DROP_INDEX (provider, cnc, op, error);
                break;
        default:
                g_assert_not_reached ();
        }
        return sql;
}

static gboolean
gda_sqlite_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
                                      GdaServerOperation *op, GError **error)
{
        GdaServerOperationType optype;

        optype = gda_server_operation_get_op_type (op);
	switch (optype) {
	case GDA_SERVER_OPERATION_CREATE_DB: {
		const GValue *value;
		const gchar *dbname = NULL;
		const gchar *dir = NULL;
		SQLITEcnc *scnc;
		gint errmsg;
		gchar *filename, *tmp;
		gboolean retval = TRUE;
		
		value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_NAME");
                if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
                        dbname = g_value_get_string (value);
		value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_DIR");
                if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
                        dir = g_value_get_string (value);
  	 
		tmp = g_strdup_printf ("%s%s", dbname, FILE_EXTENSION);
		filename = g_build_filename (dir, tmp, NULL);
		g_free (tmp);

		scnc = g_new0 (SQLITEcnc, 1);
		errmsg = sqlite3_open (filename, &scnc->connection);
		g_free (filename);

		if (errmsg != SQLITE_OK) {
			g_set_error (error, 0, 0, sqlite3_errmsg (scnc->connection)); 
			retval = FALSE;
		}
		gda_sqlite_free_cnc (scnc);

		return retval;
        }
	case GDA_SERVER_OPERATION_DROP_DB: {
		const GValue *value;
		const gchar *dbname = NULL;
		const gchar *dir = NULL;
		gchar *filename, *tmp;
		gboolean retval = TRUE;

		value = gda_server_operation_get_value_at (op, "/DB_DESC_P/DB_NAME");
                if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
                        dbname = g_value_get_string (value);
		value = gda_server_operation_get_value_at (op, "/DB_DESC_P/DB_DIR");
                if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
                        dir = g_value_get_string (value);

		tmp = g_strdup_printf ("%s%s", dbname, FILE_EXTENSION);
		filename = (gchar *) g_build_filename (dir, tmp, NULL);
		g_free (tmp);

		if (g_unlink (filename)) {
			g_set_error (error, 0, 0,
				     sys_errlist [errno]);
			retval = FALSE;
		}
		g_free (filename);
		
		return retval;
	}
        default: {
                /* use the SQL from the provider to perform the action */
                gchar *sql;
		GdaStatement *stmt;
		GdaSqlParser *parser;
		gboolean retval = FALSE;
		
		parser = g_object_get_data (G_OBJECT (provider), "_gda_parser");
		if (!parser) {
			parser = gda_server_provider_create_parser (provider, NULL);
			g_assert (parser);
			g_object_set_data_full (G_OBJECT (provider), "_gda_parser", parser, g_object_unref);
		}

                sql = gda_server_provider_render_operation (provider, cnc, op, error);
                if (!sql)
                        return FALSE;
		stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
		if (stmt) {
			if (gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL) != -1) 
				retval = TRUE;
			g_object_unref (stmt);
		}
		if (!retval) 
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_OPERATION_ERROR,
				     _("Internal error with generated SQL: %s"), sql);
		g_free (sql);
		return retval;
	}
	}
}

static gchar *
gda_sqlite_provider_get_last_insert_id (GdaServerProvider *provider,
					GdaConnection *cnc,
					GdaDataModel *recset)
{
	SQLITEcnc *scnc;
	long long int rowid;

        g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (provider), NULL);
        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

        scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
        if (!scnc) {
                gda_connection_add_event_string (cnc, _("Invalid SQLite handle"));
                return NULL;
        }

 	if (recset) {
		g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), NULL);
		TO_IMPLEMENT;
		return NULL;
	}
	rowid = sqlite3_last_insert_rowid (scnc->connection);
	if (rowid != 0)
		return g_strdup_printf ("%lld", rowid);
	else
		return NULL;
}


static gboolean
gda_sqlite_provider_begin_transaction (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       const gchar *name, GdaTransactionIsolation level,
				       GError **error)
{
	gboolean status = TRUE;
	static GdaSet *params_set = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	if (name) {
		if (!params_set)
			params_set = gda_set_new_inline (1, "name", G_TYPE_STRING, name);
		else 
			gda_set_set_holder_value (params_set, "name", name);
		if (gda_connection_statement_execute_non_select (cnc, internal_stmt[INTERNAL_BEGIN_NAMED], 
								 params_set, NULL) == -1) 
		status = FALSE;
	}
	else {
		if (gda_connection_statement_execute_non_select (cnc, internal_stmt[INTERNAL_BEGIN], 
								 NULL, NULL) == -1) 
			status = FALSE;
	}

	return status;
}

static gboolean
gda_sqlite_provider_commit_transaction (GdaServerProvider *provider,
					GdaConnection *cnc,
					const gchar *name, GError **error)
{
	gboolean status = TRUE;
	static GdaSet *params_set = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	if (name) {
		if (!params_set)
			params_set = gda_set_new_inline (1, "name", G_TYPE_STRING, name);
		else 
			gda_set_set_holder_value (params_set, "name", name);
		if (gda_connection_statement_execute_non_select (cnc, internal_stmt[INTERNAL_COMMIT_NAMED], 
								 params_set, NULL) == -1) 
		status = FALSE;
	}
	else {
		if (gda_connection_statement_execute_non_select (cnc, internal_stmt[INTERNAL_COMMIT], 
								 NULL, NULL) == -1) 
			status = FALSE;
	}

	return status;
}

static gboolean
gda_sqlite_provider_rollback_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  const gchar *name, GError **error)
{
	gboolean status = TRUE;
	static GdaSet *params_set = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	if (name) {
		if (!params_set)
			params_set = gda_set_new_inline (1, "name", G_TYPE_STRING, name);
		else 
			gda_set_set_holder_value (params_set, "name", name);
		if (gda_connection_statement_execute_non_select (cnc, internal_stmt[INTERNAL_ROLLBACK_NAMED], 
								 params_set, NULL) == -1) 
		status = FALSE;
	}
	else {
		if (gda_connection_statement_execute_non_select (cnc, internal_stmt[INTERNAL_ROLLBACK], 
								 NULL, NULL) == -1) 
			status = FALSE;
	}

	return status;
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
	case GDA_CONNECTION_FEATURE_PROCEDURES :
		return TRUE;
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
		TRUE,
		TRUE,
		TRUE
	};
	
	return &info;
}


static void
add_type_row (GdaDataModelArray *recset, const gchar *name,
	      const gchar *owner, const gchar *comments,
	      GType type)
{
	GList *value_list;
	GValue *tmpval;

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), name);
	value_list = g_list_append (NULL, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), owner);
	value_list = g_list_append (value_list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), comments);
	value_list = g_list_append (value_list, tmpval);

	g_value_set_ulong (tmpval = gda_value_new (G_TYPE_ULONG), type);
	value_list = g_list_append (value_list, tmpval);

	value_list = g_list_append (value_list, gda_value_new_null ());

	gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

	g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
	g_list_free (value_list);
}

/*
 * Returns TRUE if there is a unique index on the @tblname.@field_name field
 */
static gboolean
sqlite_find_field_unique_index (GdaConnection *cnc, const gchar *tblname, const gchar *field_name)
{
	GdaDataModel *model = NULL;
	gboolean retval = FALSE;
	static GdaSet *params_set = NULL;

	/* use the SQLite PRAGMA command to get the list of unique indexes for the table */

	if (!params_set)
		params_set = gda_set_new_inline (2, "tblname", G_TYPE_STRING, tblname,
					     "idxname", G_TYPE_STRING, NULL);
	else 
		gda_set_set_holder_value (params_set, "tblname", tblname);
	model = (GdaDataModel *) gda_connection_statement_execute (cnc, internal_stmt[INTERNAL_PRAGMA_INDEX_LIST], params_set, 
								   GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL);

	if (model) {
		int i, nrows;
		const GValue *value;

		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; !retval && (i < nrows) ; i++) {
			value = gda_data_model_get_value_at (model, 2, i);
			
			if (g_value_get_int ((GValue *) value)) {
				const gchar *uidx_name;
				GdaDataModel *contents_model = NULL;

				value = gda_data_model_get_value_at (model, 1, i);
				uidx_name = g_value_get_string ((GValue *) value);
				gda_set_set_holder_value (params_set, "idxname", uidx_name);

				contents_model = (GdaDataModel*) gda_connection_statement_execute (cnc, 
								   internal_stmt[INTERNAL_PRAGMA_INDEX_INFO], params_set, 
								   GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL);
				if (contents_model) {
					if (gda_data_model_get_n_rows (contents_model) == 1) {
						const gchar *str;

						value = gda_data_model_get_value_at (contents_model, 2, 0);
						str = g_value_get_string ((GValue *) value);
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
	gchar *reference = NULL;
	static GdaSet *params_set = NULL;

	if (!params_set) 
		params_set = gda_set_new_inline (1, "tblname", G_TYPE_STRING, tblname);
	else 
		gda_set_set_holder_value (params_set, "tblname", tblname);
	model = (GdaDataModel*) gda_connection_statement_execute (cnc, 
								  internal_stmt[INTERNAL_PRAGMA_FK_LIST], params_set, 
								  GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL);

	if (model) {
		int i, nrows;
		const GValue *value;
		const gchar *str;

		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; !reference && (i < nrows) ; i++) {
			value = gda_data_model_get_value_at (model, 3, i);
			str = g_value_get_string ((GValue *) value);
			if (!strcmp (str, field_name)) {
				const gchar *ref_table;

				value = gda_data_model_get_value_at (model, 2, i);
				ref_table = g_value_get_string ((GValue *) value);
				value = gda_data_model_get_value_at (model, 4, i);
				str = g_value_get_string ((GValue *) value);
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
	GdaDataModel *selmodel = NULL, *pragmamodel = NULL;
	int i, nrows;
	GList *list = NULL;
	GdaColumn *fa = NULL;

	static GdaSet *params_set = NULL;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}

	/* find table name */
	par = gda_parameter_list_find_param (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	tblname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname != NULL, NULL);

	/* FIXME: does the table exist? */	

	/* use the SQLite PRAGMA for table information command */
	if (!params_set) 
		params_set = gda_set_new_inline (1, "tblname", G_TYPE_STRING, tblname);
	else 
		gda_set_set_holder_value (params_set, "tblname", tblname);
	pragmamodel = (GdaDataModel*) gda_connection_statement_execute (cnc, 
									internal_stmt[INTERNAL_PRAGMA_TABLE_INFO], params_set, 
									GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL);
	if (!pragmamodel) {
		gda_connection_add_event_string (cnc, _("Can't execute PRAGMA table_info()"));
		return NULL;
	}

	/* run the "SELECT * from table" to fetch information */
	selmodel = (GdaDataModel*) gda_connection_statement_execute (cnc, 
								     internal_stmt[INTERNAL_SELECT_A_TABLE_ROW], params_set, 
								     GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL);
	if (!selmodel) {
		gda_connection_add_event_string (cnc, _("Can't get a table's row"));
		return NULL;
	}
	
	/* create the model */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_FIELDS)));
	g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_FIELDS));

	nrows = gda_data_model_get_n_rows (pragmamodel);
	for (i = 0; i < nrows; i++) {
		GList *rowlist = NULL;
		GValue *nvalue;
		GdaRow *row;
		const GValue *value;
		const gchar *field_name;
		gchar *str;
		gboolean is_pk, found;
			
		row = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (pragmamodel), i, NULL);
		g_assert (row);
		fa = gda_data_model_describe_column (selmodel, i);

		/* col name */
		value = gda_row_get_value (row, 1);
		nvalue = gda_value_copy ((GValue *) value);
		rowlist = g_list_append (rowlist, nvalue);
		field_name = g_value_get_string (nvalue);

		/* data type */
		value = gda_row_get_value (row, 2);
		nvalue = gda_value_copy ((GValue *) value);
		rowlist = g_list_append (rowlist, nvalue);

		/* size */
		g_value_set_int (nvalue = gda_value_new (G_TYPE_INT), 
				 fa ? gda_column_get_defined_size (fa) :  -1);
		rowlist = g_list_append (rowlist, nvalue);

		/* scale */
		g_value_set_int (nvalue = gda_value_new (G_TYPE_INT),
				 fa ? gda_column_get_scale (fa) : -1);
		rowlist = g_list_append (rowlist, nvalue);

		/* not null */
		value = gda_row_get_value (row, 5);
		is_pk = g_value_get_int ((GValue *) value) ? TRUE : FALSE;
		if (is_pk)
			g_value_set_boolean (nvalue = gda_value_new (G_TYPE_BOOLEAN), TRUE);
		else {
			value = gda_row_get_value (row, 3);
			g_value_set_boolean (nvalue = gda_value_new (G_TYPE_BOOLEAN), 
					     g_value_get_int ((GValue *) value) ? TRUE : FALSE);
		}
		rowlist = g_list_append (rowlist, nvalue);

		/* PK */
		g_value_set_boolean (nvalue = gda_value_new (G_TYPE_BOOLEAN), is_pk);
		rowlist = g_list_append (rowlist, nvalue);

		/* unique index */
		found = sqlite_find_field_unique_index (cnc, tblname, field_name);
		g_value_set_boolean (nvalue = gda_value_new (G_TYPE_BOOLEAN), found);
		rowlist = g_list_append (rowlist, nvalue);

		/* FK */
		str = sqlite_find_field_reference (cnc, tblname, field_name);
		if (str && *str)
			g_value_set_string (nvalue = gda_value_new (G_TYPE_STRING), str);
		else
			nvalue = gda_value_new_null ();
		g_free (str);
		rowlist = g_list_append (rowlist, nvalue);

		/* default value */
		value = gda_row_get_value (row, 4);
		if (value && !gda_value_is_null ((GValue *) value))
			str = gda_value_stringify ((GValue *) value);
		else
			str = NULL;
		if (str && *str)
			g_value_set_string (nvalue = gda_value_new (G_TYPE_STRING), str);
		else
			nvalue = gda_value_new_null ();
		g_free (str);
		rowlist = g_list_append (rowlist, nvalue);

		/* extra attributes */
		nvalue = NULL;
		if (SQLITE_VERSION_NUMBER >= 3000000) {
			int status;
			int autoinc;
			status = sqlite3_table_column_metadata (scnc->connection, NULL, tblname, field_name,
								NULL, NULL, NULL, NULL, &autoinc);
			if (status == SQLITE_OK) {
				if (autoinc)
					g_value_set_string (nvalue = gda_value_new (G_TYPE_STRING), "AUTO_INCREMENT");
			}
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
	GdaDataModel *model;

	GdaParameter *par = NULL;
	const gchar *tablename = NULL;
	gchar *part = NULL;
	static GdaSet *params_set = NULL;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (params) {
		par = gda_parameter_list_find_param (params, "name");
		if (par)
			tablename = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	}

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string (cnc, _("Invalid SQLite handle"));
		return NULL;
	}

	if (!params_set) 
		params_set = gda_set_new_inline (2, "tblname", G_TYPE_STRING, tablename,
						 "type", G_TYPE_STRING, views ? "view": "table");
	else {
		gda_set_set_holder_value (params_set, "tblname", tablename);
		gda_set_set_holder_value (params_set, "type", views ? "view": "table");
	}
	if (tablename)
		model = (GdaDataModel*) gda_connection_statement_execute (cnc, 
									  internal_stmt[INTERNAL_SELECT_A_TABLE], params_set, 
									  GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL);
	else
		model = (GdaDataModel*) gda_connection_statement_execute (cnc, 
									  internal_stmt[INTERNAL_SELECT_ALL_TABLES], params_set, 
									  GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL);

	if (tablename)
		part = g_strdup_printf ("AND name = '%s'", tablename);

	if (views) 
		g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (model), GDA_CONNECTION_SCHEMA_VIEWS));
	else 
		g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (model), GDA_CONNECTION_SCHEMA_TABLES));

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
	    strcmp (key, "text") && 
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

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TYPES)));
	g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_TYPES));
	
	/* basic data types */
	add_type_row (recset, "integer", "system", "Signed integer, stored in 1, 2, 3, 4, 6, or 8 bytes depending on the magnitude of the value", G_TYPE_INT);
	add_type_row (recset, "real", "system", "Floating point value, stored as an 8-byte IEEE floating point number", G_TYPE_DOUBLE);
	add_type_row (recset, "text", "system", "Text string, stored using the database encoding", G_TYPE_STRING);
	add_type_row (recset, "blob", "system", "Blob of data, stored exactly as it was input", GDA_TYPE_BINARY);


	/* scan the data types of all the columns of all the tables */
	_gda_sqlite_update_types_hash (scnc);
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
		if (scnc->aggregates_model) {
			g_object_ref (scnc->aggregates_model);
			return scnc->aggregates_model;
		}
		recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
					       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_AGGREGATES)));
		g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_AGGREGATES));
	}
	else {
		if (scnc->functions_model) {
			g_object_ref (scnc->functions_model);
			return scnc->functions_model;
		}
		recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
					       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_PROCEDURES)));
		g_assert (gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_PROCEDURES));
	}

	GdaDataModel *pragmamodel = NULL;
	int i, nrows;
	GList *list = NULL;

	pragmamodel = (GdaDataModel *) gda_connection_statement_execute (cnc, internal_stmt[INTERNAL_PRAGMA_PROC_LIST], NULL, 
									 GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL);
	if (pragmamodel) {
		nrows = gda_data_model_get_n_rows (pragmamodel);
		for (i = 0; i < nrows; i++) {
			GdaRow *row;
			GList *rowlist = NULL;
			GValue *value, *pvalue;
			gint nbargs;
			
			row = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (pragmamodel), i, NULL);
			g_assert (row);
			pvalue = gda_row_get_value (row, 1);
			if ((pvalue && (g_value_get_int (pvalue) != 0) && aggs) ||
			    ((g_value_get_int (pvalue) == 0) && !aggs)) {
				gchar *str;
				
				/* Proc name */
				pvalue = gda_row_get_value (row, 0);
				g_value_set_string (value = gda_value_new (G_TYPE_STRING), g_value_get_string (pvalue));
				rowlist = g_list_append (rowlist, value);
				
				/* Proc_Id */
				if (! aggs)
					str = g_strdup_printf ("p%d", i);
				else
					str = g_strdup_printf ("a%d", i);
				g_value_take_string (value = gda_value_new (G_TYPE_STRING), str);
				rowlist = g_list_append (rowlist, value);
				
				/* Owner */
				g_value_set_string (value = gda_value_new (G_TYPE_STRING), "system");
				rowlist = g_list_append (rowlist, value);
				
				/* Comments */ 
				rowlist = g_list_append (rowlist, gda_value_new_null());
				
				/* Out type */ 
				g_value_set_string (value = gda_value_new (G_TYPE_STRING), "text");
				rowlist = g_list_append (rowlist, value);
				
				if (! aggs) {
					/* Number of args */
					pvalue = gda_row_get_value (row, 2);
					nbargs = g_value_get_int (pvalue);
					g_value_set_int (value = gda_value_new (G_TYPE_INT), nbargs);
					rowlist = g_list_append (rowlist, value);
				}
				
				/* In types */
				if (! aggs) {
					if (nbargs > 0) {
						GString *string;
						gint j;
						
						string = g_string_new ("");
						for (j = 0; j < nbargs; j++) {
							if (j > 0)
								g_string_append_c (string, ',');
							g_string_append_c (string, '-');
						}
						g_value_take_string (value = gda_value_new (G_TYPE_STRING), string->str);
						g_string_free (string, FALSE);
					}
					else
						g_value_set_string (value = gda_value_new (G_TYPE_STRING), "");
				}
				else
					g_value_set_string (value = gda_value_new (G_TYPE_STRING), "");
				rowlist = g_list_append (rowlist, value);
				
				/* Definition */
				rowlist = g_list_append (rowlist, gda_value_new_null());
				
				list = g_list_append (list, rowlist);
			}
		}
		g_object_unref (pragmamodel);
	}
	else {
		/* return empty list */		
	}

	g_list_foreach (list, (GFunc) add_g_list_row, recset);
	g_list_free (list);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_constraints (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	SQLITEcnc *scnc;
	GdaParameter *par;
	const gchar *tblname;
	GdaDataModel *pragmamodel = NULL;
	int i, nrows;
	static GdaSet *params_set = NULL;

	/* current constraint */
	gint cid;
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

	tblname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
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
	if (!params_set) 
		params_set = gda_set_new_inline (1, "tblname", G_TYPE_STRING, tblname);
	else 
		gda_set_set_holder_value (params_set, "tblname", tblname);
	pragmamodel = (GdaDataModel*) gda_connection_statement_execute (cnc, 
									internal_stmt[INTERNAL_PRAGMA_TABLE_INFO], params_set, 
									GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL);
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
		GValue *value;
		row = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (pragmamodel), i, NULL);
		g_assert (row);

		value = gda_row_get_value (row, 5);
		if (g_value_get_int ((GValue *) value)) {
			if (!crow) {
				gint new_row_num;
				
				new_row_num = gda_data_model_append_row (GDA_DATA_MODEL (recset), NULL);
				crow = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (recset), new_row_num, NULL);
				
				g_value_set_string (value = gda_value_new (G_TYPE_STRING), "");
				gda_row_set_value (crow, 0, value);
				gda_value_free (value);
				
				g_value_set_string (value = gda_value_new (G_TYPE_STRING), "PRIMARY_KEY");
				gda_row_set_value (crow, 1, value);
				gda_value_free (value);
			}
			
			value = gda_row_get_value (row, 1);
			if (!cfields)
				cfields = g_string_new (g_value_get_string (value));
			else {
				g_string_append_c (cfields, ',');
				g_string_append (cfields, g_value_get_string (value));
			}
		}
	}

	if (crow) {
		GValue *value;

		g_value_set_string (value = gda_value_new (G_TYPE_STRING), cfields->str);
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
	pragmamodel = (GdaDataModel*) gda_connection_statement_execute (cnc, 
									internal_stmt[INTERNAL_PRAGMA_FK_LIST], params_set, 
									GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL);
	if (!pragmamodel) {
		gda_connection_add_event_string (cnc, _("Can't execute PRAGMA foreign_key_list()"));
		return NULL;
	}

	/* analysing the returned data model for the PRAGMA command */
	cid = 0;
	cfields = NULL;
	cref_fields = NULL;
	crow = NULL;
	nrows = gda_data_model_get_n_rows (pragmamodel);
	for (i = 0; i < nrows; i++) {
		GdaRow *row;
		GValue *value;
		row = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (pragmamodel), i, NULL);
		g_assert (row);

		value = gda_row_get_value (row, 0);
		if (!cid || (cid != g_value_get_int ((GValue *) value))) {
			gint new_row_num;
			cid = g_value_get_int ((GValue *) value);

			/* record current constraint */
			if (crow) {
				g_value_take_string (value = gda_value_new (G_TYPE_STRING), cfields->str);
				gda_row_set_value (crow, 2, value);
				gda_value_free (value);
				g_string_free (cfields, FALSE);

				g_string_append_c (cref_fields, ')');
				g_value_take_string (value = gda_value_new (G_TYPE_STRING), cref_fields->str);
				gda_row_set_value (crow, 3, value);
				gda_value_free (value);
				g_string_free (cref_fields, FALSE);
			}

			/* start a new constraint */
			new_row_num = gda_data_model_append_row (GDA_DATA_MODEL (recset), NULL);
			crow = gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (recset), new_row_num, NULL);
			
			g_value_set_string (value = gda_value_new (G_TYPE_STRING), "");
			gda_row_set_value (crow, 0, value);
			gda_value_free (value);

			g_value_set_string (value = gda_value_new (G_TYPE_STRING), "FOREIGN_KEY");
			gda_row_set_value (crow, 1, value);
			gda_value_free (value);

			
			value = gda_row_get_value (row, 3);
			cfields = g_string_new (g_value_get_string (value));
			
			value = gda_row_get_value (row, 2);
			cref_fields = g_string_new (g_value_get_string (value));
			g_string_append_c (cref_fields, '(');
			value = gda_row_get_value (row, 4);
			g_string_append (cref_fields, (gchar *) g_value_get_string (value));

			value = gda_value_new_null ();
			gda_row_set_value (crow, 4, value);
			gda_value_free (value);
		}
		else {
			/* "augment" the current constraint */
			g_string_append_c (cfields, ',');
			value = gda_row_get_value (row, 3);
			g_string_append (cfields, (gchar *) g_value_get_string (value));

			g_string_append_c (cref_fields, ',');
			value = gda_row_get_value (row, 4);
			g_string_append (cref_fields, (gchar *) g_value_get_string (value));
		}
	       
	}

	if (crow) {
		GValue *value;

		g_value_take_string (value = gda_value_new (G_TYPE_STRING), cfields->str);
		gda_row_set_value (crow, 2, value);
		gda_value_free (value);
		g_string_free (cfields, FALSE);
		
		g_string_append_c (cref_fields, ')');
		g_value_take_string (value = gda_value_new (G_TYPE_STRING), cref_fields->str);
		gda_row_set_value (crow, 3, value);
		gda_value_free (value);
		g_string_free (cref_fields, FALSE);
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
				      GType type,
				      const gchar *dbms_type)
{
	GdaDataHandler *dh = NULL;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	if (cnc) 
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

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
        else if (type == GDA_TYPE_BINARY) {
		dh = gda_server_provider_handler_find (provider, cnc, type, NULL);
		if (!dh) {
			dh = gda_sqlite_handler_bin_new ();
			if (dh) {
				gda_server_provider_handler_declare (provider, dh, cnc, GDA_TYPE_BINARY, NULL);
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
	else if (type == GDA_TYPE_TIME) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_time_new ();
			/* SQLite cannot handle date or timestamp because no defined format */
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIME, NULL);
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
		/* special case) || we take into account the dbms_type argument */
		if (dbms_type)
			TO_IMPLEMENT;
	}

	return dh;	
}

static const gchar*
gda_sqlite_provider_get_default_dbms_type (GdaServerProvider *provider,
					   GdaConnection *cnc,
					   GType type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == GDA_TYPE_USHORT) ||
	    (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) ||
	    (type == G_TYPE_ULONG) ||
	    (type == G_TYPE_UINT) ||
	    (type == G_TYPE_UINT64)) 
		return "integer";

	if (type == GDA_TYPE_BINARY)
		return "blob";

	if ((type == G_TYPE_BOOLEAN) ||
	    (type == G_TYPE_DATE) || 
	    (type == GDA_TYPE_GEOMETRIC_POINT) ||
	    (type == G_TYPE_OBJECT) ||
	    (type == GDA_TYPE_LIST) ||
	    (type == G_TYPE_STRING) ||
	    (type == GDA_TYPE_TIME) ||
	    (type == GDA_TYPE_TIMESTAMP) ||
	    (type == G_TYPE_INVALID))
		return "string";

	if ((type == G_TYPE_DOUBLE) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT))
		return "real";
	
	return "text";
}

static GdaSqlParser *
gda_sqlite_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	return (GdaSqlParser*) g_object_new (GDA_TYPE_SQL_PARSER, "tokenizer-flavour", GDA_SQL_PARSER_FLAVOUR_SQLITE, NULL);
}

static gchar *
sqlite_render_operation (GdaSqlOperation *op, GdaSqlRenderingContext *context, GError **error)
{
	gchar *str;
	GSList *list;
	GSList *sql_list; /* list of SqlOperand */
	GString *string;
	gchar *multi_op = NULL;

	typedef struct {
		gchar    *sql;
		gboolean  is_null;
		gboolean  is_default;
		gboolean  is_composed;
	} SqlOperand;
#define SQL_OPERAND(x) ((SqlOperand*)(x))

	g_return_val_if_fail (op, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (op)->type == GDA_SQL_ANY_SQL_OPERATION, NULL);

	/* can't have: 
	 *  - op->operands == NULL 
	 *  - incorrect number of operands
	 */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (op), error)) return NULL;

	/* render operands */
	for (list = op->operands, sql_list = NULL; list; list = list->next) {
		SqlOperand *sqlop = g_new0 (SqlOperand, 1);
		GdaSqlExpr *expr = (GdaSqlExpr*) list->data;
		str = context->render_expr (expr, context, &(sqlop->is_default), &(sqlop->is_null), error);
		if (!str) {
			g_free (sqlop);
			goto out;
		}
		sqlop->sql = str;
		if (expr->cond || expr->case_s || expr->select)
			sqlop->is_composed = TRUE;
		sql_list = g_slist_prepend (sql_list, sqlop);
	}
	sql_list = g_slist_reverse (sql_list);

	str = NULL;
	switch (op->operator) {
	case GDA_SQL_OPERATOR_EQ:
		if (SQL_OPERAND (sql_list->next->data)->is_null) 
			str = g_strdup_printf ("%s IS NULL", SQL_OPERAND (sql_list->data)->sql);
		else
			str = g_strdup_printf ("%s = %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_IS:
		str = g_strdup_printf ("%s IS %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_LIKE:
		str = g_strdup_printf ("%s LIKE %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_GT:
		str = g_strdup_printf ("%s > %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_LT:
		str = g_strdup_printf ("%s < %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_GEQ:
		str = g_strdup_printf ("%s >= %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_LEQ:
		str = g_strdup_printf ("%s <= %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_DIFF:
		str = g_strdup_printf ("%s != %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_REGEXP:
		str = g_strdup_printf ("%s REGEXP %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_REGEXP_CI:
	case GDA_SQL_OPERATOR_NOT_REGEXP_CI:
	case GDA_SQL_OPERATOR_SIMILAR:
		/* does not exist in SQLite => error */
		break;
	case GDA_SQL_OPERATOR_NOT_REGEXP:
		str = g_strdup_printf ("NOT %s REGEXP %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_REM:
		str = g_strdup_printf ("%s %% %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_DIV:
		str = g_strdup_printf ("%s / %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_BITAND:
		str = g_strdup_printf ("%s & %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_BITOR:
		str = g_strdup_printf ("%s | %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_BETWEEN:
		str = g_strdup_printf ("%s BETWEEN %s AND %s", SQL_OPERAND (sql_list->data)->sql, 
				       SQL_OPERAND (sql_list->next->data)->sql,
				       SQL_OPERAND (sql_list->next->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_ISNULL:
		str = g_strdup_printf ("%s IS NULL", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_ISNOTNULL:
		str = g_strdup_printf ("%s IS NOT NULL", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_BITNOT:
		str = g_strdup_printf ("~ %s", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_NOT:
		str = g_strdup_printf ("NOT %s", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_IN:
		string = g_string_new (SQL_OPERAND (sql_list->data)->sql);
		g_string_append (string, " IN (");
		for (list = sql_list->next; list; list = list->next) {
			if (list != sql_list->next)
				g_string_append (string, ", ");
			g_string_append (string, SQL_OPERAND (list->data)->sql);
		}
		g_string_append_c (string, ')');
		str = string->str;
		g_string_free (string, FALSE);
		break;
	case GDA_SQL_OPERATOR_CONCAT:
		multi_op = "||";
		break;
	case GDA_SQL_OPERATOR_PLUS:
		multi_op = "+";
		break;
	case GDA_SQL_OPERATOR_MINUS:
		multi_op = "-";
		break;
	case GDA_SQL_OPERATOR_STAR:
		multi_op = "*";
		break;
	case GDA_SQL_OPERATOR_AND:
		multi_op = "AND";
		break;
	case GDA_SQL_OPERATOR_OR:
		multi_op = "OR";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	if (multi_op) {
		if (!sql_list->next) {
			/* 1 operand only */
			string = g_string_new ("");
			g_string_append_printf (string, "%s %s", multi_op, SQL_OPERAND (sql_list->data)->sql);
		}
		else {
			/* 2 or more operands */
			if (SQL_OPERAND (sql_list->data)->is_composed) {
				string = g_string_new ("(");
				g_string_append (string, SQL_OPERAND (sql_list->data)->sql);
				g_string_append_c (string, ')');
			}
			else
				string = g_string_new (SQL_OPERAND (sql_list->data)->sql);
			for (list = sql_list->next; list; list = list->next) {
				g_string_append_printf (string, " %s ", multi_op);
				if (SQL_OPERAND (list->data)->is_composed) {
					g_string_append_c (string, '(');
					g_string_append (string, SQL_OPERAND (list->data)->sql);
					g_string_append_c (string, ')');
				}
				else
					g_string_append (string, SQL_OPERAND (list->data)->sql);
			}
		}
		str = string->str;
		g_string_free (string, FALSE);
	}

 out:
	for (list = sql_list; list; list = list->next) {
		g_free (((SqlOperand*)list->data)->sql);
		g_free (list->data);
	}
	g_slist_free (sql_list);

	return str;
}

static gchar *
gda_sqlite_provider_statement_to_sql (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
				      GSList **params_used, GError **error)
{
	gchar *str;
	GdaSqlRenderingContext context;

	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, NULL);
	}

	memset (&context, 0, sizeof (context));
	context.params = params;
	context.flags = flags;
	context.render_operation = (GdaSqlRenderingFunc) sqlite_render_operation;

	str = gda_statement_to_sql_real (stmt, &context, error);

	if (str) {
		if (params_used)
			*params_used = context.params_used;
		else
			g_slist_free (context.params_used);
	}
	else {
		if (params_used)
			*params_used = NULL;
		g_slist_free (context.params_used);
	}
	return str;
}

static SQLitePreparedStatement *
real_prepare (GdaServerProvider *provider, GdaConnection *cnc, GdaStatement *stmt, GError **error)
{
	int status;
	SQLITEcnc *scnc;
	sqlite3_stmt *sqlite_stmt;
	gchar *sql;
	const char *left;
	SQLitePreparedStatement *ps;
	GdaSet *params = NULL;
	GSList *used_params = NULL;

	/* get SQLite's private data */
	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string (cnc, _("Invalid SQLITE handle"));
		return NULL;
	}

	/* render as SQL understood by SQLite */
	if (! gda_statement_get_parameters (stmt, &params, error))
		return NULL;

	sql = gda_sqlite_provider_statement_to_sql (provider, cnc, stmt, params, GDA_STATEMENT_SQL_PARAMS_AS_QMARK,
						    &used_params, error);
	if (!sql) 
		goto out_err;

	/* prepare statement */
	status = sqlite3_prepare_v2 (scnc->connection, sql, -1, &sqlite_stmt, &left);
	if (status != SQLITE_OK) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
			     sqlite3_errmsg (scnc->connection));
		goto out_err;
	}

	if (left && (*left != 0))
		g_warning ("SQlite SQL: %s (REMAIN:%s)\n", sql, left);

	/* make a list of the parameter names used in the statement */
	GSList *param_ids = NULL;
	if (used_params) {
		GSList *list;
		for (list = used_params; list; list = list->next) {
			const gchar *cid;
			cid = gda_holder_get_id (GDA_HOLDER (list->data));
			if (cid) {
				param_ids = g_slist_append (param_ids, g_strdup (cid));
				/*g_print ("PREPARATION: param ID: %s\n", cid);*/
			}
			else {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
					     _("Unnammed parameter is not allowed in prepared statements"));
				g_slist_foreach (param_ids, (GFunc) g_free, NULL);
				g_slist_free (param_ids);
				goto out_err;
			}
		}
	}

	/* create a SQLitePreparedStatement */
	ps = g_new0 (SQLitePreparedStatement, 1);
	ps->sqlite_stmt = sqlite_stmt;
	ps->param_ids = param_ids;
	ps->sql = sql;
	ps->ref_count = 0;
	ps->stmt_used = FALSE;
	return ps;

 out_err:
	if (used_params)
		g_slist_free (used_params);
	if (params)
		g_object_unref (params);
	g_free (sql);
	return NULL;
}

static gboolean
gda_sqlite_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaStatement *stmt, GError **error)
{
	SQLitePreparedStatement *ps;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	/* fetch prepares stmt if already done */
	ps = gda_connection_get_prepared_statement (cnc, (GObject*) stmt);
	if (ps)
		return TRUE;

	ps = real_prepare (provider, cnc, stmt, error);
	if (!ps)
		return FALSE;
	else {
		ps->ref_count = 1;
		gda_connection_add_prepared_statement (cnc, G_OBJECT (stmt), ps);
		return TRUE;
	}
}

static GObject *
gda_sqlite_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaStatement *stmt, GdaSet *params,
				       GdaStatementModelUsage model_usage, GError **error)
{
	SQLitePreparedStatement *ps;
	SQLITEcnc *scnc;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	/* get SQLite's private data */
	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string (cnc, _("Invalid SQLITE handle"));
		return FALSE;
	}

	/* HACK: force SQLite to reparse the schema and thus discover new tables if necessary */
        {
                gint status;
                sqlite3_stmt *istmt = NULL;
                status = sqlite3_prepare_v2 (scnc->connection, "SELECT 1 FROM sqlite_master LIMIT 1", -1, &istmt, NULL);
                if (status == SQLITE_OK)
                        sqlite3_step (istmt);
                if (istmt)
                        sqlite3_finalize (istmt);
        }

	/* get/create new prepared statement */
	ps = gda_connection_get_prepared_statement (cnc, (GObject*) stmt);
	if (!ps) {
		if (!gda_sqlite_provider_statement_prepare (provider, cnc, stmt, NULL)) {
			/* try to use the SQL when parameters are rendered with their values */
			gchar *sql;
			int status;
			sqlite3_stmt *sqlite_stmt;
			char *left;

			sql = gda_sqlite_provider_statement_to_sql (provider, cnc, stmt, params, 0, NULL, error);
			if (!sql)
				return NULL;

			status = sqlite3_prepare_v2 (scnc->connection, sql, -1, &sqlite_stmt, (const char**) &left);			
			if (status != SQLITE_OK) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
					     sqlite3_errmsg (scnc->connection));
				g_free (sql);
				return NULL;
			}

			if (left && (*left != 0)) {
				g_warning ("SQlite SQL: %s (REMAIN:%s)\n", sql, left);
				*left = 0;
			}

			/* create a SQLitePreparedStatement */
			SQLitePreparedStatement *ps;
			ps = g_new0 (SQLitePreparedStatement, 1);
			ps->sqlite_stmt = sqlite_stmt;
			ps->ref_count = 0;
			ps->param_ids = NULL;
			ps->sql = sql;
		}
		else
			ps = gda_connection_get_prepared_statement (cnc, (GObject*) stmt);
	}
	else if (ps->stmt_used) {
		/* Don't use @ps => prepare stmt again */
		ps = real_prepare (provider, cnc, stmt, error);
		if (!ps)
			return NULL;
	}

	/* reset prepared stmt */
	if ((sqlite3_reset (ps->sqlite_stmt) != SQLITE_OK) || 
	    (sqlite3_clear_bindings (ps->sqlite_stmt) != SQLITE_OK)) {
		GdaConnectionEvent *event;
		const char *errmsg;

		errmsg = sqlite3_errmsg (scnc->connection);
		event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
		gda_connection_event_set_description (event, errmsg);
		gda_connection_add_event (cnc, event);
		gda_connection_del_prepared_statement (cnc, G_OBJECT (stmt));
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
			     errmsg);
		return NULL;
	}
	
	/* bind statement's parameters */
	GSList *list;
	GdaConnectionEvent *event = NULL;
	int i;
	for (i = 1, list = ps->param_ids; list; list = list->next, i++) {
		const gchar *pname = (gchar *) list->data;
		
		if (!params) {
			event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, _("Missing parameter(s) to execute query"));
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
				     _("Missing parameter(s) to execute query"));
			break;
		}

		GdaHolder *h;
		h = gda_set_get_holder (params, pname);
		if (!h) {
			gchar *tmp = gda_alphanum_to_text (g_strdup (pname + 1));
			if (tmp) {
				h = gda_set_get_holder (params, tmp);
				g_free (tmp);
			}
		}
		if (!h) {
			gchar *str;
			str = g_strdup_printf (_("Missing parameter '%s' to execute query"), pname);
			event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, str);
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, str);
			g_free (str);
			break;
		}

		/*g_print ("BINDING param '%s' to %p\n", pname, h);*/
		
		const GValue *value = gda_holder_get_value (h);
		if (!value || gda_value_is_null (value))
			sqlite3_bind_null (ps->sqlite_stmt, i);
		else if (G_VALUE_TYPE (value) == G_TYPE_STRING)
			sqlite3_bind_text (ps->sqlite_stmt, i, 
					   g_value_get_string (value), -1, SQLITE_STATIC);
		else if (G_VALUE_TYPE (value) == G_TYPE_INT)
			sqlite3_bind_int (ps->sqlite_stmt, i, g_value_get_int (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_DOUBLE)
			sqlite3_bind_double (ps->sqlite_stmt, i, g_value_get_double (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_FLOAT)
			sqlite3_bind_double (ps->sqlite_stmt, i, g_value_get_float (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_UINT)
			sqlite3_bind_int (ps->sqlite_stmt, i, g_value_get_uint (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_INT64)
			sqlite3_bind_int64 (ps->sqlite_stmt, i, g_value_get_int64 (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_UINT64)
			sqlite3_bind_int64 (ps->sqlite_stmt, i, g_value_get_uint64 (value));
		else if (G_VALUE_TYPE (value) == GDA_TYPE_SHORT)
			sqlite3_bind_int (ps->sqlite_stmt, i, gda_value_get_short (value));
		else if (G_VALUE_TYPE (value) == GDA_TYPE_USHORT)
			sqlite3_bind_int (ps->sqlite_stmt, i, gda_value_get_ushort (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_CHAR)
			sqlite3_bind_int (ps->sqlite_stmt, i, g_value_get_char (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_UCHAR)
			sqlite3_bind_int (ps->sqlite_stmt, i, g_value_get_uchar (value));
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GdaBinary *bin = (GdaBinary *) gda_value_get_blob (value);
			sqlite3_bind_blob (ps->sqlite_stmt, i, 
					   bin->data, bin->binary_length, SQLITE_STATIC);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			GdaBinary *bin = (GdaBinary *) gda_value_get_binary (value);
			sqlite3_bind_blob (ps->sqlite_stmt, i, 
					   bin->data, bin->binary_length, SQLITE_STATIC);
		}
		else {
			gchar *str;
			str = g_strdup_printf (_("Non handled data type '%s'"), 
					       g_type_name (G_VALUE_TYPE (value)));
			event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, str);
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, str);
			g_free (str);
			break;
		}
	}
		
	if (event) {
		gda_connection_add_event (cnc, event);
		return NULL;
	}
	
	/* add a connection event */
	event = gda_connection_event_new (GDA_CONNECTION_EVENT_COMMAND);
        gda_connection_event_set_description (event, ps->sql);
        gda_connection_add_event (cnc, event);
	
	/* treat prepared and bound statement */
	if (! g_ascii_strncasecmp (ps->sql, "SELECT", 6) ||
            ! g_ascii_strncasecmp (ps->sql, "PRAGMA", 6) ||
            ! g_ascii_strncasecmp (ps->sql, "EXPLAIN", 7)) {
		GObject *data_model;

                data_model = (GObject *) gda_sqlite_recordset_new (cnc, ps);
                g_object_set (data_model,
                              "command_text", ps->sql,
                              "command_type", GDA_COMMAND_TYPE_SQL, NULL);
		gda_connection_internal_statement_executed (cnc, stmt, NULL);
		return data_model;
        }
	else {
                int status, changes;
                sqlite3 *handle;

                /* actually execute the command */
                handle = sqlite3_db_handle (ps->sqlite_stmt);
                status = sqlite3_step (ps->sqlite_stmt);
                changes = sqlite3_changes (handle);
                if (status != SQLITE_DONE) {
                        if (sqlite3_errcode (handle) != SQLITE_OK) {
				const char *errmsg = sqlite3_errmsg (handle);
                                sqlite3_reset (ps->sqlite_stmt);
                                event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
                                gda_connection_event_set_description (event, errmsg);
                                gda_connection_add_event (cnc, event);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, errmsg);
				gda_connection_internal_statement_executed (cnc, stmt, event);
				return NULL;
                        }
			else {
				TO_IMPLEMENT;
				return NULL;
			}
                }
                else {
			GObject *set;
			gchar *str = NULL;

			event = NULL;
			set = (GObject*) gda_set_new_inline (1, "IMPACTED_ROWS", G_TYPE_INT, changes, NULL);
                        if (! g_ascii_strncasecmp (ps->sql, "DELETE", 6))
                                str = g_strdup_printf ("DELETE %d (see SQLite documentation for a \"DELETE * FROM table\" query)",
                                                       changes);
                        else {
                                if (! g_ascii_strncasecmp (ps->sql, "INSERT", 6))
                                        str = g_strdup_printf ("INSERT %lld %d",
                                                               sqlite3_last_insert_rowid (handle),
                                                               changes);
                                else {
                                        if (!g_ascii_strncasecmp (ps->sql, "DELETE", 6))
                                                str = g_strdup_printf ("DELETE %d", changes);
                                        else {
                                                if (*(ps->sql)) {
                                                        gchar *tmp = g_ascii_strup (ps->sql, -1);
                                                        for (str = tmp; *str && (*str != ' ') && (*str != '\t') &&
                                                                     (*str != '\n'); str++);
                                                        *str = 0;
                                                        if (changes > 0) {
                                                                str = g_strdup_printf ("%s %d", tmp,
                                                                                       changes);
                                                                g_free (tmp);
                                                        }
                                                        else
                                                                str = tmp;
                                                }
                                        }
                                }
                        }
			if (str) {
                                event = gda_connection_event_new (GDA_CONNECTION_EVENT_NOTICE);
                                gda_connection_event_set_description (event, str);
                                g_free (str);
                                gda_connection_add_event (cnc, event);
                        }
			gda_connection_internal_statement_executed (cnc, stmt, event);
			return set;
		}
	}
}

/* 
 * SQLite's extra functions' implementations
 */
static void
scalar_gda_file_exists_func (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	const gchar *path;

	if (argc != 1) {
		sqlite3_result_error (context, _("Function requires one argument"), -1);
		return;
	}

	path = (gchar *) sqlite3_value_text (argv [0]);
	if (g_file_test (path, G_FILE_TEST_EXISTS))
		sqlite3_result_int (context, 1);
	else
		sqlite3_result_int (context, 0);
}

static void
gda_sqlite_free_cnc (SQLITEcnc *scnc)
{
	if (!scnc)
		return;

#ifdef HAVE_SQLITE
	/* remove the (scnc->connection, scnc) to db_connections_hash */
	if (db_connections_hash && scnc->connection)
		g_hash_table_remove (db_connections_hash, scnc->connection);
#endif

	if (scnc->connection) 
		sqlite3_close (scnc->connection);
	g_free (scnc->file);
	if (scnc->types)
		g_hash_table_destroy (scnc->types);
	if (scnc->aggregates_model) 
		g_object_unref (scnc->aggregates_model);
	if (scnc->functions_model) 
		g_object_unref (scnc->functions_model);
	g_free (scnc);
}
