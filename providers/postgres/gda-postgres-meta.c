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

/*
 * predefined statements' IDs
 */
typedef enum {
        I_STMT_CATALOG,
	I_STMT_BTYPES,
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
	I_STMT_TABLES_CONSTRAINT_NAMED,
	I_STMT_REF_CONSTRAINTS,
	I_STMT_REF_CONSTRAINTS_ALL,
	I_STMT_KEY_COLUMN_USAGE,
	I_STMT_KEY_COLUMN_USAGE_ALL,
	I_STMT_CHECK_COLUMN_USAGE,
	I_STMT_CHECK_COLUMN_USAGE_ALL,
	I_STMT_UDT,
	I_STMT_UDT_ALL,
	I_STMT_UDT_COLUMNS,
	I_STMT_UDT_COLUMNS_ALL,
	I_STMT_DOMAINS,
	I_STMT_DOMAINS_ALL,
	I_STMT_DOMAINS_CONSTRAINTS,
	I_STMT_DOMAINS_CONSTRAINTS_ALL,
	I_STMT_VIEWS_COLUMNS,
	I_STMT_VIEWS_COLUMNS_ALL,
	I_STMT_TRIGGERS,
	I_STMT_TRIGGERS_ALL,
	I_STMT_EL_TYPES_COL,
	I_STMT_EL_TYPES_DOM,
	I_STMT_EL_TYPES_UDT,
	I_STMT_EL_TYPES_ROUT_PAR,
	I_STMT_EL_TYPES_ROUT_COL,
	I_STMT_EL_TYPES_ALL,
	I_STMT_ROUTINES_ALL,
	I_STMT_ROUTINES,
	I_STMT_ROUTINES_ONE,
	I_STMT_ROUTINE_PAR_ALL,
	I_STMT_ROUTINE_PAR,
	I_STMT_ROUTINE_COL_ALL,
	I_STMT_ROUTINE_COL
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	/* I_STMT_CATALOG */
	"SELECT pg_catalog.current_database()",

	/* I_STMT_BTYPES */
	"SELECT t.typname, 'pg_catalog.' || t.typname, 'gchararray', pg_catalog.obj_description(t.oid), NULL, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN typtype = 'p' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, CAST (t.oid AS int8) FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (typtype='b' OR typtype='p') AND typelem = 0",

	/* I_STMT_SCHEMAS */
	"SELECT catalog_name, schema_name, schema_owner, CASE WHEN schema_name ~'^pg_' THEN TRUE WHEN schema_name ='information_schema' THEN TRUE ELSE FALSE END FROM information_schema.schemata WHERE catalog_name = ##cat::string",

	/* I_STMT_SCHEMAS_ALL */
	"SELECT catalog_name, schema_name, schema_owner, CASE WHEN schema_name ~'^pg_' THEN TRUE WHEN schema_name ='information_schema' THEN TRUE ELSE FALSE END FROM information_schema.schemata",

	/* I_STMT_SCHEMA_NAMED */
	"SELECT catalog_name, schema_name, schema_owner, CASE WHEN schema_name ~'^pg_' THEN TRUE WHEN schema_name ='information_schema' THEN TRUE ELSE FALSE END FROM information_schema.schemata WHERE catalog_name = ##cat::string AND schema_name = ##name::string",

	/* I_STMT_TABLES */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nc.nspname::information_schema.sql_identifier AS table_schema, c.relname::information_schema.sql_identifier AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY'::text WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL::text END::information_schema.character_data AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_authid o WHERE current_database()::information_schema.sql_identifier = ##cat::string AND nc.nspname::information_schema.sql_identifier = ##schema::string AND c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text)) AND o.oid=c.relowner",

	/* I_STMT_TABLES_ALL */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nc.nspname::information_schema.sql_identifier AS table_schema, c.relname::information_schema.sql_identifier AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY'::text WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL::text END::information_schema.character_data AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_authid o WHERE c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text)) AND o.oid=c.relowner",

	/* I_STMT_TABLE_NAMED */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nc.nspname::information_schema.sql_identifier AS table_schema, c.relname::information_schema.sql_identifier AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY'::text WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL::text END::information_schema.character_data AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_authid o WHERE current_database()::information_schema.sql_identifier = ##cat::string AND nc.nspname::information_schema.sql_identifier = ##schema::string AND c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text)) AND o.oid=c.relowner AND c.relname::information_schema.sql_identifier = ##name::string",

	/* I_STMT_VIEWS */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nc.nspname::information_schema.sql_identifier AS table_schema, c.relname::information_schema.sql_identifier AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r'::\"char\" THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE current_database()::information_schema.sql_identifier = ##cat::string AND nc.nspname::information_schema.sql_identifier = ##schema::string AND c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text))",

	/* I_STMT_VIEWS_ALL */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nc.nspname::information_schema.sql_identifier AS table_schema, c.relname::information_schema.sql_identifier AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r'::\"char\" THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text))",

	/* I_STMT_VIEW_NAMED */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nc.nspname::information_schema.sql_identifier AS table_schema, c.relname::information_schema.sql_identifier AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r'::\"char\" THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE current_database()::information_schema.sql_identifier = ##cat::string AND nc.nspname::information_schema.sql_identifier = ##schema::string AND c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text)) AND c.relname::information_schema.sql_identifier = ##name::string",

	/* I_STMT_COLUMNS_OF_TABLE */
	"SELECT current_database(), nc.nspname, c.relname, a.attname, a.attnum, pg_get_expr(ad.adbin, ad.adrelid), CASE WHEN a.attnotnull OR t.typtype = 'd' AND t.typnotnull THEN FALSE ELSE TRUE END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL, NULL, NULL, NULL, NULL, CASE WHEN pg_get_expr(ad.adbin, ad.adrelid) LIKE 'nextval(%' THEN '" GDA_EXTRA_AUTO_INCREMENT "' ELSE NULL END, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.col_description(c.oid, a.attnum), CAST (t.oid AS int8) FROM pg_attribute a LEFT JOIN pg_attrdef ad ON a.attrelid = ad.adrelid AND a.attnum = ad.adnum, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typtype = 'd' AND t.typbasetype = bt.oid WHERE current_database() = ##cat::string AND nc.nspname = ##schema::string AND c.relname = ##name::string AND a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text))",

	/* I_STMT_COLUMNS_ALL */
	"SELECT current_database(), nc.nspname, c.relname, a.attname, a.attnum, pg_get_expr(ad.adbin, ad.adrelid), CASE WHEN a.attnotnull OR t.typtype = 'd' AND t.typnotnull THEN FALSE ELSE TRUE END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL, NULL, NULL, NULL, NULL, CASE WHEN pg_get_expr(ad.adbin, ad.adrelid) LIKE 'nextval(%' THEN '" GDA_EXTRA_AUTO_INCREMENT "' ELSE NULL END, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.col_description(c.oid, a.attnum), CAST (t.oid AS int8) FROM pg_attribute a LEFT JOIN pg_attrdef ad ON a.attrelid = ad.adrelid AND a.attnum = ad.adnum, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typtype = 'd' AND t.typbasetype = bt.oid WHERE a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text))",

	/* I_STMT_TABLES_CONSTRAINTS */
	"SELECT current_database()::information_schema.sql_identifier AS constraint_catalog, nc.nspname::information_schema.sql_identifier AS constraint_schema, c.conname::information_schema.sql_identifier AS constraint_name, current_database()::information_schema.sql_identifier AS table_catalog, nr.nspname::information_schema.sql_identifier AS table_schema, r.relname::information_schema.sql_identifier AS table_name, CASE c.contype WHEN 'c'::\"char\" THEN 'CHECK'::text WHEN 'f'::\"char\" THEN 'FOREIGN KEY'::text WHEN 'p'::\"char\" THEN 'PRIMARY KEY'::text WHEN 'u'::\"char\" THEN 'UNIQUE'::text ELSE NULL::text END::information_schema.character_data AS constraint_type, CASE c.contype WHEN 'c'::\"char\" THEN c.consrc ELSE NULL END, CASE WHEN c.condeferrable THEN TRUE ELSE FALSE END AS is_deferrable, CASE WHEN c.condeferred THEN TRUE ELSE FALSE END AS initially_deferred FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r'::\"char\" AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE'::text) OR has_table_privilege(r.oid, 'INSERT'::text) OR has_table_privilege(r.oid, 'UPDATE'::text) OR has_table_privilege(r.oid, 'DELETE'::text) OR has_table_privilege(r.oid, 'REFERENCES'::text) OR has_table_privilege(r.oid, 'TRIGGER'::text)) AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string "
	"UNION SELECT current_database()::information_schema.sql_identifier AS constraint_catalog, nr.nspname::information_schema.sql_identifier AS constraint_schema, (((((nr.oid::text || '_'::text) || r.oid::text) || '_'::text) || a.attnum::text) || '_not_null'::text)::information_schema.sql_identifier AS constraint_name, current_database()::information_schema.sql_identifier AS table_catalog, nr.nspname::information_schema.sql_identifier AS table_schema, r.relname::information_schema.sql_identifier AS table_name, 'CHECK'::character varying::information_schema.character_data AS constraint_type, a.attname || ' IS NOT NULL', FALSE AS is_deferrable, FALSE AS initially_deferred FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r'::\"char\" AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE'::text) OR has_table_privilege(r.oid, 'SELECT'::text) OR has_table_privilege(r.oid, 'INSERT'::text) OR has_table_privilege(r.oid, 'UPDATE'::text) OR has_table_privilege(r.oid, 'DELETE'::text) OR has_table_privilege(r.oid, 'REFERENCES'::text) OR has_table_privilege(r.oid, 'TRIGGER'::text)) AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string",

	/* I_STMT_TABLES_CONSTRAINTS_ALL */
	"SELECT current_database()::information_schema.sql_identifier AS constraint_catalog, nc.nspname::information_schema.sql_identifier AS constraint_schema, c.conname::information_schema.sql_identifier AS constraint_name, current_database()::information_schema.sql_identifier AS table_catalog, nr.nspname::information_schema.sql_identifier AS table_schema, r.relname::information_schema.sql_identifier AS table_name, CASE c.contype WHEN 'c'::\"char\" THEN 'CHECK'::text WHEN 'f'::\"char\" THEN 'FOREIGN KEY'::text WHEN 'p'::\"char\" THEN 'PRIMARY KEY'::text WHEN 'u'::\"char\" THEN 'UNIQUE'::text ELSE NULL::text END::information_schema.character_data AS constraint_type, CASE c.contype WHEN 'c'::\"char\" THEN c.consrc ELSE NULL END, CASE WHEN c.condeferrable THEN TRUE ELSE FALSE END AS is_deferrable, CASE WHEN c.condeferred THEN TRUE ELSE FALSE END AS initially_deferred FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r'::\"char\" AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE'::text) OR has_table_privilege(r.oid, 'INSERT'::text) OR has_table_privilege(r.oid, 'UPDATE'::text) OR has_table_privilege(r.oid, 'DELETE'::text) OR has_table_privilege(r.oid, 'REFERENCES'::text) OR has_table_privilege(r.oid, 'TRIGGER'::text)) "
	"UNION SELECT current_database()::information_schema.sql_identifier AS constraint_catalog, nr.nspname::information_schema.sql_identifier AS constraint_schema, (((((nr.oid::text || '_'::text) || r.oid::text) || '_'::text) || a.attnum::text) || '_not_null'::text)::information_schema.sql_identifier AS constraint_name, current_database()::information_schema.sql_identifier AS table_catalog, nr.nspname::information_schema.sql_identifier AS table_schema, r.relname::information_schema.sql_identifier AS table_name, 'CHECK'::character varying::information_schema.character_data AS constraint_type, a.attname || ' IS NOT NULL', FALSE AS is_deferrable, FALSE AS initially_deferred FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r'::\"char\" AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE'::text) OR has_table_privilege(r.oid, 'SELECT'::text) OR has_table_privilege(r.oid, 'INSERT'::text) OR has_table_privilege(r.oid, 'UPDATE'::text) OR has_table_privilege(r.oid, 'DELETE'::text) OR has_table_privilege(r.oid, 'REFERENCES'::text) OR has_table_privilege(r.oid, 'TRIGGER'::text))",

	/* I_STMT_TABLES_CONSTRAINT_NAMED */
	"SELECT constraint_catalog, constraint_schema, constraint_name, table_catalog, table_schema, table_name, constraint_type, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.table_constraints WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string",

	/* I_STMT_REF_CONSTRAINTS */
	"SELECT current_database(), nt.nspname, t.relname, c.conname, current_database(), nref.nspname, ref.relname, pkc.conname, CASE c.confmatchtype WHEN 'f'::\"char\" THEN 'FULL'::text WHEN 'p'::\"char\" THEN 'PARTIAL'::text WHEN 'u'::\"char\" THEN 'NONE'::text ELSE NULL::text END AS match_option, CASE c.confupdtype WHEN 'c'::\"char\" THEN 'CASCADE'::text WHEN 'n'::\"char\" THEN 'SET NULL'::text WHEN 'd'::\"char\" THEN 'SET DEFAULT'::text WHEN 'r'::\"char\" THEN 'RESTRICT'::text WHEN 'a'::\"char\" THEN 'NO ACTION'::text ELSE NULL::text END AS update_rule, CASE c.confdeltype WHEN 'c'::\"char\" THEN 'CASCADE'::text WHEN 'n'::\"char\" THEN 'SET NULL'::text WHEN 'd'::\"char\" THEN 'SET DEFAULT'::text WHEN 'r'::\"char\" THEN 'RESTRICT'::text WHEN 'a'::\"char\" THEN 'NO ACTION'::text ELSE NULL::text END AS delete_rule FROM pg_constraint c INNER JOIN pg_class t ON (c.conrelid=t.oid) INNER JOIN pg_namespace nt ON (nt.oid=t.relnamespace) INNER JOIN pg_class ref ON (c.confrelid=ref.oid) INNER JOIN pg_namespace nref ON (nref.oid=ref.relnamespace) INNER JOIN pg_constraint pkc ON (c.confrelid = pkc.conrelid AND information_schema._pg_keysequal(c.confkey, pkc.conkey) AND pkc.contype='p') WHERE c.contype = 'f' AND current_database() = ##cat::string AND nt.nspname = ##schema::string AND t.relname = ##name::string AND c.conname = ##name2::string",

	/* I_STMT_REF_CONSTRAINTS_ALL */
	"SELECT current_database(), nt.nspname, t.relname, c.conname, current_database(), nref.nspname, ref.relname, pkc.conname, CASE c.confmatchtype WHEN 'f'::\"char\" THEN 'FULL'::text WHEN 'p'::\"char\" THEN 'PARTIAL'::text WHEN 'u'::\"char\" THEN 'NONE'::text ELSE NULL::text END AS match_option, CASE c.confupdtype WHEN 'c'::\"char\" THEN 'CASCADE'::text WHEN 'n'::\"char\" THEN 'SET NULL'::text WHEN 'd'::\"char\" THEN 'SET DEFAULT'::text WHEN 'r'::\"char\" THEN 'RESTRICT'::text WHEN 'a'::\"char\" THEN 'NO ACTION'::text ELSE NULL::text END AS update_rule, CASE c.confdeltype WHEN 'c'::\"char\" THEN 'CASCADE'::text WHEN 'n'::\"char\" THEN 'SET NULL'::text WHEN 'd'::\"char\" THEN 'SET DEFAULT'::text WHEN 'r'::\"char\" THEN 'RESTRICT'::text WHEN 'a'::\"char\" THEN 'NO ACTION'::text ELSE NULL::text END AS delete_rule FROM pg_constraint c INNER JOIN pg_class t ON (c.conrelid=t.oid) INNER JOIN pg_namespace nt ON (nt.oid=t.relnamespace) INNER JOIN pg_class ref ON (c.confrelid=ref.oid) INNER JOIN pg_namespace nref ON (nref.oid=ref.relnamespace) INNER JOIN pg_constraint pkc ON (c.confrelid = pkc.conrelid AND information_schema._pg_keysequal(c.confkey, pkc.conkey) AND pkc.contype='p') WHERE c.contype = 'f'",

	/* I_STMT_KEY_COLUMN_USAGE */
	"SELECT table_catalog, table_schema, table_name, constraint_name, column_name, ordinal_position FROM information_schema.key_column_usage WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string",

	/* I_STMT_KEY_COLUMN_USAGE_ALL */
	"SELECT table_catalog, table_schema, table_name, constraint_name, column_name, ordinal_position FROM information_schema.key_column_usage",

	/* I_STMT_CHECK_COLUMN_USAGE */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nr.nspname::information_schema.sql_identifier AS table_schema, r.relname::information_schema.sql_identifier AS table_name, c.conname::information_schema.sql_identifier AS constraint_name,a.attname FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r, pg_attribute a, (SELECT sc.oid, information_schema._pg_expandarray (sc.conkey) as x FROM pg_constraint sc WHERE sc.contype = 'c') ss WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r'::\"char\" AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE'::text) OR has_table_privilege(r.oid, 'INSERT'::text) OR has_table_privilege(r.oid, 'UPDATE'::text) OR has_table_privilege(r.oid, 'DELETE'::text) OR has_table_privilege(r.oid, 'REFERENCES'::text) OR has_table_privilege(r.oid, 'TRIGGER'::text)) AND c.contype = 'c' AND ss.oid = c.oid AND a.attrelid = r.oid AND a.attnum = (ss.x).x AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string AND c.conname = ##name2::string "
	"UNION SELECT current_database()::information_schema.sql_identifier AS table_catalog, nr.nspname::information_schema.sql_identifier AS table_schema, r.relname::information_schema.sql_identifier AS table_name, (((((nr.oid::text || '_'::text) || r.oid::text) || '_'::text) || a.attnum::text) || '_not_null'::text)::information_schema.sql_identifier AS constraint_name, a.attname FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r'::\"char\" AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE'::text) OR has_table_privilege(r.oid, 'SELECT'::text) OR has_table_privilege(r.oid, 'INSERT'::text) OR has_table_privilege(r.oid, 'UPDATE'::text) OR has_table_privilege(r.oid, 'DELETE'::text) OR has_table_privilege(r.oid, 'REFERENCES'::text) OR has_table_privilege(r.oid, 'TRIGGER'::text)) AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string AND (((((nr.oid::text || '_'::text) || r.oid::text) || '_'::text) || a.attnum::text) || '_not_null'::text) = ##name2::string",

	/* I_STMT_CHECK_COLUMN_USAGE_ALL */
	"SELECT current_database()::information_schema.sql_identifier AS table_catalog, nr.nspname::information_schema.sql_identifier AS table_schema, r.relname::information_schema.sql_identifier AS table_name, c.conname::information_schema.sql_identifier AS constraint_name,a.attname FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r, pg_attribute a, (SELECT sc.oid, information_schema._pg_expandarray (sc.conkey) as x FROM pg_constraint sc WHERE sc.contype = 'c') ss WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r'::\"char\" AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE'::text) OR has_table_privilege(r.oid, 'INSERT'::text) OR has_table_privilege(r.oid, 'UPDATE'::text) OR has_table_privilege(r.oid, 'DELETE'::text) OR has_table_privilege(r.oid, 'REFERENCES'::text) OR has_table_privilege(r.oid, 'TRIGGER'::text)) AND c.contype = 'c' AND ss.oid = c.oid AND a.attrelid = r.oid AND a.attnum = (ss.x).x "
	"UNION SELECT current_database()::information_schema.sql_identifier AS table_catalog, nr.nspname::information_schema.sql_identifier AS table_schema, r.relname::information_schema.sql_identifier AS table_name, (((((nr.oid::text || '_'::text) || r.oid::text) || '_'::text) || a.attnum::text) || '_not_null'::text)::information_schema.sql_identifier AS constraint_name, a.attname FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r'::\"char\" AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE'::text) OR has_table_privilege(r.oid, 'SELECT'::text) OR has_table_privilege(r.oid, 'INSERT'::text) OR has_table_privilege(r.oid, 'UPDATE'::text) OR has_table_privilege(r.oid, 'DELETE'::text) OR has_table_privilege(r.oid, 'REFERENCES'::text) OR has_table_privilege(r.oid, 'TRIGGER'::text))",

	/* I_STMT_UDT */
	"SELECT pg_catalog.current_database() as cat, n.nspname, t.typname, 'gchararray', pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (n.nspname || '.', '') || t.typname END, coalesce (n.nspname || '.', '') || t.typname, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, o.rolname FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n , pg_authid o WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (t.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid)) AND o.oid=t.typowner AND pg_catalog.current_database() = ##cat::string AND n.nspname = ##schema::string",

	/* I_STMT_UDT_ALL */
	"SELECT pg_catalog.current_database(), n.nspname, t.typname, 'gchararray', pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (n.nspname || '.', '') || t.typname END, coalesce (n.nspname || '.', '') || t.typname, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, o.rolname FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n , pg_authid o WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (t.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid)) AND o.oid=t.typowner",

	/* I_STMT_UDT_COLUMNS */
	"select pg_catalog.current_database(), n.nspname, udt.typname, a.attname, a.attnum, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum ELSE NULL END, information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL , NULL, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid) WHERE udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid) AND pg_catalog.current_database() = ##cat::string AND n.nspname = ##schema::string AND udt.typname = ##name::string",

	/* I_STMT_UDT_COLUMNS_ALL */
	"select pg_catalog.current_database(), n.nspname, udt.typname, a.attname, a.attnum, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum ELSE NULL END, information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL , NULL, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid) WHERE udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid)",

	/* I_STMT_DOMAINS */
	"SELECT pg_catalog.current_database(), nt.nspname, t.typname, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN NULL ELSE coalesce (nbt.nspname || '.', '') || bt.typname END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(t.typbasetype, t.typtypmod), information_schema._pg_char_octet_length(t.typbasetype, t.typtypmod),  NULL, NULL, NULL, NULL, NULL, NULL,  information_schema._pg_numeric_precision(t.typbasetype, t.typtypmod), information_schema._pg_numeric_scale(t.typbasetype, t.typtypmod), t.typdefault, pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (nt.nspname || '.', '') || t.typname END, coalesce (nt.nspname || '.', '') || t.typname, FALSE, o.rolname FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt, pg_authid o WHERE t.typnamespace = nt.oid AND t.typbasetype = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND o.oid=t.typowner AND pg_catalog.current_database() = ##cat::string AND nt.nspname = ##schema::string", 

	/* I_STMT_DOMAINS_ALL */
	"SELECT pg_catalog.current_database(), nt.nspname, t.typname, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN NULL ELSE coalesce (nbt.nspname || '.', '') || bt.typname END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(t.typbasetype, t.typtypmod), information_schema._pg_char_octet_length(t.typbasetype, t.typtypmod),  NULL, NULL, NULL, NULL, NULL, NULL,  information_schema._pg_numeric_precision(t.typbasetype, t.typtypmod), information_schema._pg_numeric_scale(t.typbasetype, t.typtypmod), t.typdefault, pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (nt.nspname || '.', '') || t.typname END, coalesce (nt.nspname || '.', '') || t.typname, FALSE, o.rolname FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt, pg_authid o WHERE t.typnamespace = nt.oid AND t.typbasetype = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND o.oid=t.typowner",

	/* I_STMT_DOMAINS_CONSTRAINTS */
	"SELECT constraint_catalog, constraint_schema, constraint_name, domain_catalog, domain_schema, domain_name, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.domain_constraints WHERE domain_catalog = ##cat::string AND domain_schema = ##schema::string AND domain_name = ##name::string",

	/* I_STMT_DOMAINS_CONSTRAINTS_ALL */
	"SELECT constraint_catalog, constraint_schema, constraint_name, domain_catalog, domain_schema, domain_name, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.domain_constraints",

	/* I_STMT_VIEWS_COLUMNS */
	"SELECT view_catalog, view_schema, view_name, table_catalog, table_schema, table_name, column_name FROM information_schema.view_column_usage WHERE view_catalog = ##cat::string AND view_schema = ##schema::string AND view_name = ##name::string",

	/* I_STMT_VIEWS_COLUMNS_ALL */
	"SELECT view_catalog, view_schema, view_name, table_catalog, table_schema, table_name, column_name FROM information_schema.view_column_usage",

	/* I_STMT_TRIGGERS */
	"SELECT current_database()::information_schema.sql_identifier, n.nspname::information_schema.sql_identifier, t.tgname::information_schema.sql_identifier, em.text::information_schema.character_data, current_database()::information_schema.sql_identifier, n.nspname::information_schema.sql_identifier, c.relname::information_schema.sql_identifier, \"substring\"(pg_get_triggerdef(t.oid), \"position\"(\"substring\"(pg_get_triggerdef(t.oid), 48), 'EXECUTE PROCEDURE'::text) + 47)::information_schema.character_data AS action_statement, CASE WHEN (t.tgtype::integer & 1) = 1 THEN 'ROW'::text ELSE 'STATEMENT'::text END::information_schema.character_data AS action_orientation, CASE WHEN (t.tgtype::integer & 2) = 2 THEN 'BEFORE'::text ELSE 'AFTER'::text END::information_schema.character_data AS condition_timing, pg_catalog.obj_description(t.oid), t.tgname, t.tgname FROM pg_namespace n, pg_class c, pg_trigger t, (( SELECT 4, 'INSERT' UNION ALL SELECT 8, 'DELETE') UNION ALL SELECT 16, 'UPDATE') em(num, text) WHERE n.oid = c.relnamespace AND c.oid = t.tgrelid AND (t.tgtype::integer & em.num) <> 0 AND NOT t.tgisconstraint AND NOT pg_is_other_temp_schema(n.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text)) AND current_database() = ##cat::string AND n.nspname = ##schema::string AND c.relname = ##name::string",
	
	/* I_STMT_TRIGGERS_ALL */
	"SELECT current_database()::information_schema.sql_identifier, n.nspname::information_schema.sql_identifier, t.tgname::information_schema.sql_identifier, em.text::information_schema.character_data, current_database()::information_schema.sql_identifier, n.nspname::information_schema.sql_identifier, c.relname::information_schema.sql_identifier, \"substring\"(pg_get_triggerdef(t.oid), \"position\"(\"substring\"(pg_get_triggerdef(t.oid), 48), 'EXECUTE PROCEDURE'::text) + 47)::information_schema.character_data AS action_statement, CASE WHEN (t.tgtype::integer & 1) = 1 THEN 'ROW'::text ELSE 'STATEMENT'::text END::information_schema.character_data AS action_orientation, CASE WHEN (t.tgtype::integer & 2) = 2 THEN 'BEFORE'::text ELSE 'AFTER'::text END::information_schema.character_data AS condition_timing, pg_catalog.obj_description(t.oid), t.tgname, t.tgname FROM pg_namespace n, pg_class c, pg_trigger t, (( SELECT 4, 'INSERT' UNION ALL SELECT 8, 'DELETE') UNION ALL SELECT 16, 'UPDATE') em(num, text) WHERE n.oid = c.relnamespace AND c.oid = t.tgrelid AND (t.tgtype::integer & em.num) <> 0 AND NOT t.tgisconstraint AND NOT pg_is_other_temp_schema(n.oid) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'DELETE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text) OR has_table_privilege(c.oid, 'TRIGGER'::text))",

	/* I_STMT_EL_TYPES_COL */
	"SELECT 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum, current_database(), nc.nspname, c.relname, 'TABLE_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_attribute a, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typelem = bt.oid WHERE a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r'::\"char\", 'v'::\"char\"])) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text)) AND t.typelem <> 0::oid AND t.typlen = -1 AND 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum = ##name::string",

	/* I_STMT_EL_TYPES_DOM */
	"SELECT 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname, current_database(), nt.nspname, t.typname, 'DOMAIN', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt WHERE t.typnamespace = nt.oid AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND t.typlen = -1 AND 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname = ##name::string",

	/* I_STMT_EL_TYPES_UDT */
	"SELECT 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum, pg_catalog.current_database(), n.nspname, udt.typname, 'UDT_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid), pg_type bt, pg_namespace nbt where udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid) AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typlen = -1 AND 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum = ##name::string",

	/* I_STMT_EL_TYPES_ROUT_PAR */
	"SELECT 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname::text || '_'::text) || ss.p_oid::text) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname::text || '_'::text) || ss.p_oid::text), 'ROUTINE_PAR', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_type bt, pg_namespace nbt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text))) ss WHERE t.oid = (ss.x).x AND bt.oid= t.typelem AND bt.typnamespace = nbt.oid AND t.typelem <> 0::oid AND t.typlen = -1 AND 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname::text || '_'::text) || ss.p_oid::text) || '.' || (ss.x).n = ##name::string", 

	/* I_STMT_EL_TYPES_ROUT_COL */
	"SELECT 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname::text || '_'::text) || ss.p_oid::text) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname::text || '_'::text) || ss.p_oid::text), 'ROUTINE_COL', CASE WHEN at.typelem <> 0::oid AND at.typlen = -1 THEN 'array_spec' ELSE coalesce (ant.nspname || '.', '') || at.typname END, CASE WHEN at.typelem <> 0::oid AND at.typlen = -1 THEN 'ARR' || at.typelem ELSE NULL END, NULL, NULL FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text))) ss, pg_type at, pg_namespace ant WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND at.oid = t.typelem AND at.typnamespace = ant.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b') AND 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname::text || '_'::text) || ss.p_oid::text) || '.' || (ss.x).n = ##name::string",

	/* I_STMT_EL_TYPES_ALL */
	"SELECT 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum, pg_catalog.current_database(), n.nspname, udt.typname, 'UDT_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid), pg_type bt, pg_namespace nbt where udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid) AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typlen = -1 "
	"UNION SELECT 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname, current_database(), nt.nspname, t.typname, 'DOMAIN', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt WHERE t.typnamespace = nt.oid AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND t.typlen = -1 "
	"UNION SELECT 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum, current_database(), nc.nspname, c.relname, 'TABLE_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_attribute a, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typelem = bt.oid WHERE a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r'::\"char\", 'v'::\"char\"])) AND (pg_has_role(c.relowner, 'USAGE'::text) OR has_table_privilege(c.oid, 'SELECT'::text) OR has_table_privilege(c.oid, 'INSERT'::text) OR has_table_privilege(c.oid, 'UPDATE'::text) OR has_table_privilege(c.oid, 'REFERENCES'::text)) AND t.typelem <> 0::oid AND t.typlen = -1 "
	"UNION SELECT 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname::text || '_'::text) || ss.p_oid::text) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname::text || '_'::text) || ss.p_oid::text), 'ROUTINE_PAR', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_type bt, pg_namespace nbt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text))) ss WHERE t.oid = (ss.x).x AND bt.oid= t.typelem AND bt.typnamespace = nbt.oid AND t.typelem <> 0::oid AND t.typlen = -1 "
	"UNION SELECT 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname::text || '_'::text) || ss.p_oid::text) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname::text || '_'::text) || ss.p_oid::text), 'ROUTINE_COL', CASE WHEN at.typelem <> 0::oid AND at.typlen = -1 THEN 'array_spec' ELSE coalesce (ant.nspname || '.', '') || at.typname END, CASE WHEN at.typelem <> 0::oid AND at.typlen = -1 THEN 'ARR' || at.typelem ELSE NULL END, NULL, NULL FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text))) ss, pg_type at, pg_namespace ant WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND at.oid = t.typelem AND at.typnamespace = ant.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b')",

	/* I_STMT_ROUTINES_ALL */
	"SELECT current_database()::information_schema.sql_identifier, n.nspname::information_schema.sql_identifier, ((p.proname::text || '_'::text) || p.oid::text)::information_schema.sql_identifier, current_database()::information_schema.sql_identifier, n.nspname::information_schema.sql_identifier, p.proname::information_schema.sql_identifier, CASE WHEN p.proisagg THEN 'AGGREGATE' ELSE 'FUNCTION' END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || n.nspname || '.' || p.proname || '.' || p.oid ELSE coalesce (nt.nspname || '.', '') || t.typname END AS rettype, p.proretset, p.pronargs::int, CASE WHEN l.lanname = 'sql'::name THEN 'SQL'::text ELSE 'EXTERNAL'::text END, CASE WHEN pg_has_role(p.proowner, 'USAGE'::text) THEN p.prosrc ELSE NULL::text END, CASE WHEN l.lanname = 'c'::name THEN p.prosrc ELSE NULL::text END, upper(l.lanname::text)::information_schema.character_data AS external_language, 'GENERAL'::character varying::information_schema.character_data AS parameter_style, CASE WHEN p.provolatile = 'i' THEN TRUE ELSE FALSE END, 'MODIFIES'::character varying::information_schema.character_data AS sql_data_access, CASE WHEN p.proisstrict THEN TRUE ELSE FALSE END, pg_catalog.obj_description(p.oid), CASE WHEN pg_catalog.pg_function_is_visible(p.oid) IS TRUE THEN p.proname ELSE coalesce (n.nspname || '.', '') || p.proname END, coalesce (n.nspname || '.', '') || p.proname, o.rolname FROM pg_namespace n, pg_proc p, pg_language l, pg_type t, pg_namespace nt, pg_authid o WHERE n.oid = p.pronamespace AND p.prolang = l.oid AND p.prorettype = t.oid AND t.typnamespace = nt.oid AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text)) AND o.oid=p.proowner",

	/* I_STMT_ROUTINES */
	"SELECT current_database()::information_schema.sql_identifier, n.nspname::information_schema.sql_identifier, ((p.proname::text || '_'::text) || p.oid::text)::information_schema.sql_identifier, current_database()::information_schema.sql_identifier, n.nspname::information_schema.sql_identifier, p.proname::information_schema.sql_identifier, CASE WHEN p.proisagg THEN 'AGGREGATE' ELSE 'FUNCTION' END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || n.nspname || '.' || p.proname || '.' || p.oid ELSE coalesce (nt.nspname || '.', '') || t.typname END AS rettype, p.proretset, p.pronargs::int, CASE WHEN l.lanname = 'sql'::name THEN 'SQL'::text ELSE 'EXTERNAL'::text END, CASE WHEN pg_has_role(p.proowner, 'USAGE'::text) THEN p.prosrc ELSE NULL::text END, CASE WHEN l.lanname = 'c'::name THEN p.prosrc ELSE NULL::text END, upper(l.lanname::text)::information_schema.character_data AS external_language, 'GENERAL'::character varying::information_schema.character_data AS parameter_style, CASE WHEN p.provolatile = 'i' THEN TRUE ELSE FALSE END, 'MODIFIES'::character varying::information_schema.character_data AS sql_data_access, CASE WHEN p.proisstrict THEN TRUE ELSE FALSE END, pg_catalog.obj_description(p.oid), CASE WHEN pg_catalog.pg_function_is_visible(p.oid) IS TRUE THEN p.proname ELSE coalesce (n.nspname || '.', '') || p.proname END, coalesce (n.nspname || '.', '') || p.proname, o.rolname FROM pg_namespace n, pg_proc p, pg_language l, pg_type t, pg_namespace nt, pg_authid o WHERE current_database()::information_schema.sql_identifier = ##cat::string AND n.nspname::information_schema.sql_identifier = ##schema::string AND n.oid = p.pronamespace AND p.prolang = l.oid AND p.prorettype = t.oid AND t.typnamespace = nt.oid AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text)) AND o.oid=p.proowner",

	/* I_STMT_ROUTINES_ONE */
	"SELECT current_database()::information_schema.sql_identifier, n.nspname::information_schema.sql_identifier, ((p.proname::text || '_'::text) || p.oid::text)::information_schema.sql_identifier, current_database()::information_schema.sql_identifier, n.nspname::information_schema.sql_identifier, p.proname::information_schema.sql_identifier, CASE WHEN p.proisagg THEN 'AGGREGATE' ELSE 'FUNCTION' END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || n.nspname || '.' || p.proname || '.' || p.oid ELSE coalesce (nt.nspname || '.', '') || t.typname END AS rettype, p.proretset, p.pronargs, CASE WHEN l.lanname = 'sql'::name THEN 'SQL'::text ELSE 'EXTERNAL'::text END, CASE WHEN pg_has_role(p.proowner, 'USAGE'::text) THEN p.prosrc ELSE NULL::text END, CASE WHEN l.lanname = 'c'::name THEN p.prosrc ELSE NULL::text END, upper(l.lanname::text)::information_schema.character_data AS external_language, 'GENERAL'::character varying::information_schema.character_data AS parameter_style, CASE WHEN p.provolatile = 'i' THEN TRUE ELSE FALSE END, 'MODIFIES'::character varying::information_schema.character_data AS sql_data_access, CASE WHEN p.proisstrict THEN TRUE ELSE FALSE END, pg_catalog.obj_description(p.oid), CASE WHEN pg_catalog.pg_function_is_visible(p.oid) IS TRUE THEN p.proname ELSE coalesce (n.nspname || '.', '') || p.proname END, coalesce (n.nspname || '.', '') || p.proname, o.rolname FROM pg_namespace n, pg_proc p, pg_language l, pg_type t, pg_namespace nt, pg_authid o WHERE current_database()::information_schema.sql_identifier = ##cat::string AND n.nspname::information_schema.sql_identifier = ##schema::string AND ((p.proname::text || '_'::text) || p.oid::text)::information_schema.sql_identifier = ##name::string AND n.oid = p.pronamespace AND p.prolang = l.oid AND p.prorettype = t.oid AND t.typnamespace = nt.oid AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text)) AND o.oid=p.proowner",

	/* I_STMT_ROUTINE_PAR_ALL */
	"SELECT current_database(), ss.n_nspname, ((ss.proname::text || '_'::text) || ss.p_oid::text), (ss.x).n, CASE WHEN ss.proargmodes IS NULL THEN 'IN'::text WHEN ss.proargmodes[(ss.x).n] = 'i' THEN 'IN' WHEN ss.proargmodes[(ss.x).n] = 'o' THEN 'OUT' WHEN ss.proargmodes[(ss.x).n] = 'b' THEN 'INOUT' ELSE NULL::text END, NULLIF(ss.proargnames[(ss.x).n], ''), CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname::text || '_'::text) || ss.p_oid::text) || '.' || (ss.x).n ELSE NULL END FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text))) ss WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid",

	/* I_STMT_ROUTINE_PAR */
	"SELECT current_database(), ss.n_nspname, ((ss.proname::text || '_'::text) || ss.p_oid::text), (ss.x).n, CASE WHEN ss.proargmodes IS NULL THEN 'IN'::text WHEN ss.proargmodes[(ss.x).n] = 'i' THEN 'IN' WHEN ss.proargmodes[(ss.x).n] = 'o' THEN 'OUT' WHEN ss.proargmodes[(ss.x).n] = 'b' THEN 'INOUT' ELSE NULL::text END, NULLIF(ss.proargnames[(ss.x).n], ''), CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname::text || '_'::text) || ss.p_oid::text) || '.' || (ss.x).n ELSE NULL END FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text))) ss WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND current_database() = ##cat::string AND ss.n_nspname = ##schema::string AND ((ss.proname::text || '_'::text) || ss.p_oid::text) = ##name::string",

	/* I_STMT_ROUTINE_COL_ALL */
	"SELECT current_database(), ss.n_nspname, ((ss.proname::text || '_'::text) || ss.p_oid::text), NULLIF(ss.proargnames[(ss.x).n], ''), (ss.x).n, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname::text || '_'::text) || ss.p_oid::text) || '.' || (ss.x).n ELSE NULL END FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text))) ss WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b') ORDER BY 1, 2, 3, 4, 5",

	/* I_STMT_ROUTINE_COL */
	"SELECT current_database(), ss.n_nspname, ((ss.proname::text || '_'::text) || ss.p_oid::text), NULLIF(ss.proargnames[(ss.x).n], ''), (ss.x).n, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0::oid AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname::text || '_'::text) || ss.p_oid::text) || '.' || (ss.x).n ELSE NULL END FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE'::text) OR has_function_privilege(p.oid, 'EXECUTE'::text))) ss WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b') AND current_database() = ##cat::string AND ss.n_nspname = ##schema::string AND ((ss.proname::text || '_'::text) || ss.p_oid::text) = ##name::string ORDER BY 1, 2, 3, 4, 5"

};

