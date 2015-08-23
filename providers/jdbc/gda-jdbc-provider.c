/*
 * Copyright (C) 2008 - 2014 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <libgda/libgda.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-server-provider-impl.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda/gda-statement-extra.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-jdbc.h"
#include "gda-jdbc-provider.h"
#include "gda-jdbc-recordset.h"
#include "gda-jdbc-ddl.h"
#include "gda-jdbc-meta.h"
#include "gda-jdbc-util.h"
#include "jni-wrapper.h"
#include "jni-globals.h"
#include "jdbc-resources.h"
#include <libgda/gda-debug-macros.h>

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

/*
 * GObject methods
 */
static void gda_jdbc_provider_class_init (GdaJdbcProviderClass *klass);
static void gda_jdbc_provider_init       (GdaJdbcProvider *provider,
					  GdaJdbcProviderClass *klass);
static GObjectClass *parent_class = NULL;

/*
 * GdaServerProvider's virtual methods
 */
/* connection management */
static gboolean            gda_jdbc_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
							      GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_jdbc_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar        *gda_jdbc_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);

/* DDL operations */
static gboolean            gda_jdbc_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaServerOperationType type, GdaSet *options);
static GdaServerOperation *gda_jdbc_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaServerOperationType type,
							       GdaSet *options, GError **error);
static gchar              *gda_jdbc_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaServerOperation *op, GError **error);

static gboolean            gda_jdbc_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
								GdaServerOperation *op, GError **error);
/* transactions */
static gboolean            gda_jdbc_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								const gchar *name, GdaTransactionIsolation level, GError **error);
static gboolean            gda_jdbc_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *name, GError **error);
static gboolean            gda_jdbc_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection * cnc,
								   const gchar *name, GError **error);
static gboolean            gda_jdbc_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
							    const gchar *name, GError **error);
static gboolean            gda_jdbc_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *name, GError **error);
static gboolean            gda_jdbc_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
							       const gchar *name, GError **error);

/* information retrieval */
static const gchar        *gda_jdbc_provider_get_version (GdaServerProvider *provider);
static gboolean            gda_jdbc_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaConnectionFeature feature);

static GdaWorker          *gda_jdbc_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc);
static const gchar        *gda_jdbc_provider_get_name (GdaServerProvider *provider);

static GdaDataHandler     *gda_jdbc_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
							       GType g_type, const gchar *dbms_type);

static const gchar*        gda_jdbc_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc,
								    GType type);
/* statements */
static gchar               *gda_jdbc_provider_statement_to_sql  (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GdaSet *params, 
								 GdaStatementSqlFlag flags,
								 GSList **params_used, GError **error);
static gboolean             gda_jdbc_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GError **error);
static GObject             *gda_jdbc_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GdaSet *params,
								 GdaStatementModelUsage model_usage, 
								 GType *col_types, GdaSet **last_inserted_row, GError **error);
static GdaSqlStatement     *gda_jdbc_provider_statement_rewrite (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GdaSet *params, GError **error);


/* distributed transactions */
static gboolean gda_jdbc_provider_xa_start    (GdaServerProvider *provider, GdaConnection *cnc, 
					       const GdaXaTransactionId *xid, GError **error);

static gboolean gda_jdbc_provider_xa_end      (GdaServerProvider *provider, GdaConnection *cnc, 
					       const GdaXaTransactionId *xid, GError **error);
static gboolean gda_jdbc_provider_xa_prepare  (GdaServerProvider *provider, GdaConnection *cnc, 
					       const GdaXaTransactionId *xid, GError **error);

static gboolean gda_jdbc_provider_xa_commit   (GdaServerProvider *provider, GdaConnection *cnc, 
					       const GdaXaTransactionId *xid, GError **error);
static gboolean gda_jdbc_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc, 
					       const GdaXaTransactionId *xid, GError **error);

static GList   *gda_jdbc_provider_xa_recover  (GdaServerProvider *provider, GdaConnection *cnc, 
					       GError **error);

/* 
 * private connection data destroy 
 */
static void gda_jdbc_free_cnc_data (JdbcConnectionData *cdata);


/*
 * Prepared internal statements
 * TO_ADD: any prepared statement to be used internally by the provider should be
 *         declared here, as constants and as SQL statements
 */
static GMutex init_mutex;
static GdaStatement **internal_stmt = NULL;

typedef enum {
	INTERNAL_STMT1
} InternalStatementItem;

static gchar *internal_sql[] = {
	"SQL for INTERNAL_STMT1"
};

/*
 * GdaJdbcProvider class implementation
 */
GdaServerProviderBase jdbc_base_functions = {
	gda_jdbc_provider_get_name,
	gda_jdbc_provider_get_version,
	gda_jdbc_provider_get_server_version,
	gda_jdbc_provider_supports_feature,
	gda_jdbc_provider_create_worker,
	NULL,
	NULL,
	gda_jdbc_provider_get_data_handler,
	gda_jdbc_provider_get_default_dbms_type,
	gda_jdbc_provider_supports_operation,
	gda_jdbc_provider_create_operation,
	gda_jdbc_provider_render_operation,
	gda_jdbc_provider_statement_to_sql,
	NULL,
	gda_jdbc_provider_statement_rewrite,
	gda_jdbc_provider_open_connection,
	NULL,
	gda_jdbc_provider_close_connection,
	NULL,
	NULL,
	gda_jdbc_provider_perform_operation,
	gda_jdbc_provider_begin_transaction,
	gda_jdbc_provider_commit_transaction,
	gda_jdbc_provider_rollback_transaction,
	gda_jdbc_provider_add_savepoint,
	gda_jdbc_provider_rollback_savepoint,
	gda_jdbc_provider_delete_savepoint,
	gda_jdbc_provider_statement_prepare,
	gda_jdbc_provider_statement_execute,

	NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderMeta jdbc_meta_functions = {
	_gda_jdbc_meta__info,
	_gda_jdbc_meta__btypes,
	_gda_jdbc_meta__udt,
	_gda_jdbc_meta_udt,
	_gda_jdbc_meta__udt_cols,
	_gda_jdbc_meta_udt_cols,
	_gda_jdbc_meta__enums,
	_gda_jdbc_meta_enums,
	_gda_jdbc_meta__domains,
	_gda_jdbc_meta_domains,
	_gda_jdbc_meta__constraints_dom,
	_gda_jdbc_meta_constraints_dom,
	_gda_jdbc_meta__el_types,
	_gda_jdbc_meta_el_types,
	_gda_jdbc_meta__collations,
	_gda_jdbc_meta_collations,
	_gda_jdbc_meta__character_sets,
	_gda_jdbc_meta_character_sets,
	_gda_jdbc_meta__schemata,
	_gda_jdbc_meta_schemata,
	_gda_jdbc_meta__tables_views,
	_gda_jdbc_meta_tables_views,
	_gda_jdbc_meta__columns,
	_gda_jdbc_meta_columns,
	_gda_jdbc_meta__view_cols,
	_gda_jdbc_meta_view_cols,
	_gda_jdbc_meta__constraints_tab,
	_gda_jdbc_meta_constraints_tab,
	_gda_jdbc_meta__constraints_ref,
	_gda_jdbc_meta_constraints_ref,
	_gda_jdbc_meta__key_columns,
	_gda_jdbc_meta_key_columns,
	_gda_jdbc_meta__check_columns,
	_gda_jdbc_meta_check_columns,
	_gda_jdbc_meta__triggers,
	_gda_jdbc_meta_triggers,
	_gda_jdbc_meta__routines,
	_gda_jdbc_meta_routines,
	_gda_jdbc_meta__routine_col,
	_gda_jdbc_meta_routine_col,
	_gda_jdbc_meta__routine_par,
	_gda_jdbc_meta_routine_par,
	_gda_jdbc_meta__indexes_tab,
        _gda_jdbc_meta_indexes_tab,
        _gda_jdbc_meta__index_cols,
        _gda_jdbc_meta_index_cols,

	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderXa jdbc_xa_functions = {
	gda_jdbc_provider_xa_start,
	gda_jdbc_provider_xa_end,
	gda_jdbc_provider_xa_prepare,
	gda_jdbc_provider_xa_commit,
	gda_jdbc_provider_xa_rollback,
	gda_jdbc_provider_xa_recover,

	NULL, NULL, NULL, NULL, /* padding */
};

static void
gda_jdbc_provider_class_init (GdaJdbcProviderClass *klass)
{
	parent_class = g_type_class_peek_parent (klass);

	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
						(gpointer) &jdbc_base_functions);
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_META,
						(gpointer) &jdbc_meta_functions);
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_XA,
						(gpointer) &jdbc_xa_functions);
}

