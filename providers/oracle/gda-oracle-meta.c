/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

#undef GDA_DISABLE_DEPRECATED
#include <string.h>
#include "gda-oracle.h"
#include "gda-oracle-meta.h"
#include "gda-oracle-provider.h"
#include <libgda/gda-meta-store.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>
#include "gda-oracle-util.h"
#include <libgda/gda-debug-macros.h>

/*
 * predefined statements' IDs
 */
typedef enum {
        I_STMT_CATALOG,
        I_STMT_SCHEMAS_ALL,
	I_STMT_SCHEMA_NAMED,
	I_STMT_TABLES_ALL,
	I_STMT_TABLES_ALL_RAW,
	I_STMT_TABLES,
	I_STMT_TABLE_NAMED,
	I_STMT_VIEWS_ALL,
	I_STMT_VIEWS,
	I_STMT_VIEW_NAMED,
	I_STMT_COLUMNS_OF_TABLE,
        I_STMT_COLUMNS_ALL,
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	/* I_STMT_CATALOG */
	"select ora_database_name from dual",

	/* I_STMT_SCHEMAS_ALL */
	"SELECT ora_database_name, username, username, CASE WHEN username IN ('SYS', 'SYSTEM', 'DBSNMP', 'OUTLN', 'MDSYS', 'ORDSYS', 'ORDPLUGINS', 'CTXSYS', 'DSSYS', 'PERFSTAT', 'WKPROXY', 'WKSYS', 'WMSYS', 'XDB', 'ANONYMOUS', 'ODM', 'ODM_MTR', 'OLAPSYS', 'TRACESVR', 'REPADMIN') THEN 1 ELSE 0 END FROM all_users",

	/* I_STMT_SCHEMA_NAMED */
	"SELECT ora_database_name, username, username, CASE WHEN username IN ('SYS', 'SYSTEM', 'DBSNMP', 'OUTLN', 'MDSYS', 'ORDSYS', 'ORDPLUGINS', 'CTXSYS', 'DSSYS', 'PERFSTAT', 'WKPROXY', 'WKSYS', 'WMSYS', 'XDB', 'ANONYMOUS', 'ODM', 'ODM_MTR', 'OLAPSYS', 'TRACESVR', 'REPADMIN') THEN 1 ELSE 0 END FROM all_users WHERE username = ##name::string",

	/* I_STMT_TABLES_ALL */
	"SELECT ora_database_name, a.owner, a.table_name, 'BASE TABLE', 1, c.comments, CASE WHEN a.owner = user THEN a.table_name ELSE a.owner || '.' || a.table_name END, a.owner || '.' || a.table_name, a.owner FROM all_tables a LEFT JOIN ALL_TAB_COMMENTS c ON (a.table_name=c.table_name) UNION SELECT ora_database_name, v.owner, v.view_name, 'VIEW', 0, c.comments, CASE WHEN v.owner = user THEN v.view_name ELSE v.owner || '.' || v.view_name END, v.owner || '.' || v.view_name, v.owner FROM all_views v LEFT JOIN ALL_TAB_COMMENTS c ON (v.view_name=c.table_name)",

	/* I_STMT_TABLES_ALL_RAW */
	"SELECT ora_database_name, a.owner, a.table_name FROM all_tables a UNION SELECT ora_database_name, v.owner, v.view_name FROM all_views v",

	/* I_STMT_TABLES */
	"SELECT ora_database_name, a.owner, a.table_name, 'BASE TABLE', 1, c.comments, CASE WHEN a.owner = user THEN a.table_name ELSE a.owner || '.' || a.table_name END, a.owner || '.' || a.table_name, a.owner FROM all_tables a LEFT JOIN ALL_TAB_COMMENTS c ON (a.table_name=c.table_name) WHERE a.owner = ##schema::string UNION SELECT ora_database_name, v.owner, v.view_name, 'VIEW', 0, c.comments, CASE WHEN v.owner = user THEN v.view_name ELSE v.owner || '.' || v.view_name END, v.owner || '.' || v.view_name, v.owner FROM all_views v LEFT JOIN ALL_TAB_COMMENTS c ON (v.view_name=c.table_name) WHERE v.owner = ##schema::string",

	/* I_STMT_TABLE_NAMED */
	"SELECT ora_database_name, a.owner, a.table_name, 'BASE TABLE', 1, c.comments, CASE WHEN a.owner = user THEN a.table_name ELSE a.owner || '.' || a.table_name END, a.owner || '.' || a.table_name, a.owner FROM all_tables a LEFT JOIN ALL_TAB_COMMENTS c ON (a.table_name=c.table_name) WHERE a.owner = ##schema::string AND a.table_name = ##name::string UNION SELECT ora_database_name, v.owner, v.view_name, 'VIEW', 0, c.comments, CASE WHEN v.owner = user THEN v.view_name ELSE v.owner || '.' || v.view_name END, v.owner || '.' || v.view_name, v.owner FROM all_views v LEFT JOIN ALL_TAB_COMMENTS c ON (v.view_name=c.table_name) WHERE v.owner = ##schema::string AND v.view_name = ##name::string",

	/* I_STMT_VIEWS_ALL */
	"SELECT ora_database_name, v.owner, v.view_name, v.text, NULL, 0 FROM all_views v",

	/* I_STMT_VIEWS */
	"SELECT ora_database_name, v.owner, v.view_name, v.text, NULL, 0 FROM all_views v WHERE v.owner = ##schema::string",

	/* I_STMT_VIEW_NAMED */
	"SELECT ora_database_name, v.owner, v.view_name, v.text, NULL, 0 FROM all_views v WHERE v.owner = ##schema::string AND v.view_name = ##name::string",

	/* I_STMT_COLUMNS_OF_TABLE */
	"SELECT ora_database_name, tc.owner, tc.table_name, tc.column_name, tc.column_id, tc.data_default, decode(tc.nullable,'N',0,'Y',1), tc.data_type, NULL, 'gchararray', CASE WHEN tc.char_used = 'C' THEN tc.char_length ELSE NULL END as clen, CASE WHEN tc.char_used = 'B' THEN tc.char_length ELSE NULL END as olen, tc.data_precision, tc.data_scale, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, c.comments  FROM ALL_TAB_COLUMNS tc LEFT JOIN ALL_COL_COMMENTS c ON (tc.owner = c.owner AND tc.table_name=c.table_name AND tc.column_name = c.column_name) WHERE tc.table_name = ##name::string ORDER BY tc.column_id",

        /* I_STMT_COLUMNS_ALL */
	"SELECT ora_database_name, tc.owner, tc.table_name, tc.column_name, tc.column_id, tc.data_default, decode(tc.nullable,'N',0,'Y',1), tc.data_type, NULL, 'gchararray', CASE WHEN tc.char_used = 'C' THEN tc.char_length ELSE NULL END as clen, CASE WHEN tc.char_used = 'B' THEN tc.char_length ELSE NULL END as olen, tc.data_precision, tc.data_scale, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, c.comments  FROM ALL_TAB_COLUMNS tc LEFT JOIN ALL_COL_COMMENTS c ON (tc.owner = c.owner AND tc.table_name=c.table_name AND tc.column_name = c.column_name) ORDER BY tc.table_name, tc.column_id"
};

