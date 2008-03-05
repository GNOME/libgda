/* 
 * GDA common library
 * Copyright (C) 2007 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "gda-vprovider-data-model.h"
#include "gda-vconnection-data-model.h"
#include "gda-vconnection-data-model-private.h"
#include <sqlite3.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-data-proxy.h>
#include <libgda/gda-data-access-wrapper.h>
#include <libgda/gda-blob-op.h>
#include "../gda-sqlite.h"


struct _GdaVproviderDataModelPrivate {
	
};

/* properties */
enum
{
        PROP_0,
};

static void gda_vprovider_data_model_class_init (GdaVproviderDataModelClass *klass);
static void gda_vprovider_data_model_init       (GdaVproviderDataModel *prov, GdaVproviderDataModelClass *klass);
static void gda_vprovider_data_model_finalize   (GObject *object);
static void gda_vprovider_data_model_set_property (GObject *object,
					       guint param_id,
					       const GValue *value,
					       GParamSpec *pspec);
static void gda_vprovider_data_model_get_property (GObject *object,
					       guint param_id,
					       GValue *value,
					       GParamSpec *pspec);
static GObjectClass  *parent_class = NULL;

static GdaConnection *gda_vprovider_data_model_create_connection (GdaServerProvider *provider);
static gboolean       gda_vprovider_data_model_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
								GdaQuarkList *params, GdaQuarkList *auth,
								guint *task_id, GdaServerProviderAsyncCallback async_cb, 
								gpointer cb_data);
static gboolean       gda_vprovider_data_model_close_connection (GdaServerProvider *provider,
								 GdaConnection *cnc);
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

	server_class->get_name = gda_vprovider_data_model_get_name;

	/* explicitely unimplement the DDL queries */
	server_class->supports_operation = NULL;
        server_class->create_operation = NULL;
        server_class->render_operation = NULL;
        server_class->perform_operation = NULL;

	/* Properties */
        object_class->set_property = gda_vprovider_data_model_set_property;
        object_class->get_property = gda_vprovider_data_model_get_property;
}

static void
gda_vprovider_data_model_init (GdaVproviderDataModel *prov, GdaVproviderDataModelClass *klass)
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
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaVproviderDataModelClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_vprovider_data_model_class_init,
				NULL, NULL,
				sizeof (GdaVproviderDataModel),
				0,
				(GInstanceInitFunc) gda_vprovider_data_model_init
			};
			
			type = g_type_register_static (GDA_TYPE_VIRTUAL_PROVIDER, "GdaVproviderDataModel", &info, 0);
		}
	}

	return type;
}

static void
gda_vprovider_data_model_set_property (GObject *object,
				       guint param_id,
				       const GValue *value,
				       GParamSpec *pspec)
{
        GdaVproviderDataModel *prov;

        prov = GDA_VPROVIDER_DATA_MODEL (object);
        if (prov->priv) {
                switch (param_id) {
		default:
			break;
                }
        }
}

static void
gda_vprovider_data_model_get_property (GObject *object,
				       guint param_id,
				       GValue *value,
				       GParamSpec *pspec)
{
        GdaVproviderDataModel *prov;

        prov = GDA_VPROVIDER_DATA_MODEL (object);
        if (prov->priv) {
		switch (param_id) {
		default:
			break;
		}
        }
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
};

static GdaConnection *
gda_vprovider_data_model_create_connection (GdaServerProvider *provider)
{
	GdaConnection *cnc;
	g_return_val_if_fail (GDA_IS_VPROVIDER_DATA_MODEL (provider), NULL);

	cnc = g_object_new (GDA_TYPE_VCONNECTION_DATA_MODEL, "provider-obj", provider, NULL);

	return cnc;
}

static gboolean
gda_vprovider_data_model_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
					  GdaQuarkList *params, GdaQuarkList *auth,
					  guint *task_id, GdaServerProviderAsyncCallback async_cb, gpointer cb_data)
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
		gda_quark_list_add_from_string (m_params, "_IS_VIRTUAL=TRUE;LOAD_GDA_FUNCTIONS=TRUE", TRUE);
	}
	else
		params = gda_quark_list_new_from_string ("_IS_VIRTUAL=TRUE;LOAD_GDA_FUNCTIONS=TRUE");

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

		return FALSE;
	}

	/* Module to declare wirtual tables */
	if (sqlite3_create_module (scnc->connection, G_OBJECT_TYPE_NAME (provider), &Module, cnc) != SQLITE_OK)
		return FALSE;
	/*g_print ("==== Declared module for DB %p\n", scnc->connection);*/

	return TRUE;
}