extern JavaVM *_jdbc_provider_java_vm;

static void
gda_jdbc_provider_init (GdaJdbcProvider *jdbc_prv, G_GNUC_UNUSED GdaJdbcProviderClass *klass)
{
	g_mutex_lock (&init_mutex);

	if (!internal_stmt) {
		InternalStatementItem i;
		GdaSqlParser *parser;

		parser = gda_server_provider_internal_get_parser ((GdaServerProvider*) jdbc_prv);
		internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
		for (i = INTERNAL_STMT1; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
			internal_stmt[i] = gda_sql_parser_parse_string (parser, internal_sql[i], NULL, NULL);
			if (!internal_stmt[i])
				g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		}
	}

	/* meta data init */
	_gda_jdbc_provider_meta_init ((GdaServerProvider*) jdbc_prv);

	g_mutex_unlock (&init_mutex);
}

GType
gda_jdbc_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static GTypeInfo info = {
			sizeof (GdaJdbcProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_jdbc_provider_class_init,
			NULL, NULL,
			sizeof (GdaJdbcProvider),
			0,
			(GInstanceInitFunc) gda_jdbc_provider_init,
			0
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_SERVER_PROVIDER, "GdaJdbcProvider", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}

static GdaWorker *
gda_jdbc_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc)
{
	/* consider API not thread safe by default */

	static GdaWorker *unique_worker = NULL;
	return gda_worker_new_unique (&unique_worker, TRUE);
}

/*
 * Get provider name request
 */
static const gchar *
gda_jdbc_provider_get_name (GdaServerProvider *provider)
{
	return GDA_JDBC_PROVIDER (provider)->jdbc_driver;
}

/* 
 * Get provider's version, no need to change this
 */
