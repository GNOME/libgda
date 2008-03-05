/* GDA capi provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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

static gboolean append_a_row (GdaDataModel *to_model, GError **error, gint nb, ...);

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
 * predefined statements' GdaStatement
 */
static GdaStatement **internal_stmt;
static GdaSet        *internal_params;

/* 
 * global static values
 */
static GdaSqlParser *internal_parser = NULL;
/* TO_ADD: other static values */


/*
 * Meta initialization
 */
void
_gda_capi_provider_meta_init (GdaServerProvider *provider)
{
	InternalStatementItem i;

        internal_parser = gda_server_provider_internal_get_parser (provider);
	internal_params = gda_set_new (NULL);

        internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
        for (i = I_STMT_1; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
		GdaSet *set;
                internal_stmt[i] = gda_sql_parser_parse_string (internal_parser, internal_sql[i], NULL, NULL);
                if (!internal_stmt[i])
                        g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		g_assert (gda_statement_get_parameters (internal_stmt[i], &set, NULL));
		if (set) {
			gda_set_merge_with_set (internal_params, set);
			g_object_unref (set);
		}
        }

	/* initialize static values here */
}

gboolean
_gda_capi_meta_info (GdaServerProvider *prov, GdaConnection *cnc, 
		     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_capi_meta_btypes (GdaServerProvider *prov, GdaConnection *cnc, 
		       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}

gboolean 
_gda_capi_meta_schemata (GdaServerProvider *prov, GdaConnection *cnc, 
			 GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			 const GValue *schema_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}



gboolean
_gda_capi_meta_tables_views (GdaServerProvider *prov, GdaConnection *cnc, 
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}



gboolean 
_gda_capi_meta_tables_views_s (GdaServerProvider *prov, GdaConnection *cnc, 
			       GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			       const GValue *table_schema, const GValue *table_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_capi_meta_columns (GdaServerProvider *prov, GdaConnection *cnc, 
			GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}

gboolean
_gda_capi_meta_columns_t (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			  const GValue *table_schema, const GValue *table_name)
{
	return _gda_capi_meta_columns_c (prov, cnc, store, context, error, table_schema, table_name, NULL);
}

gboolean
_gda_capi_meta_columns_c (GdaServerProvider *prov, GdaConnection *cnc, 
			  GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			  const GValue *table_schema, const GValue *table_name, const GValue *column_name)
{
	GdaDataModel *model;
	gboolean retval = TRUE;

	model = gda_meta_store_create_modify_data_model (store, context->table_name);
	g_assert (model);

	/* fill in @model */
	TO_IMPLEMENT;
	if (retval)
		retval = gda_meta_store_modify (store, context->table_name, model, NULL, error, NULL);
	g_object_unref (model);

	return retval;
}
