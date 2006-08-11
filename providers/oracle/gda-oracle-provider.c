/* GDA Oracle provider
 * Copyright (C) 2000 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Tim Coleman <tim@timcoleman.com>
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

#include <libgda/gda-row.h>
#include <libgda/gda-parameter-list.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-server-provider-extra.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include "gda-oracle.h"
#include "gda-oracle-provider.h"
#include "gda-oracle-recordset.h"

#include <libgda/sql-delimiter/gda-sql-delimiter.h>

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_ORACLE_HANDLE "GDA_Oracle_OracleHandle"

static void gda_oracle_provider_class_init (GdaOracleProviderClass *klass);
static void gda_oracle_provider_init       (GdaOracleProvider *provider,
					    GdaOracleProviderClass *klass);
static void gda_oracle_provider_finalize   (GObject *object);

static const gchar *gda_oracle_provider_get_version (GdaServerProvider *provider);
static gboolean gda_oracle_provider_open_connection (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaQuarkList *params,
						     const gchar *username,
						     const gchar *password);
static gboolean gda_oracle_provider_close_connection (GdaServerProvider *provider,
						      GdaConnection *cnc);
static const gchar *gda_oracle_provider_get_server_version (GdaServerProvider *provider,
							    GdaConnection *cnc);
static const gchar *gda_oracle_provider_get_database (GdaServerProvider *provider,
						      GdaConnection *cnc);
static GList *gda_oracle_provider_execute_command (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   GdaCommand *cmd,
						   GdaParameterList *params);
static gboolean gda_oracle_provider_begin_transaction (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaTransaction *xaction);
static gboolean gda_oracle_provider_commit_transaction (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaTransaction *xaction);
static gboolean gda_oracle_provider_rollback_transaction (GdaServerProvider *provider,
							  GdaConnection *cnc,
							  GdaTransaction *xaction);
static gboolean gda_oracle_provider_supports (GdaServerProvider *provider,
					      GdaConnection *cnc,
					      GdaConnectionFeature feature);

static GdaServerProviderInfo *gda_oracle_provider_get_info (GdaServerProvider *provider,
							    GdaConnection *cnc);

static GdaDataModel *gda_oracle_provider_get_schema (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaConnectionSchema schema,
						     GdaParameterList *params);

static GObjectClass *parent_class = NULL;

typedef struct {
	gboolean primary_key;
	gboolean unique;
	gchar *references;
} GdaOracleIndexData;

/*
 * GdaOracleProvider class implementation
 */

static void
gda_oracle_provider_class_init (GdaOracleProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_oracle_provider_finalize;

	provider_class->get_version = gda_oracle_provider_get_version;
	provider_class->get_server_version = gda_oracle_provider_get_server_version;
	provider_class->get_info = gda_oracle_provider_get_info;
	provider_class->supports = gda_oracle_provider_supports;
	provider_class->get_schema = gda_oracle_provider_get_schema;

	provider_class->get_data_handler = NULL;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = NULL;

	provider_class->open_connection = gda_oracle_provider_open_connection;
	provider_class->close_connection = gda_oracle_provider_close_connection;
	provider_class->get_database = gda_oracle_provider_get_database;
	provider_class->change_database = NULL;

	provider_class->supports_operation = NULL;
        provider_class->create_operation = NULL;
        provider_class->render_operation = NULL;
        provider_class->perform_operation = NULL;

	provider_class->execute_command = gda_oracle_provider_execute_command;
	provider_class->get_last_insert_id = NULL;

	provider_class->begin_transaction = gda_oracle_provider_begin_transaction;
	provider_class->commit_transaction = gda_oracle_provider_commit_transaction;
	provider_class->rollback_transaction = gda_oracle_provider_rollback_transaction;
	
	provider_class->create_blob = NULL;
	provider_class->fetch_blob = NULL;
}

static void
gda_oracle_provider_init (GdaOracleProvider *myprv, GdaOracleProviderClass *klass)
{
}

