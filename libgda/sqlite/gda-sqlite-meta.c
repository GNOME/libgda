/* Gda Sqlite provider
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

/*
 * predefined statements' IDs
 */
typedef enum {
        I_PRAGMA_DATABASE_LIST,
	I_PRAGMA_TABLE_INFO,
	I_PRAGMA_INDEX_LIST,
	I_PRAGMA_INDEX_INFO,
	I_PRAGMA_FK_LIST,
	I_PRAGMA_PROCLIST
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	/* I_PRAGMA_DATABASE_LIST */
	"PRAGMA database_list",

	/* I_PRAGMA_TABLE_INFO */
	"PRAGMA table_info (##tblname::string)",

	/* I_PRAGMA_INDEX_LIST */
	"PRAGMA index_list (##tblname::string)",

	/* I_PRAGMA_INDEX_INFO */
	"PRAGMA index_info (##idxname::string)",

	/* I_PRAGMA_FK_LIST */
	"PRAGMA foreign_key_list (##tblname::string)",

	/* I_PRAGMA_PROCLIST */
	"PRAGMA proc_list"
};
/* name of the temporary database we don't want */
#define TMP_DATABASE_NAME "temp"
/* SQL statement where we can't use a prepared statement since the table to select from is the variable */
#define SELECT_TABLES_VIEWS "SELECT tbl_name, type, sql FROM %s.sqlite_master where type='table' OR type='view'"

/*
 * predefined statements' GdaStatement
 */
static GdaStatement **internal_stmt;
static GdaSet        *internal_params;

/* 
 * global static values
 */
static GdaSqlParser *internal_parser = NULL;
static GValue       *catalog_value;
static GValue       *table_type_value;
static GValue       *view_type_value;
static GValue       *view_check_option;
static GValue       *false_value;
static GValue       *true_value;
static GValue       *zero_value;
static GValue       *rule_value;
static GdaSet       *pragma_set;

/*
 * Meta initialization
 */
void
_gda_sqlite_provider_meta_init (GdaServerProvider *provider)
{
	static GStaticMutex init_mutex = G_STATIC_MUTEX_INIT;
	InternalStatementItem i;

	g_static_mutex_lock (&init_mutex);

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

	g_value_set_string ((catalog_value = gda_value_new (G_TYPE_STRING)), "main");
	g_value_set_string ((table_type_value = gda_value_new (G_TYPE_STRING)), "BASE TABLE");
	g_value_set_string ((view_type_value = gda_value_new (G_TYPE_STRING)), "VIEW");
	g_value_set_string ((view_check_option = gda_value_new (G_TYPE_STRING)), "NONE");
	g_value_set_boolean ((false_value = gda_value_new (G_TYPE_BOOLEAN)), FALSE);
	g_value_set_boolean ((true_value = gda_value_new (G_TYPE_BOOLEAN)), TRUE);
	g_value_set_int ((zero_value = gda_value_new (G_TYPE_INT)), 0);
	g_value_set_string ((rule_value = gda_value_new (G_TYPE_STRING)), "NONE");

	pragma_set = gda_set_new_inline (2, "tblname", G_TYPE_STRING, "",
					 "idxname", G_TYPE_STRING, "");

	g_static_mutex_unlock (&init_mutex);
}

static GdaStatement *
get_statement (InternalStatementItem type, const gchar *schema_name, const gchar *obj_name, GError **error)
{
	GdaStatement *stmt;
	if (strcmp (schema_name, "main")) {
		gchar *str;
		
		switch (type) {
		case I_PRAGMA_TABLE_INFO:
			str = g_strdup_printf ("PRAGMA %s.table_info (%s)", schema_name, obj_name);
			break;
		case I_PRAGMA_INDEX_LIST:
			str = g_strdup_printf ("PRAGMA %s.index_list (%s)", schema_name, obj_name);
			break;
		case I_PRAGMA_INDEX_INFO:
			str = g_strdup_printf ("PRAGMA %s.index_info (%s)", schema_name, obj_name);
			break;
		case I_PRAGMA_FK_LIST:
			str = g_strdup_printf ("PRAGMA %s.foreign_key_list (%s)", schema_name, obj_name);
			break;
		default:
			g_assert_not_reached ();
		}
		
		stmt = gda_sql_parser_parse_string (internal_parser, str, NULL, NULL);
		g_free (str);
		g_assert (stmt);
	}
	else {
		switch (type) {
		case I_PRAGMA_TABLE_INFO:
		case I_PRAGMA_INDEX_LIST:
		case I_PRAGMA_FK_LIST:
			if (! gda_set_set_holder_value (pragma_set, error, "tblname", obj_name))
				return NULL;
			break;
		case I_PRAGMA_INDEX_INFO:
			if (! gda_set_set_holder_value (pragma_set, error, "idxname", obj_name))
				return NULL;
			break;
		default:
			g_assert_not_reached ();
		}

		stmt = g_object_ref (internal_stmt [type]);
	}

	return stmt;
}

gboolean
_gda_sqlite_meta__info (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
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

gboolean
_gda_sqlite_meta__btypes (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
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
		{"blob", "GdaBlob", "Blob of data, stored exactly as it was input", NULL},
		{"timestamp", "GdaTimestamp", "Time stamp, stored as 'YYYY-MM-DD HH:MM:SS.SSS'", NULL},
		{"time", "GdaTime", "Time, stored as 'HH:MM:SS.SSS'", NULL},
		{"date", "GDate", "Date, stored as 'YYYY-MM-DD'", NULL},
		{"boolean", "gboolean", "Boolean value", "bool"}
	};

	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

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
				   FALSE, false_value /* internal */)) {
			retval = FALSE;
			break;
		}
	}
	if (retval) 
		retval = gda_meta_store_modify (store, context->table_name, mod_model, NULL, error, NULL);
	g_object_unref (mod_model);

	return retval;
}

/* 
 * copied from SQLite's sources to determine column affinity 
 */
static gint
get_affinity (const gchar *type)
{
	guint32 h = 0;
	gint aff = SQLITE_TEXT;
	const unsigned char *ptr = (unsigned char *) type;
	
	while( *ptr ){
		h = (h<<8) + g_ascii_tolower(*ptr);
		ptr++;
		if( h==(('c'<<24)+('h'<<16)+('a'<<8)+'r') ){             /* CHAR */
			aff = SQLITE_TEXT;
		}else if( h==(('c'<<24)+('l'<<16)+('o'<<8)+'b') ){       /* CLOB */
			aff = SQLITE_TEXT;
		}else if( h==(('t'<<24)+('e'<<16)+('x'<<8)+'t') ){       /* TEXT */
			aff = SQLITE_TEXT;
		}else if( h==(('b'<<24)+('l'<<16)+('o'<<8)+'b')          /* BLOB */
			  && (aff==SQLITE_INTEGER || aff==SQLITE_FLOAT) ){
			aff = 0;
		}else if( h==(('r'<<24)+('e'<<16)+('a'<<8)+'l')          /* REAL */
			  && aff==SQLITE_INTEGER ){
			aff = SQLITE_FLOAT;
		}else if( h==(('f'<<24)+('l'<<16)+('o'<<8)+'a')          /* FLOA */
			  && aff==SQLITE_INTEGER ){
			aff = SQLITE_FLOAT;
		}else if( h==(('d'<<24)+('o'<<16)+('u'<<8)+'b')          /* DOUB */
			  && aff==SQLITE_INTEGER ){
			aff = SQLITE_FLOAT;
		}else if( (h&0x00FFFFFF)==(('i'<<16)+('n'<<8)+'t') ){    /* INT */
			aff = SQLITE_INTEGER;
			break;
		}
	}
	
	return aff;
}

