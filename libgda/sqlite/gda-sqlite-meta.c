/* GNOME DB Sqlite Provider
 * Copyright (C) 2008 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Bas Driessen <bas.driessen@xobas.com>
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
#include "gda-sqlite.h"
#include "gda-sqlite-meta.h"
#include "gda-sqlite-provider.h"
#include <libgda/gda-meta-store.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-set.h>

static gboolean append_a_row (GdaDataModel *to_model, GError **error, gint nb, ...);
static gint check_parameters (GdaMetaContext *context, GError **error, gint nb, ...);

typedef gboolean (*UpdateFunc) (GdaSqliteProvider *, SqliteConnectionData *, GdaConnection *, 
				GdaMetaStore *, GdaMetaContext *, GError **);
static gboolean update_META_CAT     (GdaSqliteProvider *provider, SqliteConnectionData *scnc, GdaConnection *cnc,
				     GdaMetaStore *store, GdaMetaContext *context, GError **error);
static gboolean update_META_SCHEMAS (GdaSqliteProvider *provider, SqliteConnectionData *scnc, GdaConnection *cnc,
				     GdaMetaStore *store, GdaMetaContext *context, GError **error);
static gboolean update_META_TABLES_VIEWS  (GdaSqliteProvider *provider, SqliteConnectionData *scnc, GdaConnection *cnc,
				     GdaMetaStore *store, GdaMetaContext *context, GError **error);
static gboolean update_META_COLUMNS (GdaSqliteProvider *provider, SqliteConnectionData *scnc, GdaConnection *cnc,
				     GdaMetaStore *store, GdaMetaContext *context, GError **error);
static gboolean update_META_BUILTIN_TYPES (GdaSqliteProvider *provider, SqliteConnectionData *scnc, GdaConnection *cnc,
					   GdaMetaStore *store, GdaMetaContext *context, GError **error);


/*
 * predefined statements' IDs
 */
typedef enum {
        I_PRAGMA_DATABASE_LIST,
	I_PRAGMA_TABLE_INFO
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	"PRAGMA database_list",
	"PRAGMA table_info (##tblname::string)"
};

/*
 * predefined statements' GdaStatement
 */
static GdaStatement **internal_stmt;
static GdaSet        *internal_params;

/*
 * Methods hash
 */
static GHashTable    *methods;

/* 
 * global values
 */
GdaSqlParser *internal_parser = NULL;
GValue       *catalog_value;
GValue       *table_type_value;
GValue       *view_type_value;
GValue       *view_check_option;
GValue       *false_value;
GdaSet       *pragma_set;

/*
 * Meta initialization
 */
void
_gda_sqlite_provider_meta_init (GdaServerProvider *provider)
{
	InternalStatementItem i;

        internal_parser = gda_server_provider_internal_get_parser (provider);
	internal_params = gda_set_new (NULL);

        internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
        for (i = I_PRAGMA_DATABASE_LIST; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
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

	methods = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (methods, "_information_schema_catalog_name", update_META_CAT);
	g_hash_table_insert (methods, "_schemata", update_META_SCHEMAS);
	g_hash_table_insert (methods, "_tables", update_META_TABLES_VIEWS);
	g_hash_table_insert (methods, "_columns", update_META_COLUMNS);
	g_hash_table_insert (methods, "_builtin_data_types", update_META_BUILTIN_TYPES);

	catalog_value = gda_value_new (G_TYPE_STRING);
	g_value_set_string (catalog_value, "main");

	g_value_set_string ((table_type_value = gda_value_new (G_TYPE_STRING)), "BASE TABLE");
	g_value_set_string ((view_type_value = gda_value_new (G_TYPE_STRING)), "VIEW");
	g_value_set_string ((view_check_option = gda_value_new (G_TYPE_STRING)), "NONE");
	g_value_set_boolean ((false_value = gda_value_new (G_TYPE_BOOLEAN)), FALSE);

	pragma_set = gda_set_new_inline (1, "tblname", G_TYPE_STRING, "");
}

/*
 * Main dispatcher
 */
gboolean 
_gda_sqlite_provider_meta_update (GdaServerProvider *provider, GdaConnection *cnc,
				  GdaMetaContext *context, GError **error)
{
	SqliteConnectionData *scnc;
	GdaMetaStore *store;
	UpdateFunc func;

	scnc = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	g_assert (scnc);

#ifdef GDA_DEBUG
	gint i;
	g_print ("Requested SQLITE meta update for table %s\n", context->table_name);
	for (i = 0; i < context->size; i++) {
		gchar *str = gda_value_stringify (context->column_values [i]);
		g_print ("\t%s = %s\n", context->column_names [i], str);
		g_free (str);
	}
#endif

	store = gda_connection_get_meta_store (cnc);
	func = g_hash_table_lookup (methods, context->table_name);
	if (func)
		return func ((GdaSqliteProvider*) provider, scnc, cnc, store, context, error);
	else {
		g_print ("For meta on table '%s' (maybe don't needed...) ", context->table_name);
		TO_IMPLEMENT;
	}
	return TRUE;
}

/*
 * params: 
 *  - none
 */
static gboolean
update_META_CAT (GdaSqliteProvider *provider, SqliteConnectionData *scnc, GdaConnection *cnc,
		 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* actual data computation */
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	retval = append_a_row (model, error, 1, FALSE, catalog_value);
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);
	return retval;
}

