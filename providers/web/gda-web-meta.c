/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-web.h"
#include "gda-web-meta.h"
#include "gda-web-provider.h"
#include <libgda/gda-meta-store.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>

#include "gda-web-util.h"

/*
 * Meta initialization
 */
void
_gda_web_provider_meta_init (G_GNUC_UNUSED GdaServerProvider *provider)
{
}

/*
 * ... is a list of (arg name, arg value) as strings, terminated with NULL
 */
static GdaDataModel *
run_meta_command_args (GdaConnection *cnc, WebConnectionData *cdata, const gchar *type, GError **error, ...)
{
	/* send message */
	xmlDocPtr doc;
	gchar status;
	gchar *tmp, *token;
	GString *string;
	va_list ap;
#define MSG__TEMPL "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" \
		"<request>\n"						\
		"  <token>%s</token>\n"					\
		"  <cmd type=\"%s\">META%s</cmd>\n"			\
		"</request>"

	string = g_string_new ("");
	va_start (ap, error);
	for (tmp = va_arg (ap, gchar*); tmp; tmp = va_arg (ap, gchar*)) {
		gchar *argval;
		xmlChar *xargval;
		argval = va_arg (ap, gchar*);
		xargval = xmlEncodeSpecialChars (NULL, BAD_CAST argval);
		g_string_append_printf (string, "<arg name=\"%s\">%s</arg>", tmp, (gchar*) xargval);
		xmlFree (xargval);
	}
	va_end (ap);

	token = _gda_web_compute_token (cdata);
	tmp = g_strdup_printf (MSG__TEMPL, token, type, string->str);
	g_string_free (string, TRUE);
	g_free (token);
	
	doc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_META, tmp, cdata->key, &status);
	g_free (tmp);
	if (!doc)
		return FALSE;
	if (status != 'O') {
		_gda_web_set_connection_error_from_xmldoc (cnc, doc, error);
		xmlFreeDoc (doc);
		return FALSE;
	}

	/* compute returned data model */
	xmlNodePtr root, node;
	GdaDataModel *model = NULL;
	root = xmlDocGetRootElement (doc);
        for (node = root->children; node; node = node->next) {
		if (!strcmp ((gchar*) node->name, "gda_array")) {
			model = gda_data_model_import_new_xml_node (node);
			break;
                }
	}
	xmlFreeDoc (doc);
	/*gda_data_model_dump (model, NULL);*/

	if (! model)
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
                             GDA_SERVER_PROVIDER_INTERNAL_ERROR, "%s",
                             _("Can't import data from web server"));
	return model;
}

static GdaDataModel *
run_meta_command (GdaConnection *cnc, WebConnectionData *cdata, const gchar *type, GError **error)
{
	return run_meta_command_args (cnc, cdata, type, error, NULL);
}

gboolean
_gda_web_meta__info (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
		     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._info)
			return cdata->reuseable->operations->re_meta_funcs._info (NULL, cnc, store,
										  context, error);
		else
			return TRUE;
	}

	/* fallback to default method */	
	model = run_meta_command (cnc, cdata, "info", error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta__btypes (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
		       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._btypes)
			return cdata->reuseable->operations->re_meta_funcs._btypes (NULL, cnc, store,
										    context, error);
		else
			return TRUE;
	}

	/* fallback to default method */	
	model = run_meta_command (cnc, cdata, "btypes", error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta__udt (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
		    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._udt)
			return cdata->reuseable->operations->re_meta_funcs._udt (NULL, cnc, store,
										 context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_udt (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
		   GdaMetaStore *store, GdaMetaContext *context, GError **error,
		   const GValue *udt_catalog, const GValue *udt_schema)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.udt)
			return cdata->reuseable->operations->re_meta_funcs.udt (NULL, cnc, store,
										context, error, udt_catalog,
										udt_schema);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}


gboolean
_gda_web_meta__udt_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._udt_cols)
			return cdata->reuseable->operations->re_meta_funcs._udt_cols (NULL, cnc, store,
										      context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_udt_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.udt_cols)
			return cdata->reuseable->operations->re_meta_funcs.udt_cols (NULL, cnc, store,
										     context, error,
										     udt_catalog, udt_schema,
										     udt_name);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__enums (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
		      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._enums)
			return cdata->reuseable->operations->re_meta_funcs._enums (NULL, cnc, store,
										   context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_enums (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
		     GdaMetaStore *store, GdaMetaContext *context, GError **error,
		     const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.enums)
			return cdata->reuseable->operations->re_meta_funcs.enums (NULL, cnc, store,
										  context, error,
										  udt_catalog, udt_schema,
										  udt_name);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}


