/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <string.h>
#include "gda-vprovider-data-model.h"
#include "gda-vconnection-data-model.h"
#include "gda-vconnection-data-model-private.h"
#include <sqlite3.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-connection-internal.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-blob-op.h>
#include "../gda-sqlite.h"
#include <sql-parser/gda-statement-struct-util.h>

#define GDA_DEBUG_VIRTUAL
#undef GDA_DEBUG_VIRTUAL

struct _GdaVproviderDataModelPrivate {
	int foo;
};

static void gda_vprovider_data_model_class_init (GdaVproviderDataModelClass *klass);
static void gda_vprovider_data_model_init       (GdaVproviderDataModel *prov, GdaVproviderDataModelClass *klass);
static void gda_vprovider_data_model_finalize   (GObject *object);
static GObjectClass  *parent_class = NULL;

static GdaConnection *gda_vprovider_data_model_create_connection (GdaServerProvider *provider);
static gboolean       gda_vprovider_data_model_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
								GdaQuarkList *params, GdaQuarkList *auth,
								guint *task_id, GdaServerProviderAsyncCallback async_cb, 
								gpointer cb_data);
static gboolean       gda_vprovider_data_model_close_connection (GdaServerProvider *provider,
								 GdaConnection *cnc);
static GObject        *gda_vprovider_data_model_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaStatement *stmt, GdaSet *params,
								   GdaStatementModelUsage model_usage,
								   GType *col_types, GdaSet **last_inserted_row,
								   guint *task_id, GdaServerProviderExecCallback async_cb,
								   gpointer cb_data, GError **error);
static const gchar   *gda_vprovider_data_model_get_name (GdaServerProvider *provider);

/*
 * GdaVproviderDataModel class implementation
 */
static void
gda_vprovider_data_model_class_init (GdaVproviderDataModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *server_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_vprovider_data_model_finalize;
	server_class->create_connection = gda_vprovider_data_model_create_connection;
	server_class->open_connection = gda_vprovider_data_model_open_connection;
	server_class->close_connection = gda_vprovider_data_model_close_connection;
	server_class->statement_execute = gda_vprovider_data_model_statement_execute;

	server_class->get_name = gda_vprovider_data_model_get_name;

	/* explicitly unimplement the DDL queries */
	server_class->supports_operation = NULL;
        server_class->create_operation = NULL;
        server_class->render_operation = NULL;
        server_class->perform_operation = NULL;
}

static void
gda_vprovider_data_model_init (GdaVproviderDataModel *prov, G_GNUC_UNUSED GdaVproviderDataModelClass *klass)
{
	prov->priv = g_new (GdaVproviderDataModelPrivate, 1);
}

static void
gda_vprovider_data_model_finalize (GObject *object)
{
	GdaVproviderDataModel *prov = (GdaVproviderDataModel *) object;

	g_return_if_fail (GDA_IS_VPROVIDER_DATA_MODEL (prov));

	/* free memory */
	g_free (prov->priv);
	prov->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_vprovider_data_model_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaVproviderDataModelClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_vprovider_data_model_class_init,
				NULL, NULL,
				sizeof (GdaVproviderDataModel),
				0,
				(GInstanceInitFunc) gda_vprovider_data_model_init,
				0
			};
			
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_VIRTUAL_PROVIDER, "GdaVproviderDataModel", &info, 0);
		g_static_mutex_unlock (&registering);
		}
	}

	return type;
}

/**
 * gda_vprovider_data_model_new
 *
 * Creates a new GdaVirtualProvider object which allows one to 
 * add and remove GdaDataModel objects as tables within a connection
 *
 * Returns: a new #GdaVirtualProvider object.
 */
GdaVirtualProvider *
gda_vprovider_data_model_new (void)
{
	GdaVirtualProvider *provider;

        provider = g_object_new (gda_vprovider_data_model_get_type (), NULL);
        return provider;
}

/*
 * Note about RowIDs and how SQLite uses them:
 *
 * SQLite considers that each virtual table has unique row IDs, absolute value available all the time.
 * When an UPDATE or DELETE statement is executed, SQLite does the following:
 *  - xBegin (table): request a transaction start
 *  - xOpen (table): create a new cursor
 *  - xFilter (cursor): initialize the cursor
 *  - moves the cursor one step at a time up to the end (xEof, xColumn and xNext). If it finds a
 *    row needing to be updated or deleted, it calls xRowid (cursor) to get the RowID
 *  - xClose (cursor): free the now useless cursor
 *  - calls xUpdate (table) as many times as needed (one time for each row where xRowid was called)
 *  - xSync (table)
 *  - xCommit (table)
 *
 * This does not work well with Libgda because it does not know how to define a unique RowID for
 * a table: it defines the RowID as being the position of the row in the data model, which changes
 * each time the data model used for the virtuel table changes.
 *
 * Moreover, when the cursor has reached the end of the data model, it may not be possible to move
 * it backwards and so the data at any given row is not accessible anymore. The solution to this
 * problem is, each time the xRowid() is called, to copy the row in memory, using the table->rowid_hash
 * hash table.
 */

#ifdef GDA_DEBUG_VIRTUAL
#define TRACE(table,cursor) g_print ("== %s (table=>%p cursor=>%p)\n", __FUNCTION__, (table), (cursor))
#else
#define TRACE(table,cursor)
#endif

typedef struct {
	sqlite3_vtab                 base;
	GdaVconnectionDataModel     *cnc;
	GdaVConnectionTableData     *td;

	GdaDataModel                *rowid_hash_model; /* data model used to build the rowid_hash's
							* contents. No ref held there as it's never
							* dereferenced */
	GHashTable                  *rowid_hash; /* key = a gint64 rowId, value = a GdaRow */
} VirtualTable;

typedef struct {
	sqlite3_vtab_cursor      base; /* base.pVtab is a pointer to the sqlite3_vtab virtual table */
	GdaDataModel            *model; /* in which @iter iterates */
	GdaDataModelIter        *iter;
} VirtualCursor;

/* module creation */
static int virtualCreate (sqlite3 *db, void *pAux, int argc, const char *const *argv, sqlite3_vtab **ppVtab, char **pzErr);
static int virtualConnect (sqlite3 *db, void *pAux, int argc, const char *const *argv, sqlite3_vtab **ppVtab, char **pzErr);
static int virtualDisconnect (sqlite3_vtab *pVtab);
static int virtualDestroy (sqlite3_vtab *pVtab);
static int virtualOpen (sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor);
static int virtualClose (sqlite3_vtab_cursor *cur);
static int virtualEof (sqlite3_vtab_cursor *cur);
static int virtualNext (sqlite3_vtab_cursor *cur);
static int virtualColumn (sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int i);
static int virtualRowid (sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid);
static int virtualFilter (sqlite3_vtab_cursor *pVtabCursor, int idxNum, const char *idxStr, int argc, sqlite3_value **argv);
static int virtualBestIndex (sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo);
static int virtualUpdate (sqlite3_vtab *tab, int nData, sqlite3_value **apData, sqlite_int64 *pRowid);
static int virtualBegin (sqlite3_vtab *tab);
static int virtualSync (sqlite3_vtab *tab);
static int virtualCommit (sqlite3_vtab *tab);
static int virtualRollback (sqlite3_vtab *tab);
static int virtualRename (sqlite3_vtab *pVtab, const char *zNew);

