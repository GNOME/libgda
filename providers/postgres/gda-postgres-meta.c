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
	I_STMT_SCHEMAS,
	I_STMT_SCHEMA_NAMED,
	I_STMT_TABLES,
	I_STMT_TABLE_NAMED,
	I_STMT_VIEWS,
	I_STMT_VIEW_NAMED,
	I_STMT_COLUMNS_OF_TABLE,
	I_STMT_TABLES_CONSTRAINTS,
	I_STMT_TABLES_CONSTRAINT_NAMED,
	I_STMT_REF_CONSTRAINTS,
	I_STMT_KEY_COLUMN_USAGE
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	/* I_STMT_CATALOG */
	"SELECT pg_catalog.current_database()",

	/* I_STMT_BTYPES */
	"SELECT t.typname, 'pg_catalog.' || t.typname, 'gchararray', pg_catalog.obj_description(t.oid), NULL, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN typtype = 'p' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, CAST (t.oid AS int8) FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (typtype='b' OR typtype='p')",

	/* I_STMT_SCHEMAS */
	"SELECT catalog_name, schema_name, schema_owner, CASE WHEN schema_name ~'^pg_' THEN TRUE WHEN schema_name ='information_schema' THEN TRUE ELSE FALSE END FROM information_schema.schemata WHERE catalog_name = ##cat::string",

	/* I_STMT_SCHEMA_NAMED */
	"SELECT catalog_name, schema_name, schema_owner, CASE WHEN schema_name ~'^pg_' THEN TRUE WHEN schema_name ='information_schema' THEN TRUE ELSE FALSE END FROM information_schema.schemata WHERE catalog_name = ##cat::string AND schema_name = ##name::string",

	/* I_STMT_TABLES */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nc.nspname::information_schema.sql_identifier AS table_schema, c.relname::information_schema.sql_identifier AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY'::text WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL::text END::information_schema.character_data AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_authid o WHERE current_database()::information_schema.sql_identifier = ##cat::string AND nc.nspname::information_schema.sql_identifier = ##schema::string AND c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text)) AND o.oid=c.relowner",

	/* I_STMT_TABLE_NAMED */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nc.nspname::information_schema.sql_identifier AS table_schema, c.relname::information_schema.sql_identifier AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY'::text WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL::text END::information_schema.character_data AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_authid o WHERE current_database()::information_schema.sql_identifier = ##cat::string AND nc.nspname::information_schema.sql_identifier = ##schema::string AND c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text)) AND o.oid=c.relowner AND c.relname::information_schema.sql_identifier = ##name::string",

	/* I_STMT_VIEWS */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nc.nspname::information_schema.sql_identifier AS table_schema, c.relname::information_schema.sql_identifier AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r'::\"char\" THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE current_database()::information_schema.sql_identifier = ##cat::string AND nc.nspname::information_schema.sql_identifier = ##schema::string AND c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text))",

	/* I_STMT_VIEW_NAMED */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nc.nspname::information_schema.sql_identifier AS table_schema, c.relname::information_schema.sql_identifier AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r'::\"char\" THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE current_database()::information_schema.sql_identifier = ##cat::string AND nc.nspname::information_schema.sql_identifier = ##schema::string AND c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text)) AND c.relname::information_schema.sql_identifier = ##name::string",

	/* I_STMT_COLUMNS_OF_TABLE */
	"SELECT current_database(), nc.nspname, c.relname, a.attname, a.attnum, pg_get_expr(ad.adbin, ad.adrelid), CASE WHEN a.attnotnull OR t.typtype = 'd' AND t.typnotnull THEN FALSE ELSE TRUE END, coalesce (nt.nspname || '.', '') || t.typname, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 1 ELSE 0 END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'ARRAY' || 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL, NULL, NULL, NULL, NULL, CASE WHEN pg_get_expr(ad.adbin, ad.adrelid) LIKE 'nextval(%' THEN 'AUTO_INCREMENT' ELSE NULL END, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.col_description(c.oid, a.attnum), CAST (t.oid AS int8) FROM pg_attribute a LEFT JOIN pg_attrdef ad ON a.attrelid = ad.adrelid AND a.attnum = ad.adnum, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typtype = 'd' AND t.typbasetype = bt.oid WHERE current_database() = ##cat::string AND nc.nspname = ##schema::string AND c.relname = ##name::string AND a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text))",

	/* I_STMT_TABLES_CONSTRAINTS */
	"SELECT constraint_catalog, constraint_schema, constraint_name, table_catalog, table_schema, table_name, constraint_type, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.table_constraints WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string",

	/* I_STMT_TABLES_CONSTRAINT_NAMED */
	"SELECT constraint_catalog, constraint_schema, constraint_name, table_catalog, table_schema, table_name, constraint_type, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.table_constraints WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string",

	/* I_STMT_REF_CONSTRAINTS */
	"SELECT current_database(), nt.nspname, t.relname, c.conname, current_database(), nref.nspname, ref.relname, pkc.conname, CASE c.confmatchtype WHEN 'f'::\"char\" THEN 'FULL'::text WHEN 'p'::\"char\" THEN 'PARTIAL'::text WHEN 'u'::\"char\" THEN 'NONE'::text ELSE NULL::text END AS match_option, CASE c.confupdtype WHEN 'c'::\"char\" THEN 'CASCADE'::text WHEN 'n'::\"char\" THEN 'SET NULL'::text WHEN 'd'::\"char\" THEN 'SET DEFAULT'::text WHEN 'r'::\"char\" THEN 'RESTRICT'::text WHEN 'a'::\"char\" THEN 'NO ACTION'::text ELSE NULL::text END AS update_rule, CASE c.confdeltype WHEN 'c'::\"char\" THEN 'CASCADE'::text WHEN 'n'::\"char\" THEN 'SET NULL'::text WHEN 'd'::\"char\" THEN 'SET DEFAULT'::text WHEN 'r'::\"char\" THEN 'RESTRICT'::text WHEN 'a'::\"char\" THEN 'NO ACTION'::text ELSE NULL::text END AS delete_rule FROM pg_constraint c INNER JOIN pg_class t ON (c.conrelid=t.oid) INNER JOIN pg_namespace nt ON (nt.oid=t.relnamespace) INNER JOIN pg_class ref ON (c.confrelid=ref.oid) INNER JOIN pg_namespace nref ON (nref.oid=ref.relnamespace) INNER JOIN pg_constraint pkc ON (c.confrelid = pkc.conrelid AND information_schema._pg_keysequal(c.confkey, pkc.conkey)) WHERE c.contype = 'f' AND current_database() = ##cat::string AND nt.nspname = ##schema::string AND t.relname = ##name::string AND c.conname = ##name2::string",

	/* I_STMT_KEY_COLUMN_USAGE */
	"SELECT table_catalog, table_schema, table_name, constraint_name, column_name, ordinal_position FROM information_schema.key_column_usage WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string"
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
static GdaSet       *i_set;


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
	i_set = gda_set_new_inline (4, "cat", G_TYPE_STRING, "", 
				    "name", G_TYPE_STRING, "",
				    "name2", G_TYPE_STRING, "",
				    "schema", G_TYPE_STRING, "");
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
			     const GValue *catalog_name, const GValue *schema_name_n)
{
	GdaDataModel *model;
	gboolean retval;

	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), catalog_name);
	if (!schema_name_n) {
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_SCHEMAS], i_set, error);
		if (!model)
			return FALSE;
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	}
	else {
		gda_holder_set_value (gda_set_get_holder (i_set, "name"), schema_name_n);
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_SCHEMA_NAMED], i_set, error);
		if (!model)
			return FALSE;
		
		retval = gda_meta_store_modify (store, context->table_name, model, "schema_name = ##name::string", error, 
						"name", schema_name_n, NULL);
	}
	g_object_unref (model);

	return retval;
}

