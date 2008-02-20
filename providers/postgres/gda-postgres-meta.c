/* GDA postgres provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include "gda-postgres.h"
#include "gda-postgres-meta.h"
#include "gda-postgres-provider.h"
#include "gda-postgres-util.h"
#include <libgda/gda-meta-store.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-set.h>

static gboolean append_a_row (GdaDataModel *to_model, GError **error, gint nb, ...);

/*
 * predefined statements' IDs
 */
typedef enum {
        I_STMT_CATALOG,
	I_STMT_BTYPES,
	I_STMT_SCHEMATA
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	"SELECT pg_catalog.current_database()",
	"SELECT t.typname, 'pg_catalog.' || t.typname, 'gchararray', pg_catalog.obj_description(t.oid), NULL, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN typtype = 'p' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, CAST (t.oid AS int8) FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (typtype='b' OR typtype='p')",
	"SELECT catalog_name, schema_name, schema_owner, CASE WHEN schema_name ~'^pg_' THEN TRUE WHEN schema_name ='information_schema' THEN TRUE ELSE FALSE END FROM information_schema.schemata"
};

/*
 * predefined statements' GdaStatement
 */
static GdaStatement **internal_stmt;
static GdaSet        *internal_params;

/* 
 * global static values
 */
static GdaSqlParser *internal_parser = NULL;
/* TO_ADD: other static values */


/*
 * Meta initialization
 */
void
_gda_postgres_provider_meta_init (GdaServerProvider *provider)
{
	InternalStatementItem i;

        internal_parser = gda_server_provider_internal_get_parser (provider);
	internal_params = gda_set_new (NULL);

        internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
        for (i = I_STMT_CATALOG; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
		GdaSet *set;
                internal_stmt[i] = gda_sql_parser_parse_string (internal_parser, internal_sql[i], NULL, NULL);
                if (!internal_stmt[i])
                        g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		g_assert (gda_statement_get_parameters (internal_stmt[i], &set, NULL));
		if (set) {
			gda_set_merge_with_set (internal_params, set);
			g_object_unref (set);
		}
        }

	/* initialize static values here */
}

gboolean
_gda_postgres_meta_info (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_CATALOG], NULL, error);
	if (!model)
		return FALSE;

	retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta_btypes (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model, *proxy;
	gboolean retval = TRUE;
	gint i, nrows;
	PostgresConnectionData *cdata;

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	/* use a prepared statement for the "base" model */
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_BTYPES], NULL, error);
	if (!model)
		return FALSE;

	/* use a proxy to customize @model */
	proxy = (GdaDataModel*) gda_data_proxy_new (model);
	gda_data_proxy_set_sample_size ((GdaDataProxy*) proxy, 0);
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *value;
		GType type;
		value = gda_data_model_get_value_at (model, 6, i);
		
		type = _gda_postgres_type_oid_to_gda (cdata, g_value_get_int64 (value));
		if (type != G_TYPE_STRING) {
			GValue *v;
			g_value_set_string (v = gda_value_new (G_TYPE_STRING), g_type_name (type));
			retval = gda_data_model_set_value_at (proxy, 2, i, v, error);
			gda_value_free (v);
			if (!retval)
				break;
		}
	}

	/* modify meta store with @proxy */
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, proxy, NULL, error, NULL);
	g_object_unref (proxy);
	g_object_unref (model);

	return retval;
}

gboolean 
_gda_postgres_meta_schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			     const GValue *schema_name)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_SCHEMATA], NULL, error);
	if (!model)
		return FALSE;

	retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}



gboolean
_gda_postgres_meta_tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}



gboolean 
_gda_postgres_meta_tables_views_s (GdaServerProvider *prov, GdaConnection *cnc, 
				   GdaMetaStore *store, GdaMetaContext *context, GError **error, 
				   const GValue *table_schema, const GValue *table_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta_columns_t (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			      const GValue *table_schema, const GValue *table_name)
{
	return _gda_postgres_meta_columns_c (prov, cnc, store, context, error, table_schema, table_name, NULL);
}

gboolean
_gda_postgres_meta_columns_c (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			      const GValue *table_schema, const GValue *table_name, const GValue *column_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}
