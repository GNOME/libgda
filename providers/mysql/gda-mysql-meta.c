/* GDA mysql provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Carlos Savoretti <csavoretti@gmail.com>
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
#include "gda-mysql.h"
#include "gda-mysql-meta.h"
#include "gda-mysql-provider.h"
#include <libgda/gda-meta-store.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>

static gboolean append_a_row (GdaDataModel  *to_model,
			      GError       **error,
			      gint           nb,
			                     ...);

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
        I_STMT_TABLES_CONSTRAINTS_NAMED,
        I_STMT_REF_CONSTRAINTS,
        I_STMT_REF_CONSTRAINTS_ALL,
        I_STMT_KEY_COLUMN_USAGE,
        I_STMT_KEY_COLUMN_USAGE_ALL
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	/* I_STMT_CATALOG */
        "SELECT DATABASE()",

        /* I_STMT_BTYPES */
        "SELECT DISTINCT data_type, CONCAT(table_schema, '.', data_type), CASE data_type WHEN 'int' THEN 'gint' WHEN 'bigint' THEN 'gint64' WHEN 'blob' THEN 'GdaBinary' WHEN 'date' THEN 'GDate' WHEN 'time' THEN 'GdaTime' WHEN 'double' THEN 'gdouble' WHEN 'timestamp' THEN 'GdaTimestamp' ELSE 'string' END, CONCAT('Desc:', data_type), NULL, 0 FROM information_schema.columns WHERE table_schema = SCHEMA() ORDER BY 1",

        /* I_STMT_SCHEMAS */
	"SELECT IFNULL(catalog_name, schema_name), schema_name, NULL, CASE schema_name WHEN 'information_schema' THEN 1 ELSE 0 END FROM information_schema.schemata WHERE schema_name = ##cat::string",

        /* I_STMT_SCHEMAS_ALL */
	"SELECT IFNULL(catalog_name, schema_name), schema_name, NULL, CASE schema_name WHEN 'information_schema' THEN 1 ELSE 0 END FROM information_schema.schemata",

        /* I_STMT_SCHEMA_NAMED */
	"SELECT IFNULL(catalog_name, schema_name), schema_name, NULL, CASE schema_name WHEN 'information_schema' THEN 1 ELSE 0 END FROM information_schema.schemata WHERE schema_name = ##name::string",

        /* I_STMT_TABLES */
	"SELECT IFNULL(table_catalog, table_schema), table_schema, table_name, table_type, CASE table_type WHEN 'BASE TABLE' THEN 1 ELSE 0 END, table_comment, table_name, CONCAT(table_schema, '.', table_name), NULL FROM information_schema.tables WHERE table_catalog = ##cat::string AND schema_name = ##schema::string",

        /* I_STMT_TABLES_ALL */
	"SELECT IFNULL(table_catalog, table_schema), table_schema, table_name, table_type, CASE table_type WHEN 'BASE TABLE' THEN 1 ELSE 0 END, table_comment, table_name, CONCAT(table_schema, '.', table_name), NULL FROM information_schema.tables",

        /* I_STMT_TABLE_NAMED */
	"SELECT IFNULL(table_catalog, table_schema), table_schema, table_name, table_type, CASE table_type WHEN 'BASE TABLE' THEN 1 ELSE 0 END, table_comment, table_name, CONCAT(table_schema, '.', table_name), NULL FROM information_schema.tables WHERE table_catalog = ##cat::string AND schema_name = ##schema::string",

        /* I_STMT_VIEWS */
	"SELECT IFNULL(table_catalog, table_schema), table_schema, table_name, view_definition, check_option, is_updatable FROM information_schema.views ",

        /* I_STMT_VIEWS_ALL */
	"SELECT IFNULL(table_catalog, table_schema), table_schema, table_name, view_definition, check_option, is_updatable FROM information_schema.views ",

        /* I_STMT_VIEW_NAMED */
	"SELECT IFNULL(table_catalog, table_schema), table_schema, table_name, view_definition, check_option, is_updatable FROM information_schema.views WHERE table_catalog = ##cat::string AND schema_name = ##schema::string",

        /* I_STMT_COLUMNS_OF_TABLE */
	"SELECT IFNULL(table_catalog, table_schema), table_schema, table_name, column_name, ordinal_position, column_default, is_nullable, data_type, 'gchararray', character_maximum_length,character_octet_length, numeric_precision, numeric_scale, 0, character_set_name, character_set_name, character_set_name, collation_name, collation_name, collation_name, '', 1, column_comment FROM information_schema.columns WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string",

        /* I_STMT_COLUMNS_ALL */
	"SELECT IFNULL(table_catalog, table_schema), table_schema, table_name, column_name, ordinal_position, column_default, is_nullable, data_type, 'gchararray', character_maximum_length,character_octet_length, numeric_precision, numeric_scale, 0, character_set_name, character_set_name, character_set_name, collation_name, collation_name, collation_name, '', 1, column_comment FROM information_schema.columns",

        /* I_STMT_TABLES_CONSTRAINTS */
	"SELECT IFNULL(constraint_catalog, constraint_schema), constraint_schema, constraint_name, IFNULL(table_catalog, table_schema), table_schema, table_name, constraint_type, NULL, 0, 0 FROM information_schema.table_constraints WHERE constraint_catalog = ##cat::string AND constraint_schema = ##schema::string AND table_name = ##name::string",

        /* I_STMT_TABLES_CONSTRAINTS_ALL */
	"SELECT IFNULL(constraint_catalog, constraint_schema), constraint_schema, constraint_name, IFNULL(table_catalog, table_schema), table_schema, table_name, constraint_type, NULL, 0, 0 FROM information_schema.table_constraints",

        /* I_STMT_TABLES_CONSTRAINTS_NAMED */
	"SELECT IFNULL(constraint_catalog, constraint_schema), constraint_schema, constraint_name, IFNULL(table_catalog, table_schema), table_schema, table_name, constraint_type, NULL, 0, 0 FROM information_schema.table_constraints WHERE constraint_catalog = ##cat::string AND constraint_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string",

        /* I_STMT_REF_CONSTRAINTS */
	"SELECT IFNULL(t.constraint_catalog, t.constraint_schema), t.table_schema, t.table_name, t.constraint_name, IFNULL(k.constraint_catalog, k.constraint_schema), k.table_schema, k.table_name, k.constraint_name, NULL, NULL, NULL FROM information_schema.table_constraints t INNER JOIN information_schema.key_column_usage k ON t.table_schema=k.table_schema AND t.table_name=k.table_name AND t.constraint_name=k.constraint_name AND ((t.constraint_type = 'FOREIGN KEY' AND k.referenced_table_schema IS NOT NULL) OR (t.constraint_type != 'FOREIGN KEY' AND k.referenced_table_schema IS NULL)) WHERE t.constraint_catalog = ##cat::string AND t.table_schema = ##schema::string AND t.table_name = ##name::string AND t.constraint_name =  ##name2::string",

        /* I_STMT_REF_CONSTRAINTS_ALL */
	"SELECT IFNULL(t.constraint_catalog, t.constraint_schema), t.table_schema, t.table_name, t.constraint_name, IFNULL(k.constraint_catalog, k.constraint_schema), k.table_schema, k.table_name, k.constraint_name, NULL, NULL, NULL FROM information_schema.table_constraints t INNER JOIN information_schema.key_column_usage k ON t.table_schema=k.table_schema AND t.table_name=k.table_name AND t.constraint_name=k.constraint_name AND ((t.constraint_type = 'FOREIGN KEY' AND k.referenced_table_schema IS NOT NULL) OR (t.constraint_type != 'FOREIGN KEY' AND k.referenced_table_schema IS NULL))",

        /* I_STMT_KEY_COLUMN_USAGE */
	"SELECT IFNULL(table_catalog, table_schema), table_schema, table_name, constraint_name, column_name WHERE table_catalog = ##cat::string AND table_schema = ##schema::string AND table_name = ##name::string AND constraint_name = ##name2::string",

        /* I_STMT_KEY_COLUMN_USAGE_ALL */
	"SELECT IFNULL(table_catalog, table_schema), table_schema, table_name, constraint_name, column_name",

        /* I_STMT_UDT */

        /* I_STMT_UDT_ALL */

        /* I_STMT_UDT_COLUMNS */

        /* I_STMT_UDT_COLUMNS_ALL */

        /* I_STMT_DOMAINS */

        /* I_STMT_DOMAINS_ALL */

        /* I_STMT_DOMAINS_CONSTRAINTS */

        /* I_STMT_DOMAINS_CONSTRAINTS_ALL */

        /* I_STMT_VIEWS_COLUMNS */

        /* I_STMT_VIEWS_COLUMNS_ALL */

        /* I_STMT_TRIGGERS */

        /* I_STMT_TRIGGERS_ALL */

        /* I_STMT_EL_TYPES_COL */

        /* I_STMT_EL_TYPES_DOM */

        /* I_STMT_EL_TYPES_UDT */

        /* I_STMT_EL_TYPES_ALL */

};

