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

#include <string.h>
#include "gda-firebird.h"
#include "gda-firebird-meta.h"
#include "gda-firebird-provider.h"
#include <libgda/gda-meta-store.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/providers-support/gda-meta-column-types.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-debug-macros.h>

//static gboolean append_a_row (GdaDataModel *to_model, GError **error, gint nb, ...);

/*
 * predefined statements' IDs
 */
typedef enum {
	I_STMT_CATALOG,

	I_STMT_SCHEMAS,

	I_STMT_SCHEMAS_ALL,

	I_STMT_SCHEMA_NAMED,

	I_STMT_TABLES_ALL,

	I_STMT_TABLES,

	I_STMT_TABLE_NAMED,

	I_STMT_VIEWS_ALL,
	
	I_STMT_VIEWS,

	I_STMT_VIEW_NAMED,

	I_STMT_COLUMNS_OF_TABLE,

	I_STMT_COLUMNS_ALL,

	I_STMT_CHARACTER_SETS,

	I_STMT_CHARACTER_SETS_ALL,
	
	I_STMT_VIEWS_COLUMNS ,
	I_STMT_VIEWS_COLUMNS_ALL,

	I_STMT_TABLES_CONSTRAINTS,
	I_STMT_TABLES_CONSTRAINTS_ALL,
	I_STMT_TABLES_CONSTRAINTS_NAMED,
	I_STMT_REF_CONSTRAINTS,
	I_STMT_REF_CONSTRAINTS_ALL,

	I_STMT_KEY_COLUMN_USAGE,
	I_STMT_KEY_COLUMN_USAGE_ALL,

	//I_STMT_TRIGGERS,
	//I_STMT_TRIGGERS_ALL,
	
	//I_STMT_ROUTINES,
	//I_STMT_ROUTINES_ALL,
	//I_STMT_ROUTINES_ONE,
	//I_STMT_ROUTINES_PAR_ALL,
	//I_STMT_ROUTINES_PAR,
	
	I_STMT_INDEXES_ALL,
	I_STMT_INDEXES_TABLE,
	I_STMT_INDEXES_ONE,
	I_STMT_INDEX_COLUMNS_ALL,
	I_STMT_INDEX_COLUMNS_NAMED

} InternalStatementItem;


/*
* predefined statements' SQL
*/
#define CATALOG_NAME "'firebird'"
#define SCHEMA_NAME "'SYS'"