/*
 * params: 
 *  - none
 *  - @schema_name
 */
static gboolean
update_META_SCHEMAS (GdaSqliteProvider *provider, SqliteConnectionData *scnc, GdaConnection *cnc,
		     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* context's parameters analysis */
	GValue *p_schema_name = NULL;
	if (check_parameters (context, error, 2,
			      &p_schema_name, G_TYPE_STRING, NULL,
			      "schema_name", &p_schema_name, NULL,
			      NULL) < 0)
		return FALSE;

	/* actual data computation */
	GdaDataModel *model, *tmpmodel;
	gboolean retval = TRUE;
	gint nrows, i;
	tmpmodel = (GdaDataModel *) gda_connection_statement_execute (cnc, internal_stmt[I_PRAGMA_DATABASE_LIST],
								      NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, error);
	if (!tmpmodel)
		return FALSE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; (i < nrows) && retval; i++) {
		const GValue *cvalue;
		
		cvalue = gda_data_model_get_value_at (tmpmodel, 1, i);
		if (!p_schema_name || 
		    !gda_value_compare_ext (p_schema_name, cvalue)) {
			const gchar *cstr;
			GValue *v1;

			cstr = g_value_get_string (cvalue);
			if (!cstr || !strncmp (cstr, "temp", 4))
				continue;

			g_value_set_boolean ((v1 = gda_value_new (G_TYPE_BOOLEAN)), FALSE);
			retval = append_a_row (model, error, 4, 
					       FALSE, catalog_value,
					       FALSE, cvalue, 
					       FALSE, NULL,
					       TRUE, v1);
		}
	}
	g_object_unref (tmpmodel);
	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}


/*
 * params: 
 *  - none
 *  - @table_schema
 *  - @table_schema AND @table_name
 */
static gboolean update_META_TABLES_VIEWS_for_schema (GdaConnection *cnc, 
						     GdaDataModel *to_tables_model, GdaDataModel *to_views_model, 
						     const GValue *p_table_schema, const GValue *p_table_name,
						     GError **error);