/*
 * global static values, and
 * predefined statements' GdaStatement, all initialized in _gda_oracle_provider_meta_init()
 */
static GMutex init_mutex;
static GdaStatement **internal_stmt = NULL;
static GdaSet        *i_set = NULL;
static GdaSqlParser  *internal_parser = NULL;

/*
 * Meta initialization
 */
void
_gda_oracle_provider_meta_init (GdaServerProvider *provider)
{
	g_mutex_lock (&init_mutex);

	if (!internal_stmt) {
		InternalStatementItem i;
		internal_parser = gda_server_provider_internal_get_parser (provider);
		internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
		for (i = I_STMT_CATALOG; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
			internal_stmt[i] = gda_sql_parser_parse_string (internal_parser, internal_sql[i], NULL, NULL);
			if (!internal_stmt[i])
				g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		}

		/* initialize static values here */
		i_set = gda_set_new_inline (4, "cat", G_TYPE_STRING, "",
					    "name", G_TYPE_STRING, "",
					    "schema", G_TYPE_STRING, "",
					    "name2", G_TYPE_STRING, "");
	}

	g_mutex_unlock (&init_mutex);
}

gboolean
_gda_oracle_meta__info (GdaServerProvider *prov, GdaConnection *cnc, 
		      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_CATALOG], NULL, 
							 error);
	if (!model)
		return FALSE;
	gda_meta_store_set_identifiers_style (store, GDA_SQL_IDENTIFIERS_UPPER_CASE);
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_oracle_meta__btypes (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__udt (GdaServerProvider *prov, GdaConnection *cnc, 
		     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_udt (GdaServerProvider *prov, GdaConnection *cnc, 
		    GdaMetaStore *store, GdaMetaContext *context, GError **error,
		    const GValue *udt_catalog, const GValue *udt_schema)
{
	GdaDataModel *model = NULL;
	gboolean retval = TRUE;

	/* set internal holder's values from the arguments */
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), udt_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), udt_schema, error))
		return FALSE;

	TO_IMPLEMENT;
	/* fill in @model, with something like:
	 * model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_UDT], i_set, error);
	 */
	if (!model)
		return FALSE;
	gda_meta_store_set_identifiers_style (store, GDA_SQL_IDENTIFIERS_UPPER_CASE);
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}