static gchar *internal_sql[] = {
	/* I_STMT_CATALOG */
	"SELECT " CATALOG_NAME " FROM RDB$DATABASE",

	/* I_STMT_SCHEMAS */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, NULL, 1 AS schema_internal FROM RDB$DATABASE",

	/* I_STMT_SCHEMAS_ALL */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, NULL, 1 AS schema_internal FROM RDB$DATABASE",

	/* I_STMT_SCHEMA_NAMED */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, NULL, 1 AS schema_internal FROM RDB$DATABASE",

	/* I_STMT_TABLES_ALL */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, (RDB$RELATION_NAME), 'BASE TABLE', 1, RDB$RELATION_NAME, TRIM(RDB$RELATION_NAME) AS short_name, TRIM(RDB$OWNER_NAME) || '.' || (TRIM(RDB$RELATION_NAME)) AS table_full_name, TRIM(RDB$OWNER_NAME) FROM RDB$RELATIONS WHERE (RDB$SYSTEM_FLAG = 0) AND (RDB$RELATION_NAME NOT IN (SELECT RDB$VIEW_NAME FROM RDB$VIEW_RELATIONS))",

	/* I_STMT_TABLES */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, (RDB$RELATION_NAME), 'BASE TABLE', 1, RDB$RELATION_NAME, TRIM(RDB$RELATION_NAME) AS short_name, TRIM(RDB$OWNER_NAME) || '.' || (TRIM(RDB$RELATION_NAME)) AS table_full_name, TRIM(RDB$OWNER_NAME) FROM RDB$RELATIONS WHERE (RDB$SYSTEM_FLAG = 0) AND (RDB$RELATION_NAME NOT IN (SELECT RDB$VIEW_NAME FROM RDB$VIEW_RELATIONS))",

	/* I_STMT_TABLE_NAMED */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, (RDB$RELATION_NAME), 'BASE TABLE', 1, RDB$RELATION_NAME, TRIM(RDB$RELATION_NAME) AS short_name, TRIM(RDB$OWNER_NAME) || '.' || (TRIM(RDB$RELATION_NAME)) AS table_full_name, TRIM(RDB$OWNER_NAME) FROM RDB$RELATIONS WHERE (RDB$SYSTEM_FLAG = 0) AND (RDB$RELATION_NAME NOT IN (SELECT RDB$VIEW_NAME FROM RDB$VIEW_RELATIONS)) AND (TRIM(RDB$RELATION_NAME)) =  ##tblname::string",

	/* I_STMT_VIEWS_ALL */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, (RDB$RELATION_NAME), NULL, NULL, 0 FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0 AND RDB$RELATION_NAME IN (SELECT RDB$VIEW_NAME FROM RDB$VIEW_RELATIONS)",
	
	/* I_STMT_VIEWS */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, (RDB$RELATION_NAME), NULL, NULL, 0 FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0 AND RDB$RELATION_NAME IN (SELECT RDB$VIEW_NAME FROM RDB$VIEW_RELATIONS)",

	/* I_STMT_VIEW_NAMED */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, ((RDB$RELATION_NAME)), NULL, NULL, 0, 0 FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0 AND RDB$RELATION_NAME IN (SELECT RDB$VIEW_NAME FROM RDB$VIEW_RELATIONS) AND (TRIM(RDB$RELATION_NAME)) =  ##tblname::string",

	/* I_STMT_COLUMNS_OF_TABLE */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, (RF.RDB$RELATION_NAME), (RF.RDB$FIELD_NAME) AS column_name, RF.RDB$FIELD_POSITION AS ordinal_position, RF.RDB$DEFAULT_VALUE AS column_default, RF.RDB$NULL_FLAG AS is_nullable, LOWER(TRIM(T.RDB$TYPE_NAME)) AS data_type, NULL, 'gchararray', F.RDB$CHARACTER_LENGTH AS character_maximum_length, F.RDB$FIELD_LENGTH AS character_octet_length, F.RDB$FIELD_PRECISION AS numeric_precision, F.RDB$FIELD_SCALE AS numeric_scale, NULL, NULL AS CHAR_SET_CAT, NULL AS CHAR_SET_SCHEMA, NULL AS CHAR_SET_NAME, NULL AS COLLATION_CAT, NULL AS COLLATION_NAME, NULL, NULL AS Extra, RF.RDB$UPDATE_FLAG as is_updatable, '' as column_comment FROM RDB$RELATION_FIELDS AS RF INNER JOIN RDB$RELATIONS R ON R.RDB$RELATION_NAME = RF.RDB$RELATION_NAME INNER JOIN RDB$FIELDS AS F ON ((F.RDB$FIELD_NAME = RF.RDB$FIELD_SOURCE))  INNER JOIN RDB$TYPES AS T ON (((T.RDB$TYPE = F.RDB$FIELD_TYPE) AND (T.RDB$FIELD_NAME = 'RDB$FIELD_TYPE')))  WHERE RF.RDB$SYSTEM_FLAG = 0  AND R.RDB$RELATION_NAME NOT IN (SELECT RDB$VIEW_NAME FROM RDB$VIEW_RELATIONS)  AND (RF.RDB$RELATION_NAME) = ##tblname::string  ORDER BY RF.RDB$RELATION_NAME ASC, RDB$FIELD_POSITION ASC",

	/* I_STMT_COLUMNS_ALL */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS schema_name, (RF.RDB$RELATION_NAME), (RF.RDB$FIELD_NAME) AS column_name, RF.RDB$FIELD_POSITION AS ordinal_position, RF.RDB$DEFAULT_VALUE AS column_default, RF.RDB$NULL_FLAG AS is_nullable, LOWER(TRIM(T.RDB$TYPE_NAME)) AS data_type, NULL, 'gchararray', F.RDB$CHARACTER_LENGTH AS character_maximum_length, F.RDB$FIELD_LENGTH AS character_octet_length, F.RDB$FIELD_PRECISION AS numeric_precision, F.RDB$FIELD_SCALE AS numeric_scale, NULL, NULL AS CHAR_SET_CAT, NULL AS CHAR_SET_SCHEMA, NULL AS CHAR_SET_NAME, NULL AS COLLATION_CAT, NULL AS COLLATION_NAME, NULL, NULL AS Extra, RF.RDB$UPDATE_FLAG as is_updatable, '' as column_comment FROM RDB$RELATION_FIELDS AS RF INNER JOIN RDB$RELATIONS R ON R.RDB$RELATION_NAME = RF.RDB$RELATION_NAME INNER JOIN RDB$FIELDS AS F ON ((F.RDB$FIELD_NAME = RF.RDB$FIELD_SOURCE))  INNER JOIN RDB$TYPES AS T ON (((T.RDB$TYPE = F.RDB$FIELD_TYPE) AND (T.RDB$FIELD_NAME = 'RDB$FIELD_TYPE'))) WHERE RF.RDB$SYSTEM_FLAG = 0 AND R.RDB$RELATION_NAME NOT IN (SELECT RDB$VIEW_NAME FROM RDB$VIEW_RELATIONS) ORDER BY RF.RDB$RELATION_NAME ASC, RDB$FIELD_POSITION ASC",

	/* I_STMT_CHARACTER_SETS */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS character_set_schema, RDB$CHARACTER_SET_NAME, RDB$DEFAULT_COLLATE_NAME, RDB$DEFAULT_COLLATE_NAME, RDB$DEFAULT_COLLATE_NAME, NULL, RDB$CHARACTER_SET_NAME, RDB$CHARACTER_SET_NAME FROM RDB$CHARACTER_SETS WHERE RDB$CHARACTER_SET_NAME = ##char_set_name::string",

	/* I_STMT_CHARACTER_SETS_ALL */
	"SELECT " CATALOG_NAME " AS catalog_name, " SCHEMA_NAME " AS character_set_schema, RDB$CHARACTER_SET_NAME, RDB$DEFAULT_COLLATE_NAME, RDB$DEFAULT_COLLATE_NAME, RDB$DEFAULT_COLLATE_NAME, NULL, RDB$CHARACTER_SET_NAME, RDB$CHARACTER_SET_NAME FROM RDB$CHARACTER_SETS",
	
	/* I_STMT_VIEWS_COLUMNS */
	"SELECT " CATALOG_NAME ", " SCHEMA_NAME ", (RDB$RELATION_NAME), " CATALOG_NAME ", " SCHEMA_NAME ", (RDB$RELATION_NAME), RDB$FIELD_NAME FROM RDB$RELATION_FIELDS WHERE RDB$SYSTEM_FLAG = 0 AND RDB$RELATION_NAME IN (SELECT RDB$VIEW_NAME FROM RDB$VIEW_RELATIONS) AND TRIM(RDB$RELATION_NAME) = ##tblname::string",

	/* I_STMT_VIEWS_COLUMNS_ALL */
	"SELECT " CATALOG_NAME ", " SCHEMA_NAME ", (RDB$RELATION_NAME), " CATALOG_NAME ", " SCHEMA_NAME ", (RDB$RELATION_NAME), RDB$FIELD_NAME FROM RDB$RELATION_FIELDS WHERE RDB$SYSTEM_FLAG = 0 AND RDB$RELATION_NAME IN (SELECT RDB$VIEW_NAME FROM RDB$VIEW_RELATIONS)",

	/* I_STMT_TABLES_CONSTRAINTS */
	"SELECT " CATALOG_NAME ", " SCHEMA_NAME ", RDB$CONSTRAINT_NAME, " CATALOG_NAME ", " SCHEMA_NAME ", TRIM(RDB$RELATION_NAME), RDB$CONSTRAINT_TYPE, NULL, CASE WHEN RDB$DEFERRABLE = 'YES' THEN 1 ELSE 0 END, CASE WHEN RDB$INITIALLY_DEFERRED = 'YES' THEN 1 ELSE 0 END FROM RDB$RELATION_CONSTRAINTS WHERE TRIM(RDB$RELATION_NAME) = ##tblname::string",

	/* I_STMT_TABLES_CONSTRAINTS_ALL */
	"SELECT " CATALOG_NAME ", " SCHEMA_NAME ", RDB$CONSTRAINT_NAME, " CATALOG_NAME ", " SCHEMA_NAME ", RDB$RELATION_NAME, RDB$CONSTRAINT_TYPE, NULL, CASE WHEN RDB$DEFERRABLE = 'YES' THEN 1 ELSE 0 END, CASE WHEN RDB$INITIALLY_DEFERRED = 'YES' THEN 1 ELSE 0 END FROM RDB$RELATION_CONSTRAINTS",

	/* I_STMT_TABLES_CONSTRAINTS_NAMED */
	"SELECT " CATALOG_NAME ", " SCHEMA_NAME ", RDB$CONSTRAINT_NAME, " CATALOG_NAME ", " SCHEMA_NAME ", RDB$RELATION_NAME, RDB$CONSTRAINT_TYPE, NULL, CASE WHEN RDB$DEFERRABLE = 'YES' THEN 1 ELSE 0 END, CASE WHEN RDB$INITIALLY_DEFERRED = 'YES' THEN 1 ELSE 0 END FROM RDB$RELATION_CONSTRAINTS WHERE TRIM(RDB$RELATION_NAME) = ##tblname::string AND RDB$CONSTRAINT_NAME = ##constraint_name::string",

	/* I_STMT_REF_CONSTRAINTS */
	"SELECT " CATALOG_NAME ", " SCHEMA_NAME ",  TRIM(R1.RDB$RELATION_NAME), C.RDB$CONSTRAINT_NAME,  " CATALOG_NAME ", " SCHEMA_NAME ", TRIM(R2.RDB$RELATION_NAME), C.RDB$CONST_NAME_UQ, RDB$MATCH_OPTION, RDB$UPDATE_RULE, RDB$DELETE_RULE FROM RDB$REF_CONSTRAINTS C INNER JOIN RDB$RELATION_CONSTRAINTS R1 ON R1.RDB$CONSTRAINT_NAME = C.RDB$CONSTRAINT_NAME INNER JOIN RDB$RELATION_CONSTRAINTS R2 ON R2.RDB$CONSTRAINT_NAME = C.RDB$CONST_NAME_UQ WHERE TRIM(R1.RDB$RELATION_NAME) = ##tblname::string AND C.RDB$CONSTRAINT_NAME = ##constraint_name::string",

	/* I_STMT_REF_CONSTRAINTS_ALL */
	"SELECT " CATALOG_NAME ", " SCHEMA_NAME ",  TRIM(R1.RDB$RELATION_NAME), C.RDB$CONSTRAINT_NAME,  " CATALOG_NAME ", " SCHEMA_NAME ", TRIM(R2.RDB$RELATION_NAME), C.RDB$CONST_NAME_UQ, RDB$MATCH_OPTION, RDB$UPDATE_RULE, RDB$DELETE_RULE FROM RDB$REF_CONSTRAINTS C INNER JOIN RDB$RELATION_CONSTRAINTS R1 ON R1.RDB$CONSTRAINT_NAME = C.RDB$CONSTRAINT_NAME INNER JOIN RDB$RELATION_CONSTRAINTS R2 ON R2.RDB$CONSTRAINT_NAME = C.RDB$CONST_NAME_UQ",

	/* I_STMT_KEY_COLUMN_USAGE */
	"SELECT " CATALOG_NAME ", " SCHEMA_NAME ", TRIM(r.RDB$RELATION_NAME), r.rdb$constraint_name, i.rdb$field_name, i.RDB$FIELD_POSITION FROM rdb$relation_constraints r INNER JOIN rdb$index_segments i ON r.rdb$index_name = i.rdb$index_name WHERE (r.rdb$constraint_type='PRIMARY KEY') AND TRIM(r.RDB$RELATION_NAME) = ##tblname::string AND r.rdb$constraint_name = ##constraint_name::string",

	/* I_STMT_KEY_COLUMN_USAGE_ALL */
	"SELECT " CATALOG_NAME ", " SCHEMA_NAME ", TRIM(r.RDB$RELATION_NAME), r.rdb$constraint_name, i.rdb$field_name, i.RDB$FIELD_POSITION FROM rdb$relation_constraints r INNER JOIN rdb$index_segments i ON r.rdb$index_name = i.rdb$index_name WHERE (r.rdb$constraint_type='PRIMARY KEY') ORDER BY r.rdb$constraint_name, i.rdb$field_position",

	/* I_STMT_TRIGGERS */
	//"SELECT " CATALOG_NAME " AS trigger_catalog, trigger_schema, trigger_name, event_manipulation, " CATALOG_NAME " AS event_object_catalog, event_object_schema, event_object_table, action_statement, action_orientation, action_timing, NULL, trigger_name, trigger_name FROM INFORMATION_SCHEMA.triggers WHERE trigger_schema =  ##schema::string AND trigger_name = ##name::string",

	/* I_STMT_TRIGGERS_ALL */
	//"SELECT " CATALOG_NAME " AS trigger_catalog, trigger_schema, trigger_name, event_manipulation, " CATALOG_NAME " AS event_object_catalog, event_object_schema, event_object_table, action_statement, action_orientation, action_timing, NULL, trigger_name, trigger_name FROM INFORMATION_SCHEMA.triggers",

	/* I_STMT_ROUTINES_ALL */
	//"SELECT " CATALOG_NAME " AS specific_catalog, routine_schema AS specific_schema, routine_name AS specific_name, " CATALOG_NAME " AS routine_catalog, routine_schema, routine_name, routine_type, dtd_identifier AS return_type, FALSE AS returns_set, 0 AS nb_args, routine_body, routine_definition, external_name, external_language, parameter_style, CASE is_deterministic WHEN 'YES' THEN TRUE ELSE FALSE END AS is_deterministic, sql_data_access, FALSE AS is_null_call, routine_comment, routine_name, routine_name, definer FROM INFORMATION_SCHEMA.routines",

	/* I_STMT_ROUTINES */
	//"SELECT " CATALOG_NAME " AS specific_catalog, routine_schema AS specific_schema, routine_name AS specific_name, " CATALOG_NAME " AS routine_catalog, routine_schema, routine_name, routine_type, dtd_identifier AS return_type, FALSE AS returns_set, 0 AS nb_args, routine_body, routine_definition, external_name, external_language, parameter_style, CASE is_deterministic WHEN 'YES' THEN TRUE ELSE FALSE END AS is_deterministic, sql_data_access, FALSE AS is_null_call, routine_comment, routine_name, routine_name, definer FROM INFORMATION_SCHEMA.routines WHERE routine_schema =  ##schema::string",

	/* I_STMT_ROUTINES_ONE */
	//"SELECT " CATALOG_NAME " AS specific_catalog, routine_schema AS specific_schema, routine_name AS specific_name, " CATALOG_NAME " AS routine_catalog, routine_schema, routine_name, routine_type, dtd_identifier AS return_type, FALSE AS returns_set, 0 AS nb_args, routine_body, routine_definition, external_name, external_language, parameter_style, CASE is_deterministic WHEN 'YES' THEN TRUE ELSE FALSE END AS is_deterministic, sql_data_access, FALSE AS is_null_call, routine_comment, routine_name, routine_name, definer FROM INFORMATION_SCHEMA.routines WHERE routine_schema =  ##schema::string AND routine_name = ##name::string",

	/* I_STMT_ROUTINES_PAR_ALL */
	//"SELECT " CATALOG_NAME " AS specific_catalog, routine_schema AS specific_schema, routine_name AS specific_name, " CATALOG_NAME " AS routine_catalog, routine_schema, routine_name, routine_type, dtd_identifier AS return_type, FALSE AS returns_set, 0 AS nb_args, routine_body, routine_definition, external_name, external_language, parameter_style, CASE is_deterministic WHEN 'YES' THEN TRUE ELSE FALSE END AS is_deterministic, sql_data_access, FALSE AS is_null_call, routine_comment, routine_name, routine_name, definer FROM INFORMATION_SCHEMA.routines",

	/* I_STMT_ROUTINES_PAR */
	//"SELECT " CATALOG_NAME " AS specific_catalog, routine_schema AS specific_schema, routine_name AS specific_name, " CATALOG_NAME " AS routine_catalog, routine_schema, routine_name, routine_type, dtd_identifier AS return_type, FALSE AS returns_set, 0 AS nb_args, routine_body, routine_definition, external_name, external_language, parameter_style, CASE is_deterministic WHEN 'YES' THEN TRUE ELSE FALSE END AS is_deterministic, sql_data_access, FALSE AS is_null_call, routine_comment, routine_name, routine_name, definer FROM INFORMATION_SCHEMA.routines WHERE routine_schema = ##schema::string AND routine_name = ##name::string",

	/* I_STMT_INDEXES_ALL */
	"SELECT DISTINCT " CATALOG_NAME ", " SCHEMA_NAME ", INDEX_NAME, " CATALOG_NAME ", TABLE_SCHEMA, TABLE_NAME, NOT NON_UNIQUE, NULL, INDEX_TYPE, NULL, NULL, COMMENT FROM INFORMATION_SCHEMA.STATISTICS WHERE INDEX_NAME <> 'PRIMARY'",

	/* I_STMT_INDEXES_TABLE */
	"SELECT DISTINCT " CATALOG_NAME ", " SCHEMA_NAME ", INDEX_NAME, " CATALOG_NAME ", TABLE_SCHEMA, TABLE_NAME, NOT NON_UNIQUE, NULL, INDEX_TYPE, NULL, NULL, COMMENT FROM INFORMATION_SCHEMA.STATISTICS WHERE INDEX_NAME <> 'PRIMARY' AND TABLE_SCHEMA = ##schema::string AND TABLE_NAME = ##name::string",

	/* I_STMT_INDEXES_ONE */
	"SELECT DISTINCT " CATALOG_NAME ", INDEX_SCHEMA, INDEX_NAME, " CATALOG_NAME ", TABLE_SCHEMA, TABLE_NAME, NOT NON_UNIQUE, NULL, INDEX_TYPE, NULL, NULL, COMMENT FROM INFORMATION_SCHEMA.STATISTICS WHERE INDEX_NAME <> 'PRIMARY' AND TABLE_SCHEMA = ##schema::string AND TABLE_NAME = ##name::string AND INDEX_NAME = ##name2::string",

	/* I_STMT_INDEX_COLUMNS_ALL */
	"SELECT DISTINCT " CATALOG_NAME ", INDEX_SCHEMA, INDEX_NAME, " CATALOG_NAME ", TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, COLUMN_NAME, SEQ_IN_INDEX FROM INFORMATION_SCHEMA.STATISTICS WHERE INDEX_NAME <> 'PRIMARY' ORDER BY INDEX_SCHEMA, INDEX_NAME, SEQ_IN_INDEX",

	/* I_STMT_INDEX_COLUMNS_NAMED */
	"SELECT DISTINCT " CATALOG_NAME ", INDEX_SCHEMA, INDEX_NAME, " CATALOG_NAME ", TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, COLUMN_NAME, SEQ_IN_INDEX FROM INFORMATION_SCHEMA.STATISTICS WHERE INDEX_NAME <> 'PRIMARY' AND TABLE_SCHEMA = ##schema::string AND TABLE_NAME = ##name::string AND INDEX_NAME = ##name2::string ORDER BY INDEX_SCHEMA, INDEX_NAME, SEQ_IN_INDEX"

};


