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

#include "config.h"
#include "gda-server-impl.h"
#include "gda-server-private.h"

/*
 * epv structures
 */

static PortableServer_ServantBase__epv impl_GDA_Parameter_base_epv = {
	NULL,                        /* _private data */
	(gpointer) & impl_GDA_Parameter__destroy,    /* finalize routine */
	NULL,                        /* default_POA routine */
};

static POA_GDA_Parameter__epv impl_GDA_Parameter_epv = {
	NULL,                        /* _private */
	(gpointer) & impl_GDA_Parameter_appendChunk,
};

static PortableServer_ServantBase__epv impl_GDA_Command_base_epv = {
	NULL,                        /* _private data */
	(gpointer) & impl_GDA_Command__destroy,      /* finalize routine */
	NULL,                        /* default_POA routine */
};

static POA_GDA_Command__epv impl_GDA_Command_epv = {
	NULL,                        /* _private */
	(gpointer) & impl_GDA_Command__get_cmdTimeout,
	(gpointer) & impl_GDA_Command__set_cmdTimeout,
	(gpointer) & impl_GDA_Command__get_prepared,
	(gpointer) & impl_GDA_Command__get_state,
	(gpointer) & impl_GDA_Command__set_state,
	(gpointer) & impl_GDA_Command__get_text,
	(gpointer) & impl_GDA_Command__set_text,
	(gpointer) & impl_GDA_Command__get_type,
	(gpointer) & impl_GDA_Command__set_type,
	(gpointer) & impl_GDA_Command_open,
};

/*
 * vepv structures
 */
static POA_GDA_Parameter__vepv impl_GDA_Parameter_vepv = {
	&impl_GDA_Parameter_base_epv,
	&impl_GDA_Parameter_epv,
};

static POA_GDA_Command__vepv impl_GDA_Command_vepv = {
	&impl_GDA_Command_base_epv,
	&impl_GDA_Command_epv,
};

/*
 * Stub implementations
 */
GDA_Parameter
impl_GDA_Parameter__create (PortableServer_POA poa, CORBA_Environment * ev) {
	GDA_Parameter retval;
	impl_POA_GDA_Parameter *newservant;
	PortableServer_ObjectId *objid;
	
	newservant = g_new0(impl_POA_GDA_Parameter, 1);
	newservant->servant.vepv = &impl_GDA_Parameter_vepv;
	newservant->poa = poa;
	POA_GDA_Parameter__init((PortableServer_Servant) newservant, ev);
	objid = PortableServer_POA_activate_object(poa, newservant, ev);
	CORBA_free(objid);
	retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);
	
	return retval;
}

/* You shouldn't call this routine directly without first deactivating the servant... */
void
impl_GDA_Parameter__destroy (impl_POA_GDA_Parameter * servant, CORBA_Environment * ev) {
	POA_GDA_Parameter__fini((PortableServer_Servant) servant, ev);
	g_free(servant);
}

CORBA_long
impl_GDA_Parameter_appendChunk (impl_POA_GDA_Parameter * servant,
                                GDA_Parameter_VarBinString * data,
                                CORBA_Environment * ev) {
	g_error("%s: not implemented", __PRETTY_FUNCTION__);
	return 0;
}

GDA_Command
impl_GDA_Command__create (PortableServer_POA poa,
                          GdaServerCommand* cmd,
                          CORBA_Environment * ev) {
	GDA_Command retval;
	impl_POA_GDA_Command *newservant;
	PortableServer_ObjectId *objid;
	
	newservant = g_new0(impl_POA_GDA_Command, 1);
	newservant->servant.vepv = &impl_GDA_Command_vepv;
	newservant->poa = poa;
	
	newservant->cmd = cmd;
	if (newservant->cmd) {
		//newservant->cmd->type = GDA_COMMAND_TYPE_TEXT; /* default command type */
	}
	
	POA_GDA_Command__init((PortableServer_Servant) newservant, ev);
	objid = PortableServer_POA_activate_object(poa, newservant, ev);
	CORBA_free(objid);
	retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

	return retval;
}

