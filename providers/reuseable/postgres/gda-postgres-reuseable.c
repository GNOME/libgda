/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Murray Cumming <murrayc@murrayc.com>
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
#include <libgda/gda-sql-builder.h>
#include "gda-postgres-reuseable.h"
#include "gda-postgres-parser.h"

#include <libgda/sqlite/keywords_hash.h>
#include "keywords_hash-82.code" /* this one is dynamically generated */
#include "keywords_hash-83.code" /* this one is dynamically generated */
#include "keywords_hash-84.code" /* this one is dynamically generated */

/*
 * Reuseable interface entry point
 */
static GdaProviderReuseableOperations
_gda_postgres_reuseable = {
	_gda_postgres_reuseable_new_data,
	_gda_postgres_reuseable_reset_data,
	_gda_postgres_reuseable_get_g_type,
	_gda_postgres_reuseable_get_reserved_keywords_func,
	_gda_postgres_reuseable_create_parser,
	{
		._info = _gda_postgres_meta__info,
		._btypes = _gda_postgres_meta__btypes,
		._udt = _gda_postgres_meta__udt,
		.udt = _gda_postgres_meta_udt,
		._udt_cols = _gda_postgres_meta__udt_cols,
		.udt_cols = _gda_postgres_meta_udt_cols,
		._enums = _gda_postgres_meta__enums,
		.enums = _gda_postgres_meta_enums,
		._domains = _gda_postgres_meta__domains,
		.domains = _gda_postgres_meta_domains,
		._constraints_dom = _gda_postgres_meta__constraints_dom,
		.constraints_dom = _gda_postgres_meta_constraints_dom,
		._el_types = _gda_postgres_meta__el_types,
		.el_types = _gda_postgres_meta_el_types,
		._collations = _gda_postgres_meta__collations,
		.collations = _gda_postgres_meta_collations,
		._character_sets = _gda_postgres_meta__character_sets,
		.character_sets = _gda_postgres_meta_character_sets,
		._schemata = _gda_postgres_meta__schemata,
		.schemata = _gda_postgres_meta_schemata,
		._tables_views = _gda_postgres_meta__tables_views,
		.tables_views = _gda_postgres_meta_tables_views,
		._columns = _gda_postgres_meta__columns,
		.columns = _gda_postgres_meta_columns,
		._view_cols = _gda_postgres_meta__view_cols,
		.view_cols = _gda_postgres_meta_view_cols,
		._constraints_tab = _gda_postgres_meta__constraints_tab,
		.constraints_tab = _gda_postgres_meta_constraints_tab,
		._constraints_ref = _gda_postgres_meta__constraints_ref,
		.constraints_ref = _gda_postgres_meta_constraints_ref,
		._key_columns = _gda_postgres_meta__key_columns,
		.key_columns = _gda_postgres_meta_key_columns,
		._check_columns = _gda_postgres_meta__check_columns,
		.check_columns = _gda_postgres_meta_check_columns,
		._triggers = _gda_postgres_meta__triggers,
		.triggers = _gda_postgres_meta_triggers,
		._routines = _gda_postgres_meta__routines,
		.routines = _gda_postgres_meta_routines,
		._routine_col = _gda_postgres_meta__routine_col,
		.routine_col = _gda_postgres_meta_routine_col,
		._routine_par = _gda_postgres_meta__routine_par,
		.routine_par = _gda_postgres_meta_routine_par,
		._indexes_tab = _gda_postgres_meta__indexes_tab,
		.indexes_tab = _gda_postgres_meta_indexes_tab,
		._index_cols = _gda_postgres_meta__index_cols,
		.index_cols = _gda_postgres_meta_index_cols
	}
};

GdaProviderReuseableOperations *
_gda_postgres_reuseable_get_ops (void)
{
	return &_gda_postgres_reuseable;
}

/*
 * Postgres type identification
 */