static gboolean
fill_udt_model (SqliteConnectionData *cdata, GHashTable *added_hash, 
		GdaDataModel *mod_model, const GValue *p_udt_schema, GError **error)
{
	gint status;
	sqlite3_stmt *tables_stmt = NULL;
	gchar *str;
	const gchar *cstr;
	gboolean retval = TRUE;

	cstr = g_value_get_string (p_udt_schema);
	str = g_strdup_printf ("SELECT name FROM %s.sqlite_master WHERE type='table' AND name not like 'sqlite_%%'", cstr);
	status = sqlite3_prepare_v2 (cdata->connection, str, -1, &tables_stmt, NULL);
	g_free (str);
	if ((status != SQLITE_OK) || !tables_stmt)
		return FALSE;
	
	if (!cdata->types)
                _gda_sqlite_compute_types_hash (cdata);

	for (status = sqlite3_step (tables_stmt); status == SQLITE_ROW; status = sqlite3_step (tables_stmt)) {
		gchar *sql;
		sqlite3_stmt *fields_stmt;
		gint fields_status;

		if (strcmp (cstr, "main")) 
			sql = g_strdup_printf ("PRAGMA %s.table_info(%s);", cstr, sqlite3_column_text (tables_stmt, 0));
		else
			sql = g_strdup_printf ("PRAGMA table_info('%s');", sqlite3_column_text (tables_stmt, 0));
		fields_status = sqlite3_prepare_v2 (cdata->connection, sql, -1, &fields_stmt, NULL);
		g_free (sql);
		if ((fields_status != SQLITE_OK) || !fields_stmt)
			break;
		
		for (fields_status = sqlite3_step (fields_stmt); fields_status == SQLITE_ROW; 
		     fields_status = sqlite3_step (fields_stmt)) {
			GType gtype;
			const gchar *typname = (gchar *) sqlite3_column_text (fields_stmt, 2);
			if (!typname || !(*typname)) 
				continue;

			gtype = GPOINTER_TO_INT (g_hash_table_lookup (cdata->types, typname));
			if ((gtype == GDA_TYPE_NULL) && !g_hash_table_lookup (added_hash, typname)) {
				GValue *vname, *vgtyp;

				gtype = _gda_sqlite_compute_g_type (get_affinity (typname));
				g_value_set_string ((vname = gda_value_new (G_TYPE_STRING)), typname);
				g_value_set_string ((vgtyp = gda_value_new (G_TYPE_STRING)), g_type_name (gtype));

				if (!append_a_row (mod_model, error, 9,
						   FALSE, catalog_value, /* udt_catalog */
						   FALSE, p_udt_schema, /* udt_schema */
						   FALSE, vname, /* udt_name */
						   TRUE, vgtyp, /* udt_gtype */
						   TRUE, NULL, /* udt_comments */
						   FALSE, vname, /* udt_short_name */
						   TRUE, vname, /* udt_full_name */
						   FALSE, false_value, /* udt_internal */
						   FALSE, NULL /* udt_owner */)) {
					retval = FALSE;
					break;
				}
				g_hash_table_insert (added_hash, g_strdup (typname), GINT_TO_POINTER (1));
			}
		}
		sqlite3_finalize (fields_stmt);
	}
	sqlite3_finalize (tables_stmt);

	return retval;
}

static guint
nocase_str_hash (gconstpointer v)
{
	guint ret;
	gchar *up = g_ascii_strup ((gchar *) v, -1);
	ret = g_str_hash ((gconstpointer) up);
	g_free (up);
	return ret;
}

static gboolean
nocase_str_equal (gconstpointer v1, gconstpointer v2)
{
	return g_ascii_strcasecmp ((gchar *) v1, (gchar *) v2) == 0 ? TRUE : FALSE;
}

gboolean
_gda_sqlite_meta__udt (GdaServerProvider *prov, GdaConnection *cnc, 
		       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	SqliteConnectionData *cdata;
	GdaDataModel *mod_model = NULL;
	gboolean retval = TRUE;

	GHashTable *added_hash;
	GdaDataModel *tmpmodel;
	gint i, nrows;

	/* get connection's private data */
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	/* get list of schemas */
	tmpmodel = (GdaDataModel *) gda_connection_statement_execute (cnc, internal_stmt[I_PRAGMA_DATABASE_LIST],
                                                                      NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, error);
        if (!tmpmodel)
                return FALSE;

	/* prepare modification model */
	added_hash = g_hash_table_new_full (nocase_str_hash, nocase_str_equal, g_free, NULL); 
	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	nrows = gda_data_model_get_n_rows (tmpmodel);
        for (i = 0; i < nrows; i++) {
		/* iterate through the schemas */
                const GValue *cvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);
		if (!cvalue) {
			retval = FALSE;
                        break;
		}
                if (!strcmp (g_value_get_string (cvalue), TMP_DATABASE_NAME))
			continue; /* nothing to do */
		if (! fill_udt_model (cdata, added_hash, mod_model, cvalue, error)) {
			retval = FALSE;
                        break;
		}
	}
	g_object_unref (tmpmodel);
	g_hash_table_destroy (added_hash);

	/* actually use mod_model */
	if (retval) 
		retval = gda_meta_store_modify (store, context->table_name, mod_model, NULL, error, NULL);
	g_object_unref (mod_model);

	return retval;
}

gboolean
_gda_sqlite_meta_udt (GdaServerProvider *prov, GdaConnection *cnc, 
		      GdaMetaStore *store, GdaMetaContext *context, GError **error,
		      const GValue *udt_catalog, const GValue *udt_schema)
{
	SqliteConnectionData *cdata;
	GdaDataModel *mod_model = NULL;
	gboolean retval = TRUE;

	GHashTable *added_hash;

	/* get connection's private data */
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	/* prepare modification model */
	added_hash = g_hash_table_new_full (nocase_str_hash, nocase_str_equal, g_free, NULL); 
	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	if (! fill_udt_model (cdata, added_hash, mod_model, udt_schema, error))
		retval = FALSE;

	g_hash_table_destroy (added_hash);

	/* actually use mod_model */
	if (retval) 
		retval = gda_meta_store_modify (store, context->table_name, mod_model, NULL, error, NULL);
	g_object_unref (mod_model);

	return retval;
}

gboolean
_gda_sqlite_meta__udt_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* feature not supported by SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta_udt_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	/* feature not supported by SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta__enums (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* feature not supported by SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta_enums (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	/* feature not supported by SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta__domains (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* feature not supported by SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta_domains (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error,
			  const GValue *domain_catalog, const GValue *domain_schema)
{
	/* feature not supported by SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta__constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
				   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* feature not supported by SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta_constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
				  GdaMetaStore *store, GdaMetaContext *context, GError **error,
				  const GValue *domain_catalog, const GValue *domain_schema, 
				  const GValue *domain_name)
{
	/* feature not supported by SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta__el_types (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* feature not supported by SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta_el_types (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *specific_name)
{
	/* feature not supported by SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta__collations (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* FIXME: We need to do something similar to what's done with
	 * functions and aggregates as there is no pragma or API */
	return TRUE;
}

gboolean
_gda_sqlite_meta_collations (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *collation_catalog, const GValue *collation_schema, 
			     const GValue *collation_name_n)
{
	/* FIXME: We need to do something similar to what's done with
	 * functions and aggregates as there is no pragma or API */
	return TRUE;
}

