/* GNOME DB ORACLE Provider
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2000 Stephan Heinze
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gda-quark-list.h"
#include "gda-oracle.h"

typedef GdaServerRecordset* (*schema_ops_fn)(GdaError *,
					      GdaServerConnection *,
					      GDA_Connection_Constraint *constraints,
					      gint length);

static GdaServerRecordset* schema_columns (GdaError *error,
					    GdaServerConnection *cnc,
					    GDA_Connection_Constraint *constraints,
					    gint length);
static GdaServerRecordset* schema_procedures (GdaError *error,
					       GdaServerConnection *cnc,
					       GDA_Connection_Constraint *constraints,
					       gint length);
static GdaServerRecordset* schema_tables (GdaError *error,
					   GdaServerConnection *cnc,
					   GDA_Connection_Constraint *constraints,
					   gint length);
static GdaServerRecordset* schema_views (GdaError *error,
					  GdaServerConnection *cnc,
					  GDA_Connection_Constraint *constraints,
					  gint length);

schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] = { 0, };

/*
 * Private functions
 */
static void
initialize_schema_ops (void)
{
	schema_ops[GDA_Connection_GDCN_SCHEMA_TABLES] = schema_tables;
	schema_ops[GDA_Connection_GDCN_SCHEMA_COLS] = schema_columns;
	schema_ops[GDA_Connection_GDCN_SCHEMA_PROCS] = schema_procedures;
	schema_ops[GDA_Connection_GDCN_SCHEMA_VIEWS] = schema_views;
}

/*
 * Public functions
 */
gboolean
gda_oracle_connection_new (GdaServerConnection *cnc)
{
	static gboolean    initialized;
	ORACLE_Connection* ora_cnc;

	g_return_val_if_fail(cnc != NULL, FALSE);

	/* initialize OCI library */
	if (!initialized) {
		if (OCI_SUCCESS !=  OCIInitialize((ub4) OCI_THREADED | OCI_OBJECT,
						  (dvoid*) 0,
						  (dvoid*(*)(dvoid*, size_t)) 0,
						  (dvoid* (*)(dvoid*, dvoid*, size_t)) 0,
						  (void (*)(dvoid*, dvoid*)) 0)) {
			gda_server_error_make(gda_error_new(), 0, cnc, __PRETTY_FUNCTION__);
			return FALSE;
		}
		initialize_schema_ops();
	}
	initialized = TRUE;

	ora_cnc = g_new0(ORACLE_Connection, 1);
	//OCIHandleAlloc((dvoid *) NULL, (dvoid **) &ora_cnc->henv, OCI_HTYPE_ENV, 0, (dvoid **) 0);
	if (OCI_SUCCESS == OCIEnvInit((OCIEnv **) &ora_cnc->henv, OCI_DEFAULT,
				      (size_t) 0, (dvoid **) 0)) {
		if (OCI_SUCCESS == OCIHandleAlloc((dvoid *) ora_cnc->henv,
						  (dvoid **) &ora_cnc->herr,
						  OCI_HTYPE_ERROR,
						  (size_t) 0,
						  (dvoid **) 0)) {
			/* we use the Multiple Sessions/Connections
			   OCI paradigm for this server */
			if (OCI_SUCCESS == OCIHandleAlloc((dvoid *) ora_cnc->henv,
							  (dvoid **) &ora_cnc->hserver,
							  OCI_HTYPE_SERVER, (size_t) 0,
							  (dvoid **) 0)) {
				if (OCI_SUCCESS == OCIHandleAlloc((dvoid *) ora_cnc->henv,
								  (dvoid **) &ora_cnc->hservice,
								  OCI_HTYPE_SVCCTX, (size_t) 0,
								  (dvoid **) 0)) {
					gda_server_connection_set_user_data(cnc,
									    (gpointer) ora_cnc);
					return TRUE;
				}
				else
					gda_log_error(_("error in OCIHandleAlloc() HTYPE_SVCCTX"));
				OCIHandleFree((dvoid *) ora_cnc->hserver, OCI_HTYPE_SERVER);
			}
			else gda_log_error(_("error in OCIHandleAlloc() HTYPE_SERVER"));
			OCIHandleFree((dvoid *) ora_cnc->herr, OCI_HTYPE_ERROR);
		}
		else gda_log_error(_("error in OCIHandleAlloc() HTYPE_ERROR"));
		OCIHandleFree((dvoid *) ora_cnc->henv, OCI_HTYPE_ENV);
	}
	else  gda_log_error("error in OCIEnvInit()");
	g_free((gpointer) ora_cnc);

	return 0;
}