/*
 * global static values, and
 * predefined statements' GdaStatement, all initialized in _gda_postgres_provider_meta_init()
 */
static GdaStatement **internal_stmt;
static GdaSet        *i_set;
static GdaSqlParser  *internal_parser = NULL;
/* TO_ADD: other static values */


/*
 * Meta initialization
 */
void
_gda_mysql_provider_meta_init (GdaServerProvider  *provider)
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

	/* initialize static values here */
	i_set = gda_set_new_inline (3, "cat", G_TYPE_STRING, "", 
				    "name", G_TYPE_STRING, "",
				    "schema", G_TYPE_STRING, "");

	g_static_mutex_unlock (&init_mutex);
}

gboolean
_gda_mysql_meta__info (GdaServerProvider  *prov,
		       GdaConnection      *cnc, 
		       GdaMetaStore       *store,
		       GdaMetaContext     *context,
		       GError            **error)
{
	GdaDataModel *model;
        gboolean retval;
        model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_CATALOG], NULL, error);
        if (model == NULL)
                retval = FALSE;
	else {
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
		g_object_unref (G_OBJECT(model));
	}

        return retval;
}

gboolean
_gda_mysql_meta__btypes (GdaServerProvider  *prov,
			 GdaConnection      *cnc, 
			 GdaMetaStore       *store,
			 GdaMetaContext     *context,
			 GError            **error)
{
	GType col_types[] = {
                G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_NONE
        };
        GdaDataModel *model;
        gboolean retval;
	model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_BTYPES], NULL,
                                                              GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types, error);
        if (model == NULL)
                retval = FALSE;
	else {
                retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
		g_object_unref (G_OBJECT(model));
	}

        return retval;
}