static void
cnc_close_foreach_func (GdaDataModel *model, const gchar *table_name, GdaVconnectionDataModel *cnc)
{
	/*g_print ("---- FOREACH: Removing virtual table '%s'\n", table_name);*/
	if (! gda_vconnection_data_model_remove (cnc, table_name, NULL))
		g_warning ("Internal GdaVproviderDataModel error");
}

static gboolean
gda_vprovider_data_model_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_VPROVIDER_DATA_MODEL (provider), FALSE);
	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), FALSE);

	gda_vconnection_data_model_foreach (GDA_VCONNECTION_DATA_MODEL (cnc),
					    (GdaVConnectionDataModelFunc) cnc_close_foreach_func, cnc);

	return GDA_SERVER_PROVIDER_CLASS (parent_class)->close_connection (GDA_SERVER_PROVIDER (provider), cnc);
}

static const gchar *
gda_vprovider_data_model_get_name (GdaServerProvider *provider)
{
	return "Virtual";
}

/* module implementation */
#define TRACE() g_print ("== %s()\n", __FUNCTION__)
#undef TRACE
#define TRACE()

typedef struct {
	sqlite3_vtab                 base;
	GdaVconnectionDataModel     *cnc;
	GdaDataProxy                *proxy;
	GdaVConnectionTableData     *td;
} VirtualTable;

typedef struct {
	sqlite3_vtab_cursor      base;
	GdaDataModelIter        *iter;
	gint                     ncols;
} VirtualCursor;

static void virtual_table_manage_real_data_model (VirtualTable *vtable);