/*
 * global static values, and
 * predefined statements' GdaStatement, all initialized in _gda_postgres_provider_meta_init()
 */
static GdaStatement **internal_stmt;
static GdaSqlParser *internal_parser = NULL;
static GdaSet       *i_set;

/*
 * Meta initialization
 */
void
_gda_postgres_provider_meta_init (GdaServerProvider *provider)
{
	static GStaticMutex init_mutex = G_STATIC_MUTEX_INIT;
	InternalStatementItem i;

	g_static_mutex_lock (&init_mutex);

        internal_parser = gda_server_provider_internal_get_parser (provider);
        internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
        for (i = I_STMT_CATALOG; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
                internal_stmt[i] = gda_sql_parser_parse_string (internal_parser, internal_sql[i], NULL, NULL);
                if (!internal_stmt[i])
                        g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
        }

	i_set = gda_set_new_inline (4, "cat", G_TYPE_STRING, "", 
				    "name", G_TYPE_STRING, "",
				    "schema", G_TYPE_STRING, "",
				    "name2", G_TYPE_STRING, "");

	g_static_mutex_unlock (&init_mutex);
}

gboolean
_gda_postgres_meta__info (GdaServerProvider *prov, GdaConnection *cnc, 
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
_gda_postgres_meta__btypes (GdaServerProvider *prov, GdaConnection *cnc, 
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
		value = gda_data_model_get_value_at (model, 6, i, error);
		if (!value) {
			retval = FALSE;
			break;
		}
		
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
_gda_postgres_meta__udt (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_UDT_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_udt (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *udt_catalog, const GValue *udt_schema)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), udt_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), udt_schema, error))
		return FALSE;
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_UDT], i_set, error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__udt_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_UDT_COLUMNS_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_udt_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "cat"), udt_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), udt_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), udt_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_UDT_COLUMNS], i_set, error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__enums (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* nothing yet, use pg_enum if >= 8.3 */
	return TRUE;
}