gboolean
_gda_mysql_meta__udt (GdaServerProvider  *prov,
		      GdaConnection      *cnc, 
		      GdaMetaStore       *store,
		      GdaMetaContext     *context,
		      GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_udt (GdaServerProvider  *prov,
		     GdaConnection      *cnc, 
		     GdaMetaStore       *store,
		     GdaMetaContext     *context,
		     GError            **error,
		     const GValue       *udt_catalog,
		     const GValue       *udt_schema)
{
	TO_IMPLEMENT;
	return TRUE;
}


gboolean
_gda_mysql_meta__udt_cols (GdaServerProvider  *prov,
			   GdaConnection      *cnc, 
			   GdaMetaStore       *store,
			   GdaMetaContext     *context,
			   GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_mysql_meta_udt_cols (GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error,
			  const GValue       *udt_catalog,
			  const GValue       *udt_schema,
			  const GValue       *udt_name)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_mysql_meta__enums (GdaServerProvider  *prov,
			GdaConnection      *cnc, 
			GdaMetaStore       *store,
			GdaMetaContext     *context,
			GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_mysql_meta_enums (GdaServerProvider  *prov,
		       GdaConnection      *cnc, 
		       GdaMetaStore       *store,
		       GdaMetaContext     *context,
		       GError            **error,
		       const GValue       *udt_catalog,
		       const GValue       *udt_schema,
		       const GValue       *udt_name)
{
	TO_IMPLEMENT;
	return TRUE;
}


gboolean
_gda_mysql_meta__domains (GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_domains (GdaServerProvider  *prov,
			 GdaConnection      *cnc, 
			 GdaMetaStore       *store,
			 GdaMetaContext     *context,
			 GError            **error,
			 const GValue       *domain_catalog,
			 const GValue       *domain_schema)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__constraints_dom (GdaServerProvider  *prov,
				  GdaConnection      *cnc, 
				  GdaMetaStore       *store,
				  GdaMetaContext     *context,
				  GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_constraints_dom (GdaServerProvider  *prov,
				 GdaConnection      *cnc, 
				 GdaMetaStore       *store,
				 GdaMetaContext     *context,
				 GError            **error,
				 const GValue       *domain_catalog,
				 const GValue       *domain_schema, 
				 const GValue       *domain_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__el_types (GdaServerProvider  *prov,
			   GdaConnection      *cnc, 
			   GdaMetaStore       *store,
			   GdaMetaContext     *context,
			   GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__collations (GdaServerProvider  *prov,
			     GdaConnection      *cnc, 
			     GdaMetaStore       *store,
			     GdaMetaContext     *context,
			     GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_collations (GdaServerProvider  *prov,
			    GdaConnection      *cnc, 
			    GdaMetaStore       *store,
			    GdaMetaContext     *context,
			    GError            **error,
			    const GValue       *collation_catalog,
			    const GValue       *collation_schema, 
			    const GValue       *collation_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__character_sets (GdaServerProvider  *prov,
				 GdaConnection      *cnc, 
				 GdaMetaStore       *store,
				 GdaMetaContext     *context,
				 GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_character_sets (GdaServerProvider  *prov,
				GdaConnection      *cnc, 
				GdaMetaStore       *store,
				GdaMetaContext     *context,
				GError            **error,
				const GValue       *chset_catalog,
				const GValue       *chset_schema, 
				const GValue       *chset_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__schemata (GdaServerProvider  *prov,
			   GdaConnection      *cnc, 
			   GdaMetaStore       *store,
			   GdaMetaContext     *context,
			   GError            **error)
{
	// TO_IMPLEMENT;

	GdaDataModel *model;
	gboolean retval;
	model = gda_connection_statement_execute_select	(cnc, internal_stmt[I_STMT_SCHEMAS_ALL], i_set, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_schemata (GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error, 
			  const GValue       *catalog_name,
			  const GValue       *schema_name_n)
{
	// TO_IMPLEMENT;
	
	GdaDataModel *model;
	gboolean retval;

	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), catalog_name);
	if (!schema_name_n) {
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_SCHEMAS], i_set, error);
		if (model == NULL)
			retval = FALSE;
		else {
			retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
			g_object_unref (G_OBJECT(model));
		}
	} else {
		gda_holder_set_value (gda_set_get_holder (i_set, "name"), schema_name_n);
		model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_SCHEMA_NAMED], i_set, error);
		if (model == NULL)
			retval = FALSE;
		else {
			retval = gda_meta_store_modify (store, context->table_name, model,
							"schema_name=##name::string", error, "name", schema_name_n, NULL);
			g_object_unref (G_OBJECT(model));
		}
	}
	
	return retval;
}

gboolean
_gda_mysql_meta__tables_views (GdaServerProvider  *prov,
			       GdaConnection      *cnc, 
			       GdaMetaStore       *store,
			       GdaMetaContext     *context,
			       GError            **error)
{
	// TO_IMPLEMENT;
	
	GdaDataModel *model_tables, *model_views;
	gboolean retval;
	/* Check correct mysql server version. */
	MysqlConnectionData *cdata;
	cdata = (MysqlConnectionData *) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	/* Copy contents, just because we need to modify @context->table_name */
	GdaMetaContext copy = *context;

	model_tables = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TABLES_ALL], i_set, error);
	if (model_tables == NULL)
		retval = FALSE;
	else {
		copy.table_name = "_tables";
		retval = gda_meta_store_modify_with_context (store, &copy, model_tables, error);
		g_object_unref (G_OBJECT(model_tables));
	}

	model_views = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_VIEWS_ALL], i_set, error);
	if (model_views == NULL)
		retval = FALSE;
	else {
		copy.table_name = "_views";
		retval = gda_meta_store_modify_with_context (store, &copy, model_views, error);
		g_object_unref (G_OBJECT(model_views));
	}
	
	return retval;
}

gboolean
_gda_mysql_meta_tables_views (GdaServerProvider  *prov,
			      GdaConnection      *cnc, 
			      GdaMetaStore       *store,
			      GdaMetaContext     *context,
			      GError            **error,
			      const GValue       *table_catalog,
			      const GValue       *table_schema, 
			      const GValue       *table_name_n)
{
	// TO_IMPLEMENT;
	
	GdaDataModel *model_tables, *model_views;
	gboolean retval;
	/* Check correct mysql server version. */
	MysqlConnectionData *cdata;
	cdata = (MysqlConnectionData *) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     _("Mysql version 5.0 at least is required"));
		return FALSE;
	}


	/* Copy contents, just because we need to modify @context->table_name */
	GdaMetaContext copy = *context;

	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog);
	gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema);
	if (!table_name_n) {
		model_tables = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TABLES], i_set, error);
		if (model_tables == NULL)
			retval = FALSE;
		else {
			retval = gda_meta_store_modify_with_context (store, &copy, model_tables, NULL);
			g_object_unref (G_OBJECT(model_tables));
		}

		model_views = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_VIEWS], i_set, error);
		if (model_views == NULL)
			retval = FALSE;
		else {
			retval = gda_meta_store_modify_with_context (store, &copy, model_views, NULL);
			g_object_unref (G_OBJECT(model_views));
		}

	} else {
		gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name_n);
		model_tables = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_TABLE_NAMED], i_set, error);
		if (model_tables == NULL)
			retval = FALSE;
		else {
			copy.table_name = "_tables";
			retval = gda_meta_store_modify_with_context (store, &copy, model_tables, error);
			g_object_unref (G_OBJECT(model_tables));
		}

		model_views = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_VIEW_NAMED], i_set, error);
		if (model_views == NULL)
			retval = FALSE;
		else {
			copy.table_name = "_views";
			retval = gda_meta_store_modify_with_context (store, &copy, model_views, error);
			g_object_unref (G_OBJECT(model_views));
		}

	}
	
	return retval;
}