static sqlite3_module Module = {
	0,                         /* iVersion */
	virtualCreate,
	virtualConnect,
	virtualBestIndex,
	virtualDisconnect, 
	virtualDestroy,
	virtualOpen,                  /* xOpen - open a cursor */
	virtualClose,                 /* xClose - close a cursor */
	virtualFilter,                /* xFilter - configure scan constraints */
	virtualNext,                  /* xNext - advance a cursor */
	virtualEof,                   /* xEof */
	virtualColumn,                /* xColumn - read data */
	virtualRowid,                 /* xRowid - read data */
	virtualUpdate,                /* xUpdate - write data */
	virtualBegin,                 /* xBegin - begin transaction */
	virtualSync,                  /* xSync - sync transaction */
	virtualCommit,                /* xCommit - commit transaction */
	virtualRollback,              /* xRollback - rollback transaction */
	NULL,                         /* xFindFunction - function overloading */
	virtualRename,                /* Rename - Notification that the table will be given a new name */
};

/*
 * handle data model exceptions and return appropriate code
 */
static int
handle_data_model_exception (sqlite3_vtab *pVtab, GdaDataModel *model)
{
	GError **exceptions;
	gint i;
	exceptions = gda_data_model_get_exceptions (model);
	if (!exceptions)
		return SQLITE_OK;

	GError *trunc_error = NULL;
	GError *fatal_error = NULL;
	for (i = 0; exceptions [i]; i++) {
		GError *e;
		e = exceptions [i];
		if ((e->domain == GDA_DATA_MODEL_ERROR) &&
		    (e->code == GDA_DATA_MODEL_TRUNCATED_ERROR))
			trunc_error = e;
		else {
			fatal_error = e;
			break;
		}
	}
	if (fatal_error || trunc_error) {
		GError *e;
		e = fatal_error;
		if (!e)
			e = trunc_error;
		if (pVtab->zErrMsg)
			SQLITE3_CALL (sqlite3_free) (pVtab->zErrMsg);
		pVtab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
			("%s", e->message ? e->message : _("No detail"));
		if (fatal_error)
			return SQLITE_ERROR;
		else
			return SQLITE_IOERR_TRUNCATE;
	}
	return SQLITE_OK;
}

static GdaConnection *
gda_vprovider_data_model_create_connection (GdaServerProvider *provider)
{
	GdaConnection *cnc;
	g_return_val_if_fail (GDA_IS_VPROVIDER_DATA_MODEL (provider), NULL);

	cnc = g_object_new (GDA_TYPE_VCONNECTION_DATA_MODEL, "provider", provider, NULL);

	return cnc;
}

static gboolean
gda_vprovider_data_model_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
					  GdaQuarkList *params, GdaQuarkList *auth,
					  G_GNUC_UNUSED guint *task_id, GdaServerProviderAsyncCallback async_cb, G_GNUC_UNUSED gpointer cb_data)
{
	GdaQuarkList *m_params;

	g_return_val_if_fail (GDA_IS_VPROVIDER_DATA_MODEL (provider), FALSE);
	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), FALSE);

	if (async_cb) {
		gda_connection_add_event_string (cnc, _("Provider does not support asynchronous connection open"));
                return FALSE;
	}

	if (params) {
		m_params = gda_quark_list_copy (params);
		gda_quark_list_add_from_string (m_params, "_IS_VIRTUAL=TRUE;EXTRA_FUNCTIONS=TRUE", TRUE);
	}
	else
		m_params = gda_quark_list_new_from_string ("_IS_VIRTUAL=TRUE;EXTRA_FUNCTIONS=TRUE");

	if (! GDA_SERVER_PROVIDER_CLASS (parent_class)->open_connection (GDA_SERVER_PROVIDER (provider), cnc, m_params,
									 auth, NULL, NULL, NULL)) {
		gda_quark_list_free (m_params);
		return FALSE;
	}
	gda_quark_list_free (m_params);

	SqliteConnectionData *scnc;
	scnc = (SqliteConnectionData*) gda_connection_internal_get_provider_data ((GdaConnection *) cnc);
	if (!scnc) {
		gda_connection_close_no_warning (cnc);
		gda_connection_add_event_string (cnc,
						 _("Connection is closed"));
		return FALSE;
	}

	/* Module to declare wirtual tables */
	if (SQLITE3_CALL (sqlite3_create_module) (scnc->connection, G_OBJECT_TYPE_NAME (provider), &Module, cnc) != SQLITE_OK)
		return FALSE;
	/*g_print ("==== Declared module for DB %p\n", scnc->connection);*/

	return TRUE;
}

static void
cnc_close_foreach_func (G_GNUC_UNUSED GdaDataModel *model, const gchar *table_name, GdaVconnectionDataModel *cnc)
{
	/*g_print ("---- FOREACH: Removing virtual table '%s'\n", table_name);*/
	GError *lerror = NULL;
	if (! gda_vconnection_data_model_remove (cnc, table_name, &lerror)) {
		g_warning ("Internal GdaVproviderDataModel error: %s",
			   lerror && lerror->message ? lerror->message : _("No detail"));
		g_clear_error (&lerror);
	}
}

static gboolean
gda_vprovider_data_model_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_VPROVIDER_DATA_MODEL (provider), FALSE);
	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), FALSE);

	gda_vconnection_data_model_foreach (GDA_VCONNECTION_DATA_MODEL (cnc),
					    (GdaVconnectionDataModelFunc) cnc_close_foreach_func, cnc);

	return GDA_SERVER_PROVIDER_CLASS (parent_class)->close_connection (GDA_SERVER_PROVIDER (provider), cnc);
}

