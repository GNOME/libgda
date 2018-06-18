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
#include "gda-mysql-reuseable.h"
#include "gda-mysql-parser.h"

#include <libgda/sqlite/keywords_hash.h>
#include "keywords_hash_V50.code" /* this one is dynamically generated */
#include "keywords_hash_V51.code" /* this one is dynamically generated */
#include "keywords_hash_V54.code" /* this one is dynamically generated */
#include "keywords_hash_V60.code" /* this one is dynamically generated */

/*
 * Reuseable interface entry point
 */
static GdaProviderReuseableOperations
_gda_mysql_reuseable = {
	_gda_mysql_reuseable_new_data,
	_gda_mysql_reuseable_reset_data,
	_gda_mysql_reuseable_get_g_type,
	_gda_mysql_reuseable_get_reserved_keywords_func,
	_gda_mysql_reuseable_create_parser,
	{
		._info = _gda_mysql_meta__info,
		._btypes = _gda_mysql_meta__btypes,
		._udt = _gda_mysql_meta__udt,
		.udt = _gda_mysql_meta_udt,
		._udt_cols = _gda_mysql_meta__udt_cols,
		.udt_cols = _gda_mysql_meta_udt_cols,
		._enums = _gda_mysql_meta__enums,
		.enums = _gda_mysql_meta_enums,
		._domains = _gda_mysql_meta__domains,
		.domains = _gda_mysql_meta_domains,
		._constraints_dom = _gda_mysql_meta__constraints_dom,
		.constraints_dom = _gda_mysql_meta_constraints_dom,
		._el_types = _gda_mysql_meta__el_types,
		.el_types = _gda_mysql_meta_el_types,
		._collations = _gda_mysql_meta__collations,
		.collations = _gda_mysql_meta_collations,
		._character_sets = _gda_mysql_meta__character_sets,
		.character_sets = _gda_mysql_meta_character_sets,
		._schemata = _gda_mysql_meta__schemata,
		.schemata = _gda_mysql_meta_schemata,
		._tables_views = _gda_mysql_meta__tables_views,
		.tables_views = _gda_mysql_meta_tables_views,
		._columns = _gda_mysql_meta__columns,
		.columns = _gda_mysql_meta_columns,
		._view_cols = _gda_mysql_meta__view_cols,
		.view_cols = _gda_mysql_meta_view_cols,
		._constraints_tab = _gda_mysql_meta__constraints_tab,
		.constraints_tab = _gda_mysql_meta_constraints_tab,
		._constraints_ref = _gda_mysql_meta__constraints_ref,
		.constraints_ref = _gda_mysql_meta_constraints_ref,
		._key_columns = _gda_mysql_meta__key_columns,
		.key_columns = _gda_mysql_meta_key_columns,
		._check_columns = _gda_mysql_meta__check_columns,
		.check_columns = _gda_mysql_meta_check_columns,
		._triggers = _gda_mysql_meta__triggers,
		.triggers = _gda_mysql_meta_triggers,
		._routines = _gda_mysql_meta__routines,
		.routines = _gda_mysql_meta_routines,
		._routine_col = _gda_mysql_meta__routine_col,
		.routine_col = _gda_mysql_meta_routine_col,
		._routine_par = _gda_mysql_meta__routine_par,
		.routine_par = _gda_mysql_meta_routine_par,
		._indexes_tab = _gda_mysql_meta__indexes_tab,
		.indexes_tab = _gda_mysql_meta_indexes_tab,
		._index_cols = _gda_mysql_meta__index_cols,
		.index_cols = _gda_mysql_meta_index_cols
	}
};

GdaProviderReuseableOperations *
_gda_mysql_reuseable_get_ops (void)
{
	return &_gda_mysql_reuseable;
}

#ifdef GDA_DEBUG
void
_gda_mysql_test_keywords (void)
{
	V50test_keywords();
        V51test_keywords();
        V54test_keywords();
        V60test_keywords();
}
#endif

GdaProviderReuseable *
_gda_mysql_reuseable_new_data (void)
{
	GdaMysqlReuseable *reuseable;
	reuseable = g_new0 (GdaMysqlReuseable, 1);
	reuseable->version_long = 0;
	reuseable->identifiers_case_sensitive = FALSE;
	_gda_mysql_provider_meta_init (NULL);

	((GdaProviderReuseable*)reuseable)->operations = &_gda_mysql_reuseable;

	return (GdaProviderReuseable*) reuseable;
}

void
_gda_mysql_reuseable_reset_data (GdaProviderReuseable *rdata)
{
	GdaMysqlReuseable *reuseable;
	reuseable = (GdaMysqlReuseable*) rdata;

	g_free (rdata->server_version);

	memset (reuseable, 0, sizeof (GdaMysqlReuseable));
}