gboolean
_gda_sqlite_meta__character_sets (GdaServerProvider *prov, GdaConnection *cnc, 
				  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* FIXME: We need to do something similar to what's done with
	 * functions and aggregates as there is no pragma or API */
	return TRUE;
}

gboolean
_gda_sqlite_meta_character_sets (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error,
				 const GValue *chset_catalog, const GValue *chset_schema, 
				 const GValue *chset_name_n)
{
	/* FIXME: We need to do something similar to what's done with
	 * functions and aggregates as there is no pragma or API */
	return TRUE;
}

gboolean
_gda_sqlite_meta__schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	return _gda_sqlite_meta_schemata (prov, cnc, store, context, error,
					  NULL, NULL);
}

gboolean 
_gda_sqlite_meta_schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *catalog_name, const GValue *schema_name_n)
{
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
		
		cvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);
		if (!cvalue) {
			retval = FALSE;
                        break;
		}
		if (!schema_name_n || 
		    !gda_value_compare (schema_name_n, cvalue)) {
			const gchar *cstr;
			GValue *v1;

			cstr = g_value_get_string (cvalue);
			if (!cstr || !strncmp (cstr, TMP_DATABASE_NAME, 4))
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
 * Warning: this is a hack where it is assumed that the GValue values from SQLite's SELECT execution are available
 * event _after_ several calls to gda_data_model_get_value_at(), which might not be TRUE in all the cases.
 *
 * For this purpose, the @models list contains the list of data models which then have to be destroyed.
 */
static gboolean 
fill_tables_views_model (GdaConnection *cnc,
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
        if (!strcmp (schema_name, TMP_DATABASE_NAME))
                return TRUE; /* nothing to do */

        str = g_strdup_printf (SELECT_TABLES_VIEWS, schema_name);
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

                cvalue = gda_data_model_get_value_at (tmpmodel, 0, i, error);
		if (!cvalue) {
			retval = FALSE;
                        break;
		}
                if (!p_table_name ||
                    !gda_value_compare (p_table_name, cvalue)) {
                        GValue *v1, *v2 = NULL;
                        const GValue *tvalue;
                        const GValue *dvalue;
                        gboolean is_view = FALSE;
                        const gchar *this_table_name;

                        this_table_name = g_value_get_string (cvalue);
                        g_assert (this_table_name);
			if (!strcmp (this_table_name, "sqlite_sequence"))
                                continue; /* ignore that table */

                        tvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);
			if (!tvalue) {
				retval = FALSE;
				break;
			}
                        dvalue = gda_data_model_get_value_at (tmpmodel, 2, i, error);
			if (!dvalue) {
				retval = FALSE;
				break;
			}
                        if (*(g_value_get_string (tvalue)) == 'v')
                                is_view = TRUE;
                        g_value_set_boolean ((v1 = gda_value_new (G_TYPE_BOOLEAN)), TRUE);
			str = g_strdup_printf ("%s.%s", schema_name, g_value_get_string (cvalue));
			g_value_take_string ((v2 = gda_value_new (G_TYPE_STRING)), str);
                        if (! append_a_row (to_tables_model, error, 9,
                                            FALSE, catalog_value, /* table_catalog */
                                            FALSE, p_table_schema, /* table_schema */
                                            FALSE, cvalue, /* table_name */
                                            FALSE, is_view ? view_type_value : table_type_value, /* table_type */
                                            TRUE, v1, /* is_insertable_into */
                                            FALSE, NULL, /* table_comments */
                                            FALSE, cvalue, /* table_short_name */
                                            TRUE, v2, /* table_full_name */
                                            FALSE, NULL)) /* table_owner */
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

gboolean
_gda_sqlite_meta__tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *tmpmodel;
	GdaDataModel *tables_model, *views_model;
	gboolean retval = TRUE;
	gint i, nrows;

	tmpmodel = (GdaDataModel *) gda_connection_statement_execute (cnc, internal_stmt[I_PRAGMA_DATABASE_LIST],
								      NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, error);
	if (!tmpmodel)
		return FALSE;

	tables_model = gda_meta_store_create_modify_data_model (store, "_tables");
	g_assert (tables_model);
	views_model = gda_meta_store_create_modify_data_model (store, "_views");
	g_assert (views_model);

	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; i < nrows; i++) {
		/* iterate through the schemas */
		const GValue *cvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		if (!strcmp (g_value_get_string (cvalue), TMP_DATABASE_NAME))
			continue; /* nothing to do */

		if (! fill_tables_views_model (cnc, tables_model, views_model, 
					       cvalue, NULL, error)) {
			retval = FALSE;
			break;
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
	
	g_object_unref (tmpmodel);
	return retval;
}

gboolean 
_gda_sqlite_meta_tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			       const GValue *table_catalog, const GValue *table_schema, const GValue *table_name_n)
{
	GdaDataModel *tables_model, *views_model;
	gboolean retval = TRUE;
	tables_model = gda_meta_store_create_modify_data_model (store, "_tables");
	g_assert (tables_model);
	views_model = gda_meta_store_create_modify_data_model (store, "_views");
	g_assert (views_model);

	if (! fill_tables_views_model (cnc, tables_model, views_model, table_schema, table_name_n, error))
		retval = FALSE;

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

static gboolean 
fill_columns_model (GdaConnection *cnc, SqliteConnectionData *cdata, 
		    GdaDataModel *mod_model, 
		    const GValue *p_table_schema, const GValue *p_table_name,
		    GError **error)
{
	GdaDataModel *tmpmodel;
	gboolean retval = TRUE;
	gint nrows;
	const gchar *schema_name;
	gint i;
	GType col_types[] = {G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, 
			     G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_NONE};
	GdaStatement *stmt;
	
	schema_name = g_value_get_string (p_table_schema);
	stmt = get_statement (I_PRAGMA_TABLE_INFO, schema_name, g_value_get_string (p_table_name), error);
	tmpmodel = gda_connection_statement_execute_select_full (cnc, stmt, pragma_set, 
								 GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
								 col_types, error);
	g_object_unref (stmt);
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
		const GValue *cvalue;
		
		this_col_pname = gda_data_model_get_value_at (tmpmodel, 1, i, error);
		if (!this_col_pname) {
			retval = FALSE;
			break;
		}
		this_table_name = g_value_get_string (p_table_name);
		g_assert (this_table_name);
		if (!strcmp (this_table_name, "sqlite_sequence"))
			continue; /* ignore that table */
		
		this_col_name = g_value_get_string (this_col_pname);
		if (sqlite3_table_column_metadata (cdata->connection, g_value_get_string (p_table_schema), 
						   this_table_name, this_col_name,
						   &pzDataType, &pzCollSeq, &pNotNull, &pPrimaryKey, &pAutoinc)
		    != SQLITE_OK) {
			/* may fail because we have a view and not a table => use @tmpmodel to fetch info. */
			cvalue = gda_data_model_get_value_at (tmpmodel, 2, i, error);
			if (!cvalue) {
				retval = FALSE;
				break;
			}	
			pzDataType = g_value_get_string (cvalue);
			pzCollSeq = NULL;
			cvalue = gda_data_model_get_value_at (tmpmodel, 3, i, error);
			if (!cvalue) {
				retval = FALSE;
				break;
			}
			pNotNull = g_value_get_int (cvalue);
			cvalue = gda_data_model_get_value_at (tmpmodel, 5, i, error);
			if (!cvalue) {
				retval = FALSE;
				break;
			}
			pPrimaryKey = g_value_get_boolean (cvalue);
			pAutoinc = 0;
		}
		
		cvalue = gda_data_model_get_value_at (tmpmodel, 0, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		v1 = gda_value_copy (cvalue);
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), pzDataType);
		g_value_set_boolean ((v3 = gda_value_new (G_TYPE_BOOLEAN)), pNotNull ? FALSE : TRUE);
		g_value_set_string ((v4 = gda_value_new (G_TYPE_STRING)), pzCollSeq);
		if (pAutoinc)
			g_value_set_string ((v5 = gda_value_new (G_TYPE_STRING)), GDA_EXTRA_AUTO_INCREMENT);
		g_value_set_int (v1, g_value_get_int (v1) + 1);
		
		if (pzDataType) {
			gchar *tmp = g_strdup (pzDataType);
			gchar *ptr;
			for (ptr = tmp; *ptr && (*ptr != '(') && (*ptr != '['); ptr++);
			if (*ptr)
				*ptr = 0;
			gtype = GPOINTER_TO_INT (g_hash_table_lookup (cdata->types, tmp));
			g_free (tmp);
		}
		if (gtype == 0) 
			/* default to string if nothing else */
			g_value_set_string ((v6 = gda_value_new (G_TYPE_STRING)), "string");
		else
			g_value_set_string ((v6 = gda_value_new (G_TYPE_STRING)), g_type_name (gtype));
		cvalue = gda_data_model_get_value_at (tmpmodel, 4, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		if (! append_a_row (mod_model, error, 24, 
				    FALSE, catalog_value, /* table_catalog */
				    FALSE, p_table_schema, /* table_schema */
				    FALSE, p_table_name, /* table_name */
				    FALSE, this_col_pname, /* column name */
				    TRUE, v1, /* ordinal_position */
				    FALSE, cvalue, /* column default */
				    TRUE, v3, /* is_nullable */
				    TRUE, v2, /* data_type */
				    FALSE, NULL, /* array_spec */
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

gboolean
_gda_sqlite_meta__columns (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *mod_model, *tmpmodel;
	gboolean retval = TRUE;
	gint i, nrows;
	SqliteConnectionData *cdata;
	
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	tmpmodel = (GdaDataModel *) gda_connection_statement_execute (cnc, internal_stmt[I_PRAGMA_DATABASE_LIST],
								      NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, error);
	if (!tmpmodel)
		return FALSE;

	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; i < nrows; i++) {
		/* iterate through the schemas */
		GdaDataModel *tables_model;
		const gchar *schema_name;
		const GValue *cvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		schema_name = g_value_get_string (cvalue); 
		if (!strcmp (schema_name, TMP_DATABASE_NAME))
			continue; /* nothing to do */
		
		gchar *str;
		GType col_types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_NONE};
		GdaStatement *stmt;
		
		str = g_strdup_printf (SELECT_TABLES_VIEWS, schema_name);
		stmt = gda_sql_parser_parse_string (internal_parser, str, NULL, NULL);
		g_free (str);
		g_assert (stmt);
		tables_model = gda_connection_statement_execute_select_full (cnc, stmt, NULL,
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									     col_types, error);
		g_object_unref (stmt);
		if (!tables_model) {
			retval = FALSE;
			break;
		}

		gint tnrows, ti;
		tnrows = gda_data_model_get_n_rows (tables_model);
		for (ti = 0; ti < tnrows; ti++) {
			/* iterate through the tables */
			const GValue *cv;
			cv = gda_data_model_get_value_at (tables_model, 0, ti, error);
			if (!cv) {
				retval = FALSE;
				break;
			}
			if (!fill_columns_model (cnc, cdata, mod_model, cvalue, cv, error)) {
				retval = FALSE;
				break;
			}
		}
		g_object_unref (tables_model);
		if (!retval) 
			break;
	}
	g_object_unref (tmpmodel);

	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
	g_object_unref (mod_model);

	return retval;
}

gboolean
_gda_sqlite_meta_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			  const GValue *table_catalog, const GValue *table_schema, const GValue *table_name)
{
	gboolean retval = TRUE;
	GdaDataModel *mod_model = NULL;
	SqliteConnectionData *cdata;
	
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	retval = fill_columns_model (cnc, cdata, mod_model, table_schema, table_name, error);	
	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
	g_object_unref (mod_model);

	return retval;
}

gboolean
_gda_sqlite_meta__view_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* FIXME: feature not natively supported by SQLite => parse view's definition ? */
	return TRUE;
}

gboolean
_gda_sqlite_meta_view_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *view_catalog, const GValue *view_schema, 
			    const GValue *view_name)
{
	/* FIXME: feature not natively supported by SQLite => parse view's definition ? */
	return TRUE;	
}

