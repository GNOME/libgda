/*
 * Copyright (C) 2008 - 2012 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#include <string.h>
#include "gda-mysql-meta.h"
#include "gda-mysql-reuseable.h"
#include "gda-mysql-parser.h"
#include <libgda/gda-meta-store.h>
#include <libgda/gda-data-proxy.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/providers-support/gda-meta-column-types.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>

/*
 * predefined statements' IDs
 */
typedef enum {
	I_STMT_CATALOG,
        /* I_STMT_BTYPES, */
        I_STMT_SCHEMAS,
        I_STMT_SCHEMAS_ALL,
        I_STMT_SCHEMA_NAMED,
        I_STMT_TABLES,
        I_STMT_TABLES_ALL,
        I_STMT_TABLE_NAMED,
	I_STMT_VIEWS,
        I_STMT_VIEWS_ALL,
        I_STMT_VIEW_NAMED,
        I_STMT_COLUMNS_OF_TABLE,
        I_STMT_COLUMNS_ALL,
        I_STMT_TABLES_CONSTRAINTS,
        I_STMT_TABLES_CONSTRAINTS_ALL,
        I_STMT_TABLES_CONSTRAINTS_NAMED,
        I_STMT_REF_CONSTRAINTS,
        I_STMT_REF_CONSTRAINTS_ALL,
        I_STMT_KEY_COLUMN_USAGE,
        I_STMT_KEY_COLUMN_USAGE_ALL,
        /* I_STMT_UDT, */
        /* I_STMT_UDT_ALL, */
        /* I_STMT_UDT_COLUMNS, */
        /* I_STMT_UDT_COLUMNS_ALL, */
        /* I_STMT_DOMAINS, */
        /* I_STMT_DOMAINS_ALL, */
        /* I_STMT_DOMAINS_CONSTRAINTS, */
        /* I_STMT_DOMAINS_CONSTRAINTS_ALL, */
        I_STMT_CHARACTER_SETS,
        I_STMT_CHARACTER_SETS_ALL,
        I_STMT_VIEWS_COLUMNS,
        I_STMT_VIEWS_COLUMNS_ALL,
        I_STMT_TRIGGERS,
        I_STMT_TRIGGERS_ALL,
        /* I_STMT_EL_TYPES_COL, */
        /* I_STMT_EL_TYPES_DOM, */
        /* I_STMT_EL_TYPES_UDT, */
        /* I_STMT_EL_TYPES_ALL, */
        I_STMT_ROUTINES_ALL,
        I_STMT_ROUTINES,
        I_STMT_ROUTINES_ONE,
        I_STMT_ROUTINES_PAR_ALL,
        I_STMT_ROUTINES_PAR,
        /* I_STMT_ROUTINES_COL_ALL, */
        /* I_STMT_ROUTINES_COL, */

	I_STMT_INDEXES_ALL,
	I_STMT_INDEXES_TABLE,
	I_STMT_INDEXES_ONE,
	I_STMT_INDEX_COLUMNS_ALL,
	I_STMT_INDEX_COLUMNS_NAMED
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
#define SHORT_NAME(t,s,f) "CASE WHEN " t "= DATABASE() THEN " s " ELSE " f " END"
#define CATALOG_NAME "'mysql'"
static gchar *internal_sql[] = {
	/* I_STMT_CATALOG */
        "SELECT " CATALOG_NAME,

        /* I_STMT_BTYPES */

        /* I_STMT_SCHEMAS */
	"SELECT " CATALOG_NAME " AS catalog_name, schema_name, NULL, CASE WHEN schema_name = 'information_schema' OR schema_name = 'mysql' THEN TRUE ELSE FALSE END AS schema_internal, NULL FROM INFORMATION_SCHEMA.schemata",

        /* I_STMT_SCHEMAS_ALL */
	"SELECT " CATALOG_NAME " AS catalog_name, schema_name, NULL, CASE WHEN schema_name = 'information_schema' OR schema_name = 'mysql' THEN TRUE ELSE FALSE END AS schema_internal, NULL FROM INFORMATION_SCHEMA.schemata",

        /* I_STMT_SCHEMA_NAMED */
	"SELECT " CATALOG_NAME " AS catalog_name, schema_name, NULL, CASE WHEN schema_name = 'information_schema' OR schema_name = 'mysql' THEN TRUE ELSE FALSE END AS schema_internal FROM INFORMATION_SCHEMA.schemata WHERE schema_name = ##name::string",

        /* I_STMT_TABLES */
	"SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, CASE WHEN table_type LIKE '%VIEW%' THEN 'VIEW' ELSE table_type END, CASE table_type WHEN 'BASE TABLE' THEN TRUE ELSE FALSE END AS table_type, table_comment, " SHORT_NAME("table_schema", "table_name", "CONCAT(table_schema, '.', table_name)") " AS short_name, CONCAT(table_schema, '.', table_name) AS table_full_name, NULL AS table_owner FROM INFORMATION_SCHEMA.tables WHERE table_schema = BINARY ##schema::string",

        /* I_STMT_TABLES_ALL */
	"SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, CASE WHEN table_type LIKE '%VIEW%' THEN 'VIEW' ELSE table_type END, CASE table_type WHEN 'BASE TABLE' THEN TRUE ELSE FALSE END AS table_type, table_comment, " SHORT_NAME("table_schema", "table_name", "CONCAT(table_schema, '.', table_name)") " AS short_name, CONCAT(table_schema, '.', table_name) AS table_full_name, NULL AS table_owner FROM INFORMATION_SCHEMA.tables",

        /* I_STMT_TABLE_NAMED */
	"SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, CASE WHEN table_type LIKE '%VIEW%' THEN 'VIEW' ELSE table_type END, CASE table_type WHEN 'BASE TABLE' THEN TRUE ELSE FALSE END AS table_type, table_comment, " SHORT_NAME("table_schema", "table_name", "CONCAT(table_schema, '.', table_name)") " as short_name, CONCAT(table_schema, '.', table_name) AS table_full_name, NULL AS table_owner FROM INFORMATION_SCHEMA.tables WHERE table_schema = BINARY ##schema::string AND table_name = BINARY ##name::string",

	/* I_STMT_VIEWS */
	"SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, view_definition, check_option, is_updatable FROM INFORMATION_SCHEMA.views WHERE table_schema = BINARY ##schema::string  UNION SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, NULL, NULL, FALSE FROM INFORMATION_SCHEMA.tables WHERE table_schema='information_schema' AND table_type LIKE '%VIEW%' AND table_schema = BINARY ##schema::string",

        /* I_STMT_VIEWS_ALL */
	"SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, view_definition, check_option, is_updatable FROM INFORMATION_SCHEMA.views UNION SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, NULL, NULL, FALSE FROM INFORMATION_SCHEMA.tables WHERE table_schema='information_schema' and table_type LIKE '%VIEW%'",

        /* I_STMT_VIEW_NAMED */
	"SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, view_definition, check_option, is_updatable FROM INFORMATION_SCHEMA.views WHERE table_schema = BINARY ##schema::string AND table_name = BINARY ##name::string UNION SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, NULL, NULL, FALSE FROM INFORMATION_SCHEMA.tables WHERE table_schema='information_schema' AND table_type LIKE '%VIEW%' AND table_schema = BINARY ##schema::string AND table_name = BINARY ##name::string",

        /* I_STMT_COLUMNS_OF_TABLE */
	"SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, column_name, ordinal_position, column_default, CASE is_nullable WHEN 'YES' THEN TRUE ELSE FALSE END AS is_nullable, data_type, NULL, 'gchararray', character_maximum_length,character_octet_length, numeric_precision, numeric_scale, 0, character_set_name, character_set_name, character_set_name, collation_name, collation_name, collation_name, CASE WHEN extra = 'auto_increment' then '" GDA_EXTRA_AUTO_INCREMENT "' ELSE extra END, IF(FIND_IN_SET('insert', privileges) != 0 OR FIND_IN_SET('update', privileges) != 0, TRUE, FALSE) AS is_updatable, column_comment FROM INFORMATION_SCHEMA.columns WHERE table_schema = BINARY ##schema::string AND table_name = BINARY ##name::string",

        /* I_STMT_COLUMNS_ALL */
	"SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, column_name, ordinal_position, column_default, CASE is_nullable WHEN 'YES' THEN TRUE ELSE FALSE END AS is_nullable, data_type, NULL, 'gchararray', character_maximum_length, character_octet_length, numeric_precision, numeric_scale, 0, character_set_name, character_set_name, character_set_name, collation_name, collation_name, collation_name, CASE WHEN extra = 'auto_increment' then '" GDA_EXTRA_AUTO_INCREMENT "' ELSE extra END, IF(FIND_IN_SET('insert', privileges) != 0 OR FIND_IN_SET('update', privileges) != 0, TRUE, FALSE) AS is_updatable, column_comment FROM INFORMATION_SCHEMA.columns",

        /* I_STMT_TABLES_CONSTRAINTS */
	"SELECT " CATALOG_NAME " AS constraint_catalog, constraint_schema, constraint_name, " CATALOG_NAME " AS constraint_catalog, table_schema, table_name, constraint_type, NULL, FALSE, FALSE FROM INFORMATION_SCHEMA.table_constraints WHERE constraint_schema = BINARY ##schema::string AND table_name = BINARY ##name::string",

        /* I_STMT_TABLES_CONSTRAINTS_ALL */
	"SELECT " CATALOG_NAME " AS constraint_catalog, constraint_schema, constraint_name, " CATALOG_NAME " AS constraint_catalog, table_schema, table_name, constraint_type, NULL, FALSE, FALSE FROM INFORMATION_SCHEMA.table_constraints",

        /* I_STMT_TABLES_CONSTRAINTS_NAMED */
	"SELECT " CATALOG_NAME " AS constraint_catalog, constraint_schema, constraint_name, " CATALOG_NAME " AS table_catalog, table_schema, table_name, constraint_type, NULL, FALSE, FALSE FROM INFORMATION_SCHEMA.table_constraints WHERE constraint_schema = BINARY ##schema::string AND table_name = BINARY ##name::string AND constraint_name = BINARY ##name2::string",

        /* I_STMT_REF_CONSTRAINTS */
	"select " CATALOG_NAME " AS constraint_catalog, constraint_schema, table_name, constraint_name, " CATALOG_NAME " AS ref_table_cat, constraint_schema, referenced_table_name, unique_constraint_name, match_option, update_rule, delete_rule from information_schema.referential_constraints WHERE constraint_schema = BINARY ##schema::string AND table_name = BINARY ##name::string AND constraint_name = BINARY ##name2::string AND unique_constraint_name IS NOT NULL",

        /* I_STMT_REF_CONSTRAINTS_ALL */
	"select " CATALOG_NAME " AS constraint_catalog, constraint_schema, table_name, constraint_name, " CATALOG_NAME " AS ref_table_cat, constraint_schema, referenced_table_name, unique_constraint_name, match_option, update_rule, delete_rule from information_schema.referential_constraints WHERE unique_constraint_name IS NOT NULL",

        /* I_STMT_KEY_COLUMN_USAGE */
	"SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, constraint_name, column_name, ordinal_position FROM INFORMATION_SCHEMA.key_column_usage WHERE table_schema = BINARY ##schema::string AND table_name = BINARY ##name::string AND constraint_name = BINARY ##name2::string",

        /* I_STMT_KEY_COLUMN_USAGE_ALL */
	"SELECT " CATALOG_NAME " AS table_catalog, table_schema, table_name, constraint_name, column_name, ordinal_position FROM INFORMATION_SCHEMA.key_column_usage",

        /* I_STMT_CHARACTER_SETS */
	"SELECT " CATALOG_NAME " AS character_set_catalog, DATABASE() AS character_set_schema, character_set_name, default_collate_name, default_collate_name, default_collate_name, description, character_set_name, character_set_name FROM INFORMATION_SCHEMA.character_sets WHERE character_set_schema = BINARY ##schema::string AND character_set_name = BINARY ##name::string",

        /* I_STMT_CHARACTER_SETS_ALL */
	"SELECT " CATALOG_NAME " AS character_set_catalog, DATABASE() AS character_set_schema, character_set_name, default_collate_name, default_collate_name, default_collate_name, description, character_set_name, character_set_name FROM INFORMATION_SCHEMA.character_sets",

        /* I_STMT_UDT */

        /* I_STMT_UDT_ALL */

        /* I_STMT_UDT_COLUMNS */

        /* I_STMT_UDT_COLUMNS_ALL */

        /* I_STMT_DOMAINS */

        /* I_STMT_DOMAINS_ALL */

        /* I_STMT_DOMAINS_CONSTRAINTS */

        /* I_STMT_DOMAINS_CONSTRAINTS_ALL */

        /* I_STMT_VIEWS_COLUMNS */
	"SELECT " CATALOG_NAME " AS table_catalog, v.table_schema, v.table_name, " CATALOG_NAME " AS table_catalog, c.table_schema, c.table_name, c.column_name FROM INFORMATION_SCHEMA.columns c INNER JOIN INFORMATION_SCHEMA.views v ON c.table_schema=v.table_schema AND c.table_name=v.table_name WHERE v.table_schema = BINARY ##schema::string AND v.table_name = BINARY ##name::string",

        /* I_STMT_VIEWS_COLUMNS_ALL */
	"SELECT " CATALOG_NAME " AS table_catalog, v.table_schema, v.table_name, " CATALOG_NAME " AS table_catalog, c.table_schema, c.table_name, c.column_name FROM INFORMATION_SCHEMA.columns c INNER JOIN INFORMATION_SCHEMA.views v ON c.table_schema=v.table_schema AND c.table_name=v.table_name",

        /* I_STMT_TRIGGERS */
	"SELECT " CATALOG_NAME " AS trigger_catalog, trigger_schema, trigger_name, event_manipulation, " CATALOG_NAME " AS event_object_catalog, event_object_schema, event_object_table, action_statement, action_orientation, action_timing, NULL, trigger_name, trigger_name FROM INFORMATION_SCHEMA.triggers WHERE trigger_schema =  BINARY ##schema::string AND trigger_name = BINARY ##name::string",

        /* I_STMT_TRIGGERS_ALL */
	"SELECT " CATALOG_NAME " AS trigger_catalog, trigger_schema, trigger_name, event_manipulation, " CATALOG_NAME " AS event_object_catalog, event_object_schema, event_object_table, action_statement, action_orientation, action_timing, NULL, trigger_name, trigger_name FROM INFORMATION_SCHEMA.triggers",

        /* I_STMT_EL_TYPES_COL */

        /* I_STMT_EL_TYPES_DOM */

        /* I_STMT_EL_TYPES_UDT */

        /* I_STMT_EL_TYPES_ALL */

        /* I_STMT_ROUTINES_ALL */
	"SELECT " CATALOG_NAME " AS specific_catalog, routine_schema AS specific_schema, routine_name AS specific_name, " CATALOG_NAME " AS routine_catalog, routine_schema, routine_name, routine_type, dtd_identifier AS return_type, FALSE AS returns_set, 0 AS nb_args, routine_body, routine_definition, external_name, external_language, parameter_style, CASE is_deterministic WHEN 'YES' THEN TRUE ELSE FALSE END AS is_deterministic, sql_data_access, FALSE AS is_null_call, routine_comment, routine_name, routine_name, definer FROM INFORMATION_SCHEMA.routines",

        /* I_STMT_ROUTINES */
	"SELECT " CATALOG_NAME " AS specific_catalog, routine_schema AS specific_schema, routine_name AS specific_name, " CATALOG_NAME " AS routine_catalog, routine_schema, routine_name, routine_type, dtd_identifier AS return_type, FALSE AS returns_set, 0 AS nb_args, routine_body, routine_definition, external_name, external_language, parameter_style, CASE is_deterministic WHEN 'YES' THEN TRUE ELSE FALSE END AS is_deterministic, sql_data_access, FALSE AS is_null_call, routine_comment, routine_name, routine_name, definer FROM INFORMATION_SCHEMA.routines WHERE routine_schema =  BINARY ##schema::string",

        /* I_STMT_ROUTINES_ONE */
	"SELECT " CATALOG_NAME " AS specific_catalog, routine_schema AS specific_schema, routine_name AS specific_name, " CATALOG_NAME " AS routine_catalog, routine_schema, routine_name, routine_type, dtd_identifier AS return_type, FALSE AS returns_set, 0 AS nb_args, routine_body, routine_definition, external_name, external_language, parameter_style, CASE is_deterministic WHEN 'YES' THEN TRUE ELSE FALSE END AS is_deterministic, sql_data_access, FALSE AS is_null_call, routine_comment, routine_name, routine_name, definer FROM INFORMATION_SCHEMA.routines WHERE routine_schema =  BINARY ##schema::string AND routine_name = BINARY ##name::string",

        /* I_STMT_ROUTINES_PAR_ALL */
	"SELECT " CATALOG_NAME " AS specific_catalog, routine_schema AS specific_schema, routine_name AS specific_name, " CATALOG_NAME " AS routine_catalog, routine_schema, routine_name, routine_type, dtd_identifier AS return_type, FALSE AS returns_set, 0 AS nb_args, routine_body, routine_definition, external_name, external_language, parameter_style, CASE is_deterministic WHEN 'YES' THEN TRUE ELSE FALSE END AS is_deterministic, sql_data_access, FALSE AS is_null_call, routine_comment, routine_name, routine_name, definer FROM INFORMATION_SCHEMA.routines",

        /* I_STMT_ROUTINES_PAR */
	"SELECT " CATALOG_NAME " AS specific_catalog, routine_schema AS specific_schema, routine_name AS specific_name, " CATALOG_NAME " AS routine_catalog, routine_schema, routine_name, routine_type, dtd_identifier AS return_type, FALSE AS returns_set, 0 AS nb_args, routine_body, routine_definition, external_name, external_language, parameter_style, CASE is_deterministic WHEN 'YES' THEN TRUE ELSE FALSE END AS is_deterministic, sql_data_access, FALSE AS is_null_call, routine_comment, routine_name, routine_name, definer FROM INFORMATION_SCHEMA.routines WHERE routine_schema = BINARY ##schema::string AND routine_name = BINARY ##name::string",

        /* I_STMT_ROUTINES_COL_ALL */

        /* I_STMT_ROUTINES_COL */

	/* I_STMT_INDEXES_ALL */
	"SELECT DISTINCT " CATALOG_NAME ", INDEX_SCHEMA, INDEX_NAME, " CATALOG_NAME ", TABLE_SCHEMA, TABLE_NAME, NOT NON_UNIQUE, NULL, INDEX_TYPE, NULL, NULL, COMMENT FROM INFORMATION_SCHEMA.STATISTICS WHERE INDEX_NAME <> 'PRIMARY'",

	/* I_STMT_INDEXES_TABLE */
	"SELECT DISTINCT " CATALOG_NAME ", INDEX_SCHEMA, INDEX_NAME, " CATALOG_NAME ", TABLE_SCHEMA, TABLE_NAME, NOT NON_UNIQUE, NULL, INDEX_TYPE, NULL, NULL, COMMENT FROM INFORMATION_SCHEMA.STATISTICS WHERE INDEX_NAME <> 'PRIMARY' AND TABLE_SCHEMA = BINARY ##schema::string AND TABLE_NAME = BINARY ##name::string",

	/* I_STMT_INDEXES_ONE */
	"SELECT DISTINCT " CATALOG_NAME ", INDEX_SCHEMA, INDEX_NAME, " CATALOG_NAME ", TABLE_SCHEMA, TABLE_NAME, NOT NON_UNIQUE, NULL, INDEX_TYPE, NULL, NULL, COMMENT FROM INFORMATION_SCHEMA.STATISTICS WHERE INDEX_NAME <> 'PRIMARY' AND TABLE_SCHEMA = BINARY ##schema::string AND TABLE_NAME = BINARY ##name::string AND INDEX_NAME = ##name2::string",

	/* I_STMT_INDEX_COLUMNS_ALL */
	"SELECT DISTINCT " CATALOG_NAME ", INDEX_SCHEMA, INDEX_NAME, " CATALOG_NAME ", TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, COLUMN_NAME, SEQ_IN_INDEX FROM INFORMATION_SCHEMA.STATISTICS WHERE INDEX_NAME <> 'PRIMARY' ORDER BY INDEX_SCHEMA, INDEX_NAME, SEQ_IN_INDEX",

	/* I_STMT_INDEX_COLUMNS_NAMED */
	"SELECT DISTINCT " CATALOG_NAME ", INDEX_SCHEMA, INDEX_NAME, " CATALOG_NAME ", TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, COLUMN_NAME, SEQ_IN_INDEX FROM INFORMATION_SCHEMA.STATISTICS WHERE INDEX_NAME <> 'PRIMARY' AND TABLE_SCHEMA = BINARY ##schema::string AND TABLE_NAME = BINARY ##name::string AND INDEX_NAME = ##name2::string ORDER BY INDEX_SCHEMA, INDEX_NAME, SEQ_IN_INDEX"
};

/*
 * global static values, and
 * predefined statements' GdaStatement, all initialized in _gda_postgres_provider_meta_init()
 */
static GMutex init_mutex;
static GdaStatement **internal_stmt = NULL;
static GdaSet        *i_set = NULL;

/*
 * Meta initialization
 */
void
_gda_mysql_provider_meta_init (GdaServerProvider  *provider)
{
	g_mutex_lock (&init_mutex);

	if (!internal_stmt) {
		InternalStatementItem i;
		GdaSqlParser *parser;

		if (provider)
			parser = gda_server_provider_internal_get_parser (provider);
		else
			parser = GDA_SQL_PARSER (g_object_new (GDA_TYPE_MYSQL_PARSER, NULL));
		internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
		for (i = I_STMT_CATALOG; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
			internal_stmt[i] = gda_sql_parser_parse_string (parser, internal_sql[i], NULL, NULL);
			if (!internal_stmt[i])
				g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		}

		if (!provider)
			g_object_unref (parser);

		/* initialize static values here */
		i_set = gda_set_new_inline (3, "name", G_TYPE_STRING, "",
					    "schema", G_TYPE_STRING, "",
					    "name2", G_TYPE_STRING, "");
	}

	g_mutex_unlock (&init_mutex);

#ifdef GDA_DEBUG
	_gda_mysql_test_keywords ();
#endif
}

typedef struct {
	GdaServerProviderConnectionData parent;
        gpointer reuseable;
} CncDataGen;

#define GDA_MYSQL_GET_REUSEABLE_DATA(cdata) ((cdata) ? (GdaMysqlReuseable*) (((CncDataGen*) (cdata))->reuseable) : NULL)

gboolean
_gda_mysql_meta__info (G_GNUC_UNUSED GdaServerProvider  *prov,
		       GdaConnection      *cnc, 
		       GdaMetaStore       *store,
		       GdaMetaContext     *context,
		       GError            **error)
{
	GdaDataModel *model;
        gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_CATALOG],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_information_schema_catalog_name,
							      error);
        if (model == NULL)
                retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
		g_object_unref (G_OBJECT (model));
	}

        return retval;
}

