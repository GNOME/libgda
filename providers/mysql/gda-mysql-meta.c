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
        I_STMT_SCHEMAS_ALL,
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	/* I_STMT_CATALOG */
        "SELECT DATABASE()",

        /* I_STMT_BTYPES */
        "SELECT DISTINCT data_type, CONCAT(table_schema, '.', data_type),  CASE data_type WHEN 'int' THEN 'gint' WHEN 'bigint' THEN 'gint64' WHEN 'blob' THEN 'GdaBinary' WHEN 'date' THEN 'GDate' WHEN 'time' THEN 'GdaTime' WHEN 'double' THEN 'gdouble' WHEN 'timestamp' THEN 'GdaTimestamp' ELSE 'string' END  ,    CONCAT('Desc:', data_type), NULL, 0   FROM information_schema.columns WHERE table_schema = SCHEMA() ORDER BY 1",

        /* I_STMT_SCHEMAS */

        /* I_STMT_SCHEMAS_ALL */
	"SELECT catalog_name, schema_name, 'mysql', CASE schema_name WHEN 'information_schema' THEN TRUE WHEN 'mysql' THEN TRUE ELSE FALSE END   FROM information_schema.schemata",

        /* I_STMT_SCHEMA_NAMED */

        /* I_STMT_TABLES */

        /* I_STMT_TABLES_ALL */

        /* I_STMT_TABLE_NAMED */

        /* I_STMT_VIEWS */

        /* I_STMT_VIEWS_ALL */

        /* I_STMT_VIEW_NAMED */

        /* I_STMT_COLUMNS_OF_TABLE */

        /* I_STMT_COLUMNS_ALL */

        /* I_STMT_TABLES_CONSTRAINTS */

        /* I_STMT_TABLES_CONSTRAINTS_ALL */

        /* I_STMT_TABLES_CONSTRAINTS_NAMED */

        /* I_STMT_REF_CONSTRAINTS */

        /* I_STMT_REF_CONSTRAINTS_ALL */

        /* I_STMT_KEY_COLUMN_USAGE */

        /* I_STMT_KEY_COLUMN_USAGE_ALL */

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
 * predefined statements' GdaStatement
 */
static GdaStatement **internal_stmt;
static GdaSet        *i_set;

/* 
 * global static values
 */
static GdaSqlParser *internal_parser = NULL;
/* TO_ADD: other static values */


/*
 * Meta initialization
 */
void
_gda_mysql_provider_meta_init (GdaServerProvider  *provider)
{
	InternalStatementItem i;

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
}

gboolean
_gda_mysql_meta__info (GdaServerProvider  *prov,
		       GdaConnection      *cnc, 
		       GdaMetaStore       *store,
		       GdaMetaContext     *context,
		       GError            **error)
{
	g_print ("*** %s\n", __func__);
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
	g_print ("*** %s\n", __func__);
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

	/* /\* set internal holder's values from the arguments *\/ */
	/* gda_holder_set_value (gda_set_get_holder (i_set, "cat"), udt_catalog); */
	/* gda_holder_set_value (gda_set_get_holder (i_set, "schema"), udt_schema); */
	/* GdaDataModel *model; */
	/* gboolean retval; */
	/* /\* fill in @model, with something like: */
	/*  * model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_UDT], i_set, error); */
	/*  *\/ */
	/* if (model == NULL) */
	/* 	retval = FALSE; */
	/* else { */
	/* 	retval = gda_meta_store_modify_with_context (store, context, model, error); */
	/* 	g_object_unref (G_OBJECT(model)); */
	/* } */

	/* return retval; */
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
	g_print ("*** %s\n", __func__);
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
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__tables_views (GdaServerProvider  *prov,
			       GdaConnection      *cnc, 
			       GdaMetaStore       *store,
			       GdaMetaContext     *context,
			       GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
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
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__columns (GdaServerProvider  *prov,
			  GdaConnection      *cnc, 
			  GdaMetaStore       *store,
			  GdaMetaContext     *context,
			  GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
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
	TO_IMPLEMENT;
	return TRUE;
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
	TO_IMPLEMENT;
	return TRUE;
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
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_mysql_meta__key_columns (GdaServerProvider  *prov,
			      GdaConnection      *cnc, 
			      GdaMetaStore       *store,
			      GdaMetaContext     *context,
			      GError            **error)
{
	TO_IMPLEMENT;
	return TRUE;
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
	TO_IMPLEMENT;
	return TRUE;
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