/*
 * global static values, and
 * predefined statements' GdaStatement, all initialized in _gda_postgres_provider_meta_init()
 */
static GMutex init_mutex;
static GdaStatement **internal_stmt = NULL;
static GdaSqlParser *internal_parser = NULL;
static GdaSet       *i_set = NULL;

/* TO_ADD: other static values */


/*
 * Meta initialization
 */
void
_gda_firebird_provider_meta_init (GdaServerProvider *provider)
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
		i_set = gda_set_new_inline (5, "tblname", G_TYPE_STRING, "",
					    "schema", G_TYPE_STRING, "",
					    "constraint_name", G_TYPE_STRING, "",
					    "field_name", G_TYPE_STRING, ""
					    , "char_set_name", G_TYPE_STRING, "");
	
		/* initialize static values here */
		/*
		  i_set = gda_set_new_inline (3, "name", G_TYPE_STRING, "",
		  "schema", G_TYPE_STRING, "",
		  "name2", G_TYPE_STRING, ""); */
	
	}

	g_mutex_unlock (&init_mutex);
}

gboolean
_gda_firebird_meta__info (GdaServerProvider *prov, GdaConnection *cnc, 
		      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	//WHERE_AM_I;
	model = gda_connection_statement_execute_select_full (cnc
								, internal_stmt[I_STMT_CATALOG]
								, NULL
								, GDA_DATA_MODEL_ACCESS_RANDOM
								, _col_types_information_schema_catalog_name
								, error);
	if (!model)
		return FALSE;
	
	gda_meta_store_set_identifiers_style (store, GDA_SQL_IDENTIFIERS_UPPER_CASE);
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
	if (!retval){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

	return retval;
}

gboolean
_gda_firebird_meta__btypes (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;
	typedef struct
	{
		gchar  *tname;
		gchar  *gtype;
		gchar  *comments;
		gchar  *synonyms;
	} BuiltinDataType;

	BuiltinDataType data_types[] = {
		{ "CHAR"
			, "gchararray"
			, "A fixed-length string that is always right-padded with spaces to the specified length when stored. M represents the column length in characters. The range of M is 0 to 255. If M is omitted, the length is 1."
			, "" }
		, { "VARCHAR"
			, "gchararray"
			, "A variable-length string. M represents the maximum column length in characters. In MySQL 5.0, the range of M is 0 to 255 before MySQL 5.0.3, and 0 to 65,535 in MySQL 5.0.3 and later. The effective maximum length of a VARCHAR in MySQL 5.0.3 and later is subject to the maximum row size (65,535 bytes, which is shared among all columns) and the character set used. For example, utf8 characters can require up to three bytes per character, so a VARCHAR column that uses the utf8 character set can be declared to be a maximum of 21,844 characters."
			, "" }

		, { "DATE"
			, "GDate"
			, "A date. The supported range is '1000-01-01' to '9999-12-31'. MySQL displays DATE values in 'YYYY-MM-DD' format, but allows assignment of values to DATE columns using either strings or numbers."
			, "" }
		, { "TIME"
			, "GdaTime"
			, "A time. The range is '-838:59:59' to '838:59:59'. MySQL displays TIME values in 'HH:MM:SS' format, but allows assignment of values to TIME columns using either strings or numbers."
			, "" }
			
		, { "TIMESTAMP"
			, "GDateTime"
			, "A timestamp. The range is '1970-01-01 00:00:01' UTC to partway through the year 2038. TIMESTAMP values are stored as the number of seconds since the epoch ('1970-01-01 00:00:00' UTC). A TIMESTAMP cannot represent the value '1970-01-01 00:00:00' because that is equivalent to 0 seconds from the epoch and the value 0 is reserved for representing '0000-00-00 00:00:00', the \"zero\" TIMESTAMP value."
			, "" }

		, { "SMALLINT"
			, "gint"
			, "A small integer. The signed range is -32768 to 32767. The unsigned range is 0 to 65535."
			, "" }	
		, { "INTEGER"
			, "gint"
			, "A normal-size integer. The signed range is -2147483648 to 2147483647. The unsigned range is 0 to 4294967295."
			, "INTEGER" }
		, { "DECIMAL"
			, "GdaNumeric"
			, "A packed \"exact\" fixed-point number. M is the total number of digits (the precision) and D is the number of digits after the decimal point (the scale). The decimal point and (for negative numbers) the \"-\" sign are not counted in M. If D is 0, values have no decimal point or fractional part. The maximum number of digits (M) for DECIMAL is 65 (64 from 5.0.3 to 5.0.5). The maximum number of supported decimals (D) is 30. If D is omitted, the default is 0. If M is omitted, the default is 10."
			, "DEC" }
		, { "DOUBLE"
			, "gdouble"
			, "A normal-size (double-precision) floating-point number. Allowable values are -1.7976931348623157E+308 to -2.2250738585072014E-308, 0, and 2.2250738585072014E-308 to 1.7976931348623157E+308. These are the theoretical limits, based on the IEEE standard. The actual range might be slightly smaller depending on your hardware or operating system."
			, "DOUBLE PRECISION" }

		, { "FLOAT"
			, "gfloat"
			, "A small (single-precision) floating-point number. Allowable values are -3.402823466E+38 to -1.175494351E-38, 0, and 1.175494351E-38 to 3.402823466E+38. These are the theoretical limits, based on the IEEE standard. The actual range might be slightly smaller depending on your hardware or operating system."
			, "" }
		, { "NUMERIC"
			, "GdaNumeric"
			, "A BLOB column with a maximum length of 4,294,967,295 or 4GB (232 - 1) bytes. The effective maximum length of LONGBLOB columns depends on the configured maximum packet size in the client/server protocol and available memory. Each LONGBLOB value is stored using a four-byte length prefix that indicates the number of bytes in the value."
			, "" }
		, { "INT64"
			, "gint64"
			, "A large integer. The signed range is -9223372036854775808 to 9223372036854775807. The unsigned range is 0 to 18446744073709551615."
			, "" }
		, { "BIGINT"
			, "gint64"
			, "A large integer. The signed range is -9223372036854775808 to 9223372036854775807. The unsigned range is 0 to 18446744073709551615."
			, "" }

		, { "BLOB"
			, "GdaBinary"
			, "A BLOB column with a maximum length of 65,535 (216 - 1) bytes. Each BLOB value is stored using a two-byte length prefix that indicates the number of bytes in the value."
			, "" }

	};

	GdaDataModel *model;
	gboolean retval = TRUE;

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

			g_value_set_string (tmp_value = gda_value_new (G_TYPE_STRING), data_type->comments);
			values = g_list_append (values, tmp_value); 

			if (data_type->synonyms && *(data_type->synonyms))
				g_value_set_string (tmp_value = gda_value_new (G_TYPE_STRING), data_type->synonyms);
			else
				tmp_value = gda_value_new_null ();
			values = g_list_append (values, tmp_value); 

			g_value_set_boolean (tmp_value = gda_value_new (G_TYPE_BOOLEAN), FALSE);
			values = g_list_append (values, tmp_value); 

			if (gda_data_model_append_values (model, values, NULL) < 0) {
				retval = FALSE;
				break;
			}

			g_list_foreach (values, (GFunc) gda_value_free, NULL);
			g_list_free (values);
		}

		if (retval) {
			//retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
			retval = gda_meta_store_modify_with_context (store, context, model, error);
		}
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

	return retval;
}

