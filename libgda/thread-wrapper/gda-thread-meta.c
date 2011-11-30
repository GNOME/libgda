/*
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include "gda-thread-meta.h"
#include "gda-thread-provider.h"
#include <libgda/gda-meta-store.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-connection-internal.h>

#define PROV_CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))

/* common implementation of the functions used when no parameter is passed */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GdaMetaStore *store;
	GdaMetaContext *context;
} BasicThreadData;

#define main_thread_basic_core(func,prov,cnc,store,context,error) \
	ThreadConnectionData *cdata; \
	BasicThreadData wdata; \
	gpointer res; \
	guint jid; \
        cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data_error ((cnc),(error)); \
	if (!cdata) \
		return FALSE; \
	wdata.prov = cdata->cnc_provider; \
	wdata.cnc = cdata->sub_connection; \
        wdata.store = (store); \
        wdata.context = (context); \
	jid = gda_thread_wrapper_execute (cdata->wrapper, \
					  (GdaThreadWrapperFunc) (func), &wdata, NULL, NULL); \
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, jid, (error)); \
	return GPOINTER_TO_INT (res) ? TRUE : FALSE

#define sub_thread_basic_core(func,name) \
	gboolean retval; \
	if (! (func)) {		  \
	        WARN_METHOD_NOT_IMPLEMENTED (data->prov, (name)); \
		return GINT_TO_POINTER (0); \
	} \
	retval = (func) (data->prov, data->cnc, data->store, data->context, error); \
	/*g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");*/ \
	return GINT_TO_POINTER (retval ? 1 : 0)

typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GdaMetaStore *store;
	GdaMetaContext *context;
	const GValue *v1;
	const GValue *v2;
	const GValue *v3;
	const GValue *v4;
} DetailedThreadData;

#define main_thread_detailed_core(func,prov,cnc,store,context,arg1,arg2,arg3,arg4,error) \
	ThreadConnectionData *cdata; \
	DetailedThreadData wdata; \
	gpointer res; \
	guint jid; \
        cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data_error ((cnc),(error)); \
	if (!cdata) \
		return FALSE; \
	wdata.prov = cdata->cnc_provider; \
	wdata.cnc = cdata->sub_connection; \
        wdata.store = (store); \
        wdata.context = (context); \
        wdata.v1 = (arg1); \
        wdata.v2 = (arg2); \
        wdata.v3 = (arg3); \
        wdata.v4 = (arg4); \
	jid = gda_thread_wrapper_execute (cdata->wrapper, \
					  (GdaThreadWrapperFunc) (func), &wdata, NULL, NULL); \
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, jid, (error)); \
	return GPOINTER_TO_INT (res) ? TRUE : FALSE

#define sub_thread_detailed1_core(func,name) \
	gboolean retval; \
	if (! (func)) {		  \
	        WARN_METHOD_NOT_IMPLEMENTED (data->prov, (name)); \
		return GINT_TO_POINTER (0); \
	} \
	retval = (func) (data->prov, data->cnc, data->store, data->context, error, data->v1); \
	/*g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");*/ \
	return GINT_TO_POINTER (retval ? 1 : 0)

#define sub_thread_detailed2_core(func,name) \
	gboolean retval; \
	if (! (func)) {		  \
	        WARN_METHOD_NOT_IMPLEMENTED (data->prov, (name)); \
		return GINT_TO_POINTER (0); \
	} \
	retval = (func) (data->prov, data->cnc, data->store, data->context, error, data->v1, data->v2); \
	/*g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");*/ \
	return GINT_TO_POINTER (retval ? 1 : 0)

#define sub_thread_detailed3_core(func,name) \
	gboolean retval; \
	if (! (func)) {		  \
	        WARN_METHOD_NOT_IMPLEMENTED (data->prov, (name)); \
		return GINT_TO_POINTER (0); \
	} \
	retval = (func) (data->prov, data->cnc, data->store, data->context, error, data->v1, data->v2, data->v3); \
	/*g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");*/ \
	return GINT_TO_POINTER (retval ? 1 : 0)