gboolean
_gda_web_meta__domains (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._domains)
			return cdata->reuseable->operations->re_meta_funcs._domains (NULL, cnc, store,
										     context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_domains (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
		       GdaMetaStore *store, GdaMetaContext *context, GError **error,
		       const GValue *domain_catalog, const GValue *domain_schema)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.domains)
			return cdata->reuseable->operations->re_meta_funcs.domains (NULL, cnc, store,
										    context, error,
										    domain_catalog, domain_schema);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__constraints_dom (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._constraints_dom)
			return cdata->reuseable->operations->re_meta_funcs._constraints_dom (NULL, cnc, store,
											     context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_constraints_dom (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error,
			       const GValue *domain_catalog, const GValue *domain_schema, 
			       const GValue *domain_name)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.constraints_dom)
			return cdata->reuseable->operations->re_meta_funcs.constraints_dom (NULL, cnc, store,
											    context, error,
											    domain_catalog, domain_schema,
											    domain_name);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__el_types (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._el_types)
			return cdata->reuseable->operations->re_meta_funcs._el_types (NULL, cnc, store,
										      context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_el_types (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *specific_name)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.el_types)
			return cdata->reuseable->operations->re_meta_funcs.el_types (NULL, cnc, store,
										     context, error,
										     specific_name);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__collations (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._collations)
			return cdata->reuseable->operations->re_meta_funcs._collations (NULL, cnc, store,
											context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_collations (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			  GdaMetaStore *store, GdaMetaContext *context, GError **error,
			  const GValue *collation_catalog, const GValue *collation_schema, 
			  const GValue *collation_name_n)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.collations)
			return cdata->reuseable->operations->re_meta_funcs.collations (NULL, cnc, store,
										       context, error,
										       collation_catalog,
										       collation_schema, 
										       collation_name_n);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__character_sets (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._character_sets)
			return cdata->reuseable->operations->re_meta_funcs._character_sets (NULL, cnc, store,
											    context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_character_sets (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error,
			      const GValue *chset_catalog, const GValue *chset_schema, 
			      const GValue *chset_name_n)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.character_sets)
			return cdata->reuseable->operations->re_meta_funcs.character_sets (NULL, cnc, store,
											   context, error,
											   chset_catalog, chset_schema, 
											   chset_name_n);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__schemata (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._schemata)
			return cdata->reuseable->operations->re_meta_funcs._schemata (NULL, cnc, store,
										      context, error);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command (cnc, cdata, "schemas", error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta_schemata (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			const GValue *catalog_name, const GValue *schema_name_n)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.schemata)
			return cdata->reuseable->operations->re_meta_funcs.schemata (NULL, cnc, store,
										     context, error, catalog_name, schema_name_n);
		else
			return TRUE;
	}

	/* fallback to default method */
	if (schema_name_n)
		model = run_meta_command_args (cnc, cdata, "schemas", error,
					       "catalog_name", g_value_get_string (catalog_name),
					       "schema_name", g_value_get_string (schema_name_n), NULL);
	else
		model = run_meta_command_args (cnc, cdata, "schemas", error,
					       "catalog_name", g_value_get_string (catalog_name), NULL);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta__tables_views (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *tables_model, *views_model;
	gboolean retval = TRUE;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._tables_views)
			return cdata->reuseable->operations->re_meta_funcs._tables_views (NULL, cnc, store,
											  context, error);
		else
			return TRUE;
	}

	/* fallback to default method */
	tables_model = run_meta_command (cnc, cdata, "tables", error);
	if (!tables_model)
		return FALSE;
	views_model = run_meta_command (cnc, cdata, "views", error);
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
_gda_web_meta_tables_views (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error,
			    const GValue *table_catalog, const GValue *table_schema, 
			    const GValue *table_name_n)
{
	GdaDataModel *tables_model, *views_model;
	gboolean retval = TRUE;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.tables_views)
			return cdata->reuseable->operations->re_meta_funcs.tables_views (NULL, cnc, store,
											 context, error,
											 table_catalog, table_schema,
											 table_name_n);
		else
			return TRUE;
	}

	/* fallback to default method */
	if (table_name_n)
		tables_model = run_meta_command_args (cnc, cdata, "tables", error,
						      "table_catalog", g_value_get_string (table_catalog),
						      "table_schema", g_value_get_string (table_schema),
						      "table_name", g_value_get_string (table_name_n), NULL);
	else
		tables_model = run_meta_command_args (cnc, cdata, "tables", error,
						      "table_catalog", g_value_get_string (table_catalog),
						      "table_schema", g_value_get_string (table_schema), NULL);
	if (!tables_model)
		return FALSE;

	if (table_name_n)
		views_model = run_meta_command_args (cnc, cdata, "views", error,
						     "table_catalog", g_value_get_string (table_catalog),
						     "table_schema", g_value_get_string (table_schema),
						     "table_name", g_value_get_string (table_name_n), NULL);
	else
		views_model = run_meta_command_args (cnc, cdata, "views", error,
						     "table_catalog", g_value_get_string (table_catalog),
						     "table_schema", g_value_get_string (table_schema), NULL);
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
_gda_web_meta__columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._columns)
			return cdata->reuseable->operations->re_meta_funcs._columns (NULL, cnc, store,
										     context, error);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command (cnc, cdata, "columns", error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
		       GdaMetaStore *store, GdaMetaContext *context, GError **error,
		       const GValue *table_catalog, const GValue *table_schema, 
		       const GValue *table_name)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.columns)
			return cdata->reuseable->operations->re_meta_funcs.columns (NULL, cnc, store,
										    context, error, table_catalog, table_schema,
										    table_name);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command_args (cnc, cdata, "columns", error,
				       "table_catalog", g_value_get_string (table_catalog),
				       "table_schema", g_value_get_string (table_schema),
				       "table_name", g_value_get_string (table_name), NULL);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta__view_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			  GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._view_cols)
			return cdata->reuseable->operations->re_meta_funcs._view_cols (NULL, cnc, store,
										       context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_view_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			 GdaMetaStore *store, GdaMetaContext *context, GError **error,
			 const GValue *view_catalog, const GValue *view_schema, 
			 const GValue *view_name)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.view_cols)
			return cdata->reuseable->operations->re_meta_funcs.view_cols (NULL, cnc, store,
										      context, error,
										      view_catalog, view_schema, 
										      view_name);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__constraints_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._constraints_tab)
			return cdata->reuseable->operations->re_meta_funcs._constraints_tab (NULL, cnc, store,
											     context, error);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command (cnc, cdata, "constraints_tab", error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta_constraints_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error, 
			       const GValue *table_catalog, const GValue *table_schema, 
			       const GValue *table_name, const GValue *constraint_name_n)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.constraints_tab)
			return cdata->reuseable->operations->re_meta_funcs.constraints_tab (NULL, cnc, store,
											    context, error,
											    table_catalog, table_schema, 
											    table_name, constraint_name_n);
		else
			return TRUE;
	}

	/* fallback to default method */
	if (constraint_name_n)
		model = run_meta_command_args (cnc, cdata, "constraints_tab", error,
					       "table_catalog", g_value_get_string (table_catalog),
					       "table_schema", g_value_get_string (table_schema),
					       "table_name", g_value_get_string (table_name),
					       "constraint_name_", g_value_get_string (table_name), NULL);
	else
		model = run_meta_command_args (cnc, cdata, "constraints_tab", error,
					       "table_catalog", g_value_get_string (table_catalog),
					       "table_schema", g_value_get_string (table_schema),
					       "table_name", g_value_get_string (table_name), NULL);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta__constraints_ref (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
				GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._constraints_ref)
			return cdata->reuseable->operations->re_meta_funcs._constraints_ref (NULL, cnc, store,
											     context, error);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command (cnc, cdata, "constraints_ref", error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta_constraints_ref (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			       GdaMetaStore *store, GdaMetaContext *context, GError **error,
			       const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
			       const GValue *constraint_name)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.constraints_ref)
			return cdata->reuseable->operations->re_meta_funcs.constraints_ref (NULL, cnc, store,
											    context, error,
											    table_catalog, table_schema,
											    table_name,
											    constraint_name);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command_args (cnc, cdata, "constraints_ref", error,
				       "table_catalog", g_value_get_string (table_catalog),
				       "table_schema", g_value_get_string (table_schema),
				       "table_name", g_value_get_string (table_name),
				       "constraint_name_", g_value_get_string (table_name), NULL);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta__key_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._key_columns)
			return cdata->reuseable->operations->re_meta_funcs._key_columns (NULL, cnc, store,
											 context, error);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command (cnc, cdata, "key_columns", error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta_key_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *table_catalog, const GValue *table_schema, 
			   const GValue *table_name, const GValue *constraint_name)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.key_columns)
			return cdata->reuseable->operations->re_meta_funcs.key_columns (NULL, cnc, store,
											context, error,
											table_catalog, table_schema,
											table_name,
											constraint_name);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command_args (cnc, cdata, "key_columns", error,
				       "table_catalog", g_value_get_string (table_catalog),
				       "table_schema", g_value_get_string (table_schema),
				       "table_name", g_value_get_string (table_name),
				       "constraint_name_", g_value_get_string (table_name), NULL);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta__check_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			      GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._check_columns)
			return cdata->reuseable->operations->re_meta_funcs._check_columns (NULL, cnc, store,
											   context, error);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command (cnc, cdata, "check_columns", error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta_check_columns (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			     GdaMetaStore *store, GdaMetaContext *context, GError **error,
			     const GValue *table_catalog, const GValue *table_schema, 
			     const GValue *table_name, const GValue *constraint_name)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.check_columns)
			return cdata->reuseable->operations->re_meta_funcs.check_columns (NULL, cnc, store,
											  context, error,
											  table_catalog, table_schema,
											  table_name,
											  constraint_name);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command_args (cnc, cdata, "check_columns", error,
				       "table_catalog", g_value_get_string (table_catalog),
				       "table_schema", g_value_get_string (table_schema),
				       "table_name", g_value_get_string (table_name),
				       "constraint_name_", g_value_get_string (table_name), NULL);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta__triggers (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._triggers)
			return cdata->reuseable->operations->re_meta_funcs._triggers (NULL, cnc, store,
										      context, error);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command (cnc, cdata, "triggers", error);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta_triggers (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *table_catalog, const GValue *table_schema, 
			const GValue *table_name)
{
	GdaDataModel *model;
	gboolean retval;
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;
	
	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.triggers)
			return cdata->reuseable->operations->re_meta_funcs.triggers (NULL, cnc, store,
										     context, error,
										     table_catalog, table_schema,
										     table_name);
		else
			return TRUE;
	}

	/* fallback to default method */
	model = run_meta_command_args (cnc, cdata, "triggers", error,
				       "table_catalog", g_value_get_string (table_catalog),
				       "table_schema", g_value_get_string (table_schema),
				       "table_name", g_value_get_string (table_name), NULL);
	if (!model)
		return FALSE;
	retval = gda_meta_store_modify_with_context (store, context, model, error);
	g_object_unref (model);
		
	return retval;
}

