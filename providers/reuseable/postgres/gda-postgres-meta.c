/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2012 Daniel Espinosa <despinosa@src.gnome.org>
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
#include "gda-postgres-meta.h"
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-data-proxy.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-connection-private.h>
#include <libgda/providers-support/gda-meta-column-types.h>
#include <libgda/gda-debug-macros.h>

#include "gda-postgres-reuseable.h"
#include "gda-postgres-parser.h"

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
	I_STMT_ROUTINE_COL,
	I_STMT_INDEXES,
	I_STMT_INDEXES_ALL,
	I_STMT_INDEXES_NAMED,
	I_STMT_INDEXES_COLUMNS_GET_ALL_INDEXES,
	I_STMT_INDEXES_COLUMNS_GET_NAMED_INDEXES,
	I_STMT_INDEXES_COLUMNS_FOR_INDEX
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	/* I_STMT_CATALOG */
	"SELECT pg_catalog.current_database()",

	/* I_STMT_BTYPES */
	"SELECT t.typname, 'pg_catalog.' || t.typname, 'gchararray', pg_catalog.obj_description(t.oid), NULL, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN typtype = 'p' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, CAST (t.oid AS varchar) FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (typtype='b' OR typtype='p') AND typelem = 0",

	/* I_STMT_SCHEMAS */
	"SELECT current_database() AS catalog_name, n.nspname AS schema_name, u.rolname AS schema_owner, CASE WHEN n.nspname ~ '^pg_' THEN TRUE WHEN n.nspname = 'information_schema' THEN TRUE ELSE FALSE END, CASE WHEN n.nspname = 'public' THEN TRUE ELSE FALSE END AS schema_default FROM pg_namespace n, pg_roles u WHERE n.nspowner = u.oid AND current_database() = ##cat::string",

	/* I_STMT_SCHEMAS_ALL */
	"SELECT current_database() AS catalog_name, n.nspname AS schema_name, u.rolname AS schema_owner, CASE WHEN n.nspname ~ '^pg_' THEN TRUE WHEN n.nspname = 'information_schema' THEN TRUE ELSE FALSE END, CASE WHEN n.nspname = 'public' THEN TRUE ELSE FALSE END AS schema_default FROM pg_namespace n, pg_roles u WHERE n.nspowner = u.oid",

	/* I_STMT_SCHEMA_NAMED */
	"SELECT current_database() AS catalog_name, n.nspname AS schema_name, u.rolname AS schema_owner, CASE WHEN n.nspname ~ '^pg_' THEN TRUE WHEN n.nspname = 'information_schema' THEN TRUE ELSE FALSE END, CASE WHEN n.nspname = 'public' THEN TRUE ELSE FALSE END AS schema_default FROM pg_namespace n, pg_roles u WHERE n.nspowner = u.oid AND current_database() = ##cat::string AND n.nspname = ##name::string",

	/* I_STMT_TABLES */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY' WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL END AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_roles o WHERE CASE WHEN ##cat::string IS NULL THEN TRUE ELSE current_database() = ##cat::string END AND CASE WHEN ##schema::string IS NULL THEN TRUE ELSE nc.nspname = ##schema::string END AND c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER')) AND o.oid=c.relowner",

	/* I_STMT_TABLES_ALL */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY' WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL END AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_roles o WHERE c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER')) AND o.oid=c.relowner",

	/* I_STMT_TABLE_NAMED */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY' WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL END AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_roles o WHERE CASE WHEN ##cat::string IS NULL THEN TRUE ELSE current_database() = ##cat::string END AND CASE WHEN ##schema::string IS NULL THEN TRUE ELSE nc.nspname = ##schema::string END AND c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER')) AND o.oid=c.relowner AND c.relname = ##name::string",

	/* I_STMT_VIEWS */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE current_database() = ##cat::string AND nc.nspname = ##schema::string AND c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER'))",

	/* I_STMT_VIEWS_ALL */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER'))",

	/* I_STMT_VIEW_NAMED */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE current_database() = ##cat::string AND nc.nspname = ##schema::string AND c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER')) AND c.relname = ##name::string",

	/* I_STMT_COLUMNS_OF_TABLE */
	"SELECT current_database(), nc.nspname, c.relname, a.attname, a.attnum, pg_get_expr(ad.adbin, ad.adrelid), CASE WHEN a.attnotnull OR t.typtype = 'd' AND t.typnotnull THEN FALSE ELSE TRUE END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL, NULL, NULL, NULL, NULL, CASE WHEN pg_get_expr(ad.adbin, ad.adrelid) LIKE 'nextval(%' THEN '" GDA_EXTRA_AUTO_INCREMENT "' ELSE NULL END, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.col_description(c.oid, a.attnum), CAST (t.oid AS varchar) FROM pg_attribute a LEFT JOIN pg_attrdef ad ON a.attrelid = ad.adrelid AND a.attnum = ad.adnum, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typtype = 'd' AND t.typbasetype = bt.oid WHERE current_database() = ##cat::string AND nc.nspname = ##schema::string AND c.relname = ##name::string AND a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'REFERENCES'))",

	/* I_STMT_COLUMNS_ALL */
	"SELECT current_database(), nc.nspname, c.relname, a.attname, a.attnum, pg_get_expr(ad.adbin, ad.adrelid), CASE WHEN a.attnotnull OR t.typtype = 'd' AND t.typnotnull THEN FALSE ELSE TRUE END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL, NULL, NULL, NULL, NULL, CASE WHEN pg_get_expr(ad.adbin, ad.adrelid) LIKE 'nextval(%' THEN '" GDA_EXTRA_AUTO_INCREMENT "' ELSE NULL END, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.col_description(c.oid, a.attnum), CAST (t.oid AS varchar) FROM pg_attribute a LEFT JOIN pg_attrdef ad ON a.attrelid = ad.adrelid AND a.attnum = ad.adnum, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typtype = 'd' AND t.typbasetype = bt.oid WHERE a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'REFERENCES'))",

	/* I_STMT_TABLES_CONSTRAINTS */
	"SELECT current_database() AS constraint_catalog, nc.nspname AS constraint_schema, c.conname AS constraint_name, current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, CASE c.contype WHEN 'c' THEN 'CHECK' WHEN 'f' THEN 'FOREIGN KEY' WHEN 'p' THEN 'PRIMARY KEY' WHEN 'u' THEN 'UNIQUE' ELSE NULL END AS constraint_type, CASE c.contype WHEN 'c' THEN c.consrc ELSE NULL END, CASE WHEN c.condeferrable THEN TRUE ELSE FALSE END AS is_deferrable, CASE WHEN c.condeferred THEN TRUE ELSE FALSE END AS initially_deferred FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string "
	"UNION SELECT current_database() AS constraint_catalog, nr.nspname AS constraint_schema, (((((nr.oid || '_') || r.oid) || '_') || a.attnum) || '_not_null') AS constraint_name, current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, 'CHECK' AS constraint_type, a.attname || ' IS NOT NULL', FALSE AS is_deferrable, FALSE AS initially_deferred FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'SELECT') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string",

	/* I_STMT_TABLES_CONSTRAINTS_ALL */
	"SELECT current_database() AS constraint_catalog, nc.nspname AS constraint_schema, c.conname AS constraint_name, current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, CASE c.contype WHEN 'c' THEN 'CHECK' WHEN 'f' THEN 'FOREIGN KEY' WHEN 'p' THEN 'PRIMARY KEY' WHEN 'u' THEN 'UNIQUE' ELSE NULL END AS constraint_type, CASE c.contype WHEN 'c' THEN c.consrc ELSE NULL END, CASE WHEN c.condeferrable THEN TRUE ELSE FALSE END AS is_deferrable, CASE WHEN c.condeferred THEN TRUE ELSE FALSE END AS initially_deferred FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) "
	"UNION SELECT current_database() AS constraint_catalog, nr.nspname AS constraint_schema, (((((nr.oid || '_') || r.oid) || '_') || a.attnum) || '_not_null') AS constraint_name, current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, 'CHECK' AS constraint_type, a.attname || ' IS NOT NULL', FALSE AS is_deferrable, FALSE AS initially_deferred FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'SELECT') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER'))",

	/* I_STMT_TABLES_CONSTRAINT_NAMED */
	"SELECT constraint_catalog, constraint_schema, constraint_name, table_catalog, table_schema, table_name, constraint_type, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.table_constraints WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string",

	/* I_STMT_REF_CONSTRAINTS */
	"SELECT current_database() AS table_catalog, nt.nspname AS table_schema, t.relname AS table_name, c.conname AS constraint_name, current_database() AS ref_table_catalog, nref.nspname AS ref_table_schema, ref.relname AS ref_table_name, pkc.conname AS ref_constraint_name, CASE c.confmatchtype WHEN 'f' THEN 'FULL' WHEN 'p' THEN 'PARTIAL' WHEN 'u' THEN 'NONE' ELSE NULL END AS match_option, CASE c.confupdtype WHEN 'c' THEN 'CASCADE' WHEN 'n' THEN 'SET NULL' WHEN 'd' THEN 'SET DEFAULT' WHEN 'r' THEN 'RESTRICT' WHEN 'a' THEN 'NO ACTION' ELSE NULL END AS update_rule, CASE c.confdeltype WHEN 'c' THEN 'CASCADE' WHEN 'n' THEN 'SET NULL' WHEN 'd' THEN 'SET DEFAULT' WHEN 'r' THEN 'RESTRICT' WHEN 'a' THEN 'NO ACTION' ELSE NULL END AS delete_rule FROM pg_constraint c INNER JOIN pg_class t ON (c.conrelid=t.oid) INNER JOIN pg_namespace nt ON (nt.oid=t.relnamespace) INNER JOIN pg_class ref ON (c.confrelid=ref.oid) INNER JOIN pg_namespace nref ON (nref.oid=ref.relnamespace) INNER JOIN pg_constraint pkc ON (c.confrelid = pkc.conrelid AND information_schema._pg_keysequal(c.confkey, pkc.conkey) AND pkc.contype='p') WHERE c.contype = 'f' AND current_database() = ##cat::string AND nt.nspname = ##schema::string AND t.relname = ##name::string AND c.conname = ##name2::string",

	/* I_STMT_REF_CONSTRAINTS_ALL */
	"SELECT current_database() AS table_catalog, nt.nspname AS table_schema, t.relname AS table_name, c.conname AS constraint_name, current_database() AS ref_table_catalog, nref.nspname AS ref_table_schema, ref.relname AS ref_table_name, pkc.conname AS ref_constraint_name, CASE c.confmatchtype WHEN 'f' THEN 'FULL' WHEN 'p' THEN 'PARTIAL' WHEN 'u' THEN 'NONE' ELSE NULL END AS match_option, CASE c.confupdtype WHEN 'c' THEN 'CASCADE' WHEN 'n' THEN 'SET NULL' WHEN 'd' THEN 'SET DEFAULT' WHEN 'r' THEN 'RESTRICT' WHEN 'a' THEN 'NO ACTION' ELSE NULL END AS update_rule, CASE c.confdeltype WHEN 'c' THEN 'CASCADE' WHEN 'n' THEN 'SET NULL' WHEN 'd' THEN 'SET DEFAULT' WHEN 'r' THEN 'RESTRICT' WHEN 'a' THEN 'NO ACTION' ELSE NULL END AS delete_rule FROM pg_constraint c INNER JOIN pg_class t ON (c.conrelid=t.oid) INNER JOIN pg_namespace nt ON (nt.oid=t.relnamespace) INNER JOIN pg_class ref ON (c.confrelid=ref.oid) INNER JOIN pg_namespace nref ON (nref.oid=ref.relnamespace) INNER JOIN pg_constraint pkc ON (c.confrelid = pkc.conrelid AND information_schema._pg_keysequal(c.confkey, pkc.conkey) AND pkc.contype='p') WHERE c.contype = 'f'",

	/* I_STMT_KEY_COLUMN_USAGE */
	"SELECT table_catalog, table_schema, table_name, constraint_name, column_name, ordinal_position FROM information_schema.key_column_usage WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string",

	/* I_STMT_KEY_COLUMN_USAGE_ALL */
	"SELECT table_catalog, table_schema, table_name, constraint_name, column_name, ordinal_position FROM information_schema.key_column_usage",

	/* I_STMT_CHECK_COLUMN_USAGE */
	"SELECT current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, c.conname AS constraint_name,a.attname FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r, pg_attribute a, (SELECT sc.oid, information_schema._pg_expandarray (sc.conkey) as x FROM pg_constraint sc WHERE sc.contype = 'c') ss WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) AND c.contype = 'c' AND ss.oid = c.oid AND a.attrelid = r.oid AND a.attnum = (ss.x).x AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string AND c.conname = ##name2::string "
	"UNION SELECT current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, (((((nr.oid || '_') || r.oid) || '_') || a.attnum) || '_not_null') AS constraint_name, a.attname FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'SELECT') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string AND (((((nr.oid || '_') || r.oid) || '_') || a.attnum) || '_not_null') = ##name2::string",

	/* I_STMT_CHECK_COLUMN_USAGE_ALL */
	"SELECT current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, c.conname AS constraint_name,a.attname FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r, pg_attribute a, (SELECT sc.oid, information_schema._pg_expandarray (sc.conkey) as x FROM pg_constraint sc WHERE sc.contype = 'c') ss WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) AND c.contype = 'c' AND ss.oid = c.oid AND a.attrelid = r.oid AND a.attnum = (ss.x).x "
	"UNION SELECT current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, (((((nr.oid || '_') || r.oid) || '_') || a.attnum) || '_not_null') AS constraint_name, a.attname FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'SELECT') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER'))",

	/* I_STMT_UDT */
	"SELECT pg_catalog.current_database() as cat, n.nspname, t.typname, 'gchararray', pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (n.nspname || '.', '') || t.typname END, coalesce (n.nspname || '.', '') || t.typname, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, o.rolname FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n , pg_roles o WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (t.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid)) AND o.oid=t.typowner AND pg_catalog.current_database() = ##cat::string AND n.nspname = ##schema::string",

	/* I_STMT_UDT_ALL */
	"SELECT pg_catalog.current_database(), n.nspname, t.typname, 'gchararray', pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (n.nspname || '.', '') || t.typname END, coalesce (n.nspname || '.', '') || t.typname, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, o.rolname FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n , pg_roles o WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (t.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid)) AND o.oid=t.typowner",

	/* I_STMT_UDT_COLUMNS */
	"select pg_catalog.current_database(), n.nspname, udt.typname, a.attname, a.attnum, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum ELSE NULL END, information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL , NULL, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid) WHERE udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid) AND pg_catalog.current_database() = ##cat::string AND n.nspname = ##schema::string AND udt.typname = ##name::string",

	/* I_STMT_UDT_COLUMNS_ALL */
	"select pg_catalog.current_database(), n.nspname, udt.typname, a.attname, a.attnum, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum ELSE NULL END, information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL , NULL, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid) WHERE udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid)",

	/* I_STMT_DOMAINS */
	"SELECT pg_catalog.current_database(), nt.nspname, t.typname, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nbt.nspname || '.', '') || bt.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(t.typbasetype, t.typtypmod), information_schema._pg_char_octet_length(t.typbasetype, t.typtypmod),  NULL, NULL, NULL, NULL, NULL, NULL,  information_schema._pg_numeric_precision(t.typbasetype, t.typtypmod), information_schema._pg_numeric_scale(t.typbasetype, t.typtypmod), t.typdefault, pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (nt.nspname || '.', '') || t.typname END, coalesce (nt.nspname || '.', '') || t.typname, FALSE, o.rolname FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt, pg_roles o WHERE t.typnamespace = nt.oid AND t.typbasetype = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND o.oid=t.typowner AND pg_catalog.current_database() = ##cat::string AND nt.nspname = ##schema::string", 

	/* I_STMT_DOMAINS_ALL */
	"SELECT pg_catalog.current_database(), nt.nspname, t.typname, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nbt.nspname || '.', '') || bt.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(t.typbasetype, t.typtypmod), information_schema._pg_char_octet_length(t.typbasetype, t.typtypmod),  NULL, NULL, NULL, NULL, NULL, NULL,  information_schema._pg_numeric_precision(t.typbasetype, t.typtypmod), information_schema._pg_numeric_scale(t.typbasetype, t.typtypmod), t.typdefault, pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (nt.nspname || '.', '') || t.typname END, coalesce (nt.nspname || '.', '') || t.typname, FALSE, o.rolname FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt, pg_roles o WHERE t.typnamespace = nt.oid AND t.typbasetype = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND o.oid=t.typowner",

	/* I_STMT_DOMAINS_CONSTRAINTS */
	"SELECT constraint_catalog, constraint_schema, constraint_name, domain_catalog, domain_schema, domain_name, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.domain_constraints WHERE domain_catalog = ##cat::string AND domain_schema = ##schema::string AND domain_name = ##name::string",

	/* I_STMT_DOMAINS_CONSTRAINTS_ALL */
	"SELECT constraint_catalog, constraint_schema, constraint_name, domain_catalog, domain_schema, domain_name, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.domain_constraints",

	/* I_STMT_VIEWS_COLUMNS */
	"SELECT view_catalog, view_schema, view_name, table_catalog, table_schema, table_name, column_name FROM information_schema.view_column_usage WHERE view_catalog = ##cat::string AND view_schema = ##schema::string AND view_name = ##name::string",

	/* I_STMT_VIEWS_COLUMNS_ALL */
	"SELECT view_catalog, view_schema, view_name, table_catalog, table_schema, table_name, column_name FROM information_schema.view_column_usage",

	/* I_STMT_TRIGGERS */
	"SELECT current_database(), n.nspname, t.tgname, em.text, current_database(), n.nspname, c.relname, \"substring\"(pg_get_triggerdef(t.oid), \"position\"(\"substring\"(pg_get_triggerdef(t.oid), 48), 'EXECUTE PROCEDURE') + 47) AS action_statement, CASE WHEN (t.tgtype & 1) = 1 THEN 'ROW' ELSE 'STATEMENT' END AS action_orientation, CASE WHEN (t.tgtype & 2) = 2 THEN 'BEFORE' ELSE 'AFTER' END AS condition_timing, pg_catalog.obj_description(t.oid), t.tgname, t.tgname FROM pg_namespace n, pg_class c, pg_trigger t, (( SELECT 4, 'INSERT' UNION ALL SELECT 8, 'DELETE') UNION ALL SELECT 16, 'UPDATE') em(num, text) WHERE n.oid = c.relnamespace AND c.oid = t.tgrelid AND (t.tgtype & em.num) <> 0 AND (t.tgconstraint = 0) AND NOT pg_is_other_temp_schema(n.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER')) AND current_database() = ##cat::string AND n.nspname = ##schema::string AND c.relname = ##name::string",
	
	/* I_STMT_TRIGGERS_ALL */
	"SELECT current_database(), n.nspname, t.tgname, em.text, current_database(), n.nspname, c.relname, \"substring\"(pg_get_triggerdef(t.oid), \"position\"(\"substring\"(pg_get_triggerdef(t.oid), 48), 'EXECUTE PROCEDURE') + 47) AS action_statement, CASE WHEN (t.tgtype & 1) = 1 THEN 'ROW' ELSE 'STATEMENT' END AS action_orientation, CASE WHEN (t.tgtype & 2) = 2 THEN 'BEFORE' ELSE 'AFTER' END AS condition_timing, pg_catalog.obj_description(t.oid), t.tgname, t.tgname FROM pg_namespace n, pg_class c, pg_trigger t, (( SELECT 4, 'INSERT' UNION ALL SELECT 8, 'DELETE') UNION ALL SELECT 16, 'UPDATE') em(num, text) WHERE n.oid = c.relnamespace AND c.oid = t.tgrelid AND (t.tgtype & em.num) <> 0 AND NOT (t.tgconstraint = 0) AND NOT pg_is_other_temp_schema(n.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER'))",

	/* I_STMT_EL_TYPES_COL */
	"SELECT 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum, current_database(), nc.nspname, c.relname, 'TABLE_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_attribute a, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typelem = bt.oid WHERE a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'REFERENCES')) AND t.typelem <> 0 AND t.typlen = -1 AND 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum = ##name::string",

	/* I_STMT_EL_TYPES_DOM */
	"SELECT 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname, current_database(), nt.nspname, t.typname, 'DOMAIN', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt WHERE t.typnamespace = nt.oid AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND t.typlen = -1 AND 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname = ##name::string",

	/* I_STMT_EL_TYPES_UDT */
	"SELECT 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum, pg_catalog.current_database(), n.nspname, udt.typname, 'UDT_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid), pg_type bt, pg_namespace nbt where udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid) AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typlen = -1 AND 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum = ##name::string",

	/* I_STMT_EL_TYPES_ROUT_PAR */
	"SELECT 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), 'ROUTINE_PAR', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_type bt, pg_namespace nbt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE t.oid = (ss.x).x AND bt.oid= t.typelem AND bt.typnamespace = nbt.oid AND t.typelem <> 0 AND t.typlen = -1 AND 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n = ##name::string", 

	/* I_STMT_EL_TYPES_ROUT_COL */
	"SELECT 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), 'ROUTINE_COL', CASE WHEN at.typelem <> 0 AND at.typlen = -1 THEN 'array_spec' ELSE coalesce (ant.nspname || '.', '') || at.typname END, CASE WHEN at.typelem <> 0 AND at.typlen = -1 THEN 'ARR' || at.typelem ELSE NULL END, NULL, NULL FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss, pg_type at, pg_namespace ant WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND at.oid = t.typelem AND at.typnamespace = ant.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b') AND 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n = ##name::string",

	/* I_STMT_EL_TYPES_ALL */
	"SELECT 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum, pg_catalog.current_database(), n.nspname, udt.typname, 'UDT_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid), pg_type bt, pg_namespace nbt where udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid) AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typlen = -1 "
	"UNION SELECT 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname, current_database(), nt.nspname, t.typname, 'DOMAIN', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt WHERE t.typnamespace = nt.oid AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND t.typlen = -1 "
	"UNION SELECT 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum, current_database(), nc.nspname, c.relname, 'TABLE_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_attribute a, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typelem = bt.oid WHERE a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'REFERENCES')) AND t.typelem <> 0 AND t.typlen = -1 "
	"UNION SELECT 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), 'ROUTINE_PAR', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_type bt, pg_namespace nbt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE t.oid = (ss.x).x AND bt.oid= t.typelem AND bt.typnamespace = nbt.oid AND t.typelem <> 0 AND t.typlen = -1 "
	"UNION SELECT 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), 'ROUTINE_COL', CASE WHEN at.typelem <> 0 AND at.typlen = -1 THEN 'array_spec' ELSE coalesce (ant.nspname || '.', '') || at.typname END, CASE WHEN at.typelem <> 0 AND at.typlen = -1 THEN 'ARR' || at.typelem ELSE NULL END, NULL, NULL FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss, pg_type at, pg_namespace ant WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND at.oid = t.typelem AND at.typnamespace = ant.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b')",

	/* I_STMT_ROUTINES_ALL */
	"SELECT current_database(), n.nspname, ((p.proname || '_') || p.oid), current_database(), n.nspname, p.proname, CASE WHEN p.proisagg THEN 'AGGREGATE' ELSE 'FUNCTION' END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || n.nspname || '.' || p.proname || '.' || p.oid ELSE coalesce (nt.nspname || '.', '') || t.typname END AS rettype, p.proretset, p.pronargs, CASE WHEN l.lanname = 'sql' THEN 'SQL' ELSE 'EXTERNAL' END, CASE WHEN pg_has_role(p.proowner, 'USAGE') THEN p.prosrc ELSE NULL END, CASE WHEN l.lanname = 'c' THEN p.prosrc ELSE NULL END, upper(l.lanname) AS external_language, 'GENERAL' AS parameter_style, CASE WHEN p.provolatile = 'i' THEN TRUE ELSE FALSE END, 'MODIFIES' AS sql_data_access, CASE WHEN p.proisstrict THEN TRUE ELSE FALSE END, pg_catalog.obj_description(p.oid), CASE WHEN pg_catalog.pg_function_is_visible(p.oid) IS TRUE THEN p.proname ELSE coalesce (n.nspname || '.', '') || p.proname END, coalesce (n.nspname || '.', '') || p.proname, o.rolname FROM pg_namespace n, pg_proc p, pg_language l, pg_type t, pg_namespace nt, pg_roles o WHERE n.oid = p.pronamespace AND p.prolang = l.oid AND p.prorettype = t.oid AND t.typnamespace = nt.oid AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE')) AND o.oid=p.proowner",

	/* I_STMT_ROUTINES */
	"SELECT current_database(), n.nspname, ((p.proname || '_') || p.oid), current_database(), n.nspname, p.proname, CASE WHEN p.proisagg THEN 'AGGREGATE' ELSE 'FUNCTION' END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || n.nspname || '.' || p.proname || '.' || p.oid ELSE coalesce (nt.nspname || '.', '') || t.typname END AS rettype, p.proretset, p.pronargs, CASE WHEN l.lanname = 'sql' THEN 'SQL' ELSE 'EXTERNAL' END, CASE WHEN pg_has_role(p.proowner, 'USAGE') THEN p.prosrc ELSE NULL END, CASE WHEN l.lanname = 'c' THEN p.prosrc ELSE NULL END, upper(l.lanname) AS external_language, 'GENERAL' AS parameter_style, CASE WHEN p.provolatile = 'i' THEN TRUE ELSE FALSE END, 'MODIFIES' AS sql_data_access, CASE WHEN p.proisstrict THEN TRUE ELSE FALSE END, pg_catalog.obj_description(p.oid), CASE WHEN pg_catalog.pg_function_is_visible(p.oid) IS TRUE THEN p.proname ELSE coalesce (n.nspname || '.', '') || p.proname END, coalesce (n.nspname || '.', '') || p.proname, o.rolname FROM pg_namespace n, pg_proc p, pg_language l, pg_type t, pg_namespace nt, pg_roles o WHERE current_database() = ##cat::string AND n.nspname = ##schema::string AND n.oid = p.pronamespace AND p.prolang = l.oid AND p.prorettype = t.oid AND t.typnamespace = nt.oid AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE')) AND o.oid=p.proowner",

	/* I_STMT_ROUTINES_ONE */
	"SELECT current_database(), n.nspname, ((p.proname || '_') || p.oid), current_database(), n.nspname, p.proname, CASE WHEN p.proisagg THEN 'AGGREGATE' ELSE 'FUNCTION' END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || n.nspname || '.' || p.proname || '.' || p.oid ELSE coalesce (nt.nspname || '.', '') || t.typname END AS rettype, p.proretset, p.pronargs, CASE WHEN l.lanname = 'sql' THEN 'SQL' ELSE 'EXTERNAL' END, CASE WHEN pg_has_role(p.proowner, 'USAGE') THEN p.prosrc ELSE NULL END, CASE WHEN l.lanname = 'c' THEN p.prosrc ELSE NULL END, upper(l.lanname) AS external_language, 'GENERAL' AS parameter_style, CASE WHEN p.provolatile = 'i' THEN TRUE ELSE FALSE END, 'MODIFIES' AS sql_data_access, CASE WHEN p.proisstrict THEN TRUE ELSE FALSE END, pg_catalog.obj_description(p.oid), CASE WHEN pg_catalog.pg_function_is_visible(p.oid) IS TRUE THEN p.proname ELSE coalesce (n.nspname || '.', '') || p.proname END, coalesce (n.nspname || '.', '') || p.proname, o.rolname FROM pg_namespace n, pg_proc p, pg_language l, pg_type t, pg_namespace nt, pg_roles o WHERE current_database() = ##cat::string AND n.nspname = ##schema::string AND ((p.proname || '_') || p.oid) = ##name::string AND n.oid = p.pronamespace AND p.prolang = l.oid AND p.prorettype = t.oid AND t.typnamespace = nt.oid AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE')) AND o.oid=p.proowner",

	/* I_STMT_ROUTINE_PAR_ALL */
	"SELECT current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), (ss.x).n, CASE WHEN ss.proargmodes IS NULL THEN 'IN' WHEN ss.proargmodes[(ss.x).n] = 'i' THEN 'IN' WHEN ss.proargmodes[(ss.x).n] = 'o' THEN 'OUT' WHEN ss.proargmodes[(ss.x).n] = 'b' THEN 'INOUT' ELSE NULL END, NULLIF(ss.proargnames[(ss.x).n], ''), CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n ELSE NULL END FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE ((ss.proargmodes[(ss.x).n] != 't' AND ss.proargmodes[(ss.x).n] != 'v') OR ss.proargmodes[(ss.x).n] IS NULL) AND t.oid = (ss.x).x AND t.typnamespace = nt.oid",

	/* I_STMT_ROUTINE_PAR */
	"SELECT current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), (ss.x).n, CASE WHEN ss.proargmodes IS NULL THEN 'IN' WHEN ss.proargmodes[(ss.x).n] = 'i' THEN 'IN' WHEN ss.proargmodes[(ss.x).n] = 'o' THEN 'OUT' WHEN ss.proargmodes[(ss.x).n] = 'b' THEN 'INOUT' ELSE NULL END, NULLIF(ss.proargnames[(ss.x).n], ''), CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n ELSE NULL END FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE ((ss.proargmodes[(ss.x).n] != 't' AND ss.proargmodes[(ss.x).n] != 'v') OR ss.proargmodes[(ss.x).n] IS NULL) AND t.oid = (ss.x).x AND t.typnamespace = nt.oid AND current_database() = ##cat::string AND ss.n_nspname = ##schema::string AND ((ss.proname || '_') || ss.p_oid) = ##name::string",

	/* I_STMT_ROUTINE_COL_ALL */
	"SELECT current_database() AS specific_catalog, ss.n_nspname AS specific_schema, ((ss.proname || '_') || ss.p_oid) AS specific_name, NULLIF(ss.proargnames[(ss.x).n], '') AS column_name, (ss.x).n AS ordinal_position, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END AS data_type, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n ELSE NULL END AS array_spec FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b') ORDER BY 1, 2, 3, 4, 5",

	/* I_STMT_ROUTINE_COL */
	"SELECT current_database() AS specific_catalog, ss.n_nspname AS specific_schema, ((ss.proname || '_') || ss.p_oid) AS specific_name, NULLIF(ss.proargnames[(ss.x).n], '') AS column_name, (ss.x).n AS ordinal_position, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END AS data_type, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n ELSE NULL END FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b') AND current_database() = ##cat::string AND ss.n_nspname = ##schema::string AND ((ss.proname || '_') || ss.p_oid) = ##name::string AND column_name = ##name2::string AND ordinal_position = ##pos::string ORDER BY 1, 2, 3, 4, 5",

	/* I_STMT_INDEXES */
	"SELECT current_database() AS index_catalog, nc2.nspname AS index_schema, c2.relname AS index_name, current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, i.indisunique, pg_get_indexdef (i.indexrelid, 0, false), NULL, NULL, o.rolname, pg_catalog.obj_description (c2.oid), i.indexrelid FROM pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid), pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) LEFT JOIN pg_roles o ON (o.oid=c2.relowner), pg_catalog.pg_index i WHERE c.oid = i.indrelid AND i.indexrelid = c2.oid AND NOT i.indisprimary AND pg_catalog.pg_table_is_visible(c.oid) AND nc.nspname = ##schema::string AND c.relname = ##name::string ORDER BY c.relname",

	/* I_STMT_INDEXES_ALL */
	"SELECT current_database() AS index_catalog, nc2.nspname AS index_schema, c2.relname AS index_name, current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, i.indisunique, pg_get_indexdef (i.indexrelid, 0, false), NULL, NULL, o.rolname, pg_catalog.obj_description (c2.oid), i.indexrelid FROM pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid), pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) LEFT JOIN pg_roles o ON (o.oid=c2.relowner), pg_catalog.pg_index i WHERE c.oid = i.indrelid AND i.indexrelid = c2.oid AND NOT i.indisprimary AND pg_catalog.pg_table_is_visible(c.oid) ORDER BY c.relname",

	/* I_STMT_INDEXES_NAMED */
	"SELECT current_database() AS index_catalog, nc2.nspname AS index_schema, c2.relname AS index_name, current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, i.indisunique, pg_get_indexdef (i.indexrelid, 0, false), NULL, NULL, o.rolname, pg_catalog.obj_description (c2.oid), i.indexrelid FROM pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid), pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) LEFT JOIN pg_roles o ON (o.oid=c2.relowner), pg_catalog.pg_index i WHERE c.oid = i.indrelid AND i.indexrelid = c2.oid AND NOT i.indisprimary AND pg_catalog.pg_table_is_visible(c.oid) AND nc.nspname = ##schema::string AND c.relname = ##name::string AND c2.relname = ##name2::string ORDER BY c.relname",

	/* I_STMT_INDEXES_COLUMNS_GET_ALL_INDEXES */
	"SELECT i.indexrelid FROM pg_catalog.pg_class c, pg_catalog.pg_index i WHERE c.oid = i.indrelid AND NOT i.indisprimary AND pg_catalog.pg_table_is_visible(c.oid)",

	/* I_STMT_INDEXES_COLUMNS_GET_NAMED_INDEXES */
	"SELECT i.indexrelid FROM pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid), pg_catalog.pg_index i, pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) WHERE c.oid = i.indrelid AND NOT i.indisprimary AND pg_catalog.pg_table_is_visible(c.oid) AND i.indexrelid = c2.oid AND c.relname = ##name::string AND nc.nspname = ##schema::string AND c2.relname=##name2::string",

	/* I_STMT_INDEXES_COLUMNS_FOR_INDEX */
	"SELECT current_database() AS index_catalog, nc2.nspname AS index_schema, c2.relname AS index_name, current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, a.attname, NULL, (ss.a).n FROM pg_catalog.pg_index i, (SELECT information_schema._pg_expandarray(indkey) AS a FROM pg_catalog.pg_index WHERE indexrelid = ##oid::guint) ss, pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid) INNER JOIN pg_catalog.pg_attribute a ON (a.attrelid = c.oid), pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) WHERE i.indexrelid = ##oid::guint AND (ss.a).x != 0 AND a.attnum = (ss.a).x AND c.oid = i.indrelid AND i.indexrelid = c2.oid AND pg_catalog.pg_table_is_visible(c.oid) UNION SELECT current_database() AS index_catalog, nc2.nspname AS index_schema, c2.relname AS index_name, current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, NULL, pg_get_indexdef (i.indexrelid, (ss.a).n, false), (ss.a).n FROM pg_catalog.pg_index i, (SELECT information_schema._pg_expandarray(indkey) AS a FROM pg_catalog.pg_index WHERE indexrelid = ##oid::guint) ss, pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid), pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) WHERE i.indexrelid = ##oid::guint AND (ss.a).x = 0 AND c.oid = i.indrelid AND i.indexrelid = c2.oid AND pg_catalog.pg_table_is_visible(c.oid) order by 9"
};