typedef struct {
        gchar              *name;
        unsigned int        oid; /* <=> to Postgres's Oid type */
        GType               type;
        gchar              *comments;
        gchar              *owner;
} GdaPostgresTypeOid;

static void
gda_postgres_type_oid_free (GdaPostgresTypeOid *typedata)
{
	g_free (typedata->name);
	g_free (typedata->comments);
	g_free (typedata->owner);
	g_free (typedata);
}

GdaProviderReuseable *
_gda_postgres_reuseable_new_data (void)
{
	GdaPostgresReuseable *reuseable;
	reuseable = g_new0 (GdaPostgresReuseable, 1);
	reuseable->types_oid_hash = NULL;
	reuseable->types_dbtype_hash = NULL;
	_gda_postgres_provider_meta_init (NULL);

	((GdaProviderReuseable*)reuseable)->operations = &_gda_postgres_reuseable;

	return (GdaProviderReuseable*) reuseable;
}

void
_gda_postgres_reuseable_reset_data (GdaProviderReuseable *rdata)
{
	GdaPostgresReuseable *reuseable;
	reuseable = (GdaPostgresReuseable*) rdata;

	g_free (rdata->server_version);
	if (reuseable->types_dbtype_hash)
		g_hash_table_destroy (reuseable->types_dbtype_hash);
	if (reuseable->types_oid_hash)
		g_hash_table_destroy (reuseable->types_oid_hash);

	/* don't free reuseable->avoid_types */
        g_free (reuseable->avoid_types_oids);
        g_free (reuseable->any_type_oid);
	memset (reuseable, 0, sizeof (GdaPostgresReuseable));
}

static GdaDataModel *
execute_select (GdaConnection *cnc, GdaPostgresReuseable *rdata, const gchar *sql)
{
	GdaSqlParser *parser;
	GdaStatement *stmt;
	GdaDataModel *model;
	parser = _gda_postgres_reuseable_create_parser ((GdaProviderReuseable*) rdata);
	
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	g_object_unref (parser);
	g_assert (stmt);

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, NULL);
	g_object_unref (stmt);

	return model;
}

gboolean
_gda_postgres_compute_version (GdaConnection *cnc, GdaPostgresReuseable *rdata, GError **error)
{
	GdaSqlBuilder *b;
	GdaStatement *stmt;
	GdaDataModel *model;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
        const guint id_func = gda_sql_builder_add_function (b, "version", 0);
        gda_sql_builder_add_field_value_id (b, id_func, 0);
	stmt = gda_sql_builder_get_statement (b, NULL);
	g_object_unref (b);
	g_assert (stmt);

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, error);
	g_object_unref (stmt);
	if (!model)
		return FALSE;
	
	const GValue *cvalue;
	GError *lerror = NULL;
	cvalue = gda_data_model_get_value_at (model, 0, 0, &lerror);
	if (!cvalue) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
		            GDA_SERVER_PROVIDER_INTERNAL_ERROR,
		            _("Can't get version data from server: %s"),
		            lerror && lerror->message ? lerror->message : _("No detail"));
		g_clear_error (&lerror);
		g_object_unref (model);
		return FALSE;
	}

	const gchar *str;
	str = g_value_get_string (cvalue);
	((GdaProviderReuseable*)rdata)->server_version = g_strdup (str);

	/* analyse string */
	const gchar *ptr;
	rdata->version_float = 0;

	/* go on  the first digit of version number */
        ptr = str;
        while (*ptr && *ptr != ' ')
                ptr++;
	if (*ptr) {
		ptr++;
		
		/* scan version parts */
		GdaProviderReuseable *prdata = (GdaProviderReuseable*) rdata;
		sscanf (ptr, "%d.%d.%d", &(prdata->major),  &(prdata->minor),  &(prdata->micro));

		/* elaborate the version number as a float */
		rdata->version_float = ((gdouble) ((GdaProviderReuseable*)rdata)->major) +
		((gdouble) ((GdaProviderReuseable*)rdata)->minor) / 10.0 +
		((gdouble) ((GdaProviderReuseable*)rdata)->micro) / 100.0;
	}

	g_object_unref (model);