gboolean
_gda_mysql_meta__btypes (G_GNUC_UNUSED GdaServerProvider  *prov,
			 GdaConnection      *cnc, 
			 GdaMetaStore       *store,
			 GdaMetaContext     *context,
			 GError            **error)
{
#if MYSQL8
	// No supported
	return TRUE;
#else
	typedef struct
	{
		gchar  *tname;
		gchar  *gtype;
		gchar  *comments;
		gchar  *synonyms;
	} BuiltinDataType;
	BuiltinDataType data_types[] = {
		{ "AUTO_INCREMENT", "gint", "The AUTO_INCREMENT attribute can be used to generate a unique identity for new rows", "" },
		{ "BIGINT", "gint64", "A large integer. The signed range is -9223372036854775808 to 9223372036854775807. The unsigned range is 0 to 18446744073709551615.", "" },
		{ "BINARY", "GdaBinary", "The BINARY type is similar to the CHAR type, but stores binary byte strings rather than non-binary character strings. M represents the column length in bytes.", "CHAR BYTE" },
		{ "BIT", "gint", "A bit-field type. M indicates the number of bits per value, from 1 to 64. The default is 1 if M is omitted.", "" },
		{ "BLOB", "GdaBinary", "A BLOB column with a maximum length of 65,535 (216 - 1) bytes. Each BLOB value is stored using a two-byte length prefix that indicates the number of bytes in the value.", "" },
		{ "BLOB DATA TYPE", "GdaBinary", "A BLOB is a binary large object that can hold a variable amount of data. The four BLOB types are TINYBLOB, BLOB, MEDIUMBLOB, and LONGBLOB. These differ only in the maximum length of the values they can hold.", "" },
		{ "BOOLEAN", "gboolean", "These types are synonyms for TINYINT(1). A value of zero is considered false. Non-zero values are considered true", "" },
		{ "CHAR", "gchararray", "A fixed-length string that is always right-padded with spaces to the specified length when stored. M represents the column length in characters. The range of M is 0 to 255. If M is omitted, the length is 1.", "" },
		{ "DATE", "GDate", "A date. The supported range is '1000-01-01' to '9999-12-31'. MySQL displays DATE values in 'YYYY-MM-DD' format, but allows assignment of values to DATE columns using either strings or numbers.", "" },
		{ "DATETIME", "GDateTime", "A date and time combination. The supported range is '1000-01-01 00:00:00' to '9999-12-31 23:59:59'. MySQL displays DATETIME values in 'YYYY-MM-DD HH:MM:SS' format, but allows assignment of values to DATETIME columns using either strings or numbers.", "" },
		{ "DECIMAL", "GdaNumeric", "A packed \"exact\" fixed-point number. M is the total number of digits (the precision) and D is the number of digits after the decimal point (the scale). The decimal point and (for negative numbers) the \"-\" sign are not counted in M. If D is 0, values have no decimal point or fractional part. The maximum number of digits (M) for DECIMAL is 65 (64 from 5.0.3 to 5.0.5). The maximum number of supported decimals (D) is 30. If D is omitted, the default is 0. If M is omitted, the default is 10.", "DEC" },
		{ "DOUBLE", "gdouble", "A normal-size (double-precision) floating-point number. Allowable values are -1.7976931348623157E+308 to -2.2250738585072014E-308, 0, and 2.2250738585072014E-308 to 1.7976931348623157E+308. These are the theoretical limits, based on the IEEE standard. The actual range might be slightly smaller depending on your hardware or operating system.", "DOUBLE PRECISION" },
		{ "ENUM", "gchararray", "An enumeration. A string object that can have only one value, chosen from the list of values 'value1', 'value2', ..., NULL or the special '' error value. An ENUM column can have a maximum of 65,535 distinct values. ENUM values are represented internally as integers.", "" },
		{ "FLOAT", "gfloat", "A small (single-precision) floating-point number. Allowable values are -3.402823466E+38 to -1.175494351E-38, 0, and 1.175494351E-38 to 3.402823466E+38. These are the theoretical limits, based on the IEEE standard. The actual range might be slightly smaller depending on your hardware or operating system.", "" },
		{ "INT", "gint", "A normal-size integer. The signed range is -2147483648 to 2147483647. The unsigned range is 0 to 4294967295.", "INTEGER" },
		{ "LONGBLOB", "GdaBinary", "A BLOB column with a maximum length of 4,294,967,295 or 4GB (232 - 1) bytes. The effective maximum length of LONGBLOB columns depends on the configured maximum packet size in the client/server protocol and available memory. Each LONGBLOB value is stored using a four-byte length prefix that indicates the number of bytes in the value.", "" },
		{ "LONGTEXT", "GdaBinary", "A TEXT column with a maximum length of 4,294,967,295 or 4GB (232 - 1) characters. The effective maximum length is less if the value contains multi-byte characters. The effective maximum length of LONGTEXT columns also depends on the configured maximum packet size in the client/server protocol and available memory. Each LONGTEXT value is stored using a four-byte length prefix that indicates the number of bytes in the value.", "" },
		{ "MEDIUMBLOB", "GdaBinary", "A BLOB column with a maximum length of 16,777,215 (224 - 1) bytes. Each MEDIUMBLOB value is stored using a three-byte length prefix that indicates the number of bytes in the value.", "" },
		{ "MEDIUMINT", "gint", "A medium-sized integer. The signed range is -8388608 to 8388607. The unsigned range is 0 to 16777215.", "" },
		{ "MEDIUMTEXT", "GdaBinary", "A TEXT column with a maximum length of 16,777,215 (224 - 1) characters. The effective maximum length is less if the value contains multi-byte characters. Each MEDIUMTEXT value is stored using a three-byte length prefix that indicates the number of bytes in the value.", "" },
		{ "SET DATA TYPE", "gchararray", "A set. A string object that can have zero or more values, each of which must be chosen from the list of values 'value1', 'value2', ... A SET column can have a maximum of 64 members. SET values are represented internally as integers.", "" },
		{ "SMALLINT", "gshort", "A small integer. The signed range is -32768 to 32767. The unsigned range is 0 to 65535.", "" },
		{ "TEXT", "GdaText", "A TEXT column with a maximum length of 65,535 (216 - 1) characters. The effective maximum length is less if the value contains multi-byte characters. Each TEXT value is stored using a two-byte length prefix that indicates the number of bytes in the value.", "" },
		{ "TIME", "GdaTime", "A time. The range is '-838:59:59' to '838:59:59'. MySQL displays TIME values in 'HH:MM:SS' format, but allows assignment of values to TIME columns using either strings or numbers.", "" },
		{ "TIMESTAMP", "GDateTime", "A timestamp. The range is '1970-01-01 00:00:01' UTC to partway through the year 2038. TIMESTAMP values are stored as the number of seconds since the epoch ('1970-01-01 00:00:00' UTC). A TIMESTAMP cannot represent the value '1970-01-01 00:00:00' because that is equivalent to 0 seconds from the epoch and the value 0 is reserved for representing '0000-00-00 00:00:00', the \"zero\" TIMESTAMP value.", "" },
		{ "TINYBLOB", "GdaBinary", "A BLOB column with a maximum length of 255 (28 - 1) bytes. Each TINYBLOB value is stored using a one-byte length prefix that indicates the number of bytes in the value.", "" },
		{ "TINYINT", "gchar", "A very small integer. The signed range is -128 to 127. The unsigned range is 0 to 255.", "" },
		{ "TINYTEXT", "GdaBinary", "A TEXT column with a maximum length of 255 (28 - 1) characters. The effective maximum length is less if the value contains multi-byte characters. Each TINYTEXT value is stored using a one-byte length prefix that indicates the number of bytes in the value.", "" },
		{ "VARBINARY", "GdaBinary", "The VARBINARY type is similar to the VARCHAR type, but stores binary byte strings rather than non-binary character strings. M represents the maximum column length in bytes.", "" },
		{ "VARCHAR", "gchararray", "A variable-length string. M represents the maximum column length in characters. In MySQL 5.0, the range of M is 0 to 255 before MySQL 5.0.3, and 0 to 65,535 in MySQL 5.0.3 and later. The effective maximum length of a VARCHAR in MySQL 5.0.3 and later is subject to the maximum row size (65,535 bytes, which is shared among all columns) and the character set used. For example, utf8 characters can require up to three bytes per character, so a VARCHAR column that uses the utf8 character set can be declared to be a maximum of 21,844 characters.", "" },
		{ "YEAR DATA TYPE", "gint", "A year in two-digit or four-digit format. The default is four-digit format. In four-digit format, the allowable values are 1901 to 2155, and 0000. In two-digit format, the allowable values are 70 to 69, representing years from 1970 to 2069. MySQL displays YEAR values in YYYY format, but allows you to assign values to YEAR columns using either strings or numbers.", "" },
		{ "POINT", "GdaGeometricPoint", "A point with x and y coordinates", "" }
	};
        GdaDataModel *model;
        gboolean retval = TRUE;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
        if (model == NULL)
                retval = FALSE;
	else {
		gsize i;
		for (i = 0; i < sizeof(data_types) / sizeof(BuiltinDataType); ++i) {
			BuiltinDataType *data_type = &(data_types[i]);
			GList *values = NULL;
			GValue *tmp_value = NULL;

			g_value_set_string (tmp_value = gda_value_new (G_TYPE_STRING), data_type->tname);
			values = g_list_append (values, tmp_value); 

			g_value_set_string (tmp_value = gda_value_new (G_TYPE_STRING), data_type->tname);
			values = g_list_append (values, tmp_value); 

			g_value_set_string (tmp_value = gda_value_new (G_TYPE_STRING), data_type->gtype);
			values = g_list_append (values, tmp_value); 

			g_value_set_string (tmp_value = gda_value_new (GDA_TYPE_TEXT), data_type->comments);
			values = g_list_append (values, tmp_value); 

			if (data_type->synonyms && *(data_type->synonyms))
				g_value_set_string (tmp_value = gda_value_new (G_TYPE_STRING), data_type->synonyms);
			else
				tmp_value = gda_value_new_null ();
			values = g_list_append (values, tmp_value); 

			g_value_set_int (tmp_value = gda_value_new (G_TYPE_INT), 0);
			values = g_list_append (values, tmp_value); 

			if (gda_data_model_append_values (model, values, NULL) < 0) {
				retval = FALSE;
				break;
			}

			g_list_free_full (values, (GDestroyNotify) gda_value_free);
		}

		if (retval) {
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
		}
		g_object_unref (G_OBJECT(model));
	}

	return retval;
#endif
}