static int
virtualCreate (sqlite3 *db, void *pAux, int argc, const char *const *argv, sqlite3_vtab **ppVtab, char **pzErr)
{
	GdaVconnectionDataModel *cnc = GDA_VCONNECTION_DATA_MODEL (pAux);
	GdaDataProxy *proxy = NULL;
	GString *sql;
	gint i, ncols;
	gchar *spec_name;
	GList *columns = NULL;
	GdaVConnectionTableData *td;

	TRACE ();

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
	g_assert (td);

	/* preparations */
	if (td->spec->data_model) {
		if (GDA_IS_DATA_PROXY (td->spec->data_model)) {
			proxy = (GdaDataProxy *) (td->spec->data_model);
			g_object_ref (G_OBJECT (proxy));
		}
		else {
			if (gda_data_model_get_access_flags (td->spec->data_model) & GDA_DATA_MODEL_ACCESS_RANDOM)
				proxy = g_object_new (GDA_TYPE_DATA_PROXY, 
						      "model", td->spec->data_model, 
						      "sample_size", 0, NULL);
			else {
				/* no random access => use a wrapper */
				GdaDataModel *wrapper;
				wrapper = gda_data_access_wrapper_new (td->spec->data_model);
				proxy = (GdaDataProxy *) gda_data_proxy_new (wrapper);
				g_object_unref (wrapper);
			}
		}
		ncols = gda_data_proxy_get_proxied_model_n_cols (proxy);

		/* proxy settings */
		gda_data_proxy_set_sample_size (proxy, 0);
		g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, NULL);
		
		if (ncols <= 0) {
			*pzErr = sqlite3_mprintf (_("Data model must have at least one column"));
			g_object_unref (proxy);
			return SQLITE_ERROR;
		}
		td->real_model = td->spec->data_model;
		g_object_ref (td->real_model);
	}
	else  {
		columns = td->spec->create_columns_func (td->spec);
		ncols = g_list_length (columns);
	}

	/* create the CREATE TABLE statement */
	sql = g_string_new ("CREATE TABLE ");
	g_string_append (sql, argv[2]);
	g_string_append (sql, " (");
	for (i = 0; i < ncols; i++) {
		GdaColumn *column;
		const gchar *name, *type;

		if (i != 0)
			g_string_append (sql, ", ");
		if (columns)
			column = g_list_nth_data (columns, i);
		else
			column = gda_data_model_describe_column ((GdaDataModel*) proxy, i);
		if (!column) {
			*pzErr = sqlite3_mprintf (_("Can't get data model description for column %d"), i);
			g_string_free (sql, TRUE);
			return SQLITE_ERROR;
		}
		name = gda_column_get_name (column);
		type = gda_column_get_dbms_type (column);
		if (!type) 
			type = g_type_name (gda_column_get_g_type (column));
		if (!name) {
			*pzErr = sqlite3_mprintf (_("Can't get data model's column name for column %d"), i);
			g_string_free (sql, TRUE);
			return SQLITE_ERROR;
		}
		if (!type) {
			*pzErr = sqlite3_mprintf (_("Can't get data model's column type or type for column %d"), i);
			g_string_free (sql, TRUE);
			return SQLITE_ERROR;
		}
		else if (!strcmp (type, "GdaBlob") || !strcmp (type, "GdaBinary"))
			type = "blob";
		else if (!strcmp (type, "gchararray"))
			type = "text";
		else if (!strncmp (type, "gint", 4))
			type = "integer";
		else if (!strcmp (type, "gdouble"))
			type = "real";
		else if (!strcmp (type, "gfloat"))
			type = "real";
		else {
			/* use the declared GType */
			type = g_type_name (gda_column_get_g_type (column));
			if (!strcmp (type, "GdaBlob") || !strcmp (type, "GdaBinary"))
				type = "blob";
			else if (!strcmp (type, "gchararray"))
				type = "text";
			else if (!strncmp (type, "gint", 4))
				type = "integer";
			else if (!strcmp (type, "gdouble"))
				type = "real";
			else if (!strcmp (type, "gfloat"))
				type = "real";
			else
				type = "text";
		}

		g_string_append (sql, name);
		g_string_append_c (sql, ' ');
		g_string_append (sql, type);
		if (! gda_column_get_allow_null (column)) 
			g_string_append (sql, " NOT NULL");
	}

	/* add a hidden column which contains the row number of the data model */
	if (ncols != 0)
		g_string_append (sql, ", ");
	g_string_append (sql, "__gda_row_nb hidden integer");

	g_string_append_c (sql, ')');
	if (columns) {
		g_list_foreach (columns, (GFunc) g_object_unref, NULL);
		g_list_free (columns);
	}

	/* VirtualTable structure */
	VirtualTable *vtable;
	vtable = g_new0 (VirtualTable, 1);
	vtable->cnc = cnc;
	vtable->proxy = proxy;
	vtable->td = td;
	*ppVtab = &(vtable->base);

	if (sqlite3_declare_vtab (db, sql->str) != SQLITE_OK) {
		*pzErr = sqlite3_mprintf (_("Can't declare virtual table (%s)"), sql->str);
		g_string_free (sql, TRUE);
		g_free (vtable);
		*ppVtab = NULL;
		return SQLITE_ERROR;
	}

	/*g_print ("VIRTUAL TABLE: %s\n", sql->str);*/
	g_string_free (sql, TRUE);

	return SQLITE_OK;
}

static int
virtualConnect (sqlite3 *db, void *pAux, int argc, const char *const *argv, sqlite3_vtab **ppVtab, char **pzErr)
{
	TRACE ();

	return virtualCreate (db, pAux, argc, argv, ppVtab, pzErr);
}

static int
virtualDisconnect (sqlite3_vtab *pVtab)
{
	VirtualTable *vtable = (VirtualTable *) pVtab;

	TRACE ();

	if (vtable->proxy)
		g_object_unref (vtable->proxy);
	g_free (vtable);
	return SQLITE_OK;
}

static int
virtualDestroy (sqlite3_vtab *pVtab)
{
	TRACE ();

	return virtualDisconnect (pVtab);
}

static void
virtual_table_manage_real_data_model (VirtualTable *vtable)
{
	if (vtable->td->spec->create_model_func) {
		if (vtable->td->real_model)
			g_object_unref (vtable->td->real_model);
		if (vtable->proxy)
			g_object_unref (vtable->proxy);

		vtable->td->real_model = vtable->td->spec->create_model_func (vtable->td->spec);
		/*g_print ("Created real model %p for table %s\n", vtable->td->real_model, vtable->td->table_name);*/
		
		if (GDA_IS_DATA_PROXY (vtable->td->real_model)) {
			vtable->proxy = (GdaDataProxy *) (vtable->td->real_model);
			g_object_ref (G_OBJECT (vtable->proxy));
		}
		else 
			vtable->proxy = (GdaDataProxy *) gda_data_proxy_new (vtable->td->real_model);

		/* proxy settings */
		gda_data_proxy_set_sample_size (vtable->proxy, 0);
		g_object_set (G_OBJECT (vtable->proxy), "defer-sync", FALSE, NULL);
	}
}