// PostgreSQL > 11.0
static gchar *internal_sql_11[] = {
	/* I_STMT_CATALOG */
	"SELECT pg_catalog.current_database()",

	/* I_STMT_BTYPES */
	"SELECT t.typname, 'pg_catalog.' || t.typname, 'gchararray', pg_catalog.obj_description(t.oid), NULL, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN typtype = 'p' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, CAST (t.oid AS varchar) FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (typtype='b' OR typtype='p') AND typelem = 0",

	/* I_STMT_SCHEMAS */
	"SELECT current_database() AS catalog_name, n.nspname AS schema_name, u.rolname AS schema_owner, CASE WHEN n.nspname ~ '^pg_' THEN TRUE WHEN n.nspname = 'information_schema' THEN TRUE ELSE FALSE END, CASE WHEN n.nspname = 'public' THEN TRUE ELSE FALSE END AS schema_default FROM pg_namespace n, pg_roles u WHERE n.nspowner = u.oid AND current_database() = ##cat::string",

	/* I_STMT_SCHEMAS_ALL */
	"SELECT current_database() AS catalog_name, n.nspname AS schema_name, u.rolname AS schema_owner, CASE WHEN n.nspname ~ '^pg_' THEN TRUE WHEN n.nspname = 'information_schema' THEN TRUE ELSE FALSE END, CASE WHEN n.nspname = 'public' THEN TRUE ELSE FALSE END AS schema_default FROM pg_namespace n, pg_roles u WHERE n.nspowner = u.oid",

	/* I_STMT_SCHEMA_NAMED */
	"SELECT current_database() AS catalog_name, n.nspname AS schema_name, u.rolname AS schema_owner, CASE WHEN n.nspname ~ '^pg_' THEN TRUE WHEN n.nspname = 'information_schema' THEN TRUE ELSE FALSE END, CASE WHEN n.nspname = 'public' THEN TRUE ELSE FALSE END AS schema_default FROM pg_namespace n, pg_roles u WHERE n.nspowner = u.oid AND current_database() = ##cat::string AND n.nspname = ##name::string",

	/* I_STMT_TABLES */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY' WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL END AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_roles o WHERE CASE WHEN ##cat::string IS NULL THEN TRUE ELSE current_database() = ##cat::string END AND CASE WHEN ##schema::string IS NULL THEN TRUE ELSE nc.nspname = ##schema::string END AND c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER')) AND o.oid=c.relowner",

	/* I_STMT_TABLES_ALL */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY' WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL END AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_roles o WHERE c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER')) AND o.oid=c.relowner",

	/* I_STMT_TABLE_NAMED */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, CASE WHEN nc.oid = pg_my_temp_schema() THEN 'LOCAL TEMPORARY' WHEN c.relkind = 'r' THEN 'BASE TABLE' WHEN c.relkind = 'v' THEN 'VIEW' ELSE NULL END AS table_type, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.obj_description(c.oid), CASE WHEN pg_catalog.pg_table_is_visible(c.oid) IS TRUE AND nc.nspname!='pg_catalog' THEN c.relname ELSE coalesce (nc.nspname || '.', '') || c.relname END, coalesce (nc.nspname || '.', '') || c.relname, o.rolname FROM pg_namespace nc, pg_class c, pg_roles o WHERE CASE WHEN ##cat::string IS NULL THEN TRUE ELSE current_database() = ##cat::string END AND CASE WHEN ##schema::string IS NULL THEN TRUE ELSE nc.nspname = ##schema::string END AND c.relnamespace = nc.oid AND (c.relkind = ANY (ARRAY['r', 'v'])) AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER')) AND o.oid=c.relowner AND c.relname = ##name::string",

	/* I_STMT_VIEWS */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE current_database() = ##cat::string AND nc.nspname = ##schema::string AND c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER'))",

	/* I_STMT_VIEWS_ALL */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER'))",

	/* I_STMT_VIEW_NAMED */
	"SELECT current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, pg_catalog.pg_get_viewdef(c.oid, TRUE), NULL, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END FROM pg_namespace nc, pg_class c WHERE current_database() = ##cat::string AND nc.nspname = ##schema::string AND c.relnamespace = nc.oid AND c.relkind = 'v' AND NOT pg_is_other_temp_schema(nc.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER')) AND c.relname = ##name::string",

	/* I_STMT_COLUMNS_OF_TABLE */
	"SELECT current_database(), nc.nspname, c.relname, a.attname, a.attnum, pg_get_expr(ad.adbin, ad.adrelid), CASE WHEN a.attnotnull OR t.typtype = 'd' AND t.typnotnull THEN FALSE ELSE TRUE END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL, NULL, NULL, NULL, NULL, CASE WHEN pg_get_expr(ad.adbin, ad.adrelid) LIKE 'nextval(%' THEN '" GDA_EXTRA_AUTO_INCREMENT "' ELSE NULL END, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.col_description(c.oid, a.attnum), CAST (t.oid AS varchar) FROM pg_attribute a LEFT JOIN pg_attrdef ad ON a.attrelid = ad.adrelid AND a.attnum = ad.adnum, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typtype = 'd' AND t.typbasetype = bt.oid WHERE current_database() = ##cat::string AND nc.nspname = ##schema::string AND c.relname = ##name::string AND a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'REFERENCES'))",

	/* I_STMT_COLUMNS_ALL */
	"SELECT current_database(), nc.nspname, c.relname, a.attname, a.attnum, pg_get_expr(ad.adbin, ad.adrelid), CASE WHEN a.attnotnull OR t.typtype = 'd' AND t.typnotnull THEN FALSE ELSE TRUE END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL, NULL, NULL, NULL, NULL, CASE WHEN pg_get_expr(ad.adbin, ad.adrelid) LIKE 'nextval(%' THEN '" GDA_EXTRA_AUTO_INCREMENT "' ELSE NULL END, CASE WHEN c.relkind = 'r' THEN TRUE ELSE FALSE END, pg_catalog.col_description(c.oid, a.attnum), CAST (t.oid AS varchar) FROM pg_attribute a LEFT JOIN pg_attrdef ad ON a.attrelid = ad.adrelid AND a.attnum = ad.adnum, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typtype = 'd' AND t.typbasetype = bt.oid WHERE a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'REFERENCES'))",

	/* I_STMT_TABLES_CONSTRAINTS */
	"SELECT current_database() AS constraint_catalog, nc.nspname AS constraint_schema, c.conname AS constraint_name, current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, CASE c.contype WHEN 'c' THEN 'CHECK' WHEN 'f' THEN 'FOREIGN KEY' WHEN 'p' THEN 'PRIMARY KEY' WHEN 'u' THEN 'UNIQUE' ELSE NULL END AS constraint_type, CASE c.contype WHEN 'c' THEN c.consrc ELSE NULL END, CASE WHEN c.condeferrable THEN TRUE ELSE FALSE END AS is_deferrable, CASE WHEN c.condeferred THEN TRUE ELSE FALSE END AS initially_deferred FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string "
	"UNION SELECT current_database() AS constraint_catalog, nr.nspname AS constraint_schema, (((((nr.oid || '_') || r.oid) || '_') || a.attnum) || '_not_null') AS constraint_name, current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, 'CHECK' AS constraint_type, a.attname || ' IS NOT NULL', FALSE AS is_deferrable, FALSE AS initially_deferred FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'SELECT') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string",

	/* I_STMT_TABLES_CONSTRAINTS_ALL */
	"SELECT current_database() AS constraint_catalog, nc.nspname AS constraint_schema, c.conname AS constraint_name, current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, CASE c.contype WHEN 'c' THEN 'CHECK' WHEN 'f' THEN 'FOREIGN KEY' WHEN 'p' THEN 'PRIMARY KEY' WHEN 'u' THEN 'UNIQUE' ELSE NULL END AS constraint_type, CASE c.contype WHEN 'c' THEN c.consrc ELSE NULL END, CASE WHEN c.condeferrable THEN TRUE ELSE FALSE END AS is_deferrable, CASE WHEN c.condeferred THEN TRUE ELSE FALSE END AS initially_deferred FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) "
	"UNION SELECT current_database() AS constraint_catalog, nr.nspname AS constraint_schema, (((((nr.oid || '_') || r.oid) || '_') || a.attnum) || '_not_null') AS constraint_name, current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, 'CHECK' AS constraint_type, a.attname || ' IS NOT NULL', FALSE AS is_deferrable, FALSE AS initially_deferred FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'SELECT') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER'))",

	/* I_STMT_TABLES_CONSTRAINT_NAMED */
	"SELECT constraint_catalog, constraint_schema, constraint_name, table_catalog, table_schema, table_name, constraint_type, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.table_constraints WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string",

	/* I_STMT_REF_CONSTRAINTS */
	"SELECT current_database() AS table_catalog, nt.nspname AS table_schema, t.relname AS table_name, c.conname AS constraint_name, current_database() AS ref_table_catalog, nref.nspname AS ref_table_schema, ref.relname AS ref_table_name, pkc.conname AS ref_constraint_name, CASE c.confmatchtype WHEN 'f' THEN 'FULL' WHEN 'p' THEN 'PARTIAL' WHEN 'u' THEN 'NONE' ELSE NULL END AS match_option, CASE c.confupdtype WHEN 'c' THEN 'CASCADE' WHEN 'n' THEN 'SET NULL' WHEN 'd' THEN 'SET DEFAULT' WHEN 'r' THEN 'RESTRICT' WHEN 'a' THEN 'NO ACTION' ELSE NULL END AS update_rule, CASE c.confdeltype WHEN 'c' THEN 'CASCADE' WHEN 'n' THEN 'SET NULL' WHEN 'd' THEN 'SET DEFAULT' WHEN 'r' THEN 'RESTRICT' WHEN 'a' THEN 'NO ACTION' ELSE NULL END AS delete_rule FROM pg_constraint c INNER JOIN pg_class t ON (c.conrelid=t.oid) INNER JOIN pg_namespace nt ON (nt.oid=t.relnamespace) INNER JOIN pg_class ref ON (c.confrelid=ref.oid) INNER JOIN pg_namespace nref ON (nref.oid=ref.relnamespace) INNER JOIN pg_constraint pkc ON (c.confrelid = pkc.conrelid AND information_schema._pg_keysequal(c.confkey, pkc.conkey) AND pkc.contype='p') WHERE c.contype = 'f' AND current_database() = ##cat::string AND nt.nspname = ##schema::string AND t.relname = ##name::string AND c.conname = ##name2::string",

	/* I_STMT_REF_CONSTRAINTS_ALL */
	"SELECT current_database() AS table_catalog, nt.nspname AS table_schema, t.relname AS table_name, c.conname AS constraint_name, current_database() AS ref_table_catalog, nref.nspname AS ref_table_schema, ref.relname AS ref_table_name, pkc.conname AS ref_constraint_name, CASE c.confmatchtype WHEN 'f' THEN 'FULL' WHEN 'p' THEN 'PARTIAL' WHEN 'u' THEN 'NONE' ELSE NULL END AS match_option, CASE c.confupdtype WHEN 'c' THEN 'CASCADE' WHEN 'n' THEN 'SET NULL' WHEN 'd' THEN 'SET DEFAULT' WHEN 'r' THEN 'RESTRICT' WHEN 'a' THEN 'NO ACTION' ELSE NULL END AS update_rule, CASE c.confdeltype WHEN 'c' THEN 'CASCADE' WHEN 'n' THEN 'SET NULL' WHEN 'd' THEN 'SET DEFAULT' WHEN 'r' THEN 'RESTRICT' WHEN 'a' THEN 'NO ACTION' ELSE NULL END AS delete_rule FROM pg_constraint c INNER JOIN pg_class t ON (c.conrelid=t.oid) INNER JOIN pg_namespace nt ON (nt.oid=t.relnamespace) INNER JOIN pg_class ref ON (c.confrelid=ref.oid) INNER JOIN pg_namespace nref ON (nref.oid=ref.relnamespace) INNER JOIN pg_constraint pkc ON (c.confrelid = pkc.conrelid AND information_schema._pg_keysequal(c.confkey, pkc.conkey) AND pkc.contype='p') WHERE c.contype = 'f'",

	/* I_STMT_KEY_COLUMN_USAGE */
	"SELECT table_catalog, table_schema, table_name, constraint_name, column_name, ordinal_position FROM information_schema.key_column_usage WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string",

	/* I_STMT_KEY_COLUMN_USAGE_ALL */
	"SELECT table_catalog, table_schema, table_name, constraint_name, column_name, ordinal_position FROM information_schema.key_column_usage",

	/* I_STMT_CHECK_COLUMN_USAGE */
	"SELECT current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, c.conname AS constraint_name,a.attname FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r, pg_attribute a, (SELECT sc.oid, information_schema._pg_expandarray (sc.conkey) as x FROM pg_constraint sc WHERE sc.contype = 'c') ss WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) AND c.contype = 'c' AND ss.oid = c.oid AND a.attrelid = r.oid AND a.attnum = (ss.x).x AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string AND c.conname = ##name2::string "
	"UNION SELECT current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, (((((nr.oid || '_') || r.oid) || '_') || a.attnum) || '_not_null') AS constraint_name, a.attname FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'SELECT') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) AND current_database() = ##cat::string AND nr.nspname = ##schema::string AND r.relname = ##name::string AND (((((nr.oid || '_') || r.oid) || '_') || a.attnum) || '_not_null') = ##name2::string",

	/* I_STMT_CHECK_COLUMN_USAGE_ALL */
	"SELECT current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, c.conname AS constraint_name,a.attname FROM pg_namespace nc, pg_namespace nr, pg_constraint c, pg_class r, pg_attribute a, (SELECT sc.oid, information_schema._pg_expandarray (sc.conkey) as x FROM pg_constraint sc WHERE sc.contype = 'c') ss WHERE nc.oid = c.connamespace AND nr.oid = r.relnamespace AND c.conrelid = r.oid AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER')) AND c.contype = 'c' AND ss.oid = c.oid AND a.attrelid = r.oid AND a.attnum = (ss.x).x "
	"UNION SELECT current_database() AS table_catalog, nr.nspname AS table_schema, r.relname AS table_name, (((((nr.oid || '_') || r.oid) || '_') || a.attnum) || '_not_null') AS constraint_name, a.attname FROM pg_namespace nr, pg_class r, pg_attribute a WHERE nr.oid = r.relnamespace AND r.oid = a.attrelid AND a.attnotnull AND a.attnum > 0 AND NOT a.attisdropped AND r.relkind = 'r' AND NOT pg_is_other_temp_schema(nr.oid) AND (pg_has_role(r.relowner, 'USAGE') OR has_table_privilege(r.oid, 'SELECT') OR has_table_privilege(r.oid, 'INSERT') OR has_table_privilege(r.oid, 'UPDATE') OR has_table_privilege(r.oid, 'DELETE') OR has_table_privilege(r.oid, 'REFERENCES') OR has_table_privilege(r.oid, 'TRIGGER'))",

	/* I_STMT_UDT */
	"SELECT pg_catalog.current_database() as cat, n.nspname, t.typname, 'gchararray', pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (n.nspname || '.', '') || t.typname END, coalesce (n.nspname || '.', '') || t.typname, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, o.rolname FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n , pg_roles o WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (t.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid)) AND o.oid=t.typowner AND pg_catalog.current_database() = ##cat::string AND n.nspname = ##schema::string",

	/* I_STMT_UDT_ALL */
	"SELECT pg_catalog.current_database(), n.nspname, t.typname, 'gchararray', pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (n.nspname || '.', '') || t.typname END, coalesce (n.nspname || '.', '') || t.typname, CASE WHEN t.typname ~ '^_' THEN TRUE WHEN t.typname in ('any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid', 'oid', 'aclitem') THEN TRUE ELSE FALSE END, o.rolname FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n , pg_roles o WHERE t.typowner=u.usesysid AND n.oid = t.typnamespace AND pg_catalog.pg_type_is_visible(t.oid) AND (t.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid)) AND o.oid=t.typowner",

	/* I_STMT_UDT_COLUMNS */
	"select pg_catalog.current_database(), n.nspname, udt.typname, a.attname, a.attnum, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum ELSE NULL END, information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL , NULL, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid) WHERE udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid) AND pg_catalog.current_database() = ##cat::string AND n.nspname = ##schema::string AND udt.typname = ##name::string",

	/* I_STMT_UDT_COLUMNS_ALL */
	"select pg_catalog.current_database(), n.nspname, udt.typname, a.attname, a.attnum, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum ELSE NULL END, information_schema._pg_char_max_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_char_octet_length(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_numeric_scale(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), information_schema._pg_datetime_precision(information_schema._pg_truetypid(a.*, t.*), information_schema._pg_truetypmod(a.*, t.*)), NULL, NULL , NULL, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid) WHERE udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid)",

	/* I_STMT_DOMAINS */
	"SELECT pg_catalog.current_database(), nt.nspname, t.typname, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nbt.nspname || '.', '') || bt.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(t.typbasetype, t.typtypmod), information_schema._pg_char_octet_length(t.typbasetype, t.typtypmod),  NULL, NULL, NULL, NULL, NULL, NULL,  information_schema._pg_numeric_precision(t.typbasetype, t.typtypmod), information_schema._pg_numeric_scale(t.typbasetype, t.typtypmod), t.typdefault, pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (nt.nspname || '.', '') || t.typname END, coalesce (nt.nspname || '.', '') || t.typname, FALSE, o.rolname FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt, pg_roles o WHERE t.typnamespace = nt.oid AND t.typbasetype = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND o.oid=t.typowner AND pg_catalog.current_database() = ##cat::string AND nt.nspname = ##schema::string",

	/* I_STMT_DOMAINS_ALL */
	"SELECT pg_catalog.current_database(), nt.nspname, t.typname, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN NULL ELSE coalesce (nbt.nspname || '.', '') || bt.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname ELSE NULL END, 'gchararray', information_schema._pg_char_max_length(t.typbasetype, t.typtypmod), information_schema._pg_char_octet_length(t.typbasetype, t.typtypmod),  NULL, NULL, NULL, NULL, NULL, NULL,  information_schema._pg_numeric_precision(t.typbasetype, t.typtypmod), information_schema._pg_numeric_scale(t.typbasetype, t.typtypmod), t.typdefault, pg_catalog.obj_description(t.oid), CASE WHEN pg_catalog.pg_type_is_visible(t.oid) IS TRUE THEN t.typname ELSE coalesce (nt.nspname || '.', '') || t.typname END, coalesce (nt.nspname || '.', '') || t.typname, FALSE, o.rolname FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt, pg_roles o WHERE t.typnamespace = nt.oid AND t.typbasetype = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND o.oid=t.typowner",

	/* I_STMT_DOMAINS_CONSTRAINTS */
	"SELECT constraint_catalog, constraint_schema, constraint_name, domain_catalog, domain_schema, domain_name, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.domain_constraints WHERE domain_catalog = ##cat::string AND domain_schema = ##schema::string AND domain_name = ##name::string",

	/* I_STMT_DOMAINS_CONSTRAINTS_ALL */
	"SELECT constraint_catalog, constraint_schema, constraint_name, domain_catalog, domain_schema, domain_name, NULL, CASE WHEN is_deferrable = 'YES' THEN TRUE ELSE FALSE END, CASE WHEN initially_deferred = 'YES' THEN TRUE ELSE FALSE END FROM information_schema.domain_constraints",

	/* I_STMT_VIEWS_COLUMNS */
	"SELECT view_catalog, view_schema, view_name, table_catalog, table_schema, table_name, column_name FROM information_schema.view_column_usage WHERE view_catalog = ##cat::string AND view_schema = ##schema::string AND view_name = ##name::string",

	/* I_STMT_VIEWS_COLUMNS_ALL */
	"SELECT view_catalog, view_schema, view_name, table_catalog, table_schema, table_name, column_name FROM information_schema.view_column_usage",

	/* I_STMT_TRIGGERS */
	"SELECT current_database(), n.nspname, t.tgname, em.text, current_database(), n.nspname, c.relname, \"substring\"(pg_get_triggerdef(t.oid), \"position\"(\"substring\"(pg_get_triggerdef(t.oid), 48), 'EXECUTE PROCEDURE') + 47) AS action_statement, CASE WHEN (t.tgtype & 1) = 1 THEN 'ROW' ELSE 'STATEMENT' END AS action_orientation, CASE WHEN (t.tgtype & 2) = 2 THEN 'BEFORE' ELSE 'AFTER' END AS condition_timing, pg_catalog.obj_description(t.oid), t.tgname, t.tgname FROM pg_namespace n, pg_class c, pg_trigger t, (( SELECT 4, 'INSERT' UNION ALL SELECT 8, 'DELETE') UNION ALL SELECT 16, 'UPDATE') em(num, text) WHERE n.oid = c.relnamespace AND c.oid = t.tgrelid AND (t.tgtype & em.num) <> 0 AND (t.tgconstraint = 0) AND NOT pg_is_other_temp_schema(n.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER')) AND current_database() = ##cat::string AND n.nspname = ##schema::string AND c.relname = ##name::string",

	/* I_STMT_TRIGGERS_ALL */
	"SELECT current_database(), n.nspname, t.tgname, em.text, current_database(), n.nspname, c.relname, \"substring\"(pg_get_triggerdef(t.oid), \"position\"(\"substring\"(pg_get_triggerdef(t.oid), 48), 'EXECUTE PROCEDURE') + 47) AS action_statement, CASE WHEN (t.tgtype & 1) = 1 THEN 'ROW' ELSE 'STATEMENT' END AS action_orientation, CASE WHEN (t.tgtype & 2) = 2 THEN 'BEFORE' ELSE 'AFTER' END AS condition_timing, pg_catalog.obj_description(t.oid), t.tgname, t.tgname FROM pg_namespace n, pg_class c, pg_trigger t, (( SELECT 4, 'INSERT' UNION ALL SELECT 8, 'DELETE') UNION ALL SELECT 16, 'UPDATE') em(num, text) WHERE n.oid = c.relnamespace AND c.oid = t.tgrelid AND (t.tgtype & em.num) <> 0 AND NOT (t.tgconstraint = 0) AND NOT pg_is_other_temp_schema(n.oid) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'DELETE') OR has_table_privilege(c.oid, 'REFERENCES') OR has_table_privilege(c.oid, 'TRIGGER'))",

	/* I_STMT_EL_TYPES_COL */
	"SELECT 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum, current_database(), nc.nspname, c.relname, 'TABLE_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_attribute a, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typelem = bt.oid WHERE a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'REFERENCES')) AND t.typelem <> 0 AND t.typlen = -1 AND 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum = ##name::string",

	/* I_STMT_EL_TYPES_DOM */
	"SELECT 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname, current_database(), nt.nspname, t.typname, 'DOMAIN', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt WHERE t.typnamespace = nt.oid AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND t.typlen = -1 AND 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname = ##name::string",

	/* I_STMT_EL_TYPES_UDT */
	"SELECT 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum, pg_catalog.current_database(), n.nspname, udt.typname, 'UDT_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid), pg_type bt, pg_namespace nbt where udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid) AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typlen = -1 AND 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum = ##name::string",

	/* I_STMT_EL_TYPES_ROUT_PAR */
	"SELECT 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), 'ROUTINE_PAR', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_type bt, pg_namespace nbt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE t.oid = (ss.x).x AND bt.oid= t.typelem AND bt.typnamespace = nbt.oid AND t.typelem <> 0 AND t.typlen = -1 AND 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n = ##name::string",

	/* I_STMT_EL_TYPES_ROUT_COL */
	"SELECT 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), 'ROUTINE_COL', CASE WHEN at.typelem <> 0 AND at.typlen = -1 THEN 'array_spec' ELSE coalesce (ant.nspname || '.', '') || at.typname END, CASE WHEN at.typelem <> 0 AND at.typlen = -1 THEN 'ARR' || at.typelem ELSE NULL END, NULL, NULL FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss, pg_type at, pg_namespace ant WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND at.oid = t.typelem AND at.typnamespace = ant.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b') AND 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n = ##name::string",

	/* I_STMT_EL_TYPES_ALL */
	"SELECT 'UDT' || current_database() || '.' || n.nspname || '.' || udt.typname || '.' || a.attnum, pg_catalog.current_database(), n.nspname, udt.typname, 'UDT_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type udt INNER JOIN pg_namespace n ON (udt.typnamespace=n.oid) INNER JOIN pg_attribute a ON (a.attrelid=udt.typrelid) INNER JOIN pg_type t ON (a.atttypid=t.oid) INNER JOIN pg_namespace nt ON (t.typnamespace = nt.oid), pg_type bt, pg_namespace nbt where udt.typrelid != 0 AND (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = udt.typrelid) AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typlen = -1 "
	"UNION SELECT 'DOM' || current_database() || '.' || nt.nspname || '.' || t.typname, current_database(), nt.nspname, t.typname, 'DOMAIN', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_namespace nt, pg_type bt, pg_namespace nbt WHERE t.typnamespace = nt.oid AND t.typelem = bt.oid AND bt.typnamespace = nbt.oid AND t.typtype = 'd' AND t.typlen = -1 "
	"UNION SELECT 'COL' || current_database() || '.' || nc.nspname || '.' || c.relname || '.' || a.attnum, current_database(), nc.nspname, c.relname, 'TABLE_COL', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_attribute a, pg_class c, pg_namespace nc, pg_type t JOIN pg_namespace nt ON t.typnamespace = nt.oid LEFT JOIN (pg_type bt JOIN pg_namespace nbt ON bt.typnamespace = nbt.oid) ON t.typelem = bt.oid WHERE a.attrelid = c.oid AND a.atttypid = t.oid AND nc.oid = c.relnamespace AND NOT pg_is_other_temp_schema(nc.oid) AND a.attnum > 0 AND NOT a.attisdropped AND (c.relkind = ANY (ARRAY['r', 'v'])) AND (pg_has_role(c.relowner, 'USAGE') OR has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid, 'INSERT') OR has_table_privilege(c.oid, 'UPDATE') OR has_table_privilege(c.oid, 'REFERENCES')) AND t.typelem <> 0 AND t.typlen = -1 "
	"UNION SELECT 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), 'ROUTINE_PAR', coalesce (nbt.nspname || '.', '') || bt.typname, NULL, NULL, NULL FROM pg_type t, pg_type bt, pg_namespace nbt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE t.oid = (ss.x).x AND bt.oid= t.typelem AND bt.typnamespace = nbt.oid AND t.typelem <> 0 AND t.typlen = -1 "
	"UNION SELECT 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n, current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), 'ROUTINE_COL', CASE WHEN at.typelem <> 0 AND at.typlen = -1 THEN 'array_spec' ELSE coalesce (ant.nspname || '.', '') || at.typname END, CASE WHEN at.typelem <> 0 AND at.typlen = -1 THEN 'ARR' || at.typelem ELSE NULL END, NULL, NULL FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss, pg_type at, pg_namespace ant WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND at.oid = t.typelem AND at.typnamespace = ant.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b')",

	/* I_STMT_ROUTINES_ALL */
	"SELECT current_database(), n.nspname, ((p.proname || '_') || p.oid), current_database(), n.nspname, p.proname, CASE WHEN p.prokind = 'a' THEN 'AGGREGATE' ELSE 'FUNCTION' END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || n.nspname || '.' || p.proname || '.' || p.oid ELSE coalesce (nt.nspname || '.', '') || t.typname END AS rettype, p.proretset, p.pronargs, CASE WHEN l.lanname = 'sql' THEN 'SQL' ELSE 'EXTERNAL' END, CASE WHEN pg_has_role(p.proowner, 'USAGE') THEN p.prosrc ELSE NULL END, CASE WHEN l.lanname = 'c' THEN p.prosrc ELSE NULL END, upper(l.lanname) AS external_language, 'GENERAL' AS parameter_style, CASE WHEN p.provolatile = 'i' THEN TRUE ELSE FALSE END, 'MODIFIES' AS sql_data_access, CASE WHEN p.proisstrict THEN TRUE ELSE FALSE END, pg_catalog.obj_description(p.oid), CASE WHEN pg_catalog.pg_function_is_visible(p.oid) IS TRUE THEN p.proname ELSE coalesce (n.nspname || '.', '') || p.proname END, coalesce (n.nspname || '.', '') || p.proname, o.rolname FROM pg_namespace n, pg_proc p, pg_language l, pg_type t, pg_namespace nt, pg_roles o WHERE n.oid = p.pronamespace AND p.prolang = l.oid AND p.prorettype = t.oid AND t.typnamespace = nt.oid AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE')) AND o.oid=p.proowner",

	/* I_STMT_ROUTINES */
	"SELECT current_database(), n.nspname, ((p.proname || '_') || p.oid), current_database(), n.nspname, p.proname, CASE WHEN p.prokind = 'a' THEN 'AGGREGATE' ELSE 'FUNCTION' END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || n.nspname || '.' || p.proname || '.' || p.oid ELSE coalesce (nt.nspname || '.', '') || t.typname END AS rettype, p.proretset, p.pronargs, CASE WHEN l.lanname = 'sql' THEN 'SQL' ELSE 'EXTERNAL' END, CASE WHEN pg_has_role(p.proowner, 'USAGE') THEN p.prosrc ELSE NULL END, CASE WHEN l.lanname = 'c' THEN p.prosrc ELSE NULL END, upper(l.lanname) AS external_language, 'GENERAL' AS parameter_style, CASE WHEN p.provolatile = 'i' THEN TRUE ELSE FALSE END, 'MODIFIES' AS sql_data_access, CASE WHEN p.proisstrict THEN TRUE ELSE FALSE END, pg_catalog.obj_description(p.oid), CASE WHEN pg_catalog.pg_function_is_visible(p.oid) IS TRUE THEN p.proname ELSE coalesce (n.nspname || '.', '') || p.proname END, coalesce (n.nspname || '.', '') || p.proname, o.rolname FROM pg_namespace n, pg_proc p, pg_language l, pg_type t, pg_namespace nt, pg_roles o WHERE current_database() = ##cat::string AND n.nspname = ##schema::string AND n.oid = p.pronamespace AND p.prolang = l.oid AND p.prorettype = t.oid AND t.typnamespace = nt.oid AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE')) AND o.oid=p.proowner",

	/* I_STMT_ROUTINES_ONE */
	"SELECT current_database(), n.nspname, ((p.proname || '_') || p.oid), current_database(), n.nspname, p.proname, CASE WHEN p.prokind = 'a' THEN 'AGGREGATE' ELSE 'FUNCTION' END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || n.nspname || '.' || p.proname || '.' || p.oid ELSE coalesce (nt.nspname || '.', '') || t.typname END AS rettype, p.proretset, p.pronargs, CASE WHEN l.lanname = 'sql' THEN 'SQL' ELSE 'EXTERNAL' END, CASE WHEN pg_has_role(p.proowner, 'USAGE') THEN p.prosrc ELSE NULL END, CASE WHEN l.lanname = 'c' THEN p.prosrc ELSE NULL END, upper(l.lanname) AS external_language, 'GENERAL' AS parameter_style, CASE WHEN p.provolatile = 'i' THEN TRUE ELSE FALSE END, 'MODIFIES' AS sql_data_access, CASE WHEN p.proisstrict THEN TRUE ELSE FALSE END, pg_catalog.obj_description(p.oid), CASE WHEN pg_catalog.pg_function_is_visible(p.oid) IS TRUE THEN p.proname ELSE coalesce (n.nspname || '.', '') || p.proname END, coalesce (n.nspname || '.', '') || p.proname, o.rolname FROM pg_namespace n, pg_proc p, pg_language l, pg_type t, pg_namespace nt, pg_roles o WHERE current_database() = ##cat::string AND n.nspname = ##schema::string AND ((p.proname || '_') || p.oid) = ##name::string AND n.oid = p.pronamespace AND p.prolang = l.oid AND p.prorettype = t.oid AND t.typnamespace = nt.oid AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE')) AND o.oid=p.proowner",

	/* I_STMT_ROUTINE_PAR_ALL */
	"SELECT current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), (ss.x).n, CASE WHEN ss.proargmodes IS NULL THEN 'IN' WHEN ss.proargmodes[(ss.x).n] = 'i' THEN 'IN' WHEN ss.proargmodes[(ss.x).n] = 'o' THEN 'OUT' WHEN ss.proargmodes[(ss.x).n] = 'b' THEN 'INOUT' ELSE NULL END, NULLIF(ss.proargnames[(ss.x).n], ''), CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n ELSE NULL END FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE ((ss.proargmodes[(ss.x).n] != 't' AND ss.proargmodes[(ss.x).n] != 'v') OR ss.proargmodes[(ss.x).n] IS NULL) AND t.oid = (ss.x).x AND t.typnamespace = nt.oid",

	/* I_STMT_ROUTINE_PAR */
	"SELECT current_database(), ss.n_nspname, ((ss.proname || '_') || ss.p_oid), (ss.x).n, CASE WHEN ss.proargmodes IS NULL THEN 'IN' WHEN ss.proargmodes[(ss.x).n] = 'i' THEN 'IN' WHEN ss.proargmodes[(ss.x).n] = 'o' THEN 'OUT' WHEN ss.proargmodes[(ss.x).n] = 'b' THEN 'INOUT' ELSE NULL END, NULLIF(ss.proargnames[(ss.x).n], ''), CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUP' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n ELSE NULL END FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE ((ss.proargmodes[(ss.x).n] != 't' AND ss.proargmodes[(ss.x).n] != 'v') OR ss.proargmodes[(ss.x).n] IS NULL) AND t.oid = (ss.x).x AND t.typnamespace = nt.oid AND current_database() = ##cat::string AND ss.n_nspname = ##schema::string AND ((ss.proname || '_') || ss.p_oid) = ##name::string",

	/* I_STMT_ROUTINE_COL_ALL */
	"SELECT current_database() AS specific_catalog, ss.n_nspname AS specific_schema, ((ss.proname || '_') || ss.p_oid) AS specific_name, NULLIF(ss.proargnames[(ss.x).n], '') AS column_name, (ss.x).n AS ordinal_position, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END AS data_type, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n ELSE NULL END AS array_spec FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b') ORDER BY 1, 2, 3, 4, 5",

	/* I_STMT_ROUTINE_COL */
	"SELECT current_database() AS specific_catalog, ss.n_nspname AS specific_schema, ((ss.proname || '_') || ss.p_oid) AS specific_name, NULLIF(ss.proargnames[(ss.x).n], '') AS column_name, (ss.x).n AS ordinal_position, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'array_spec' ELSE coalesce (nt.nspname || '.', '') || t.typname END AS data_type, CASE WHEN t.typelem <> 0 AND t.typlen = -1 THEN 'ROUC' || current_database() || '.' || ss.n_nspname || '.' || ((ss.proname || '_') || ss.p_oid) || '.' || (ss.x).n ELSE NULL END FROM pg_type t, pg_namespace nt, ( SELECT n.nspname AS n_nspname, p.proname, p.oid AS p_oid, p.proargnames, p.proargmodes, information_schema._pg_expandarray(COALESCE(p.proallargtypes, p.proargtypes::oid[])) AS x FROM pg_namespace n, pg_proc p WHERE n.oid = p.pronamespace AND (pg_has_role(p.proowner, 'USAGE') OR has_function_privilege(p.oid, 'EXECUTE'))) ss WHERE t.oid = (ss.x).x AND t.typnamespace = nt.oid AND (ss.proargmodes[(ss.x).n] = 'o' OR ss.proargmodes[(ss.x).n] = 'b') AND current_database() = ##cat::string AND ss.n_nspname = ##schema::string AND ((ss.proname || '_') || ss.p_oid) = ##name::string AND column_name = ##name2::string AND ordinal_position = ##pos::string ORDER BY 1, 2, 3, 4, 5",

	/* I_STMT_INDEXES */
	"SELECT current_database() AS index_catalog, nc2.nspname AS index_schema, c2.relname AS index_name, current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, i.indisunique, pg_get_indexdef (i.indexrelid, 0, false), NULL, NULL, o.rolname, pg_catalog.obj_description (c2.oid), i.indexrelid FROM pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid), pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) LEFT JOIN pg_roles o ON (o.oid=c2.relowner), pg_catalog.pg_index i WHERE c.oid = i.indrelid AND i.indexrelid = c2.oid AND NOT i.indisprimary AND pg_catalog.pg_table_is_visible(c.oid) AND nc.nspname = ##schema::string AND c.relname = ##name::string ORDER BY c.relname",

	/* I_STMT_INDEXES_ALL */
	"SELECT current_database() AS index_catalog, nc2.nspname AS index_schema, c2.relname AS index_name, current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, i.indisunique, pg_get_indexdef (i.indexrelid, 0, false), NULL, NULL, o.rolname, pg_catalog.obj_description (c2.oid), i.indexrelid FROM pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid), pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) LEFT JOIN pg_roles o ON (o.oid=c2.relowner), pg_catalog.pg_index i WHERE c.oid = i.indrelid AND i.indexrelid = c2.oid AND NOT i.indisprimary AND pg_catalog.pg_table_is_visible(c.oid) ORDER BY c.relname",

	/* I_STMT_INDEXES_NAMED */
	"SELECT current_database() AS index_catalog, nc2.nspname AS index_schema, c2.relname AS index_name, current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, i.indisunique, pg_get_indexdef (i.indexrelid, 0, false), NULL, NULL, o.rolname, pg_catalog.obj_description (c2.oid), i.indexrelid FROM pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid), pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) LEFT JOIN pg_roles o ON (o.oid=c2.relowner), pg_catalog.pg_index i WHERE c.oid = i.indrelid AND i.indexrelid = c2.oid AND NOT i.indisprimary AND pg_catalog.pg_table_is_visible(c.oid) AND nc.nspname = ##schema::string AND c.relname = ##name::string AND c2.relname = ##name2::string ORDER BY c.relname",

	/* I_STMT_INDEXES_COLUMNS_GET_ALL_INDEXES */
	"SELECT i.indexrelid FROM pg_catalog.pg_class c, pg_catalog.pg_index i WHERE c.oid = i.indrelid AND NOT i.indisprimary AND pg_catalog.pg_table_is_visible(c.oid)",

	/* I_STMT_INDEXES_COLUMNS_GET_NAMED_INDEXES */
	"SELECT i.indexrelid FROM pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid), pg_catalog.pg_index i, pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) WHERE c.oid = i.indrelid AND NOT i.indisprimary AND pg_catalog.pg_table_is_visible(c.oid) AND i.indexrelid = c2.oid AND c.relname = ##name::string AND nc.nspname = ##schema::string AND c2.relname=##name2::string",

	/* I_STMT_INDEXES_COLUMNS_FOR_INDEX */
	"SELECT current_database() AS index_catalog, nc2.nspname AS index_schema, c2.relname AS index_name, current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, a.attname, NULL, (ss.a).n FROM pg_catalog.pg_index i, (SELECT information_schema._pg_expandarray(indkey) AS a FROM pg_catalog.pg_index WHERE indexrelid = ##oid::guint) ss, pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid) INNER JOIN pg_catalog.pg_attribute a ON (a.attrelid = c.oid), pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) WHERE i.indexrelid = ##oid::guint AND (ss.a).x != 0 AND a.attnum = (ss.a).x AND c.oid = i.indrelid AND i.indexrelid = c2.oid AND pg_catalog.pg_table_is_visible(c.oid) UNION SELECT current_database() AS index_catalog, nc2.nspname AS index_schema, c2.relname AS index_name, current_database() AS table_catalog, nc.nspname AS table_schema, c.relname AS table_name, NULL, pg_get_indexdef (i.indexrelid, (ss.a).n, false), (ss.a).n FROM pg_catalog.pg_index i, (SELECT information_schema._pg_expandarray(indkey) AS a FROM pg_catalog.pg_index WHERE indexrelid = ##oid::guint) ss, pg_catalog.pg_class c INNER JOIN pg_namespace nc ON (c.relnamespace = nc.oid), pg_catalog.pg_class c2 INNER JOIN pg_namespace nc2 ON (c2.relnamespace = nc2.oid) WHERE i.indexrelid = ##oid::guint AND (ss.a).x = 0 AND c.oid = i.indrelid AND i.indexrelid = c2.oid AND pg_catalog.pg_table_is_visible(c.oid) order by 9"
};