gboolean
_gda_mysql_meta__udt (G_GNUC_UNUSED GdaServerProvider  *prov,
		      G_GNUC_UNUSED GdaConnection      *cnc,
		      G_GNUC_UNUSED GdaMetaStore       *store,
		      G_GNUC_UNUSED GdaMetaContext     *context,
		      G_GNUC_UNUSED GError            **error)
{
	// TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_udt (G_GNUC_UNUSED GdaServerProvider  *prov,
		     G_GNUC_UNUSED GdaConnection      *cnc,
		     G_GNUC_UNUSED GdaMetaStore       *store,
		     G_GNUC_UNUSED GdaMetaContext     *context,
		     G_GNUC_UNUSED GError            **error,
		     G_GNUC_UNUSED const GValue       *udt_catalog,
		     G_GNUC_UNUSED const GValue       *udt_schema)
{
	// TO_IMPLEMENT;
	return TRUE;
}


gboolean
_gda_mysql_meta__udt_cols (G_GNUC_UNUSED GdaServerProvider  *prov,
			   G_GNUC_UNUSED GdaConnection      *cnc,
			   G_GNUC_UNUSED GdaMetaStore       *store,
			   G_GNUC_UNUSED GdaMetaContext     *context,
			   G_GNUC_UNUSED GError            **error)
{
	// TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_mysql_meta_udt_cols (G_GNUC_UNUSED GdaServerProvider  *prov,
			  G_GNUC_UNUSED GdaConnection      *cnc,
			  G_GNUC_UNUSED GdaMetaStore       *store,
			  G_GNUC_UNUSED GdaMetaContext     *context,
			  G_GNUC_UNUSED GError            **error,
			  G_GNUC_UNUSED const GValue       *udt_catalog,
			  G_GNUC_UNUSED const GValue       *udt_schema,
			  G_GNUC_UNUSED const GValue       *udt_name)
{
	// TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_mysql_meta__enums (G_GNUC_UNUSED GdaServerProvider  *prov,
			G_GNUC_UNUSED GdaConnection      *cnc,
			G_GNUC_UNUSED GdaMetaStore       *store,
			G_GNUC_UNUSED GdaMetaContext     *context,
			G_GNUC_UNUSED GError            **error)
{
	/* Feature not supported by MySQL. */
	return TRUE;	
}