/* You shouldn't call this routine directly without first deactivating the servant... */
void
impl_GDA_Command__destroy (impl_POA_GDA_Command * servant, CORBA_Environment * ev) {
	POA_GDA_Command__fini((PortableServer_Servant) servant, ev);
	g_free(servant);
}

CORBA_long
impl_GDA_Command__get_cmdTimeout (impl_POA_GDA_Command * servant,
                                  CORBA_Environment * ev) {
	return 0;
}

void
impl_GDA_Command__set_cmdTimeout (impl_POA_GDA_Command * servant,
                                  CORBA_long value,
                                  CORBA_Environment * ev) {
}

CORBA_boolean
impl_GDA_Command__get_prepared (impl_POA_GDA_Command * servant, CORBA_Environment * ev) {
	g_error("%s: not implemented", __PRETTY_FUNCTION__);
	return 0;
}

CORBA_long
impl_GDA_Command__get_state (impl_POA_GDA_Command * servant, CORBA_Environment * ev) {
	g_error("%s: not implemented", __PRETTY_FUNCTION__);
	return 0;
}

void
impl_GDA_Command__set_state (impl_POA_GDA_Command * servant,
                             CORBA_long value,
                             CORBA_Environment * ev) {
}

CORBA_char *
impl_GDA_Command__get_text (impl_POA_GDA_Command * servant, CORBA_Environment * ev) {
	CORBA_char *retval;
	
	if (!servant->cmd->text)
		retval = 0;
	else
		retval = CORBA_string_dup(servant->cmd->text);
	return retval;
}

void
impl_GDA_Command__set_text (impl_POA_GDA_Command * servant,
                            CORBA_char * value,
                            CORBA_Environment * ev) {
	g_return_if_fail(!CORBA_Object_is_nil(servant, ev));
	gda_server_command_set_text(servant->cmd, (const gchar *) value);
}

GDA_CommandType
impl_GDA_Command__get_type (impl_POA_GDA_Command * servant, CORBA_Environment * ev) {
	CORBA_unsigned_long retval;
	retval = servant->cmd->type;
	return retval;
}

void
impl_GDA_Command__set_type (impl_POA_GDA_Command * servant,
                            GDA_CommandType value,
                            CORBA_Environment * ev) {
	g_return_if_fail(!CORBA_Object_is_nil(servant, ev));
	gda_server_command_set_type(servant->cmd, value);
}

GDA_Recordset
impl_GDA_Command_open (impl_POA_GDA_Command * servant,
                       GDA_CmdParameterSeq * param,
                       GDA_CursorType ct,
                       GDA_LockType lt,
                       CORBA_unsigned_long * affected,
                       CORBA_Environment * ev) {
	GDA_Recordset        retval;
	GdaServerRecordset* recset;
	GdaServerError*     error;
	gulong               laffected = 0;
	
	error = gda_server_error_new();
	recset = gda_server_command_execute(servant->cmd, error, param, &laffected, 0);
	if (!recset) {
		if (error->description) {
			GDA_DriverError* exception = GDA_DriverError__alloc();
			exception->errors._length = 1;
			exception->errors._buffer = CORBA_sequence_GDA_Error_allocbuf(1);
			exception->errors._buffer[0].description = CORBA_string_dup(error->description);
			exception->errors._buffer[0].number      = error->number;
			exception->errors._buffer[0].source      = CORBA_string_dup(error->source);
			exception->errors._buffer[0].sqlstate    = CORBA_string_dup(error->sqlstate);
			exception->errors._buffer[0].nativeMsg   = CORBA_string_dup(error->native);
			exception->realcommand = CORBA_string_dup("<Unknown>");
			CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GDA_DriverError, exception);
		}
		gda_server_error_free(error);
		return CORBA_OBJECT_NIL;
	}
	gda_server_error_free(error);
	if (affected) *affected = laffected;
	retval = impl_GDA_Recordset__create(servant->poa, recset, ev);
	gda_server_impl_exception(ev);
	return retval;
}