gboolean
_gda_postgres_meta_enums (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error,
			  const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	/* nothing yet, use pg_enum if >= 8.3 */
	return TRUE;
}

gboolean
_gda_postgres_meta__domains (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_DOMAINS_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_domains (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *domain_catalog, const GValue *domain_schema)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), domain_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), domain_schema, error))
		return FALSE;
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_DOMAINS], i_set, error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
				     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_DOMAINS_CONSTRAINTS_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
				    GdaMetaStore *store, GdaMetaContext *context, GError **error,
				    const GValue *domain_catalog, const GValue *domain_schema, 
				    const GValue *domain_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), domain_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), domain_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), domain_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_DOMAINS_CONSTRAINTS], i_set, error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__el_types (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_EL_TYPES_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_el_types (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *specific_name)
{
	const gchar *cstr;
	GdaDataModel *model;
	gboolean retval;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), specific_name, error))
		return FALSE;
	cstr = g_value_get_string (specific_name);
	if (*cstr == 'C') {
		/* check correct postgres server version */
		PostgresConnectionData *cdata;
		cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
		if (!cdata)
			return FALSE;
		if (cdata->version_float < 8.2) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
				     "%s", _("PostgreSQL version 8.2.0 at least is required"));
			return FALSE;
		}
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_EL_TYPES_COL], i_set, 
								 error);
	}
	else if (*cstr == 'D')
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_EL_TYPES_DOM], i_set, 
								 error);
	else if (*cstr == 'U')
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_EL_TYPES_UDT], i_set, 
								 error);
	else if (!strcmp (cstr, "ROUTINE_PAR"))
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_EL_TYPES_ROUT_PAR], i_set, 
								 error);
	else if (!strcmp (cstr, "ROUTINE_COL"))
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_EL_TYPES_ROUT_COL], i_set, 
								 error);
	else
		TO_IMPLEMENT;
	
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta__collations (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* nothing to do */
	return TRUE;
}