gboolean
_gda_mysql_meta_enums (G_GNUC_UNUSED GdaServerProvider  *prov,
		       G_GNUC_UNUSED GdaConnection      *cnc,
		       G_GNUC_UNUSED GdaMetaStore       *store,
		       G_GNUC_UNUSED GdaMetaContext     *context,
		       G_GNUC_UNUSED GError            **error,
		       G_GNUC_UNUSED const GValue       *udt_catalog,
		       G_GNUC_UNUSED const GValue       *udt_schema,
		       G_GNUC_UNUSED const GValue       *udt_name)
{
	/* Feature not supported by MySQL. */
	return TRUE;
}


gboolean
_gda_mysql_meta__domains (G_GNUC_UNUSED GdaServerProvider  *prov,
			  G_GNUC_UNUSED GdaConnection      *cnc,
			  G_GNUC_UNUSED GdaMetaStore       *store,
			  G_GNUC_UNUSED GdaMetaContext     *context,
			  G_GNUC_UNUSED GError            **error)
{
	/* Feature not supported by MySQL. */
	return TRUE;
}

gboolean
_gda_mysql_meta_domains (G_GNUC_UNUSED GdaServerProvider  *prov,
			 G_GNUC_UNUSED GdaConnection      *cnc,
			 G_GNUC_UNUSED GdaMetaStore       *store,
			 G_GNUC_UNUSED GdaMetaContext     *context,
			 G_GNUC_UNUSED GError            **error,
			 G_GNUC_UNUSED const GValue       *domain_catalog,
			 G_GNUC_UNUSED const GValue       *domain_schema)
{
	/* Feature not supported by MySQL. */
	return TRUE;
}