/*
 * global static values, and
 * predefined statements' GdaStatement, all initialized in _gda_postgres_provider_meta_init()
 */
static GMutex init_mutex;
static GdaStatement **internal_stmt = NULL;
static GdaSet       *i_set = NULL;

/*
 * Meta initialization
 */
void
_gda_postgres_provider_meta_init (GdaServerProvider *provider)
{
	g_mutex_lock (&init_mutex);

	if (!internal_stmt) {
		InternalStatementItem i;
		GdaSqlParser *parser;

		if (provider)
			parser = gda_server_provider_internal_get_parser (provider);
		else
			parser = GDA_SQL_PARSER (g_object_new (GDA_TYPE_POSTGRES_PARSER, NULL));
		internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
		for (i = I_STMT_CATALOG; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
			internal_stmt[i] = gda_sql_parser_parse_string (parser, internal_sql[i], NULL, NULL);
			if (!internal_stmt[i])
				g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		}
		if (!provider)
			g_object_unref (parser);

		i_set = gda_set_new_inline (6, "cat", G_TYPE_STRING, "",
					    "name", G_TYPE_STRING, "",
					    "schema", G_TYPE_STRING, "",
					    "name2", G_TYPE_STRING, "",
					    "pos", G_TYPE_INT, "",
					    "oid", G_TYPE_UINT, 0);
	}

	g_mutex_unlock (&init_mutex);

}