static int
virtualOpen (sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor)
{
	VirtualCursor *cursor;
	VirtualTable *vtable = (VirtualTable*) pVTab;

	TRACE ();

	virtual_table_manage_real_data_model (vtable);

	cursor = g_new0 (VirtualCursor, 1);
	cursor->iter = gda_data_model_iter_new (GDA_DATA_MODEL (vtable->proxy));
	cursor->ncols = gda_data_model_get_n_columns (GDA_DATA_MODEL (vtable->td->real_model));
	*ppCursor = &(cursor->base);
	return SQLITE_OK;
}

static int
virtualClose (sqlite3_vtab_cursor *cur)
{
	VirtualCursor *cursor = (VirtualCursor*) cur;

	TRACE ();

	g_object_unref (cursor->iter);
	/* FIXME: destroy table->spec->model and table->proxy */
	g_free (cur);
	return SQLITE_OK;
}

static int
virtualEof (sqlite3_vtab_cursor *cur)
{
	VirtualCursor *cursor = (VirtualCursor*) cur;

	TRACE ();

	if (gda_data_model_iter_is_valid (cursor->iter))
		return FALSE;
	else
		return TRUE;
}

static int
virtualNext (sqlite3_vtab_cursor *cur)
{
	VirtualCursor *cursor = (VirtualCursor*) cur;
	VirtualTable *vtable = (VirtualTable*) cur->pVtab;

	TRACE ();

	gda_data_model_iter_move_next (cursor->iter);
	while (gda_data_model_iter_is_valid (cursor->iter) &&
	       gda_data_proxy_row_is_deleted (vtable->proxy, gda_data_model_iter_get_row (cursor->iter)))
		gda_data_model_iter_move_next (cursor->iter);
	return SQLITE_OK;
}