static void
gda_oracle_provider_finalize (GObject *object)
{
	GdaOracleProvider *myprv = (GdaOracleProvider *) object;

	g_return_if_fail (GDA_IS_ORACLE_PROVIDER (myprv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_oracle_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaOracleProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_oracle_provider_class_init,
			NULL, NULL,
			sizeof (GdaOracleProvider),
			0,
			(GInstanceInitFunc) gda_oracle_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaOracleProvider", &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_oracle_provider_new (void)
{
	GdaOracleProvider *provider;

	provider = g_object_new (gda_oracle_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

/* get_version handler for the GdaOracleProvider class */
static const gchar *
gda_oracle_provider_get_version (GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/* open_connection handler for the GdaOracleProvider class */
static gboolean
gda_oracle_provider_open_connection (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaQuarkList *params,
				     const gchar *username,
				     const gchar *password)
{
        const gchar *tnsname;
	gint  result;

        GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;
	GdaOracleConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* check we have a TNS name to connect to */

	if ((tnsname = gda_quark_list_find (params, "TNSNAME")) == NULL) {
		gda_connection_add_event_string (cnc, 
						 _("No TNS name supplied"));
		return FALSE;
	}

	priv_data = g_new0 (GdaOracleConnectionData, 1);

        g_assert (priv_data != NULL);
        /* initialize Oracle */
	result = OCIInitialize ((ub4) OCI_THREADED,
				(dvoid *) 0,
				(dvoid * (*)(dvoid *, size_t)) 0,
				(dvoid * (*)(dvoid *, dvoid *, size_t)) 0,
				(void (*)(dvoid *, dvoid *)) 0);

	if (result != OCI_SUCCESS) {
		gda_connection_add_event_string (cnc, 
						 _("Could not initialize Oracle"));
		return FALSE;
	}
	
	/* initialize the Oracle environment */
	result = OCIEnvInit ((OCIEnv **) & priv_data->henv, 
			     (ub4) OCI_DEFAULT, 
			     (size_t) 0, 
			     (dvoid **) 0);
	if (result != OCI_SUCCESS) {
		gda_connection_add_event_string (cnc, 
						 _("Could not initialize the Oracle environment"));
		return FALSE;
	}

	/* create the service context */
	result = OCIHandleAlloc ((dvoid *) priv_data->henv,
				 (dvoid **) &priv_data->hservice,
				 (ub4) OCI_HTYPE_SVCCTX,
				 (size_t) 0,
				 (dvoid **) 0);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ENV,
				      _("Could not allocate the Oracle service handle")))
		return FALSE;

	/* create the error handle */
	result = OCIHandleAlloc ((dvoid *) priv_data->henv, 
				 (dvoid **) &(priv_data->herr), 
				 (ub4) OCI_HTYPE_ERROR, 
				 (size_t) 0, 
				 (dvoid **) 0);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ENV,
				      _("Could not allocate the Oracle error handle"))) {
		OCIHandleFree ((dvoid *) priv_data->hservice, OCI_HTYPE_SVCCTX);
		return FALSE;
	}
			
	/* we use the Multiple Sessions/Connections OCI paradigm for this server */
	result = OCIHandleAlloc ((dvoid *) priv_data->henv,
				 (dvoid **) & priv_data->hserver,
				 (ub4) OCI_HTYPE_SERVER, 
				 (size_t) 0,
				 (dvoid **) 0);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ENV,
				      _("Could not allocate the Oracle server handle"))) {
		OCIHandleFree ((dvoid *) priv_data->herr, OCI_HTYPE_ERROR);
		OCIHandleFree ((dvoid *) priv_data->hservice, OCI_HTYPE_SVCCTX);
		return FALSE;
	}

	/* create the session handle */
	result = OCIHandleAlloc ((dvoid *) priv_data->henv,
				 (dvoid **) &priv_data->hsession,
				 (ub4) OCI_HTYPE_SESSION,
				 (size_t) 0,
				 (dvoid **) 0);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ENV,
				      _("Could not allocate the Oracle session handle"))) {
		OCIHandleFree ((dvoid *) priv_data->hserver, OCI_HTYPE_SERVER);
		OCIHandleFree ((dvoid *) priv_data->herr, OCI_HTYPE_ERROR);
		OCIHandleFree ((dvoid *) priv_data->hservice, OCI_HTYPE_SVCCTX);
		return FALSE;
	}

	/* if the username isn't provided, try to find it in the DSN */

	if (username == NULL || *username == '\0') {
		username = gda_quark_list_find (params, "USER");
		if (username == NULL)
			username="";
	}

	/* if the password isn't provided, try to find it in the DSN */
	if (password == NULL || *password == '\0') {
		password = gda_quark_list_find (params, "PASSWORD");
		if (password == NULL)
			password="";
	}


        /* attach to Oracle server */
	result = OCIServerAttach (priv_data->hserver,
				  priv_data->herr,
				  (text *) tnsname,
				  (ub4) strlen (tnsname),
				  OCI_DEFAULT);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not attach to the Oracle server"))) {
		OCIHandleFree ((dvoid *) priv_data->hsession, OCI_HTYPE_SESSION);
		OCIHandleFree ((dvoid *) priv_data->hserver, OCI_HTYPE_SERVER);
		OCIHandleFree ((dvoid *) priv_data->herr, OCI_HTYPE_ERROR);
		OCIHandleFree ((dvoid *) priv_data->hservice, OCI_HTYPE_SVCCTX);
		return FALSE;
	}

	/* set the server attribute in the service context */
	result = OCIAttrSet ((dvoid *) priv_data->hservice, 
			     (ub4) OCI_HTYPE_SVCCTX,
			     (dvoid *) priv_data->hserver, 
			     (ub4) 0,
			     (ub4) OCI_ATTR_SERVER, 
			     priv_data->herr);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not set the Oracle server attribute in the service context"))) {
		OCIHandleFree ((dvoid *) priv_data->hsession, OCI_HTYPE_SESSION);
		OCIHandleFree ((dvoid *) priv_data->hserver, OCI_HTYPE_SERVER);
		OCIHandleFree ((dvoid *) priv_data->herr, OCI_HTYPE_ERROR);
		OCIHandleFree ((dvoid *) priv_data->hservice, OCI_HTYPE_SVCCTX);
		return FALSE;
	}
	
	/* set the username attribute */
	result = OCIAttrSet ((dvoid *) priv_data->hsession, 
			     (ub4) OCI_HTYPE_SESSION, 
			     (dvoid *) username,
			     (ub4) strlen (username),
			     (ub4) OCI_ATTR_USERNAME,
			     priv_data->herr);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not set the Oracle username attribute"))) {
		OCIHandleFree ((dvoid *) priv_data->hsession, OCI_HTYPE_SESSION);
		OCIHandleFree ((dvoid *) priv_data->hserver, OCI_HTYPE_SERVER);
		OCIHandleFree ((dvoid *) priv_data->herr, OCI_HTYPE_ERROR);
		OCIHandleFree ((dvoid *) priv_data->hservice, OCI_HTYPE_SVCCTX);
		return FALSE;
	}

	/* set the password attribute */
	result = OCIAttrSet ((dvoid *) priv_data->hsession, 
			     (ub4) OCI_HTYPE_SESSION, 
			     (dvoid *) password,
			     (ub4) strlen (password), 
			     (ub4) OCI_ATTR_PASSWORD,
			     priv_data->herr);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not set the Oracle password attribute"))) {
		OCIHandleFree ((dvoid *) priv_data->hsession, OCI_HTYPE_SESSION);
		OCIHandleFree ((dvoid *) priv_data->hserver, OCI_HTYPE_SERVER);
		OCIHandleFree ((dvoid *) priv_data->herr, OCI_HTYPE_ERROR);
		OCIHandleFree ((dvoid *) priv_data->hservice, OCI_HTYPE_SVCCTX);
		return FALSE;
	}

	/* begin the session */
	result = OCISessionBegin (priv_data->hservice,
				  priv_data->herr,
				  priv_data->hsession,
				  (ub4) OCI_CRED_RDBMS,
				  (ub4) OCI_DEFAULT);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not begin the Oracle session"))) {
		OCIServerDetach (priv_data->hserver, priv_data->herr, OCI_DEFAULT);
		OCIHandleFree ((dvoid *) priv_data->hsession, OCI_HTYPE_SESSION);
		OCIHandleFree ((dvoid *) priv_data->hserver, OCI_HTYPE_SERVER);
		OCIHandleFree ((dvoid *) priv_data->herr, OCI_HTYPE_ERROR);
		OCIHandleFree ((dvoid *) priv_data->hservice, OCI_HTYPE_SVCCTX);
		priv_data->hsession = NULL;
		return FALSE;
	}

	/* set the session attribute in the service context */
	result = OCIAttrSet ((dvoid *) priv_data->hservice,
			     (ub4) OCI_HTYPE_SVCCTX,
			     (dvoid *) priv_data->hsession,
			     (ub4) 0, 
			     (ub4) OCI_ATTR_SESSION,
			     priv_data->herr);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not set the Oracle session attribute in the service context"))) {
		OCIServerDetach (priv_data->hserver, priv_data->herr, OCI_DEFAULT);
		OCIHandleFree ((dvoid *) priv_data->hsession, OCI_HTYPE_SESSION);
		OCIHandleFree ((dvoid *) priv_data->hserver, OCI_HTYPE_SERVER);
		OCIHandleFree ((dvoid *) priv_data->herr, OCI_HTYPE_ERROR);
		OCIHandleFree ((dvoid *) priv_data->hservice, OCI_HTYPE_SVCCTX);
		return FALSE;
	}

	priv_data->schema = g_ascii_strup(username, -1);
	priv_data->tables = NULL;
	priv_data->views = NULL;
	priv_data->aggregates = NULL;
	priv_data->procedures = NULL;
	
	/* attach the oracle connection data to the gda connection object */
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE, priv_data);

	return TRUE;
}