static gboolean
update_META_TABLES_VIEWS (GdaSqliteProvider *provider, SqliteConnectionData *scnc, GdaConnection *cnc,
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* context's parameters analysis */
	const GValue *p_table_schema = NULL;
	const GValue *p_table_name = NULL;
	if (check_parameters (context, error, 3,
			      &p_table_schema, G_TYPE_STRING,
			      &p_table_name, G_TYPE_STRING, NULL,
			      "table_schema", &p_table_schema, "table_name", &p_table_name, NULL,
			      "table_schema", &p_table_schema, NULL,
			      NULL) < 0)
		return FALSE;
	
	/* actual data computation */
	GdaDataModel *tables_model, *views_model;
	gboolean retval = TRUE;
	tables_model = gda_meta_store_create_modify_data_model (store, "_tables");
	g_assert (tables_model);
	views_model = gda_meta_store_create_modify_data_model (store, "_views");
	g_assert (views_model);
	if (p_table_schema) {
		if (! update_META_TABLES_VIEWS_for_schema (cnc, tables_model, views_model, 
							   p_table_schema, p_table_name, error))
			retval = FALSE;
	}
	else {
		GdaDataModel *sh_model;

		sh_model = gda_connection_get_meta_store_data (cnc, GDA_CONNECTION_META_NAMESPACES, error, 0);
		if (!sh_model)
			retval = TRUE;
		else {
			gint nrows, i;
			nrows = gda_data_model_get_n_rows (sh_model);
			for (i = 0; i < nrows; i++) {
				p_table_schema = gda_data_model_get_value_at (sh_model, 0, i);
				if (!update_META_TABLES_VIEWS_for_schema (cnc, tables_model, views_model, 
									  p_table_schema, NULL, error)) {
					retval = FALSE;
					break;
				}
			}
			g_object_unref (sh_model);
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

/*
 * Warning: this is a hack where it is assumed that the GValue values from SQLite's SELECT execution are available
 * event _after_ several calls to gda_data_model_get_value_at(), which might not be TRUE in all the cases.
 *
 * For this purpose, the @models list contains the list of data models which then have to be destroyed.
 */
static gboolean
update_META_TABLES_VIEWS_for_schema (GdaConnection *cnc, 
				     GdaDataModel *to_tables_model, GdaDataModel *to_views_model,
				     const GValue *p_table_schema, const GValue *p_table_name,
				     GError **error)
{
	gchar *str;
	GdaDataModel *tmpmodel;
	gboolean retval = TRUE;
	gint nrows, i;
	GdaStatement *stmt;
	GType col_types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_NONE};
	const gchar *schema_name;

	schema_name = g_value_get_string (p_table_schema);
	if (!strcmp (schema_name, "temp"))
		return TRUE; /* nothing to do */
	
	str = g_strdup_printf ("SELECT tbl_name, type, sql FROM %s.sqlite_master where type='table' OR type='view'", 
			       schema_name);
	stmt = gda_sql_parser_parse_string (internal_parser, str, NULL, NULL);
	g_free (str);
	g_assert (stmt);
	tmpmodel = gda_connection_statement_execute_select_full (cnc, stmt, NULL, 
								 GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
								 col_types, error);
	g_object_unref (stmt);
	if (!tmpmodel)
		return FALSE;
	
	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; (i < nrows) && retval; i++) {
		const GValue *cvalue;
	       
		cvalue = gda_data_model_get_value_at (tmpmodel, 0, i);
		if (!p_table_name || 
		    !gda_value_compare_ext (p_table_name, cvalue)) {
			GValue *v1, *v2 = NULL;
			const GValue *tvalue;
			const GValue *dvalue;
			gboolean is_view = FALSE;
			gchar *this_table_name;

			this_table_name = g_value_get_string (cvalue);
			g_assert (this_table_name);
			if (!strcmp (this_table_name, "sqlite_sequence"))
				continue; /* ignore that table */

			tvalue = gda_data_model_get_value_at (tmpmodel, 1, i);
			dvalue = gda_data_model_get_value_at (tmpmodel, 2, i);
			if (*(g_value_get_string (tvalue)) == 'v')
				is_view = TRUE;
			g_value_set_boolean ((v1 = gda_value_new (G_TYPE_BOOLEAN)), TRUE);
			if (strcmp (schema_name, "main")) {
				str = g_strdup_printf ("%s.%s", schema_name, g_value_get_string (cvalue));
				g_value_take_string ((v2 = gda_value_new (G_TYPE_STRING)), str);
			}
			if (! append_a_row (to_tables_model, error, 9, 
					    FALSE, catalog_value,
					    FALSE, p_table_schema,
					    FALSE, cvalue,
					    FALSE, is_view ? view_type_value : table_type_value,
					    TRUE, v1,
					    FALSE, NULL,
					    FALSE, cvalue,
					    v2 ? TRUE : FALSE, v2 ? v2 : cvalue,
					    FALSE, NULL))
				retval = FALSE;
			if (is_view && ! append_a_row (to_views_model, error, 6, 
						       FALSE, catalog_value,
						       FALSE, p_table_schema,
						       FALSE, cvalue,
						       FALSE, dvalue, 
						       FALSE, view_check_option,
						       FALSE, false_value))
				retval = FALSE;
		}
	}
	g_object_unref (tmpmodel);

	return retval;
}

/*
 * params: 
 *  - none
 *  - @table_schema AND @table_name
 *  - @table_schema AND @table_name AND @column_name
 */
static gboolean update_META_COLUMNS_for_table (GdaConnection *cnc, SqliteConnectionData *scnc, 
					       GdaDataModel *mod_model, 
					       const GValue *p_table_schema, const GValue *p_table_name,
					       const GValue *p_column_name, GError **error);
static gboolean
update_META_COLUMNS (GdaSqliteProvider *provider, SqliteConnectionData *scnc, GdaConnection *cnc,
		     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* context's parameters analysis */
	const GValue *p_table_schema = NULL;
	const GValue *p_table_name = NULL;
	const GValue *p_column_name = NULL;

	if (check_parameters (context, error, 3,
			      &p_table_schema, G_TYPE_STRING,
			      &p_table_name, G_TYPE_STRING, 
			      &p_column_name, G_TYPE_STRING, NULL,
			      "table_schema", &p_table_schema, "table_name", &p_table_name, "column_name", &p_column_name, NULL,
			      "table_schema", &p_table_schema, "table_name", &p_table_name, NULL,
			      NULL) < 0)
		return FALSE;
	
	/* actual data computation */
	gboolean retval = TRUE;
	GdaDataModel *mod_model = NULL;

	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	if (p_table_name) {
		g_assert (mod_model);
		retval = update_META_COLUMNS_for_table (cnc, scnc, mod_model, p_table_schema, 
							p_table_name, p_column_name, error);
	}
	else {
		GdaDataModel *tmpmodel;

		tmpmodel = gda_connection_get_meta_store_data (cnc, GDA_CONNECTION_META_TABLES, error, 0);
		if (!tmpmodel)
			retval = TRUE;
		else {
			gint nrows, i;
			nrows = gda_data_model_get_n_rows (tmpmodel);
			for (i = 0; i < nrows; i++) {
				p_table_name = gda_data_model_get_value_at (tmpmodel, 0, i);
				p_table_schema = gda_data_model_get_value_at (tmpmodel, 1, i);
				if (!update_META_COLUMNS_for_table (cnc, scnc, mod_model, p_table_schema, 
								    p_table_name, NULL, error)) {
					retval = FALSE;
					break;
				}
			}
			g_object_unref (tmpmodel);
		}
	}
	
	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
	g_object_unref (mod_model);

	return retval;
}

static gboolean 
update_META_COLUMNS_for_table (GdaConnection *cnc, SqliteConnectionData *scnc, 
			       GdaDataModel *mod_model, 
			       const GValue *p_table_schema, const GValue *p_table_name,
			       const GValue *p_column_name, GError **error)
{
	GdaDataModel *tmpmodel;
	gboolean retval = TRUE;
	gint nrows;
	const gchar *schema_name;
	gint i;
	GType col_types[] = {G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, 
			     G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_NONE};
	
	schema_name = g_value_get_string (p_table_schema);
	if (strcmp (schema_name, "main")) {
		gchar *str;
		str = g_strdup_printf ("%s.%s", schema_name, g_value_get_string (p_table_name));
		gda_set_set_holder_value (pragma_set, "tblname", str);
		g_free (str);
	}
	else
		gda_set_set_holder_value (pragma_set, "tblname", g_value_get_string (p_table_name));

	tmpmodel = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_PRAGMA_TABLE_INFO], pragma_set, 
								 GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
								 col_types, error);
	if (!tmpmodel)
		return FALSE;
		
	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; i < nrows; i++) {
		GValue *v1, *v2, *v3, *v4, *v5 = NULL, *v6;
		char const *pzDataType; /* Declared data type */
		char const *pzCollSeq; /* Collation sequence name */
		int pNotNull; /* True if NOT NULL constraint exists */
		int pPrimaryKey; /* True if column part of PK */
		int pAutoinc; /* True if column is auto-increment */
		const gchar *this_table_name;
		const gchar *this_col_name;
		const GValue *this_col_pname;
		GType gtype = 0;
		
		this_col_pname = gda_data_model_get_value_at (tmpmodel, 1, i);
		if (p_column_name &&
		    gda_value_compare_ext (p_column_name, this_col_pname))
			continue;

		
		this_table_name = g_value_get_string (p_table_name);
		g_assert (this_table_name);
		if (!strcmp (this_table_name, "sqlite_sequence"))
			continue; /* ignore that table */
		
		this_col_name = g_value_get_string (this_col_pname);
		if (sqlite3_table_column_metadata (scnc->connection, g_value_get_string (p_table_schema), 
						   this_table_name, this_col_name,
						   &pzDataType, &pzCollSeq, &pNotNull, &pPrimaryKey, &pAutoinc)
		    != SQLITE_OK) {
			g_set_error (error, 0, 0,
				     _("Internal error"));
			retval = FALSE;
			break;
		}
		
		v1 = gda_value_copy (gda_data_model_get_value_at (tmpmodel, 0, i));
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), pzDataType);
		g_value_set_boolean ((v3 = gda_value_new (G_TYPE_BOOLEAN)), pNotNull ? FALSE : TRUE);
		g_value_set_string ((v4 = gda_value_new (G_TYPE_STRING)), pzCollSeq);
		if (pAutoinc)
			g_value_set_string ((v5 = gda_value_new (G_TYPE_STRING)), "AUTO_INCREMENT");
		g_value_set_int (v1, g_value_get_int (v1) + 1);

		
		if (pzDataType)
			gtype = GPOINTER_TO_INT (g_hash_table_lookup (scnc->types, pzDataType));
		if (gtype == 0) {
			g_warning ("Internal error: could not get GType for DBMS type '%s'", pzDataType);
			g_value_set_string ((v6 = gda_value_new (G_TYPE_STRING)), "string");
		}
		else
			g_value_set_string ((v6 = gda_value_new (G_TYPE_STRING)), g_type_name (gtype));
		if (! append_a_row (mod_model, error, 23, 
				    FALSE, catalog_value,
				    FALSE, p_table_schema,
				    FALSE, p_table_name,
				    FALSE, this_col_pname, /* column name */
				    TRUE, v1, /* ordinal_position */
				    FALSE, gda_data_model_get_value_at (tmpmodel, 4, i), /* column default */
				    TRUE, v3, /* is_nullable */
				    TRUE, v2, /* data_type */
				    TRUE, v6, /* gtype */
				    FALSE, NULL, /* character_maximum_length */
				    FALSE, NULL, /* character_octet_length */
				    FALSE, NULL, /* numeric_precision */
				    FALSE, NULL, /* numeric_scale */
				    FALSE, NULL, /* datetime_precision */
				    FALSE, NULL, /* character_set_catalog */
				    FALSE, NULL, /* character_set_schema */
				    FALSE, NULL, /* character_set_name */
				    FALSE, catalog_value, /* collation_catalog */
				    FALSE, catalog_value, /* collation_schema */
				    TRUE, v4, /* collation_name */
				    v5 ? TRUE : FALSE, v5, /* extra */
				    FALSE, NULL, /* is_updatable */
				    FALSE, NULL /* column_comments */))
			retval = FALSE;
	}

	g_object_unref (tmpmodel);
	return retval;
}