static int
virtualColumn (sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int i)
{
	VirtualCursor *cursor = (VirtualCursor*) cur;

	TRACE ();

	GdaHolder *param;
	
	if (i == cursor->ncols) {
		/* private hidden column, which returns the row number */
		sqlite3_result_int (ctx, gda_data_model_iter_get_row (cursor->iter));
		return SQLITE_OK;
	}

	param = gda_data_model_iter_get_param_for_column (cursor->iter, i);
	if (!param) {
		sqlite3_result_error (ctx, _("Column not found"), -1);
		return SQLITE_EMPTY;
	}
	else {
		const GValue *value;
		value = gda_holder_get_value (param);

		if (!value || gda_value_is_null (value))
			sqlite3_result_null (ctx);
		else  if (G_VALUE_TYPE (value) == G_TYPE_INT) 
			sqlite3_result_int (ctx, g_value_get_int (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_INT64) 
			sqlite3_result_int64 (ctx, g_value_get_int64 (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_DOUBLE) 
			sqlite3_result_double (ctx, g_value_get_double (value));
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GdaBlob *blob;
			blob = (GdaBlob *) gda_value_get_blob (value);
			if (blob->op)
				gda_blob_op_read_all (blob->op, blob);
			sqlite3_result_blob (ctx, blob->data.data, blob->data.binary_length, SQLITE_TRANSIENT);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			const GdaBinary *bin;
			bin = gda_value_get_binary (value);
			sqlite3_result_blob (ctx, bin->data, bin->binary_length, SQLITE_TRANSIENT);
		}
		else {
			gchar *str = gda_value_stringify (value);
			sqlite3_result_text (ctx, str, -1, SQLITE_TRANSIENT);
			g_free (str);
		}
		return SQLITE_OK;
	}
}

static int
virtualRowid (sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid)
{
	VirtualCursor *cursor = (VirtualCursor*) cur;

	TRACE ();

	*pRowid = gda_data_model_iter_get_row (cursor->iter);
	return SQLITE_OK;
}

static int
virtualFilter (sqlite3_vtab_cursor *pVtabCursor, int idxNum, const char *idxStr, int argc, sqlite3_value **argv)
{
	VirtualCursor *cursor = (VirtualCursor*) pVtabCursor;

	TRACE ();

	switch (idxNum) {
	case 0: /* no filtering at all */
		gda_data_model_iter_move_next (cursor->iter);
		break; 
	default:
		TO_IMPLEMENT;
		break;
	}
	return SQLITE_OK;
}

static int
virtualBestIndex (sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo)
{
	TRACE ();
	
	pIdxInfo->idxNum = 0;
	return SQLITE_OK;
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

	TRACE ();

	if ((nData>1) && (sqlite3_value_type (apData[0])==SQLITE_INTEGER)) {
		/* UPDATE */
		gint i;
		for (i = 2; i < nData; i++) {
			GValue *value;
			GType type;
			gint rowid = sqlite3_value_int (apData [0]);
			gboolean res;
			GError *error = NULL;

			/*g_print ("%d => %s\n", i, sqlite3_value_text (apData [i]));*/
			type = gda_column_get_g_type (gda_data_model_describe_column ((GdaDataModel*) vtable->proxy, i - 2));
			value = gda_value_new_from_string ((const gchar*) sqlite3_value_text (apData [i]), type);
			res = gda_data_model_set_value_at ((GdaDataModel*) vtable->proxy, i - 2, rowid, value, &error);
			gda_value_free (value);
			if (!res) {
				g_print ("Error: %s\n", error && error->message ? error->message : "???");
				return SQLITE_READONLY;
			}
		}
		return SQLITE_OK;
	}
	else if ((nData==1) && (sqlite3_value_type (apData[0])==SQLITE_INTEGER)) {
		/* DELETE */
		gint rowid = sqlite3_value_int (apData [0]);
		return gda_data_model_remove_row (GDA_DATA_MODEL (vtable->proxy), rowid, NULL) ? SQLITE_OK : SQLITE_READONLY;
	}
	else if ((nData>2) && (sqlite3_value_type (apData[0])==SQLITE_NULL)) {
		/* INSERT */
		gint newrow, i;
		GList *values = NULL;
		
		newrow = gda_data_model_append_row (GDA_DATA_MODEL (vtable->proxy), NULL);
		if (newrow < 0)
			return SQLITE_READONLY;

		for (i = 2; i < nData; i++) {
			GType type;
			GValue *value;
			type = gda_column_get_g_type (gda_data_model_describe_column ((GdaDataModel*) vtable->proxy, i - 2));
			if (sqlite3_value_text (apData [i]))
				value = gda_value_new_from_string ((const gchar*) sqlite3_value_text (apData [i]), type);
			else
				value = gda_value_new_null ();
			/*g_print ("TXT #%s# => value %p (type=%s)\n", sqlite3_value_text (apData [i]), value,
			  g_type_name (type));*/
			values = g_list_append (values, value);
		}
		gda_data_model_set_values (GDA_DATA_MODEL (vtable->proxy), newrow, values, NULL);
		g_list_foreach (values, (GFunc) gda_value_free, NULL);
		g_list_free (values);

		*pRowid = newrow;
	}
	else {
		/* error */
		/* this case should never happen... */
		g_warning ("Invalid parameters provided by SQLite...");
		return SQLITE_ERROR;
	}

	return SQLITE_OK;
}

static int
virtualBegin (sqlite3_vtab *tab)
{
	TRACE ();

	virtual_table_manage_real_data_model ((VirtualTable *) tab);

	return SQLITE_OK;
}

static int
virtualSync (sqlite3_vtab *tab)
{
	TRACE ();

	return SQLITE_OK;
}

static int
virtualCommit (sqlite3_vtab *tab)
{
	VirtualTable *vtable = (VirtualTable *) tab;
	
	TRACE ();

	if (vtable->proxy)
		return gda_data_proxy_apply_all_changes (vtable->proxy, NULL) ? SQLITE_OK : SQLITE_READONLY;
	else
		return TRUE;
}

static int
virtualRollback (sqlite3_vtab *tab)
{
	VirtualTable *vtable = (VirtualTable *) tab;
	
	TRACE ();

	return gda_data_proxy_cancel_all_changes (vtable->proxy) ? SQLITE_OK : SQLITE_ERROR;
}