/* close_connection handler for the GdaOracleProvider class */
static gboolean
gda_oracle_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaOracleConnectionData *priv_data;
	GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;
	gint result;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data)
		return FALSE;

	/* end the session */
	if ((result =
	     OCISessionEnd (priv_data->hservice,
			    priv_data->herr,
			    priv_data->hsession,
			    OCI_DEFAULT))) {
		gda_connection_add_event (cnc, 
					  gda_oracle_make_error (priv_data->herr, OCI_HTYPE_ERROR, __FILE__, __LINE__));
		return FALSE;
	}

	/* free all of the handles */
        if (priv_data->hsession)
		OCIHandleFree ((dvoid *) priv_data->hsession, OCI_HTYPE_SESSION);
	if (priv_data->hservice)
		OCIHandleFree ((dvoid *) priv_data->hservice, OCI_HTYPE_SVCCTX);
	if (priv_data->hserver)
		OCIHandleFree ((dvoid *) priv_data->hserver, OCI_HTYPE_SERVER);
	if (priv_data->herr)
		OCIHandleFree ((dvoid *) priv_data->herr, OCI_HTYPE_ERROR);
	if (priv_data->henv)
		OCIHandleFree ((dvoid *) priv_data->henv, OCI_HTYPE_ENV);
	if (priv_data->schema)
		g_free(priv_data->schema);
	if (priv_data->tables)
		g_tree_destroy(priv_data->tables);
	if (priv_data->views)
		g_tree_destroy(priv_data->views);
	if (priv_data->aggregates)
		g_tree_destroy(priv_data->aggregates);
	if (priv_data->procedures)
		g_tree_destroy(priv_data->procedures);

	g_free (priv_data);
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE, NULL);

	return TRUE;
}

/* get_server_version handler for the GdaOracleProvider class */
static const gchar *
gda_oracle_provider_get_server_version (GdaServerProvider *provider,
					GdaConnection *cnc)
{
	GdaOracleConnectionData *priv_data;
	static gchar version[512];

	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

	/* Get the OracleConnectionData */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
		return NULL;
	}

	OCIServerVersion (priv_data->hservice, priv_data->herr, version, 255, OCI_HTYPE_SVCCTX);

	return (const gchar *) version;
}

/* 
 * prepare_oracle_statement prepares the Oracle statement handle for use 
 *
 * Its parameters are the connection object, a list of parameters to be bound,
 * and the SQL statement to be prepared.
 *
 * It's generally good style to name your parameters beginning with a colon (:)
 * i.e. :parameter_name.  This will avoid confusion.
 *
 * IMPORTANT: If you pass in a parameter in this list that isn't in the statement
 * it will still try to perform a bind anyway.
 */
static OCIStmt *
prepare_oracle_statement (GdaConnection *cnc, GdaParameterList *params, gchar *sql)
{
	GdaOracleConnectionData *priv_data;
	OCIStmt *stmthp = (OCIStmt *) 0;
	GSList *l;
	gint result;

	/* Get the OracleConnectionData */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
		return NULL;
	}

	/* Allocate an oracle statement handle */
	result = OCIHandleAlloc ((dvoid *) priv_data->henv,
				 (dvoid **) &stmthp,
				 (ub4) OCI_HTYPE_STMT,
				 (size_t) 0,
				 (dvoid **) 0);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ENV,
				      _("Could not allocate the Oracle statement handle")))
		return NULL;
	
	/* Prepare the statement */
	result = OCIStmtPrepare ((dvoid *) stmthp,
				 (dvoid *) priv_data->herr,
				 (text *) sql,
				 (ub4) strlen(sql),
				 (ub4) OCI_NTV_SYNTAX,
				 (ub4) OCI_DEFAULT);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not prepare the Oracle statement"))) {
		OCIHandleFree ((dvoid *) stmthp, OCI_HTYPE_STMT);
		return NULL;
	}

	/* Bind parameters */
	if (params != NULL) {
		/* loop through parameters and bind by name */
		GSList *parm_list = params->parameters;

		for (l = parm_list; l != NULL; l = l->next) {
			const gchar *parm_name = gda_object_get_name (GDA_OBJECT (l->data));
			const GValue *gda_value = gda_parameter_get_value (GDA_PARAMETER (l->data));
			GdaOracleValue *ora_value = gda_value_to_oracle_value (gda_value);
			OCIBind *bindpp = (OCIBind *) 0;

			result = OCIBindByName ((dvoid *) stmthp,
						(OCIBind **) &bindpp,
						(OCIError *) priv_data->herr,
						(text *) parm_name,
						(sb4) strlen (parm_name),
						(dvoid *) ora_value->value,
						(sb4) ora_value->defined_size,
						(ub2) ora_value->sql_type,
						(dvoid *) &ora_value->indicator,
						(ub2 *) 0,
						(ub2) 0,
						(ub4) 0,
						(ub4 *) 0,
						(ub4) OCI_DEFAULT);
			if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
						      _("Could not bind the Oracle statement parameter"))) {
				OCIHandleFree ((dvoid *) stmthp, OCI_HTYPE_STMT);
				return NULL;
			}
		}
	}
	return stmthp;
}

static GList *
process_sql_commands (GList *reclist, GdaConnection *cnc, 
		      const gchar *sql, GdaParameterList *params, 
		      GdaCommandOptions options)
{
	GdaOracleConnectionData *priv_data;
	gchar **arr;
	gint result;
	ub4 prefetch = 200;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
		return NULL;
	}

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = gda_delimiter_split_sql (sql);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			GdaDataModel *recset;
			GList *columns = NULL;
			ub4 ncolumns;
			ub4 i;

			/* prepare the statement for execution */
			OCIStmt *stmthp = prepare_oracle_statement (cnc, params, arr[n]);

			result = OCIAttrGet (stmthp,
					     OCI_HTYPE_STMT,
					     (dvoid *) &priv_data->stmt_type,
					     NULL,
					     OCI_ATTR_STMT_TYPE,
					     priv_data->herr);
			if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
						      _("Could not get the Oracle statement type"))) {
				OCIHandleFree ((dvoid *) stmthp, OCI_HTYPE_STMT);
				return NULL;
			}

			result = OCIAttrSet (stmthp,
					     OCI_HTYPE_STMT,
					     &prefetch,
					     0,
					     OCI_ATTR_PREFETCH_ROWS,
					     priv_data->herr);
			if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
						      _("Could not set the Oracle statement pre-fetch row count"))) {
				OCIHandleFree ((dvoid *) stmthp, OCI_HTYPE_STMT);
				return NULL;
			}

			result = OCIStmtExecute (priv_data->hservice,
						 stmthp,
						 priv_data->herr,
						 (ub4) ((OCI_STMT_SELECT == priv_data->stmt_type) ? 0 : 1),
						 (ub4) 0,
						 (CONST OCISnapshot *) NULL,
						 (OCISnapshot *) NULL,
						 OCI_DEFAULT);
			if (result == OCI_NO_DATA) 
				continue;

			if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
						      _("Could not execute the Oracle statement"))) {
				OCIHandleFree ((dvoid *) stmthp, OCI_HTYPE_STMT);
				return NULL;
			}

			/* get the number of columns in the result set */
			result = OCIAttrGet ((dvoid *) stmthp,
					     (ub4) OCI_HTYPE_STMT,
					     (dvoid *) &ncolumns,
					     (ub4 *) 0,
					     (ub4) OCI_ATTR_PARAM_COUNT,
					     priv_data->herr);
			if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
						      _("Could not get the number of columns in the result set"))) {
				OCIHandleFree ((dvoid *) stmthp, OCI_HTYPE_STMT);
				return NULL;
			}

			for (i = 0; i < ncolumns; i += 1) {
				text *pgchar_dummy = (text *) 0;
				ub4 col_name_len;
				OCIParam *pard = (OCIParam *) 0;
				gchar *name_buffer;

				result = OCIParamGet ((dvoid *) stmthp,
						      OCI_HTYPE_STMT,
						      priv_data->herr,
						      (dvoid **) &pard,
						      (ub4) i + 1);
				if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
							      _("Could not get the parameter descripter in the result set"))) {
					OCIHandleFree ((dvoid *) stmthp, OCI_HTYPE_STMT);
					return NULL;
				}

				result = OCIAttrGet ((dvoid *) pard,
						     (ub4) OCI_DTYPE_PARAM,
						     (dvoid **) &pgchar_dummy,
						     (ub4 *) &col_name_len,
						     (ub4) OCI_ATTR_NAME,
						     (OCIError *) priv_data->herr);
				if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
							      _("Could not get the column name in the result set"))) {
					OCIHandleFree ((dvoid *) stmthp, OCI_HTYPE_STMT);
					return NULL;
				}

				name_buffer = g_malloc0 (col_name_len + 1);
				memcpy (name_buffer, pgchar_dummy, col_name_len);
				name_buffer[col_name_len] = '\0';
				columns = g_list_append (columns, name_buffer);
			}

			recset = GDA_DATA_MODEL (gda_oracle_recordset_new (cnc, priv_data, stmthp));
			stmthp = (OCIStmt *) 0;

			if (GDA_IS_ORACLE_RECORDSET (recset)) {
				GList *l;
				g_object_set (G_OBJECT (recset), 
					      "command_text", arr[n],
					      "command_type", GDA_COMMAND_TYPE_SQL, NULL);

				i = 0;
				for (l = g_list_first (columns); l != NULL; l = g_list_next (l)) {
					gchar *col_name = (gchar *) l->data;

					gda_data_model_set_column_title (recset, i, col_name);
					i += 1;
				}

				reclist = g_list_append (reclist, recset);
			}
			g_list_free (columns);
			n++;
		}

		g_strfreev (arr);
	}

	return reclist;
}

