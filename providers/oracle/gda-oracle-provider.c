/* GDA Oracle provider
 * Copyright (C) 2000-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Tim Coleman <tim@timcoleman.com>
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

#include <config.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-intl.h>
#include <stdlib.h>
#include "gda-oracle.h"
#include "gda-oracle-provider.h"
#include "gda-oracle-recordset.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_ORACLE_HANDLE "GDA_Oracle_OracleHandle"

static void gda_oracle_provider_class_init (GdaOracleProviderClass *klass);
static void gda_oracle_provider_init       (GdaOracleProvider *provider,
					   GdaOracleProviderClass *klass);
static void gda_oracle_provider_finalize   (GObject *object);

static gboolean gda_oracle_provider_open_connection (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaQuarkList *params,
						    const gchar *username,
						    const gchar *password);
static gboolean gda_oracle_provider_close_connection (GdaServerProvider *provider,
						     GdaConnection *cnc);
static const gchar *gda_oracle_provider_get_database (GdaServerProvider *provider,
						     GdaConnection *cnc);
static gboolean gda_oracle_provider_create_database (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    const gchar *name);
static gboolean gda_oracle_provider_drop_database (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  const gchar *name);
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
static GdaDataModel *gda_oracle_provider_get_schema (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaConnectionSchema schema,
						    GdaParameterList *params);

static GObjectClass *parent_class = NULL;

typedef struct {
	gchar *col_name;
	GdaValueType data_type;
} GdaOracleColData;

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
	provider_class->open_connection = gda_oracle_provider_open_connection;
	provider_class->close_connection = gda_oracle_provider_close_connection;
	provider_class->get_database = gda_oracle_provider_get_database;
	provider_class->create_database = gda_oracle_provider_create_database;
	provider_class->drop_database = gda_oracle_provider_drop_database;
	provider_class->execute_command = gda_oracle_provider_execute_command;
	provider_class->begin_transaction = gda_oracle_provider_begin_transaction;
	provider_class->commit_transaction = gda_oracle_provider_commit_transaction;
	provider_class->rollback_transaction = gda_oracle_provider_rollback_transaction;
	provider_class->supports = gda_oracle_provider_supports;
	provider_class->get_schema = gda_oracle_provider_get_schema;
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

/* open_connection handler for the GdaOracleProvider class */
static gboolean
gda_oracle_provider_open_connection (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    GdaQuarkList *params,
				    const gchar *username,
				    const gchar *password)
{
        gchar *ora_tnsname;
	gchar *ora_username;
	gchar *ora_password;

        GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;
	GdaOracleConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_new0 (GdaOracleConnectionData, 1);

        /* initialize Oracle */
	if (OCI_SUCCESS != 
		OCIInitialize ((ub4) OCI_DEFAULT,
				(dvoid *) 0,
				(dvoid * (*)(dvoid *, size_t)) 0,
				(dvoid * (*)(dvoid *, dvoid *, size_t)) 0,
				(void (*)(dvoid *, dvoid *)) 0)) {
		gda_connection_add_error_string (
			cnc, _("Could not initialize Oracle "));
		return FALSE;
	}

	/* initialize the Oracle environment */
	if (OCI_SUCCESS != 
		OCIEnvInit ((OCIEnv **) & priv_data->henv, 
				(ub4) OCI_DEFAULT, 
				(size_t) 0, 
				(dvoid **) 0)) {
		gda_connection_add_error_string (
			cnc, _("Could not initialize Oracle environment"));
		return FALSE;
	}

	/* create the service context */
	if (OCI_SUCCESS !=
		OCIHandleAlloc ((dvoid *) priv_data->henv,
				(dvoid **) &priv_data->hservice,
				(ub4) OCI_HTYPE_SVCCTX,
				(size_t) 0,
				(dvoid **) 0)) {
		gda_connection_add_error_string (
			cnc, _("Could not initialize Oracle service context"));
		return FALSE;
	}

	/* create the error handle */
	if (OCI_SUCCESS != 
		OCIHandleAlloc ((dvoid *) priv_data->henv, 
				(dvoid **) &(priv_data->herr), 
				(ub4) OCI_HTYPE_ERROR, 
				(size_t) 0, 
				(dvoid **) 0)) {
		gda_connection_add_error_string (
			cnc, _("Could not initialize Oracle error handle"));
		return FALSE;
	}
			
	/* we use the Multiple Sessions/Connections OCI paradigm for this server */
	if (OCI_SUCCESS !=
		OCIHandleAlloc ((dvoid *) priv_data->henv,
				(dvoid **) & priv_data->hserver,
				(ub4) OCI_HTYPE_SERVER, 
				(size_t) 0,
				(dvoid **) 0)) {
		gda_connection_add_error_string (
			cnc, _("Could not initialize Oracle server handle"));
		return FALSE;
	}

	/* create the session handle */
	if (OCI_SUCCESS != 
		OCIHandleAlloc ((dvoid *) priv_data->henv,
				(dvoid **) &priv_data->hsession,
				(ub4) OCI_HTYPE_SESSION,
				(size_t) 0,
				(dvoid **) 0)) {
		gda_connection_add_error_string (
			cnc, _("Could not initialize Oracle session handle"));
		return FALSE;
	}

	/* if the username isn't provided, try to find it in the DSN */
	if (strlen(username) > 0)
		ora_username = username;
	else
		ora_username = gda_quark_list_find (params, "USERNAME");

	/* if the username isn't provided, try to find it in the DSN */
	if (strlen(password) > 0)
		ora_password = password;
	else
		ora_password = gda_quark_list_find (params, "PASSWORD");


	ora_tnsname = gda_quark_list_find (params, "TNSNAME");
        g_assert (priv_data != NULL);

        /* attach to Oracle server */
	if (OCI_SUCCESS != 
		OCIServerAttach (priv_data->hserver,
				priv_data->herr,
				(text *) ora_tnsname,
				(ub4) strlen (ora_tnsname),
				OCI_DEFAULT)) {
		gda_connection_add_error_string (
			cnc, _("Could not attach to the Oracle server"));
		return FALSE;
	}

	/* set the server attribute in the service context */
	if (OCI_SUCCESS != 
		OCIAttrSet ((dvoid *) priv_data->hservice, 
				(ub4) OCI_HTYPE_SVCCTX,
				(dvoid *) priv_data->hserver, 
				(ub4) 0,
				(ub4) OCI_ATTR_SERVER, 
				priv_data->herr)) {
		gda_connection_add_error_string (
			cnc, _("Could not set the Oracle server attribute in the service context"));
		return FALSE;
	}
	
	/* set the username attribute */
	if (OCI_SUCCESS != 
		OCIAttrSet ((dvoid *) priv_data->hsession, 
				(ub4) OCI_HTYPE_SESSION, 
				(dvoid *) ora_username,
				(ub4) strlen (ora_username), 
				(ub4) OCI_ATTR_USERNAME,
				priv_data->herr)) {
		gda_connection_add_error_string (
			cnc, _("Could not set the Oracle username attribute"));
		return FALSE;
	}

	/* set the password attribute */
	if (OCI_SUCCESS != 
		OCIAttrSet ((dvoid *) priv_data->hsession, 
				(ub4) OCI_HTYPE_SESSION, 
				(dvoid *) ora_password,
				(ub4) strlen (ora_password), 
				(ub4) OCI_ATTR_PASSWORD,
				priv_data->herr)) {
		gda_connection_add_error_string (
			cnc, _("Could not set the Oracle password attribute"));
		return FALSE;
	}

	/* begin the session */
	if (OCI_SUCCESS != 
		OCISessionBegin (priv_data->hservice,
				priv_data->herr,
				priv_data->hsession,
				(ub4) OCI_CRED_RDBMS,
				(ub4) OCI_DEFAULT)) {
		OCIServerDetach (priv_data->hserver, priv_data->herr, OCI_DEFAULT);
		OCIHandleFree ((dvoid *) priv_data->hsession, OCI_HTYPE_SESSION);
		priv_data->hsession = NULL;
		
		gda_connection_add_error_string (
			cnc, _("Could not initialize the Oracle connection"));
		return FALSE;
	}

	/* set the session attribute in the service context */
	if (OCI_SUCCESS != 
		OCIAttrSet ((dvoid *) priv_data->hservice,
				(ub4) OCI_HTYPE_SVCCTX,
				(dvoid *) priv_data->hsession,
				(ub4) 0, 
				(ub4) OCI_ATTR_SESSION,
				priv_data->herr)) {
		gda_connection_add_error_string (
			cnc, _("Could not set the Oracle session attribute in the service context"));
		return FALSE;
	
	}

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

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data)
		return FALSE;

	/* end the session */
	if (OCI_SUCCESS != 
		OCISessionEnd (priv_data->hservice,
				priv_data->herr,
				priv_data->hsession,
				OCI_DEFAULT)) {
		gda_connection_add_error_string (
			cnc, _("Could not end the Oracle session"));
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
							
	g_free (priv_data);
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE, NULL);

	return TRUE;
}