/*
 * params: 
 *  - none
 */
static gboolean
update_META_BUILTIN_TYPES (GdaSqliteProvider *provider, SqliteConnectionData *scnc, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* actual data computation */
	GdaDataModel *mod_model;
	gboolean retval = TRUE;
	gint i;
	typedef struct {
		gchar *tname;
		gchar *gtype;
		gchar *comments;
		gchar *synonyms;
	} InternalType;

	InternalType internal_types[] = {
		{"integer", "gint", "Signed integer, stored in 1, 2, 3, 4, 6, or 8 bytes depending on the magnitude of the value", "int"},
		{"real", "gdouble",  "Floating point value, stored as an 8-byte IEEE floating point number", NULL},
		{"text", "string", "Text string, stored using the database encoding", "string"},
		{"blob", "GdaBinary", "Blob of data, stored exactly as it was input", NULL},
		{"timestamp", "GdaTimestamp", "Time stamp, stored as 'YYYY-MM-DD HH:MM:SS.SSS'", NULL},
		{"time", "GdaTime", "Time, stored as 'HH:MM:SS.SSS'", NULL},
		{"date", "GDate", "Date, stored as 'YYYY-MM-DD'", NULL},
		{"boolean", "gboolean", "Boolean value", "bool"}
	};

	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	GValue *vint;
	g_value_set_boolean (vint = gda_value_new (G_TYPE_BOOLEAN), FALSE);
	for (i = 0; i < sizeof (internal_types) / sizeof (InternalType); i++) {
		GValue *v1, *v2, *v3, *v4;
		InternalType *it = &(internal_types[i]);

		g_value_set_string (v1 = gda_value_new (G_TYPE_STRING), it->tname);
		g_value_set_string (v2 = gda_value_new (G_TYPE_STRING), it->gtype);
		g_value_set_string (v3 = gda_value_new (G_TYPE_STRING), it->comments);
		if (it->synonyms)
			g_value_set_string (v4 = gda_value_new (G_TYPE_STRING), it->synonyms);
		else
			v4 = NULL;
		
		if (!append_a_row (mod_model, error, 6,
				   FALSE, v1, /* short_type_name */
				   TRUE, v1, /* full_type_name */
				   TRUE, v2, /* gtype */
				   TRUE, v3, /* comments */
				   TRUE, v4, /* synonyms */
				   FALSE, vint /* internal */)) {
			retval = FALSE;
			break;
		}
	}
	gda_value_free (vint);
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, mod_model, NULL, error, NULL);
	g_object_unref (mod_model);
	return retval;
}