/*
	g_print ("VERSIONS: [%f] [%d.%d.%d]\n", rdata->version_float,
		 ((GdaProviderReuseable*)rdata)->major,
		 ((GdaProviderReuseable*)rdata)->minor,
		 ((GdaProviderReuseable*)rdata)->micro);
*/
	return TRUE;
}

static GType
postgres_name_to_g_type (const gchar *name, const gchar *conv_func_name)
{
	/* default built in data types */
	if (!strcmp (name, "bool"))
		return G_TYPE_BOOLEAN;
	else if (!strcmp (name, "int8"))
		return G_TYPE_INT64;
	else if (!strcmp (name, "int4") || !strcmp (name, "abstime"))
		return G_TYPE_INT;
	else if (!strcmp (name, "int2"))
		return GDA_TYPE_SHORT;
	else if (!strcmp (name, "float4"))
		return G_TYPE_FLOAT;
	else if (!strcmp (name, "float8"))
		return G_TYPE_DOUBLE;
	else if (!strcmp (name, "numeric"))
		return GDA_TYPE_NUMERIC;
	else if (!strncmp (name, "timestamp", 9))
		return G_TYPE_DATE_TIME;
	else if (!strcmp (name, "date"))
		return G_TYPE_DATE;
	else if (!strncmp (name, "time", 4))
		return GDA_TYPE_TIME;
	else if (!strcmp (name, "point"))
		return GDA_TYPE_GEOMETRIC_POINT;
	else if (!strcmp (name, "oid"))
		return G_TYPE_UINT;
	else if (!strcmp (name, "bytea"))
		return GDA_TYPE_BINARY;
	else if (!strcmp (name, "text"))
    return GDA_TYPE_TEXT;

	/* other data types, using the conversion function name as a hint */
	if (!conv_func_name)
		return G_TYPE_STRING;

	if (!strncmp (conv_func_name, "int2", 4))
		return GDA_TYPE_SHORT;
	if (!strncmp (conv_func_name, "int4", 4))
		return G_TYPE_INT;
	if (!strncmp (conv_func_name, "int8", 4))
		return G_TYPE_INT64;
	if (!strncmp (conv_func_name, "float4", 6))
		return G_TYPE_FLOAT;
	if (!strncmp (conv_func_name, "float8", 6))
		return G_TYPE_DOUBLE;
	if (!strncmp (conv_func_name, "timestamp", 9))
		return G_TYPE_DATE_TIME;
	if (!strncmp (conv_func_name, "time", 4))
		return GDA_TYPE_TIME;
	if (!strncmp (conv_func_name, "date", 4))
		return G_TYPE_DATE;
	if (!strncmp (conv_func_name, "bool", 4))
		return G_TYPE_BOOLEAN;
	if (!strncmp (conv_func_name, "oid", 3))
		return G_TYPE_UINT;
	if (!strncmp (conv_func_name, "bytea", 5))
		return GDA_TYPE_BINARY;
	return G_TYPE_STRING;
}