static gboolean 
fill_constraints_tab_model (GdaConnection *cnc, SqliteConnectionData *cdata, GdaDataModel *mod_model, 
			    const GValue *p_table_schema, const GValue *p_table_name, const GValue *constraint_name_n,
			    GError **error)
{
	GdaDataModel *tmpmodel;
	gboolean retval = TRUE;
	gint nrows;
	const gchar *schema_name;
	gint i;
	GdaStatement *stmt;

	schema_name = g_value_get_string (p_table_schema);

	/* 
	 * PRIMARY KEY
	 *
	 * SQLite only allows 1 primary key to be defined per table.
	 */
	GType pk_col_types[] = {G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, 
				G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_NONE};
	gboolean has_pk = FALSE;

	stmt = get_statement (I_PRAGMA_TABLE_INFO, schema_name, g_value_get_string (p_table_name), error);
	tmpmodel = gda_connection_statement_execute_select_full (cnc, stmt, pragma_set, 
								 GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
								 pk_col_types, error);
	g_object_unref (stmt);
	if (!tmpmodel)
		return FALSE;
		
	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; i < nrows; i++) {
		char const *pzDataType; /* Declared data type */
		char const *pzCollSeq; /* Collation sequence name */
		int pNotNull; /* True if NOT NULL constraint exists */
		int pPrimaryKey; /* True if column part of PK */
		int pAutoinc; /* True if column is auto-increment */
		const gchar *this_table_name;
		const gchar *this_col_name;
		const GValue *this_col_pname, *cvalue;
		
		this_col_pname = gda_data_model_get_value_at (tmpmodel, 1, i, error);
		if (!this_col_pname) {
			retval = FALSE;
			break;
		}
		this_table_name = g_value_get_string (p_table_name);
		g_assert (this_table_name);
		if (!strcmp (this_table_name, "sqlite_sequence"))
			continue; /* ignore that table */
		
		this_col_name = g_value_get_string (this_col_pname);
		if (sqlite3_table_column_metadata (cdata->connection, g_value_get_string (p_table_schema), 
						   this_table_name, this_col_name,
						   &pzDataType, &pzCollSeq, &pNotNull, &pPrimaryKey, &pAutoinc)
		    != SQLITE_OK) {
			/* may fail because we have a view and not a table => use @tmpmodel to fetch info. */
			cvalue = gda_data_model_get_value_at (tmpmodel, 5, i, error);
			if (!cvalue) {
				retval = FALSE;
				break;
			}
			pPrimaryKey = g_value_get_boolean (cvalue);
		}
		
		if (pPrimaryKey) {
			has_pk = TRUE;
			break;
		}
	}

	if (retval && has_pk) {
		if (!constraint_name_n || ! strcmp (g_value_get_string (constraint_name_n), "primary_key")) {
			GValue *v1, *v2;
			g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), "primary_key");
			g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), "PRIMARY KEY");
			
			if (! append_a_row (mod_model, error, 10, 
					    FALSE, catalog_value, /* constraint_catalog */
					    FALSE, p_table_schema, /* constraint_schema */
					    TRUE, v1, /* constraint_name */
					    FALSE, catalog_value, /* table_catalog */
					    FALSE, p_table_schema, /* table_schema */
					    FALSE, p_table_name, /* table_name */
					    TRUE, v2, /* constraint_type */
					    FALSE, NULL, /* check_clause */
					    FALSE, NULL, /* is_deferrable */
					    FALSE, NULL /* initially_deferred */))
				retval = FALSE;
		}
	}

	g_object_unref (tmpmodel);
	if (!retval)
		return FALSE;

	/* 
	 * UNIQUE 
	 */
	GType unique_col_types[] = {G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT, G_TYPE_NONE};
	
	stmt = get_statement (I_PRAGMA_INDEX_LIST, schema_name, g_value_get_string (p_table_name), error);
	tmpmodel = gda_connection_statement_execute_select_full (cnc, stmt, pragma_set, 
								 GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
								 unique_col_types, error);
	g_object_unref (stmt);
	if (!tmpmodel)
		return FALSE;
		
	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; i < nrows; i++) {
		const GValue *cvalue;
		GValue *v2;

		cvalue = gda_data_model_get_value_at (tmpmodel, 2, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		if (!g_value_get_int (cvalue))
			continue;
		
		cvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		if (constraint_name_n && strcmp (g_value_get_string (constraint_name_n), g_value_get_string (cvalue)))
			continue;

		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), "UNIQUE");
		
		if (! append_a_row (mod_model, error, 10, 
				    FALSE, catalog_value, /* constraint_catalog */
				    FALSE, p_table_schema, /* constraint_schema */
				    FALSE, cvalue, /* constraint_name */
				    FALSE, catalog_value, /* table_catalog */
				    FALSE, p_table_schema, /* table_schema */
				    FALSE, p_table_name, /* table_name */
				    TRUE, v2, /* constraint_type */
				    FALSE, NULL, /* check_clause */
				    FALSE, NULL, /* is_deferrable */
				    FALSE, NULL /* initially_deferred */))
			retval = FALSE;
	}
	g_object_unref (tmpmodel);
	if (!retval)
		return FALSE;

	/*
	 * FOREIGN KEYS
	 */
	GType fk_col_types[] = {G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_NONE};
	gint fkid = -1;
	stmt = get_statement (I_PRAGMA_FK_LIST, schema_name, g_value_get_string (p_table_name), error);
	tmpmodel = gda_connection_statement_execute_select_full (cnc, stmt, pragma_set, 
								 GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
								 fk_col_types, error);
	g_object_unref (stmt);
	if (!tmpmodel)
		return FALSE;
		
	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; i < nrows; i++) {
		const GValue *cvalue;

		cvalue = gda_data_model_get_value_at (tmpmodel, 0, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		if ((fkid == -1) || (fkid != g_value_get_int (cvalue))) {
			gchar *constname;
			GValue *v1, *v2;

			fkid = g_value_get_int (cvalue);

			cvalue = gda_data_model_get_value_at (tmpmodel, 2, i, error);
			if (!cvalue) {
				retval = FALSE;
				break;
			}
			constname = g_strdup_printf ("fk%d_%s", fkid, g_value_get_string (cvalue));
			if (constraint_name_n && strcmp (g_value_get_string (constraint_name_n), constname)) {
				g_free (constname);
				continue;
			}
		
			g_value_take_string ((v1 = gda_value_new (G_TYPE_STRING)), constname);
			g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), "FOREIGN KEY");
		
			if (! append_a_row (mod_model, error, 10, 
					    FALSE, catalog_value, /* constraint_catalog */
					    FALSE, p_table_schema, /* constraint_schema */
					    TRUE, v1, /* constraint_name */
					    FALSE, catalog_value, /* table_catalog */
					    FALSE, p_table_schema, /* table_schema */
					    FALSE, p_table_name, /* table_name */
					    TRUE, v2, /* constraint_type */
					    FALSE, NULL, /* check_clause */
					    FALSE, NULL, /* is_deferrable */
					    FALSE, NULL /* initially_deferred */))
				retval = FALSE;
		}
	}
	g_object_unref (tmpmodel);

	/*
	 * CHECK constraint
	 * FIXME: how to get that information?
	 */

	return retval;
}