static GdaDataModel *
execute_select (GdaConnection *cnc, GdaMysqlReuseable *rdata, const gchar *sql, GError **error)
{
	GdaSqlParser *parser;
	GdaStatement *stmt;
	GdaDataModel *model;
	parser = _gda_mysql_reuseable_create_parser ((GdaProviderReuseable*) rdata);

	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	g_object_unref (parser);
	g_assert (stmt);

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, error);
	g_object_unref (stmt);

	return model;
}

gboolean
_gda_mysql_compute_version (GdaConnection *cnc, GdaMysqlReuseable *rdata, GError **error)
{
	GdaSqlBuilder *b;
	GdaStatement *stmt;
	GdaDataModel *model;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
        const GdaSqlBuilderId id_func = gda_sql_builder_add_function (b, "version", 0);
        gda_sql_builder_add_field_value_id (b, id_func, 0);
	stmt = gda_sql_builder_get_statement (b, NULL);
	g_object_unref (b);
	g_assert (stmt);

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, error);
	g_object_unref (stmt);
	if (!model)
		return FALSE;

	const GValue *cvalue;
	cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
	if (!cvalue) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
                             GDA_SERVER_PROVIDER_INTERNAL_ERROR, "%s",
                             _("Can't import data from web server"));
		g_object_unref (model);
		return FALSE;
	}

	const gchar *str;
	str = g_value_get_string (cvalue);
	((GdaProviderReuseable*)rdata)->server_version = g_strdup (str);

	/* analyse string */
	const gchar *ptr;
	rdata->version_long = 0;

	/* go on  the first digit of version number */
        ptr = str;
	if (*ptr) {
		/* scan version parts */
		GdaProviderReuseable *prdata = (GdaProviderReuseable*) rdata;
		sscanf (ptr, "%d.%d.%d", &(prdata->major),  &(prdata->minor),  &(prdata->micro));

		/* elaborate the version number as a long int, similar to mysql_get_server_version() */
		rdata->version_long = prdata->major * 10000 + prdata->minor *100 + prdata->micro;

	}

	g_object_unref (model);

	/*g_print ("VERSIONS: [%ld] [%d.%d.%d]\n", rdata->version_long,
		 ((GdaProviderReuseable*)rdata)->major,
		 ((GdaProviderReuseable*)rdata)->minor,
		 ((GdaProviderReuseable*)rdata)->micro);*/

	if ((rdata->version_long / 10000) < 5)
		model = execute_select (cnc, rdata, "SHOW VARIABLES LIKE 'lower_case_table_names'", error);
	else
		model = execute_select (cnc, rdata, "SHOW VARIABLES WHERE Variable_name = 'lower_case_table_names'", error);
	if (!model)
		return FALSE;
	cvalue = gda_data_model_get_value_at (model, 1, 0, NULL);
	if (!cvalue) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
                             GDA_SERVER_PROVIDER_INTERNAL_ERROR, "%s",
                             _("Can't import data from web server"));
		g_object_unref (model);
		return FALSE;
	}
	str = g_value_get_string (cvalue);
	rdata->identifiers_case_sensitive = FALSE;
	if (atoi (str) == 0)
		rdata->identifiers_case_sensitive = TRUE;
	g_object_unref (model);

	return TRUE;
}

static GType
mysql_name_to_g_type (const gchar *name, const gchar *conv_func_name)
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
		return GDA_TYPE_BLOB;
	else if (!strcmp (name, "bytea"))
		return GDA_TYPE_BINARY;

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
		return GDA_TYPE_BLOB;
	if (!strncmp (conv_func_name, "bytea", 5))
		return GDA_TYPE_BINARY;
	return GDA_TYPE_NULL;
}

GType
_gda_mysql_reuseable_get_g_type (G_GNUC_UNUSED GdaConnection *cnc, G_GNUC_UNUSED GdaProviderReuseable *rdata,
				 const gchar *db_type)
{
	g_return_val_if_fail (db_type, GDA_TYPE_NULL);

	return mysql_name_to_g_type (db_type, NULL);
}


GdaSqlReservedKeywordsFunc
_gda_mysql_reuseable_get_reserved_keywords_func (GdaProviderReuseable *rdata)
{
	if (rdata) {
                switch (rdata->major) {
                case 5:
                        if (rdata->minor == 1)
                                return V51is_keyword;
                        if (rdata->minor == 0)
                                return V50is_keyword;
                        return V54is_keyword;
                case 6:
                default:
                        return V60is_keyword;
                break;
                }
        }
        return V60is_keyword;
}

GdaSqlParser *
_gda_mysql_reuseable_create_parser (G_GNUC_UNUSED GdaProviderReuseable *rdata)
{
	return GDA_SQL_PARSER (g_object_new (GDA_TYPE_MYSQL_PARSER, NULL));
}