gboolean
_gda_mysql_meta__constraints_dom (G_GNUC_UNUSED GdaServerProvider  *prov,
				  G_GNUC_UNUSED GdaConnection      *cnc,
				  G_GNUC_UNUSED GdaMetaStore       *store,
				  G_GNUC_UNUSED GdaMetaContext     *context,
				  G_GNUC_UNUSED GError            **error)
{
	/* Feature not supported by MySQL. */
	return TRUE;
}

gboolean
_gda_mysql_meta_constraints_dom (G_GNUC_UNUSED GdaServerProvider  *prov,
				 G_GNUC_UNUSED GdaConnection      *cnc,
				 G_GNUC_UNUSED GdaMetaStore       *store,
				 G_GNUC_UNUSED GdaMetaContext     *context,
				 G_GNUC_UNUSED GError            **error,
				 G_GNUC_UNUSED const GValue       *domain_catalog,
				 G_GNUC_UNUSED const GValue       *domain_schema, 
				 G_GNUC_UNUSED const GValue       *domain_name)
{
	/* Feature not supported by MySQL. */
	return TRUE;
}

gboolean
_gda_mysql_meta__el_types (G_GNUC_UNUSED GdaServerProvider  *prov,
			   G_GNUC_UNUSED GdaConnection      *cnc,
			   G_GNUC_UNUSED GdaMetaStore       *store,
			   G_GNUC_UNUSED GdaMetaContext     *context,
			   G_GNUC_UNUSED GError            **error)
{
	/* Feature not supported by MySQL. */
	return TRUE;
}

gboolean
_gda_mysql_meta_el_types (G_GNUC_UNUSED GdaServerProvider  *prov,
			  G_GNUC_UNUSED GdaConnection      *cnc,
			  G_GNUC_UNUSED GdaMetaStore       *store,
			  G_GNUC_UNUSED GdaMetaContext     *context,
			  G_GNUC_UNUSED GError            **error,
			  G_GNUC_UNUSED const GValue       *specific_name)
{
	/* Feature not supported by MySQL. */
        return TRUE;
}

gboolean
_gda_mysql_meta__collations (G_GNUC_UNUSED GdaServerProvider  *prov,
			     G_GNUC_UNUSED GdaConnection      *cnc,
			     G_GNUC_UNUSED GdaMetaStore       *store,
			     G_GNUC_UNUSED GdaMetaContext     *context,
			     G_GNUC_UNUSED GError            **error)
{
	/* Feature not supported by MySQL. */
	return TRUE;
}