static const gchar *
gda_jdbc_provider_get_version (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/*
 * make_url_from_params:
 *
 * Creates the URL to pass to the JDBC driver to open a connection. It uses the
 * jdbc_mappings.xml file
 *
 * Returns: a new string, or %NULL if not enough information found to create the connection URL
 */
static gchar *
make_url_from_params (GdaServerProvider *provider, GdaConnection *cnc,
		      GdaQuarkList *params, G_GNUC_UNUSED GdaQuarkList *auth)
{
	GBytes *data;
	const gchar *xmlstr;
	gsize data_size = 0;
	_jdbc_register_resource ();
	data = g_resources_lookup_data ("/jdbc/jdbc-mappings.xml", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	g_assert (data);
	xmlstr = g_bytes_get_data (data, &data_size);

	xmlDocPtr doc;
	doc = xmlParseMemory (xmlstr, data_size);
	g_bytes_unref (data);
	_jdbc_unregister_resource ();

        if (!doc)
		return NULL;

	xmlNodePtr root, node;
	GString *url = NULL;
	root = xmlDocGetRootElement (doc);
	if (strcmp ((gchar*) root->name, "jdbc-mappings"))
		goto out;

	for (node = root->children; node; node = node->next) {
                if (strcmp ((gchar *) node->name, "driver"))
                        continue;
		xmlChar *prop;
		prop = xmlGetProp (node, BAD_CAST "name");
		if (!prop)
			continue;
		if (!strcmp ((gchar*) prop, gda_server_provider_get_name (provider))) {
			xmlFree (prop);
			break;
		}
		xmlFree (prop);
        }
	if (!node)
		goto out;

	url = g_string_new ("");
	for (node = node->children; node; node = node->next) {
                if (!strcmp ((gchar *) node->name, "prefix")) {
			xmlChar *contents;
			contents = xmlNodeGetContent (node);
			if (contents && *contents)
				g_string_append (url, (gchar*) contents);
		}
		else if (!strcmp ((gchar *) node->name, "part")) {
			xmlChar *prop;
			const gchar *cvarvalue = NULL;
			gchar *varvalue = NULL;
			gboolean opt = FALSE;
			prop = xmlGetProp (node, BAD_CAST "variable");
			if (prop) {
				cvarvalue = gda_quark_list_find (params, (gchar*) prop);
				xmlFree (prop);
			}
			prop = xmlGetProp (node, BAD_CAST "optional");
			if (prop) {
				if ((*prop == 't') || (*prop == 'T'))
					opt = TRUE;
				xmlFree (prop);
			}

			prop = xmlGetProp (node, BAD_CAST "if");
			if (prop) {
				if (!strcmp ((gchar*) prop, "CncReadOnly")) {
					if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
						xmlFree (prop);
						prop = xmlGetProp (node, BAD_CAST "value");
						if (prop)
							varvalue = g_strdup ((gchar*) prop);
					}
				}
				if (prop)
					xmlFree (prop);
			}

			if (cvarvalue || varvalue) {
				prop = xmlGetProp (node, BAD_CAST "prefix");
				if (prop) {
					g_string_append (url, (gchar*) prop);
					xmlFree (prop);
				}
				g_string_append (url, varvalue ? varvalue : cvarvalue);
				g_free (varvalue);
			}
			else if (!varvalue && !cvarvalue && !opt) {
				/* missing parameter */
				g_string_free (url, TRUE);
				url = NULL;
				goto out;
			}
		}
        }

 out:
	xmlFreeDoc (doc);
	if (url)
		return g_string_free (url, FALSE);
	else
		return NULL;
}

/* 
 * Open connection request
 *
 * In this function, the following _must_ be done:
 *   - check for the presence and validify of the parameters required to actually open a connection,
 *     using @params
 *   - open the real connection to the database using the parameters previously checked
 *   - create a JdbcConnectionData structure and associate it to @cnc
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR gonnection event must be added to @cnc)
 */
static gboolean
gda_jdbc_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				   GdaQuarkList *params, GdaQuarkList *auth)
{
	GdaJdbcProvider *jprov;
	g_return_val_if_fail (GDA_IS_JDBC_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	jprov = (GdaJdbcProvider*) provider;

	/* Check for connection parameters */
	gchar *url;
	const gchar *cstr;
	cstr = gda_quark_list_find (params, "URL");
	if (cstr)
		url = g_strdup (cstr);
	else {
		url = make_url_from_params (provider, cnc, params, auth);
		if (!url) {
			gda_connection_add_event_string (cnc,
							 _("Missing parameters to open database connection"));
			return FALSE;
		}
	}

	/* Check for username / password */
	const gchar *username = NULL, *password = NULL;
	if (auth) {
		username = gda_quark_list_find (auth, "USERNAME");
		password = gda_quark_list_find (auth, "PASSWORD");
	}
	
	/* open the real connection to the database */
	g_assert (jprov->jprov_obj);
	GValue *obj_value;
	jstring jstr, jstr1, jstr2;
	JNIEnv *env;
	GError *error = NULL;
	gint error_code;
	gchar *sql_state;
	gboolean jni_detach;

	env = _gda_jdbc_get_jenv (&jni_detach, &error);

	if (!env) {
		gda_connection_add_event_string (cnc, "%s",
						 error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
		g_free (url);
		return FALSE;
	}

	jstr = (*env)->NewStringUTF (env, url);
	/*g_print ("URL = [%s] USERNAME = [%s] PASSWORD = [%s]\n", url, username, password);*/
	g_free (url);
	url = NULL;
	if (username)
		jstr1 = (*env)->NewStringUTF (env, username);
	else
		jstr1 = NULL;
	if (password)
		jstr2 = (*env)->NewStringUTF (env, password);
	else
		jstr2 = NULL;

	obj_value = jni_wrapper_method_call (env, GdaJProvider__openConnection, 
					     jprov->jprov_obj, &error_code, &sql_state, &error,
					     jstr, jstr1, jstr2);
	(*env)->DeleteLocalRef(env, jstr);
	if (jstr1)
		(*env)->DeleteLocalRef(env, jstr1);
	if (jstr2)
		(*env)->DeleteLocalRef(env, jstr2);

	if (!obj_value) {
		_gda_jdbc_make_error (cnc, error_code, sql_state, error);
		_gda_jdbc_release_jenv (jni_detach);
		return FALSE;
	}

	/* Create a new instance of the provider specific data associated to a connection (JdbcConnectionData),
	 * and set its contents */
	JdbcConnectionData *cdata;
	cdata = g_new0 (JdbcConnectionData, 1);
	gda_connection_internal_set_provider_data (cnc, (GdaServerProviderConnectionData*) cdata,
						   (GDestroyNotify) gda_jdbc_free_cnc_data);
	cdata->jcnc_obj = obj_value;

	_gda_jdbc_release_jenv (jni_detach);
	return TRUE;
}

/* 
 * Close connection request
 *
 * In this function, the following _must_ be done:
 *   - Actually close the connection to the database using @cnc's associated JdbcConnectionData structure
 *   - Free the JdbcConnectionData structure and its contents
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR gonnection event must be added to @cnc)
 */
static gboolean
gda_jdbc_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	JdbcConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	/* Close the connection using the C API */
	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return FALSE;

	/* Free the JdbcConnectionData structure and its contents */
	gda_jdbc_free_cnc_data (cdata);
	gda_connection_internal_set_provider_data (cnc, NULL, NULL);

	return TRUE;
}

/*
 * Server version request
 *
 * Returns the server version as a string
 */
static const gchar *
gda_jdbc_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	JdbcConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return FALSE;

	if (! cdata->server_version && cdata->jcnc_obj) {
		JNIEnv *jenv = NULL;
		gboolean jni_detach;
		GError *error = NULL;

		jenv = _gda_jdbc_get_jenv (&jni_detach, &error);
		if (!jenv) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
		else {
			GValue *res;
			res = jni_wrapper_method_call (jenv, GdaJConnection__getServerVersion,
						       cdata->jcnc_obj, NULL, NULL, NULL);
			if (res) {
				cdata->server_version = g_value_dup_string (res);
				gda_value_free (res);
			}
			_gda_jdbc_release_jenv (jni_detach);
		}
	}

	return cdata->server_version;
}

/*
 * Support operation request
 *
 * Tells what the implemented server operations are. To add support for an operation, the following steps are required:
 *   - create a jdbc_specs_....xml.in file describing the required and optional parameters for the operation
 *   - add it to the Makefile.am
 *   - make this method return TRUE for the operation type
 *   - implement the gda_jdbc_provider_render_operation() and gda_jdbc_provider_perform_operation() methods
 *
 */
static gboolean
gda_jdbc_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
				      G_GNUC_UNUSED GdaServerOperationType type,
				      G_GNUC_UNUSED GdaSet *options)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	/* use native provider for help */
	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Create operation request
 *
 * Creates a #GdaServerOperation. The following code is generic and should only be changed
 * if some further initialization is required, or if operation's contents is dependent on @cnc
 */
static GdaServerOperation *
gda_jdbc_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
				    G_GNUC_UNUSED GdaServerOperationType type, G_GNUC_UNUSED GdaSet *options,
				    G_GNUC_UNUSED GError **error)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	/* use native provider for help */
	TO_IMPLEMENT;
	
	return NULL;
}

/*
 * Render operation request
 */
static gchar *
gda_jdbc_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
				    G_GNUC_UNUSED GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	/* use native provider for help */
	TO_IMPLEMENT;
	
	return NULL;
}

/*
 * Perform operation request
 */
static gboolean
gda_jdbc_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
				     G_GNUC_UNUSED GdaServerOperation *op, GError **error)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	/* use native provider for help */
	TO_IMPLEMENT;
	return FALSE;
}

/*
 * Begin transaction request
 */
static gboolean
gda_jdbc_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				     G_GNUC_UNUSED const gchar *name,
				     G_GNUC_UNUSED GdaTransactionIsolation level, GError **error)
{
	JdbcConnectionData *cdata;
	GValue *jexec_res;
	GError *lerror = NULL;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;
	gint error_code;
	gchar *sql_state;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	jenv = _gda_jdbc_get_jenv (&jni_detach, error);
	if (!jenv) 
		return FALSE;

	jexec_res = jni_wrapper_method_call (jenv, GdaJConnection__begin,
					     cdata->jcnc_obj, &error_code, &sql_state, &lerror);
	if (!jexec_res) {
		if (error && lerror)
			*error = g_error_copy (lerror);
		_gda_jdbc_make_error (cnc, error_code, sql_state, lerror);
		_gda_jdbc_release_jenv (jni_detach);
		return FALSE;
	}
	gda_value_free (jexec_res);
	_gda_jdbc_release_jenv (jni_detach);

	return TRUE;
}

