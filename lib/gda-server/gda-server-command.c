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
#include "gda-server.h"
#include "gda-server-private.h"

static void gda_server_command_init       (GdaServerCommand *cmd);
static void gda_server_command_class_init (GdaServerCommandClass *klass);
static void gda_server_command_destroy    (GtkObject *object);

/*
 * Stub implementations
 */
CORBA_char *
impl_GDA_Command__get_text (PortableServer_Servant servant, CORBA_Environment * ev)
{
	CORBA_char *retval;
	GdaServerCommand *cmd = (GdaServerCommand *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_COMMAND (cmd), CORBA_OBJECT_NIL);
	
	if (cmd->text)
		retval = CORBA_string_dup ("");
	else
		retval = CORBA_string_dup (cmd->text);
	return retval;
}

void
impl_GDA_Command__set_text (PortableServer_Servant servant,
                            CORBA_char * value,
                            CORBA_Environment * ev)
{
	GdaServerCommand *cmd = (GdaServerCommand *) bonobo_x_object (servant);
	g_return_if_fail (GDA_IS_SERVER_COMMAND (cmd));
	gda_server_command_set_text (cmd, (const gchar *) value);
}

GDA_CommandType
impl_GDA_Command__get_type (PortableServer_Servant servant, CORBA_Environment * ev)
{
	GdaServerCommand *cmd = (GdaServerCommand *) bonobo_x_object (servant);
	g_return_val_if_fail (GDA_IS_SERVER_COMMAND (cmd), -1);
	return cmd->type;
}

void
impl_GDA_Command__set_type (PortableServer_Servant servant,
                            GDA_CommandType value,
                            CORBA_Environment * ev)
{
	GdaServerCommand *cmd = (GdaServerCommand *) bonobo_x_object (servant);
	g_return_if_fail (GDA_IS_SERVER_COMMAND (cmd));
	gda_server_command_set_cmd_type (cmd, value);
}

GDA_Recordset
impl_GDA_Command_open (PortableServer_Servant servant,
                       GDA_CmdParameterSeq * param,
                       GDA_CursorType ct,
                       GDA_LockType lt,
                       CORBA_unsigned_long * affected,
                       CORBA_Environment * ev)
{
	GdaError *error;
	GdaServerRecordset *recset;
	gulong laffected;
	GdaServerCommand *cmd = (GdaServerCommand *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_COMMAND (cmd), CORBA_OBJECT_NIL);
	
	error = gda_error_new ();
	recset = gda_server_command_execute (cmd, error, param, &laffected, 0);
	if (!GDA_IS_SERVER_RECORDSET (recset)) {
		gda_error_to_exception (error, ev);
		gda_error_free (error);
		return CORBA_OBJECT_NIL;
	}
	gda_error_free(error);
	if (affected)
		*affected = laffected;

	return bonobo_object_corba_objref (BONOBO_OBJECT (recset));
}

/*
 * GdaServerCommand class implementation
 */
static void
gda_server_command_class_init (GdaServerCommandClass *klass)
{
	POA_GDA_Command__epv *epv;
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = gda_server_command_destroy;

	/* set the epv */
	epv = &klass->epv;
	epv->_get_text = impl_GDA_Command__get_text;
	epv->_set_text = impl_GDA_Command__set_text;
	epv->_get_type = impl_GDA_Command__get_type;
	epv->_set_type = impl_GDA_Command__set_type;
	epv->open = impl_GDA_Command_open;
}

static void
gda_server_command_init (GdaServerCommand *cmd)
{
	cmd->cnc = NULL;
	cmd->text = NULL;
	cmd->type = 0;
	cmd->user_data = NULL;
}

static void
gda_server_command_destroy (GtkObject *object)
{
	GtkObjectClass *parent_class;
	GdaServerCommand *cmd = (GdaServerCommand *) object;

	g_return_if_fail (GDA_IS_SERVER_COMMAND (cmd));

	if ((cmd->cnc != NULL) &&
	    (cmd->cnc->server_impl != NULL) &&
	    (cmd->cnc->server_impl->functions.command_free != NULL)) {
		cmd->cnc->server_impl->functions.command_free(cmd);
	}

	cmd->cnc->commands = g_list_remove(cmd->cnc->commands, (gpointer) cmd);
	if (cmd->text)
		g_free((gpointer) cmd->text);

	parent_class = gtk_type_class (BONOBO_X_OBJECT_TYPE);
	if (parent_class && parent_class->destroy)
		parent_class->destroy (object);
		
}

GtkType
gda_server_command_get_type (void)
{
        static GtkType type = 0;

        if (!type) {
                GtkTypeInfo info = {
                        "GdaServerCommand",
                        sizeof (GdaServerCommand),
                        sizeof (GdaServerCommandClass),
                        (GtkClassInitFunc) gda_server_command_class_init,
                        (GtkObjectInitFunc) gda_server_command_init,
                        (GtkArgSetFunc) NULL,
                        (GtkArgSetFunc) NULL
                };
                type = bonobo_x_type_unique(
                        BONOBO_X_OBJECT_TYPE,
                        POA_GDA_Command__init, NULL,
                        GTK_STRUCT_OFFSET (GdaServerCommandClass, epv),
                        &info);
        }
        return type;
}

/**
 * gda_server_command_new
 */
GdaServerCommand *
gda_server_command_new (GdaServerConnection *cnc)
{
	GdaServerCommand* cmd;
	
	g_return_val_if_fail(GDA_IS_SERVER_CONNECTION (cnc), NULL);
	
	cmd = GDA_SERVER_COMMAND (gtk_type_new (gda_server_command_get_type ()));
	cmd->cnc = cnc;

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
 * gda_server_command_get_cmd_type
 */
GDA_CommandType
gda_server_command_get_cmd_type (GdaServerCommand *cmd)
{
	g_return_val_if_fail(cmd != NULL, 0);
	return cmd->type;
}

/**
 * gda_server_command_set_cmd_type
 */
void
gda_server_command_set_cmd_type (GdaServerCommand *cmd, GDA_CommandType type)
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
	bonobo_object_unref (BONOBO_OBJECT (cmd));
}

/**
 * gda_server_command_execute
 */
GdaServerRecordset *
gda_server_command_execute (GdaServerCommand *cmd,
                            GdaError *error,
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