/* get_database handler for the GdaOracleProvider class */
static const gchar *
gda_oracle_provider_get_database (GdaServerProvider *provider,
				  GdaConnection *cnc)
{
	GdaOracleConnectionData *priv_data;
	GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	/* don't know what to do here yet. */
	return NULL;
}

/* execute_command handler for the GdaMysqlProvider class */
static GList *
gda_oracle_provider_execute_command (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaCommand *cmd,
				     GdaParameterList *params)
{
	GList *reclist = NULL;
	gchar *str;
	GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;
	GdaOracleConnectionData *priv_data;
	gint result;

	GdaCommandOptions options;
	GdaTransaction *xaction;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	options = gda_command_get_options (cmd);
	xaction = gda_command_get_transaction (cmd);

	if (xaction != NULL) {
		GdaOracleTransaction *ora_xaction = g_object_get_data (G_OBJECT (xaction), OBJECT_DATA_ORACLE_HANDLE);
		/* attach a transaction */
		result = OCIAttrSet ((dvoid *) priv_data->hservice,
				     OCI_HTYPE_SVCCTX,
				     (dvoid *) ora_xaction->txnhp,
				     0,
				     OCI_ATTR_TRANS,
				     priv_data->herr);
		if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
					      _("Could not attach the transaction to the service context")))
			return NULL;
	}

	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL:
		reclist = process_sql_commands (reclist, cnc, gda_command_get_text (cmd), NULL, options);
		break;
	case GDA_COMMAND_TYPE_TABLE:
		str = g_strdup_printf ("SELECT * FROM %s", gda_command_get_text (cmd));
		reclist = process_sql_commands (reclist, cnc, str, NULL, options);
		g_free (str);
		break;
	default:
		break;
	}
	/* don't know what to do here yet. */
	return reclist;
}

/* begin_transaction handler for the GdaOracleProvider class */
static gboolean
gda_oracle_provider_begin_transaction (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaTransaction *xaction)
{
	GdaOracleConnectionData *priv_data;
	GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;
	GdaOracleTransaction *ora_xaction;
	const gchar *xaction_name;
	gint result;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (xaction != NULL, FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	ora_xaction = g_new0 (GdaOracleTransaction, 1);

	/* allocate the Oracle transaction handle */
	result = OCIHandleAlloc ((dvoid *) priv_data->henv,
				 (dvoid **) &(ora_xaction->txnhp),
				 (ub4) OCI_HTYPE_TRANS,
				 (size_t) 0,
				 (dvoid **) 0);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ENV,
				      _("Could not allocate the Oracle transaction handle")))
		return FALSE;

	/* set the transaction name if applicable */
 	xaction_name = gda_transaction_get_name (xaction);
	if (xaction_name) {
		result = OCIAttrSet ((dvoid *) ora_xaction->txnhp,
				     OCI_HTYPE_TRANS,
				     (text *) xaction_name,
				     strlen (xaction_name),
				     OCI_ATTR_TRANS_NAME,
				     priv_data->herr);
		if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
					      _("Could not set the Oracle transaction name"))) {
			OCIHandleFree ((dvoid *) ora_xaction->txnhp, OCI_HTYPE_TRANS);
			return FALSE;
		}
	}
	

	/* attach the transaction to the service context */
	result = OCIAttrSet ((dvoid *) priv_data->hservice,
			     OCI_HTYPE_SVCCTX,
			     (dvoid *) ora_xaction->txnhp,
			     0,
			     OCI_ATTR_TRANS,
			     priv_data->herr);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not attach the transaction to the service context"))) {
		OCIHandleFree ((dvoid *) ora_xaction->txnhp, OCI_HTYPE_TRANS);
		return FALSE;
	}

	/* start the transaction */
	result = OCITransStart (priv_data->hservice, 
				priv_data->herr, 
				60, 
				OCI_TRANS_NEW);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not start the Oracle transaction"))) {
		OCIHandleFree ((dvoid *) ora_xaction->txnhp, OCI_HTYPE_TRANS);
		return FALSE; 
	}

	g_object_set_data (G_OBJECT (xaction), OBJECT_DATA_ORACLE_HANDLE, ora_xaction);
	return TRUE;
}