static GObject *
gda_vprovider_data_model_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
					    GdaStatement *stmt, GdaSet *params,
					    GdaStatementModelUsage model_usage,
					    GType *col_types, GdaSet **last_inserted_row,
					    guint *task_id, GdaServerProviderExecCallback async_cb,
					    gpointer cb_data, GError **error)
{
	GObject *retval;
	if (async_cb) {
                g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
                             "%s", _("Provider does not support asynchronous statement execution"));
                return NULL;
        }
        
	retval = GDA_SERVER_PROVIDER_CLASS (parent_class)->statement_execute (provider, cnc, stmt, params,
									      model_usage, col_types,
									      last_inserted_row, task_id,
									      async_cb, cb_data, error);
	if (retval) {
		gchar *sql;
		sql = gda_statement_to_sql (stmt, params, NULL);
		if (sql) {
			gchar *ptr = NULL;
			/* look for DROP TABLE to signal table removal */
			if (! g_ascii_strncasecmp (sql, "DROP", 4))
				ptr = sql + 4;
			else if (! g_ascii_strncasecmp (sql, "CREATE", 6))
				ptr = sql + 6;
			if (ptr) {
				/* skip spaces */
				for (; *ptr && (g_ascii_isspace (*ptr) || (*ptr == '\n')); ptr++);

				if (! g_ascii_strncasecmp (ptr, "TABLE", 5)) {
					/* skip spaces */
					gchar delim = 0;
					gchar *table_name, *quoted;
					for (ptr = ptr+5;
					     *ptr && (g_ascii_isspace (*ptr) || (*ptr == '\n'));
					     ptr++);
					if (*ptr == '\'') {
						delim = '\'';
						ptr++;
					}
					else if (*ptr == '"') {
						delim = '"';
						ptr++;
					}
					table_name = ptr;
					if (delim)
						for (; *ptr && (*ptr != delim) ; ptr++);
					else
						for (; *ptr && g_ascii_isalnum (*ptr); ptr++);
					*ptr = 0;
					quoted = _gda_connection_compute_table_virtual_name (GDA_CONNECTION (cnc), table_name);
					/*g_print ("%s() emits the 'vtable-dropped' signal for table [%s]\n",
					  __FUNCTION__, quoted);*/
					g_signal_emit_by_name (cnc, "vtable-dropped", quoted);
					g_free (quoted);
				}
			}
			g_free (sql);
		}
	}
	return retval;
}

static const gchar *
gda_vprovider_data_model_get_name (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return "Virtual";
}

static int
virtualCreate (sqlite3 *db, void *pAux, int argc, const char *const *argv, sqlite3_vtab **ppVtab, char **pzErr)
{
	GdaVconnectionDataModel *cnc = GDA_VCONNECTION_DATA_MODEL (pAux);
	GString *sql;
	gint i, ncols;
	gchar *spec_name, *tmp;
	GdaVConnectionTableData *td;
	GHashTable *hash;

	TRACE (NULL, NULL);

	/* find Spec */
	g_assert (argc == 4);
	spec_name = g_strdup (argv[3]);
	i = strlen (spec_name);
	if (spec_name [i-1] == '\'')
		spec_name [i-1] = 0;
	if (*spec_name == '\'')
		memmove (spec_name, spec_name+1, i);

	td = gda_vconnection_get_table_data_by_unique_name (cnc, spec_name);
	g_free (spec_name);
	if (!td) {
		/* wrong usage! */
		*pzErr = SQLITE3_CALL (sqlite3_mprintf) (_("Wrong usage of Libgda's virtual tables"));
		return SQLITE_ERROR;
	}

	/* preparations */
	if (td->spec->data_model) {
		ncols = gda_data_model_get_n_columns (td->spec->data_model);
		if (ncols <= 0) {
			*pzErr = SQLITE3_CALL (sqlite3_mprintf) (_("Data model must have at least one column"));
			return SQLITE_ERROR;
		}
		td->real_model = td->spec->data_model;
		g_object_ref (td->real_model);
	}

	GError *error = NULL;
	if (!td->columns && td->spec->create_columns_func)
		td->columns = td->spec->create_columns_func (td->spec, &error);
	if (!td->columns) {
		if (error && error->message) {
			int len = strlen (error->message) + 1;
			*pzErr = SQLITE3_CALL (sqlite3_malloc) (sizeof (gchar) * len);
			memcpy (*pzErr, error->message, len); /* Flawfinder: ignore */
		}
		else 
			*pzErr = SQLITE3_CALL (sqlite3_mprintf) (_("Could not compute virtual table's columns"));
		return SQLITE_ERROR;
	}
	ncols = g_list_length (td->columns);
	td->n_columns = ncols;

	/* create the CREATE TABLE statement */
	sql = g_string_new ("CREATE TABLE ");
	tmp = gda_connection_quote_sql_identifier (GDA_CONNECTION (cnc), argv[2]);
	g_string_append (sql, tmp);
	g_free (tmp);
	g_string_append (sql, " (");
	hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	for (i = 0; i < ncols; i++) {
		GdaColumn *column;
		const gchar *name, *type;
		GType gtype;
		gchar *newcolname, *tmp;

		if (i != 0)
			g_string_append (sql, ", ");
		column = g_list_nth_data (td->columns, i);
		if (!column) {
			*pzErr = SQLITE3_CALL (sqlite3_mprintf) (_("Can't get data model description for column %d"), i);
			g_string_free (sql, TRUE);
			return SQLITE_ERROR;
		}

		name = gda_column_get_name (column);
		if (!name || !(*name))
			newcolname = g_strdup_printf ("_%d", i + 1);
		else
			newcolname = gda_sql_identifier_quote (name, GDA_CONNECTION (cnc), NULL, FALSE, FALSE);

		tmp = g_ascii_strdown (newcolname, -1);
		if (g_hash_table_lookup (hash, tmp)) {
			gint i;
			g_free (tmp);
			for (i = 0; ; i++) {
				gchar *nc2;
				nc2 = g_strdup_printf ("%s%d", newcolname, i);
				tmp = g_ascii_strdown (nc2, -1);
				if (! g_hash_table_lookup (hash, tmp)) {
					g_free (newcolname);
					newcolname = nc2;
					break;
				}
				g_free (tmp);
				g_free (nc2);
			}
		}

		gtype = gda_column_get_g_type (column);
		type = g_type_name (gtype);
		if (!type) {
			*pzErr = SQLITE3_CALL (sqlite3_mprintf) (_("Can't get data model's column type for column %d"), i);
			g_string_free (sql, TRUE);
			return SQLITE_ERROR;
		}
		else if (gtype == GDA_TYPE_BLOB)
			type = "blob";
		else if (gtype == GDA_TYPE_BINARY)
			type = "binary";
		else if (gtype == G_TYPE_STRING)
			type = "string";
		else if (gtype == G_TYPE_INT)
			type = "integer";
		else if (gtype == G_TYPE_UINT)
			type = "unsigned integer";
		else if ((gtype == G_TYPE_INT64) || (gtype == G_TYPE_LONG))
			type = "int64";
		else if ((gtype == G_TYPE_UINT64) || (gtype == G_TYPE_ULONG))
			type = "uint64";
		else if ((gtype == G_TYPE_DOUBLE) || (gtype == G_TYPE_FLOAT))
			type = "real";
		else if (gtype == G_TYPE_DATE)
			type = "date";
		else if (gtype == GDA_TYPE_TIME)
			type = "time";
		else if (gtype == GDA_TYPE_TIMESTAMP)
			type = "timestamp";
		else if (gtype == GDA_TYPE_SHORT)
			type = "short";
		else if (gtype == GDA_TYPE_USHORT)
			type = "unsigned short";
		else
			type = "text";

		g_string_append (sql, newcolname);
		g_hash_table_insert (hash, tmp, (gpointer) 0x01);
		g_free (newcolname);
		g_string_append_c (sql, ' ');
		g_string_append (sql, type);
		if (! gda_column_get_allow_null (column)) 
			g_string_append (sql, " NOT NULL");
		if (gtype == G_TYPE_STRING)
			g_string_append (sql, " COLLATE LOCALE");
	}
	g_hash_table_destroy (hash);

	/* add a hidden column which contains the row number of the data model */
	if (ncols != 0)
		g_string_append (sql, ", ");
	g_string_append (sql, "__gda_row_nb hidden integer");

	g_string_append_c (sql, ')');

	/* VirtualTable structure */
	VirtualTable *vtable;
	vtable = g_new0 (VirtualTable, 1);
	vtable->cnc = cnc;
	vtable->td = td;
	*ppVtab = &(vtable->base);

	if (SQLITE3_CALL (sqlite3_declare_vtab) (db, sql->str) != SQLITE_OK) {
		*pzErr = SQLITE3_CALL (sqlite3_mprintf) (_("Can't declare virtual table (%s)"), sql->str);
		g_string_free (sql, TRUE);
		g_free (vtable);
		*ppVtab = NULL;
		return SQLITE_ERROR;
	}

	/*g_print ("VIRTUAL TABLE [%p]: %s\n", vtable, sql->str);*/
	g_string_free (sql, TRUE);

	return SQLITE_OK;
}