gint
gda_oracle_connection_open (GdaServerConnection *cnc,
			    const gchar *dsn,
			    const gchar *user,
			    const gchar *password)
{
	ORACLE_Connection* ora_cnc;
	GdaError*          error;
	gchar*             database;
	GdaQuarkList*      qlist;

	g_return_val_if_fail(cnc != NULL, -1);

	ora_cnc = (ORACLE_Connection *) gda_server_connection_get_user_data(cnc);
	if (!ora_cnc)
		return -1;

	/* get database name */
	qlist = gda_quark_list_new_from_string (dsn);
	database = (gchar *) gda_quark_list_find (qlist, "DATABASE");

	/* attach to server */
	if (OCI_SUCCESS == OCIServerAttach(ora_cnc->hserver,
					   ora_cnc->herr,
					   (text *) database,
					   strlen(database),
					   OCI_DEFAULT)) {
		OCIAttrSet((dvoid *) ora_cnc->hservice, OCI_HTYPE_SVCCTX,
			   (dvoid *) ora_cnc->hserver, (ub4) 0,
			   OCI_ATTR_SERVER, ora_cnc->herr);

		/* create the session handle */
		if (OCI_SUCCESS == OCIHandleAlloc((dvoid *) ora_cnc->henv,
						  (dvoid **) &ora_cnc->hsession,
						  OCI_HTYPE_SESSION,
						  (size_t) 0,
						  (dvoid **) 0)) {
			OCIAttrSet((dvoid *) ora_cnc->hsession, OCI_HTYPE_SESSION,
				   (dvoid *) user, (ub4) strlen(user),
				   OCI_ATTR_USERNAME, ora_cnc->herr);
			OCIAttrSet((dvoid *) ora_cnc->hsession, OCI_HTYPE_SESSION,
				   (dvoid *) password, (ub4) strlen(password),
				   OCI_ATTR_PASSWORD, ora_cnc->herr);

			/* inititalize connection */
			if (OCI_SUCCESS == OCISessionBegin(ora_cnc->hservice,
							   ora_cnc->herr,
							   ora_cnc->hsession,
							   OCI_CRED_RDBMS,
							   OCI_DEFAULT)) {
				OCIAttrSet((dvoid *) ora_cnc->hservice, OCI_HTYPE_SVCCTX,
					   (dvoid *) ora_cnc->hsession,
					   (ub4) 0, OCI_ATTR_SESSION,
					   ora_cnc->herr);
				gda_quark_list_free (qlist);
				return 0;
			}
			else
				OCIHandleFree((dvoid *) ora_cnc->hsession, OCI_HTYPE_SESSION);
		}
		else
			OCIServerDetach(ora_cnc->hserver, ora_cnc->herr, OCI_DEFAULT);
	}

	/* return error to client */
	error = gda_error_new();
	gda_server_error_make(error, 0, cnc, __PRETTY_FUNCTION__);

	gda_quark_list_free (qlist);

	return -1;
}