gboolean
_gda_postgres_meta_collations (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error,
			       const GValue *collation_catalog, const GValue *collation_schema, 
			       const GValue *collation_name_n)
{
	/* nothing to do */
	return TRUE;
}

gboolean
_gda_postgres_meta__character_sets (GdaServerProvider *prov, GdaConnection *cnc, 
				    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	/* nothing to do */
	return TRUE;
}

gboolean
_gda_postgres_meta_character_sets (GdaServerProvider *prov, GdaConnection *cnc, 
				   GdaMetaStore *store, GdaMetaContext *context, GError **error,
				   const GValue *chset_catalog, const GValue *chset_schema, 
				   const GValue *chset_name_n)
{
	/* nothing to do */
	return TRUE;
}

gboolean
_gda_postgres_meta__schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_SCHEMAS_ALL], i_set, error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
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

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), catalog_name, error))
		return FALSE;
	if (!schema_name_n) {
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_SCHEMAS], i_set, error);
		if (!model)
			return FALSE;
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	}
	else {
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), schema_name_n, error))
			return FALSE;
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
_gda_postgres_meta__tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
				  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *tables_model, *views_model;
	gboolean retval = TRUE;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	tables_model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TABLES_ALL], i_set, error);
	if (!tables_model)
		return FALSE;
	views_model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_VIEWS_ALL], i_set, error);
	if (!views_model) {
		g_object_unref (tables_model);
		return FALSE;
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
_gda_postgres_meta_tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error, 
				 const GValue *table_catalog, const GValue *table_schema, const GValue *table_name_n)
{
	GdaDataModel *tables_model, *views_model;
	gboolean retval = TRUE;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
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
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name_n, error))
			return FALSE;
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