gboolean
_gda_web_meta__routines (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			 GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._routines)
			return cdata->reuseable->operations->re_meta_funcs._routines (NULL, cnc, store,
										      context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_routines (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			GdaMetaStore *store, GdaMetaContext *context, GError **error,
			const GValue *routine_catalog, const GValue *routine_schema, 
			const GValue *routine_name_n)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.routines)
			return cdata->reuseable->operations->re_meta_funcs.routines (NULL, cnc, store,
										     context, error,
										     routine_catalog, routine_schema, 
										     routine_name_n);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__routine_col (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._routine_col)
			return cdata->reuseable->operations->re_meta_funcs._routine_col (NULL, cnc, store,
											 context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_routine_col (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *rout_catalog, const GValue *rout_schema, 
			   const GValue *rout_name, const GValue *col_name, const GValue *ordinal_position)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.routine_col)
			return cdata->reuseable->operations->re_meta_funcs.routine_col (NULL, cnc, store,
											context, error,
											rout_catalog, rout_schema, 
											rout_name, col_name, ordinal_position);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__routine_par (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._routine_par)
			return cdata->reuseable->operations->re_meta_funcs._routine_par (NULL, cnc, store,
											 context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_routine_par (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *rout_catalog, const GValue *rout_schema, 
			   const GValue *rout_name)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.routine_par)
			return cdata->reuseable->operations->re_meta_funcs.routine_par (NULL, cnc, store,
											context, error,
											rout_catalog, rout_schema, 
											rout_name);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			    GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._indexes_tab)
			return cdata->reuseable->operations->re_meta_funcs._indexes_tab (NULL, cnc, store,
											 context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_indexes_tab (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error,
			   const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
			   const GValue *index_name_n)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.indexes_tab)
			return cdata->reuseable->operations->re_meta_funcs.indexes_tab (NULL, cnc, store,
											context, error,
											table_catalog, table_schema, 
											table_name, index_name_n);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta__index_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			   GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs._index_cols)
			return cdata->reuseable->operations->re_meta_funcs._index_cols (NULL, cnc, store,
											context, error);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}

gboolean
_gda_web_meta_index_cols (G_GNUC_UNUSED GdaServerProvider *prov, GdaConnection *cnc,
			  GdaMetaStore *store, GdaMetaContext *context, GError **error,
			  const GValue *table_catalog, const GValue *table_schema,
			  const GValue *table_name, const GValue *index_name)
{
	WebConnectionData *cdata;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* use reuseable methods if available */
	if (cdata->reuseable) {
		if (cdata->reuseable->operations->re_meta_funcs.index_cols)
			return cdata->reuseable->operations->re_meta_funcs.index_cols (NULL, cnc, store,
										       context, error,
										       table_catalog, table_schema,
										       table_name, index_name);
		else
			return TRUE;
	}

	/* no default method */
	return TRUE;
}