gboolean
_gda_mysql_meta_collations (G_GNUC_UNUSED GdaServerProvider  *prov,
			    G_GNUC_UNUSED GdaConnection      *cnc,
			    G_GNUC_UNUSED GdaMetaStore       *store,
			    G_GNUC_UNUSED GdaMetaContext     *context,
			    G_GNUC_UNUSED GError            **error,
			    G_GNUC_UNUSED const GValue       *collation_catalog,
			    G_GNUC_UNUSED const GValue       *collation_schema,
			    G_GNUC_UNUSED const GValue       *collation_name_n)
{
	/* Feature not supported by MySQL. */
	return TRUE;
}

gboolean
_gda_mysql_meta__character_sets (G_GNUC_UNUSED GdaServerProvider  *prov,
				 GdaConnection      *cnc, 
				 GdaMetaStore       *store,
				 GdaMetaContext     *context,
				 GError            **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_CHARACTER_SETS_ALL],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_character_sets,
							      error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_character_sets (G_GNUC_UNUSED GdaServerProvider  *prov,
				GdaConnection      *cnc, 
				GdaMetaStore       *store,
				GdaMetaContext     *context,
				GError            **error,
				G_GNUC_UNUSED const GValue       *chset_catalog,
				const GValue       *chset_schema, 
				const GValue       *chset_name_n)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), chset_schema, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), chset_name_n, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_CHARACTER_SETS],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_character_sets,
							      error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

gboolean
_gda_mysql_meta__schemata (G_GNUC_UNUSED GdaServerProvider  *prov,
			   GdaConnection      *cnc, 
			   GdaMetaStore       *store,
			   GdaMetaContext     *context,
			   GError            **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_SCHEMAS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_schemata, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_schemata (G_GNUC_UNUSED GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error, 
			  G_GNUC_UNUSED const GValue       *catalog_name,
			  const GValue       *schema_name_n)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (!schema_name_n) {
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_SCHEMAS],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_schemata,
								      error);
		if (model == NULL)
			retval = FALSE;
		else {
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
			g_object_unref (G_OBJECT(model));
		}
	} else {
		if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), schema_name_n, error))
			return FALSE;
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_SCHEMA_NAMED],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_schemata,
								      error);
		if (model == NULL)
			retval = FALSE;
		else {
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify (store, context->table_name, model,
							"schema_name=##name::string",
							error,
							"name", schema_name_n, NULL);
			g_object_unref (G_OBJECT(model));
		}
	}
	
	return retval;
}

gboolean
_gda_mysql_meta__tables_views (G_GNUC_UNUSED GdaServerProvider  *prov,
			       GdaConnection      *cnc, 
			       GdaMetaStore       *store,
			       GdaMetaContext     *context,
			       GError            **error)
{	
	GdaDataModel *model_tables, *model_views;
	gboolean retval;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	/* Copy contents, just because we need to modify @context->table_name */
	GdaMetaContext copy = *context;

	model_tables = gda_connection_statement_execute_select_full (cnc,
								     internal_stmt[I_STMT_TABLES_ALL],
								     NULL,
								     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								     _col_types_tables, error);
	if (model_tables == NULL)
		retval = FALSE;
	else {
		copy.table_name = "_tables";
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, &copy, model_tables, error);
		g_object_unref (G_OBJECT(model_tables));
	}

	model_views = gda_connection_statement_execute_select_full (cnc,
								    internal_stmt[I_STMT_VIEWS_ALL],
								    NULL, 
								    GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								    _col_types_views, error);
	if (model_views == NULL)
		retval = FALSE;
	else {
		copy.table_name = "_views";
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, &copy, model_views, error);
		g_object_unref (G_OBJECT(model_views));
	}
	
	return retval;
}

gboolean
_gda_mysql_meta_tables_views (G_GNUC_UNUSED GdaServerProvider  *prov,
			      GdaConnection      *cnc, 
			      GdaMetaStore       *store,
			      GdaMetaContext     *context,
			      GError            **error,
			      G_GNUC_UNUSED const GValue       *table_catalog,
			      const GValue       *table_schema, 
			      const GValue       *table_name_n)
{
	GdaDataModel *model_tables, *model_views;
	gboolean retval;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}


	/* Copy contents, just because we need to modify @context->table_name */
	GdaMetaContext copy = *context;

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (!table_name_n) {
		model_tables = gda_connection_statement_execute_select_full (cnc,
									     internal_stmt[I_STMT_TABLES],
									     i_set, 
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									     _col_types_tables,
									     error);
		if (model_tables == NULL)
			retval = FALSE;
		else {
			copy.table_name = "_tables";
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify_with_context (store, &copy, model_tables, error);
			g_object_unref (G_OBJECT (model_tables));
		}

		if (!retval)
			return FALSE;

		model_views = gda_connection_statement_execute_select_full (cnc,
									    internal_stmt[I_STMT_VIEWS],
									    i_set, 
									    GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									    _col_types_views,
									    error);
		if (model_views == NULL)
			retval = FALSE;
		else {
			copy.table_name = "_views";
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify_with_context (store, &copy, model_views, error);
			g_object_unref (G_OBJECT (model_views));
		}

	} else {
		if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name_n, error))
			return FALSE;
		model_tables = gda_connection_statement_execute_select_full (cnc,
									     internal_stmt[I_STMT_TABLE_NAMED],
									     i_set, 
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									     _col_types_tables,
									     error);
		if (model_tables == NULL)
			retval = FALSE;
		else {
			copy.table_name = "_tables";
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify_with_context (store, &copy, model_tables, error);
			g_object_unref (G_OBJECT(model_tables));
		}

		if (!retval)
			return FALSE;
		model_views = gda_connection_statement_execute_select_full (cnc,
									    internal_stmt[I_STMT_VIEW_NAMED],
									    i_set, 
									    GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									    _col_types_views,
									    error);
		if (model_views == NULL)
			retval = FALSE;
		else {
			copy.table_name = "_views";
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify_with_context (store, &copy, model_views, error);
			g_object_unref (G_OBJECT(model_views));
		}

	}
	
	return retval;
}

/*
 * map_mysql_type_to_gda:
 * @value: a #GValue string.
 *
 * It maps a mysql type to a gda type.  This means that when a
 * mysql type is given, it will return its mapped gda type.
 *
 * Returns a newly created GValue string.
 */
