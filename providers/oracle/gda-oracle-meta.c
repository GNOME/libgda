/* GDA oracle provider
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-oracle.h"
#include "gda-oracle-meta.h"
#include "gda-oracle-provider.h"
#include <libgda/gda-meta-store.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>

/*
 * predefined statements' IDs
 */
typedef enum {
        I_STMT_CATALOG,
        I_STMT_SCHEMAS_ALL,
	I_STMT_SCHEMA_NAMED
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	/* I_STMT_CATALOG */
	"select ora_database_name from dual",

	/* I_STMT_SCHEMAS_ALL */
	"SELECT ora_database_name, username, username, CASE WHEN username = user THEN 1 ELSE 0 END FROM all_users",

	/* I_STMT_SCHEMA_NAMED */
	"SELECT ora_database_name, username, username, CASE WHEN username = user THEN 1 ELSE 0 END FROM all_users WHERE username = ##name::string",
};

/*
 * global static values, and
 * predefined statements' GdaStatement, all initialized in _gda_oracle_provider_meta_init()
 */
static GdaStatement **internal_stmt;
static GdaSet        *i_set;
static GdaSqlParser  *internal_parser = NULL;
/* TO_ADD: other static values */


/*
 * Meta initialization
 */
void
_gda_oracle_provider_meta_init (GdaServerProvider *provider)
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
	i_set = gda_set_new_inline (4, "cat", G_TYPE_STRING, "", 
				    "name", G_TYPE_STRING, "",
				    "schema", G_TYPE_STRING, "",
				    "name2", G_TYPE_STRING, "");

	g_static_mutex_unlock (&init_mutex);
}

gboolean
_gda_oracle_meta__info (GdaServerProvider *prov, GdaConnection *cnc, 
		      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;

	model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_CATALOG], NULL, 
							 error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_oracle_meta__btypes (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__udt (GdaServerProvider *prov, GdaConnection *cnc, 
		     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_udt (GdaServerProvider *prov, GdaConnection *cnc, 
		    GdaMetaStore *store, GdaMetaContext *context, GError **error,
		    const GValue *udt_catalog, const GValue *udt_schema)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	/* set internal holder's values from the arguments */
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), udt_catalog, error))
		return FALSE;
	if (! gda_holder_set_value (gda_set_get_holder (i_set, "schema"), udt_schema, error))
		return FALSE;

	TO_IMPLEMENT;
	/* fill in @model, with something like:
	 * model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_UDT], i_set, error);
	 */
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}


gboolean
_gda_oracle_meta__udt_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_oracle_meta_udt_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_oracle_meta__enums (GdaServerProvider *prov, GdaConnection *cnc, 
		       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_oracle_meta_enums (GdaServerProvider *prov, GdaConnection *cnc, 
		      GdaMetaStore *store, GdaMetaContext *context, GError **error,
		      const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	TO_IMPLEMENT;
	return TRUE;
}


gboolean
_gda_oracle_meta__domains (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_domains (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *domain_catalog, const GValue *domain_schema)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *domain_catalog, const GValue *domain_schema, 
				const GValue *domain_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__el_types (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_el_types (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *specific_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__collations (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_collations (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *collation_catalog, const GValue *collation_schema, 
			   const GValue *collation_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__character_sets (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_character_sets (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error,
			       const GValue *chset_catalog, const GValue *chset_schema, 
			       const GValue *chset_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	GType col_types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_NONE};

	model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_SCHEMAS_ALL], NULL, 
                                                              GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types, error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_oracle_meta_schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			 const GValue *catalog_name, const GValue *schema_name_n)
{
	GdaDataModel *model;
        gboolean retval;
	GType col_types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_NONE};

        if (! gda_holder_set_value (gda_set_get_holder (i_set, "cat"), catalog_name, error))
                return FALSE;
        if (!schema_name_n) {
		model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_SCHEMAS_ALL], NULL, 
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types, error);
                if (!model)
                        return FALSE;
                retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
        }
        else {
                if (! gda_holder_set_value (gda_set_get_holder (i_set, "name"), schema_name_n, error))
                        return FALSE;
		model = gda_connection_statement_execute_select_full (cnc, internal_stmt[I_STMT_SCHEMA_NAMED], i_set, 
								      GDA_STATEMENT_MODEL_RANDOM_ACCESS, col_types, error);
                if (!model)
                        return FALSE;

                retval = gda_meta_store_modify (store, context->table_name, model, "schema_name = ##name::string", error,
                                                "name", schema_name_n, NULL);
        }
        g_object_unref (model);

        return retval;
}

gboolean
_gda_oracle_meta__tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *table_catalog, const GValue *table_schema, 
			     const GValue *table_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__columns (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *table_catalog, const GValue *table_schema, 
			const GValue *table_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__view_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_view_cols (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error,
			  const GValue *view_catalog, const GValue *view_schema, 
			  const GValue *view_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error, 
				const GValue *table_catalog, const GValue *table_schema, 
				const GValue *table_name, const GValue *constraint_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				const GValue *constraint_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__key_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_key_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *table_catalog, const GValue *table_schema, 
			    const GValue *table_name, const GValue *constraint_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__check_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_check_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *table_catalog, const GValue *table_schema, 
			      const GValue *table_name, const GValue *constraint_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__triggers (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_triggers (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *table_catalog, const GValue *table_schema, 
			 const GValue *table_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__routines (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_routines (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *routine_catalog, const GValue *routine_schema, 
			 const GValue *routine_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__routine_col (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_routine_col (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *rout_catalog, const GValue *rout_schema, 
			    const GValue *rout_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta__routine_par (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_oracle_meta_routine_par (GdaServerProvider *prov, GdaConnection *cnc, 
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *rout_catalog, const GValue *rout_schema, 
			    const GValue *rout_name)
{
	TO_IMPLEMENT;
	return TRUE;
}