gboolean _gda_postgres_meta__columns (GdaServerProvider *prov, GdaConnection *cnc, 
				      GdaMetaStore *store, GdaMetaContext *context, GError **error)
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
	model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_COLUMNS_ALL], i_set, 
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
		value = gda_data_model_get_value_at (model, 24, i, error);
		if (!value) {
			retval = FALSE;
			break;
		}
		
		type = _gda_postgres_type_oid_to_gda (cdata, g_value_get_int64 (value));
		if (type != G_TYPE_STRING) {
			GValue *v;
			g_value_set_string (v = gda_value_new (G_TYPE_STRING), g_type_name (type));
			retval = gda_data_model_set_value_at (proxy, 9, i, v, error);
			gda_value_free (v);
			if (!retval)
				break;
		}
	}

	/* modify meta store with @proxy */
	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, proxy, error);
	g_object_unref (proxy);
	g_object_unref (model);

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

	/* check correct postgres server version */
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	/* use a prepared statement for the "base" model */
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
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
		value = gda_data_model_get_value_at (model, 24, i, error);
		if (!value) {
			retval = FALSE;
			break;
		}

		type = _gda_postgres_type_oid_to_gda (cdata, g_value_get_int64 (value));
		if (type != G_TYPE_STRING) {
			GValue *v;
			g_value_set_string (v = gda_value_new (G_TYPE_STRING), g_type_name (type));
			retval = gda_data_model_set_value_at (proxy, 9, i, v, error);
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
_gda_postgres_meta__view_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_VIEWS_COLUMNS_ALL], i_set, error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
	
	return retval;
}