void
gda_oracle_connection_close (GdaServerConnection *cnc)
{
	ORACLE_Connection* ora_cnc;

	g_return_if_fail(cnc != NULL);

	ora_cnc = (ORACLE_Connection *) gda_server_connection_get_user_data(cnc);
	if (ora_cnc) {
		if (OCI_SUCCESS == OCISessionEnd(ora_cnc->hservice,
						 ora_cnc->herr,
						 ora_cnc->hsession,
						 OCI_DEFAULT)) {
			OCIHandleFree((dvoid *) ora_cnc->hsession, OCI_HTYPE_SESSION);
			OCIServerDetach(ora_cnc->hserver, ora_cnc->herr, OCI_DEFAULT);
			OCIHandleFree((dvoid *) ora_cnc->hservice, OCI_HTYPE_SVCCTX);
			OCIHandleFree((dvoid *) ora_cnc->hserver, OCI_HTYPE_SERVER);
			OCIHandleFree((dvoid *) ora_cnc->herr, OCI_HTYPE_ERROR);
			OCIHandleFree((dvoid *) ora_cnc->henv, OCI_HTYPE_ENV);
		}
		else gda_server_error_make(gda_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
	}
}

gint
gda_oracle_connection_begin_transaction (GdaServerConnection *cnc)
{
	ORACLE_Connection* ora_cnc;

	g_return_val_if_fail(cnc != NULL, -1);

	ora_cnc = (ORACLE_Connection *) gda_server_connection_get_user_data(cnc);
	if (ora_cnc) {
		if (OCITransStart(ora_cnc->hservice, ora_cnc->herr, 0,
				  OCI_TRANS_NEW) == OCI_SUCCESS) {
			return 0;
		}
		gda_server_error_make(gda_error_new(), 0, cnc, __PRETTY_FUNCTION__);
	}
	return -1;
}

gint
gda_oracle_connection_commit_transaction (GdaServerConnection *cnc)
{
	ORACLE_Connection* ora_cnc;

	g_return_val_if_fail(cnc != NULL, -1);

	ora_cnc = (ORACLE_Connection *) gda_server_connection_get_user_data(cnc);
	if (ora_cnc) {
		if (OCITransCommit(ora_cnc->hservice, ora_cnc->herr, OCI_DEFAULT)
		    == OCI_SUCCESS) {
			return 0;
		}
		gda_server_error_make(gda_error_new(), 0, cnc, __PRETTY_FUNCTION__);
	}
	return -1;
}

gint
gda_oracle_connection_rollback_transaction (GdaServerConnection *cnc)
{
	ORACLE_Connection* ora_cnc;

	g_return_val_if_fail(cnc != NULL, -1);

	ora_cnc = (ORACLE_Connection *) gda_server_connection_get_user_data(cnc);
	if (ora_cnc) {
		if (OCITransRollback(ora_cnc->hservice, ora_cnc->herr, OCI_DEFAULT)
		    == OCI_SUCCESS) {
			return 0;
		}
		gda_server_error_make(gda_error_new(), 0, cnc, __PRETTY_FUNCTION__);
	}
	return -1;
}

GdaServerRecordset *
gda_oracle_connection_open_schema (GdaServerConnection *cnc,
				   GdaError *error,
				   GDA_Connection_QType t,
				   GDA_Connection_Constraint *constraints,
				   gint length)
{
	schema_ops_fn fn;

	g_return_val_if_fail(cnc != NULL, NULL);

	fn = schema_ops[(gint) t];
	if (fn)
		return fn(error, cnc, constraints, length);
	else
		gda_log_error(_("Unhandled SCHEMA_QTYPE %d"), (gint) t);

	return NULL;
}

glong
gda_oracle_connection_modify_schema (GdaServerConnection *cnc,
                                   GDA_Connection_QType t,
                                   GDA_Connection_Constraint *constraints,
                                   gint length)
{
	return -1;
}

gint
gda_oracle_connection_start_logging (GdaServerConnection *cnc,
				   const gchar *filename)
{
	return -1;
}

gint
gda_oracle_connection_stop_logging (GdaServerConnection *cnc)
{
	return -1;
}

gchar *
gda_oracle_connection_create_table (GdaServerConnection *cnc,
				       GDA_RowAttributes *columns)
{
	return NULL;
}

gboolean
gda_oracle_connection_supports (GdaServerConnection *cnc,
				GDA_Connection_Feature feature)
{
	g_return_val_if_fail(cnc != NULL, FALSE);

	switch (feature) {
	case GDA_Connection_FEATURE_FOREIGN_KEYS :
	case GDA_Connection_FEATURE_TRANSACTIONS :
	case GDA_Connection_FEATURE_SEQUENCES :
	case GDA_Connection_FEATURE_PROCS :
	case GDA_Connection_FEATURE_SQL :
	case GDA_Connection_FEATURE_SQL_SUBSELECT :
	case GDA_Connection_FEATURE_TRIGGERS :
	case GDA_Connection_FEATURE_VIEWS :
		return TRUE;
	}
	return FALSE; /* not supported or know nothing about it */
}

GDA_ValueType
gda_oracle_connection_get_gda_type (GdaServerConnection *cnc, gulong sql_type)
{
	g_return_val_if_fail(cnc != NULL, GDA_TypeNull);

	switch (sql_type) {
	case SQLT_CHR : /* VARCHAR2 */
	case SQLT_STR : /* null-terminated string */
	case SQLT_VCS : /* VARCHAR */
	case SQLT_RID : /* ROWID */
	case SQLT_LNG : /* LONG */
	case SQLT_LVC : /* LONG VARCHAR */
	case SQLT_AFC : /* CHAR */
	case SQLT_AVC : /* CHARZ */
	case SQLT_LAB : /* MSLABEL */
		return GDA_TypeVarchar;
	case SQLT_FLT : /* FLOAT */
		return GDA_TypeSingle;
	case SQLT_INT : /* 8-bit, 16-bit and 32-bit integers */
	case SQLT_UIN : /* UNSIGNED INT */
		return GDA_TypeInteger;
	case SQLT_DAT : /* DATE */
		return GDA_TypeDate;
	case SQLT_VBI : /* VARRAW */
	case SQLT_BIN : /* RAW */
	case SQLT_LBI : /* LONG RAW */
	case SQLT_LVB : /* LONG VARRAW */
		return GDA_TypeBinary;
	default :
		gda_log_error(_("Unknown ORACLE type %ld"), sql_type);
	}
	return GDA_TypeNull;
}

gshort
gda_oracle_connection_get_c_type (GdaServerConnection *cnc, GDA_ValueType type)
{
	g_return_val_if_fail(cnc != NULL, -1);

	//switch (type) {
	//  }
	return -1;
}

gchar *
gda_oracle_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql)
{
	return NULL;
}