void
_gda_postgres_compute_types (GdaConnection *cnc, GdaPostgresReuseable *rdata)
{
	if (rdata->types_oid_hash)
		return;

	rdata->types_oid_hash = g_hash_table_new_full (g_int_hash, g_int_equal,
						       NULL, (GDestroyNotify) gda_postgres_type_oid_free);
	rdata->types_dbtype_hash = g_hash_table_new (g_str_hash, g_str_equal);

	GdaDataModel *model, *model_avoid, *model_anyoid = NULL;
	gint ncols, nrows, i;
	gchar *avoid_types = NULL;
	GString *string;

	if (rdata->version_float == 0) {
		GError *lerror = NULL;
		_gda_postgres_compute_version (cnc, rdata, &lerror);
		if (lerror != NULL) {
			g_warning (_("Internal Error: Can't calculate PostgreSQL version: %s"),
			           lerror && lerror->message ? lerror->message : _("No detail"));
			return;
		}
	}
	if (rdata->version_float < 7.3) {
		gchar *query;
		avoid_types = "'SET', 'cid', 'oid', 'int2vector', 'oidvector', 'regproc', 'smgr', 'tid', 'unknown', 'xid'";
		/* main query to fetch infos about the data types */
		query = g_strdup_printf ("SELECT pg_type.oid, typname, usename, obj_description(pg_type.oid) "
					 "FROM pg_type, pg_user "
					 "WHERE typowner=usesysid AND typrelid = 0 AND typname !~ '^_' "
					 "AND  typname not in (%s) "
					 "ORDER BY typname", avoid_types);
		model = execute_select (cnc, rdata, query);
		g_free (query);

		/* query to fetch non returned data types */
		query = g_strdup_printf ("SELECT pg_type.oid FROM pg_type WHERE typname in (%s)", avoid_types);
		model_avoid = execute_select (cnc, rdata, query);
		g_free (query);
	}
	else {
		gchar *query;
		avoid_types = "'any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid'";

		/* main query to fetch infos about the data types */
		query = g_strdup_printf (
                          "SELECT t.oid, t.typname, u.usename, pg_catalog.obj_description(t.oid), t.typinput "
			  "FROM pg_catalog.pg_type t LEFT JOIN pg_catalog.pg_user u ON (t.typowner=u.usesysid), pg_catalog.pg_namespace n "
			  "WHERE n.oid = t.typnamespace "
			  "AND pg_catalog.pg_type_is_visible(t.oid) "
			  /*--AND (n.nspname = 'public' OR n.nspname = 'pg_catalog')*/
			  "AND typname !~ '^_' "
			  "AND (t.typrelid = 0 OR "
			  "(SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid)) "
			  "AND t.typname not in (%s) "
			  "ORDER BY typname", avoid_types);
		model = execute_select (cnc, rdata, query);
		g_free (query);

		/* query to fetch non returned data types */
		query = g_strdup_printf ("SELECT t.oid FROM pg_catalog.pg_type t WHERE t.typname in (%s)",
					 avoid_types);
		model_avoid = execute_select (cnc, rdata, query);
		g_free (query);

		/* query to fetch the oid of the 'any' data type */
		model_anyoid = execute_select (cnc, rdata,
					       "SELECT t.oid FROM pg_catalog.pg_type t WHERE t.typname = 'any'");
	}

	if (rdata->version_float == 0)
		_gda_postgres_compute_version (cnc, rdata, NULL);
	if (!model || !model_avoid ||
	    ((rdata->version_float >= 7.3) && !model_anyoid)) {
		if (model)
			g_object_unref (model);
		if (model_avoid)
			g_object_unref (model_avoid);
		if (model_anyoid)
			g_object_unref (model_anyoid);
		return;
	}

	/* Data types returned to the Gda client */
	nrows = gda_data_model_get_n_rows (model);
	ncols = gda_data_model_get_n_columns (model);
	if (nrows == 0)
		g_warning ("PostgreSQL provider did not find any data type (expect some mis-behaviours) please report the error to http://gitlab.gnome.org/GNOME/libgda/issues");
	for (i = 0; i < nrows; i++) {
		const GValue *conv_func_name = NULL;
		const GValue *values[4];
		gint j;
		gboolean allread = TRUE;
		if (ncols >= 5)
			conv_func_name = gda_data_model_get_value_at (model, 4, i, NULL);
		for (j = 0; j < 4; j++) {
			values[j] = gda_data_model_get_value_at (model, j, i, NULL);
			if (!values [j]) {
				allread = FALSE;
				break;
			}
		}
		if (allread && (G_VALUE_TYPE (values[1]) == G_TYPE_STRING)) {
			GdaPostgresTypeOid *td;
			td = g_new0 (GdaPostgresTypeOid, 1);
			td->name = g_value_dup_string (values [1]);
			td->oid = (guint) g_ascii_strtoull (g_value_get_string (values[0]), NULL, 10);
			td->type = postgres_name_to_g_type (td->name,
							   conv_func_name ? g_value_get_string (conv_func_name) : NULL);
			if (G_VALUE_TYPE (values[3]) == G_TYPE_STRING)
				td->comments = g_value_dup_string (values [3]);
			if (G_VALUE_TYPE (values[2]) == G_TYPE_STRING)
				td->owner = g_value_dup_string (values [2]);

			g_hash_table_insert (rdata->types_oid_hash, &(td->oid), td);
			g_hash_table_insert (rdata->types_dbtype_hash, &(td->name), td);
		}
	}

	/* Make a string of data types internal to postgres and not returned, for future queries */
        string = NULL;
        nrows = gda_data_model_get_n_rows (model_avoid);
        for (i = 0; i < nrows; i++) {
		const GValue *cvalue;

		cvalue = gda_data_model_get_value_at (model_avoid, 0, i, NULL);
		if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING)) {
			if (!string)
				string = g_string_new (g_value_get_string (cvalue));
			else {
				g_string_append (string, ", ");
				g_string_append (string, g_value_get_string (cvalue));
			}
		}
        }
        rdata->avoid_types = avoid_types;
	if (string)
		rdata->avoid_types_oids = g_string_free (string, FALSE);

	g_object_unref (model);
	g_object_unref (model_avoid);

        /* make a string of the oid of type 'any' */
        rdata->any_type_oid = NULL;
        if (model_anyoid) {
                if (gda_data_model_get_n_rows (model_anyoid) == 1) {
			const GValue *cvalue;
			cvalue = gda_data_model_get_value_at (model_anyoid, 0, 0, NULL);
			if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
				rdata->any_type_oid = g_value_dup_string (cvalue);
                }
		g_object_unref (model_anyoid);
        }
}