gboolean
_gda_firebird_meta__udt (GdaServerProvider *prov, GdaConnection *cnc, 
		     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_udt (GdaServerProvider *prov, GdaConnection *cnc, 
		    GdaMetaStore *store, GdaMetaContext *context, GError **error,
		    const GValue *udt_catalog, const GValue *udt_schema)
{
	//GdaDataModel *model;
	gboolean retval = TRUE;

	//WHERE_AM_I;

	/* set internal holder's values from the arguments */
	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), udt_catalog, error);
	gda_holder_set_value (gda_set_get_holder (i_set, "schema"), udt_schema, error);

	//TO_IMPLEMENT;
	/* fill in @model, with something like:
	 * model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_UDT], i_set, error);
	 *
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
	*/
	return retval;
}


gboolean
_gda_firebird_meta__udt_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_udt_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta__enums (GdaServerProvider *prov, GdaConnection *cnc, 
		       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_enums (GdaServerProvider *prov, GdaConnection *cnc, 
		      GdaMetaStore *store, GdaMetaContext *context, GError **error,
		      const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}


gboolean
_gda_firebird_meta__domains (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_domains (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *domain_catalog, const GValue *domain_schema)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta__constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *domain_catalog, const GValue *domain_schema, 
				const GValue *domain_name)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta__el_types (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_el_types (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *specific_name)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta__collations (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_collations (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *collation_catalog, const GValue *collation_schema, 
			   const GValue *collation_name_n)
{
	//WHERE_AM_I;

	//TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta__character_sets (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	//WHERE_AM_I;
	model = gda_connection_statement_execute_select_full (cnc,
									internal_stmt[I_STMT_CHARACTER_SETS_ALL],
									NULL, 
									GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									_col_types_character_sets,
									error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

	return retval;
}

gboolean
_gda_firebird_meta_character_sets (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error,
			       const GValue *chset_catalog, const GValue *chset_schema, 
			       const GValue *chset_name_n)
{
	GdaDataModel *model;
	gboolean retval;
	//WHERE_AM_I;

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "char_set_name"), chset_name_n, error))
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
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

	return retval;

}

gboolean
_gda_firebird_meta__schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	//WHERE_AM_I;
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_SCHEMAS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_schemata, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

	return retval;
}

