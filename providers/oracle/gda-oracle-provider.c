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

GdaValueType 
oracle_sqltype_to_gda_type (const ub2 sqltype)
{
	/* an incomplete list of all the oracle types */
	switch (sqltype) {
	case OCI_TYPECODE_REF: 			/* SQLT_REF; -- SQL/OTS OBJECT REFERENCE */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_DATE: 		/* SQLT_DAT; -- SQL DATE  OTS DATE */
		return GDA_VALUE_TYPE_DATE;
	case OCI_TYPECODE_SIGNED8: 		/* 27; -- SQL SIGNED INTEGER(8)  OTS SINT8 */
		return GDA_VALUE_TYPE_TINYINT;
	case OCI_TYPECODE_SIGNED16: 		/* 28; -- SQL SIGNED INTEGER(16)  OTS SINT16 */
		return GDA_VALUE_TYPE_SMALLINT;
	case OCI_TYPECODE_SIGNED32: 		/* 29; -- SQL SIGNED INTEGER(32)  OTS SINT32 */
		return GDA_VALUE_TYPE_INTEGER;
	case OCI_TYPECODE_REAL: 		/* 21; -- SQL REAL  OTS SQL_REAL */
		return GDA_VALUE_TYPE_SINGLE;
	case OCI_TYPECODE_DOUBLE: 		/* 22; -- SQL DOUBLE PRECISION  OTS SQL_DOUBLE */
		return GDA_VALUE_TYPE_DOUBLE;
	case OCI_TYPECODE_FLOAT: 		/* SQLT_FLT; -- SQL FLOAT(P)  OTS FLOAT(P) */
		return GDA_VALUE_TYPE_SINGLE;
	case OCI_TYPECODE_NUMBER: 		/* SQLT_NUM; -- SQL NUMBER(P S)  OTS NUMBER(P S) */
		return GDA_VALUE_TYPE_NUMERIC;
	case OCI_TYPECODE_DECIMAL: 		/* SQLT_PDN; -- SQL DECIMAL(P S)  OTS DECIMAL(P S) */
		return GDA_VALUE_TYPE_NUMERIC;
	case OCI_TYPECODE_UNSIGNED8: 		/* SQLT_BIN; -- SQL UNSIGNED INTEGER(8)  OTS UINT8 */
		return GDA_VALUE_TYPE_TINYINT;
	case OCI_TYPECODE_UNSIGNED16: 		/* 25; -- SQL UNSIGNED INTEGER(16)  OTS UINT16 */
		return GDA_VALUE_TYPE_SMALLINT;
	case OCI_TYPECODE_UNSIGNED32: 		/* 26; -- SQL UNSIGNED INTEGER(32)  OTS UINT32 */
		return GDA_VALUE_TYPE_INTEGER;
	case OCI_TYPECODE_OCTET: 		/* 245; -- SQL ???  OTS OCTET */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_SMALLINT: 		/* 246; -- SQL SMALLINT  OTS SMALLINT */
		return GDA_VALUE_TYPE_SMALLINT;
	case OCI_TYPECODE_INTEGER: 		/* SQLT_INT; -- SQL INTEGER  OTS INTEGER */
		return GDA_VALUE_TYPE_INTEGER;
	case OCI_TYPECODE_RAW: 			/* SQLT_LVB; -- SQL RAW(N)  OTS RAW(N) */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_PTR: 			/* 32; -- SQL POINTER  OTS POINTER */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_VARCHAR2: 		/* SQLT_VCS; -- SQL VARCHAR2(N)  OTS SQL_VARCHAR2(N) */
		return GDA_VALUE_TYPE_STRING;
	case OCI_TYPECODE_CHAR: 		/* SQLT_AFC; -- SQL CHAR(N)  OTS SQL_CHAR(N) */
		return GDA_VALUE_TYPE_STRING;
	case OCI_TYPECODE_VARCHAR: 		/* SQLT_CHR; -- SQL VARCHAR(N)  OTS SQL_VARCHAR(N) */
		return GDA_VALUE_TYPE_STRING;
	case OCI_TYPECODE_MLSLABEL: 		/* SQLT_LAB; -- OTS MLSLABEL */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_VARRAY: 		/* 247; -- SQL VARRAY  OTS PAGED VARRAY */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_TABLE: 		/* 248; -- SQL TABLE  OTS MULTISET */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_OBJECT: 		/* SQLT_NTY; -- SQL/OTS NAMED OBJECT TYPE */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_OPAQUE: 		/* 58; --  SQL/OTS Opaque Types */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_NAMEDCOLLECTION: 	/* SQLT_NCO; -- SQL/OTS NAMED COLLECTION TYPE */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_BLOB: 		/* SQLT_BLOB; -- SQL/OTS BINARY LARGE OBJECT */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_BFILE: 		/* SQLT_BFILE; -- SQL/OTS BINARY FILE OBJECT */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_CLOB: 		/* SQLT_CLOB; -- SQL/OTS CHARACTER LARGE OBJECT */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_CFILE: 		/* SQLT_CFILE; -- SQL/OTS CHARACTER FILE OBJECT */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_OTMFIRST: 		/* 228; -- first Open Type Manager typecode */
		return GDA_VALUE_TYPE_UNKNOWN;
	case OCI_TYPECODE_OTMLAST: 		/* 320; -- last OTM typecode */
		return GDA_VALUE_TYPE_UNKNOWN;
	}

	return GDA_VALUE_TYPE_UNKNOWN;
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

        /* initialize Oracle environment */
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

	if (OCI_SUCCESS != 
		OCIEnvInit ((OCIEnv **) & priv_data->henv, 
				(ub4) OCI_DEFAULT, 
				(size_t) 0, 
				(dvoid **) 0)) {
		gda_connection_add_error_string (
			cnc, _("Could not initialize Oracle environment"));
		return FALSE;
	}

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
		
	if (OCI_SUCCESS != 
		OCIHandleAlloc ((dvoid *) priv_data->henv, 
				(dvoid **) &priv_data->herr, 
				(ub4) OCI_HTYPE_ERROR, 
				(size_t) 0, 
				(dvoid **) 0)) {
		gda_connection_add_error_string (
			cnc, _("Could not initialize Oracle error handler"));
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

	/* inititalize connection */
	
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

	/* Allocate an oracle statement handle */
	if (OCI_SUCCESS !=
		OCIHandleAlloc ((dvoid *) priv_data->henv,
				(dvoid **) &priv_data->hstmt,
				(ub4) OCI_HTYPE_STMT,
				(size_t) 0,
				(dvoid **) 0)) {
		gda_connection_add_error_string (
			cnc, _("Could not allocate the Oracle statement handle" ));
		return FALSE;
	}

			
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

	if (OCI_SUCCESS != 
		OCISessionEnd (priv_data->hservice,
				priv_data->herr,
				priv_data->hsession,
				OCI_DEFAULT)) {
		gda_connection_add_error_string (
			cnc, _("Could not end the Oracle session"));
		return FALSE;
	}

        if (priv_data->hstmt)
		OCIHandleFree ((dvoid *) priv_data->hstmt, OCI_HTYPE_STMT);
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

static GList *
process_sql_commands (GList *reclist, GdaConnection *cnc, const gchar *sql)
{
	GdaOracleConnectionData *priv_data;

	gchar **arr;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return FALSE;
	}

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			GdaDataModel *recset;

			if (OCI_SUCCESS != 
				OCIStmtPrepare (priv_data->hstmt,
						priv_data->herr,
						(text *) arr[n],
						(ub4) strlen(arr[n]),
						(ub4) OCI_NTV_SYNTAX,
						(ub4) OCI_DEFAULT)) {
				gda_connection_add_error_string (
					cnc, _("Could not prepare the Oracle statement"));
				return NULL;
			}

			if (OCI_SUCCESS != 
				OCIAttrGet (priv_data->hstmt,
						OCI_HTYPE_STMT,
						(dvoid *) &priv_data->stmt_type,
						NULL,
						OCI_ATTR_STMT_TYPE,
						priv_data->herr)) {
				gda_connection_add_error_string (
					cnc, _("Could not get the Oracle statement type"));
				return NULL;
			}
				

			if (OCI_SUCCESS != 
				OCIStmtExecute (priv_data->hservice,
						priv_data->hstmt,
						priv_data->herr,
						(ub4) ((OCI_STMT_SELECT == priv_data->stmt_type) ? 0 : 1),
						(ub4) 0,
						(CONST OCISnapshot *) NULL,
						(OCISnapshot *) NULL,
						OCI_DEFAULT)) {
				gda_connection_add_error_string (
					cnc, _("Could not execute the Oracle command"));
				return NULL;
			}

			recset = GDA_DATA_MODEL (gda_oracle_recordset_new (cnc, priv_data));
			if (GDA_IS_ORACLE_RECORDSET (recset)) {
				gda_data_model_set_command_text (recset, arr[n]);
				gda_data_model_set_command_type (recset, GDA_COMMAND_TYPE_SQL);
				reclist = g_list_append (reclist, recset);
			}
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
	GdaOracleProvider *ora_prv = (GdaOracleProvider *) provider;

	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (ora_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

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
	/*
	if (OCI_SUCCESS !=
		OCIAttrSet ((dvoid *) priv_data->hservice,
				OCI_HTYPE_SVCCTX,
				(dvoid *) txnhp,
				0,
				OCI_ATTR_TRANS,
				priv_data->herr)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
		return FALSE;
	}
	*/

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

	/* get the transaction object */

	/* attach the transaction to the service context */
	/*
	if (OCI_SUCCESS !=
		OCIAttrSet ((dvoid *) priv_data->hservice,
				OCI_HTYPE_SVCCTX,
				(dvoid *) txnhp,
				0,
				OCI_ATTR_TRANS,
				priv_data->herr)) {
		gda_connection_add_error (cnc, gda_oracle_make_error (priv_data));
		return FALSE;
	}
	*/

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

static GdaDataModel *
get_oracle_databases (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaOracleRecordset *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "show databases");
	if (!reclist)
		return NULL;

	recset = GDA_ORACLE_RECORDSET (reclist->data);
	g_list_free (reclist);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_oracle_indexes (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "select object_name from user_objects where object_type='INDEX'");
	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	gda_data_model_set_column_title (recset, 0, _("Indexes"));

	return recset;
}

static GdaDataModel *
get_oracle_procedures (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "select object_name from user_objects where object_type='PROCEDURE'");
	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	gda_data_model_set_column_title (recset, 0, _("Sequences"));

	return recset;
}

static GdaDataModel *
get_oracle_sequences (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "select object_name from user_objects where object_type='SEQUENCE'");
	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	gda_data_model_set_column_title (recset, 0, _("Sequences"));

	return recset;
}

