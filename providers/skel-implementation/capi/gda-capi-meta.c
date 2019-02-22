/*
 * Copyright (C) YEAR The GNOME Foundation.
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include "gda-capi.h"
#include "gda-capi-meta.h"
#include "gda-capi-provider.h"
#include <libgda/gda-meta-store.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/sqlite/keywords_hash.h>
#include "keywords_hash.code" /* this one is dynamically generated */

/*
 * predefined statements' IDs
 */
typedef enum {
        I_STMT_1,
} InternalStatementItem;


/*
 * predefined statements' SQL
 */
static gchar *internal_sql[] = {
	"SQL for I_STMT_1"
};

/*
 * global static values, and
 * predefined statements' GdaStatement, all initialized in _gda_capi_provider_meta_init()
 */
static GMutex init_mutex;
static GdaStatement **internal_stmt = NULL;
static GdaSet        *i_set = NULL;
static GdaSqlParser  *internal_parser = NULL;
/* TO_ADD: other static values */


/*
 * Meta initialization
 */
void
_gda_capi_provider_meta_init (GdaServerProvider *provider)
{
	g_mutex_lock (&init_mutex);

	if (!internal_stmt) {
		InternalStatementItem i;
		internal_parser = gda_server_provider_internal_get_parser (provider);
		internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
		for (i = I_STMT_1; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
			internal_stmt[i] = gda_sql_parser_parse_string (internal_parser, internal_sql[i], NULL, NULL);
			if (!internal_stmt[i])
				g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		}

		/* initialize static values here */
		i_set = gda_set_new_inline (4, "cat", G_TYPE_STRING, "",
					    "name", G_TYPE_STRING, "",
					    "schema", G_TYPE_STRING, "",
					    "name2", G_TYPE_STRING, "");
	}

#ifdef GDA_DEBUG
	test_keywords ();
#endif

	g_mutex_unlock (&init_mutex);

}

