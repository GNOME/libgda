/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#include "gda-error.h"
#include "gda-server.h"
#include "gda-server-private.h"

/*
 * epv structures
 */

static PortableServer_ServantBase__epv impl_GDA_Connection_base_epv = {
	NULL,                        /* _private data */
	(gpointer) & impl_GDA_Connection__destroy,   /* finalize routine */
	NULL,                        /* default_POA routine */
};

static POA_GDA_Connection__epv impl_GDA_Connection_epv = {
	NULL,                        /* _private */
	(gpointer) & impl_GDA_Connection__get_flags,
	(gpointer) & impl_GDA_Connection__set_flags,
	(gpointer) & impl_GDA_Connection__get_cmdTimeout,
	(gpointer) & impl_GDA_Connection__set_cmdTimeout,
	(gpointer) & impl_GDA_Connection__get_connectTimeout,
	(gpointer) & impl_GDA_Connection__set_connectTimeout,
	(gpointer) & impl_GDA_Connection__get_cursor,
	(gpointer) & impl_GDA_Connection__set_cursor,
	(gpointer) & impl_GDA_Connection__get_version,
	(gpointer) & impl_GDA_Connection__get_errors,
	(gpointer) & impl_GDA_Connection_beginTransaction,
	(gpointer) & impl_GDA_Connection_commitTransaction,
	(gpointer) & impl_GDA_Connection_rollbackTransaction,
	(gpointer) & impl_GDA_Connection_close,
	(gpointer) & impl_GDA_Connection_open,
	(gpointer) & impl_GDA_Connection_openSchema,
	(gpointer) & impl_GDA_Connection_modifySchema,
	(gpointer) & impl_GDA_Connection_createCommand,
	(gpointer) & impl_GDA_Connection_createRecordset,
	(gpointer) & impl_GDA_Connection_startLogging,
	(gpointer) & impl_GDA_Connection_stopLogging,
	(gpointer) & impl_GDA_Connection_createTable,
	(gpointer) & impl_GDA_Connection_supports,
	(gpointer) & impl_GDA_Connection_sql2xml,
	(gpointer) & impl_GDA_Connection_xml2sql
};

/*
 * vepv structures
 */

static POA_GDA_Connection__vepv impl_GDA_Connection_vepv = {
	&impl_GDA_Connection_base_epv,
	&impl_GDA_Connection_epv,
};

/*
 * Stub implementations
 */
GDA_Connection
impl_GDA_Connection__create (PortableServer_POA poa, CORBA_char *id, CORBA_Environment * ev)
{
	GDA_Connection retval;
	impl_POA_GDA_Connection *newservant;
	PortableServer_ObjectId *objid;

	newservant = g_new0(impl_POA_GDA_Connection, 1);
	newservant->servant.vepv = &impl_GDA_Connection_vepv;
	newservant->poa = poa;

	newservant->cnc = NULL;
	newservant->id = g_strdup (id);

	POA_GDA_Connection__init((PortableServer_Servant) newservant, ev);
	objid = PortableServer_POA_activate_object(poa, newservant, ev);
	CORBA_free(objid);
	retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

	return retval;
}

/* You shouldn't call this routine directly without first deactivating the servant... */
void
impl_GDA_Connection__destroy (impl_POA_GDA_Connection * servant, CORBA_Environment * ev)
{
	POA_GDA_Connection__fini((PortableServer_Servant) servant, ev);
	g_free(servant);
}