static int
virtualConnect (sqlite3 *db, void *pAux, int argc, const char *const *argv, sqlite3_vtab **ppVtab, char **pzErr)
{
	TRACE (NULL, NULL);

	return virtualCreate (db, pAux, argc, argv, ppVtab, pzErr);
}

static int
virtualDisconnect (sqlite3_vtab *pVtab)
{
	VirtualTable *vtable = (VirtualTable *) pVtab;

	TRACE (pVtab, NULL);

	if (vtable->rowid_hash)
		g_hash_table_destroy (vtable->rowid_hash);
	g_free (vtable);
	return SQLITE_OK;
}

static int
virtualDestroy (sqlite3_vtab *pVtab)
{
	TRACE (pVtab, NULL);

	return virtualDisconnect (pVtab);
}

static int
virtualOpen (G_GNUC_UNUSED sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor)
{
	VirtualCursor *cursor;

	/* create empty cursor */
	cursor = g_new0 (VirtualCursor, 1);
	*ppCursor = (sqlite3_vtab_cursor*) cursor;
	TRACE (pVTab, cursor);

	return SQLITE_OK;
}

static int
virtualClose (sqlite3_vtab_cursor *cur)
{
	VirtualCursor *cursor = (VirtualCursor*) cur;

	TRACE (cur->pVtab, cur);

	if (cursor->iter)
		g_object_unref (cursor->iter);
	if (cursor->model)
		g_object_unref (cursor->model);

	g_free (cur);

	return SQLITE_OK;
}

static int
virtualEof (sqlite3_vtab_cursor *cur)
{
	VirtualCursor *cursor = (VirtualCursor*) cur;

	TRACE (cur->pVtab, cur);

	if (gda_data_model_iter_is_valid (cursor->iter))
		return FALSE;
	else
		return TRUE;
}

static int
virtualNext (sqlite3_vtab_cursor *cur)
{
	VirtualCursor *cursor = (VirtualCursor*) cur;
	/*VirtualTable *vtable = (VirtualTable*) cur->pVtab;*/

	TRACE (cur->pVtab, cur);

	if (gda_data_model_get_exceptions (cursor->model))
		return SQLITE_IOERR_TRUNCATE;

	if (!gda_data_model_iter_move_next (cursor->iter)) {
		if (gda_data_model_iter_is_valid (cursor->iter))
			return SQLITE_IOERR;
	}
	return handle_data_model_exception (cur->pVtab, cursor->model);
}