/* commit_transaction handler for the GdaOracleProvider class */
static gboolean
gda_oracle_provider_commit_transaction (GdaServerProvider *provider,
					GdaConnection *cnc,
					GdaTransaction *xaction)
{
	GdaOracleConnectionData *priv_data;
	GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;
	GdaOracleTransaction *ora_xaction;
	gint result;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (xaction != NULL, FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	ora_xaction = g_object_get_data (G_OBJECT (xaction), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data || !ora_xaction) {
		gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	/* attach the transaction to the service context */
	result = OCIAttrSet ((dvoid *) priv_data->hservice,
			     OCI_HTYPE_SVCCTX,
			     (dvoid *) ora_xaction->txnhp,
			     0,
			     OCI_ATTR_TRANS,
			     priv_data->herr);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not attach the transaction to the service context")))
		return FALSE;
	

	/* prepare to commit.  This may return OCI_SUCCESS_WITH_INFO.?? */
	result = OCITransPrepare (priv_data->hservice,
				  priv_data->herr,
				  (ub4) 0);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not prepare the transaction to commit")))
		return FALSE;

	/* commit */
	result = OCITransCommit (priv_data->hservice, 
				 priv_data->herr, 
				 OCI_DEFAULT);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not commit the Oracle transaction")))
		return FALSE;

	OCIHandleFree ((dvoid *) ora_xaction->txnhp, OCI_HTYPE_TRANS);
	return TRUE;

}

/* rollback_transaction handler for the GdaMysqlProvider class */
static gboolean
gda_oracle_provider_rollback_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GdaTransaction *xaction)
{
	GdaOracleConnectionData *priv_data;
	GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;
	GdaOracleTransaction *ora_xaction;
	gint result;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (xaction != NULL, FALSE);


	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	ora_xaction = g_object_get_data (G_OBJECT (xaction), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data || !ora_xaction) {
		gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	/* attach the transaction to the service context */
	result = OCIAttrSet ((dvoid *) priv_data->hservice,
			     OCI_HTYPE_SVCCTX,
			     (dvoid *) ora_xaction->txnhp,
			     0,
			     OCI_ATTR_TRANS,
			     priv_data->herr);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not attach the transaction to the service context")))
		return FALSE;

	result = OCITransRollback (priv_data->hservice, 
				   priv_data->herr, 
				   OCI_DEFAULT);
	if (!gda_oracle_check_result (result, cnc, priv_data, OCI_HTYPE_ERROR,
				      _("Could not rollback the Oracle transaction")))
		return FALSE;

	OCIHandleFree ((dvoid *) ora_xaction->txnhp, OCI_HTYPE_TRANS);
	return TRUE;
}

/* supports handler for the GdaOracleProvider class */
static gboolean
gda_oracle_provider_supports (GdaServerProvider *provider,
			      GdaConnection *cnc,
			      GdaConnectionFeature feature)
{
	GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);

	switch (feature) {
        case GDA_CONNECTION_FEATURE_AGGREGATES :
	case GDA_CONNECTION_FEATURE_INDEXES :
	case GDA_CONNECTION_FEATURE_PROCEDURES :
	case GDA_CONNECTION_FEATURE_SEQUENCES :
	case GDA_CONNECTION_FEATURE_SQL :
	case GDA_CONNECTION_FEATURE_TRANSACTIONS :
	case GDA_CONNECTION_FEATURE_TRIGGERS :
	case GDA_CONNECTION_FEATURE_USERS :
	case GDA_CONNECTION_FEATURE_VIEWS :
		return TRUE;
	default :
		break;
	}

	return FALSE;
}

static GdaServerProviderInfo *
gda_oracle_provider_get_info (GdaServerProvider *provider, GdaConnection *cnc)
{
	static GdaServerProviderInfo info = {
		"Oracle",
		TRUE, 
		TRUE,
		FALSE,
	};
	
	return &info;
}

typedef struct
{
	const gchar *name;
	const gchar *uid;
	const gchar *args;
} gda_oracle_aggregate_t;

static gda_oracle_aggregate_t aggregate_tab[] =
{
	{ "AVG",             "SYS_AGG01", "NUMBER"        },
	{ "CORR",            "SYS_AGG02", "NUMBER NUMBER" },
	{ "COUNT",           "SYS_AGG03", "-"             },
	{ "COVAR_POP",       "SYS_AGG04", "NUMBER NUMBER" },
	{ "COVAR_SAMP",      "SYS_AGG05", "NUMBER NUMBER" },
	{ "CUME_DIST",       "SYS_AGG06", "NUMBER"        },
	{ "DENSE_RANK",      "SYS_AGG07", "NUMBER"        },
	{ "GROUP_ID",        "SYS_AGG08", ""              },
	{ "GROUPING",        "SYS_AGG09", "-"             },
	{ "GROUPING_ID",     "SYS_AGG10", "-"             },
	{ "MAX",             "SYS_AGG11", "NUMBER"        },
	{ "MIN",             "SYS_AGG12", "NUMBER"        },
	{ "PERCENTILE_CONT", "SYS_AGG13", "NUMBER"        },
	{ "PERCENTILE_DISC", "SYS_AGG14", "NUMBER"        },
	{ "PERCENT_RANK",    "SYS_AGG15", "NUMBER"        },
	{ "RANK",            "SYS_AGG16", "NUMBER"        },
	{ "REGR_SLOPE",      "SYS_AGG17", "NUMBER NUMBER" },
	{ "REGR_INTERCEPT",  "SYS_AGG18", "NUMBER NUMBER" },
	{ "REGR_COUNT",      "SYS_AGG19", "NUMBER NUMBER" },
	{ "REGR_R2",         "SYS_AGG20", "NUMBER NUMBER" },
	{ "REGR_AVGX",       "SYS_AGG21", "NUMBER NUMBER" },
	{ "REGR_AVGY",       "SYS_AGG22", "NUMBER NUMBER" },
	{ "REGR_SXX",        "SYS_AGG23", "NUMBER NUMBER" },
	{ "REGR_SYY",        "SYS_AGG24", "NUMBER NUMBER" },
	{ "REGR_SXY",        "SYS_AGG25", "NUMBER NUMBER" },
	{ "STDDEV",          "SYS_AGG26", "NUMBER"        },
	{ "STDDEV_POP",      "SYS_AGG27", "NUMBER"        },
	{ "STDDEV_SAMP",     "SYS_AGG28", "NUMBER"        },
	{ "SUM",             "SYS_AGG29", "NUMBER"        },
	{ "VAR_POP",         "SYS_AGG30", "NUMBER"        },
	{ "VAR_SAMP",        "SYS_AGG31", "NUMBER"        },
	{ "VARIANCE",        "SYS_AGG32", "NUMBER"        }
};
static const gda_oracle_aggregate_t
*aggregate_end = aggregate_tab + (sizeof(aggregate_tab)/sizeof(gda_oracle_aggregate_t));

static GTree *
build_aggregate_tree (void)
{
	GTree *tree;
	const gda_oracle_aggregate_t *ptr;

	tree = g_tree_new((GCompareFunc)strcmp);
	for (ptr = aggregate_tab; ptr < aggregate_end; ptr++)
		g_tree_insert(tree, (gpointer)ptr->name, (gpointer)ptr);
	return tree;
}

static gboolean
aggregate_foreach (gpointer key, gpointer value, gpointer data)
{
	gda_oracle_aggregate_t *ptr = value;
	GList *list = NULL;
	GList *listp;
	GValue *tmpval;

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), ptr->name);
	list = g_list_append(list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), ptr->uid);
	list = g_list_append(list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "SYS");
	list = g_list_append(list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
	list = g_list_append(list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "NUMBER");
	list = g_list_append(list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), ptr->args);
	list = g_list_append(list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
	list = g_list_append(list, tmpval);

	gda_data_model_append_values (GDA_DATA_MODEL(data), list, NULL);
	for (listp = list; listp; listp = listp->next)
		gda_value_free(listp->data);
	g_list_free(list);
	return FALSE;
}

static GdaDataModel *
get_oracle_aggregates (GdaConnection *cnc, GdaParameterList *params)
{
	GdaOracleConnectionData *priv_data;
	GdaDataModelArray *recset;
	GdaParameter *par = NULL;
	gchar *name, *uc_name;
	gda_oracle_aggregate_t *row;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
		return NULL;
	}

	/* if necessary build the GTree of aggregates. */
	if (priv_data->aggregates == NULL)
		priv_data->aggregates = build_aggregate_tree();
	
	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_AGGREGATES)));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_AGGREGATES);

	if (params)
		par = gda_parameter_list_find_param (params, "name");
	if (par) {
		name = (gchar *) g_value_get_string ((GValue *) gda_parameter_get_value (par));
		uc_name = g_ascii_strup (name, -1);
		if ((row = g_tree_lookup (priv_data->aggregates, uc_name)))
			aggregate_foreach (uc_name, row, recset);
		g_free (uc_name);
		g_free (name);
	} 
	else
		g_tree_foreach (priv_data->aggregates, aggregate_foreach, recset);

	return GDA_DATA_MODEL (recset);
}