gboolean 
_gda_postgres_meta_view_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *view_catalog, const GValue *view_schema, 
			      const GValue *view_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), view_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), view_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), view_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_VIEWS_COLUMNS], i_set, error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
				     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TABLES_CONSTRAINTS_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
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

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;

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
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name_n, error))
			return FALSE;
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
_gda_postgres_meta__constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
				     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_REF_CONSTRAINTS_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
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

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name, error))
		return FALSE;
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
_gda_postgres_meta__key_columns (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_KEY_COLUMN_USAGE_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
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

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name, error))
		return FALSE;
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

gboolean
_gda_postgres_meta__check_columns (GdaServerProvider *prov, GdaConnection *cnc, 
				   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_CHECK_COLUMN_USAGE_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_check_columns (GdaServerProvider *prov, GdaConnection *cnc, 
				  GdaMetaStore *store, GdaMetaContext *context, GError **error,
				  const GValue *table_catalog, const GValue *table_schema, 
				  const GValue *table_name, const GValue *constraint_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_CHECK_COLUMN_USAGE], i_set, 
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
_gda_postgres_meta__triggers (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TRIGGERS_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_triggers (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *table_catalog, const GValue *table_schema, 
			     const GValue *table_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TRIGGERS], i_set, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta__routines (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_ROUTINES_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_routines (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *routine_catalog, const GValue *routine_schema, 
			     const GValue *routine_name_n)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), routine_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), routine_schema, error))
		return FALSE;
	if (routine_name_n) {
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), routine_name_n, error))
			return FALSE;
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_ROUTINES_ONE], i_set, 
								 error);
	}
	else 
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_ROUTINES], i_set, 
								 error);

	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta__routine_col (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model, *proxy;
	gint ordinal_pos, i, nrows;
	const GValue *spname = NULL;
	gboolean retval;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_ROUTINE_COL_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;

	/* use a proxy to customize @model */
	proxy = (GdaDataModel*) gda_data_proxy_new (model);
	gda_data_proxy_set_sample_size ((GdaDataProxy*) proxy, 0);
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *cvalue;
		GValue *v;

		cvalue = gda_data_model_get_value_at (model, 2, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}
		if (!spname || gda_value_compare (spname, cvalue))
			/* reinit ordinal position */
			ordinal_pos = 1;
		spname = cvalue;

		g_value_set_int ((v = gda_value_new (G_TYPE_INT)), ordinal_pos++);
		retval = gda_data_model_set_value_at (proxy, 4, i, v, error);
		gda_value_free (v);
		if (!retval)
			break;
	}

	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, proxy, error);
	g_object_unref (model);
	g_object_unref (proxy);
		
	return retval;
}