#define sub_thread_detailed4_core(func,name) \
	gboolean retval; \
	if (! (func)) {		  \
	        WARN_METHOD_NOT_IMPLEMENTED (data->prov, (name)); \
		return GINT_TO_POINTER (0); \
	} \
	retval = (func) (data->prov, data->cnc, data->store, data->context, error, data->v1, data->v2, data->v3, data->v4); \
	/*g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");*/ \
	return GINT_TO_POINTER (retval ? 1 : 0)

/*
 * Meta initialization
 */
void
_gda_thread_provider_meta_init (G_GNUC_UNUSED GdaServerProvider *provider)
{
	/* nothing to be done */
}



static gpointer
sub_thread__gda_thread_meta__info (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._info, "_info");
}

gboolean
_gda_thread_meta__info (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__info, prov, cnc, store, context, error);
}



static gpointer
sub_thread__gda_thread_meta__btypes (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._btypes, "_btypes");
}

gboolean
_gda_thread_meta__btypes (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__btypes, prov, cnc, store, context, error);
}


static gpointer
sub_thread__gda_thread_meta__udt (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._udt, "_udt");
}
gboolean
_gda_thread_meta__udt (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
		       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__udt, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_udt (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed2_core (PROV_CLASS (data->prov)->meta_funcs.udt, "_udt");
}
gboolean
_gda_thread_meta_udt (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
		      GdaMetaStore *store, GdaMetaContext *context, GError **error,
		      const GValue *udt_catalog, const GValue *udt_schema)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_udt, prov, cnc, store, context, 
				    udt_catalog, udt_schema, NULL, NULL, error);
}



static gpointer
sub_thread__gda_thread_meta__udt_cols (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._udt_cols, "_udt_cols");
}

gboolean
_gda_thread_meta__udt_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__udt_cols, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_udt_cols (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.udt_cols, "udt_cols");
}

gboolean
_gda_thread_meta_udt_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_udt_cols, prov, cnc, store, context, 
				    udt_catalog, udt_schema, udt_name, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__enums (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._enums, "_enums");
}

gboolean
_gda_thread_meta__enums (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__enums, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_enums (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.enums, "enums");
}

gboolean
_gda_thread_meta_enums (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_enums, prov, cnc, store, context, 
				    udt_catalog, udt_schema, udt_name, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__domains (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._domains, "_domains");
}

gboolean
_gda_thread_meta__domains (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__domains, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_domains (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed2_core (PROV_CLASS (data->prov)->meta_funcs.domains, "domains");
}

gboolean
_gda_thread_meta_domains (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			  GdaMetaStore *store, GdaMetaContext *context, GError **error,
			  const GValue *domain_catalog, const GValue *domain_schema)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_domains, prov, cnc, store, context, 
				    domain_catalog, domain_schema, NULL, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__constraints_dom (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._constraints_dom, "_constraints_dom");
}

gboolean
_gda_thread_meta__constraints_dom (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__constraints_dom, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_constraints_dom (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.constraints_dom, "constraints_dom");
}

gboolean
_gda_thread_meta_constraints_dom (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				  GdaMetaStore *store, GdaMetaContext *context, GError **error,
				  const GValue *domain_catalog, const GValue *domain_schema, 
				  const GValue *domain_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_constraints_dom, prov, cnc, store, context, 
				    domain_catalog, domain_schema, domain_name, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__el_types (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._el_types, "_el_types");
}

gboolean
_gda_thread_meta__el_types (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__el_types, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_el_types (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed1_core (PROV_CLASS (data->prov)->meta_funcs.el_types, "el_types");
}

gboolean
_gda_thread_meta_el_types (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *specific_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_el_types, prov, cnc, store, context, 
				    specific_name, NULL, NULL, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__collations (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._collations, "_collations");
}

gboolean
_gda_thread_meta__collations (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__collations, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_collations (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.collations, "collations");
}

gboolean
_gda_thread_meta_collations (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *collation_catalog, const GValue *collation_schema, 
			     const GValue *collation_name_n)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_collations, prov, cnc, store, context, 
				    collation_catalog, collation_schema, collation_name_n, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__character_sets (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._character_sets, "_character_sets");
}