gboolean
_gda_sqlite_meta__constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
				   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *mod_model, *tmpmodel;
	gboolean retval = TRUE;
	gint i, nrows;
	SqliteConnectionData *cdata;
	
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	tmpmodel = (GdaDataModel *) gda_connection_statement_execute (cnc, internal_stmt[I_PRAGMA_DATABASE_LIST],
								      NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, error);
	if (!tmpmodel)
		return FALSE;

	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; i < nrows; i++) {
		/* iterate through the schemas */
		GdaDataModel *tables_model;
		const gchar *schema_name;
		const GValue *cvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		schema_name = g_value_get_string (cvalue); 
		if (!strcmp (schema_name, TMP_DATABASE_NAME))
			 continue; /* nothing to do */
		
		gchar *str;
		GType col_types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_NONE};
		GdaStatement *stmt;
		
		str = g_strdup_printf (SELECT_TABLES_VIEWS, schema_name);
		stmt = gda_sql_parser_parse_string (internal_parser, str, NULL, NULL);
		g_free (str);
		g_assert (stmt);
		tables_model = gda_connection_statement_execute_select_full (cnc, stmt, NULL,
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									     col_types, error);
		g_object_unref (stmt);
		if (!tables_model) {
			retval = FALSE;
			break;
		}

		gint tnrows, ti;
		tnrows = gda_data_model_get_n_rows (tables_model);
		for (ti = 0; ti < tnrows; ti++) {
			/* iterate through the tables */
			const GValue *cv = gda_data_model_get_value_at (tables_model, 0, ti, error);
			if (!cv) {
				retval = FALSE;
				break;
			}
			if (!fill_constraints_tab_model (cnc, cdata, mod_model, cvalue, cv, NULL, error)) {
				retval = FALSE;
				break;
			}
		}
		g_object_unref (tables_model);
		if (!retval) 
			break;
	}
	g_object_unref (tmpmodel);

	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
	g_object_unref (mod_model);

	return retval;
}

gboolean 
_gda_sqlite_meta_constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
				  GdaMetaStore *store, GdaMetaContext *context, GError **error,
				  const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
				  const GValue *constraint_name_n)
{
	gboolean retval = TRUE;
	GdaDataModel *mod_model = NULL;
	SqliteConnectionData *cdata;
	
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	retval = fill_constraints_tab_model (cnc, cdata, mod_model, table_schema, table_name, constraint_name_n, error);	
	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
	g_object_unref (mod_model);

	return retval;
}