gboolean
_gda_firebird_meta_schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			 const GValue *catalog_name, const GValue *schema_name_n)
{
	GdaDataModel *model;
	gboolean retval;
	//WHERE_AM_I;
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
			retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
			g_object_unref (G_OBJECT(model));
		}
	} else {
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_SCHEMA_NAMED],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_schemata,
								      error);
		if (model == NULL)
			retval = FALSE;
		else {
			retval = gda_meta_store_modify (store, context->table_name, model,
							"schema_name=##name::string",
							error,
							"schema", schema_name_n, NULL);
			g_object_unref (G_OBJECT(model));
		}
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}
	return retval;
}

gboolean
_gda_firebird_meta__tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model_tables, *model_views;
	gboolean retval;
	//WHERE_AM_I;
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
		retval = gda_meta_store_modify_with_context (store, &copy, model_tables, error);
		g_object_unref (G_OBJECT(model_tables));
		if (retval == FALSE){g_print("\n\n***ERROR (_tables): \n\n");}
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
		retval = gda_meta_store_modify_with_context (store, &copy, model_views, error);
		g_object_unref (G_OBJECT(model_views));
	}
	if (retval == FALSE){g_print("\n\n***ERROR (_views): \n\n");}
	return retval;
}

gboolean
_gda_firebird_meta_tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *table_catalog, const GValue *table_schema, 
			     const GValue *table_name_n)
{
	GdaDataModel *model_tables, *model_views;
	gboolean retval;
	//WHERE_AM_I;
	
	FirebirdConnectionData *cdata;
	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return FALSE;

	/* Copy contents, just because we need to modify @context->table_name */
	GdaMetaContext copy = *context;

	if (!table_name_n) {
		model_tables = gda_connection_statement_execute_select_full (cnc,
									     internal_stmt[I_STMT_TABLES],
									     NULL, 
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									     _col_types_tables,
									     error);
		if (model_tables == NULL)
			retval = FALSE;
		else {
			copy.table_name = "_tables";
			retval = gda_meta_store_modify_with_context (store, &copy, model_tables, error);
			g_object_unref (G_OBJECT (model_tables));
		}

		if (!retval)
			return FALSE;

		model_views = gda_connection_statement_execute_select_full (cnc,
									    internal_stmt[I_STMT_VIEWS],
									    NULL, 
									    GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									    _col_types_views,
									    error);
		if (model_views == NULL)
			retval = FALSE;
		else {
			copy.table_name = "_views";
			retval = gda_meta_store_modify_with_context (store, &copy, model_views, error);
			g_object_unref (G_OBJECT (model_views));
		}

	} 
	else {
		g_print("got to the named portion\n");
		if (!gda_holder_set_value (gda_set_get_holder (i_set, "tblname"), table_name_n, error))
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
			retval = gda_meta_store_modify_with_context (store, &copy, model_views, error);
			g_object_unref (G_OBJECT(model_views));
		}
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

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
map_firebird_type_to_gda (const GValue  *value)
{
	const gchar *string = g_value_get_string (value);
	GValue *newvalue;
	const gchar *newstring;

	if (strcmp (string, "blob") == 0)
		newstring = "GdaBinary";
	else
	if (strcmp (string, "int64") == 0)
		newstring = "gint64";
	else
	if (strcmp (string, "char") == 0)
		newstring = "gchar";
	else
	if (strcmp (string, "date") == 0)
		newstring = "GDate";
	else
	if (strcmp (string, "decimal") == 0)
		newstring = "GdaNumeric";
	else
	if (strcmp (string, "numeric") == 0)
		newstring = "GdaNumeric";
	else
	if (strcmp (string, "double") == 0)
		newstring = "gdouble";
	else
	if (strcmp (string, "float") == 0)
		newstring = "gfloat";
	else
	if (strcmp (string, "int") == 0)
		newstring = "gint";
	else
	if (strcmp (string, "long") == 0)
		newstring = "glong";
	else
	if (strcmp (string, "short") == 0)
		newstring = "gint";
	else
	if (strcmp (string, "text") == 0)
		newstring = "gchararray";
	else
	if (strcmp (string, "smallint") == 0)
		newstring = "gint";
	else
	if (strcmp (string, "time") == 0)
		newstring = "GdaTime";
	else
	if (strcmp (string, "timestamp") == 0)
		newstring = "GDateTime";
	else
	if (strcmp (string, "varchar") == 0)
		newstring = "gchararray";
	else
	if (strcmp (string, "varying") == 0)
		newstring = "gchararray";
	else
	{	
		g_print("Please report this bug. The following data-type is not supported in the library. %s\n", string);
		newstring = "";
	}
	g_value_set_string (newvalue = gda_value_new (G_TYPE_STRING), newstring);

	return newvalue;
}

gboolean
_gda_firebird_meta__columns (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model, *proxy;
	gboolean retval = TRUE;

	//WHERE_AM_I;

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
			const GValue *value = gda_data_model_get_value_at (model, 7, i, error);
			if (!value) {
				retval = FALSE;
				break;
			}

			GValue *newvalue = map_firebird_type_to_gda (value);

			retval = gda_data_model_set_value_at (GDA_DATA_MODEL(proxy), 9, i, newvalue, error);
			gda_value_free (newvalue);
			if (!retval)
				break;
		}

		if (retval) {
			retval = gda_meta_store_modify_with_context (store, context, proxy, error);
			if (!retval)
				g_print("ERROR MESSAGE: \n\n%s\n\n", (*error)->message);
		}

		g_object_unref (G_OBJECT(proxy));
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

	return retval;
}