gboolean
_gda_thread_meta__character_sets (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__character_sets, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_character_sets (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.character_sets, "character_sets");
}

gboolean
_gda_thread_meta_character_sets (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				 GdaMetaStore *store, GdaMetaContext *context, GError **error,
				 const GValue *chset_catalog, const GValue *chset_schema, 
				 const GValue *chset_name_n)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_character_sets, prov, cnc, store, context, 
				    chset_catalog, chset_schema, chset_name_n, NULL, error);
}


static gpointer
sub_thread__gda_thread_meta__schemata (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._schemata, "_schemata");
}

gboolean
_gda_thread_meta__schemata (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__schemata, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_schemata (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed2_core (PROV_CLASS (data->prov)->meta_funcs.schemata, "schemata");
}

gboolean
_gda_thread_meta_schemata (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			   const GValue *catalog_name, const GValue *schema_name_n)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_schemata, prov, cnc, store, context, 
				    catalog_name, schema_name_n, NULL, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__tables_views (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._tables_views, "_tables_views");
}

gboolean
_gda_thread_meta__tables_views (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__tables_views, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_tables_views (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.tables_views, "tables_views");
}

gboolean
_gda_thread_meta_tables_views (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error,
			       const GValue *table_catalog, const GValue *table_schema, 
			       const GValue *table_name_n)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_tables_views, prov, cnc, store, context, 
				    table_catalog, table_schema, table_name_n, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__columns (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._columns, "_columns");
}

gboolean
_gda_thread_meta__columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__columns, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_columns (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.columns, "columns");
}

gboolean
_gda_thread_meta_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			  GdaMetaStore *store, GdaMetaContext *context, GError **error,
			  const GValue *table_catalog, const GValue *table_schema, 
			  const GValue *table_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_columns, prov, cnc, store, context, 
				    table_catalog, table_schema, table_name, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__view_cols (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._view_cols, "_view_cols");
}

gboolean
_gda_thread_meta__view_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__view_cols, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_view_cols (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.view_cols, "view_cols");
}

gboolean
_gda_thread_meta_view_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *view_catalog, const GValue *view_schema, 
			    const GValue *view_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_view_cols, prov, cnc, store, context, 
				    view_catalog, view_schema, view_name, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__constraints_tab (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._constraints_tab, "_constraints_tab");
}

gboolean
_gda_thread_meta__constraints_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__constraints_tab, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_constraints_tab (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed4_core (PROV_CLASS (data->prov)->meta_funcs.constraints_tab, "constraints_tab");
}

gboolean
_gda_thread_meta_constraints_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				  GdaMetaStore *store, GdaMetaContext *context, GError **error, 
				  const GValue *table_catalog, const GValue *table_schema, 
				  const GValue *table_name, const GValue *constraint_name_n)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_constraints_tab, prov, cnc, store, context, 
				   table_catalog, table_schema, table_name, constraint_name_n, error);
}

static gpointer
sub_thread__gda_thread_meta__constraints_ref (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._constraints_ref, "_constraints_ref");
}

gboolean
_gda_thread_meta__constraints_ref (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__constraints_ref, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_constraints_ref (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed4_core (PROV_CLASS (data->prov)->meta_funcs.constraints_ref, "constraints_ref");
}

gboolean
_gda_thread_meta_constraints_ref (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				  GdaMetaStore *store, GdaMetaContext *context, GError **error,
				  const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				  const GValue *constraint_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_constraints_ref, prov, cnc, store, context, 
				    table_catalog, table_schema, table_name, constraint_name, error);
}

static gpointer
sub_thread__gda_thread_meta__key_columns (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._key_columns, "_key_columns");
}

gboolean
_gda_thread_meta__key_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__key_columns, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_key_columns (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed4_core (PROV_CLASS (data->prov)->meta_funcs.key_columns, "key_columns");
}