/**
 * map_mysql_to_gda:
 * @value: a #GValue string.
 *
 * It maps a mysql type to a gda type.  This means that when a
 * mysql type is given, it will return its mapped gda type.
 *
 * Returns a newly created GValue string.
 */
static inline GValue *
map_mysql_to_gda (const GValue  *value)
{
	const gchar *string = g_value_get_string (value);
	GValue *newvalue;
	const gchar *newstring;
	if (strcmp (string, "bool") == 0)
		newstring = "gboolean";
	else
	if (strcmp (string, "blob") == 0)
		newstring = "GdaBinary";
	else
	if (strcmp (string, "bigint") == 0)
		newstring = "gint64";
	else
	if (strcmp (string, "bigint unsigned") == 0)
		newstring = "guint64";
	else
	if (strcmp (string, "char") == 0)
		newstring = "gchar";
	else
	if (strcmp (string, "date") == 0)
		newstring = "GDate";
	else
	if (strcmp (string, "datetime") == 0)
		newstring = "GdaTimestamp";
	else
	if (strcmp (string, "decimal") == 0)
		newstring = "GdaNumeric";
	else
	if (strcmp (string, "double") == 0)
		newstring = "gdouble";
	else
	if (strcmp (string, "double unsigned") == 0)
		newstring = "double";
	else
	if (strcmp (string, "enum") == 0)
		newstring = "gchararray";
	else
	if (strcmp (string, "float") == 0)
		newstring = "gfloat";
	else
	if (strcmp (string, "float unsigned") == 0)
		newstring = "gfloat";
	else
	if (strcmp (string, "int") == 0)
		newstring = "int";
	else
	if (strcmp (string, "unsigned int") == 0)
		newstring = "guint";
	else
	if (strcmp (string, "long") == 0)
		newstring = "glong";
	else
	if (strcmp (string, "unsigned long") == 0)
		newstring = "gulong";
	else
	if (strcmp (string, "longblob") == 0)
		newstring = "GdaBinary";
	else
	if (strcmp (string, "longtext") == 0)
		newstring = "GdaBinary";
	else
	if (strcmp (string, "mediumint") == 0)
		newstring = "gint";
	else
	if (strcmp (string, "mediumint unsigned") == 0)
		newstring = "guint";
	else
	if (strcmp (string, "mediumblob") == 0)
		newstring = "GdaBinary";
	else
	if (strcmp (string, "mediumtext") == 0)
		newstring = "GdaBinary";
	else
	if (strcmp (string, "set") == 0)
		newstring = "gchararray";
	else
	if (strcmp (string, "smallint") == 0)
		newstring = "gshort";
	else
	if (strcmp (string, "smallint unsigned") == 0)
		newstring = "gushort";
	else
	if (strcmp (string, "text") == 0)
		newstring = "GdaBinary";
	else
	if (strcmp (string, "tinyint") == 0)
		newstring = "gchar";
	else
	if (strcmp (string, "tinyint unsigned") == 0)
		newstring = "guchar";
	else
	if (strcmp (string, "tinyblob") == 0)
		newstring = "GdaBinary";
	else
	if (strcmp (string, "time") == 0)
		newstring = "GdaTime";
	else
	if (strcmp (string, "timestamp") == 0)
		newstring = "GdaTimestamp";
	else
	if (strcmp (string, "varchar") == 0)
		newstring = "gchararray";
	else
	if (strcmp (string, "year") == 0)
		newstring = "gint";
	else
		newstring = "";

	g_value_set_string (newvalue = gda_value_new (G_TYPE_STRING),
			    newstring);
	
	
	return newvalue;
}