static int
virtualColumn (sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int i)
{
	VirtualCursor *cursor = (VirtualCursor*) cur;

	TRACE (cur->pVtab, cur);

	GdaHolder *param;
	
	if (i == ((VirtualTable*) cur->pVtab)->td->n_columns) {
		/* private hidden column, which returns the row number */
		SQLITE3_CALL (sqlite3_result_int) (ctx, gda_data_model_iter_get_row (cursor->iter));
		return SQLITE_OK;
	}

	param = gda_data_model_iter_get_holder_for_field (cursor->iter, i);
	if (!param) {
		SQLITE3_CALL (sqlite3_result_text) (ctx, _("Column not found"), -1, SQLITE_TRANSIENT);
		return SQLITE_EMPTY;
	}
	else {
		const GValue *value;
		GError *lerror = NULL;
		value = gda_holder_get_value (param);
		if (! gda_holder_is_valid_e (param, &lerror)) {
			g_hash_table_insert (error_blobs_hash, lerror, GINT_TO_POINTER (1));
			SQLITE3_CALL (sqlite3_result_blob) (ctx, lerror, sizeof (GError), NULL);
		}
		else if (!value || gda_value_is_null (value))
			SQLITE3_CALL (sqlite3_result_null) (ctx);
		else  if (G_VALUE_TYPE (value) == G_TYPE_INT) 
			SQLITE3_CALL (sqlite3_result_int) (ctx, g_value_get_int (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_INT64) 
			SQLITE3_CALL (sqlite3_result_int64) (ctx, g_value_get_int64 (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_DOUBLE) 
			SQLITE3_CALL (sqlite3_result_double) (ctx, g_value_get_double (value));
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GdaBlob *blob;
			GdaBinary *bin;
			blob = (GdaBlob *) gda_value_get_blob (value);
			bin = (GdaBinary *) blob;
			if (blob->op &&
			    (bin->binary_length != gda_blob_op_get_length (blob->op)))
				gda_blob_op_read_all (blob->op, blob);
			SQLITE3_CALL (sqlite3_result_blob) (ctx, blob->data.data, blob->data.binary_length, SQLITE_TRANSIENT);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			const GdaBinary *bin;
			bin = gda_value_get_binary (value);
			SQLITE3_CALL (sqlite3_result_blob) (ctx, bin->data, bin->binary_length, SQLITE_TRANSIENT);
		}
		else {
			gchar *str = gda_value_stringify (value);
			SQLITE3_CALL (sqlite3_result_text) (ctx, str, -1, SQLITE_TRANSIENT);
			g_free (str);
		}
		return SQLITE_OK;
	}
}

static int
virtualRowid (sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid)
{
	VirtualCursor *cursor = (VirtualCursor*) cur;
	VirtualTable *vtable = (VirtualTable*) cur->pVtab;

	TRACE (vtable, cur);

	*pRowid = gda_data_model_iter_get_row (cursor->iter);
	if (! vtable->rowid_hash || (vtable->rowid_hash_model == vtable->td->real_model)) {
		if (! vtable->rowid_hash) {
			vtable->rowid_hash = g_hash_table_new_full (g_int64_hash, g_int64_equal,
								    g_free,
								    (GDestroyNotify) g_object_unref);
			vtable->rowid_hash_model = vtable->td->real_model;
		}

		GdaRow *grow;
		gint i, ncols;
		gint64 *hid;
		ncols = g_list_length (vtable->td->columns);
		
		grow = gda_row_new (ncols);
		for (i = 0; i < ncols; i++) {
			const GValue *cvalue;
			GValue *gvalue;
			cvalue = gda_data_model_iter_get_value_at (cursor->iter, i);
			gvalue = gda_row_get_value (grow, i);
			if (cvalue) {
				gda_value_reset_with_type (gvalue, G_VALUE_TYPE (cvalue));
				g_value_copy (cvalue, gvalue);
			}
			else
				gda_row_invalidate_value (grow, gvalue);
		}
		
		hid = g_new (gint64, 1);
		*hid = *pRowid;
		g_hash_table_insert (vtable->rowid_hash, hid, grow);
	}

	return SQLITE_OK;
}

/* NEVER returns %NULL */
static GValue *
create_value_from_sqlite3_value_notype (sqlite3_value *svalue)
{
	GValue *value;
	value = g_new0 (GValue, 1);

	switch (SQLITE3_CALL (sqlite3_value_type) (svalue)) {
	case SQLITE_INTEGER:
		g_value_init (value, G_TYPE_INT64);
		g_value_set_int64 (value, SQLITE3_CALL (sqlite3_value_int64) (svalue));
		break;
	case SQLITE_FLOAT:
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, SQLITE3_CALL (sqlite3_value_double) (svalue));
		break;
	case SQLITE_BLOB: {
		GdaBinary *bin;
		g_value_init (value, GDA_TYPE_BINARY);
		bin = g_new0 (GdaBinary, 1);
		bin->binary_length = SQLITE3_CALL (sqlite3_value_bytes) (svalue);
		if (bin->binary_length > 0) {
			bin->data = g_new (guchar, bin->binary_length);
			memcpy (bin->data, SQLITE3_CALL (sqlite3_value_blob) (svalue),
				bin->binary_length);
		}
		else
			bin->binary_length = 0;
		gda_value_take_binary (value, bin);
		break;
	}
	case SQLITE_NULL:
		gda_value_set_null (value);
		break;
	case SQLITE3_TEXT:
	default:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, (gchar *) SQLITE3_CALL (sqlite3_value_text) (svalue));
		break;
	}
	return value;
}

/*
 * Returns: a new array of @argc values, and %NULL if @argc = 0
 */
static GValue **
create_gvalues_array_from_sqlite3_array (int argc, sqlite3_value **argv)
{
	GValue **array;
	gint i;
	if (argc == 0)
		return NULL;
	array = g_new (GValue *, argc);
	for (i = 0; i < argc; i++)
		array[i] = create_value_from_sqlite3_value_notype (argv[i]);
	return array;
}

static void
virtual_table_manage_real_data_model (VirtualTable *vtable, int idxNum, const char *idxStr,
				      int argc, sqlite3_value **argv)
{
	if (!vtable->td->spec->create_filtered_model_func && !vtable->td->spec->create_model_func)
		return;

	if (vtable->td->real_model) {
		g_object_unref (vtable->td->real_model);
		vtable->td->real_model = NULL;
	}

	/* actual data model creation */
	if (vtable->td->spec->create_filtered_model_func) {
		GValue **gargv;
		gargv = create_gvalues_array_from_sqlite3_array (argc, argv);
		vtable->td->real_model = vtable->td->spec->create_filtered_model_func (vtable->td->spec,
										       idxNum, idxStr,
										       argc, gargv);
		if (gargv) {
			gint i;
			for (i = 0; i < argc; i++)
				gda_value_free (gargv[i]);
			g_free (gargv);
		}
	}
	else if (vtable->td->spec->create_model_func)
		vtable->td->real_model = vtable->td->spec->create_model_func (vtable->td->spec);
	if (! vtable->td->real_model)
		return;
	
	/* columns if not yet created */
	if (! vtable->td->columns && vtable->td->spec->create_columns_func)
		vtable->td->columns = vtable->td->spec->create_columns_func (vtable->td->spec, NULL);

	if (vtable->td->columns) {
		/* columns */
		GList *list;
		guint i, ncols;
		ncols = gda_data_model_get_n_columns (vtable->td->real_model);
		g_assert (ncols == g_list_length (vtable->td->columns));
		for (i = 0, list = vtable->td->columns;
		     i < ncols;
		     i++, list = list->next) {
			GdaColumn *mcol = gda_data_model_describe_column (vtable->td->real_model, i);
			GdaColumn *ccol = (GdaColumn*) list->data;
			if (gda_column_get_g_type (mcol) == GDA_TYPE_NULL)
				gda_column_set_g_type (mcol, gda_column_get_g_type (ccol));
		}
	}

	/*g_print ("Created real model %p for table %s\n", vtable->td->real_model, vtable->td->table_name);*/
}

static int
virtualFilter (sqlite3_vtab_cursor *pVtabCursor, int idxNum, const char *idxStr, int argc, sqlite3_value **argv)
{
	VirtualCursor *cursor = (VirtualCursor*) pVtabCursor;
	VirtualTable *vtable = (VirtualTable*) pVtabCursor->pVtab;

	TRACE (pVtabCursor->pVtab, pVtabCursor);

	virtual_table_manage_real_data_model (vtable, idxNum, idxStr, argc, argv);
	if (! vtable->td->real_model)
		return SQLITE_ERROR;

	/* initialize cursor */
	if (GDA_IS_DATA_PROXY (vtable->td->real_model))
		cursor->iter = g_object_new (GDA_TYPE_DATA_MODEL_ITER,
					     "data-model", vtable->td->real_model, NULL);
	else
		cursor->iter = gda_data_model_create_iter (vtable->td->real_model);

	if (gda_data_model_iter_is_valid (cursor->iter) &&
	    (gda_data_model_iter_get_row (cursor->iter) > 0)) {
		/*g_print ("@%d, rewinding...", gda_data_model_iter_get_row (cursor->iter));*/
		for (;gda_data_model_iter_move_prev (cursor->iter););
		/*g_print ("done: @%d\n", gda_data_model_iter_get_row (cursor->iter));*/
		if (gda_data_model_iter_is_valid (cursor->iter))
			goto onerror;
	}
	if (! gda_data_model_iter_is_valid (cursor->iter)) {
		gda_data_model_iter_move_next (cursor->iter);
		if (! gda_data_model_iter_is_valid (cursor->iter))
			goto onerror;
	}
	cursor->model = g_object_ref (vtable->td->real_model);

	return handle_data_model_exception (pVtabCursor->pVtab, cursor->model);

 onerror:
	g_object_unref (cursor->iter);
	cursor->iter = NULL;
	if (pVtabCursor->pVtab->zErrMsg)
			SQLITE3_CALL (sqlite3_free) (pVtabCursor->pVtab->zErrMsg);
	pVtabCursor->pVtab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
		("%s", _("Can't obtain an iterator located on the first row"));
	return SQLITE_ERROR;
}