/**
 * gda_server_command_new
 */
GdaServerCommand *
gda_server_command_new (GdaServerConnection *cnc)
{
	GdaServerCommand* cmd;
	
	g_return_val_if_fail(cnc != NULL, NULL);
	
	cmd = g_new0(GdaServerCommand, 1);
	cmd->cnc = cnc;
	cmd->users = 1;
	
	if ((cmd->cnc->server_impl != NULL) &&
	    (cmd->cnc->server_impl->functions.command_new != NULL)) {
		cmd->cnc->server_impl->functions.command_new(cmd);
	}
	cmd->cnc->commands = g_list_append(cmd->cnc->commands, (gpointer) cmd);
	
	return cmd;
}

/**
 * gda_server_command_get_connection
 */
GdaServerConnection *
gda_server_command_get_connection (GdaServerCommand *cmd)
{
	g_return_val_if_fail(cmd != NULL, NULL);
	return cmd->cnc;
}

/**
 * gda_server_command_get_text
 */
gchar *
gda_server_command_get_text (GdaServerCommand *cmd)
{
	g_return_val_if_fail(cmd != NULL, NULL);
	return cmd->text;
}

/**
 * gda_server_command_set_text
 */
void
gda_server_command_set_text (GdaServerCommand *cmd, const gchar *text)
{
	g_return_if_fail(cmd != NULL);
	
	if (cmd->text) g_free((gpointer) cmd->text);
	if (text) cmd->text = g_strdup(text);
	else cmd->text = NULL;
}

/**
 * gda_server_command_get_type
 */
GDA_CommandType
gda_server_command_get_type (GdaServerCommand *cmd)
{
	g_return_val_if_fail(cmd != NULL, 0);
	return cmd->type;
}

/**
 * gda_server_command_set_type
 */
void
gda_server_command_set_type (GdaServerCommand *cmd, GDA_CommandType type)
{
	g_return_if_fail(cmd != NULL);
	cmd->type = type;
}

/**
 * gda_server_command_get_user_data
 */
gpointer
gda_server_command_get_user_data (GdaServerCommand *cmd)
{
	g_return_val_if_fail(cmd != NULL, NULL);
	return cmd->user_data;
}

/**
 * gda_server_command_set_user_data
 */
void
gda_server_command_set_user_data (GdaServerCommand *cmd, gpointer user_data)
{
	g_return_if_fail(cmd != NULL);
	cmd->user_data = user_data;
}

/**
 * gda_server_command_free
 */
void
gda_server_command_free (GdaServerCommand *cmd)
{
	g_return_if_fail(cmd != NULL);
	
	if ((cmd->cnc != NULL) &&
	    (cmd->cnc->server_impl != NULL) &&
	    (cmd->cnc->server_impl->functions.command_free != NULL)) {
		cmd->cnc->server_impl->functions.command_free(cmd);
	}
	
	cmd->users--;
	if (!cmd->users) {
		cmd->cnc->commands = g_list_remove(cmd->cnc->commands, (gpointer) cmd);
		if (cmd->text) g_free((gpointer) cmd->text);
		g_free((gpointer) cmd);
	}
}

/**
 * gda_server_command_execute
 */
GdaServerRecordset *
gda_server_command_execute (GdaServerCommand *cmd,
                            GdaServerError *error,
                            const GDA_CmdParameterSeq *params,
                            gulong *affected,
                            gulong options)
{
	GdaServerRecordset* recset;
	
	g_return_val_if_fail(cmd != NULL, NULL);
	g_return_val_if_fail(cmd->cnc != NULL, NULL);
	g_return_val_if_fail(cmd->cnc->server_impl != NULL, NULL);
	g_return_val_if_fail(cmd->cnc->server_impl->functions.command_execute != NULL, NULL);
	
	recset = cmd->cnc->server_impl->functions.command_execute(cmd, error, params,
	                                                          affected, options);
	return recset;
}