gboolean
_gda_firebird_meta_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *table_catalog, const GValue *table_schema, 
			const GValue *table_name)
{
	GdaDataModel *model, *proxy;
	gboolean retval = TRUE;
	//WHERE_AM_I;
	/* Use a prepared statement for the "base" model. */
	const gchar *str;
	str = g_value_get_string (table_name);
	g_print("get columns for: %s\n", str);
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "tblname"), table_name, error))
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
			const GValue *value = gda_data_model_get_value_at (model, 7, i, error);
			if (!value) {
				retval = FALSE;
				break;
			}

			GValue *newvalue = map_firebird_type_to_gda (value);

			retval = gda_data_model_set_value_at (GDA_DATA_MODEL(proxy), 9, i, newvalue, error);
			gda_value_free (newvalue);
			if (!retval)
				break;
		}

		if (retval) {
			retval = gda_meta_store_modify (store, context->table_name, proxy,
							NULL,
							error,
							NULL);

			/*
			retval = gda_meta_store_modify (store, context->table_name, proxy,
							"RF.RDB$RELATION_NAME = ##tblname::string",
							error,
							"tblname", table_name, NULL);
			*/
		}
		g_object_unref (G_OBJECT(proxy));
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR (%s): %s\n\n", __FUNCTION__, (*error)->message);}

	return retval;
}