/*
 *
 */
static gint
check_parameters (GdaMetaContext *context, GError **error, gint nb, ...)
{
#define MAX_PARAMS 10
	gint i;
	va_list ap;
	gint retval = -1;
	GValue **pvalue;
	struct {
		GValue **pvalue;
		GType    type;
	} spec_array [MAX_PARAMS];
	gint nb_params = 0;

	va_start (ap, nb);
	/* make a list of all the GValue pointers */
	for (pvalue = va_arg (ap, GValue **); pvalue; pvalue = va_arg (ap, GValue **), nb_params++) {
		g_assert (nb_params < MAX_PARAMS); /* hard limit, recompile to change it (should never be needed) */
		spec_array[nb_params].pvalue = pvalue;
		spec_array[nb_params].type = va_arg (ap, GType);
	}

	/* test against each test case */
	for (i = 0; i < nb; i++) {
		gchar *pname;
		gboolean allfound = TRUE;
		gint j;
		for (j = 0; j < nb_params; j++)
			*(spec_array[j].pvalue) = NULL;

		for (pname = va_arg (ap, gchar*); pname; pname = va_arg (ap, gchar*)) {
			gint j;
			pvalue = va_arg (ap, GValue **);
			*pvalue = NULL;
			for (j = 0; allfound && (j < context->size); j++) {
				if (!strcmp (context->column_names[j], pname)) {
					*pvalue = context->column_values[j];
					break;
				}
			}
			if (j == context->size)
				allfound = FALSE;
		}
		if (allfound) {
			retval = i;
			break;
		}
	}
	va_end (ap);

	if (retval >= 0) {
		gint j;
		for (j = 0; j < nb_params; j++) {
			GValue *v = *(spec_array[j].pvalue);
			if (v && (gda_value_is_null (v) || (G_VALUE_TYPE (v) != spec_array[j].type))) {
				g_set_error (error, 0, 0,
					     _("Invalid argument"));
				retval = -1;
			}
		}
	}
	else 
		g_set_error (error, 0, 0,
			     _("Missing and/or wrong arguments"));

	g_print ("Check context => %d\n", retval);
	return retval;
}

/*
 * @...: a list of TRUE/FALSE, GValue*  -- if TRUE then the following GValue must be freed
 */
static gboolean 
append_a_row (GdaDataModel *to_model, GError **error, gint nb, ...)
{
	va_list ap;
	GList *values = NULL;
	gint i;
	GValue **free_array;
	gboolean retval = TRUE;

	/* compute list of values */
	free_array = g_new0 (GValue *, nb);
	va_start (ap, nb);
	for (i = 0; i < nb; i++) {
		gboolean to_free;
		GValue *value;
		to_free = va_arg (ap, gboolean);
		value = va_arg (ap, GValue *);
		if (to_free)
			free_array[i] = value;
		values = g_list_prepend (values, value);
	}
	va_end (ap);
	values = g_list_reverse (values);
	
	/* add to model */
	if (gda_data_model_append_values (to_model, values, error) < 0)
		retval = FALSE;

	/* memory free */
	g_list_free (values);
	for (i = 0; i < nb; i++) {
		if (free_array[i])
			gda_value_free (free_array[i]);
	}
	g_free (free_array);
	return retval;
}