gboolean
_gda_capi_meta__info (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
		      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model = NULL;
	gboolean retval;

	TO_IMPLEMENT;
	/* fill in @model, with something like:
	 * model = gda_connection_statement_execute_select (cnc, internal_stmt[I_STMT_1], NULL, 
	 *                                                  error);
	 */
	if (!model)
		return FALSE;

	gda_meta_store_set_reserved_keywords_func (store, is_keyword);
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_capi_meta__btypes (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__udt (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
		     G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context, G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_udt (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
		    GdaMetaStore *store, GdaMetaContext *context, GError **error,
		    const GValue *udt_catalog, const GValue *udt_schema)
{
	GdaDataModel *model = NULL;
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

	gda_meta_store_set_reserved_keywords_func (store, is_keyword);
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);

	return retval;
}


gboolean
_gda_capi_meta__udt_cols (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			  G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			  G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_capi_meta_udt_cols (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			 G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			 G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *udt_catalog,
			 G_GNUC_UNUSED const GValue *udt_schema, G_GNUC_UNUSED const GValue *udt_name)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_capi_meta__enums (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
		       G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
		       G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;	
}

gboolean
_gda_capi_meta_enums (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
		      G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
		      G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *udt_catalog,
		      G_GNUC_UNUSED const GValue *udt_schema, G_GNUC_UNUSED const GValue *udt_name)
{
	TO_IMPLEMENT;
	return TRUE;
}


gboolean
_gda_capi_meta__domains (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			 G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			 G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_domains (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *domain_catalog,
			G_GNUC_UNUSED const GValue *domain_schema)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__constraints_dom (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
				 G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
				 G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_constraints_dom (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
				G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
				G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *domain_catalog,
				G_GNUC_UNUSED const GValue *domain_schema,
				G_GNUC_UNUSED const GValue *domain_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__el_types (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			  G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			  G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_el_types (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			 G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			 G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *specific_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__collations (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			    G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			    G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_collations (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			   G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			   G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *collation_catalog,
			   G_GNUC_UNUSED const GValue *collation_schema,
			   G_GNUC_UNUSED const GValue *collation_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__character_sets (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
				G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
				G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_character_sets (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			       G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			       G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *chset_catalog,
			       G_GNUC_UNUSED const GValue *chset_schema,
			       G_GNUC_UNUSED const GValue *chset_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__schemata (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			  G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			  G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_schemata (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			 G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			 G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *catalog_name,
			 G_GNUC_UNUSED const GValue *schema_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__tables_views (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			      G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			      G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_tables_views (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			     G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			     G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *table_catalog,
			     G_GNUC_UNUSED const GValue *table_schema,
			     G_GNUC_UNUSED const GValue *table_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__columns (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			 G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			 G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_columns (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *table_catalog,
			G_GNUC_UNUSED const GValue *table_schema, G_GNUC_UNUSED const GValue *table_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__view_cols (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			   G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			   G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_view_cols (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			  G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			  G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *view_catalog,
			  G_GNUC_UNUSED const GValue *view_schema, G_GNUC_UNUSED const GValue *view_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__constraints_tab (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
				 G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
				 G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_constraints_tab (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
				G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
				G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *table_catalog,
				G_GNUC_UNUSED const GValue *table_schema,
				G_GNUC_UNUSED const GValue *table_name,
				G_GNUC_UNUSED const GValue *constraint_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__constraints_ref (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
				 G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
				 G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_constraints_ref (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
				G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
				G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *table_catalog,
				G_GNUC_UNUSED const GValue *table_schema,
				G_GNUC_UNUSED const GValue *table_name,
				G_GNUC_UNUSED const GValue *constraint_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__key_columns (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			     G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			     G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_key_columns (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			    G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			    G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *table_catalog,
			    G_GNUC_UNUSED const GValue *table_schema,
			    G_GNUC_UNUSED const GValue *table_name,
			    G_GNUC_UNUSED const GValue *constraint_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__check_columns (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			       G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			       G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_check_columns (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			      G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			      G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *table_catalog,
			      G_GNUC_UNUSED const GValue *table_schema,
			      G_GNUC_UNUSED const GValue *table_name,
			      G_GNUC_UNUSED const GValue *constraint_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__triggers (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			  G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			  G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_triggers (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			 G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			 G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *table_catalog,
			 G_GNUC_UNUSED const GValue *table_schema, G_GNUC_UNUSED const GValue *table_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__routines (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			  G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			  G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_routines (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			 G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			 G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *routine_catalog,
			 G_GNUC_UNUSED const GValue *routine_schema,
			 G_GNUC_UNUSED const GValue *routine_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__routine_col (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			     G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			     G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_routine_col (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			    G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			    G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *rout_catalog,
			    G_GNUC_UNUSED const GValue *rout_schema, G_GNUC_UNUSED const GValue *rout_name,
			    G_GNUC_UNUSED const GValue *col_name, G_GNUC_UNUSED const GValue *ordinal_position)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__routine_par (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			     G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context, G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_routine_par (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			    G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			    G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *rout_catalog,
			    G_GNUC_UNUSED const GValue *rout_schema, G_GNUC_UNUSED const GValue *rout_name)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			     G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			     G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			    G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			    G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *table_catalog,
			    G_GNUC_UNUSED const GValue *table_schema, G_GNUC_UNUSED const GValue *table_name,
			    G_GNUC_UNUSED const GValue *index_name_n)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta__index_cols (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			    G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			    G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return TRUE;
}

gboolean
_gda_capi_meta_index_cols (G_GNUC_UNUSED GdaServerProvider *prov, G_GNUC_UNUSED GdaConnection *cnc,
			   G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *context,
			   G_GNUC_UNUSED GError **error, G_GNUC_UNUSED const GValue *table_catalog,
			   G_GNUC_UNUSED const GValue *table_schema, G_GNUC_UNUSED const GValue *table_name,
			   G_GNUC_UNUSED const GValue *index_name)
{
	TO_IMPLEMENT;
	return TRUE;
}