static GTree *
gda_oracle_table_tree (GdaConnection *cnc)
{
	GdaOracleConnectionData *priv_data;
	GTree *tree;
	GList *reclist;
	GdaDataModel *recset;
	GdaRow *row;
	GValue *value;
	int i;
	gchar *name, *owner;

	reclist = process_sql_commands (NULL, cnc,
					"SELECT TABLE_NAME,OWNER "
					"FROM ALL_TABLES "
					"ORDER BY TABLE_NAME",
					NULL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (!reclist)
		return NULL;
	recset = reclist->data;
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	tree = g_tree_new((GCompareFunc)strcmp);
	for (i = 0; (row = (GdaRow *)gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (recset), i, NULL)); 
	     i++) {
		value = gda_row_get_value (row, 0);
		name = gda_value_stringify (value);
		value = gda_row_get_value (row, 1);
		owner = gda_value_stringify (value);
		if (strcmp(owner, priv_data->schema) == 0 || g_tree_lookup(tree, name) == NULL) 
			g_tree_insert(tree, name, owner);
	}
	priv_data->tables = tree;
	return tree;
}
     
static GTree *
gda_oracle_view_tree(GdaConnection *cnc)
{
	GdaOracleConnectionData *priv_data;
	GTree *tree;
	GList *reclist;
	GdaDataModel *recset;
	GdaRow *row;
	GValue *value;
	int i;
	gchar *name;

	reclist = process_sql_commands (NULL, cnc,
					"SELECT VIEW_NAME "
					"FROM USER_VIEWS "
					"ORDER BY VIEW_NAME",
					NULL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (!reclist)
		return NULL;
	recset = reclist->data;
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	tree = g_tree_new((GCompareFunc)strcmp);
	for (i = 0; (row = (GdaRow *)gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (recset), i, NULL));
	     i++) {
		value = gda_row_get_value(row, 0);
		name = gda_value_stringify (value);
		g_tree_insert(tree, name, priv_data->schema);
	}
	priv_data->views = tree;
	return tree;
}

static GString *
g_string_right_trim (GString * input)
{
	gint i;

	for (i = input->len - 1; i >= 0; i -= 1)
		if (input->str[i] != ' ')
			return g_string_truncate (input, i+1);

	return g_string_truncate (input, 0);
}

static GHashTable *
get_oracle_index_data (GdaConnection *cnc, const gchar *owner, const gchar *tblname)
{
	gchar *sql;
	GList *reclist;
	GdaDataModel *recset;
	GdaOracleIndexData *index_data;
	GdaParameter *param;
	GdaParameterList *query_params = gda_parameter_list_new (NULL);
	GString *references;
	gint nrows;
	GString *colname;
	GString *newcolname;
	GHashTable *h_table = g_hash_table_new (g_str_hash, g_str_equal);
	GValue *value;
	gint i;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	sql = g_strdup ("SELECT c1.column_name column_name, "
			"con.constraint_type constraint_type, "
			"'' uniqueness, "
			"c2.table_name reference "
			"FROM all_cons_columns c1, all_cons_columns c2, all_constraints con "
			"WHERE con.constraint_name = c1.constraint_name " 
			"AND con.constraint_type = 'R' "
			"AND con.constraint_type IS NOT NULL "
			"AND c1.table_name = upper (:TBLNAME) "
			"AND c1.owner = upper (:OWNER) "
			"AND con.r_constraint_name = c2.constraint_name "
			"UNION ALL "
			"SELECT c.column_name column_name, "
			"t.constraint_type constraint_type, "
			"substr (i.uniqueness, 1, 6) uniqueness, "
			"'' reference "
			"FROM all_indexes i, all_ind_columns c, all_constraints t "
			"WHERE i.table_name = upper (:TBLNAME) "
			"AND i.table_owner = upper (:OWNER) "
			"AND t.constraint_type != 'R' "
			"AND i.index_name = c.index_name "
			"AND i.index_name = t.constraint_name "
			"ORDER BY 1" );

	param = gda_parameter_new_string (":OWNER", owner);
	gda_parameter_list_add_param (query_params, param);
	g_object_unref (param);

	param = gda_parameter_new_string (":TBLNAME", tblname);
	gda_parameter_list_add_param (query_params, param);
	g_object_unref (param);

	reclist = process_sql_commands (NULL, cnc, sql, 
					query_params, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	nrows = gda_data_model_get_n_rows (recset);

	if (nrows == 0)
		{
			g_object_unref(recset);		
			return NULL;
		}

	index_data = g_new0 (GdaOracleIndexData, 1);
	references = g_string_new ("");

	value = (GValue *)gda_data_model_get_value_at (recset, 0, 0);
	colname = g_string_right_trim (g_string_new (g_value_get_string (value)));

	for (i = 0; i < nrows; i += 1) {
		gchar *constraint_type = "";

		value = (GValue *)gda_data_model_get_value_at (recset, 1, i);
		if (!value)
			continue;
		if (gda_value_is_null (value))
			continue;

		constraint_type = g_strdup (g_value_get_string (value));

		if (!strcmp (constraint_type, "P"))
			index_data->primary_key = TRUE;

		value = (GValue *)gda_data_model_get_value_at (recset, 2, i);
		if (value) 
			if (!gda_value_is_null (value))
				if (!strcmp (g_value_get_string (value), "UNIQUE"))  
					index_data->unique = TRUE;

		if (!strcmp (constraint_type, "R")) {
			value = (GValue *)gda_data_model_get_value_at (recset, 3, i);
			if (value)
				if (!gda_value_is_null (value)) {
					if (references->len > 0) 
						references = g_string_append (references, ", ");
					references = g_string_right_trim (g_string_append (references, g_value_get_string (value)));
				}
		}

		if (i == nrows - 1) {
			index_data->references = g_strdup (references->str);
			g_hash_table_insert (h_table, colname->str, index_data);
			break;
		}

		value = (GValue *)gda_data_model_get_value_at (recset, 0, i+1);
		newcolname = g_string_right_trim (g_string_new (g_value_get_string (value)));

		if (strcmp (colname->str, newcolname->str)) {
			index_data->references = g_strdup (references->str);
			g_hash_table_insert (h_table, colname->str, index_data);

			index_data = g_new0 (GdaOracleIndexData, 1);
			index_data->unique = FALSE;
			index_data->primary_key = FALSE;
			index_data->references = g_strdup ("");
			references = g_string_new ("");
			colname = g_string_new (newcolname->str);
		}
	}
	g_object_unref(recset);
	return h_table;
}



static GdaDataModel *
get_oracle_users (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "SELECT USERNAME FROM USER_USERS",
					NULL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	gda_data_model_set_column_title (recset, 0, _("Users"));

	return recset;
}

static GdaDataModel *
get_oracle_databases (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;
        gchar *sql = g_strdup ("SELECT TABLESPACE_NAME FROM USER_TABLESPACES");

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, sql, NULL, 
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	g_free (sql);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	/* Set it here instead of the SQL query to allow i18n */
	if (recset)
		gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_DATABASES);	

	return recset;
}