/*
 * Commit transaction request
 */
static gboolean
gda_jdbc_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				      G_GNUC_UNUSED const gchar *name, GError **error)
{
	JdbcConnectionData *cdata;
	GValue *jexec_res;
	GError *lerror = NULL;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;
	gint error_code;
	gchar *sql_state;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	jenv = _gda_jdbc_get_jenv (&jni_detach, error);
	if (!jenv) 
		return FALSE;

	jexec_res = jni_wrapper_method_call (jenv, GdaJConnection__commit,
					     cdata->jcnc_obj, &error_code, &sql_state, &lerror);
	if (!jexec_res) {
		if (error && lerror)
			*error = g_error_copy (lerror);
		_gda_jdbc_make_error (cnc, error_code, sql_state, lerror);
		_gda_jdbc_release_jenv (jni_detach);
		return FALSE;
	}
	gda_value_free (jexec_res);
	_gda_jdbc_release_jenv (jni_detach);

	return TRUE;
}

/*
 * Rollback transaction request
 */
static gboolean
gda_jdbc_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					G_GNUC_UNUSED const gchar *name, GError **error)
{
	JdbcConnectionData *cdata;
	GValue *jexec_res;
	GError *lerror = NULL;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;
	gint error_code;
	gchar *sql_state;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	jenv = _gda_jdbc_get_jenv (&jni_detach, error);
	if (!jenv) 
		return FALSE;

	jexec_res = jni_wrapper_method_call (jenv, GdaJConnection__rollback,
					     cdata->jcnc_obj, &error_code, &sql_state, &lerror);
	if (!jexec_res) {
		if (error && lerror)
			*error = g_error_copy (lerror);
		_gda_jdbc_make_error (cnc, error_code, sql_state, lerror);
		_gda_jdbc_release_jenv (jni_detach);
		return FALSE;
	}
	gda_value_free (jexec_res);
	_gda_jdbc_release_jenv (jni_detach);

	return TRUE;
}

/*
 * Add savepoint request
 */
static gboolean
gda_jdbc_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				 const gchar *name, GError **error)
{
	JdbcConnectionData *cdata;
	GValue *jexec_res;
	GError *lerror = NULL;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;
	gint error_code;
	gchar *sql_state;
	jstring jname;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	jenv = _gda_jdbc_get_jenv (&jni_detach, error);
	if (!jenv) 
		return FALSE;

	if (name)
		jname = (*jenv)->NewStringUTF (jenv, name);
	else
		jname = (*jenv)->NewStringUTF (jenv, "");
	if ((*jenv)->ExceptionCheck (jenv)) {
		_gda_jdbc_release_jenv (jni_detach);
		return FALSE;
	}

	jexec_res = jni_wrapper_method_call (jenv, GdaJConnection__addSavepoint,
					     cdata->jcnc_obj, &error_code, &sql_state, &lerror, jname);
	(*jenv)->DeleteLocalRef (jenv, jname);
	if (!jexec_res) {
		if (error && lerror)
			*error = g_error_copy (lerror);
		_gda_jdbc_make_error (cnc, error_code, sql_state, lerror);
		_gda_jdbc_release_jenv (jni_detach);
		return FALSE;
	}
	gda_value_free (jexec_res);
	_gda_jdbc_release_jenv (jni_detach);

	return TRUE;
}

/*
 * Rollback savepoint request
 */
static gboolean
gda_jdbc_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				      const gchar *name, GError **error)
{
	JdbcConnectionData *cdata;
	GValue *jexec_res;
	GError *lerror = NULL;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;
	gint error_code;
	gchar *sql_state;
	jstring jname;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	jenv = _gda_jdbc_get_jenv (&jni_detach, error);
	if (!jenv) 
		return FALSE;

	if (name)
		jname = (*jenv)->NewStringUTF (jenv, name);
	else
		jname = (*jenv)->NewStringUTF (jenv, "");
	if ((*jenv)->ExceptionCheck (jenv)) {
		_gda_jdbc_release_jenv (jni_detach);
		return FALSE;
	}

	jexec_res = jni_wrapper_method_call (jenv, GdaJConnection__rollbackSavepoint,
					     cdata->jcnc_obj, &error_code, &sql_state, &lerror, jname);
	(*jenv)->DeleteLocalRef (jenv, jname);
	if (!jexec_res) {
		if (error && lerror)
			*error = g_error_copy (lerror);
		_gda_jdbc_make_error (cnc, error_code, sql_state, lerror);
		_gda_jdbc_release_jenv (jni_detach);
		return FALSE;
	}
	gda_value_free (jexec_res);
	_gda_jdbc_release_jenv (jni_detach);

	return TRUE;
}

/*
 * Delete savepoint request
 */
static gboolean
gda_jdbc_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				    const gchar *name, GError **error)
{
	JdbcConnectionData *cdata;
	GValue *jexec_res;
	GError *lerror = NULL;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;
	gint error_code;
	gchar *sql_state;
	jstring jname;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	jenv = _gda_jdbc_get_jenv (&jni_detach, error);
	if (!jenv) 
		return FALSE;

	if (name)
		jname = (*jenv)->NewStringUTF (jenv, name);
	else
		jname = (*jenv)->NewStringUTF (jenv, "");
	if ((*jenv)->ExceptionCheck (jenv)) {
		_gda_jdbc_release_jenv (jni_detach);
		return FALSE;
	}

	jexec_res = jni_wrapper_method_call (jenv, GdaJConnection__releaseSavepoint,
					     cdata->jcnc_obj, &error_code, &sql_state, &lerror, jname);
	(*jenv)->DeleteLocalRef (jenv, jname);
	if (!jexec_res) {
		if (error && lerror)
			*error = g_error_copy (lerror);
		_gda_jdbc_make_error (cnc, error_code, sql_state, lerror);
		_gda_jdbc_release_jenv (jni_detach);
		return FALSE;
	}
	gda_value_free (jexec_res);
	_gda_jdbc_release_jenv (jni_detach);

	return TRUE;
}

/*
 * Feature support request
 */
static gboolean
gda_jdbc_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc, GdaConnectionFeature feature)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	switch (feature) {
	case GDA_CONNECTION_FEATURE_SQL :
		return TRUE;
	default: 
		return FALSE;
	}
}

/*
 * Get data handler request
 *
 * This method allows one to obtain a pointer to a #GdaDataHandler object specific to @type or @dbms_type (@dbms_type
 * must be considered only if @type is not a valid GType).
 *
 * A data handler allows one to convert a value between its different representations which are a human readable string,
 * an SQL representation and a GValue.
 *
 * The recommended method is to create GdaDataHandler objects only when they are needed and to keep a reference to them
 * for further usage, using the gda_server_provider_handler_declare() method.
 *
 * The implementation shown here does not define any specific data handler, but there should be some for at least 
 * binary and time related types.
 */