gchar *
gda_oracle_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml)
{
	return NULL;
}

void
gda_oracle_connection_free (GdaServerConnection *cnc)
{
	ORACLE_Connection* ora_cnc;

	g_return_if_fail(cnc != NULL);

	ora_cnc = (ORACLE_Connection *) gda_server_connection_get_user_data(cnc);
	if (ora_cnc) {
		g_free((gpointer) ora_cnc);
	}
}

void
gda_oracle_error_make (GdaError *error,
		     GdaServerRecordset *recset,
		     GdaServerConnection *cnc,
		     gchar *where)
{
	ORACLE_Connection* ora_cnc;

	ora_cnc = (ORACLE_Connection *) gda_server_connection_get_user_data(cnc);
	if (ora_cnc) {
		text err_msg[512];
		sb4  errcode;

		memset(err_msg, 0, sizeof(err_msg));
		OCIErrorGet((dvoid *) ora_cnc->herr, (ub4) 1, (text *) NULL, &errcode,
			    (text *) err_msg, (ub4) sizeof(err_msg), OCI_HTYPE_ERROR);
		gda_log_error(_("error '%s' at %s"), err_msg, where);

		gda_error_set_description(error, err_msg);
		gda_error_set_number(error, errcode);
		gda_error_set_source(error, "[gda-oracle]");
		gda_error_set_help_url(error, _("Not available"));
		gda_error_set_help_context(error, _("Not available"));
		gda_error_set_sqlstate(error, _("error"));
		gda_error_set_native(error, err_msg);
	}
}