typedef struct {
	GdaServerProviderConnectionData parent;
        gpointer reuseable;
} CncDataGen;

#define GDA_POSTGRES_GET_REUSEABLE_DATA(cdata) ((cdata) ? (GdaPostgresReuseable*) (((CncDataGen*) (cdata))->reuseable) : NULL)

gboolean
_gda_postgres_meta__info (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_CATALOG],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_information_schema_catalog_name,
							      error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, 
						   _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__btypes (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model, *proxy;
	gboolean retval = TRUE;
	gint i, nrows;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	/* use a prepared statement for the "base" model */
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_BTYPES],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_builtin_data_types, error);
	if (!model)
		return FALSE;

	/* use a proxy to customize @model */
	proxy = (GdaDataModel*) gda_data_proxy_new (model);
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, "sample-size", 0, NULL);
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *value;
		GType type;
		value = gda_data_model_get_value_at (model, 6, i, error);
		if (!value) {
			retval = FALSE;
			break;
		}
		
		guint oid = (guint) g_ascii_strtoull (g_value_get_string (value), NULL, 10);
		type = _gda_postgres_type_oid_to_gda (cnc, rdata, oid);
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
	if (retval) {
		gda_meta_store_set_reserved_keywords_func (store, 
							   _gda_postgres_reuseable_get_reserved_keywords_func
							   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, context->table_name, proxy, NULL, error, NULL);
	}
	g_object_unref (proxy);
	g_object_unref (model);

	return retval;
}

