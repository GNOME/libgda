/* GDA Server Library
 * Copyright (C) 2000, Rodrigo Moya <rmoya@chez.com>
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

#include "config.h"
#include "gda-server-impl.h"
#include "gda-server-impl-connection.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough.  */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

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
	GDA_ErrorSeq* rc = GDA_ErrorSeq__alloc();

	rc->_length = 0;
	rc->_buffer = gda_server_impl_make_error_buffer(servant->cnc);
	return rc;
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
	if (gda_server_impl_exception(ev) < 0)
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
	servant->cnc = gda_server_connection_new(gda_server_impl_find(servant->id));
	if (!servant->cnc)
		return -1;

	if (gda_server_connection_open(servant->cnc, dsn, user, passwd) != 0) {
		GDA_DriverError* exception = GDA_DriverError__alloc();

		if (servant->cnc != NULL && servant->cnc->errors != NULL) {
			exception->errors._length = g_list_length(servant->cnc->errors);
			exception->errors._buffer = gda_server_impl_make_error_buffer(servant->cnc);
		}
		else
			exception->errors._length = 0;
		exception->realcommand = CORBA_string_dup(__PRETTY_FUNCTION__);
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GDA_DriverError, exception);
		return -1;
	}

	return 0;
}

GDA_Recordset
impl_GDA_Connection_openSchema (impl_POA_GDA_Connection * servant,
                                GDA_Connection_QType t,
				GDA_Connection_ConstraintSeq * constraints,
				CORBA_Environment * ev)
{
	GdaServerConnection* cnc = servant->cnc;
	GDA_Recordset             new_recset;
	GdaServerRecordset*  recset;
	GdaServerError       e;

	memset(&e, '\0', sizeof(e));
	if ((recset = gda_server_connection_open_schema(cnc, &e, t,
							constraints->_buffer,
							constraints->_length)) == 0) {
		GDA_DriverError* exception = GDA_DriverError__alloc();
		exception->errors._length = g_list_length(servant->cnc->errors);
		exception->errors._buffer = gda_server_impl_make_error_buffer(servant->cnc);
		exception->realcommand = CORBA_string_dup(__PRETTY_FUNCTION__);
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GDA_DriverError, exception);
		return CORBA_OBJECT_NIL;
    }
	new_recset = impl_GDA_Recordset__create(servant->poa, recset, ev);
	gda_server_impl_exception(ev);
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
		GDA_DriverError* exception = GDA_DriverError__alloc();
		exception->errors._length = g_list_length(servant->cnc->errors);
		exception->errors._buffer = gda_server_impl_make_error_buffer(servant->cnc);
		exception->realcommand = CORBA_string_dup(__PRETTY_FUNCTION__);
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GDA_DriverError, exception);
		return -1;
    }
	if (!gda_server_impl_exception(ev)) return -1;
  
	return 0;
}

GDA_Command
impl_GDA_Connection_createCommand (impl_POA_GDA_Connection * servant,
				   CORBA_Environment * ev)
{
	GDA_Command retval;
	GdaServerCommand* cmd = gda_server_command_new(servant->cnc);

	retval = impl_GDA_Command__create(servant->poa, cmd, ev);
	if (gda_server_impl_exception(ev)) {
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
	if (gda_server_impl_exception(ev)) {
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
		return (0);
	gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
	return (-1);
}

CORBA_long
impl_GDA_Connection_stopLogging (impl_POA_GDA_Connection * servant,
				 CORBA_Environment * ev)
{
	GdaServerConnection *cnc = servant->cnc;

	if (gda_server_connection_stop_logging(cnc) != -1)
		return (0);
	gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
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