gboolean 
_gda_postgres_meta_tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error, 
				 const GValue *table_catalog, const GValue *table_schema, const GValue *table_name_n)
{
	GdaDataModel *tables_model, *views_model;
	gboolean retval = TRUE;

	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog);
	gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema);
	if (!table_name_n) {
		tables_model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TABLES], i_set, error);
		if (!tables_model)
			return FALSE;
		views_model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_VIEWS], i_set, error);
		if (!views_model) {
			g_object_unref (tables_model);
			return FALSE;
		}
	}
	else {
		gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name_n);
		tables_model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TABLE_NAMED], i_set, error);
		if (!tables_model)
			return FALSE;
		views_model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_VIEW_NAMED], i_set, error);
		if (!views_model) {
			g_object_unref (tables_model);
			return FALSE;
		}
	}

	GdaMetaContext c2;
	c2 = *context; /* copy contents, just because we need to modify @context->table_name */
	if (retval) {
		c2.table_name = "_tables";
		retval = gda_meta_store_modify_with_context (store, &c2, tables_model, error);
	}
	if (retval) {
		c2.table_name = "_views";
		retval = gda_meta_store_modify_with_context (store, &c2, views_model, error);
	}
	g_object_unref (tables_model);
	g_object_unref (views_model);


	return retval;
}