GType
_gda_postgres_type_oid_to_gda (GdaConnection *cnc, GdaPostgresReuseable *rdata, unsigned int postgres_oid)
{
	GdaPostgresTypeOid *type;
	guint id;
	id = postgres_oid;

	_gda_postgres_compute_types (cnc, rdata);
	type = g_hash_table_lookup (rdata->types_oid_hash, &id);
	if (type)
		return type->type;
	else
		return G_TYPE_STRING;
}

GType
_gda_postgres_reuseable_get_g_type (GdaConnection *cnc, GdaProviderReuseable *rdata, const gchar *db_type)
{
	GdaPostgresTypeOid *type;
	g_return_val_if_fail (db_type, GDA_TYPE_NULL);

	_gda_postgres_compute_types (cnc, (GdaPostgresReuseable*)rdata);
	type = g_hash_table_lookup (((GdaPostgresReuseable*)rdata)->types_dbtype_hash, db_type);
	if (type)
		return type->type;
	else
		return GDA_TYPE_NULL;
}


GdaSqlReservedKeywordsFunc
_gda_postgres_reuseable_get_reserved_keywords_func (GdaProviderReuseable *rdata)
{
	if (rdata) {
		switch (rdata->major) {
		case 8:
#ifdef GDA_DEBUG
				V82test_keywords ();
				V83test_keywords ();
				V84test_keywords ();
#endif
			if (rdata->minor == 2)
				return V82is_keyword;
			if (rdata->minor == 3)
				return V83is_keyword;
			if (rdata->minor == 4)
				return V84is_keyword;
			return V84is_keyword;
		default:
			return V84is_keyword;
			break;
		}
	}
        return V84is_keyword;
}

GdaSqlParser *
_gda_postgres_reuseable_create_parser (G_GNUC_UNUSED GdaProviderReuseable *rdata)
{
	return GDA_SQL_PARSER (g_object_new (GDA_TYPE_POSTGRES_PARSER, NULL));
}