static GdaDataHandler *
gda_jdbc_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
				    GType type, const gchar *dbms_type)
{
	GdaDataHandler *dh;
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	if (type == G_TYPE_INVALID) {
		TO_IMPLEMENT; /* use @dbms_type */
		dh = NULL;
	}
	/*
	else if ((type == GDA_TYPE_BINARY) ||
		 (type == GDA_TYPE_BLOB)) {
		dh = gda_server_provider_handler_find (provider, cnc, type, NULL);
                if (!dh) {
                        dh = gda_postgres_handler_bin_new (cnc);
                        if (dh) {
                                gda_server_provider_handler_declare (provider, dh, cnc, GDA_TYPE_BINARY, NULL);
                                gda_server_provider_handler_declare (provider, dh, cnc, GDA_TYPE_BLOB, NULL);
                                g_object_unref (dh);
                        }
                }
	}
	*/
	else if ((type == GDA_TYPE_TIME) ||
		 (type == GDA_TYPE_TIMESTAMP) ||
		 (type == G_TYPE_DATE)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
                if (!dh) {
                        dh = gda_handler_time_new ();
                        gda_handler_time_set_sql_spec ((GdaHandlerTime *) dh, G_DATE_YEAR,
						       G_DATE_MONTH, G_DATE_DAY, '-', FALSE);
                        gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DATE, NULL);
                        gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIME, NULL);
                        gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIMESTAMP, NULL);
                        g_object_unref (dh);
                }
	}
	else
		dh = gda_server_provider_handler_use_default (provider, type);

	return dh;
}

/*
 * Get default DBMS type request
 *
 * This method returns the "preferred" DBMS type for GType
 */
static const gchar*
gda_jdbc_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc, GType type)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}

	TO_IMPLEMENT;

	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == GDA_TYPE_USHORT) ||
	    (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) ||
	    (type == G_TYPE_ULONG) ||
	    (type == G_TYPE_UINT) ||
	    (type == G_TYPE_UINT64))
		return "integer";

	if ((type == GDA_TYPE_BINARY) ||
	    (type == GDA_TYPE_BLOB))
		return "blob";

	if (type == G_TYPE_BOOLEAN)
		return "boolean";
	
	if ((type == G_TYPE_DATE) || 
	    (type == GDA_TYPE_GEOMETRIC_POINT) ||
	    (type == G_TYPE_OBJECT) ||
	    (type == G_TYPE_STRING) ||
	    (type == GDA_TYPE_TIME) ||
	    (type == GDA_TYPE_TIMESTAMP) ||
	    (type == G_TYPE_GTYPE))
		return "string";

	if ((type == G_TYPE_DOUBLE) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT))
		return "real";
	
	if (type == GDA_TYPE_TIME)
		return "time";
	if (type == GDA_TYPE_TIMESTAMP)
		return "timestamp";
	if (type == G_TYPE_DATE)
		return "date";

	if ((type == GDA_TYPE_NULL) ||
	    (type == G_TYPE_GTYPE))
		return NULL;

	return "text";
}

/*
 * GdaStatement to SQL request
 * 
 * This method renders a #GdaStatement into its SQL representation.
 *
 * The implementation show here simply calls gda_statement_to_sql_extended() but the rendering
 * can be specialized to the database's SQL dialect, see the implementation of gda_statement_to_sql_extended()
 * and SQLite's specialized rendering for more details
 *
 * NOTE: This implementation can call gda_statement_to_sql_extended() _ONLY_ if it passes a %NULL @cnc argument
 */
static gchar *
gda_jdbc_provider_statement_to_sql (GdaServerProvider *provider, GdaConnection *cnc,
				    GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
				    GSList **params_used, GError **error)
{
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}

	return gda_statement_to_sql_extended (stmt, NULL, params, flags, params_used, error);
}

/*
 * Statement prepare request
 *
 * This methods "converts" @stmt into a prepared statement. A prepared statement is a notion
 * specific in its implementation details to the C API used here. If successfull, it must create
 * a new #GdaJdbcPStmt object and declare it to @cnc.
 */
static gboolean
gda_jdbc_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaStatement *stmt, GError **error)
{
	GdaJdbcPStmt *ps;
	JdbcConnectionData *cdata;
	gboolean retval = FALSE;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	/* fetch prepares stmt if already done */
	ps = (GdaJdbcPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (ps)
		return TRUE;

	/* Get private data */
	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* render as SQL understood by JDBC */
	GdaSet *params = NULL;
	gchar *sql;
	GSList *used_params = NULL;
	if (! gda_statement_get_parameters (stmt, &params, error))
                return FALSE;
        sql = gda_jdbc_provider_statement_to_sql (provider, cnc, stmt, params, GDA_STATEMENT_SQL_PARAMS_AS_UQMARK,
						  &used_params, error);

	JNIEnv *jenv = NULL;
        if (!sql)
		goto out;

	/* prepare @stmt using the C API, creates @ps */
	GValue *pstmt_obj;
	gboolean jni_detach = FALSE;
	jstring jsql;
	
	jenv = _gda_jdbc_get_jenv (&jni_detach, error);
	if (!jenv)
		goto out;
	
	jsql = (*jenv)->NewStringUTF (jenv, sql);
	pstmt_obj = jni_wrapper_method_call (jenv, GdaJConnection__prepareStatement,
					     cdata->jcnc_obj, NULL, NULL, error, jsql);
	(*jenv)->DeleteLocalRef (jenv, jsql);
	if (!pstmt_obj) 
		goto out;

	/* make a list of the parameter names used in the statement */
	GSList *param_ids = NULL;
        if (used_params) {
                GSList *list;
		jbyte *ctypes;
		gint i, nparams;

		nparams = g_slist_length (used_params);
		ctypes = g_new (jbyte, nparams);

                for (i = 0, list = used_params;
		     list;
		     i++, list = list->next) {
                        const gchar *cid;
                        cid = gda_holder_get_id (GDA_HOLDER (list->data));
                        if (cid) {
                                param_ids = g_slist_append (param_ids, g_strdup (cid));
				ctypes [i] = _gda_jdbc_gtype_to_proto_type (gda_holder_get_g_type ((GdaHolder*) list->data));
				/* g_print ("PREPARATION: param ID: %s\n", cid);*/
                        }
                        else {
                                g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
                                             "%s", _("Unnamed parameter is not allowed in prepared statements"));
                                g_slist_foreach (param_ids, (GFunc) g_free, NULL);
                                g_slist_free (param_ids);
				g_free (ctypes);
                                goto out;
                        }
                }

		/* inform JDBC of the parameters' data types */
		jbyteArray jtypes;
		jtypes = (*jenv)->NewByteArray (jenv, nparams);
		if (jni_wrapper_handle_exception (jenv, NULL, NULL, error)) {
			g_free (ctypes);
			g_slist_foreach (param_ids, (GFunc) g_free, NULL);
			g_slist_free (param_ids);
			goto out;
		}
		
		(*jenv)->SetByteArrayRegion (jenv, jtypes, 0, nparams, ctypes);
		if (jni_wrapper_handle_exception (jenv, NULL, NULL, error)) {
			g_free (ctypes);
			(*jenv)->DeleteLocalRef (jenv, jtypes);
			g_slist_foreach (param_ids, (GFunc) g_free, NULL);
			g_slist_free (param_ids);
			goto out;
		}

		GValue *jexec_res;
		jexec_res = jni_wrapper_method_call (jenv, GdaJPStmt__declareParamTypes,
						     pstmt_obj, NULL, NULL, error, jni_cpointer_to_jlong (cnc), jtypes);
		(*jenv)->DeleteLocalRef (jenv, jtypes);
		g_free (ctypes);
		
		if (!jexec_res) {
			g_slist_foreach (param_ids, (GFunc) g_free, NULL);
			g_slist_free (param_ids);
			goto out;
		}
		gda_value_free (jexec_res);
        }

	/* create a prepared statement object */
	ps = gda_jdbc_pstmt_new (pstmt_obj);
	gda_pstmt_set_gda_statement (_GDA_PSTMT (ps), stmt);
        _GDA_PSTMT (ps)->param_ids = param_ids;
        _GDA_PSTMT (ps)->sql = sql;

	gda_connection_add_prepared_statement (cnc, stmt, (GdaPStmt *) ps);
	g_object_unref (ps);
	retval = TRUE;

 out:
	if (used_params)
                g_slist_free (used_params);
        if (params)
                g_object_unref (params);
	if (jenv)
		_gda_jdbc_release_jenv (jni_detach);

	return retval;
}