CORBA_long
impl_GDA_Connection__get_flags (impl_POA_GDA_Connection * servant,
				CORBA_Environment * ev)
{
	gda_log_message(_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

void
impl_GDA_Connection__set_flags (impl_POA_GDA_Connection * servant,
				CORBA_long value,
				CORBA_Environment * ev)
{
	gda_log_message(_("%s not implemented"), __PRETTY_FUNCTION__);
}

CORBA_long
impl_GDA_Connection__get_cmdTimeout (impl_POA_GDA_Connection * servant,
				     CORBA_Environment * ev)
{
	gda_log_message(_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

void
impl_GDA_Connection__set_cmdTimeout (impl_POA_GDA_Connection * servant,
				     CORBA_long value,
				     CORBA_Environment * ev)
{
	gda_log_message(_("%s not implemented"), __PRETTY_FUNCTION__);
}

CORBA_long
impl_GDA_Connection__get_connectTimeout (impl_POA_GDA_Connection * servant,
					 CORBA_Environment * ev)
{
	gda_log_message(_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

void
impl_GDA_Connection__set_connectTimeout (impl_POA_GDA_Connection * servant,
					 CORBA_long value,
					 CORBA_Environment * ev)
{
	gda_log_message(_("%s not implemented"), __PRETTY_FUNCTION__);
}

GDA_CursorLocation
impl_GDA_Connection__get_cursor (impl_POA_GDA_Connection * servant,
				 CORBA_Environment * ev)
{
	gda_log_message(_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

void
impl_GDA_Connection__set_cursor (impl_POA_GDA_Connection * servant,
				 GDA_CursorLocation value,
				 CORBA_Environment * ev)
{
	gda_log_message(_("%s not implemented"), __PRETTY_FUNCTION__);
}

CORBA_char *
impl_GDA_Connection__get_version (impl_POA_GDA_Connection * servant,
				  CORBA_Environment * ev)
{
	return CORBA_string_dup(VERSION);
}

GDA_ErrorSeq *
impl_GDA_Connection__get_errors (impl_POA_GDA_Connection * servant,
				 CORBA_Environment * ev)
{
	return gda_error_list_to_corba_seq (servant->cnc->errors);
}

CORBA_long
impl_GDA_Connection_beginTransaction (impl_POA_GDA_Connection * servant,
				      CORBA_Environment * ev)
{
	GdaServerConnection* cnc = servant->cnc;
	GDA_NotSupported*     exception;

	if (gda_server_connection_begin_transaction(cnc) != -1)
		return (0);
	exception = GDA_NotSupported__alloc();
	exception->errormsg = CORBA_string_dup("Transactions not Supported");
	CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GDA_NotSupported, exception);
	return -1;
}

CORBA_long
impl_GDA_Connection_commitTransaction (impl_POA_GDA_Connection * servant,
				       CORBA_Environment * ev)
{
	GdaServerConnection* cnc = servant->cnc;
	GDA_NotSupported*         exception;

	if (gda_server_connection_commit_transaction(cnc) != -1)
		return 0;
	exception = GDA_NotSupported__alloc();
	exception->errormsg = CORBA_string_dup("Transactions not Supported");
	CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GDA_NotSupported, exception); 
	return -1;
}

CORBA_long
impl_GDA_Connection_rollbackTransaction (impl_POA_GDA_Connection * servant,
					 CORBA_Environment * ev)
{
	GdaServerConnection* cnc = servant->cnc;
	GDA_NotSupported*         exception;

	if (gda_server_connection_rollback_transaction(cnc) != -1)
		return (0);
	exception = GDA_NotSupported__alloc();
	exception->errormsg = CORBA_string_dup("Transactions not Supported");
	CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GDA_NotSupported, exception); 
	return (-1);

}

CORBA_long
impl_GDA_Connection_close (impl_POA_GDA_Connection * servant,
			   CORBA_Environment * ev)
{
	CORBA_long rc = 0;

	gda_server_connection_close (servant->cnc);
	gda_server_connection_free (servant->cnc);
	servant->cnc = NULL;
	g_free (servant->id);

	PortableServer_POA_deactivate_object (
		servant->poa,
		PortableServer_POA_servant_to_id(servant->poa, servant, ev),
		ev);
	if (gda_server_exception(ev) < 0)
		rc = -1;
	return rc;
}

CORBA_long
impl_GDA_Connection_open (impl_POA_GDA_Connection * servant,
			  CORBA_char * dsn,
			  CORBA_char * user,
			  CORBA_char * passwd,
			  CORBA_Environment * ev)
{
	gda_log_message(_("impl_GDA_Connection_open: opening connection with DSN: %s"), dsn);

	/* free the previous GdaServerConnection, if any */
	if (servant->cnc != NULL)
		gda_server_connection_free (servant->cnc);
	servant->cnc = gda_server_connection_new(gda_server_find(servant->id));
	if (!servant->cnc)
		return -1;

	if (gda_server_connection_open(servant->cnc, dsn, user, passwd) != 0)
		gda_error_list_to_exception (servant->cnc->errors, ev);

	return 0;
}

GDA_Recordset
impl_GDA_Connection_openSchema (impl_POA_GDA_Connection * servant,
                                GDA_Connection_QType t,
				GDA_Connection_ConstraintSeq * constraints,
				CORBA_Environment * ev)
{
	GdaServerConnection* cnc = servant->cnc;
	GDA_Recordset        new_recset;
	GdaServerRecordset*  recset;
	GdaServer            e;

	memset(&e, '\0', sizeof(e));
	if ((recset = gda_server_connection_open_schema(cnc, &e, t,
							constraints->_buffer,
							constraints->_length)) == 0) {
		gda_error_list_to_exception (cnc->errors, ev);
		return CORBA_OBJECT_NIL;
	}
	new_recset = impl_GDA_Recordset__create(servant->poa, recset, ev);
	gda_server_exception(ev);

	return new_recset;
}

CORBA_long
impl_GDA_Connection_modifySchema (impl_POA_GDA_Connection * servant,
                                  GDA_Connection_QType t,
                                  GDA_Connection_ConstraintSeq * constraints,
                                  CORBA_Environment * ev)
{
	GdaServerConnection* cnc = servant->cnc;

	if (gda_server_connection_modify_schema(cnc, t, constraints->_buffer, constraints->_length)
	    != 0) {
		gda_error_list_to_exception (cnc->errors, ev);
		return -1;
	}
	if (!gda_server_exception(ev))
		return -1;
  
	return 0;
}

GDA_Command
impl_GDA_Connection_createCommand (impl_POA_GDA_Connection * servant,
				   CORBA_Environment * ev)
{
	GDA_Command retval;
	GdaServerCommand* cmd = gda_server_command_new(servant->cnc);

	retval = impl_GDA_Command__create(servant->poa, cmd, ev);
	if (gda_server_exception(ev)) {
		gda_server_command_free(cmd);
		return CORBA_OBJECT_NIL;
	}
	cmd->cnc = servant->cnc;

	return retval;
}

GDA_Recordset
impl_GDA_Connection_createRecordset (impl_POA_GDA_Connection * servant,
				     CORBA_Environment * ev)
{
	GDA_Recordset            retval;
	GdaServerRecordset* rs = gda_server_recordset_new(servant->cnc);

	retval = impl_GDA_Recordset__create(servant->poa, rs, ev);
	if (gda_server_exception(ev)) {
		gda_server_recordset_free(rs);
		return CORBA_OBJECT_NIL;
    }
	rs->cnc = servant->cnc;
	return retval;
}

CORBA_long
impl_GDA_Connection_startLogging (impl_POA_GDA_Connection * servant,
				  CORBA_char * filename,
				  CORBA_Environment * ev)
{
	GdaServerConnection *cnc = servant->cnc;

	if (gda_server_connection_start_logging(cnc, filename) != -1)
		return 0;
	gda_error_list_to_exception (cnc->errors, ev);

	return -1;
}

CORBA_long
impl_GDA_Connection_stopLogging (impl_POA_GDA_Connection * servant,
				 CORBA_Environment * ev)
{
	GdaServerConnection *cnc = servant->cnc;

	if (gda_server_connection_stop_logging(cnc) != -1)
		return 0;
	gda_error_list_to_exception (cnc->errors, ev);

	return (-1);
}

CORBA_char *
impl_GDA_Connection_createTable (impl_POA_GDA_Connection * servant,
				 CORBA_char * name,
				 GDA_RowAttributes * columns,
				 CORBA_Environment * ev)
{
	CORBA_char* retval;

	retval = gda_server_connection_create_table(servant->cnc, columns);
	return (retval);
}

CORBA_boolean
impl_GDA_Connection_supports (impl_POA_GDA_Connection * servant,
                              GDA_Connection_Feature feature,
                              CORBA_Environment *ev)
{
	return gda_server_connection_supports(servant->cnc, feature);
}

CORBA_char *
impl_GDA_Connection_sql2xml (impl_POA_GDA_Connection *servant,
                             CORBA_char *sql,
                             CORBA_Environment *ev)
{
	return gda_server_connection_sql2xml(servant->cnc, sql);
}

CORBA_char *
impl_GDA_Connection_xml2sql (impl_POA_GDA_Connection *servant,
                             CORBA_char *xml,
                             CORBA_Environment *ev)
{
	return gda_server_connection_xml2sql(servant->cnc, xml);
}

/*
 * Private functions
 */
static void
free_error_list (GList *list)
{
	GList* node;
	
	g_return_if_fail(list != NULL);
	
	while ((node = g_list_first(list))) {
		GdaError* error = (GdaError *) node->data;
		list = g_list_remove(list, (gpointer) error);
		gda_error_free(error);
	}
}

/**
 * gda_server_connection_new
 */
GdaServerConnection *
gda_server_connection_new (GdaServer *server_impl)
{
	GdaServerConnection* cnc;
	
	g_return_val_if_fail(server_impl != NULL, NULL);
	
	/* FIXME: could be possible to share connections */
	cnc = g_new0(GdaServerConnection, 1);
	cnc->server_impl = server_impl;
	cnc->users = 1;
	
	/* notify 'real' provider */
	cnc->server_impl->connections = g_list_append(cnc->server_impl->connections, (gpointer) cnc);
	if (cnc->server_impl->functions.connection_new != NULL) {
		cnc->server_impl->functions.connection_new(cnc);
	}
	
	return cnc;
}

/**
 * gda_server_connection_open
 */
gint
gda_server_connection_open (GdaServerConnection *cnc,
                            const gchar *dsn,
                            const gchar *user,
                            const gchar *password)
{
	gint rc;
	
	g_return_val_if_fail(cnc != NULL, -1);
	g_return_val_if_fail(dsn != NULL, -1);
	g_return_val_if_fail(cnc->server_impl != NULL, -1);
	g_return_val_if_fail(cnc->server_impl->functions.connection_open != NULL, -1);
	
	rc = cnc->server_impl->functions.connection_open(cnc, dsn, user, password);
	if (rc != -1) {
		gda_server_connection_set_dsn(cnc, dsn);
		gda_server_connection_set_username(cnc, user);
		gda_server_connection_set_password(cnc, password);
		rc = 0;
	}
	return rc;
}

/**
 * gda_server_connection_close
 */
void
gda_server_connection_close (GdaServerConnection *cnc)
{
	g_return_if_fail(cnc != NULL);
	g_return_if_fail(cnc->server_impl != NULL);
	g_return_if_fail(cnc->server_impl->functions.connection_close != NULL);
	
	cnc->server_impl->functions.connection_close(cnc);
}

/**
 * gda_server_connection_begin_transaction
 */
gint
gda_server_connection_begin_transaction (GdaServerConnection *cnc)
{
	g_return_val_if_fail(cnc != NULL, -1);
	g_return_val_if_fail(cnc->server_impl != NULL, -1);
	g_return_val_if_fail(cnc->server_impl->functions.connection_begin_transaction != NULL, -1);
	
	return cnc->server_impl->functions.connection_begin_transaction(cnc);
}

/**
 * gda_server_connection_commit_transaction
 */
gint
gda_server_connection_commit_transaction (GdaServerConnection *cnc)
{
	g_return_val_if_fail(cnc != NULL, -1);
	g_return_val_if_fail(cnc->server_impl != NULL, -1);
	g_return_val_if_fail(cnc->server_impl->functions.connection_commit_transaction != NULL, -1);
	
	return cnc->server_impl->functions.connection_commit_transaction(cnc);
}

/**
 * gda_server_connection_rollback_transaction
 */
gint
gda_server_connection_rollback_transaction (GdaServerConnection *cnc)
{
	g_return_val_if_fail(cnc != NULL, -1);
	g_return_val_if_fail(cnc->server_impl != NULL, -1);
	g_return_val_if_fail(cnc->server_impl->functions.connection_rollback_transaction != NULL, -1);
	
	return cnc->server_impl->functions.connection_rollback_transaction(cnc);
}

/**
 * gda_server_connection_open_schema
 */
GdaServerRecordset *
gda_server_connection_open_schema (GdaServerConnection *cnc,
                                   GdaError *error,
                                   GDA_Connection_QType t,
                                   GDA_Connection_Constraint *constraints,
                                   gint length)
{
	g_return_val_if_fail(cnc != NULL, NULL);
	g_return_val_if_fail(cnc->server_impl != NULL, NULL);
	g_return_val_if_fail(cnc->server_impl->functions.connection_open_schema != NULL, NULL);
	
	return cnc->server_impl->functions.connection_open_schema(cnc, error, t, constraints, length);
}

/**
 * gda_server_connection_modify_schema
 */
glong
gda_server_connection_modify_schema (GdaServerConnection *cnc,
                                     GDA_Connection_QType t,
                                     GDA_Connection_Constraint *constraints,
                                     gint length)
{
	g_return_val_if_fail(cnc != NULL, -1);
	g_return_val_if_fail(cnc->server_impl != NULL, -1);
	g_return_val_if_fail(cnc->server_impl->functions.connection_modify_schema != NULL, -1);
	
	return cnc->server_impl->functions.connection_modify_schema(cnc, t, constraints, length);
}

/**
 * gda_server_connection_start_logging
 */
gint
gda_server_connection_start_logging (GdaServerConnection *cnc, const gchar *filename)
{
	g_return_val_if_fail(cnc != NULL, -1);
	g_return_val_if_fail(cnc->server_impl != NULL, -1);
	g_return_val_if_fail(cnc->server_impl->functions.connection_start_logging != NULL, -1);
	
	return cnc->server_impl->functions.connection_start_logging(cnc, filename);
}

/**
 * gda_server_connection_stop_logging
 */
gint
gda_server_connection_stop_logging (GdaServerConnection *cnc)
{
	g_return_val_if_fail(cnc != NULL, -1);
	g_return_val_if_fail(cnc->server_impl != NULL, -1);
	g_return_val_if_fail(cnc->server_impl->functions.connection_stop_logging != NULL, -1);
	
	return cnc->server_impl->functions.connection_stop_logging(cnc);
}

/**
 * gda_server_connection_create_table
 */
gchar *
gda_server_connection_create_table (GdaServerConnection *cnc, GDA_RowAttributes *columns)
{
	g_return_val_if_fail(cnc != NULL, NULL);
	g_return_val_if_fail(cnc->server_impl != NULL, NULL);
	g_return_val_if_fail(cnc->server_impl->functions.connection_create_table != NULL, NULL);
	g_return_val_if_fail(columns != NULL, NULL);
	
	return cnc->server_impl->functions.connection_create_table(cnc, columns);
}

/**
 * gda_server_connection_supports
 */
gboolean
gda_server_connection_supports (GdaServerConnection *cnc, GDA_Connection_Feature feature)
{
	g_return_val_if_fail(cnc != NULL, FALSE);
	g_return_val_if_fail(cnc->server_impl != NULL, FALSE);
	g_return_val_if_fail(cnc->server_impl->functions.connection_supports != NULL, FALSE);
	
	return cnc->server_impl->functions.connection_supports(cnc, feature);
}

/**
 * gda_server_connection_get_gda_type
 */
GDA_ValueType
gda_server_connection_get_gda_type (GdaServerConnection *cnc, gulong sql_type)
{
	g_return_val_if_fail(cnc != NULL, GDA_TypeNull);
	g_return_val_if_fail(cnc->server_impl != NULL, GDA_TypeNull);
	g_return_val_if_fail(cnc->server_impl->functions.connection_get_gda_type != NULL, GDA_TypeNull);

	return cnc->server_impl->functions.connection_get_gda_type(cnc, sql_type);
}

/**
 * gda_server_connection_get_c_type
 */
gshort
gda_server_connection_get_c_type (GdaServerConnection *cnc, GDA_ValueType type)
{
	g_return_val_if_fail(cnc != NULL, -1);
	g_return_val_if_fail(cnc->server_impl != NULL, -1);
	g_return_val_if_fail(cnc->server_impl->functions.connection_get_c_type != NULL, -1);
	
	return cnc->server_impl->functions.connection_get_c_type(cnc, type);
}

/**
 * gda_server_connection_sql2xml
 */
gchar *
gda_server_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql)
{
	g_return_if_fail(cnc != NULL);
	g_return_if_fail(cnc->server_impl != NULL);
	g_return_if_fail(cnc->server_impl->functions.connection_sql2xml != NULL);
	
	return cnc->server_impl->functions.connection_sql2xml(cnc, sql);
}

/**
 * gda_server_connection_xml2sql
 */
gchar *
gda_server_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml)
{
	g_return_if_fail(cnc != NULL);
	g_return_if_fail(cnc->server_impl != NULL);
	g_return_if_fail(cnc->server_impl->functions.connection_xml2sql != NULL);
	
	return cnc->server_impl->functions.connection_xml2sql(cnc, xml);
}

/**
 * gda_server_connection_get_dsn
 */
gchar *
gda_server_connection_get_dsn (GdaServerConnection *cnc)
{
	g_return_val_if_fail(cnc != NULL, NULL);
	return cnc->dsn;
}

/**
 * gda_server_connection_set_dsn
 */
void
gda_server_connection_set_dsn (GdaServerConnection *cnc, const gchar *dsn)
{
	g_return_if_fail(cnc != NULL);
	
	if (cnc->dsn)
		g_free((gpointer) cnc->dsn);
	if (dsn)
		cnc->dsn = g_strdup(dsn);
	else cnc->dsn = NULL;
}

/**
 * gda_server_connection_get_username
 */
gchar *
gda_server_connection_get_username (GdaServerConnection *cnc)
{
	g_return_val_if_fail(cnc != NULL, NULL);
	return cnc->username;
}

/**
 * gda_server_connection_set_username
 */
void
gda_server_connection_set_username (GdaServerConnection *cnc, const gchar *username)
{
	g_return_if_fail(cnc != NULL);
	
	if (cnc->username) g_free((gpointer) cnc->username);
	if (username) cnc->username = g_strdup(username);
	else cnc->username = NULL;
}

/**
 * gda_server_connection_get_password
 */
gchar *
gda_server_connection_get_password (GdaServerConnection *cnc)
{
	g_return_val_if_fail(cnc != NULL, NULL);
	return cnc->password;
}

/**
 * gda_server_connection_set_password
 */
void
gda_server_connection_set_password (GdaServerConnection *cnc, const gchar *password)
{
	g_return_if_fail(cnc != NULL);
	
	if (cnc->password) g_free((gpointer) cnc->password);
	if (password) cnc->password = g_strdup(password);
	else cnc->password = NULL;
}

/**
 * gda_server_connection_add_error
 */
void
gda_server_connection_add_error (GdaServerConnection *cnc, GdaError *error)
{
	g_return_if_fail(cnc != NULL);
	g_return_if_fail(error != NULL);
	
	cnc->errors = g_list_append(cnc->errors, (gpointer) error);
}

/**
 * gda_server_connection_add_error_string
 * @cnc: connection
 * @msg: error message
 *
 * Adds a new error to the given connection from a given error string
 */
void
gda_server_connection_add_error_string (GdaServerConnection *cnc, const gchar *msg)
{
	GdaError* error;
	
	g_return_if_fail(cnc != NULL);
	g_return_if_fail(msg != NULL);
	
	error = gda_error_new();
	gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
	gda_error_set_description(error, msg);
	gda_error_set_native(error, msg);

	cnc->errors = g_list_append(cnc->errors, (gpointer) error);
}

/**
 * gda_server_connection_get_user_data
 */
gpointer
gda_server_connection_get_user_data (GdaServerConnection *cnc)
{
	g_return_val_if_fail(cnc != NULL, NULL);
	return cnc->user_data;
}

/**
 * gda_server_connection_set_user_data
 */
void
gda_server_connection_set_user_data (GdaServerConnection *cnc, gpointer user_data)
{
	g_return_if_fail(cnc != NULL);
	cnc->user_data = user_data;
}

/**
 * gda_server_connection_free
 */
void
gda_server_connection_free (GdaServerConnection *cnc)
{
	g_return_if_fail(cnc != NULL);
	
	if ((cnc->server_impl != NULL) &&
	    (cnc->server_impl->functions.connection_free != NULL)) {
		cnc->server_impl->functions.connection_free(cnc);
	}
	
	cnc->users--;
	if (!cnc->users) {
		if (cnc->dsn) g_free((gpointer) cnc->dsn);
		if (cnc->username) g_free((gpointer) cnc->username);
		if (cnc->password) g_free((gpointer) cnc->password);
		//g_list_foreach(cnc->commands, (GFunc) gda_server_command_free, NULL);
		//free_error_list(cnc->errors);
		
		if (cnc->server_impl) {
			cnc->server_impl->connections = g_list_remove(cnc->server_impl->connections,
			                                              (gpointer) cnc);
			if (!cnc->server_impl->connections) {
				/* if no connections left, terminate */
				gda_log_message("No connections left. Terminating");
				gda_server_stop(cnc->server_impl);
			}
		}
		g_free((gpointer) cnc);
	}
}