static gboolean 
fill_constraints_ref_model (GdaConnection *cnc, SqliteConnectionData *cdata, GdaDataModel *mod_model, 
			    const GValue *p_table_schema, const GValue *p_table_name, 
			    const GValue *constraint_name_n, GError **error)
{
	GdaDataModel *tmpmodel;
	gboolean retval = TRUE;
	gint nrows;
	const gchar *schema_name;
	gint i;
	GdaStatement *stmt;

	/*
	 * Setup stmt to execute
	 */
	schema_name = g_value_get_string (p_table_schema);

	GType fk_col_types[] = {G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_NONE};
	gint fkid = -1;
	stmt = get_statement (I_PRAGMA_FK_LIST, schema_name, g_value_get_string (p_table_name), error);
	tmpmodel = gda_connection_statement_execute_select_full (cnc, stmt, pragma_set, 
								 GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
								 fk_col_types, error);
	g_object_unref (stmt);
	if (!tmpmodel)
		return FALSE;
		
	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; i < nrows; i++) {
		const GValue *cvalue;

		cvalue = gda_data_model_get_value_at (tmpmodel, 0, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		if ((fkid  == -1) || (fkid != g_value_get_int (cvalue))) {
			gchar *constname;
			GValue *v2, *v3, *v4, *v5;

			fkid = g_value_get_int (cvalue);

			cvalue = gda_data_model_get_value_at (tmpmodel, 2, i, error);
			if (!cvalue) {
				retval = FALSE;
				break;
			}

			constname = g_strdup_printf ("fk%d_%s", fkid, g_value_get_string (cvalue));
			if (constraint_name_n && strcmp (g_value_get_string (constraint_name_n), constname)) {
				g_free (constname);
				continue;
			}
		
			g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), "FOREIGN KEY");
			g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), g_value_get_string (cvalue));
			g_value_set_string ((v4 = gda_value_new (G_TYPE_STRING)), "primary_key");
			if (!constraint_name_n)
				g_value_take_string ((v5 = gda_value_new (G_TYPE_STRING)), constname);

			if (! append_a_row (mod_model, error, 11, 
					    FALSE, catalog_value, /* table_catalog */
					    FALSE, p_table_schema, /* table_schema */
					    FALSE, p_table_name, /* table_name */
					    constraint_name_n ? FALSE: TRUE, 
					    constraint_name_n ? constraint_name_n : v5, /* constraint_name */
					    FALSE, catalog_value, /* ref_table_catalog */
					    FALSE, p_table_schema, /* ref_table_schema */
					    TRUE, v3, /* ref_table_name */
					    TRUE, v4, /* ref_constraint_name */
					    FALSE, NULL, /* match_option */
					    FALSE, rule_value, /* update_rule */
					    FALSE, rule_value /* delete_rule */))
				retval = FALSE;
			if (constraint_name_n)
				g_free (constname);
		}
	}
	g_object_unref (tmpmodel);

	return retval;
}

gboolean
_gda_sqlite_meta__constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
				   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *mod_model, *tmpmodel;
	gboolean retval = TRUE;
	gint i, nrows;
	SqliteConnectionData *cdata;
	
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	tmpmodel = (GdaDataModel *) gda_connection_statement_execute (cnc, internal_stmt[I_PRAGMA_DATABASE_LIST],
								      NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, error);
	if (!tmpmodel)
		return FALSE;

	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; i < nrows; i++) {
		/* iterate through the schemas */
		GdaDataModel *tables_model;
		const gchar *schema_name;
		const GValue *cvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		schema_name = g_value_get_string (cvalue); 
		if (!strcmp (schema_name, TMP_DATABASE_NAME))
			 continue; /* nothing to do */
		
		gchar *str;
		GType col_types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_NONE};
		GdaStatement *stmt;
		
		str = g_strdup_printf (SELECT_TABLES_VIEWS, schema_name);
		stmt = gda_sql_parser_parse_string (internal_parser, str, NULL, NULL);
		g_free (str);
		g_assert (stmt);
		tables_model = gda_connection_statement_execute_select_full (cnc, stmt, NULL,
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									     col_types, error);
		g_object_unref (stmt);
		if (!tables_model) {
			retval = FALSE;
			break;
		}

		gint tnrows, ti;
		tnrows = gda_data_model_get_n_rows (tables_model);
		for (ti = 0; ti < tnrows; ti++) {
			/* iterate through the tables */
			const GValue *cv = gda_data_model_get_value_at (tables_model, 0, ti, error);
			if (!cv) {
				retval = FALSE;
				break;
			}
			if (!fill_constraints_ref_model (cnc, cdata, mod_model, cvalue, cv, NULL, error)) {
				retval = FALSE;
				break;
			}
		}
		g_object_unref (tables_model);
		if (!retval) 
			break;
	}
	g_object_unref (tmpmodel);

	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
	g_object_unref (mod_model);

	return retval;
}

gboolean
_gda_sqlite_meta_constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
				  GdaMetaStore *store, GdaMetaContext *context, GError **error,
				  const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				  const GValue *constraint_name)
{
	gboolean retval = TRUE;
	GdaDataModel *mod_model = NULL;
	SqliteConnectionData *cdata;
	
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	retval = fill_constraints_ref_model (cnc, cdata, mod_model, table_schema, table_name, constraint_name, error);
	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
	g_object_unref (mod_model);

	return retval;
}