/*
 * Execute statement request
 *
 * Executes a statement. This method should do the following:
 *    - try to prepare the statement if not yet done
 *    - optionnally reset the prepared statement
 *    - bind the variables's values (which are in @params)
 *    - add a connection event to log the execution
 *    - execute the prepared statement
 *
 * If @stmt is an INSERT statement and @last_inserted_row is not NULL then additional actions must be taken to return the
 * actual inserted row
 */
static GObject *
gda_jdbc_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaStatement *stmt, GdaSet *params,
				     GdaStatementModelUsage model_usage, 
				     GType *col_types, GdaSet **last_inserted_row, GError **error)
{
	GdaJdbcPStmt *ps;
	JdbcConnectionData *cdata;
	gboolean allow_noparam;
        gboolean empty_rs = FALSE; /* TRUE when @allow_noparam is TRUE and there is a problem with @params
                                      => resulting data model will be empty (0 row) */
	JNIEnv *jenv = NULL;
	gboolean jni_detach;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	if (! (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS) &&
            ! (model_usage & GDA_STATEMENT_MODEL_CURSOR_FORWARD))
                model_usage |= GDA_STATEMENT_MODEL_RANDOM_ACCESS;
	
        allow_noparam = (model_usage & GDA_STATEMENT_MODEL_ALLOW_NOPARAM) &&
                (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT);
	
        if (last_inserted_row)
                *last_inserted_row = NULL;

	/* Get private data */
	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return NULL;

	jenv = _gda_jdbc_get_jenv (&jni_detach, error);
	if (!jenv) 
		return NULL;

	/* get/create new prepared statement */
	ps = (GdaJdbcPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (!ps) {
		if (!gda_jdbc_provider_statement_prepare (provider, cnc, stmt, NULL)) {
			/* this case can appear for example if some variables are used in places
			 * where the C API cannot allow them (for example if the variable is the table name
			 * in a SELECT statement). The action here is to get the actual SQL code for @stmt,
			 * and use that SQL instead of @stmt to create another GdaJdbcPStmt object.
			 */
			gchar *sql;
			jstring jsql;
			GValue *pstmt_obj;

                        sql = gda_jdbc_provider_statement_to_sql (provider, cnc, stmt, params, 0, NULL, error);
                        if (!sql) {
				_gda_jdbc_release_jenv (jni_detach);
                                return NULL;
			}
			
			jsql = (*jenv)->NewStringUTF (jenv, sql);
			pstmt_obj = jni_wrapper_method_call (jenv, GdaJConnection__prepareStatement,
							     cdata->jcnc_obj, NULL, NULL, error, jsql);
			(*jenv)->DeleteLocalRef (jenv, jsql);
                        g_free (sql);

                        if (!pstmt_obj) {
				_gda_jdbc_release_jenv (jni_detach);
                                return NULL;
			}

			ps = gda_jdbc_pstmt_new (pstmt_obj);
		}
		else {
			ps = (GdaJdbcPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
			g_object_ref (ps);
		}
	}
	else
		g_object_ref (ps);
	g_assert (ps);

	/* reset the prepared statement's parameters */
	GValue *jexec_res;
	jexec_res = jni_wrapper_method_call (jenv, GdaJPStmt__clearParameters,
					     ps->pstmt_obj, NULL, NULL, error);
	if (!jexec_res) {
		g_object_unref (ps);
		_gda_jdbc_release_jenv (jni_detach);
		return NULL;
	}
	gda_value_free (jexec_res);

	/* bind statement's parameters */
	GSList *list;
	GdaConnectionEvent *event = NULL;
	int i;
	for (i = 0, list = _GDA_PSTMT (ps)->param_ids; list; list = list->next, i++) {
		const gchar *pname = (gchar *) list->data;
		GdaHolder *h;
		GError *lerror = NULL;

		/* find requested parameter */
		if (!params) {
			event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, _("Missing parameter(s) to execute query"));
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
				     "%s", _("Missing parameter(s) to execute query"));
			break;
		}

		h = gda_set_get_holder (params, pname);
		if (!h) {
			gchar *tmp = gda_alphanum_to_text (g_strdup (pname + 1));
			if (tmp) {
				h = gda_set_get_holder (params, tmp);
				g_free (tmp);
			}
		}
		if (!h) {
			if (allow_noparam) {
                                /* bind param to NULL */
				jexec_res = jni_wrapper_method_call (jenv, GdaJPStmt__setParameterValue,
								     ps->pstmt_obj, NULL, NULL, &lerror, i, 0);
				if (!jexec_res) {
					event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
					if (lerror)
						gda_connection_event_set_description (event,
						lerror->message ? lerror->message : _("No detail"));
					g_propagate_error (error, lerror);
					break;
				}
				gda_value_free (jexec_res);
                                empty_rs = TRUE;
                                continue;
			}
			else {
				gchar *str;
				str = g_strdup_printf (_("Missing parameter '%s' to execute query"), pname);
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
				g_free (str);
				break;
                        }

		}
		if (!gda_holder_is_valid (h)) {
			if (allow_noparam) {
                                /* bind param to NULL */
				jexec_res = jni_wrapper_method_call (jenv, GdaJPStmt__setParameterValue,
								     ps->pstmt_obj, NULL, NULL, &lerror, i, 0);
				if (!jexec_res) {
					event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
					if (lerror)
						gda_connection_event_set_description (event,
						lerror->message ? lerror->message : _("No detail"));
					g_propagate_error (error, lerror);
					break;
				}
				gda_value_free (jexec_res);
                                empty_rs = TRUE;
                                continue;
			}
			else {
				gchar *str;
				str = g_strdup_printf (_("Parameter '%s' is invalid"), pname);
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
				g_free (str);
				break;
                        }
		}
		else if (gda_holder_value_is_default (h) && !gda_holder_get_value (h)) {
			/* create a new GdaStatement to handle all default values and execute it instead */
			GdaSqlStatement *sqlst;
			GError *lerror = NULL;
			sqlst = gda_statement_rewrite_for_default_values (stmt, params, TRUE, &lerror);
			if (!sqlst) {
				event = gda_connection_point_available_event (cnc,
									      GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, lerror && lerror->message ? 
								      lerror->message :
								      _("Can't rewrite statement handle default values"));
				g_propagate_error (error, lerror);
				break;
			}
			
			GdaStatement *rstmt;
			GObject *res;
			rstmt = g_object_new (GDA_TYPE_STATEMENT, "structure", sqlst, NULL);
			gda_sql_statement_free (sqlst);
			g_object_unref (ps);
			_gda_jdbc_release_jenv (jni_detach);
			res = gda_jdbc_provider_statement_execute (provider, cnc,
								   rstmt, params,
								   model_usage,
								   col_types, last_inserted_row, error);
			g_object_unref (rstmt);
			return res;
		}

		/* actual binding using the C API, for parameter at position @i */
		const GValue *value = gda_holder_get_value (h);
		if (!value || gda_value_is_null (value)) {
			GdaStatement *rstmt;
			if (! gda_rewrite_statement_for_null_parameters (stmt, params, &rstmt, error)) {
				jexec_res = jni_wrapper_method_call (jenv, GdaJPStmt__setParameterValue,
								     ps->pstmt_obj, NULL, NULL, &lerror, i, 
								     (G_VALUE_TYPE (value) == GDA_TYPE_NULL) ? (glong) 0 : (glong) value);
				if (!jexec_res) {
					event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
					if (lerror)
						gda_connection_event_set_description (event,
										      lerror->message ? lerror->message : _("No detail"));
					
					g_propagate_error (error, lerror);
					break;
				}
				gda_value_free (jexec_res);
			}
			else if (!rstmt)
				return NULL;
			else {
				_gda_jdbc_release_jenv (jni_detach);

				/* The strategy here is to execute @rstmt using its prepared
				 * statement, but with common data from @ps. Beware that
				 * the @param_ids attribute needs to be retained (i.e. it must not
				 * be the one copied from @ps) */
				GObject *obj;
				GdaJdbcPStmt *tps;
				GdaPStmt *gtps;
				GSList *prep_param_ids, *copied_param_ids;
				if (!gda_jdbc_provider_statement_prepare (provider, cnc,
									  rstmt, error))
					return NULL;
				tps = (GdaJdbcPStmt *)
					gda_connection_get_prepared_statement (cnc, rstmt);
				gtps = (GdaPStmt *) tps;

				/* keep @param_ids to avoid being cleared by gda_pstmt_copy_contents() */
				prep_param_ids = gtps->param_ids;
				gtps->param_ids = NULL;
				
				/* actual copy */
				gda_pstmt_copy_contents ((GdaPStmt *) ps, (GdaPStmt *) tps);

				/* restore previous @param_ids */
				copied_param_ids = gtps->param_ids;
				gtps->param_ids = prep_param_ids;

				/* execute */
				obj = gda_jdbc_provider_statement_execute (provider, cnc,
									   rstmt, params,
									   model_usage,
									   col_types,
									   last_inserted_row, error);
				/* clear original @param_ids and restore copied one */
				g_slist_foreach (prep_param_ids, (GFunc) g_free, NULL);
				g_slist_free (prep_param_ids);

				gtps->param_ids = copied_param_ids;

				/*if (GDA_IS_DATA_MODEL (obj))
				  gda_data_model_dump ((GdaDataModel*) obj, NULL);*/

				g_object_unref (rstmt);
				g_object_unref (ps);
				return obj;
			}
		}
		else {
			jexec_res = jni_wrapper_method_call (jenv, GdaJPStmt__setParameterValue,
							     ps->pstmt_obj, NULL, NULL, &lerror, i, 
							     (G_VALUE_TYPE (value) == GDA_TYPE_NULL) ? (glong) 0 : (glong) value);
			if (!jexec_res) {
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				if (lerror)
					gda_connection_event_set_description (event,
									      lerror->message ? lerror->message : _("No detail"));
				
				g_propagate_error (error, lerror);
				break;
			}
			gda_value_free (jexec_res);
		}
	}
		
	if (event) {
		gda_connection_add_event (cnc, event);
		g_object_unref (ps);
		_gda_jdbc_release_jenv (jni_detach);
		return NULL;
	}
	
	/* add a connection event for the execution */
	event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_COMMAND);
        gda_connection_event_set_description (event, _GDA_PSTMT (ps)->sql);
        gda_connection_add_event (cnc, event);

	if (empty_rs) {
		/* There are some missing parameters, so the SQL can't be executed but we still want
		 * to execute something to get the columns correctly. A possibility is to actually
		 * execute another SQL which is the code shown here.
		 *
		 * To adapt depending on the C API and its features */
		GdaStatement *estmt;
                gchar *esql;
		jstring jsql;
                estmt = gda_select_alter_select_for_empty (stmt, error);
                if (!estmt) {
			g_object_unref (ps);
			_gda_jdbc_release_jenv (jni_detach);
                        return NULL;
		}
                esql = gda_statement_to_sql (estmt, NULL, error);
                g_object_unref (estmt);
                if (!esql) {
			g_object_unref (ps);
			_gda_jdbc_release_jenv (jni_detach);
                        return NULL;
		}

		/* Execute the 'esql' SQL code */
		jsql = (*jenv)->NewStringUTF (jenv, esql);
                g_free (esql);
		jexec_res = jni_wrapper_method_call (jenv, GdaJConnection__executeDirectSQL,
						     cdata->jcnc_obj, NULL, NULL, error, jsql);
		(*jenv)->DeleteLocalRef (jenv, jsql);
		if (!jexec_res) {
			g_object_unref (ps);
			_gda_jdbc_release_jenv (jni_detach);
			return NULL;
		}
	}
	else {
		/* Execute the _GDA_PSTMT (ps)->sql SQL code */
		gboolean is_rs;
		jexec_res = jni_wrapper_method_call (jenv, GdaJPStmt__execute,
						     ps->pstmt_obj, NULL, NULL, error);
		if (!jexec_res) {
			g_object_unref (ps);
			_gda_jdbc_release_jenv (jni_detach);
			return NULL;
		}
		
		is_rs = g_value_get_boolean (jexec_res);
		gda_value_free (jexec_res);
		if (is_rs) {
			jexec_res = jni_wrapper_method_call (jenv, GdaJPStmt__getResultSet,
							     ps->pstmt_obj, NULL, NULL, error);
			if (!jexec_res) {
				g_object_unref (ps);
				_gda_jdbc_release_jenv (jni_detach);
				return NULL;
			}
		}
		else {
			jexec_res = jni_wrapper_method_call (jenv, GdaJPStmt__getImpactedRows,
							     ps->pstmt_obj, NULL, NULL, error);
			if (!jexec_res) {
				g_object_unref (ps);
				_gda_jdbc_release_jenv (jni_detach);
				return NULL;
			}
		}
	}
	
	if (G_VALUE_TYPE (jexec_res) == GDA_TYPE_JNI_OBJECT) {
		/* Note: at this point jexec_res must contain a GdaJResultSet JAVA object */
		GObject *data_model;
		GdaDataModelAccessFlags flags;

		if (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS)
			flags = GDA_DATA_MODEL_ACCESS_RANDOM;
		else
			flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

                data_model = (GObject *) gda_jdbc_recordset_new (cnc, ps, params, jenv, jexec_res, flags, col_types);
		gda_connection_internal_statement_executed (cnc, stmt, params, NULL); /* required: help @cnc keep some stats */

		g_object_unref (ps);
		_gda_jdbc_release_jenv (jni_detach);
		return data_model;
        }
	else {
		GdaSet *set = NULL;
		
		/*
		gchar *str;
		GdaConnectionEvent *event;

		event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_NOTICE);
		str = g_strdup (PQcmdStatus (pg_res));
		gda_connection_event_set_description (event, str);
		g_free (str);
		gda_connection_add_event (cnc, event);
		*/

		set = gda_set_new_inline (1, "IMPACTED_ROWS", G_TYPE_INT, g_value_get_int (jexec_res));
		gda_value_free (jexec_res);
		
		/*
		if ((PQoidValue (pg_res) != InvalidOid) && last_inserted_row)
			*last_inserted_row = make_last_inserted_set (cnc, stmt, PQoidValue (pg_res));
			*/
		gda_connection_internal_statement_executed (cnc, stmt, params, event); /* required: help @cnc keep some stats */
		g_object_unref (ps);
		_gda_jdbc_release_jenv (jni_detach);
		return (GObject*) set;
	}
}