static inline GValue *
map_mysql_type_to_gda (const GValue *value, const gchar *vlength)
{
	const gchar *string = g_value_get_string (value);
	GValue *newvalue;
	const gchar *newstring;

	if (!strcmp (string, "bool"))
		newstring = "gboolean";
	else if (!strcmp (string, "blob"))
		newstring = "GdaBinary";
	else if (!strcmp (string, "bigint"))
		newstring = "gint64";
	else if (!strcmp (string, "bigint unsigned"))
		newstring = "guint64";
	else if (!strcmp (string, "char")) {
		if (vlength && (G_VALUE_TYPE ((GValue*)vlength) == G_TYPE_INT) &&
		    (g_value_get_int ((GValue*)vlength) > 1))
			newstring = "gchararray";
		else
			newstring = "gchar";
	}
	else if (!strcmp (string, "date"))
		newstring = "GDate";
	else if (!strcmp (string, "datetime"))
		newstring = "GDateTime";
	else if (!strcmp (string, "decimal"))
		newstring = "GdaNumeric";
	else if (!strcmp (string, "double"))
		newstring = "gdouble";
	else if (!strcmp (string, "double unsigned"))
		newstring = "double";
	else if (!strcmp (string, "enum"))
		newstring = "gchararray";
	else if (!strcmp (string, "float"))
		newstring = "gfloat";
	else if (!strcmp (string, "float unsigned"))
		newstring = "gfloat";
	else if (!strcmp (string, "int"))
		newstring = "int";
	else if (!strcmp (string, "unsigned int"))
		newstring = "guint";
	else if (!strcmp (string, "long"))
		newstring = "glong";
	else if (!strcmp (string, "unsigned long"))
		newstring = "gulong";
	else if (!strcmp (string, "longblob"))
		newstring = "GdaBinary";
	else if (!strcmp (string, "longtext"))
		newstring = "GdaBinary";
	else if (!strcmp (string, "mediumint"))
		newstring = "gint";
	else if (!strcmp (string, "mediumint unsigned"))
		newstring = "guint";
	else if (!strcmp (string, "mediumblob"))
		newstring = "GdaBinary";
	else if (!strcmp (string, "mediumtext"))
		newstring = "GdaBinary";
	else if (!strcmp (string, "set"))
		newstring = "gchararray";
	else if (!strcmp (string, "smallint"))
		newstring = "gshort";
	else if (!strcmp (string, "smallint unsigned"))
		newstring = "gushort";
	else if (!strcmp (string, "text"))
		newstring = "GdaText";
	else if (!strcmp (string, "tinyint"))
		newstring = "gchar";
	else if (!strcmp (string, "tinyint unsigned"))
		newstring = "guchar";
	else if (!strcmp (string, "tinyblob"))
		newstring = "GdaBinary";
	else if (!strcmp (string, "time"))
		newstring = "GdaTime";
	else if (!strcmp (string, "timestamp"))
		newstring = "GDateTime";
	else if (!strcmp (string, "varchar"))
		newstring = "gchararray";
	else if (!strcmp (string, "year"))
		newstring = "gint";
	else
		newstring = "gchararray";

	g_value_set_string (newvalue = gda_value_new (G_TYPE_STRING),
			    newstring);

	return newvalue;
}

gboolean
_gda_mysql_meta__columns (G_GNUC_UNUSED GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error)
{
	GdaDataModel *model, *proxy;
	gboolean retval = TRUE;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	/* Use a prepared statement for the "base" model. */
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_COLUMNS_ALL], NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_columns, error);
	if (model == NULL)
		retval = FALSE;
	else {
		proxy = (GdaDataModel *) gda_data_proxy_new (model);
		gda_data_proxy_set_sample_size ((GdaDataProxy *) proxy, 0);
		gint n_rows = gda_data_model_get_n_rows (model);
		gint i;
		for (i = 0; i < n_rows; ++i) {
			const GValue *value1, *value2 = NULL;
			value1 = gda_data_model_get_value_at (model, 7, i, error);
			if (value1)
				value2 = gda_data_model_get_value_at (model, 10, i, error);
			if (!value1 || !value2) {
				retval = FALSE;
				break;
			}

			GValue *newvalue = map_mysql_type_to_gda (value1, (const char*)(value2));

			retval = gda_data_model_set_value_at (GDA_DATA_MODEL(proxy), 9, i, newvalue, error);
			gda_value_free (newvalue);
			if (!retval)
				break;
		}

		if (retval) {
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify_with_context (store, context, proxy, error);
		}

		g_object_unref (G_OBJECT(proxy));
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_columns (G_GNUC_UNUSED GdaServerProvider  *prov,
			 GdaConnection      *cnc, 
			 GdaMetaStore       *store,
			 GdaMetaContext     *context,
			 GError            **error,
			 G_GNUC_UNUSED const GValue       *table_catalog,
			 const GValue       *table_schema, 
			 const GValue       *table_name)
{
	GdaDataModel *model, *proxy;
	gboolean retval;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	/* Use a prepared statement for the "base" model. */
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_COLUMNS_OF_TABLE],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_columns, error);
	if (model == NULL)
		retval = FALSE;
	else {
		proxy = (GdaDataModel *) gda_data_proxy_new (model);
		gda_data_proxy_set_sample_size ((GdaDataProxy *) proxy, 0);
		gint n_rows = gda_data_model_get_n_rows (model);
		gint i;
		for (i = 0; i < n_rows; ++i) {
			const GValue *value1, *value2 = NULL;
			value1 = gda_data_model_get_value_at (model, 7, i, error);
			if (value1)
				value2 = gda_data_model_get_value_at (model, 10, i, error);
			if (!value1 || !value2) {
				retval = FALSE;
				break;
			}

			GValue *newvalue = map_mysql_type_to_gda (value1, (const char*)value2);

			retval = gda_data_model_set_value_at (GDA_DATA_MODEL(proxy), 9, i, newvalue, error);
			gda_value_free (newvalue);
			if (!retval)
				break;
		}

		if (retval) {
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify (store, context->table_name, proxy,
							"table_schema=##schema::string AND table_name=##name::string",
							error,
							"schema", table_schema, "name", table_name, NULL);
		}
		g_object_unref (G_OBJECT(proxy));
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta__view_cols (G_GNUC_UNUSED GdaServerProvider  *prov,
			    GdaConnection      *cnc, 
			    GdaMetaStore       *store,
			    GdaMetaContext     *context,
			    GError            **error)
{
	GdaDataModel *model;
	gboolean retval;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_VIEWS_COLUMNS_ALL],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_view_column_usage, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_view_cols (G_GNUC_UNUSED GdaServerProvider  *prov,
			   GdaConnection      *cnc, 
			   GdaMetaStore       *store,
			   GdaMetaContext     *context,
			   GError            **error,
			   G_GNUC_UNUSED const GValue       *view_catalog,
			   const GValue       *view_schema, 
			   const GValue       *view_name)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), view_schema, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), view_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_VIEWS_COLUMNS],
							      i_set, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_view_column_usage, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

gboolean
_gda_mysql_meta__constraints_tab (G_GNUC_UNUSED GdaServerProvider  *prov,
				  GdaConnection      *cnc, 
				  GdaMetaStore       *store,
				  GdaMetaContext     *context,
				  GError            **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_TABLES_CONSTRAINTS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_table_constraints, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_constraints_tab (G_GNUC_UNUSED GdaServerProvider  *prov,
				 GdaConnection      *cnc, 
				 GdaMetaStore       *store,
				 GdaMetaContext     *context,
				 GError            **error, 
				 G_GNUC_UNUSED const GValue       *table_catalog,
				 const GValue       *table_schema, 
				 const GValue       *table_name,
				 const GValue       *constraint_name_n)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (!constraint_name_n) {
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_TABLES_CONSTRAINTS],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_table_constraints, error);
		if (model == NULL)
			retval = FALSE;
		else {
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify (store, context->table_name, model,
							"table_schema = ##schema::string AND table_name = ##name::string",
							error,
							"schema", table_schema, "name", table_name, NULL);
			g_object_unref (G_OBJECT(model));
		}
	} else {
		if (!gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name_n, error))
			return FALSE;
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_TABLES_CONSTRAINTS_NAMED],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_table_constraints, error);
		if (model == NULL)
			retval = FALSE;
		else {
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify (store, context->table_name, model,
							"table_schema=##schema::string AND table_name=##name::string AND constraint_name=##name2::string",
							error,
							"schema", table_schema, "name", table_name, "name2", constraint_name_n, NULL);
			g_object_unref (G_OBJECT(model));
		}
	}

	return retval;
}

gboolean
_gda_mysql_meta__constraints_ref (G_GNUC_UNUSED GdaServerProvider  *prov,
				  GdaConnection      *cnc, 
				  GdaMetaStore       *store,
				  GdaMetaContext     *context,
				  GError            **error)
{
	GdaMysqlReuseable *rdata;

	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	g_return_val_if_fail (rdata, FALSE);

	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long >= 50110) {
		GdaDataModel *model;
		gboolean retval;
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_REF_CONSTRAINTS_ALL],
								      NULL,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_referential_constraints, error);
		if (model == NULL)
			retval = FALSE;
		else {
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify_with_context (store, context, model, error);
			g_object_unref (G_OBJECT(model));
		}

		return retval;
	}
	else {
		// TO_IMPLEMENT;
		return TRUE;
	}
}