static gboolean 
fill_key_columns_model (GdaConnection *cnc, SqliteConnectionData *cdata, GdaDataModel *mod_model, 
			const GValue *p_table_schema, const GValue *p_table_name, const GValue *constraint_name,
			GError **error)
{
	GdaDataModel *tmpmodel;
	gboolean retval = TRUE;
	gint nrows;
	const gchar *schema_name, *const_name;
	gint i;
	GdaStatement *stmt;

	schema_name = g_value_get_string (p_table_schema);
	const_name = g_value_get_string (constraint_name);
	if (!strcmp (const_name, "primary_key")) {
		/* 
		 * PRIMARY KEY columns
		 */
		GType pk_col_types[] = {G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, 
					G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_NONE};
		gint ord_pos = 1;
		stmt = get_statement (I_PRAGMA_TABLE_INFO, schema_name, g_value_get_string (p_table_name), error);
		tmpmodel = gda_connection_statement_execute_select_full (cnc, stmt, pragma_set, 
									 GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
									 pk_col_types, error);
		g_object_unref (stmt);
		if (!tmpmodel)
			return FALSE;
		
		nrows = gda_data_model_get_n_rows (tmpmodel);
		for (i = 0; i < nrows; i++) {
			char const *pzDataType; /* Declared data type */
			char const *pzCollSeq; /* Collation sequence name */
			int pNotNull; /* True if NOT NULL constraint exists */
			int pPrimaryKey; /* True if column part of PK */
			int pAutoinc; /* True if column is auto-increment */
			const gchar *this_table_name;
			const gchar *this_col_name;
			const GValue *this_col_pname, *cvalue;
			GValue *v1;
			
			this_col_pname = gda_data_model_get_value_at (tmpmodel, 1, i, error);
			if (!this_col_pname) {
				retval = FALSE;
				break;
			}
			this_table_name = g_value_get_string (p_table_name);
			g_assert (this_table_name);
			if (!strcmp (this_table_name, "sqlite_sequence"))
				continue; /* ignore that table */
			
			this_col_name = g_value_get_string (this_col_pname);
			if (sqlite3_table_column_metadata (cdata->connection, g_value_get_string (p_table_schema), 
							   this_table_name, this_col_name,
							   &pzDataType, &pzCollSeq, &pNotNull, &pPrimaryKey, &pAutoinc)
			    != SQLITE_OK) {
				/* may fail because we have a view and not a table => use @tmpmodel to fetch info. */
				cvalue = gda_data_model_get_value_at (tmpmodel, 5, i, error);
				if (!cvalue) {
					retval = FALSE;
					break;
				}
				pPrimaryKey = g_value_get_boolean (cvalue);
			}
			if (pPrimaryKey) {
				cvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);
				if (!cvalue) {
					retval = FALSE;
					break;
				}
				g_value_set_int ((v1 = gda_value_new (G_TYPE_INT)), ord_pos++);
				if (! append_a_row (mod_model, error, 6, 
						    FALSE, catalog_value, /* table_catalog */
						    FALSE, p_table_schema, /* table_schema */
						    FALSE, p_table_name, /* table_name */
						    FALSE, constraint_name, /* constraint_name */
						    FALSE, cvalue, /* column_name */
						    TRUE, v1 /* ordinal_position */)) {
					retval = FALSE;
					break;
				}
			}
		}
		g_object_unref (tmpmodel);
	}
	else if ((*const_name == 'f')  && (const_name[1] == 'k')) {
		/*
		 * FOREIGN key columns
		 */
		GType fk_col_types[] = {G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_NONE};
		gint fkid = -1;
		gint ord_pos;
		stmt = get_statement (I_PRAGMA_FK_LIST, schema_name, g_value_get_string (p_table_name), error);
		tmpmodel = gda_connection_statement_execute_select_full (cnc, stmt, pragma_set, 
									 GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
									 fk_col_types, error);
		g_object_unref (stmt);
		if (!tmpmodel)
			return FALSE;
		
		nrows = gda_data_model_get_n_rows (tmpmodel);
		for (i = 0; i < nrows; i++) {
			const GValue *cvalue;
			GValue *v1;
			
			cvalue = gda_data_model_get_value_at (tmpmodel, 0, i, error);
			if (!cvalue) {
				retval = FALSE;
				break;
			}
			if ((fkid == -1) || (fkid != g_value_get_int (cvalue))) {
				gchar *constname;
				
				fkid = g_value_get_int (cvalue);

				cvalue = gda_data_model_get_value_at (tmpmodel, 2, i, error);
				if (!cvalue) {
					retval = FALSE;
					break;
				}
				constname = g_strdup_printf ("fk%d_%s", fkid, g_value_get_string (cvalue));
				if (strcmp (const_name, constname)) {
					fkid = -1;
					g_free (constname);
					continue;
				}
				ord_pos = 1;
			}
			
			g_value_set_int ((v1 = gda_value_new (G_TYPE_INT)), ord_pos++);
			cvalue = gda_data_model_get_value_at (tmpmodel, 3, i, error);
			if (!cvalue) {
				retval = FALSE;
				break;
			}
			if (! append_a_row (mod_model, error, 6, 
					    FALSE, catalog_value, /* table_catalog */
					    FALSE, p_table_schema, /* table_schema */
					    FALSE, p_table_name, /* table_name */
					    FALSE, constraint_name, /* constraint_name */
					    FALSE, cvalue, /* column_name */
					    TRUE, v1 /* ordinal_position */))
				retval = FALSE;
		}
		g_object_unref (tmpmodel);
	}
	else {
		/*
		 * UNIQUE columns
		 */
		GType unique_col_types[] = {G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_NONE};
		
		stmt = get_statement (I_PRAGMA_INDEX_INFO, schema_name, g_value_get_string (constraint_name),
				      error);
		tmpmodel = gda_connection_statement_execute_select_full (cnc, stmt, pragma_set, 
									 GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
									 unique_col_types, error);
		g_object_unref (stmt);
		if (!tmpmodel)
			return FALSE;
		
		nrows = gda_data_model_get_n_rows (tmpmodel);
		for (i = 0; i < nrows; i++) {
			GValue *v1;
			const GValue *cvalue;
			cvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);
			if (!cvalue) {
				retval = FALSE;
				break;
			}
			g_value_set_int ((v1 = gda_value_new (G_TYPE_INT)), g_value_get_int (cvalue) + 1);
			cvalue = gda_data_model_get_value_at (tmpmodel, 2, i, error);
			if (!cvalue) {
				retval = FALSE;
				break;
			}
			if (! append_a_row (mod_model, error, 6, 
					    FALSE, catalog_value, /* table_catalog */
					    FALSE, p_table_schema, /* table_schema */
					    FALSE, p_table_name, /* table_name */
					    FALSE, constraint_name, /* constraint_name */
					    FALSE, cvalue, /* column_name */
					    TRUE, v1 /* ordinal_position */))
				retval = FALSE;
		}
		g_object_unref (tmpmodel);
	}

	return retval;
}

gboolean
_gda_sqlite_meta__key_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* iterate through the tables, and each time call fill_constraints_tab_model() 
	 * to get the list of table constraints. 
	 * Then iterate through that data model and call fill_key_columns_model()
	 * for each row */

	GdaDataModel *const_model, *tmpmodel;
	gboolean retval = TRUE;
	gint i, nrows;
	SqliteConnectionData *cdata;
	
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	tmpmodel = (GdaDataModel *) gda_connection_statement_execute (cnc, internal_stmt[I_PRAGMA_DATABASE_LIST],
								      NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, error);
	if (!tmpmodel)
		return FALSE;

	const_model = gda_meta_store_create_modify_data_model (store, "_table_constraints");
	g_assert (const_model);

	nrows = gda_data_model_get_n_rows (tmpmodel);
	for (i = 0; i < nrows; i++) {
		/* iterate through the schemas */
		GdaDataModel *tables_model;
		const gchar *schema_name;
		const GValue *cvalue = gda_data_model_get_value_at (tmpmodel, 1, i, error);

		if (!cvalue) {
			retval = FALSE;
			break;
		}
		schema_name = g_value_get_string (cvalue); 
		if (!strcmp (schema_name, TMP_DATABASE_NAME))
			 continue; /* nothing to do */
		
		gchar *str;
		GType col_types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_NONE};
		GdaStatement *stmt;
		
		str = g_strdup_printf (SELECT_TABLES_VIEWS, schema_name);
		stmt = gda_sql_parser_parse_string (internal_parser, str, NULL, NULL);
		g_free (str);
		g_assert (stmt);
		tables_model = gda_connection_statement_execute_select_full (cnc, stmt, NULL,
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									     col_types, error);
		g_object_unref (stmt);
		if (!tables_model) {
			retval = FALSE;
			break;
		}

		gint tnrows, ti;
		tnrows = gda_data_model_get_n_rows (tables_model);
		for (ti = 0; ti < tnrows; ti++) {
			/* iterate through the tables */
			const GValue *cv = gda_data_model_get_value_at (tables_model, 0, ti, error);
			if (!cv) {
				retval = FALSE;
				break;
			}
			if (!fill_constraints_tab_model (cnc, cdata, const_model, cvalue, cv, NULL, error)) {
				retval = FALSE;
				break;
			}
		}
		g_object_unref (tables_model);
		if (!retval) 
			break;
	}
	g_object_unref (tmpmodel);
	if (!retval) {
		g_object_unref (const_model);
		return FALSE;
	}

	GdaDataModel *mod_model;
	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	nrows = gda_data_model_get_n_rows (const_model);
	for (i = 0; i < nrows; i++) {
		const GValue *v2, *v4, *v5;
		v2 = gda_data_model_get_value_at (const_model, 2, i, error);
		if (!v2) {
			retval = FALSE;
			break;
		}
		v4 = gda_data_model_get_value_at (const_model, 4, i, error);
		if (!v4) {
			retval = FALSE;
			break;
		}
		v5 = gda_data_model_get_value_at (const_model, 5, i, error);
		if (!v5) {
			retval = FALSE;
			break;
		}
		if (!fill_key_columns_model (cnc, cdata, mod_model, v4, v5, v2, error)) {
			retval = FALSE;
			break;
		}
	}
	g_object_unref (const_model);

	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
	g_object_unref (mod_model);

	return retval;
}