#ifdef GDA_DEBUG_VIRTUAL

static void
index_info_dump (sqlite3_index_info *pIdxInfo, gboolean dump_out)
{
	int nc;
	if (dump_out) {
		g_print ("Dump of OUT sqlite3_index_info [%p]\n", pIdxInfo);
		for (nc = 0; nc < pIdxInfo->nConstraint; nc++) {
			struct sqlite3_index_constraint_usage *cons;
			cons = &(pIdxInfo->aConstraintUsage[nc]);
			g_print ("sqlite3_index_constraint_usage[%d]\n", nc);
			g_print ("   argvIndex=%d\n", cons->argvIndex);
			g_print ("   omit=%d\n", cons->omit);
		}
		g_print ("idxNum=%d\n", pIdxInfo->idxNum);
		g_print ("orderByConsumed=%d\n", pIdxInfo->orderByConsumed);
	}
	else {
		g_print ("Dump of IN sqlite3_index_info [%p]\n", pIdxInfo);
		for (nc = 0; nc < pIdxInfo->nConstraint; nc++) {
			struct sqlite3_index_constraint *cons;
			cons = &(pIdxInfo->aConstraint[nc]);
			g_print ("sqlite3_index_constraint[%d]\n", nc);
			g_print ("   iColumn=%d\n", cons->iColumn);
			g_print ("   op=%d\n", cons->op);
			g_print ("   usable=%d\n", cons->usable);
			g_print ("   iTermOffset=%d\n", cons->iTermOffset);
		}
		
		for (nc = 0; nc < pIdxInfo->nOrderBy; nc++) {
			struct sqlite3_index_orderby *cons;
			cons = &(pIdxInfo->aOrderBy[nc]);
			g_print ("sqlite3_index_orderby[%d]\n", nc);
			g_print ("   iColumn=%d\n", cons->iColumn);
			g_print ("   desc=%d\n", cons->desc);
		}
	}
}
#endif

static void
map_sqlite3_info_to_gda_filter (sqlite3_index_info *info, GdaVconnectionDataModelFilter *filter)
{
	memset (filter, 0, sizeof (GdaVconnectionDataModelFilter));
	if (info->nConstraint > 0) {
		gint i, j;
		filter->aConstraint = g_new (struct GdaVirtualConstraint, info->nConstraint);
		filter->aConstraintUsage = g_new (struct GdaVirtualConstraintUsage, info->nConstraint);
		for (i = 0, j = 0; i < info->nConstraint; i++) {
			if (! info->aConstraint[i].usable)
				continue;

			filter->aConstraint[j].iColumn = info->aConstraint[i].iColumn;
			switch (info->aConstraint[i].op) {
			case SQLITE_INDEX_CONSTRAINT_EQ:
				filter->aConstraint[j].op = GDA_SQL_OPERATOR_TYPE_EQ;
				break;
			case SQLITE_INDEX_CONSTRAINT_GT:
				filter->aConstraint[j].op = GDA_SQL_OPERATOR_TYPE_GT;
				break;
			case SQLITE_INDEX_CONSTRAINT_LE:
				filter->aConstraint[j].op = GDA_SQL_OPERATOR_TYPE_LEQ;
				break;
			case SQLITE_INDEX_CONSTRAINT_LT:
				filter->aConstraint[j].op = GDA_SQL_OPERATOR_TYPE_LT;
				break;
			case SQLITE_INDEX_CONSTRAINT_GE:
				filter->aConstraint[j].op = GDA_SQL_OPERATOR_TYPE_GEQ;
				break;
			case SQLITE_INDEX_CONSTRAINT_MATCH:
				filter->aConstraint[j].op = GDA_SQL_OPERATOR_TYPE_REGEXP;
				break;
			default:
				g_assert_not_reached ();
			}

			filter->aConstraintUsage[j].argvIndex = 0;
			filter->aConstraintUsage[j].omit = FALSE;
			j++;
		}
		filter->nConstraint = j;
	}
	filter->nOrderBy = info->nOrderBy;
	if (filter->nOrderBy > 0) {
		gint i;
		filter->aOrderBy = g_new (struct GdaVirtualOrderby, filter->nOrderBy);
		for (i = 0; i < info->nOrderBy; i++) {
			filter->aOrderBy[i].iColumn = info->aOrderBy[i].iColumn;
			filter->aOrderBy[i].desc = info->aOrderBy[i].desc ? TRUE : FALSE;
		}
	}
	filter->idxNum = 0;
	filter->idxPointer = NULL;
	filter->orderByConsumed = FALSE;
	filter->estimatedCost = info->estimatedCost;
}

/*
 * Also frees @filter's dynamically allocated parts
 */
static void
map_consume_gda_filter_to_sqlite3_info (GdaVconnectionDataModelFilter *filter, sqlite3_index_info *info)
{
	g_assert (filter->nConstraint == info->nConstraint);
	if (info->nConstraint > 0) {
		gint i, j;
		for (i = 0, j = 0; i < info->nConstraint; i++) {
			if (! info->aConstraint[i].usable)
				continue;
			info->aConstraintUsage[i].argvIndex = filter->aConstraintUsage[j].argvIndex;
			info->aConstraintUsage[i].omit = filter->aConstraintUsage[j].omit ? 1 : 0;
			j++;
		}
		g_free (filter->aConstraint);
		g_free (filter->aConstraintUsage);

	}
	if (filter->nOrderBy > 0)
		g_free (filter->aOrderBy);
	info->idxNum = filter->idxNum;
	info->idxStr = (char*) filter->idxPointer;
	info->needToFreeIdxStr = 0;
	info->orderByConsumed = filter->orderByConsumed ? 1 : 0;
	info->estimatedCost = filter->estimatedCost;
}