gboolean
_gda_thread_meta_key_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *table_catalog, const GValue *table_schema, 
			      const GValue *table_name, const GValue *constraint_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_key_columns, prov, cnc, store, context, 
				    table_catalog, table_schema, table_name, constraint_name, error);
}

static gpointer
sub_thread__gda_thread_meta__check_columns (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._check_columns, "_check_columns");
}

gboolean
_gda_thread_meta__check_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__check_columns, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_check_columns (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed4_core (PROV_CLASS (data->prov)->meta_funcs.check_columns, "check_columns");
}

gboolean
_gda_thread_meta_check_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				GdaMetaStore *store, GdaMetaContext *context, GError **error,
				const GValue *table_catalog, const GValue *table_schema, 
				const GValue *table_name, const GValue *constraint_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_check_columns, prov, cnc, store, context, 
				    table_catalog, table_schema, table_name, constraint_name, error);
}

static gpointer
sub_thread__gda_thread_meta__triggers (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._triggers, "_triggers");
}

gboolean
_gda_thread_meta__triggers (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__triggers, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_triggers (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.triggers, "triggers");
}

gboolean
_gda_thread_meta_triggers (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *table_catalog, const GValue *table_schema, 
			   const GValue *table_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_triggers, prov, cnc, store, context, 
				    table_catalog, table_schema, table_name, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__routines (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._routines, "_routines");
}

gboolean
_gda_thread_meta__routines (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__routines, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_routines (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.routines, "routines");
}

gboolean
_gda_thread_meta_routines (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *routine_catalog, const GValue *routine_schema, 
			   const GValue *routine_name_n)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_routines, prov, cnc, store, context, 
				    routine_catalog, routine_schema, routine_name_n, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__routine_col (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._routine_col, "_routine_col");
}

gboolean
_gda_thread_meta__routine_col (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__routine_col, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_routine_col (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.routine_col, "routine_col");
}

gboolean
_gda_thread_meta_routine_col (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *rout_catalog, const GValue *rout_schema, 
			      const GValue *rout_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_routine_col, prov, cnc, store, context, 
				    rout_catalog, rout_schema, rout_name, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__routine_par (BasicThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._routine_par, "_routine_par");
}

gboolean
_gda_thread_meta__routine_par (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__routine_par, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_routine_par (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed3_core (PROV_CLASS (data->prov)->meta_funcs.routine_par, "routine_par");
}

gboolean
_gda_thread_meta_routine_par (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *rout_catalog, const GValue *rout_schema, 
			      const GValue *rout_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_routine_par, prov, cnc, store, context, 
				   rout_catalog, rout_schema, rout_name, NULL, error);
}

static gpointer
sub_thread__gda_thread_meta__indexes_tab (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._indexes_tab, "_table_indexes");
}

gboolean
_gda_thread_meta__indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__indexes_tab, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_indexes_tab (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed4_core (PROV_CLASS (data->prov)->meta_funcs.indexes_tab, "table_indexes");
}

gboolean
_gda_thread_meta_indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
			      const GValue *index_name_n)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_indexes_tab, prov, cnc, store, context, 
				   table_catalog, table_schema, table_name, index_name_n, error);
}

static gpointer
sub_thread__gda_thread_meta__index_cols (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_basic_core (PROV_CLASS (data->prov)->meta_funcs._index_cols, "_index_column_usage");
}

gboolean
_gda_thread_meta__index_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	main_thread_basic_core (sub_thread__gda_thread_meta__index_cols, prov, cnc, store, context, error);
}

static gpointer
sub_thread__gda_thread_meta_index_cols (DetailedThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	sub_thread_detailed4_core (PROV_CLASS (data->prov)->meta_funcs.index_cols, "index_column_usage");
}

gboolean
_gda_thread_meta_index_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *table_catalog, const GValue *table_schema,
			     const GValue *table_name, const GValue *index_name)
{
	main_thread_detailed_core (sub_thread__gda_thread_meta_index_cols, prov, cnc, store, context, 
				   table_catalog, table_schema, table_name, index_name, error);
}