static GdaServerRecordset *
schema_columns (GdaError *error,
		GdaServerConnection *cnc,
		GDA_Connection_Constraint *constraint,
		gint length)
{
	gchar*                      query = NULL;
	gchar*                      name = NULL;
	GdaServerCommand*          cmd = NULL;
	GDA_Connection_Constraint*  ptr = NULL;
	gint                        cnt;

	/* process constraints */
	ptr = constraint;
	for (cnt = 0; cnt < length && ptr != NULL; cnt++) {
		switch ( ptr->ctype ) {
		case GDA_Connection_OBJECT_NAME:
			name = ptr->value;
			break;
		default:
			g_warning( "schema_types: invalid constraint type %d\n", ptr->ctype);
			return NULL;
		}
		ptr++;
	}

	/* build the command object  and the query */
	cmd = gda_server_command_new(cnc);

	query = g_strdup_printf( "SELECT "
				 " column_name AS \"Name\", "
				 " data_type AS \"Type\", "
				 " data_length AS \"Size\", "
				 " data_precision AS \"Precision\", "
				 " nullable AS \"Nullable\", "
				 " null AS \"Comments\" "
				 " FROM all_tab_columns "
				 " WHERE table_name LIKE '%s' "
				 " ORDER BY column_name ",
				 (NULL==name) ? "%" : g_strstrip(name));
	gda_server_command_set_text(cmd, query);

	/* execute command */
	return gda_server_command_execute(cmd, error, NULL, NULL, 0);
}

static GdaServerRecordset *
schema_procedures (GdaError *error,
		   GdaServerConnection *cnc,
		   GDA_Connection_Constraint *constraints,
		   gint length)
{
	gchar*                      query = NULL;
	gchar*                      name = NULL;
	GdaServerCommand*          cmd = NULL;
	GDA_Connection_Constraint*  ptr = NULL;
	gboolean                    extra_info = FALSE;
	gint                        cnt;

	/* process constraints */
	ptr = constraints;
	for ( cnt = 0; cnt < length && NULL != ptr; cnt++ ) {
		switch ( ptr->ctype ) {
		case GDA_Connection_EXTRA_INFO:
			extra_info = TRUE;
			break;
		case GDA_Connection_OBJECT_NAME:
			name = ptr->value;
			break;
		default:
			g_warning( "schema_types: invalid constraint type %d\n", ptr->ctype);
			return NULL;
		}
		ptr++;
	}

	/* build the command object and the query */
	cmd = gda_server_command_new(cnc);

	if (extra_info) {
		query = g_strdup_printf( "SELECT "
					 " distinct a.name AS \"Name\", "
					 " b.owner AS \"Owner\", "
					 " null AS \"Comments\", "
					 " null AS \"SQL\" "
					 " FROM all_source b, "
					 "      user_source a "
					 " WHERE "
					 "   a.type = 'PROCEDURE' "
					 "   AND a.name LIKE '%s' "
					 "   AND a.name = b.name"
					 " ORDER BY a.name ",
					 (NULL==name) ? "%" : g_strstrip(name)
			);
	}
	else {
		query = g_strdup_printf( "SELECT "
					 " distinct name AS \"Name\", "
					 " null AS \"Comments\" "
					 " FROM user_source "
					 " WHERE "
					 "   name LIKE '%s' "
					 " ORDER BY name ",
					 (NULL==name) ? "%" : g_strstrip(name)
			);
	}
	gda_server_command_set_text(cmd, query);

	/* execute the command */
	return gda_server_command_execute(cmd, error, NULL, NULL, 0);
}