gboolean
_gda_postgres_meta_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			    const GValue *table_catalog, const GValue *table_schema, const GValue *table_name)
{
	GdaDataModel *model, *proxy;
	gboolean retval = TRUE;
	gint i, nrows;
	PostgresConnectionData *cdata;
	GType col_types[] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 
		G_TYPE_INT, G_TYPE_NONE
	};

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	/* use a prepared statement for the "base" model */
	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog);
	gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema);
	gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name);
	model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_COLUMNS_OF_TABLE], i_set, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types, error);
	if (!model)
		return FALSE;

	/* use a proxy to customize @model */
	proxy = (GdaDataModel*) gda_data_proxy_new (model);
	gda_data_proxy_set_sample_size ((GdaDataProxy*) proxy, 0);
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *value;
		GType type;
		value = gda_data_model_get_value_at (model, 25, i);
		
		type = _gda_postgres_type_oid_to_gda (cdata, g_value_get_int64 (value));
		if (type != G_TYPE_STRING) {
			GValue *v;
			g_value_set_string (v = gda_value_new (G_TYPE_STRING), g_type_name (type));
			retval = gda_data_model_set_value_at (proxy, 10, i, v, error);
			gda_value_free (v);
			if (!retval)
				break;
		}
	}

	/* modify meta store with @proxy */
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, proxy, 
						"table_schema = ##schema::string AND table_name = ##name::string", error, 
						"schema", table_schema, "name", table_name, NULL);
	g_object_unref (proxy);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta_constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
				    GdaMetaStore *store, GdaMetaContext *context, GError **error,
				    const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
				    const GValue *constraint_name_n)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	PostgresConnectionData *cdata;

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog);
	gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema);
	gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name);

	if (!constraint_name_n) {
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TABLES_CONSTRAINTS], i_set, 
								 error);
		if (!model)
			return FALSE;
		if (retval)
			retval = gda_meta_store_modify (store, context->table_name, model, 
							"table_schema = ##schema::string AND table_name = ##name::string", 
							error, 
							"schema", table_schema, "name", table_name, NULL);

	}
	else {
		gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name_n);
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TABLES_CONSTRAINT_NAMED], i_set, 
								 error);
		if (!model)
			return FALSE;
		if (retval)
			retval = gda_meta_store_modify (store, context->table_name, model, 
							"table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string", error, 
							"schema", table_schema, "name", table_name, "name2", constraint_name_n, NULL);
	}

	g_object_unref (model);

	return retval;
}


gboolean
_gda_postgres_meta_constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
				    GdaMetaStore *store, GdaMetaContext *context, GError **error,
				    const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				    const GValue *constraint_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	PostgresConnectionData *cdata;

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog);
	gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema);
	gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name);
	gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name);
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_REF_CONSTRAINTS], i_set, 
							 error);
	if (!model)
		return FALSE;


	/* modify meta store */
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, 
						"table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string", 
						error, 
						"schema", table_schema, "name", table_name, "name2", constraint_name, NULL);
	g_object_unref (model);

	return retval;
}

gboolean 
_gda_postgres_meta_key_columns (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				const GValue *constraint_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	PostgresConnectionData *cdata;

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog);
	gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema);
	gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name);
	gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name);
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_KEY_COLUMN_USAGE], i_set, 
							 error);
	if (!model)
		return FALSE;


	/* modify meta store */
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, 
						"table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string", 
						error, 
						"schema", table_schema, "name", table_name, "name2", constraint_name, NULL);
	g_object_unref (model);

	return retval;	
}