gboolean 
_gda_postgres_meta__udt (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_UDT_ALL],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_udt, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_udt (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *udt_catalog, const GValue *udt_schema)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), udt_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), udt_schema, error))
		return FALSE;
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_UDT],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_udt, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__udt_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_UDT_COLUMNS_ALL],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_udt_columns, error);

	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_udt_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (!gda_holder_set_value (gda_set_get_holder (i_set, "cat"), udt_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), udt_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), udt_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_UDT_COLUMNS],
							      i_set, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_udt_columns, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__enums (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			   G_GNUC_UNUSED GError **error)
{
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	/*
	if (rdata->version_float >= 8.3)
	        TO_IMPLEMENT;
	*/

	return TRUE;
}

gboolean
_gda_postgres_meta_enums (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			  G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			  G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *udt_catalog,
			  G_GNUC_UNUSED const GValue *udt_schema, G_GNUC_UNUSED const GValue *udt_name)
{
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	/*
	if (rdata->version_float >= 8.3)
		TO_IMPLEMENT;
	*/

	return TRUE;
}

gboolean
_gda_postgres_meta__domains (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_DOMAINS_ALL],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_domains, error);
	if (!model)
		return FALSE;
	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_domains (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *domain_catalog, const GValue *domain_schema)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), domain_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), domain_schema, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_DOMAINS],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_domains, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__constraints_dom (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_DOMAINS_CONSTRAINTS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_domain_constraints, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_constraints_dom (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				    GdaMetaStore *store, GdaMetaContext *context, GError **error,
				    const GValue *domain_catalog, const GValue *domain_schema, 
				    const GValue *domain_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), domain_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), domain_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), domain_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_DOMAINS_CONSTRAINTS],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_domain_constraints, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__el_types (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_EL_TYPES_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_element_types, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_el_types (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *specific_name)
{
	const gchar *cstr;
	GdaDataModel *model = NULL;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), specific_name, error))
		return FALSE;
	cstr = g_value_get_string (specific_name);
	if (*cstr == 'C') {
		/* check correct postgres server version */
		if (rdata->version_float < 8.2) {
			/* nothing for this version of PostgreSQL */
			return TRUE;
		}
		
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_EL_TYPES_COL],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_element_types, error);
	}
	else if (*cstr == 'D')
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_EL_TYPES_DOM],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_element_types, error);
	else if (*cstr == 'U')
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_EL_TYPES_UDT],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_element_types, error);
	else if (!strcmp (cstr, "ROUTINE_PAR"))
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_EL_TYPES_ROUT_PAR],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_element_types, error);
	else if (!strcmp (cstr, "ROUTINE_COL"))
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_EL_TYPES_ROUT_COL],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_element_types, error);
	else
		TO_IMPLEMENT;
	
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta__collations (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
				G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
				G_GNUC_UNUSED GError **error)
{
	/* nothing to do */
	return TRUE;
}