gboolean
_gda_mysql_meta_constraints_ref (G_GNUC_UNUSED GdaServerProvider  *prov,
				 GdaConnection      *cnc, 
				 GdaMetaStore       *store,
				 GdaMetaContext     *context,
				 GError            **error,
				 G_GNUC_UNUSED const GValue       *table_catalog,
				 const GValue       *table_schema,
				 const GValue       *table_name, 
				 const GValue       *constraint_name)
{
	GdaMysqlReuseable *rdata;

	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	g_return_val_if_fail (rdata, FALSE);

	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long >= 50110) {
		GdaDataModel *model;
		gboolean retval;
		
		/* Use a prepared statement for the "base" model. */
		if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
			return FALSE;
		if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
			return FALSE;
		if (!gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name, error))
			return FALSE;
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_REF_CONSTRAINTS],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_referential_constraints, error);
		if (model == NULL)
			retval = FALSE;
		else {
			gda_meta_store_set_reserved_keywords_func (store,
								   _gda_mysql_reuseable_get_reserved_keywords_func
								   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify (store, context->table_name, model,
							"table_schema=##schema::string AND table_name=##name::string AND constraint_name=##name2::string",
							error,
							"schema", table_schema, "name", table_name, "name2", constraint_name, NULL);
			g_object_unref (G_OBJECT(model));
			
		}
		
		return retval;
	}
	else {
		// TO_IMPLEMENT;
		return TRUE;
	}
}

gboolean
_gda_mysql_meta__key_columns (G_GNUC_UNUSED GdaServerProvider  *prov,
			      GdaConnection      *cnc, 
			      GdaMetaStore       *store,
			      GdaMetaContext     *context,
			      GError            **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_KEY_COLUMN_USAGE_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_key_column_usage, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_key_columns (G_GNUC_UNUSED GdaServerProvider  *prov,
			     GdaConnection      *cnc, 
			     GdaMetaStore       *store,
			     GdaMetaContext     *context,
			     GError            **error,
			     G_GNUC_UNUSED const GValue       *table_catalog,
			     const GValue       *table_schema, 
			     const GValue       *table_name,
			     const GValue       *constraint_name)
{
	GdaDataModel *model;
	gboolean retval;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	/* Use a prepared statement for the "base" model. */
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_KEY_COLUMN_USAGE],
							      i_set, GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_key_column_usage,
							      error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, context->table_name, model,
						"table_schema=##schema::string AND table_name=##name::string AND constraint_name=##name2::string",
						error,
						"schema", table_schema, "name", table_name, "name2", constraint_name, NULL);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

gboolean
_gda_mysql_meta__check_columns (G_GNUC_UNUSED GdaServerProvider  *prov,
				G_GNUC_UNUSED GdaConnection      *cnc,
				G_GNUC_UNUSED GdaMetaStore       *store,
				G_GNUC_UNUSED GdaMetaContext     *context,
				G_GNUC_UNUSED GError            **error)
{
	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_check_columns (G_GNUC_UNUSED GdaServerProvider  *prov,
			       G_GNUC_UNUSED GdaConnection      *cnc,
			       G_GNUC_UNUSED GdaMetaStore       *store,
			       G_GNUC_UNUSED GdaMetaContext     *context,
			       G_GNUC_UNUSED GError            **error,
			       G_GNUC_UNUSED const GValue       *table_catalog,
			       G_GNUC_UNUSED const GValue       *table_schema,
			       G_GNUC_UNUSED const GValue       *table_name,
			       G_GNUC_UNUSED const GValue       *constraint_name)
{
	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__triggers (G_GNUC_UNUSED GdaServerProvider  *prov,
			   GdaConnection      *cnc, 
			   GdaMetaStore       *store,
			   GdaMetaContext     *context,
			   GError            **error)
{
	GdaDataModel *model;
	gboolean retval;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_TRIGGERS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_triggers, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_triggers (G_GNUC_UNUSED GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error,
			  G_GNUC_UNUSED const GValue       *table_catalog,
			  const GValue       *table_schema, 
			  const GValue       *table_name)
{
	GdaDataModel *model;
	gboolean retval;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_TRIGGERS],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_triggers, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

gboolean
_gda_mysql_meta__routines (G_GNUC_UNUSED GdaServerProvider  *prov,
			   GdaConnection      *cnc, 
			   GdaMetaStore       *store,
			   GdaMetaContext     *context,
			   GError            **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_ROUTINES_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_routines, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_routines (G_GNUC_UNUSED GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error,
			  G_GNUC_UNUSED const GValue       *routine_catalog,
			  const GValue       *routine_schema, 
			  const GValue       *routine_name_n)
{
	GdaDataModel *model;
	gboolean retval;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), routine_schema, error))
		return FALSE;
	if (routine_name_n != NULL) {
 		if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), routine_name_n, error))
			return FALSE;
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_ROUTINES_ONE],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_routines, error);
	}
	else
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_ROUTINES],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_routines, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

gboolean
_gda_mysql_meta__routine_col (G_GNUC_UNUSED GdaServerProvider  *prov,
			      G_GNUC_UNUSED GdaConnection      *cnc,
			      G_GNUC_UNUSED GdaMetaStore       *store,
			      G_GNUC_UNUSED GdaMetaContext     *context,
			      G_GNUC_UNUSED GError            **error)
{
	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_routine_col (G_GNUC_UNUSED GdaServerProvider  *prov,
			     G_GNUC_UNUSED GdaConnection      *cnc,
			     G_GNUC_UNUSED GdaMetaStore       *store,
			     G_GNUC_UNUSED GdaMetaContext     *context,
			     G_GNUC_UNUSED GError            **error,
			     G_GNUC_UNUSED const GValue       *rout_catalog,
			     G_GNUC_UNUSED const GValue       *rout_schema,
			     G_GNUC_UNUSED const GValue       *rout_name,
			     G_GNUC_UNUSED const GValue       *col_name,
			     G_GNUC_UNUSED const GValue       *ordinal_position)
{
	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__routine_par (G_GNUC_UNUSED GdaServerProvider  *prov,
			      G_GNUC_UNUSED GdaConnection      *cnc,
			      G_GNUC_UNUSED GdaMetaStore       *store,
			      G_GNUC_UNUSED GdaMetaContext     *context,
			      G_GNUC_UNUSED GError            **error)
{
	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_routine_par (G_GNUC_UNUSED GdaServerProvider  *prov,
			     G_GNUC_UNUSED GdaConnection      *cnc,
			     G_GNUC_UNUSED GdaMetaStore       *store,
			     G_GNUC_UNUSED GdaMetaContext     *context,
			     G_GNUC_UNUSED GError            **error,
			     G_GNUC_UNUSED const GValue       *rout_catalog,
			     G_GNUC_UNUSED const GValue       *rout_schema,
			     G_GNUC_UNUSED const GValue       *rout_name)
{
	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	/* Check correct mysql server version. */
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_INDEXES_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_table_indexes, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     G_GNUC_UNUSED const GValue *table_catalog, const GValue *table_schema,
			     const GValue *table_name, const GValue *index_name_n)
{
	GdaDataModel *model;
	gboolean retval;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (index_name_n) {
		if (!gda_holder_set_value (gda_set_get_holder (i_set, "name2"), index_name_n, error))
			return FALSE;
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_INDEXES_ONE],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_table_indexes, error);
	}
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_INDEXES_TABLE],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_table_indexes, error);

	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

gboolean
_gda_mysql_meta__index_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	/* Check correct mysql server version. */
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_INDEX_COLUMNS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_index_column_usage, error);
	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_index_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    G_GNUC_UNUSED const GValue *table_catalog, const GValue *table_schema,
			    const GValue *table_name, const GValue *index_name)
{
	GdaDataModel *model;
	gboolean retval;
	/* Check correct mysql server version. */
	GdaMysqlReuseable *rdata;
	rdata = GDA_MYSQL_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_long == 0) && ! _gda_mysql_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "name2"), index_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_INDEX_COLUMNS_NAMED],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_index_column_usage, error);

	if (model == NULL)
		retval = FALSE;
	else {
		gda_meta_store_set_reserved_keywords_func (store,
							   _gda_mysql_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