static int
virtualBestIndex (sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo)
{
	VirtualTable *vtable = (VirtualTable *) tab;

	TRACE (tab, NULL);
#ifdef GDA_DEBUG_VIRTUAL
	index_info_dump (pIdxInfo, FALSE);
#endif

	if (vtable->td->spec->create_filter_func) {
		GdaVconnectionDataModelFilter filter;
		map_sqlite3_info_to_gda_filter (pIdxInfo, &filter);
		vtable->td->spec->create_filter_func (vtable->td->spec, &filter);
		map_consume_gda_filter_to_sqlite3_info (&filter, pIdxInfo);
#ifdef GDA_DEBUG_VIRTUAL
		index_info_dump (pIdxInfo, TRUE);
#endif
	}

	return SQLITE_OK;
}

/*
 *
 * Returns: >= 0 if Ok, -1 on error
 */
static gint
param_name_to_number (gint maxrows, const gchar *str)
{
	gchar *endptr [1];
	long int i;
	i = strtol (str, endptr, 10);
	if ((**endptr == '\0') && (i < maxrows) && (i >= 0))
		return i;
	else
		return -1;
}

/*
 *    apData[0]  apData[1]  apData[2..]
 *
 *    INTEGER                              DELETE            
 *
 *    INTEGER    NULL       (nCol args)    UPDATE (do not set rowid)
 *    INTEGER    INTEGER    (nCol args)    UPDATE (with SET rowid = <arg1>)
 *
 *    NULL       NULL       (nCol args)    INSERT INTO (automatic rowid value)
 *    NULL       INTEGER    (nCol args)    INSERT (incl. rowid value)
 */
static int
virtualUpdate (sqlite3_vtab *tab, int nData, sqlite3_value **apData, sqlite_int64 *pRowid)
{
	VirtualTable *vtable = (VirtualTable *) tab;
	const gchar *api_misuse_error = NULL;
	gint optype; /* 1 => DELETE
		      * 2 => INSERT
		      * 3 => UPDATE
		      */

	TRACE (tab, NULL);

	/* determine operation type */
	if (nData == 1)
		optype = 1;
	else if ((nData > 1) && (SQLITE3_CALL (sqlite3_value_type) (apData[0]) == SQLITE_NULL)) {
		optype = 2;
		if (SQLITE3_CALL (sqlite3_value_type) (apData[1]) != SQLITE_NULL) {
			/* argc>1 and argv[0] is not NULL: rowid is imposed by SQLite
			 * which is not supported */
			return SQLITE_READONLY;
		}
	}
	else if ((nData > 1) && (SQLITE3_CALL (sqlite3_value_type) (apData[0]) == SQLITE_INTEGER)) {
		optype = 3;
		if (SQLITE3_CALL (sqlite3_value_int) (apData[0]) != 
		    SQLITE3_CALL (sqlite3_value_int) (apData[1])) {
			/* argc>1 and argv[0]==argv[1]: rowid is imposed by SQLite 
			 * which is not supported */
			return SQLITE_READONLY;
		}
	}
	else
		return SQLITE_MISUSE;

	/* handle data model */
	if (! vtable->td->real_model) {
		virtual_table_manage_real_data_model (vtable, -1, NULL, 0, NULL);
		if (! vtable->td->real_model)
			return SQLITE_ERROR;
	}

	GdaDataModelAccessFlags access_flags;
	access_flags = gda_data_model_get_access_flags (vtable->td->real_model);
	if (((optype == 1) && ! (access_flags & GDA_DATA_MODEL_ACCESS_DELETE)) ||
	    ((optype == 2) && ! (access_flags & GDA_DATA_MODEL_ACCESS_INSERT)) ||
	    ((optype == 3) && ! (access_flags & GDA_DATA_MODEL_ACCESS_UPDATE))) {
		/* we can't use vtable->td->real_model because it can't be accessed correctly */
		if (! GDA_IS_DATA_SELECT (vtable->td->real_model)) {
			tab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
				(_("Data model representing the table is read only"));
			return SQLITE_READONLY;
		}
		
		/* determine parameters required to execute MOD statement */
		GdaStatement *stmt = NULL;
		ParamType ptype;
		switch (optype) {
		case 1:
			ptype = PARAMS_DELETE;
			if (! vtable->td->modif_stmt [ptype])
				g_object_get (vtable->td->real_model, "delete-stmt", &stmt, NULL);
			break;
		case 2:
			ptype = PARAMS_INSERT;
			if (! vtable->td->modif_stmt [ptype])
				g_object_get (vtable->td->real_model, "insert-stmt", &stmt, NULL);
			break;
		case 3:
			ptype = PARAMS_UPDATE;
			if (! vtable->td->modif_stmt [ptype])
				g_object_get (vtable->td->real_model, "update-stmt", &stmt, NULL);
			break;
		default:
			g_assert_not_reached ();
		}
		
		if (! vtable->td->modif_stmt [ptype]) {
			if (! stmt) {
				tab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
					(_("No statement provided to modify the data"));
				return SQLITE_READONLY;
			}
		
			GdaSet *params;
			if (! gda_statement_get_parameters (stmt, &params, NULL) || !params) {
				tab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
					(_("Invalid statement provided to modify the data"));
				g_object_unref (stmt);
				return SQLITE_READONLY;
			}
			vtable->td->modif_stmt [ptype] = stmt;
			vtable->td->modif_params [ptype] = params;
		}
		stmt = vtable->td->modif_stmt [ptype];
		
		/* bind parameters */
		GSList *list;
		for (list = vtable->td->modif_params [ptype]->holders; list; list = list->next) {
			const gchar *id;
			GdaHolder *holder = GDA_HOLDER (list->data);
			gboolean holder_value_set = FALSE;
			
			id = gda_holder_get_id (holder);
			if (!id) {
				tab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
					(_("Invalid parameter in statement to modify the data"));
				return SQLITE_READONLY;
			}
			if (*id == '+' && id[1]) {
				long int i;
				i = param_name_to_number (vtable->td->n_columns, id+1);
				if (i >= 0) {
					GType type;
					GValue *value;
					type = gda_column_get_g_type (gda_data_model_describe_column (vtable->td->real_model, i));
					if ((type != GDA_TYPE_NULL) && SQLITE3_CALL (sqlite3_value_text) (apData [i+2]))
						value = gda_value_new_from_string ((const gchar*) SQLITE3_CALL (sqlite3_value_text) (apData [i+2]), type);
					else
						value = gda_value_new_null ();
					if (gda_holder_take_value (holder, value, NULL))
						holder_value_set = TRUE;
				}
			}
			else if (*id == '-') {
				if (! vtable->rowid_hash) {
					tab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
						(_("Could not retreive row to delete"));
					return SQLITE_READONLY;
				}

				gint64 rowid = SQLITE3_CALL (sqlite3_value_int64) (apData [0]);
				GdaRow *grow = NULL;
				if (vtable->rowid_hash_model == vtable->td->real_model)
					grow = g_hash_table_lookup (vtable->rowid_hash, &rowid);
				if (!grow) {
					tab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
						(_("Could not retreive row to delete"));
					return SQLITE_READONLY;
				}

				long int i;
				i = param_name_to_number (vtable->td->n_columns, id+1);
				if (i >= 0) {
					GValue *value;
					value = gda_row_get_value (grow, i);
					if (gda_holder_set_value (holder, value, NULL))
						holder_value_set = TRUE;
				}
			}
			
			if (! holder_value_set) {
				GdaSet *exec_set;
				GdaHolder *eh;
				g_object_get (vtable->td->real_model, "exec-params",
					      &exec_set, NULL);
				if (! exec_set) {
					/* can't give value to param named @id */
					tab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
						(_("Invalid parameter in statement to modify the data"));
					return SQLITE_READONLY;
				}
				eh = gda_set_get_holder (exec_set, id);
				if (! eh ||
				    ! gda_holder_set_bind (holder, eh, NULL)) {
					/* can't give value to param named @id */
					tab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
						(_("Invalid parameter in statement to modify the data"));
					return SQLITE_READONLY;
				}
			}
		}

		GdaConnection *cnc;
		cnc = gda_data_select_get_connection (GDA_DATA_SELECT (vtable->td->real_model));

		GError *lerror = NULL;
#ifdef GDA_DEBUG_NO
		gchar *sql;
		sql = gda_statement_to_sql (stmt, NULL, NULL);
		g_print ("SQL: [%s] ", sql);
		g_free (sql);
		sql = gda_statement_to_sql_extended (stmt, cnc, vtable->td->modif_params [ptype], GDA_STATEMENT_SQL_PRETTY, NULL, &lerror);
		if (sql) {
			g_print ("With params: [%s]\n", sql);
			g_free (sql);
		}
		else {
			g_print ("params ERROR [%s]\n", lerror && lerror->message ? lerror->message : "No detail");
		}
		g_clear_error (&lerror);
#endif
		
		if (!cnc ||
		    (gda_connection_statement_execute_non_select (cnc, stmt,
								  vtable->td->modif_params [ptype],
								  NULL, &lerror) == -1)) {
			/* failed to execute */
			tab->zErrMsg = SQLITE3_CALL (sqlite3_mprintf)
				(_("Failed to modify data: %s"),
				 lerror && lerror->message ? lerror->message : _("No detail"));
			g_clear_error (&lerror);
			return SQLITE_READONLY;
		}
		return SQLITE_OK;
	}

	/* REM: when using the values of apData[], the limit is
	 * (nData -1 ) and not nData because the last column of the corresponding CREATE TABLE ...
	 * is an internal hidden field which does not correspond to any column of the real data model
	 */

	if (optype == 1) {
		/* DELETE */
		if (SQLITE3_CALL (sqlite3_value_type) (apData[0]) == SQLITE_INTEGER) {
			gint rowid = SQLITE3_CALL (sqlite3_value_int) (apData [0]);
			return gda_data_model_remove_row (vtable->td->real_model, rowid, NULL) ?
				SQLITE_OK : SQLITE_READONLY;
		}
		else {
			api_misuse_error = "argc==1 and argv[0] is not an integer";
			goto api_misuse;
		}
	}
	else if (optype == 2) {
		/* INSERT */
		gint newrow, i;
		GList *values = NULL;
		
		for (i = 2; i < (nData - 1); i++) {
			GType type;
			GValue *value;
			type = gda_column_get_g_type (gda_data_model_describe_column (vtable->td->real_model, i - 2));
			if ((type != GDA_TYPE_NULL) && SQLITE3_CALL (sqlite3_value_text) (apData [i]))
				value = gda_value_new_from_string ((const gchar*) SQLITE3_CALL (sqlite3_value_text) (apData [i]), type);
			else
				value = gda_value_new_null ();
			/*g_print ("TXT #%s# => value %p (type=%s) apData[]=%p\n",
			  SQLITE3_CALL (sqlite3_value_text) (apData [i]), value,
			  g_type_name (type), apData[i]);*/
			values = g_list_prepend (values, value);
		}
		values = g_list_reverse (values);

		newrow = gda_data_model_append_values (vtable->td->real_model, values, NULL);
		g_list_foreach (values, (GFunc) gda_value_free, NULL);
		g_list_free (values);
		if (newrow < 0)
			return SQLITE_READONLY;

		*pRowid = newrow;
	}
	else if (optype == 3) {
		/* UPDATE */
		gint i;

		
		for (i = 2; i < (nData - 1); i++) {
			GValue *value;
			GType type;
			gint rowid = SQLITE3_CALL (sqlite3_value_int) (apData [0]);
			gboolean res;
			GError *error = NULL;

			/*g_print ("%d => %s\n", i, SQLITE3_CALL (sqlite3_value_text) (apData [i]));*/
			type = gda_column_get_g_type (gda_data_model_describe_column (vtable->td->real_model, i - 2));
			value = gda_value_new_from_string ((const gchar*) SQLITE3_CALL (sqlite3_value_text) (apData [i]), type);
			res = gda_data_model_set_value_at (vtable->td->real_model, i - 2, rowid,
							   value, &error);
			gda_value_free (value);
			if (!res) {
				g_print ("Error: %s\n", error && error->message ? error->message : "???");
				return SQLITE_READONLY;
			}
		}
		return SQLITE_OK;
	}
	else {
		api_misuse_error = "argc>1 and argv[0] is not NULL and not an integer";
		goto api_misuse;
	}

	return SQLITE_OK;

 api_misuse:
	g_warning ("Error in the xUpdate SQLite's virtual method: %s\n"
		   "this is an SQLite error, please report it", api_misuse_error);
	return SQLITE_ERROR;
}