gboolean
_gda_postgres_meta_collations (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			       G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			       G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const
			       G_GNUC_UNUSED GValue *collation_catalog,
			       G_GNUC_UNUSED const GValue *collation_schema,
			       G_GNUC_UNUSED const GValue *collation_name_n)
{
	/* nothing to do */
	return TRUE;
}

gboolean
_gda_postgres_meta__character_sets (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
				    G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
				    G_GNUC_UNUSED GError **error)
{
	/* nothing to do */
	return TRUE;
}

gboolean
_gda_postgres_meta_character_sets (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
				   G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
				   G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *chset_catalog,
				   G_GNUC_UNUSED const GValue *chset_schema,
				   G_GNUC_UNUSED const GValue *chset_name_n)
{
	/* nothing to do */
	return TRUE;
}

gboolean
_gda_postgres_meta__schemata (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_SCHEMAS_ALL],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_schemata, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
	
	return retval;
}

gboolean 
_gda_postgres_meta_schemata (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			     const GValue *catalog_name, const GValue *schema_name_n)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), catalog_name, error))
		return FALSE;
	if (!schema_name_n) {
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_SCHEMAS],
								      i_set, 
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_schemata, error);
		if (!model)
			return FALSE;

		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	}
	else {
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), schema_name_n, error))
			return FALSE;
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_SCHEMA_NAMED],
								      i_set, 
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_schemata, error);
		if (!model)
			return FALSE;
		
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, context->table_name, model, "schema_name = ##name::string", error, 
						"name", schema_name_n, NULL);
	}
	g_object_unref (model);
	
	return retval;
}