gboolean
_gda_firebird_meta__view_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;

	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select_full (cnc,
							internal_stmt[I_STMT_VIEWS_COLUMNS_ALL],
							NULL, 
							GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							_col_types_view_column_usage, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

	return retval;
}

gboolean
_gda_firebird_meta_view_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error,
			  const GValue *view_catalog, const GValue *view_schema, 
			  const GValue *view_name)
{
	GdaDataModel *model;
	gboolean retval;
//WHERE_AM_I;

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "tblname"), view_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select_full (cnc,
							internal_stmt[I_STMT_VIEWS_COLUMNS],
							i_set, 
							GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							_col_types_view_column_usage, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

	return retval;
}

gboolean
_gda_firebird_meta__constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
//WHERE_AM_I;
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select_full (cnc,
						internal_stmt[I_STMT_TABLES_CONSTRAINTS_ALL],
						NULL,
						GDA_STATEMENT_MODEL_RANDOM_ACCESS,
						_col_types_table_constraints, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

	return retval;

}

gboolean
_gda_firebird_meta_constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error, 
				const GValue *table_catalog, const GValue *table_schema, 
				const GValue *table_name, const GValue *constraint_name_n)
{
//WHERE_AM_I;
	return TRUE;
	GdaDataModel *model;
	gboolean retval;

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "tblname"), table_name, error))
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
			retval = gda_meta_store_modify (store, context->table_name, model,
							"TRIM(RDB$RELATION_NAME) = ##tblname::string",
							error,
							"tblname", table_name, NULL);
			g_object_unref (G_OBJECT(model));
		}
	} else {
		if (!gda_holder_set_value (gda_set_get_holder (i_set, "constraint_name"), constraint_name_n, error))
			return FALSE;
		model = gda_connection_statement_execute_select_full (cnc,
							internal_stmt[I_STMT_TABLES_CONSTRAINTS_NAMED],
							i_set,
							GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							_col_types_table_constraints, error);
		if (model == NULL)
			retval = FALSE;
		else {
			retval = gda_meta_store_modify (store, context->table_name, model,
							"TRIM(RDB$RELATION_NAME) = ##tblname::string AND RDB$CONSTRAINT_NAME = ##constraint_name::string",
							error,
							"tblname", table_name, "constraint_name", constraint_name_n, NULL);
			g_object_unref (G_OBJECT(model));
		}
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}

	return retval;
}