static GdaDataModel *
get_oracle_tables (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "select object_name from user_objects where object_type='TABLE'");
	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	gda_data_model_set_column_title (recset, 0, _("Tables"));

	return recset;
}

static GdaDataModel *
get_oracle_triggers (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "select object_name from user_objects where object_type='TRIGGER'");
	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	gda_data_model_set_column_title (recset, 0, _("Triggers"));

	return recset;
}

static GdaDataModel *
get_oracle_users (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "select username from user_users");
	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	gda_data_model_set_column_title (recset, 0, _("Users"));

	return recset;
}

static GdaDataModel *
get_oracle_views (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "select object_name from user_objects where object_type='VIEW'");
	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	gda_data_model_set_column_title (recset, 0, _("Views"));

	return recset;
}

static GdaDataModel *
get_oracle_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (1);
	//gda_server_recordset_model_set_field_defined_size (recset, 0, 32);
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Type"));
	//gda_server_recordset_model_set_field_scale (recset, 0, 0);
	//gda_server_recordset_model_set_field_gdatype (recset, 0, GDA_TYPE_STRING);

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


static GdaDataModel *
get_table_fields (GdaConnection *cnc, GdaParameterList *params)
{
	const gchar *table_name;
	GdaParameter *par;
	/*gchar *cmd_str;*/
	/*GdaDataModelArray *recset;*/
	/*gint rows, r;*/
	GdaOracleConnectionData *priv_data;

	/*
	struct {
		const gchar *name;
		GdaValueType type;
	} fields_desc[8] = {
		{ N_("Field name")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Data type")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Size")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Scale")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Not null?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Primary key?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Unique index?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("References")	, GDA_VALUE_TYPE_STRING  }
	};
	*/

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid Oracle handle"));
		return NULL;
	}

	/* get parameters sent by client */
	par = gda_parameter_list_find (params, "name");
	if (!par) {
		gda_connection_add_error_string (
			cnc,
			_("You need to specify the name of the table"));
		return NULL;
	}

	table_name = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
	if (!table_name) {
		gda_connection_add_error_string (
			cnc,
			_("You need to specify the name of the table"));
		return NULL;
	}

	/*return GDA_DATA_MODEL (recset);*/
	return NULL;
}