gboolean
_gda_postgres_meta__tables_views (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *tables_model, *views_model;
	gboolean retval = TRUE;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if ((rdata->version_float == 0) && ! _gda_postgres_compute_version (cnc, rdata, error))
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	tables_model = gda_connection_statement_execute_select_full (cnc,
								     internal_stmt[I_STMT_TABLES_ALL],
								     NULL, 
								     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								     _col_types_tables, error);
	views_model = gda_connection_statement_execute_select_full (cnc,
								    internal_stmt[I_STMT_VIEWS_ALL],
								    NULL, 
								    GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								    _col_types_views, error);

	if (tables_model != NULL) {
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, "_tables", tables_model, NULL, error, NULL);
	  g_object_unref (tables_model);
	}
	if (views_model != NULL) {
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, "_views", views_model, NULL, error, NULL);
	  g_object_unref (views_model);
	}


	return retval;
}

gboolean 
_gda_postgres_meta_tables_views (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				 GdaMetaStore *store, GdaMetaContext *context, GError **error, 
				 const GValue *table_catalog, const GValue *table_schema, const GValue *table_name_n)
{
	GdaDataModel *tables_model, *views_model;
	gboolean retval = TRUE;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (!table_name_n) {
		tables_model = gda_connection_statement_execute_select_full (cnc,
									     internal_stmt[I_STMT_TABLES],
									     i_set, 
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									     _col_types_tables, error);
		if (!tables_model)
			return FALSE;
		views_model = gda_connection_statement_execute_select_full (cnc,
									    internal_stmt[I_STMT_VIEWS],
									    i_set, 
									    GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									    _col_types_views, error);
		if (!views_model) {
			g_object_unref (tables_model);
			return FALSE;
		}
	}
	else {
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name_n, error))
			return FALSE;
		tables_model = gda_connection_statement_execute_select_full (cnc,
									     internal_stmt[I_STMT_TABLE_NAMED],
									     i_set, 
									     GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									     _col_types_tables, error);
		if (!tables_model)
			return FALSE;
		views_model = gda_connection_statement_execute_select_full (cnc,
									    internal_stmt[I_STMT_VIEW_NAMED],
									    i_set, 
									    GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									    _col_types_views, error);
		if (!views_model) {
			g_object_unref (tables_model);
			return FALSE;
		}
	}

	GdaMetaContext c2;
	c2 = *context; /* copy contents, just because we need to modify @context->table_name */
	if (retval) {
		c2.table_name = "_tables";
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, &c2, tables_model, error);
	}
	if (retval) {
		c2.table_name = "_views";
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, &c2, views_model, error);
	}
	g_object_unref (tables_model);
	g_object_unref (views_model);

	return retval;
}

gboolean _gda_postgres_meta__columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model, *proxy;
	gboolean retval = TRUE;
	gint i, nrows;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	/* use a prepared statement for the "base" model */
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_COLUMNS_ALL],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_columns, error);
	if (!model)
		return FALSE;

	/* use a proxy to customize @model */
	proxy = (GdaDataModel*) gda_data_proxy_new (model);
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, "sample-size", 0, NULL);
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *value;
		GType type;

		/* GType */
		value = gda_data_model_get_value_at (model, 24, i, error);
		if (!value) {
			retval = FALSE;
			break;
		}
		
		guint oid = (guint) g_ascii_strtoull (g_value_get_string (value), NULL, 10);
		type = _gda_postgres_type_oid_to_gda (cnc, rdata, oid);
		if (type != G_TYPE_STRING) {
			GValue *v;
			g_value_set_string (v = gda_value_new (G_TYPE_STRING), g_type_name (type));
			retval = gda_data_model_set_value_at (proxy, 9, i, v, error);
			gda_value_free (v);
			if (!retval)
				break;
		}

		/* column default: remove the datatype cast on strings:
		 * 'abd'::character varying  => 'abd' */
		value = gda_data_model_get_value_at (model, 5, i, error);
		if (!value) {
			retval = FALSE;
			break;
		}
		
		if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
			const gchar *cstr;
			cstr = g_value_get_string (value);
			if (cstr && (*cstr == '\'')) {
				gint len;
				len = strlen (cstr);
				if (cstr [len-1] != '\'') {
					gchar *tmp = g_strdup (cstr);
					gint k;
					for (k = len - 1; k > 0; k--) {
						if (tmp [k] == '\'') {
							tmp [k+1] = 0;
							break;
						}
					}
					GValue *v;
					g_value_take_string (v = gda_value_new (G_TYPE_STRING), tmp);
					retval = gda_data_model_set_value_at (proxy, 5, i, v, error);
					gda_value_free (v);
					if (!retval)
						break;
				}
			}
		}
	}

	/* modify meta store with @proxy */
	if (retval) {
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), proxy, NULL, error, NULL);
	}
	g_object_unref (proxy);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			    const GValue *table_catalog, const GValue *table_schema, const GValue *table_name)
{
	GdaDataModel *model, *proxy;
	gboolean retval = TRUE;
	gint i, nrows;
	GdaPostgresReuseable *rdata;

	/* check correct postgres server version */
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	/* use a prepared statement for the "base" model */
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_COLUMNS_OF_TABLE],
							      i_set, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_columns, error);
	if (!model)
		return FALSE;

	/* use a proxy to customize @model */
	proxy = (GdaDataModel*) gda_data_proxy_new (model);
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, "sample-size", 0, NULL);
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *value;
		GType type;

		/* GType */
		value = gda_data_model_get_value_at (model, 24, i, error);
		if (!value) {
			retval = FALSE;
			break;
		}

		guint oid = (guint) g_ascii_strtoull (g_value_get_string (value), NULL, 10);
		type = _gda_postgres_type_oid_to_gda (cnc, rdata, oid);
		if (type != G_TYPE_STRING) {
			GValue *v;
			g_value_set_string (v = gda_value_new (G_TYPE_STRING), g_type_name (type));
			retval = gda_data_model_set_value_at (proxy, 9, i, v, error);
			gda_value_free (v);
			if (!retval)
				break;
		}

		/* column default: remove the datatype cast on strings:
		 * 'abd'::character varying  => 'abd' */
		value = gda_data_model_get_value_at (model, 5, i, error);
		if (!value) {
			retval = FALSE;
			break;
		}
		
		if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
			const gchar *cstr;
			cstr = g_value_get_string (value);
			if (cstr && (*cstr == '\'')) {
				gint len;
				len = strlen (cstr);
				if (cstr [len-1] != '\'') {
					gchar *tmp = g_strdup (cstr);
					gint k;
					for (k = len - 1; k > 0; k--) {
						if (tmp [k] == '\'') {
							tmp [k+1] = 0;
							break;
						}
					}
					GValue *v;
					g_value_take_string (v = gda_value_new (G_TYPE_STRING), tmp);
					retval = gda_data_model_set_value_at (proxy, 5, i, v, error);
					gda_value_free (v);
					if (!retval)
						break;
				}
			}
		}
	}

	/* modify meta store with @proxy */
	if (retval) {
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, context->table_name, proxy, 
						"table_schema = ##schema::string AND table_name = ##name::string", error, 
						"schema", table_schema, "name", table_name, NULL);
	}
	g_object_unref (proxy);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__view_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_VIEWS_COLUMNS_ALL],
							      NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_view_column_usage, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
	
	return retval;
}