static int
virtualBegin (G_GNUC_UNUSED sqlite3_vtab *tab)
{
	TRACE (tab, NULL);
	/* no documentation currently available, don't do anything */
	return SQLITE_OK;
}

static int
virtualSync (G_GNUC_UNUSED sqlite3_vtab *tab)
{
	TRACE (tab, NULL);
	/* no documentation currently available, don't do anything */
	return SQLITE_OK;
}

static int
virtualCommit (G_GNUC_UNUSED sqlite3_vtab *tab)
{
	VirtualTable *vtable = (VirtualTable *) tab;
	TRACE (tab, NULL);

	if (vtable->rowid_hash) {
		g_hash_table_destroy (vtable->rowid_hash);
		vtable->rowid_hash = NULL;
		vtable->rowid_hash_model = NULL;
	}

	return SQLITE_OK;
}

static int
virtualRollback (G_GNUC_UNUSED sqlite3_vtab *tab)
{	
	TRACE (tab, NULL);
	/* no documentation currently available, don't do anything */
	return SQLITE_OK;
}

static int
virtualRename (G_GNUC_UNUSED sqlite3_vtab *pVtab, G_GNUC_UNUSED const char *zNew)
{
	TRACE (pVtab, NULL);
	/* not yet analysed and implemented */
	return SQLITE_OK;
}