gboolean
_gda_mysql_meta__columns (GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error)
{
	// TO_IMPLEMENT;

	/* GType col_types[] = { */
	/* 	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, */
	/* 	G_TYPE_INT, G_TYPE_NONE */
	/* }; */
	GdaDataModel *model, *proxy;
	gboolean retval;
	/* Check correct mysql server version. */
	MysqlConnectionData *cdata;
	cdata = (MysqlConnectionData *) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	/* Use a prepared statement for the "base" model. */
	model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_COLUMNS_ALL], i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS, /* col_types */ NULL, error);
	if (model == NULL)
		retval = FALSE;
	else {
		proxy = (GdaDataModel *) gda_data_proxy_new (model);
		gda_data_proxy_set_sample_size ((GdaDataProxy *) proxy, 0);
		gint n_rows = gda_data_model_get_n_rows (model);
		gint i;
		for (i = 0; i < n_rows; ++i) {
			const GValue *value = gda_data_model_get_value_at (model, 7, i);

			GValue *newvalue = map_mysql_to_gda (value);

			retval = gda_data_model_set_value_at (GDA_DATA_MODEL(proxy), 8, i, newvalue, error);
			gda_value_free (newvalue);
			if (retval == 0)
				break;

		}

		if (retval != 0)
			retval = gda_meta_store_modify_with_context (store, context, proxy, error);
		g_object_unref (G_OBJECT(proxy));
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

gboolean
_gda_mysql_meta_columns (GdaServerProvider  *prov,
			 GdaConnection      *cnc, 
			 GdaMetaStore       *store,
			 GdaMetaContext     *context,
			 GError            **error,
			 const GValue       *table_catalog,
			 const GValue       *table_schema, 
			 const GValue       *table_name)
{
	// TO_IMPLEMENT;

	/* GType col_types[] = { */
	/* 	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, */
	/* 	G_TYPE_INT, G_TYPE_NONE */
	/* }; */
	GdaDataModel *model, *proxy;
	gboolean retval;
	/* Check correct mysql server version. */
	MysqlConnectionData *cdata;
	cdata = (MysqlConnectionData *) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	/* Use a prepared statement for the "base" model. */
	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog);
	gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema);
	gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name);
	model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_COLUMNS_OF_TABLE], i_set,
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS, /* col_types */ NULL, error);
	if (model == NULL)
		retval = FALSE;
	else {
		proxy = (GdaDataModel *) gda_data_proxy_new (model);
		gda_data_proxy_set_sample_size ((GdaDataProxy *) proxy, 0);
		gint n_rows = gda_data_model_get_n_rows (model);
		gint i;
		for (i = 0; i < n_rows; ++i) {
			const GValue *value = gda_data_model_get_value_at (model, 7, i);

			GValue *newvalue = map_mysql_to_gda (value);

			retval = gda_data_model_set_value_at (GDA_DATA_MODEL(proxy), 8, i, newvalue, error);
			gda_value_free (newvalue);
			if (retval == 0)
				break;

		}

		if (retval != 0)
			retval = gda_meta_store_modify (store, context->table_name, proxy,
							"table_schema=##schema::string AND table_name=##name::string", error,
							"schema", table_schema, "name", table_name, NULL);
		g_object_unref (G_OBJECT(proxy));
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

gboolean
_gda_mysql_meta__view_cols (GdaServerProvider  *prov,
			    GdaConnection      *cnc, 
			    GdaMetaStore       *store,
			    GdaMetaContext     *context,
			    GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_view_cols (GdaServerProvider  *prov,
			   GdaConnection      *cnc, 
			   GdaMetaStore       *store,
			   GdaMetaContext     *context,
			   GError            **error,
			   const GValue       *view_catalog,
			   const GValue       *view_schema, 
			   const GValue       *view_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__constraints_tab (GdaServerProvider  *prov,
				  GdaConnection      *cnc, 
				  GdaMetaStore       *store,
				  GdaMetaContext     *context,
				  GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_constraints_tab (GdaServerProvider  *prov,
				 GdaConnection      *cnc, 
				 GdaMetaStore       *store,
				 GdaMetaContext     *context,
				 GError            **error, 
				 const GValue       *table_catalog,
				 const GValue       *table_schema, 
				 const GValue       *table_name,
				 const GValue       *constraint_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__constraints_ref (GdaServerProvider  *prov,
				  GdaConnection      *cnc, 
				  GdaMetaStore       *store,
				  GdaMetaContext     *context,
				  GError            **error)
{
	// TO_IMPLEMENT;

	GdaDataModel *model;
	gboolean retval;
	model = gda_connection_statement_execute_select	(cnc, internal_stmt[I_STMT_REF_CONSTRAINTS_ALL], NULL, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_constraints_ref (GdaServerProvider  *prov,
				 GdaConnection      *cnc, 
				 GdaMetaStore       *store,
				 GdaMetaContext     *context,
				 GError            **error,
				 const GValue       *table_catalog,
				 const GValue       *table_schema,
				 const GValue       *table_name, 
				 const GValue       *constraint_name)
{
	// TO_IMPLEMENT;

	GdaDataModel *model;
	gboolean retval;
	/* Check correct mysql server version. */
	MysqlConnectionData *cdata;
	cdata = (MysqlConnectionData *) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	/* Use a prepared statement for the "base" model. */
	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog);
	gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema);
	gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name);
	gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name);
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_REF_CONSTRAINTS], i_set, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify (store, context->table_name, model,
						"table_schema=##schema::string AND table_name=##name::string AND constraint_name=##name2::string", error,
						"schema", table_schema, "name", table_name, "name2", constraint_name, NULL);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

gboolean
_gda_mysql_meta__key_columns (GdaServerProvider  *prov,
			      GdaConnection      *cnc, 
			      GdaMetaStore       *store,
			      GdaMetaContext     *context,
			      GError            **error)
{
	// TO_IMPLEMENT;

	GdaDataModel *model;
	gboolean retval;
	model = gda_connection_statement_execute_select	(cnc, internal_stmt[I_STMT_KEY_COLUMN_USAGE_ALL], NULL, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify_with_context (store, context, model, error);
		g_object_unref (G_OBJECT(model));
	}

	return retval;
}

gboolean
_gda_mysql_meta_key_columns (GdaServerProvider  *prov,
			     GdaConnection      *cnc, 
			     GdaMetaStore       *store,
			     GdaMetaContext     *context,
			     GError            **error,
			     const GValue       *table_catalog,
			     const GValue       *table_schema, 
			     const GValue       *table_name,
			     const GValue       *constraint_name)
{
	// TO_IMPLEMENT;

	GdaDataModel *model;
	gboolean retval;
	/* Check correct mysql server version. */
	MysqlConnectionData *cdata;
	cdata = (MysqlConnectionData *) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return FALSE;
	if (cdata->version_long < 50000) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
			     _("Mysql version 5.0 at least is required"));
		return FALSE;
	}

	/* Use a prepared statement for the "base" model. */
	gda_holder_set_value (gda_set_get_holder (i_set, "cat"), table_catalog);
	gda_holder_set_value (gda_set_get_holder (i_set, "schema"), table_schema);
	gda_holder_set_value (gda_set_get_holder (i_set, "name"), table_name);
	gda_holder_set_value (gda_set_get_holder (i_set, "name2"), constraint_name);
	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_KEY_COLUMN_USAGE], i_set, error);
	if (model == NULL)
		retval = FALSE;
	else {
		retval = gda_meta_store_modify (store, context->table_name, model,
						"table_schema=##schema::string AND table_name=##name::string AND constraint_name=##name2::string", error,
						"schema", table_schema, "name", table_name, "name2", constraint_name, NULL);
		g_object_unref (G_OBJECT(model));

	}

	return retval;
}