gboolean 
_gda_postgres_meta_view_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *view_catalog, const GValue *view_schema, 
			      const GValue *view_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), view_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), view_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), view_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_VIEWS_COLUMNS],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_view_column_usage, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta__constraints_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_TABLES_CONSTRAINTS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_table_constraints, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_constraints_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				    GdaMetaStore *store, GdaMetaContext *context, GError **error,
				    const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
				    const GValue *constraint_name_n)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;

	if (!constraint_name_n) {
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_TABLES_CONSTRAINTS],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_table_constraints, error);
		if (!model)
			return FALSE;
		if (retval) {
			gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify (store, context->table_name, model, 
							"table_schema = ##schema::string AND table_name = ##name::string", 
							error, 
							"schema", table_schema, "name", table_name, NULL);
		}

	}
	else {
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name_n, error))
			return FALSE;
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_TABLES_CONSTRAINT_NAMED],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_table_constraints, error);
		if (!model)
			return FALSE;
		if (retval) {
			gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
			retval = gda_meta_store_modify (store, context->table_name, model, 
							"table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string", error, 
							"schema", table_schema, "name", table_name, "name2", constraint_name_n, NULL);
		}
	}

	g_object_unref (model);

	return retval;
}

gboolean 
_gda_postgres_meta__constraints_ref (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_REF_CONSTRAINTS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							     _col_types_referential_constraints, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_constraints_ref (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				    GdaMetaStore *store, GdaMetaContext *context, GError **error,
				    const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				    const GValue *constraint_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_REF_CONSTRAINTS],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							     _col_types_referential_constraints, error);
	if (!model)
		return FALSE;


	/* modify meta store */
	if (retval) {
    gchar *str = gda_meta_context_stringify (context);
    g_message ("Updating using: %s", str);
    g_free (str);
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, context->table_name, model, 
						"table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string", 
						error, 
						"schema", table_schema, "name", table_name, "name2", constraint_name, NULL);
	}
	g_object_unref (model);

	return retval;
}

gboolean 
_gda_postgres_meta__key_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_KEY_COLUMN_USAGE_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							     _col_types_key_column_usage, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean 
_gda_postgres_meta_key_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				const GValue *constraint_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_KEY_COLUMN_USAGE],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_key_column_usage, error);
	if (!model)
		return FALSE;


	/* modify meta store */
	if (retval) {
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, context->table_name, model, 
						"table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string", 
						error, 
						"schema", table_schema, "name", table_name, "name2", constraint_name, NULL);
	}
	g_object_unref (model);

	return retval;	
}

gboolean
_gda_postgres_meta__check_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_CHECK_COLUMN_USAGE_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_check_column_usage, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_check_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				  GdaMetaStore *store, GdaMetaContext *context, GError **error,
				  const GValue *table_catalog, const GValue *table_schema, 
				  const GValue *table_name, const GValue *constraint_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	GdaPostgresReuseable *rdata;

	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_CHECK_COLUMN_USAGE],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_check_column_usage, error);
	if (!model)
		return FALSE;


	/* modify meta store */
	if (retval) {
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, context->table_name, model, 
						"table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string", 
						error, 
						"schema", table_schema, "name", table_name, "name2", constraint_name, NULL);
	}
	g_object_unref (model);

	return retval;	
}

gboolean
_gda_postgres_meta__triggers (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_TRIGGERS_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_triggers, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_triggers (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *table_catalog, const GValue *table_schema, 
			     const GValue *table_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_TRIGGERS],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_triggers, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta__routines (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GdaStatement *stmt = NULL;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}
	stmt = internal_stmt[I_STMT_ROUTINES_ALL];
	if (rdata->version_float >= 11.0) {
		stmt = gda_connection_parse_sql_string (cnc, internal_sql_11[I_STMT_ROUTINES_ALL], NULL, error);
		if (stmt == NULL) {
			return FALSE;
		}
	}
	model = gda_connection_statement_execute_select_full (cnc,
							      stmt,
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_routines, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_routines (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *routine_catalog, const GValue *routine_schema, 
			     const GValue *routine_name_n)
{
	GdaDataModel *model;
	GdaStatement *stmt = NULL;
	gboolean retval = TRUE;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), routine_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), routine_schema, error))
		return FALSE;
	if (routine_name_n) {
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), routine_name_n, error))
			return FALSE;
		stmt = internal_stmt[I_STMT_ROUTINES_ONE];
		if (rdata->version_float >= 11.0) {
			stmt = gda_connection_parse_sql_string (cnc, internal_sql_11[I_STMT_ROUTINES_ONE], NULL, error);
			if (stmt == NULL) {
				return FALSE;
			}
		}
		model = gda_connection_statement_execute_select_full (cnc,
								      stmt,
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_routines, error);
	} else {
		stmt = internal_stmt[I_STMT_ROUTINES];
		if (rdata->version_float >= 11.0) {
			stmt = gda_connection_parse_sql_string (cnc, internal_sql_11[I_STMT_ROUTINES], NULL, error);
			if (stmt == NULL) {
				return FALSE;
			}
		}
		model = gda_connection_statement_execute_select_full (cnc,
								      stmt,
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      _col_types_routines, error);
	}
	if (model == NULL)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta__routine_col (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model, *proxy;
	gint ordinal_pos, i, nrows;
	const GValue *spname = NULL;
	gboolean retval;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_ROUTINE_COL_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_routine_columns, error);
	if (!model)
		return FALSE;

	/* use a proxy to customize @model */
	proxy = (GdaDataModel*) gda_data_proxy_new (model);
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, "sample-size", 0, NULL);
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

	if (retval) {
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), proxy, NULL, error, NULL);
	}
	g_object_unref (model);
	g_object_unref (proxy);
		
	return retval;
}

gboolean
_gda_postgres_meta_routine_col (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *rout_catalog, const GValue *rout_schema, 
				const GValue *rout_name, const GValue *col_name, const GValue *ordinal_position)
{
	GdaDataModel *model, *proxy;
	gint ordinal_pos, i, nrows;
	const GValue *spname = NULL;
	gboolean retval;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), rout_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), rout_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), rout_name, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), col_name, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name3"), col_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_ROUTINE_COL],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_routine_columns, error);
	if (!model)
		return FALSE;

	/* use a proxy to customize @model */
	proxy = (GdaDataModel*) gda_data_proxy_new (model);
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, "sample-size", 0, NULL);
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

	if (retval) {
		gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
		retval = gda_meta_store_modify_with_context (store, context, proxy, error);
	}
	g_object_unref (model);
	g_object_unref (proxy);
		
	return retval;
}

gboolean
_gda_postgres_meta__routine_par (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_ROUTINE_PAR_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_parameters, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta_routine_par (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *rout_catalog, const GValue *rout_schema, 
				const GValue *rout_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), rout_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), rout_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), rout_name, error))
		return FALSE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_ROUTINE_PAR],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      _col_types_parameters, error);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_postgres_meta__indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GType *types;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	gint tsize;
	tsize = sizeof (_col_types_table_indexes) / sizeof (GType);
	types = g_new (GType, tsize + 1);
	memcpy (types, _col_types_table_indexes, tsize * sizeof (GType));
	types [tsize-1] = G_TYPE_UINT;
	types [tsize] = G_TYPE_NONE;

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_INDEXES_ALL],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      types, error);
	g_free (types);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_postgres_meta_indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
				const GValue *index_name_n)
{
	GdaDataModel *model;
	gboolean retval = TRUE;
	GType *types;

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;

	gint tsize;
	tsize = sizeof (_col_types_table_indexes) / sizeof (GType);
	types = g_new (GType, tsize + 1);
	memcpy (types, _col_types_table_indexes, tsize * sizeof (GType));
	types [tsize-1] = G_TYPE_UINT;
	types [tsize] = G_TYPE_NONE;

	if (index_name_n) {
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), index_name_n, error)) {
			g_free (types);
			return FALSE;
		}
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_INDEXES_NAMED],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      types, error);
	}
	else
		model = gda_connection_statement_execute_select_full (cnc,
								      internal_stmt[I_STMT_INDEXES],
								      i_set,
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
								      types, error);
	g_free (types);
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

static GdaDataModel *
concatenate_index_details (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   G_GNUC_UNUSED GdaMetaStore *store, GdaDataModel *index_oid_model, GError **error)
{
	GdaDataModel *concat = NULL;
	gint i, nrows;
	nrows = gda_data_model_get_n_rows (index_oid_model);
	if (nrows == 0) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", _("could not determine the indexed columns for index"));
		return NULL;
	}
	for (i = 0; i < nrows; i++) {
		const GValue *value;
		value = gda_data_model_get_value_at (index_oid_model, 0, i, error);
		if (!value) {
			if (concat)
				g_object_unref (concat);
			return NULL;
		}

		if (G_VALUE_TYPE (value) == GDA_TYPE_NULL)
			continue;
		
		/* get index's columns details */
		GdaDataModel *tmpmodel;
		if (! gda_holder_set_value (gda_set_get_holder (i_set, "oid"), value, error)) {
			if (concat)
				g_object_unref (concat);
			return NULL;
		}
		tmpmodel = gda_connection_statement_execute_select_full (cnc,
									 internal_stmt[I_STMT_INDEXES_COLUMNS_FOR_INDEX],
									 i_set,
									 GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									 _col_types_index_column_usage, error);
		if (!tmpmodel) {
			if (concat)
				g_object_unref (concat);
			return NULL;
		}

		/* create a concatenated model of all the @tmpmodel's contents */
		if (!concat) {
			concat = (GdaDataModel*) gda_data_model_array_copy_model (tmpmodel, error);
			if (!concat) {
				g_object_unref (tmpmodel);
				return NULL;
			}
		}
		else {
			gint tnrows, ti, tncols;
			tnrows = gda_data_model_get_n_rows (tmpmodel);
			tncols = gda_data_model_get_n_columns (tmpmodel);
			for (ti = 0; ti < tnrows; ti++) {
				GList *list = NULL;
				gint tj;
				for (tj = tncols - 1; tj >= 0 ; tj--) {
					value = gda_data_model_get_value_at (tmpmodel, tj, ti, error);
					if (!value) {
						g_list_free (list);
						g_object_unref (tmpmodel);
						if (concat)
							g_object_unref (concat);
						return NULL;
					}
					list = g_list_prepend (list, (gpointer) value);
				}
				if (gda_data_model_append_values (concat, list, error) == -1) {
					g_list_free (list);
					g_object_unref (tmpmodel);
					if (concat)
						g_object_unref (concat);
					return NULL;
				}
				g_list_free (list);
			}
		}
	}
	return concat;
}

gboolean
_gda_postgres_meta__index_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model, *concat;
	GType index_oids_types[] = {G_TYPE_UINT, G_TYPE_NONE};

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}

	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_INDEXES_COLUMNS_GET_ALL_INDEXES],
							      NULL,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      index_oids_types, error);
	if (!model)
		return FALSE;

	concat = concatenate_index_details (prov, cnc, store, model, error);
	g_object_unref (model);
	if (!concat)
		return FALSE;

	gboolean retval;
	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify (store, gda_meta_context_get_table (context), concat, NULL, error, NULL);
	g_object_unref (concat);

	return retval;
}

gboolean
_gda_postgres_meta_index_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error,
			       const GValue *table_catalog, const GValue *table_schema,
			       const GValue *table_name, const GValue *index_name)
{
	GdaDataModel *model, *concat;
	GType index_oids_types[] = {G_TYPE_UINT, G_TYPE_NONE};

	/* check correct postgres server version */
	GdaPostgresReuseable *rdata;
	rdata = GDA_POSTGRES_GET_REUSEABLE_DATA (gda_connection_internal_get_provider_data_error (cnc, error));
	if (!rdata)
		return FALSE;
	if (rdata->version_float < 8.2) {
		/* nothing for this version of PostgreSQL */
		return TRUE;
	}
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "name2"), index_name, error))
		return FALSE;
	model = gda_connection_statement_execute_select_full (cnc,
							      internal_stmt[I_STMT_INDEXES_COLUMNS_GET_NAMED_INDEXES],
							      i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      index_oids_types, error);
	if (!model)
		return FALSE;

	concat = concatenate_index_details (prov, cnc, store, model, error);
	g_object_unref (model);
	if (!concat)
		return FALSE;

	gboolean retval;
	gda_meta_store_set_reserved_keywords_func (store, _gda_postgres_reuseable_get_reserved_keywords_func
						   ((GdaProviderReuseable*) rdata));
	retval = gda_meta_store_modify_with_context (store, context, concat, error);
	g_object_unref (concat);
	return retval;
}