static GdaServerRecordset *
schema_tables (GdaError *error,
	       GdaServerConnection *cnc,
	       GDA_Connection_Constraint *constraints,
	       gint length)
{
	gchar*                      query = NULL;
	gchar*                      name = NULL;
	gchar*                      schema = NULL;
	GString*                    where_clause = NULL;
	GdaServerCommand*          cmd = NULL;
	GDA_Connection_Constraint*  ptr = NULL;
	gboolean                    extra_info = FALSE;
	gint                        cnt;

	/* process constraints */
	ptr = constraints;
	for ( cnt = 0; cnt < length && NULL != ptr; cnt++ ) {
		switch ( ptr->ctype ) {
		case GDA_Connection_EXTRA_INFO:
			extra_info = TRUE;
			break;
		case GDA_Connection_OBJECT_NAME:
			name = ptr->value;
			break;
		case GDA_Connection_OBJECT_CATALOG:
			g_warning( "schema_types: not supported constraint type %d\n", ptr->ctype);
			break;
		case GDA_Connection_OBJECT_SCHEMA:
			schema = ptr->value;
			break;
		default:
			g_warning( "schema_types: invalid constraint type %d\n", ptr->ctype);
			return NULL;
		}
		ptr++;
	}

	/* build the command object and the query */
	cmd = gda_server_command_new(cnc);

	where_clause = g_string_new(" WHERE d.table_name (+)= b.table_name" );

	if( NULL != name )
		g_string_sprintfa( where_clause, " AND b.table_name = '%s' ", name);

	if( NULL != schema )
		g_string_sprintfa( where_clause, " AND b.ownwer = '%s' ", schema);

	/* build the query */
	if (extra_info) {
		query = g_strdup_printf( "SELECT "
					 " a.table_name AS \"Name\", "
					 " b.owner AS \"Owner\", "
					 " d.comments AS \"Comments\", "
					 " null AS \"SQL\" "
					 " FROM all_tables b, "
					 "      dictionary d"
					 " %s "
					 " ORDER BY b.table_name ",
					 where_clause->str
			);
	}
	else {
		query = g_strdup_printf( "SELECT "
					 " b.table_name AS \"Name\", "
					 " d.comments AS \"Comments\" "
					 " FROM all_tables b, dictionary d"
					 " %s ",
					 where_clause->str
			);
	}
	g_string_free( where_clause, TRUE );
	gda_server_command_set_text(cmd, query);

	/* execute the command */
	return gda_server_command_execute(cmd, error, NULL, NULL, 0);
}

static GdaServerRecordset *
schema_views (GdaError *error,
	      GdaServerConnection *cnc,
	      GDA_Connection_Constraint *constraints,
	      gint length)
{
	gchar*                      query = NULL;
	gchar*                      name = NULL;
	gchar*                      schema = NULL;
	GString*                    where_clause = NULL;
	GdaServerCommand*          cmd = NULL;
	GDA_Connection_Constraint*  ptr = NULL;
	gboolean                    extra_info = FALSE;
	gint                        cnt;

	/* process constraints */
	ptr = constraints;
	for ( cnt = 0; cnt < length && NULL != ptr; cnt++ ) {
		switch ( ptr->ctype ) {
		case GDA_Connection_EXTRA_INFO:
			extra_info = TRUE;
			break;
		case GDA_Connection_OBJECT_NAME:
			name = ptr->value;
			break;
		case GDA_Connection_OBJECT_CATALOG:
			g_warning( "schema_types: not supported constraint type %d\n", ptr->ctype);
			break;
		case GDA_Connection_OBJECT_SCHEMA:
			schema = ptr->value;
			break;
		default:
			g_warning( "schema_types: invalid constraint type %d\n", ptr->ctype);
			return NULL;
		}
		ptr++;
	}

	/* build the command object and the query */
	cmd = gda_server_command_new(cnc);

	where_clause = g_string_new(" WHERE 1 = 1 " ); /* lausy - I know */

	if( NULL != name )
		g_string_sprintfa( where_clause, " AND b.view_name = '%s' ", name);

	if( NULL != schema )
		g_string_sprintfa( where_clause, " AND b.ownwer = '%s' ", schema);

	if (extra_info) {
		query = g_strdup_printf( "SELECT "
					 " b.view_name AS \"Name\", "
					 " b.owner AS \"Owner\", "
					 " null AS \"Comments\", "
					 " null AS \"SQL\" "
					 " FROM all_tables b "
					 " %s "
					 " ORDER BY b.view_name ",
					 where_clause->str);
	}
	else {
		query = g_strdup_printf( "SELECT "
					 " b.view_name AS \"Name\", "
					 " null AS \"Comments\" "
					 " FROM all_views b "
					 " %s ",
					 where_clause->str
			);
	}
	g_string_free(where_clause, TRUE);
	gda_server_command_set_text(cmd, query);

	/* execute the command */
	return gda_server_command_execute(cmd, error, NULL, NULL, 0);
}