gboolean
_gda_postgres_meta_routine_col (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *rout_catalog, const GValue *rout_schema, 
				const GValue *rout_name)
{
GdaDataModel *model, *proxy;
	gint ordinal_pos, i, nrows;
	const GValue *spname = NULL;
	gboolean retval;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), rout_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), rout_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), rout_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_ROUTINE_COL], i_set, 
							 error);
	if (!model)
		return FALSE;

	/* use a proxy to customize @model */
	proxy = (GdaDataModel*) gda_data_proxy_new (model);
	gda_data_proxy_set_sample_size ((GdaDataProxy*) proxy, 0);
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *cvalue;
		GValue *v;

		cvalue = gda_data_model_get_value_at (model, 2, i, error);
		if (!cvalue) {
			retval = FALSE;
			break;
		}

		if (!spname || gda_value_compare (spname, cvalue))
			/* reinit ordinal position */
			ordinal_pos = 1;
		spname = cvalue;

		g_value_set_int ((v = gda_value_new (G_TYPE_INT)), ordinal_pos++);
		retval = gda_data_model_set_value_at (proxy, 4, i, v, error);
		gda_value_free (v);
		if (!retval)
			break;
	}

	if (retval)
		retval = gda_meta_store_modify_with_context (store, context, proxy, error);
	g_object_unref (model);
	g_object_unref (proxy);
		
	return retval;
}

gboolean
_gda_postgres_meta__routine_par (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_ROUTINE_PAR_ALL], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_routine_par (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *rout_catalog, const GValue *rout_schema, 
				const GValue *rout_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	/* check correct postgres server version */
	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_float < 8.2) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     "%s", _("PostgreSQL version 8.2.0 at least is required"));
		return FALSE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), rout_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), rout_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), rout_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_ROUTINE_PAR], i_set, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}