/* get_schema handler for the GdaOracleProvider class */
static GdaDataModel *
gda_oracle_provider_get_schema (GdaServerProvider *provider,
			       GdaConnection *cnc,
			       GdaConnectionSchema schema,
			       GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_AGGREGATES :
		return get_oracle_aggregates (cnc, params);
	case GDA_CONNECTION_SCHEMA_DATABASES :
		return get_oracle_databases (cnc, params);
	case GDA_CONNECTION_SCHEMA_FIELDS :
		return get_table_fields (cnc, params);
	case GDA_CONNECTION_SCHEMA_INDEXES :
		return get_oracle_indexes (cnc, params);
	case GDA_CONNECTION_SCHEMA_PROCEDURES :
		return get_oracle_procedures (cnc, params);
	case GDA_CONNECTION_SCHEMA_SEQUENCES :
		return get_oracle_sequences (cnc, params);
	case GDA_CONNECTION_SCHEMA_TABLES :
		return get_oracle_tables (cnc, params);
	case GDA_CONNECTION_SCHEMA_TRIGGERS :
		return get_oracle_triggers (cnc, params);
	case GDA_CONNECTION_SCHEMA_TYPES :
		return get_oracle_types (cnc, params);
	case GDA_CONNECTION_SCHEMA_USERS :
		return get_oracle_users (cnc, params);
	case GDA_CONNECTION_SCHEMA_VIEWS :
		return get_oracle_views (cnc, params);
	default :
	}

	return NULL;
}