/* prepare_oracle_statement prepares the Oracle statement handle for use */
static OCIStmt *
prepare_oracle_statement (GdaConnection *cnc, GdaParameterList *params, gchar *sql)
{
	GdaOracleConnectionData *priv_data;
	OCIStmt *stmthp = (OCIStmt *) 0;
	GList *l;
	gint result;

	/* Get the OracleConnectionData */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return NULL;
	}

	/* Allocate an oracle statement handle */
	if ((result =
		OCIHandleAlloc ((dvoid *) priv_data->henv,
				(dvoid **) &stmthp,
				(ub4) OCI_HTYPE_STMT,
				(size_t) 0,
				(dvoid **) 0))) {
		switch (result) {
		case OCI_SUCCESS_WITH_INFO:
			break;
		default:
			gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
			return NULL;
		}
	}
	
	/* Prepare the statement */
	if ((result = 
		OCIStmtPrepare ((dvoid *) stmthp,
				(dvoid *) priv_data->herr,
				(text *) sql,
				(ub4) strlen(sql),
				(ub4) OCI_NTV_SYNTAX,
				(ub4) OCI_DEFAULT))) {
		switch (result) {
		case OCI_SUCCESS_WITH_INFO:
			break;
		default:
			gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
			return NULL;
		}
	}

	/* Bind parameters */
	if (params != NULL) {
		/* loop through parameters and bind by name */
		GList *parm_list = gda_parameter_list_get_names (params);

		for (l = g_list_first (parm_list); l != NULL; l = l->next) {
			const gchar *parm_name = (gchar *) l->data;
			GdaParameter *parm = gda_parameter_list_find (params, parm_name);
			const GdaValue *gda_value = gda_parameter_get_value (parm);
			GdaOracleValue *ora_value = gda_value_to_oracle_value (gda_value);
			OCIBind *bindpp = (OCIBind *) 0;

			if ((result =
				OCIBindByName ((dvoid *) stmthp,
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
						(ub4) OCI_DEFAULT))) {
				switch (result) {
				case OCI_SUCCESS_WITH_INFO:
					break;
				default:
					gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
					return NULL;
				}
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

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return NULL;
	}

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			GdaDataModel *recset;
			GList *columns = NULL;
			ub4 ncolumns;
			ub4 i;

			/* prepare the statement for execution */
			OCIStmt *stmthp = prepare_oracle_statement (cnc, params, arr[n]);

			if (OCI_SUCCESS != 
				OCIAttrGet (stmthp,
						OCI_HTYPE_STMT,
						(dvoid *) &priv_data->stmt_type,
						NULL,
						OCI_ATTR_STMT_TYPE,
						priv_data->herr)) {
				gda_connection_add_error_string (
					cnc, _("Could not get the Oracle statement type"));
				return NULL;
			}
				

			if ((result = OCIStmtExecute (priv_data->hservice,
						stmthp,
						priv_data->herr,
						(ub4) ((OCI_STMT_SELECT == priv_data->stmt_type) ? 0 : 1),
						(ub4) 0,
						(CONST OCISnapshot *) NULL,
						(OCISnapshot *) NULL,
						OCI_DEFAULT))) {
				switch (result) {
				case OCI_NO_DATA:
					continue;
				default:
					gda_connection_add_error_string (
						cnc, _("Could not execute the Oracle command"));
					return NULL;
				}
			}

			/* get the number of columns in the result set */
			OCIAttrGet ((dvoid *) stmthp,
					(ub4) OCI_HTYPE_STMT,
					(dvoid *) &ncolumns,
					(ub4 *) 0,
					(ub4) OCI_ATTR_PARAM_COUNT,
					priv_data->herr);

			for (i = 0; i < ncolumns; i += 1) {
				text *pgchar_dummy = (text *) 0;
				ub4 col_name_len;
				OCIParam *pard = (OCIParam *) 0;
				gchar *name_buffer;

				if (OCI_SUCCESS != 
					OCIParamGet ((dvoid *) stmthp,
							OCI_HTYPE_STMT,
							priv_data->herr,
							(dvoid **) &pard,
							(ub4) i + 1)) {
					return NULL;
				}
				if (OCI_SUCCESS != 
					OCIAttrGet ((dvoid *) pard,
							(ub4) OCI_DTYPE_PARAM,
							(dvoid **) &pgchar_dummy,
							(ub4 *) &col_name_len,
							(ub4) OCI_ATTR_NAME,
							(OCIError *) priv_data->herr)) {
					gda_connection_add_error_string (
						cnc, _("Could not get the column name"));
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

				gda_data_model_set_command_text (recset, arr[n]);
				gda_data_model_set_command_type (recset, GDA_COMMAND_TYPE_SQL);

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
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	/* don't know what to do here yet. */
	return NULL;
}

/* create_database handler for the GdaOracleProvider class */
static gboolean
gda_oracle_provider_create_database (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    const gchar *name)
{
	GdaOracleConnectionData *priv_data;
	GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	/* don't know what to do here yet. */
	return FALSE;
}

/* drop_database handler for the GdaOracleProvider class */
static gboolean
gda_oracle_provider_drop_database (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  const gchar *name)
{
	GdaOracleConnectionData *priv_data;
	GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	/* don't know what to do here yet. */
	return FALSE;
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

	GdaCommandOptions options;
	GdaTransaction *xaction;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	options = gda_command_get_options (cmd);
	xaction = gda_command_get_transaction (cmd);

	if (xaction != NULL) {
		GdaOracleTransaction *ora_xaction = g_object_get_data (G_OBJECT (xaction), OBJECT_DATA_ORACLE_HANDLE);
		/* attach a transaction */
		if (OCI_SUCCESS !=
			OCIAttrSet ((dvoid *) priv_data->hservice,
					OCI_HTYPE_SVCCTX,
					(dvoid *) ora_xaction->txnhp,
					0,
					OCI_ATTR_TRANS,
					priv_data->herr)) {
			gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
			return FALSE;
		}
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
	gchar *xaction_name;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (xaction != NULL, FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	ora_xaction = g_new0 (GdaOracleTransaction, 1);

	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	/* allocate the Oracle transaction handle */
	if (OCI_SUCCESS !=
		OCIHandleAlloc ((dvoid *) priv_data->henv,
				(dvoid **) &(ora_xaction->txnhp),
				(ub4) OCI_HTYPE_TRANS,
				(size_t) 0,
				(dvoid **) 0)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
		return FALSE;
	}

	/* set the transaction name if applicable */
 	xaction_name = gda_transaction_get_name (xaction);
	if (OCI_SUCCESS !=
		OCIAttrSet ((dvoid *) ora_xaction->txnhp,
				OCI_HTYPE_TRANS,
				(text *) xaction_name,
				strlen (xaction_name),
				OCI_ATTR_TRANS_NAME,
				priv_data->herr)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
		return FALSE;
	}
	

	/* attach the transaction to the service context */
	if (OCI_SUCCESS !=
		OCIAttrSet ((dvoid *) priv_data->hservice,
				OCI_HTYPE_SVCCTX,
				(dvoid *) ora_xaction->txnhp,
				0,
				OCI_ATTR_TRANS,
				priv_data->herr)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
		return FALSE;
	}

	/* start the transaction */
        if (OCI_SUCCESS != 
		OCITransStart (priv_data->hservice, 
				priv_data->herr, 
				60, 
				OCI_TRANS_NEW)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
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

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (xaction != NULL, FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	ora_xaction = g_object_get_data (G_OBJECT (xaction), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	/* attach the transaction to the service context */
	if (OCI_SUCCESS !=
		OCIAttrSet ((dvoid *) priv_data->hservice,
				OCI_HTYPE_SVCCTX,
				(dvoid *) ora_xaction->txnhp,
				0,
				OCI_ATTR_TRANS,
				priv_data->herr)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
		return FALSE;
	}
	

	/* prepare to commit.  This may return OCI_SUCCESS_WITH_INFO.?? */
	if (OCI_SUCCESS != 
		OCITransPrepare (priv_data->hservice,
				priv_data->herr,
				(ub4) 0)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
		return FALSE;
	}

	/* commit */
	if (OCI_SUCCESS != 
		OCITransCommit (priv_data->hservice, 
				priv_data->herr, 
				OCI_DEFAULT)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
		return FALSE;
	}

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

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (xaction != NULL, FALSE);


	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	ora_xaction = g_object_get_data (G_OBJECT (xaction), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	/* attach the transaction to the service context */
	if (OCI_SUCCESS !=
		OCIAttrSet ((dvoid *) priv_data->hservice,
				OCI_HTYPE_SVCCTX,
				(dvoid *) ora_xaction->txnhp,
				0,
				OCI_ATTR_TRANS,
				priv_data->herr)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
		return FALSE;
	}

	if (OCI_SUCCESS != 
		OCITransRollback (priv_data->hservice, 
				priv_data->herr, 
				OCI_DEFAULT)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
		return FALSE;
	}

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
	}

	return FALSE;
}

static void
add_string_row (GdaDataModelArray *recset, const gchar *str)
{
	GdaValue *value;
	GList list;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (recset));

	value = gda_value_new_string (str);
	list.data = value;
	list.next = NULL;
	list.prev = NULL;

	gda_data_model_append_row (GDA_DATA_MODEL (recset), &list);

	gda_value_free (value);
}

static GdaDataModel *
get_oracle_aggregates (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (1);
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Name"));

	/* fill the recordset */
	add_string_row (recset, "abs");
	add_string_row (recset, "acos");
	add_string_row (recset, "ascii");
	add_string_row (recset, "asin");
	add_string_row (recset, "atan");
	add_string_row (recset, "atan2");
	add_string_row (recset, "ceil");
	add_string_row (recset, "concat");
	add_string_row (recset, "cos");
	add_string_row (recset, "cosh");
	add_string_row (recset, "count");
	add_string_row (recset, "decode");
	add_string_row (recset, "exp");
	add_string_row (recset, "floor");
	add_string_row (recset, "greatest");
	add_string_row (recset, "instr");
	add_string_row (recset, "least");
	add_string_row (recset, "length");
	add_string_row (recset, "ln");
	add_string_row (recset, "log");
	add_string_row (recset, "lower");
	add_string_row (recset, "lpad");
	add_string_row (recset, "ltrim");
	add_string_row (recset, "max");
	add_string_row (recset, "min");
	add_string_row (recset, "mod");
	add_string_row (recset, "power");
	add_string_row (recset, "replace");	
	add_string_row (recset, "reverse");
	add_string_row (recset, "round");
	add_string_row (recset, "rpad");
	add_string_row (recset, "rtrim");
	add_string_row (recset, "sign");
	add_string_row (recset, "sinh");
	add_string_row (recset, "sqrt");
	add_string_row (recset, "substr");
	add_string_row (recset, "sysdate");
	add_string_row (recset, "tanh");
	add_string_row (recset, "to_char");
	add_string_row (recset, "to_date");
	add_string_row (recset, "trim");
	add_string_row (recset, "trunc");
	add_string_row (recset, "upper");
	add_string_row (recset, "user");

	return GDA_DATA_MODEL (recset);
}

static GdaDataModelArray *
gda_oracle_init_md_recset (GdaConnection *cnc)
{
	GdaDataModelArray *recset;
	gint i;
	GdaOracleColData cols[8] = {
		{ N_("Field name")	, GDA_VALUE_TYPE_STRING },
		{ N_("Data type")	, GDA_VALUE_TYPE_STRING },
		{ N_("Size")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Scale")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Not null?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Primary key?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Unique index?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("References")	, GDA_VALUE_TYPE_STRING }
	};

	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (sizeof cols / sizeof cols[0]));
	for (i = 0; i < sizeof cols / sizeof cols[0]; i += 1)
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i, _(cols[i].col_name));
	return recset;
}

static GList *
gda_oracle_fill_md_data (const gchar *tblname, 
			GdaDataModelArray *recset,
			GdaOracleConnectionData *priv_data)
{
	OCIDescribe *dschp = (OCIDescribe *) 0;
	OCIParam *parmh;    /* parameter handle */
	OCIParam *collsthd; /* handle to list of columns */
	OCIParam *colhd;    /* column handle */
	gint i;
	ub2 numcols;
	GList *list = NULL;

	/* Allocate the Describe handle */
	if (OCI_SUCCESS != 
		OCIHandleAlloc ((dvoid *) priv_data->henv,
				(dvoid **) &dschp,
				(ub4) OCI_HTYPE_DESCRIBE,
				(size_t) 0, 
				(dvoid **) 0)) {
		return NULL;
	}

	/* Describe the table */
	if (OCI_SUCCESS != 
		OCIDescribeAny (priv_data->hservice,
				priv_data->herr,
				(text *) tblname,
				strlen (tblname),
				OCI_OTYPE_NAME,
				0,
				OCI_PTYPE_TABLE,
				(OCIDescribe *) dschp)) {
		return NULL;
	}

	/* Get the parameter handle */
	if (OCI_SUCCESS != 
		OCIAttrGet ((dvoid *) dschp,
				(ub4) OCI_HTYPE_DESCRIBE, 
				(dvoid **) &parmh,
				(ub4 *) 0,
				(ub4) OCI_ATTR_PARAM,
				(OCIError *) priv_data->herr)) {
		return NULL;
	}

	/* Get the number of columns */
	if (OCI_SUCCESS !=
		OCIAttrGet ((dvoid *) parmh,
				(ub4) OCI_DTYPE_PARAM,
				(dvoid *) &numcols,
				(ub4 *) 0,
				(ub4) OCI_ATTR_NUM_COLS,
				(OCIError *) priv_data->herr)) {
		return NULL;
	}

	if (OCI_NO_DATA ==
		OCIAttrGet ((dvoid *) parmh,
				(ub4) OCI_DTYPE_PARAM,
				(dvoid *) &collsthd,
				(ub4 *) 0,
				(ub4) OCI_ATTR_LIST_COLUMNS,
				(OCIError *) priv_data->herr)) {
		return NULL;
	}

	for (i = 1; i <= numcols; i += 1) {
		text *strp;
		ub4 sizep;
		ub2 type;
		gchar *colname;
		gchar *typename;
		ub2 nullable;
		ub2 defined_size;
		ub2 scale;

		GdaValue *value;
		GList *rowlist = NULL;

		/* Get the column handle */
		if (OCI_SUCCESS != 
			OCIParamGet ((dvoid *) collsthd,
					(ub4) OCI_DTYPE_PARAM,
					(OCIError *) priv_data->herr,
					(dvoid **) &colhd,
					(ub2) i)) {
			return NULL;
		}
		
		/* Field name */
		if (OCI_SUCCESS !=
			OCIAttrGet ((dvoid *) colhd,
					(ub4) OCI_DTYPE_PARAM,
					(dvoid *) &strp,
					(ub4 *) &sizep,
					(ub4) OCI_ATTR_NAME,
					(OCIError *) priv_data->herr)) {
			return NULL;
		}

		colname = g_malloc0 (sizep + 1);
		memcpy (colname, strp, (size_t) sizep);
		colname[sizep] = '\0';
		value = gda_value_new_string (colname);
		rowlist = g_list_append (rowlist, value);

		/* Data type */
		typename = g_malloc0 (31);
		if (OCI_SUCCESS !=
			OCIAttrGet ((dvoid *)colhd,
					(ub4) OCI_DTYPE_PARAM,
					(dvoid *) &type,
					(ub4 *) 0,
					(ub4) OCI_ATTR_DATA_TYPE,
					(OCIError *) priv_data->herr)) {
			return NULL;
		}

		typename = oracle_sqltype_to_string (type);
		value = gda_value_new_string (typename);
		rowlist = g_list_append (rowlist, value);

		/* Defined Size */
		if (OCI_SUCCESS !=
			OCIAttrGet ((dvoid *)colhd,
					(ub4) OCI_DTYPE_PARAM,
					(dvoid *) &defined_size,
					(ub4 *) 0,
					(ub4) OCI_ATTR_DATA_SIZE,
					(OCIError *) priv_data->herr)) {
			return NULL;
		}
		value = gda_value_new_integer (defined_size);
		rowlist = g_list_append (rowlist, value);

		/* Scale */
		if (OCI_SUCCESS !=
			OCIAttrGet ((dvoid *)colhd,
					(ub4) OCI_DTYPE_PARAM,
					(dvoid *) &scale,
					(ub4 *) 0,
					(ub4) OCI_ATTR_SCALE,
					(OCIError *) priv_data->herr)) {
			return NULL;
		}
		value = gda_value_new_integer (scale);
		rowlist = g_list_append (rowlist, value);

		/* Not null? */
		if (OCI_SUCCESS !=
			OCIAttrGet ((dvoid *)colhd,
					(ub4) OCI_DTYPE_PARAM,
					(dvoid *) &nullable,
					(ub4 *) 0,
					(ub4) OCI_ATTR_DATA_SIZE,
					(OCIError *) priv_data->herr)) {
			return NULL;
		}
		value = gda_value_new_boolean (nullable > 0);
		rowlist = g_list_append (rowlist, value);

		/* primary key? */
		value = gda_value_new_boolean (FALSE); // FIXME
		rowlist = g_list_append (rowlist, value);

		/* unique? */
		value = gda_value_new_boolean (FALSE); // FIXME
		rowlist = g_list_append (rowlist, value);

		/* references */
		value = gda_value_new_string ("");  // FIXME
		rowlist = g_list_append (rowlist, value);

		list = g_list_append (list, rowlist);
		rowlist = NULL;
	}

	return list;
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
get_oracle_objects (GdaConnection *cnc, GdaParameterList *params, GdaConnectionSchema schema)
{
	GList *reclist;
	GdaDataModel *recset;
	gchar *sql = "SELECT OBJECT_NAME FROM USER_OBJECTS WHERE OBJECT_TYPE=:OBJ_TYPE";
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (!params)
		params = gda_parameter_list_new ();

	/* add the object type parameter */
	switch (schema) {
	case GDA_CONNECTION_SCHEMA_INDEXES:
		gda_parameter_list_add_parameter (params, 
			gda_parameter_new_string (":OBJ_TYPE", "INDEX"));
		break;
	case GDA_CONNECTION_SCHEMA_PROCEDURES:
		gda_parameter_list_add_parameter (params, 
			gda_parameter_new_string (":OBJ_TYPE", "PROCEDURE"));
		break;
	case GDA_CONNECTION_SCHEMA_SEQUENCES:
		gda_parameter_list_add_parameter (params, 
			gda_parameter_new_string (":OBJ_TYPE", "SEQUENCE"));
		break;
	case GDA_CONNECTION_SCHEMA_TABLES:
		gda_parameter_list_add_parameter (params, 
			gda_parameter_new_string (":OBJ_TYPE", "TABLE"));
		break;
	case GDA_CONNECTION_SCHEMA_TRIGGERS:
		gda_parameter_list_add_parameter (params, 
			gda_parameter_new_string (":OBJ_TYPE", "TRIGGER"));
		break;
	case GDA_CONNECTION_SCHEMA_VIEWS:
		gda_parameter_list_add_parameter (params, 
			gda_parameter_new_string (":OBJ_TYPE", "VIEW"));
		break;
	default:
		return NULL;
	}

	reclist = process_sql_commands (NULL, cnc, sql, params,
			GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	return recset;
}

static GdaDataModel *
get_oracle_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (1);
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Type"));

	/* fill the recordset */
	add_string_row (recset, "blob");
	add_string_row (recset, "date");
	add_string_row (recset, "datetime");
	add_string_row (recset, "decimal");
	add_string_row (recset, "double");
	add_string_row (recset, "enum");
	add_string_row (recset, "float");
	add_string_row (recset, "int24");
	add_string_row (recset, "long");
	add_string_row (recset, "longlong");
	add_string_row (recset, "set");
	add_string_row (recset, "short");
	add_string_row (recset, "string");
	add_string_row (recset, "time");
	add_string_row (recset, "timestamp");
	add_string_row (recset, "tiny");
	add_string_row (recset, "year");

	return GDA_DATA_MODEL (recset);
}

static void 
add_g_list_row (gpointer data, gpointer user_data)
{
	GList *rowlist = data;
	GdaDataModelArray *recset = user_data;

	gda_data_model_append_row (GDA_DATA_MODEL (recset), rowlist);
	g_list_foreach (rowlist, (GFunc) gda_value_free, NULL);
	g_list_free (rowlist);
}

static GdaDataModel *
get_oracle_fields_metadata (GdaConnection *cnc, GdaParameterList *params)
{
	GList *list;
	GdaOracleConnectionData *priv_data;
	GdaDataModelArray *recset;
	GdaParameter *par;
	const gchar *tblname;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	par = gda_parameter_list_find (params, "name");
	g_return_val_if_fail (par != NULL, NULL);
	
	tblname = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname != NULL, NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);

	recset = gda_oracle_init_md_recset (cnc);
	list = gda_oracle_fill_md_data (tblname, recset, priv_data);
	g_list_foreach (list, add_g_list_row, recset);
	g_list_free (list);

	return GDA_DATA_MODEL (recset);
}


/* get_schema handler for the GdaOracleProvider class */
static GdaDataModel *
gda_oracle_provider_get_schema (GdaServerProvider *provider,
			       GdaConnection *cnc,
			       GdaConnectionSchema schema,
			       GdaParameterList *params)
{
	GdaDataModel *recset;

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_AGGREGATES :
		return get_oracle_aggregates (cnc, params);
	case GDA_CONNECTION_SCHEMA_FIELDS :
		return get_oracle_fields_metadata (cnc, params);
	case GDA_CONNECTION_SCHEMA_TYPES :
		return get_oracle_types (cnc, params);
	case GDA_CONNECTION_SCHEMA_USERS :
		return get_oracle_users (cnc, params);
	case GDA_CONNECTION_SCHEMA_DATABASES :
		return NULL;
	case GDA_CONNECTION_SCHEMA_INDEXES :
		recset = get_oracle_objects (cnc, params, schema);
		gda_data_model_set_column_title (recset, 0, _("Indexes"));
		return recset;
	case GDA_CONNECTION_SCHEMA_PROCEDURES :
		recset = get_oracle_objects (cnc, params, schema);
		gda_data_model_set_column_title (recset, 0, _("Procedures"));
		return recset;
	case GDA_CONNECTION_SCHEMA_SEQUENCES :
		recset = get_oracle_objects (cnc, params, schema);
		gda_data_model_set_column_title (recset, 0, _("Sequences"));
		return recset;
	case GDA_CONNECTION_SCHEMA_TABLES :
		recset = get_oracle_objects (cnc, params, schema);
		gda_data_model_set_column_title (recset, 0, _("Tables"));
		return recset;
	case GDA_CONNECTION_SCHEMA_TRIGGERS :
		recset = get_oracle_objects (cnc, params, schema);
		gda_data_model_set_column_title (recset, 0, _("Triggers"));
		return recset;
	case GDA_CONNECTION_SCHEMA_VIEWS :
		recset = get_oracle_objects (cnc, params, schema);
		gda_data_model_set_column_title (recset, 0, _("Views"));
		return recset;
	default :
	}

	return NULL;
}