gboolean
_gda_firebird_meta__constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
//WHERE_AM_I;
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
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}
	return retval;
}

gboolean
_gda_firebird_meta_constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				const GValue *constraint_name)
{
//WHERE_AM_I;
	GdaDataModel *model;
	gboolean retval = TRUE;
	
	/* Use a prepared statement for the "base" model. */

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "tblname"), table_name, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "constraint_name"), constraint_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							internal_stmt[I_STMT_REF_CONSTRAINTS],
							i_set,
							GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							_col_types_referential_constraints, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify (store, context->table_name, model,
						"TRIM(R1.RDB$RELATION_NAME) = ##tblname::string AND C.RDB$CONSTRAINT_NAME = ##constraint_name::string",
						error,
						"tblname", table_name, "constraint_name", constraint_name, NULL);
		g_object_unref (G_OBJECT(model));
		
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}
	return retval;
}

gboolean
_gda_firebird_meta__key_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
//WHERE_AM_I;
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_connection_statement_execute_select_full (cnc,
						internal_stmt[I_STMT_KEY_COLUMN_USAGE_ALL],
						NULL,
						GDA_STATEMENT_MODEL_RANDOM_ACCESS,
						_col_types_key_column_usage, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}
	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}
	return retval;
}

gboolean
_gda_firebird_meta_key_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *table_catalog, const GValue *table_schema, 
			    const GValue *table_name, const GValue *constraint_name)
{
//WHERE_AM_I;
	GdaDataModel *model;
	gboolean retval = TRUE;

	/* Use a prepared statement for the "base" model. */

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "tblname"), table_name, error))
		return FALSE;
	if (!gda_holder_set_value (gda_set_get_holder (i_set, "constraint_name"), constraint_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select_full (cnc,
							internal_stmt[I_STMT_KEY_COLUMN_USAGE],
							i_set, GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							_col_types_key_column_usage,
							error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify (store, context->table_name, model,
						"TRIM(r.RDB$RELATION_NAME) = ##tblname::string AND r.rdb$constraint_name = ##constraint_name::string",
						error,
						"tblname", table_name, "constraint_name", constraint_name, NULL);
		g_object_unref (G_OBJECT(model));
	}

	if (retval == FALSE){g_print("\n\n***ERROR: %s\n\n", (*error)->message);}
	return retval;
}

gboolean
_gda_firebird_meta__check_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;

	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_check_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *table_catalog, const GValue *table_schema, 
			      const GValue *table_name, const GValue *constraint_name)
{
	//WHERE_AM_I;
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta__triggers (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
//WHERE_AM_I;

/*	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_TRIGGERS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_triggers, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
*/
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_triggers (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *table_catalog, const GValue *table_schema, 
			 const GValue *table_name)
{
//WHERE_AM_I; return TRUE;
/*
	GdaDataModel *model;
	gboolean retval;

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
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
*/
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta__routines (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
//WHERE_AM_I; return TRUE;
/*
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_ROUTINES_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_routines, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
*/
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_routines (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *routine_catalog, const GValue *routine_schema, 
			 const GValue *routine_name_n)
{
//WHERE_AM_I; return TRUE;
/*
	GdaDataModel *model;
	gboolean retval;

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
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
*/
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta__routine_col (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_routine_col (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *rout_catalog, const GValue *rout_schema, 
			    const GValue *rout_name, const GValue *col_name, const GValue *ordinal_position)
{
	//WHERE_AM_I;
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta__routine_par (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	//WHERE_AM_I;
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_routine_par (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *rout_catalog, const GValue *rout_schema, 
			    const GValue *rout_name)
{
	//WHERE_AM_I;
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta__indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
//WHERE_AM_I;
	TO_IMPLEMENT;
	return TRUE;
/*
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_INDEXES_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_table_indexes, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
*/
}

gboolean
_gda_firebird_meta_indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     G_GNUC_UNUSED const GValue *table_catalog, const GValue *table_schema,
			     const GValue *table_name, const GValue *index_name_n)
{
//WHERE_AM_I;
	TO_IMPLEMENT;
	return TRUE;
/*
	GdaDataModel *model;
	gboolean retval;

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
*/
}

gboolean
_gda_firebird_meta__index_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
//WHERE_AM_I; return TRUE;
/*
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_INDEX_COLUMNS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_index_column_usage, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
*/
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_firebird_meta_index_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    G_GNUC_UNUSED const GValue *table_catalog, const GValue *table_schema,
			    const GValue *table_name, const GValue *index_name)
{
//WHERE_AM_I; return TRUE;
/*
	GdaDataModel *model;
	gboolean retval;

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
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
*/
	TO_IMPLEMENT;
	return TRUE;
}