static gboolean
get_tables_foreach (gpointer key, gpointer value, gpointer data)
{
	GList *cols = NULL;
	GValue *tmpval;

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), (const gchar *)key);
	cols = g_list_append(cols, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), (const gchar *)value);
	cols = g_list_append(cols, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
	cols = g_list_append(cols, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
	cols = g_list_append(cols, tmpval);

	gda_data_model_append_values ((GdaDataModel *)data, cols, NULL);

	return FALSE;
}

static GdaDataModel *
get_oracle_tables (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *recset;
	GdaParameter *par = NULL;
	const gchar  *namespace, *upc_namespace;
	GList *reclist;
	gchar *sql;
	GdaOracleConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (params)
		par = gda_parameter_list_find_param (params, "namespace");

	if (par) {
		namespace = g_value_get_string ((GValue *) gda_parameter_get_value (par));
		upc_namespace = g_ascii_strup (namespace, -1);
	    
		sql = g_strdup_printf ("SELECT TABLE_NAME,"
				       "OWNER,"
				       "NULL,"
				       "NULL"
				       "FROM ALL_TABLES "
				       "WHERE OWNER=\"%s\" "
				       "ORDER BY TABLE_NAME",
				       upc_namespace);

		reclist = process_sql_commands (NULL, cnc, sql, NULL, 
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		g_free((gpointer)upc_namespace);
		g_free (sql);

		if (!reclist)
			return NULL;

		recset = GDA_DATA_MODEL (reclist->data);
		g_list_free (reclist);
	}
	else {
		priv_data = g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_ORACLE_HANDLE);
		if (priv_data->tables == NULL)
			if (gda_oracle_table_tree(cnc) == NULL)
				return NULL;

		recset = GDA_DATA_MODEL (gda_data_model_array_new 
					 (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TABLES)));
		
		g_tree_foreach (priv_data->tables, get_tables_foreach, recset);
	}

	/* Set it here instead of the SQL query to allow i18n */
	if (recset)
		gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_TABLES);

	return recset;
}

static GdaDataModel *
get_oracle_views (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *recset;
	GdaParameter *par = NULL;
	const gchar  *namespace, *upc_namespace;
	GList *reclist;
	gchar *sql;
	GdaOracleConnectionData *priv_data;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	
	if (params)
		par = gda_parameter_list_find_param (params, "namespace");
	if (par) {
		namespace = g_value_get_string ((GValue *) gda_parameter_get_value (par));
		upc_namespace = g_ascii_strup(namespace, -1);
		
		sql = g_strdup_printf ("SELECT VIEW_NAME AS \"%s\","
				       "OWNER AS \"%s\","
				       "NULL AS \"%s\","
				       "NULL AS \"SQL\" "
				       "FROM ALL_VIEWS "
				       "WHERE OWNER=\"%s\" "
				       "ORDER BY VIEW_NAME",
				       _("Name"), _("Owner"), _("Comments"),
				       upc_namespace);
		
		reclist = process_sql_commands (NULL, cnc, sql, NULL, 
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		g_free((gpointer)upc_namespace);
		g_free (sql);
		
		if (!reclist)
			return NULL;
		
		recset = GDA_DATA_MODEL (reclist->data);
		g_list_free (reclist);
	}
	else {
		priv_data = g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_ORACLE_HANDLE);
		if (priv_data->views == NULL)
			if (gda_oracle_view_tree(cnc) == NULL)
				return NULL;
		recset = GDA_DATA_MODEL (gda_data_model_array_new 
					 (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_VIEWS)));
		g_tree_foreach (priv_data->views, get_tables_foreach, recset);
	}

	/* Set it here instead of the SQL query to allow i18n */
	if (recset)
		gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_VIEWS);	

	return recset;
}

/*
 * get_oracle_objects
 *
 * This function gets the objects of the given schema type from the
 * database.  This is done by quering against the USER_OBJECTS
 * table which the OBJECT_TYPE parameter set to the appropriate
 * value.  This makes use of parameter binding, so it is a good
 * test to see if that is working for strings
 *
 * We effectively ignore the GdaParameterList we are given
 * because I don't want to rely on it being empty, and I can't
 * see any use for any user passed parameters at this point.
 */