gboolean
_gda_mysql_meta__check_columns (GdaServerProvider  *prov,
				GdaConnection      *cnc, 
				GdaMetaStore       *store,
				GdaMetaContext     *context,
				GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_check_columns (GdaServerProvider  *prov,
			       GdaConnection      *cnc, 
			       GdaMetaStore       *store,
			       GdaMetaContext     *context,
			       GError            **error,
			       const GValue       *table_catalog,
			       const GValue       *table_schema, 
			       const GValue       *table_name,
			       const GValue       *constraint_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__triggers (GdaServerProvider  *prov,
			   GdaConnection      *cnc, 
			   GdaMetaStore       *store,
			   GdaMetaContext     *context,
			   GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_triggers (GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error,
			  const GValue       *table_catalog,
			  const GValue       *table_schema, 
			  const GValue       *table_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__routines (GdaServerProvider  *prov,
			   GdaConnection      *cnc, 
			   GdaMetaStore       *store,
			   GdaMetaContext     *context,
			   GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_routines (GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error,
			  const GValue       *routine_catalog,
			  const GValue       *routine_schema, 
			  const GValue       *routine_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__routine_col (GdaServerProvider  *prov,
			      GdaConnection      *cnc, 
			      GdaMetaStore       *store,
			      GdaMetaContext     *context,
			      GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_routine_col (GdaServerProvider  *prov,
			     GdaConnection      *cnc, 
			     GdaMetaStore       *store,
			     GdaMetaContext     *context,
			     GError            **error,
			     const GValue       *rout_catalog,
			     const GValue       *rout_schema, 
			     const GValue       *rout_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__routine_par (GdaServerProvider  *prov,
			      GdaConnection      *cnc, 
			      GdaMetaStore       *store,
			      GdaMetaContext     *context,
			      GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta_routine_par (GdaServerProvider  *prov,
			     GdaConnection      *cnc, 
			     GdaMetaStore       *store,
			     GdaMetaContext     *context,
			     GError            **error,
			     const GValue       *rout_catalog,
			     const GValue       *rout_schema, 
			     const GValue       *rout_name)
{
	TO_IMPLEMENT;
	return TRUE;
}