gboolean
_gda_oracle_meta__udt_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_oracle_meta_udt_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_oracle_meta__enums (GdaServerProvider *prov, GdaConnection *cnc, 
		       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_oracle_meta_enums (GdaServerProvider *prov, GdaConnection *cnc, 
		      GdaMetaStore *store, GdaMetaContext *context, GError **error,
		      const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	TO_IMPLEMENT;
	return TRUE;
}


gboolean
_gda_oracle_meta__domains (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_domains (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *domain_catalog, const GValue *domain_schema)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *domain_catalog, const GValue *domain_schema, 
				const GValue *domain_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__el_types (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_el_types (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *specific_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__collations (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_collations (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *collation_catalog, const GValue *collation_schema, 
			   const GValue *collation_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__character_sets (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_character_sets (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error,
			       const GValue *chset_catalog, const GValue *chset_schema, 
			       const GValue *chset_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GType col_types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_NONE};

	model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_SCHEMAS_ALL], NULL, 
                                                              GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types, error);
	if (!model)
		return FALSE;
	gda_meta_store_set_identifiers_style (store, GDA_SQL_IDENTIFIERS_UPPER_CASE);
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_oracle_meta_schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			 const GValue *catalog_name, const GValue *schema_name_n)
{
	GdaDataModel *model;
        gboolean retval;
	GType col_types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_NONE};

        if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), catalog_name, error))
                return FALSE;
        if (!schema_name_n) {
		model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_SCHEMAS_ALL], NULL, 
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types, error);
                if (!model)
                        return FALSE;
		gda_meta_store_set_identifiers_style (store, GDA_SQL_IDENTIFIERS_UPPER_CASE);
                retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
        }
        else {
                if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), schema_name_n, error))
                        return FALSE;
		model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_SCHEMA_NAMED], i_set, 
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types, error);
                if (!model)
                        return FALSE;

		gda_meta_store_set_identifiers_style (store, GDA_SQL_IDENTIFIERS_UPPER_CASE);
                retval = gda_meta_store_modify (store, context->table_name, model, "schema_name = ##name::string", error,
                                                "name", schema_name_n, NULL);
        }
        g_object_unref (model);

        return retval;
}

static GType col_types_tables[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_NONE};
static GType col_types_views[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_NONE};


gboolean
_gda_oracle_meta__tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *tables_model, *views_model;
        gboolean retval = TRUE;

        OracleConnectionData *cdata;
        cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
        if (!cdata)
                return FALSE;

        tables_model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_TABLES_ALL], NULL, 
								     GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types_tables,
								     error);
        if (!tables_model)
                return FALSE;
        views_model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_VIEWS_ALL], NULL,
								    GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types_views,
								    error);
        if (!views_model) {
                g_object_unref (tables_model);
                return FALSE;
        }

	gda_meta_store_set_identifiers_style (store, GDA_SQL_IDENTIFIERS_UPPER_CASE);

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
_gda_oracle_meta_tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *table_catalog, const GValue *table_schema, 
			     const GValue *table_name_n)
{
	GdaDataModel *tables_model, *views_model;
        gboolean retval = TRUE;

        OracleConnectionData *cdata;
        cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
        if (!cdata)
                return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
                return FALSE;

	if (!table_name_n) {
		tables_model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_TABLES], i_set, 
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types_tables,
									     error);
		if (!tables_model)
			return FALSE;
		views_model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_VIEWS], i_set,
									    GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types_views,
									    error);
		if (!views_model) {
			g_object_unref (tables_model);
			return FALSE;
		}
	}
	else {
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name_n, error))
                        return FALSE;
		tables_model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_TABLE_NAMED], i_set, 
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types_tables,
									     error);
		if (!tables_model)
			return FALSE;
		views_model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_VIEW_NAMED], i_set,
									    GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types_views,
									    error);
		if (!views_model) {
			g_object_unref (tables_model);
			return FALSE;
		}
	}

	gda_meta_store_set_identifiers_style (store, GDA_SQL_IDENTIFIERS_UPPER_CASE);

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