static GdaDataModel *
get_oracle_objects (GdaConnection *cnc, GdaParameterList *params, GdaConnectionSchema schema, gint nargs, const gchar *title)
{
	GList *reclist;
	GdaDataModel *recset;
	gint cnt;
	GdaParameterList *query_params = gda_parameter_list_new (NULL);
	GdaParameter *param;
        GString *sql;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	sql = g_string_new ("SELECT OBJECT_NAME");
	for (cnt = 1; cnt < nargs; cnt++)
		sql = g_string_append (sql, ", NULL");

	sql = g_string_append (sql, " FROM USER_OBJECTS WHERE OBJECT_TYPE=:OBJ_TYPE");

	/* add the object type parameter */
	switch (schema) {
	case GDA_CONNECTION_SCHEMA_INDEXES:
		param = gda_parameter_new_string (":OBJ_TYPE", "INDEX");		
		gda_parameter_list_add_param (query_params, param);
		g_object_unref (param);
		break;
	case GDA_CONNECTION_SCHEMA_PROCEDURES:
		param = gda_parameter_new_string (":OBJ_TYPE", "PROCEDURE");
		gda_parameter_list_add_param (query_params, param);
		g_object_unref (param);
		break;
	case GDA_CONNECTION_SCHEMA_SEQUENCES:
		param = gda_parameter_new_string (":OBJ_TYPE", "SEQUENCE");
		gda_parameter_list_add_param (query_params, param);
		g_object_unref (param);
		break;
	case GDA_CONNECTION_SCHEMA_TRIGGERS:
		param = gda_parameter_new_string (":OBJ_TYPE", "TRIGGER");
		gda_parameter_list_add_param (query_params, param);
		g_object_unref (param);
		break;
	case GDA_CONNECTION_SCHEMA_VIEWS:
		param = gda_parameter_new_string (":OBJ_TYPE", "VIEW");
		gda_parameter_list_add_param (query_params, param);
		g_object_unref (param);
		break;
	default:
		return NULL;
	}

	reclist = process_sql_commands (NULL, cnc, sql->str, query_params,
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	g_object_unref (query_params);
	g_string_free (sql, TRUE);

	if (!reclist)
		return NULL;
	recset = GDA_DATA_MODEL (reclist->data);

	/* Set it here instead of the SQL query to allow i18n */
	if (recset)
		gda_server_provider_init_schema_model (recset, schema);

	g_list_free (reclist);
	
	return recset;
}

typedef struct
{
	const gchar  *name;
	GType  type;
	const gchar  *synonyms;
} ora_native_type;


static GdaDataModel *
get_oracle_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	ora_native_type   *otp;
	GList             *value_list;
	GValue *tmpval;

	ora_native_type ora_type_tab[] = {
			{ "BFILE",     GDA_TYPE_BINARY, NULL   },
			{ "BLOB",      GDA_TYPE_BINARY, NULL   },
			{ "CHAR",      G_TYPE_STRING, "CHARACTER"   },
			{ "NCHAR",     G_TYPE_STRING, NULL   },
			{ "CLOB",      G_TYPE_STRING, NULL   },
			{ "NCLOB",     G_TYPE_STRING, NULL   },
			{ "DATE",      GDA_TYPE_TIMESTAMP, NULL},
			{ "NUMBER",    GDA_TYPE_NUMERIC, "INTEGER,SMALLINT,INT,DEC,DECIMAL,NUMERIC,DOUBLE PRECISION,FLOAT,REAL"  },
			{ "LONG",      G_TYPE_STRING, NULL   },
			{ "LONG RAW",  GDA_TYPE_BINARY, NULL   },
			{ "RAW",       GDA_TYPE_BINARY, NULL   },
			{ "ROWID",     G_TYPE_STRING, NULL   },
			{ "UROWID",    G_TYPE_STRING, NULL   },
			{ "TIMESTAMP", GDA_TYPE_TIMESTAMP, NULL},
			{ "VARCHAR2",  G_TYPE_STRING, "VARCHAR,STRING"   },
			{ "NVARCHAR2", G_TYPE_STRING, NULL },
		};
	ora_native_type *ora_type_end = ora_type_tab+sizeof(ora_type_tab)/sizeof(ora_native_type);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TYPES)));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_TYPES);

	/* fill the recordset */
	for (otp = ora_type_tab; otp < ora_type_end; otp++) {
		value_list = NULL;

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), otp->name);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "SYS");
		value_list = g_list_append (value_list, tmpval);

		value_list = g_list_append (value_list, gda_value_new_null ());

		g_value_set_ulong (tmpval = gda_value_new (G_TYPE_ULONG), otp->type);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), otp->synonyms);
		value_list = g_list_append (value_list, tmpval);
		
		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);
		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}
	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_oracle_fields_metadata (GdaConnection *cnc, GdaParameterList *params)
{
	GdaParameter *par;
	const gchar *tblname;
	gchar *upc_tblname, *owner;
	GList *reclist;
	gchar *sql;
	GdaOracleConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params, NULL);

	/* look up the table name to get the fully qualified name. */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data)
		return FALSE;

	par = gda_parameter_list_find_param (params, "name");
	g_return_val_if_fail (par, NULL);
	
	tblname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname, NULL);
	upc_tblname = g_ascii_strup (tblname, -1);
	g_print ("tblname=#%s#, upc_tblname=#%s#\n", tblname, upc_tblname);

	owner = g_tree_lookup (priv_data->tables, upc_tblname);
	if (!owner)
		owner = g_tree_lookup (priv_data->views, upc_tblname);
	g_assert (owner);
	/*if (strcmp (owner, "HR"))
		return NULL;*/
	    
	sql = g_strdup_printf ("SELECT t.column_name, "
			       "CASE WHEN t.data_type_owner IS NOT NULL THEN t.data_type_owner||'.'||t.data_type ELSE t.data_type END \"Datatype\", "
			       "CASE WHEN t.char_used='B' THEN t.data_length ELSE NULL END \"Size\", "
			       "t.data_scale, "
			       "CASE t.nullable WHEN 'N' THEN 'TRUE' ELSE 'FALSE' END, "
			       "CASE WHEN c1.column_name IS NULL THEN 'FALSE' ELSE 'TRUE' END \"pkey\", "
			       "CASE WHEN c2.column_name IS NULL THEN 'FALSE' ELSE 'TRUE' END \"uindex\", "
			       "NULL, "
			       "t.data_default, "
			       "NULL "
			       "from all_tab_columns t "
			       "join all_col_comments c on (c.owner=t.owner and c.table_name=t.table_name and c.column_name=t.column_name) "
			       "left join (SELECT i.column_name FROM all_constraints ac join all_ind_columns i ON (i.index_name=ac.index_name AND i.table_name=ac.table_name) where ac.owner='%s' AND ac.constraint_type='P' AND ac.table_name='%s') c1 on (c1.column_name=t.column_name) "
			       "left join (SELECT i.column_name FROM all_constraints ac join all_ind_columns i ON (i.index_name=ac.index_name AND i.table_name=ac.table_name) where ac.owner='%s' AND ac.constraint_type='U' AND ac.table_name='%s') c2 on (c2.column_name=t.column_name) "
			       "where t.owner='%s' and t.table_name='%s' order by t.column_id",
			       owner, upc_tblname, owner, upc_tblname, owner, upc_tblname);

	g_free(upc_tblname);
	g_print ("SQL: #%s#\n", sql);
	reclist = process_sql_commands (NULL, cnc, sql, NULL, 
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	g_free (sql);

	if (!reclist)
		return NULL;
	else {
		GdaDataModelArray *recset;
		GdaDataModel *tmprecset;
		GError *error = NULL;

		tmprecset = GDA_DATA_MODEL (reclist->data);

		recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
					       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_FIELDS)));
		gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_FIELDS);
		if (!gda_data_model_import_from_model (GDA_DATA_MODEL (recset), tmprecset, NULL, &error)) {
			g_warning (_("Can't convert model for fields schema: %s"),
				   error && error->message ?  error->message : _("No detail"));
			g_error_free (error);
			g_object_unref (recset);
			recset = NULL;
		}

		g_object_unref (tmprecset);
		g_list_free (reclist);
		return GDA_DATA_MODEL (recset);
	}
}


/* get_schema handler for the GdaOracleProvider class */
static GdaDataModel *
gda_oracle_provider_get_schema (GdaServerProvider *provider,
				GdaConnection *cnc,
				GdaConnectionSchema schema,
				GdaParameterList *params)
{
	GdaDataModel *recset;

	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_AGGREGATES :
		recset = get_oracle_aggregates (cnc, params);
		break;
	case GDA_CONNECTION_SCHEMA_FIELDS :
		recset = get_oracle_fields_metadata (cnc, params);
		break;
	case GDA_CONNECTION_SCHEMA_TYPES :
		recset = get_oracle_types (cnc, params);
		break;
	case GDA_CONNECTION_SCHEMA_USERS :
		recset = get_oracle_users (cnc, params);
		break;
	case GDA_CONNECTION_SCHEMA_DATABASES :
		recset = get_oracle_databases (cnc, params);
		break;
	case GDA_CONNECTION_SCHEMA_INDEXES :
		recset = get_oracle_objects (cnc, params, schema, 1, _("Indexes"));
		break;
	case GDA_CONNECTION_SCHEMA_PROCEDURES :
		recset = get_oracle_objects (cnc, params, schema, 8, _("Procedures"));
		break;
	case GDA_CONNECTION_SCHEMA_SEQUENCES :
		recset = get_oracle_objects (cnc, params, schema, 4, _("Sequences"));
		break;
	case GDA_CONNECTION_SCHEMA_TABLES :
		recset = get_oracle_tables (cnc, params);
		break;
	case GDA_CONNECTION_SCHEMA_TRIGGERS :
		recset = get_oracle_objects (cnc, params, schema, 1, _("Triggers"));
		break;
	case GDA_CONNECTION_SCHEMA_VIEWS :
		recset = get_oracle_views(cnc, params);
		gda_data_model_set_column_title (recset, 0, _("Views"));
		break;
	default :
		recset = NULL;
	}
	return recset;
}

/*
  Local Variables:
  mode:C
  c-basic-offset: 8
  End:
*/