gboolean 
_gda_sqlite_meta_key_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
			      const GValue *constraint_name)
{
	gboolean retval = TRUE;
	GdaDataModel *mod_model = NULL;
	SqliteConnectionData *cdata;
	
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;

	mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (mod_model);

	retval = fill_key_columns_model (cnc, cdata, mod_model, table_schema, table_name, constraint_name, error);
	if (retval) 
		retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
	g_object_unref (mod_model);

	return retval;
}

gboolean
_gda_sqlite_meta__check_columns (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* FIXME: How to get this ? */
	return TRUE;
}

gboolean
_gda_sqlite_meta_check_columns (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *table_catalog, const GValue *table_schema, 
				const GValue *table_name, const GValue *constraint_name)
{
	/* FIXME: How to get this ? */
	return TRUE;
}

gboolean
_gda_sqlite_meta__triggers (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* FIXME: How to get this ? */
	return TRUE;
}

gboolean
_gda_sqlite_meta_triggers (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *table_catalog, const GValue *table_schema, 
			   const GValue *table_name)
{
	/* FIXME: How to get this ? */
	return TRUE;
}


gboolean
_gda_sqlite_meta__routines (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	return _gda_sqlite_meta_routines (prov, cnc, store, context, error, NULL, NULL, NULL);
}

static gboolean
fill_routines (GdaDataModel *mod_model, 
	       const GValue *rname, const GValue *is_agg, const GValue *rnargs, const GValue *sname, GError **error)
{
	GValue *v0;
	gboolean retval = TRUE;
	
	if (g_value_get_int (is_agg) == 0)
		g_value_set_string ((v0 = gda_value_new (G_TYPE_STRING)), "FUNCTION");
	else
		g_value_set_string ((v0 = gda_value_new (G_TYPE_STRING)), "AGGREGATE");
	if (!append_a_row (mod_model, error, 22,
			   FALSE, catalog_value, /* specific_catalog */
			   FALSE, catalog_value, /* specific_schema */
			   FALSE, sname, /* specific_name */
			   FALSE, NULL, /* routine_catalog */
			   FALSE, NULL, /* routine_schema */
			   FALSE, rname, /* routine_name */
			   TRUE, v0, /* routine_type */
			   FALSE, NULL, /* return_type */
			   FALSE, false_value, /* returns_set */
			   FALSE, rnargs, /* nb_args */
			   FALSE, NULL, /* routine_body */
			   FALSE, NULL, /* routine_definition */
			   FALSE, NULL, /* external_name */
			   FALSE, NULL, /* external_language */
			   FALSE, NULL, /* parameter_style */
			   FALSE, NULL, /* is_deterministic */
			   FALSE, NULL, /* sql_data_access */
			   FALSE, NULL, /* is_null_call */
			   FALSE, NULL, /* routine_comments */
			   FALSE, rname, /* routine_short_name */
			   FALSE, rname, /* routine_full_name */
			   FALSE, NULL /* routine_owner */)) {
		retval = FALSE;
	}

	return retval;
}

gboolean
_gda_sqlite_meta_routines (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *routine_catalog, const GValue *routine_schema, 
			   const GValue *routine_name_n)
{
	SqliteConnectionData *cdata;
	gboolean retval = TRUE;

	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	
	if (cdata->functions_model) {
		/* use cdata->functions_model and cdata->aggregates_model */
		GdaDataModel *mod_model;
		mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
		g_assert (mod_model);
		
		if (cdata->functions_model) {
			gint i, nrows;
			for (i = 0; i < nrows; i++) {
				const GValue *cv0, *cv2, *cv3;
				cv0 = gda_data_model_get_value_at (cdata->functions_model, 0, i, error);
				if (!cv0) {
					retval = FALSE;
					break;
				}
				cv2 = gda_data_model_get_value_at (cdata->functions_model, 1, i, error);
				if (!cv2) {
					retval = FALSE;
					break;
				}
				cv3 = gda_data_model_get_value_at (cdata->functions_model, 2, i, error);
				if (!cv3) {
					retval = FALSE;
					break;
				}
				if ((!routine_name_n || (routine_name_n && !gda_value_compare (routine_name_n, cv0)))
				    && ! fill_routines (mod_model, cv0, false_value, cv2, cv3, error)) {
					retval = FALSE;
					break;
				}
			}
		}
		if (cdata->aggregates_model) {
			gint i, nrows;
			for (i = 0; i < nrows; i++) {
				const GValue *cv0, *cv2, *cv3;
				cv0 = gda_data_model_get_value_at (cdata->aggregates_model, 0, i, error);
				if (!cv0) {
					retval = FALSE;
					break;
				}
				cv2 = gda_data_model_get_value_at (cdata->aggregates_model, 1, i, error);
				if (!cv2) {
					retval = FALSE;
					break;
				}
				cv3 = gda_data_model_get_value_at (cdata->aggregates_model, 2, i, error);
				if (!cv3) {
					retval = FALSE;
					break;
				}
				if ((!routine_name_n || (routine_name_n && !gda_value_compare (routine_name_n, cv0)))
				    && ! fill_routines (mod_model, cv0, true_value, cv2, cv3, error)) {
					retval = FALSE;
					break;
				}
			}
		}

		if (retval)
			retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
		g_object_unref (mod_model);
	}
	else {
		/* get list of procedures */
		GdaDataModel *tmpmodel, *mod_model;
		gint i, nrows;
		tmpmodel = (GdaDataModel *) gda_connection_statement_execute (cnc, internal_stmt[I_PRAGMA_PROCLIST],
									      NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, 
									      NULL, error);
		if (!tmpmodel)
			return FALSE;

		mod_model = gda_meta_store_create_modify_data_model (store, context->table_name);
		g_assert (mod_model);

		nrows = gda_data_model_get_n_rows (tmpmodel);
		for (i = 0; i < nrows; i++) {
			const GValue *cv0, *cv1, *cv2, *cv3;
			cv0 = gda_data_model_get_value_at (tmpmodel, 0, i, error);
			if (!cv0) {
				retval = FALSE;
				break;
			}
			cv1 = gda_data_model_get_value_at (tmpmodel, 1, i, error);
			if (!cv1) {
				retval = FALSE;
				break;
			}
			cv2 = gda_data_model_get_value_at (tmpmodel, 2, i, error);
			if (!cv2) {
				retval = FALSE;
				break;
			}
			cv3 = gda_data_model_get_value_at (tmpmodel, 3, i, error);
			if (!cv3) {
				retval = FALSE;
				break;
			}
			if ((!routine_name_n || (routine_name_n && !gda_value_compare (routine_name_n, cv0)))
			    && ! fill_routines (mod_model, cv0, cv1, cv2, cv3, error)) {
				retval = FALSE;
				break;
			}
		}

		if (retval)
			retval = gda_meta_store_modify_with_context (store, context, mod_model, error);
		g_object_unref (mod_model);
		g_object_unref (tmpmodel);
	}

	return retval;
}

gboolean
_gda_sqlite_meta__routine_col (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* feature not available in SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta_routine_col (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *rout_catalog, const GValue *rout_schema, 
			      const GValue *rout_name)
{
	/* feature not available in SQLite */
	return TRUE;
}

gboolean
_gda_sqlite_meta__routine_par (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* Routines in SQLite accept any type of value => nothing to do */
	return TRUE;
}

gboolean
_gda_sqlite_meta_routine_par (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *rout_catalog, const GValue *rout_schema, 
			      const GValue *rout_name)
{
	/* Routines in SQLite accept any type of value => nothing to do */
	return TRUE;
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