/*
 * Returns: a new G_TYPE_STRING GValue
 */
static GValue *
oracle_identifier_to_value (const gchar *sqlid)
{
	GValue *v;
	g_return_val_if_fail (sqlid, NULL);

	v = gda_value_new (G_TYPE_STRING);
	if (g_ascii_isalnum (*sqlid)) {
		const gchar *ptr;
		for (ptr = sqlid; *ptr; ptr++) {
			if ((*ptr == ' ') || (*ptr != g_ascii_toupper (*ptr))) {
				/* add quotes */
				g_value_take_string (v, gda_sql_identifier_force_quotes (sqlid));
				return v;
			}
		}

		g_value_set_string (v, sqlid);
	}
	else
		g_value_take_string (v, gda_sql_identifier_force_quotes (sqlid));
	return v;
}

gboolean
_gda_oracle_meta__columns (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model, *tables;
	gboolean retval = FALSE;
	gint i, nrows;
	OracleConnectionData *cdata;

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return FALSE;

	/* use a prepared statement for the "base" model */
	tables = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TABLES_ALL_RAW], NULL, error);
	if (!tables)
		return FALSE;

	model = gda_data_model_array_new_with_g_types (24, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 
						       G_TYPE_INT, G_TYPE_STRING, G_TYPE_BOOLEAN,
						       G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
						       G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, 
						       G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
						       G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
						       G_TYPE_BOOLEAN, G_TYPE_STRING);

	/* fill in @model */
	const GValue *cv0 = NULL;
	GList *values = NULL;
	nrows = gda_data_model_get_n_rows (tables);
	for (i = 0; i < nrows; i++) {
		const GValue *cv1, *cv2;
		values = NULL;

		if (!cv0) {
			cv0 = gda_data_model_get_value_at (tables, 0, i, error);
			if (!cv0)
				goto out;
		}
		if (!(cv1 = gda_data_model_get_value_at (tables, 1, i, error)) ||
		    !(cv2 = gda_data_model_get_value_at (tables, 2, i, error)))
			goto out;

		/* Allocate the Describe handle */
		int result;
		OCIDescribe *dschp = (OCIDescribe *) 0;
		GdaConnectionEvent *event;

		result = OCIHandleAlloc ((dvoid *) cdata->henv,
					 (dvoid **) &dschp,
					 (ub4) OCI_HTYPE_DESCRIBE,
					 (size_t) 0,
					 (dvoid **) 0);
		if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
						      _("Could not fetch next row")))) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
				     "%s", gda_connection_event_get_description (event));
			goto out;
		}

		/* Describe the table */
		gchar *fq_tblname;
		const gchar *schema_name, *table_name;
		GValue *v1, *v2;
		v1 = oracle_identifier_to_value (g_value_get_string (cv1));
		schema_name = g_value_get_string (v1);
		v2 = oracle_identifier_to_value (g_value_get_string (cv2));
		table_name = g_value_get_string (v2);
		fq_tblname = g_strdup_printf ("%s.%s", schema_name, table_name);
		gda_value_free (v1);
		gda_value_free (v2);

		result = OCIDescribeAny (cdata->hservice,
					 cdata->herr,
					 (text *) fq_tblname,
					 strlen (fq_tblname),
					 OCI_OTYPE_NAME,
					 0,
					 OCI_PTYPE_UNK,
					 (OCIDescribe *) dschp);
		g_free (fq_tblname);
		if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
						      _("Could not get a description handle")))) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
				     "%s", gda_connection_event_get_description (event));
			OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
			goto out;
		}


		/* Get the parameter handle */
		OCIParam *parmh;
		result = OCIAttrGet ((dvoid *) dschp,
				     (ub4) OCI_HTYPE_DESCRIBE,
				     (dvoid **) &parmh,
				     (ub4 *) 0,
				     (ub4) OCI_ATTR_PARAM,
				     (OCIError *) cdata->herr);
		if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
						      _("Could not get parameter handle")))) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
				     "%s", gda_connection_event_get_description (event));
			OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
			goto out;
		}

		/* Get the number of columns */
		ub2 numcols;
		OCIParam *collsthd;
		result = OCIAttrGet ((dvoid *) parmh,
				     (ub4) OCI_DTYPE_PARAM,
				     (dvoid *) &numcols,
				     (ub4 *) 0,
				     (ub4) OCI_ATTR_NUM_COLS,
				     (OCIError *) cdata->herr);
		if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
						      _("Could not get attribute")))) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
				     "%s", gda_connection_event_get_description (event));
			OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
			goto out;
		}

		result = OCIAttrGet ((dvoid *) parmh,
				     (ub4) OCI_DTYPE_PARAM,
				     (dvoid *) &collsthd,
				     (ub4 *) 0,
				     (ub4) OCI_ATTR_LIST_COLUMNS,
				     (OCIError *) cdata->herr);
		if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
						      _("Could not get attribute")))) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
				     "%s", gda_connection_event_get_description (event));
			OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
			goto out;
		}
		
		/* fetch information for each column, loop starts at 1! */
		gint col;
		for (col = 1; col <= numcols; col++) {
			/* column's catalog, schema, table */
			GValue *v;
			values = g_list_prepend (NULL, (gpointer) cv0);
			v = oracle_identifier_to_value (g_value_get_string (cv1));
			values = g_list_prepend (values, v);
			v = oracle_identifier_to_value (g_value_get_string (cv2));
			values = g_list_prepend (values, v);

			/* Get the column handle */
			OCIParam *colhd;
			result = OCIParamGet ((dvoid *) collsthd,
					      (ub4) OCI_DTYPE_PARAM,
					      (OCIError *) cdata->herr,
					      (dvoid **) &colhd,
					      (ub2) col);
			if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
							      _("Could not get attribute")))) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
					     "%s", gda_connection_event_get_description (event));
				OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
				goto out;
			}

			/* Field name */
			text *strp;
			ub4 sizep;
			result = OCIAttrGet ((dvoid *) colhd,
					     (ub4) OCI_DTYPE_PARAM,
					     (dvoid *) &strp,
					     (ub4 *) &sizep,
					     (ub4) OCI_ATTR_NAME,
					     (OCIError *) cdata->herr);
			if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
							      _("Could not get attribute")))) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
					     "%s", gda_connection_event_get_description (event));
				OCIDescriptorFree ((dvoid *) colhd, OCI_DTYPE_PARAM);
				OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
				goto out;
			}
			gchar *tmpname;
			tmpname = (gchar*) g_malloc (sizep + 1);
			strncpy (tmpname, strp, sizep);
			tmpname[sizep] = 0;
			v = oracle_identifier_to_value (tmpname);
			g_free (tmpname);
			values = g_list_prepend (values, v);
			
			/* ordinal position */
			g_value_set_int ((v = gda_value_new (G_TYPE_INT)), col);
			values = g_list_prepend (values, v);

			/* default */
			values = g_list_prepend (values, gda_value_new_null ());

			/* Not null? */
			ub1 nullable;
			result = OCIAttrGet ((dvoid *)colhd,
					     (ub4) OCI_DTYPE_PARAM,
					     (dvoid *) &nullable,
					     (ub4 *) 0,
					     (ub4) OCI_ATTR_IS_NULL,
					     (OCIError *) cdata->herr);
			if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
							      _("Could not get attribute")))) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
					     "%s", gda_connection_event_get_description (event));
				OCIDescriptorFree ((dvoid *) colhd, OCI_DTYPE_PARAM);
				OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
				goto out;
			}
			g_value_set_boolean ((v = gda_value_new (G_TYPE_BOOLEAN)), ! (nullable > 0));
			values = g_list_prepend (values, v);

			/* Data type */
			ub2 type;
			ub1 precision;
			sb1 scale;
			ub4 char_used;
			ub2 char_size = 0;
			ub4 size;
			result = OCIAttrGet ((dvoid *)colhd,
					     (ub4) OCI_DTYPE_PARAM,
					     (dvoid *) &type,
					     (ub4 *) 0,
					     (ub4) OCI_ATTR_DATA_TYPE,
					     (OCIError *) cdata->herr);
			if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
							      _("Could not get attribute")))) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
					     "%s", gda_connection_event_get_description (event));
				OCIDescriptorFree ((dvoid *) colhd, OCI_DTYPE_PARAM);
				OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
				goto out;
			}
			
			result = OCIAttrGet ((dvoid *)colhd,
					     (ub4) OCI_DTYPE_PARAM,
					     (dvoid *) &precision,
					     (ub4 *) 0,
					     (ub4) OCI_ATTR_PRECISION,
					     (OCIError *) cdata->herr);
			if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
							      _("Could not get attribute")))) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
					     "%s", gda_connection_event_get_description (event));
				OCIDescriptorFree ((dvoid *) colhd, OCI_DTYPE_PARAM);
				OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
				goto out;
			}

			result = OCIAttrGet ((dvoid *)colhd,
					     (ub4) OCI_DTYPE_PARAM,
					     (dvoid *) &scale,
					     (ub4 *) 0,
					     (ub4) OCI_ATTR_SCALE,
					     (OCIError *) cdata->herr);
			if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
							      _("Could not get attribute")))) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
					     "%s", gda_connection_event_get_description (event));
				OCIDescriptorFree ((dvoid *) colhd, OCI_DTYPE_PARAM);
				OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
				goto out;
			}

			result = OCIAttrGet ((dvoid *)colhd,
					     (ub4) OCI_DTYPE_PARAM,
					     (dvoid *) &char_used,
					     (ub4 *) 0,
					     (ub4) OCI_ATTR_CHAR_USED,
					     (OCIError *) cdata->herr);
			if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
							      _("Could not get attribute")))) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
					     "%s", gda_connection_event_get_description (event));
				OCIDescriptorFree ((dvoid *) colhd, OCI_DTYPE_PARAM);
				OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
				goto out;
			}
			if (char_used) {
				result = OCIAttrGet ((dvoid *)colhd,
						     (ub4) OCI_DTYPE_PARAM,
						     (dvoid *) &char_size,
						     (ub4 *) 0,
						     (ub4) OCI_ATTR_CHAR_SIZE,
						     (OCIError *) cdata->herr);
				if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
								      _("Could not get attribute")))) {
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
						     "%s", gda_connection_event_get_description (event));
					OCIDescriptorFree ((dvoid *) colhd, OCI_DTYPE_PARAM);
					OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
					goto out;
				}
			}

			result = OCIAttrGet ((dvoid *)colhd,
					     (ub4) OCI_DTYPE_PARAM,
					     (dvoid *) &size,
					     (ub4 *) 0,
					     (ub4) OCI_ATTR_DATA_SIZE,
					     (OCIError *) cdata->herr);
			if ((event = gda_oracle_check_result ((result), (cnc), (cdata), OCI_HTYPE_ERROR,
							      _("Could not get attribute")))) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
					     "%s", gda_connection_event_get_description (event));
				OCIDescriptorFree ((dvoid *) colhd, OCI_DTYPE_PARAM);
				OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);
				goto out;
			}


			g_value_set_string ((v = gda_value_new (G_TYPE_STRING)), _oracle_sqltype_to_string (type));
			values = g_list_prepend (values, v);

			/* array spec */
			values = g_list_prepend (values, gda_value_new_null ());
			
			/* GType */
			const gchar *ctmp;
			ctmp = g_type_name (_oracle_sqltype_to_g_type (type, precision, scale));
			g_value_set_string ((v = gda_value_new (G_TYPE_STRING)),
					    ctmp ? ctmp : "GdaBinary");
			values = g_list_prepend (values, v);

			if (char_used) {
				/* character_maximum_length */
				g_value_set_int ((v = gda_value_new (G_TYPE_INT)), char_size);
				values = g_list_prepend (values, v);

				/* character_octet_length */
				g_value_set_int ((v = gda_value_new (G_TYPE_INT)), size);
				values = g_list_prepend (values, v);

				/* numeric_precision */
				values = g_list_prepend (values, gda_value_new_null ());

				/* numeric_scale */
				values = g_list_prepend (values, gda_value_new_null ());
			}
			else {
				/* REM: size==22 for NUMBERS */
				/* character_maximum_length */
				values = g_list_prepend (values, gda_value_new_null ());

				/* character_octet_length */
				if (size == 22)
					values = g_list_prepend (values, gda_value_new_null ());
				else {
					g_value_set_int ((v = gda_value_new (G_TYPE_INT)), size);
					values = g_list_prepend (values, v);
				}

				/* numeric_precision */
				g_value_set_int ((v = gda_value_new (G_TYPE_INT)), precision);
				values = g_list_prepend (values, v);
				
				/* numeric_scale */
				g_value_set_int ((v = gda_value_new (G_TYPE_INT)), scale);
				values = g_list_prepend (values, v);
			}

			/* datetime_precision */
			values = g_list_prepend (values, gda_value_new_null ());

			/* character_set_catalog */
			values = g_list_prepend (values, gda_value_new_null ());

			/* character_set_schema */
			values = g_list_prepend (values, gda_value_new_null ());
			
			/* character_set_name: see NLS_CHARSET_NAME and OCI_ATTR_CHARSET_ID */
			values = g_list_prepend (values, gda_value_new_null ());

			/* collation_catalog */
			values = g_list_prepend (values, gda_value_new_null ());

			/* collation_schema */
			values = g_list_prepend (values, gda_value_new_null ());
			
			/* collation_name */
			values = g_list_prepend (values, gda_value_new_null ());

			/* extra */
			values = g_list_prepend (values, gda_value_new_null ());

			/* is_updatable */
			values = g_list_prepend (values, gda_value_new_null ());

			/* column_comments */
			values = g_list_prepend (values, gda_value_new_null ());

			/* add to @model */
			gint newrow;
			values = g_list_reverse (values);
			newrow = gda_data_model_append_values (model, values, error);
			
			/* free values */
			g_list_foreach (values->next, (GFunc) gda_value_free, NULL);
			g_list_free (values);
			values = NULL;
			if (newrow == -1)
				goto out;
		}

		OCIHandleFree ((dvoid *) dschp, OCI_HTYPE_DESCRIBE);		
	}
	retval = TRUE;

 out:
	if (values) {
		/* in case of error */
		values = g_list_reverse (values);
		g_list_foreach (values->next, (GFunc) gda_value_free, NULL);
		g_list_free (values);
	}

	/* modify meta store with @model */
	if (retval) {
		/*
		FILE *out;
		out = fopen ("_columns", "w");
		gda_data_model_dump (model, out);
		fclose (out);
		*/

		gda_meta_store_set_identifiers_style (store, GDA_SQL_IDENTIFIERS_UPPER_CASE);
		retval = gda_meta_store_modify_with_context (store, context, model, error);
	}
	g_object_unref (tables);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_oracle_meta_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error,
			  const GValue *table_catalog, const GValue *table_schema, 
			  const GValue *table_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__view_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_view_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error,
			  const GValue *view_catalog, const GValue *view_schema, 
			  const GValue *view_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error, 
				const GValue *table_catalog, const GValue *table_schema, 
				const GValue *table_name, const GValue *constraint_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				const GValue *constraint_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__key_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_key_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *table_catalog, const GValue *table_schema, 
			    const GValue *table_name, const GValue *constraint_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__check_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_check_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *table_catalog, const GValue *table_schema, 
			      const GValue *table_name, const GValue *constraint_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__triggers (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_triggers (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *table_catalog, const GValue *table_schema, 
			 const GValue *table_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__routines (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_routines (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *routine_catalog, const GValue *routine_schema, 
			 const GValue *routine_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__routine_col (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_routine_col (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *rout_catalog, const GValue *rout_schema, 
			    const GValue *rout_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__routine_par (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_routine_par (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *rout_catalog, const GValue *rout_schema, 
			    const GValue *rout_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__indexes_tab (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_indexes_tab (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
			      const GValue *index_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__index_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_index_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *table_catalog, const GValue *table_schema,
			     const GValue *table_name, const GValue *index_name)
{
	TO_IMPLEMENT;
	return TRUE;
}