/*
 * Rewrites a statement in case some parameters in @params are set to DEFAULT, for INSERT or UPDATE statements
 *
 * Safely removes any default value inserted or updated
 */
static GdaSqlStatement *
gda_jdbc_provider_statement_rewrite (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaStatement *stmt, GdaSet *params, GError **error)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}
	return gda_statement_rewrite_for_default_values (stmt, params, TRUE, error);
}

/*
 * starts a distributed transaction: put the XA transaction in the ACTIVE state
 */
static gboolean
gda_jdbc_provider_xa_start (GdaServerProvider *provider, GdaConnection *cnc, 
				const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	JdbcConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* 
	 * see Sun's Java Transaction API:
	 * http://java.sun.com/javaee/technologies/jta/index.jsp
	 */
	TO_IMPLEMENT;
	return FALSE;
}

/*
 * put the XA transaction in the IDLE state: the connection won't accept any more modifications.
 * This state is required by some database providers before actually going to the PREPARED state
 */
static gboolean
gda_jdbc_provider_xa_end (GdaServerProvider *provider, GdaConnection *cnc, 
			      const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	JdbcConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * prepares the distributed transaction: put the XA transaction in the PREPARED state
 */
static gboolean
gda_jdbc_provider_xa_prepare (GdaServerProvider *provider, GdaConnection *cnc, 
				  const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	JdbcConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * commits the distributed transaction: actually write the prepared data to the database and
 * terminates the XA transaction
 */
static gboolean
gda_jdbc_provider_xa_commit (GdaServerProvider *provider, GdaConnection *cnc, 
				 const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	JdbcConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * Rolls back an XA transaction, possible only if in the ACTIVE, IDLE or PREPARED state
 */
static gboolean
gda_jdbc_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc, 
				   const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	JdbcConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * Lists all XA transactions that are in the PREPARED state
 *
 * Returns: a list of GdaXaTransactionId structures, which will be freed by the caller
 */
static GList *
gda_jdbc_provider_xa_recover (GdaServerProvider *provider, GdaConnection *cnc,
				  G_GNUC_UNUSED GError **error)
{
	JdbcConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return NULL;

	TO_IMPLEMENT;
	return NULL;
}

/*
 * Free connection's specific data
 */
static void
gda_jdbc_free_cnc_data (JdbcConnectionData *cdata)
{
	if (!cdata)
		return;

	g_free (cdata->server_version);

	if (cdata->jcnc_obj) {
		/* force the connection to be closed */
		JNIEnv *jenv = NULL;
		gboolean jni_detach;
		GError *error = NULL;

		jenv = _gda_jdbc_get_jenv (&jni_detach, &error);
		if (!jenv) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
		else {
			GValue *res;
			res = jni_wrapper_method_call (jenv, GdaJConnection__close,
						       cdata->jcnc_obj, NULL, NULL, &error);
			if (res) {
#ifdef GDA_DEBUG
				g_print ("Connection closed!\n");
#endif
				gda_value_free (res);
			}
			else {
				g_warning ("Could not propertly close JDBC connection (will be done by the garbage collector): %s",
					   error && error->message ? error->message : "No detail");
				if (error)
					g_error_free (error);
			}
			_gda_jdbc_release_jenv (jni_detach);
		}
		gda_value_free (cdata->jcnc_obj);
		cdata->jcnc_obj = NULL;
	}

	if (cdata->jmeta_obj)
		gda_value_free (cdata->jmeta_obj);

	g_free (cdata);
}

/**
 * gda_jdbc_provider_new
 * @jdbc_driver: the JDBC driver to use (such as "sun.jdbc.odbc.JdbcOdbcDriver")
 * @error: a place to store errors, or %NULL
 *
 * Returns: a new #GdaServerProvider for that JDBC driver, or %NULL if an error occurred
 */
GdaServerProvider *
gda_jdbc_provider_new (const gchar *jdbc_driver, GError **error)
{
	GdaServerProvider *prov;
	g_return_val_if_fail (jdbc_driver, NULL);

	if (!_jdbc_provider_java_vm) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", "No JVM runtime identified (this should not happen at this point)!");
		return NULL;
	}

	/* create a JAVA's GdaJProvider object */
	JNIEnv *env;
	GValue *obj_value;
	jstring jstr;
	gboolean detach_jni;

	env = _gda_jdbc_get_jenv (&detach_jni, error);
	if (!env)
		return NULL;

	jstr = (*env)->NewStringUTF(env, jdbc_driver);
	obj_value = jni_wrapper_instantiate_object (env, GdaJProvider_class, "(Ljava/lang/String;)V", error, jstr);
	(*env)->DeleteLocalRef(env, jstr);
	if (!obj_value) {
		_gda_jdbc_release_jenv (detach_jni);
		return NULL;
	}
		
	prov = (GdaServerProvider*) g_object_new (GDA_TYPE_JDBC_PROVIDER, NULL);
	GDA_JDBC_PROVIDER (prov)->jprov_obj = obj_value;
	_gda_jdbc_release_jenv (detach_jni);

	GDA_JDBC_PROVIDER (prov)->jdbc_driver = g_strdup (jdbc_driver);

	return prov;
}
