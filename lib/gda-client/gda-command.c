/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999,2000 Rodrigo Moya
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
#include "gda-command.h"
#include "gda-corba.h"

enum
{
	GDA_COMMAND_LAST_SIGNAL
};

typedef struct _Parameter
{
	gchar *name;
	GDA_Value *value;
	GDA_ParameterDirection inout;
}
Parameter;

static gint gda_command_signals[GDA_COMMAND_LAST_SIGNAL] = { 0, };

#ifdef HAVE_GOBJECT
static void gda_command_class_init (GdaCommandClass * klass, gpointer data);
static void gda_command_init (GdaCommand * cmd, GdaCommandClass * klass);
#else
static void gda_command_class_init (GdaCommandClass * klass);
static void gda_command_init (GdaCommand * cmd);
static void gda_command_destroy (GtkObject * object);
#endif

static void
release_connection_object (GdaCommand * cmd, GdaConnection * cnc)
{
	g_return_if_fail (GDA_IS_COMMAND (cmd));
	g_return_if_fail (GDA_IS_CONNECTION (cmd->connection));

	cmd->connection->commands =
		g_list_remove (cmd->connection->commands, cmd);
}

#ifdef HAVE_GOBJECT
GType
gda_command_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GdaCommandClass),
			NULL,
			NULL,
			(GClassInitFunc) gda_command_class_init,
			NULL,
			NULL,
			sizeof (GdaCommand),
			0,
			(GInstanceInitFunc) gda_command_init,
			NULL,
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaCommand",
					       &info, 0);
	}
	return type;
}
#else
guint
gda_command_get_type (void)
{
	static guint gda_command_type = 0;

	if (!gda_command_type) {
		GtkTypeInfo gda_command_info = {
			"GdaCommand",
			sizeof (GdaCommand),
			sizeof (GdaCommandClass),
			(GtkClassInitFunc) gda_command_class_init,
			(GtkObjectInitFunc) gda_command_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL,
		};
		gda_command_type =
			gtk_type_unique (gtk_object_get_type (),
					 &gda_command_info);
	}
	return gda_command_type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_command_class_init (GdaCommandClass * klass, gpointer data)
{
}
#else
static void
gda_command_class_init (GdaCommandClass * klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	object_class->destroy = gda_command_destroy;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_command_init (GdaCommand * cmd, GdaCommandClass * klass)
#else
gda_command_init (GdaCommand * cmd)
#endif
{
	g_return_if_fail (GDA_IS_COMMAND (cmd));

	cmd->command = CORBA_OBJECT_NIL;
	cmd->orb = gda_corba_get_orb ();
	cmd->connection = NULL;
	cmd->text = NULL;
	cmd->type = GDA_TypeNull;
	cmd->parameters = NULL;
	cmd->text_pending = cmd->type_pending = FALSE;
}

/**
 * gda_command_new:
 *
 * This function allocates the memory for a command object.
 *
 * Returns: a pointer to the command object
 */
GdaCommand *
gda_command_new (void)
{
#ifdef HAVE_GOBJECT
	return GDA_COMMAND (g_object_new (GDA_TYPE_COMMAND, NULL));
#else
	return GDA_COMMAND (gtk_type_new (GDA_TYPE_COMMAND));
#endif
}

static void
gda_command_destroy (GtkObject * object)
{
	CORBA_Environment ev;
	GtkObjectClass *parent_class;
	GdaCommand *cmd = (GdaCommand *) object;

	g_return_if_fail (GDA_IS_COMMAND (cmd));

	if (cmd->connection && cmd->connection->commands)
		release_connection_object (cmd, cmd->connection);
	else {
		if (cmd->connection && !cmd->connection->commands)
			g_error ("gda_command_free: connection object has no command list");
	}
	if (cmd->text)
		g_free (cmd->text);

	CORBA_exception_init (&ev);
	if (!CORBA_Object_is_nil (cmd->command, &ev)) {
		CORBA_Object_release (cmd->command, &ev);
		gda_connection_corba_exception (gda_command_get_connection
						(cmd), &ev);
	}
	CORBA_exception_free (&ev);

	parent_class = gtk_type_class (gtk_object_get_type ());
	if (parent_class && parent_class->destroy)
		parent_class->destroy (object);
}

/**
 * gda_command_free:
 * @cmd: The command object which should be deallocated.
 *
 * This function frees the memory of command object and
 * cuts the association with its connection object.
 */
void
gda_command_free (GdaCommand * cmd)
{
#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (cmd));
#else
	gtk_object_destroy (GTK_OBJECT (cmd));
#endif
}

/**
 * gda_command_set_connection:
 * @cmd: The command object
 * @cnc: The connection object
 *
 * Associates a connection with a command. All functions with this
 * command will use the connection to talk to the data source. If the
 * command is already associated with a connection, this association
 * is destroyed.  
 *
 * Returns: -1 on error, 0 on success
 */
void
gda_command_set_connection (GdaCommand * cmd, GdaConnection * cnc)
{
	CORBA_Environment ev;

	g_return_if_fail (GDA_IS_COMMAND (cmd));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->connection != 0);

	/* clean up */

	if (cmd->connection) {
		release_connection_object (cmd, cmd->connection);
	}
	cmd->connection = cnc;

	CORBA_exception_init (&ev);
	if (cmd->command != CORBA_OBJECT_NIL) {
		CORBA_Object_release (cmd->command, &ev);
		cmd->command = NULL;
	}

	/* get a new GDA::Command object */
	cmd->command = GDA_Connection_createCommand (cnc->connection, &ev);
	if (gda_connection_corba_exception
	    (gda_command_get_connection (cmd), &ev) < 0) {
		cmd->connection = NULL;
		cmd->command = CORBA_OBJECT_NIL;
		return;
	}
	cmd->connection->commands =
		g_list_append (cmd->connection->commands, cmd);
	if (cmd->text_pending) {
		GDA_Command__set_text (cmd->command, cmd->text, &ev);
		gda_connection_corba_exception (gda_command_get_connection
						(cmd), &ev);
		cmd->text_pending = 0;
	}
}

/**
 * gda_command_get_connection:
 * @cmd: the command object
 *
 * Returns the #gda_Connection object which is used by the command.
 *
 * Returns: a pointer to the #gda_Connection object
 */
GdaConnection *
gda_command_get_connection (GdaCommand * cmd)
{
	g_return_val_if_fail (GDA_IS_COMMAND (cmd), NULL);
	return cmd->connection;
}

/**
 * gda_command_set_text:
 * @cmd: the #GdaCommand object
 * @text: the command to perform. There are some special texts
 * which are reckognized by the servers. See the server documantation
 * for a list of special commands.
 *
 * Sets the command which is executed when the gda_command_execute()
 * function is called.
 *
 */
void
gda_command_set_text (GdaCommand * cmd, gchar * text)
{
	CORBA_Environment ev;

	g_return_if_fail (GDA_IS_COMMAND (cmd));

	if (cmd->text)
		g_free (cmd->text);
	cmd->text = g_strdup (text);
	if (!cmd->command) {
		g_print ("No CORBA command yet allocated, setting to pending");
		cmd->text_pending = 1;
	}
	else {
		CORBA_exception_init (&ev);
		GDA_Command__set_text (cmd->command, text, &ev);
		gda_connection_corba_exception (gda_command_get_connection
						(cmd), &ev);
	}
}

/**
 * gda_command_get_text:
 * @cmd: the #GdaCommand object
 *
 * Gets the command string which is executed when the gda_command_execute()
 * function is called.
 *
 * Returns: a reference to the command string.
 */
gchar *
gda_command_get_text (GdaCommand * cmd)
{
	gchar *txt;
	CORBA_Environment ev;

	g_return_val_if_fail (GDA_IS_COMMAND (cmd), 0);

	if (!cmd->command) {
		g_print ("No CORBA command_yet allocated, using pending value\n");
		return cmd->text;
	}
	CORBA_exception_init (&ev);
	txt = GDA_Command__get_text (cmd->command, &ev);
	return txt;
}

/**
 * gda_command_set_cmd_type:
 * @cmd: the #GdaCommand object
 * @type: the type of the command. See the provider specification
 * which command type is understood and the semantics of the different 
 * types.
 *
 * Sets the command which is executed when the gda_command_execute()
 * function is called.
 *
 */
void
gda_command_set_cmd_type (GdaCommand * cmd, GDA_CommandType type)
{
	CORBA_Environment ev;

	g_return_if_fail (GDA_IS_COMMAND (cmd));

	if (!cmd->command) {
		g_print ("No CORBA command yet allocated, setting to pending");
		cmd->type_pending = 1;
		cmd->type = type;
	}
	else {
		CORBA_exception_init (&ev);
		GDA_Command__set_type (cmd->command, type, &ev);
		gda_connection_corba_exception (gda_command_get_connection
						(cmd), &ev);
	}
}

/**
 * gda_command_get_cmd_type:
 * @cmd: the #GdaCommand object
 *
 * Gets the type of the command 
 *
 * Returns: the type of the command
 */
GDA_CommandType
gda_command_get_cmd_type (GdaCommand * cmd)
{
	GDA_CommandType type;
	CORBA_Environment ev;

	g_return_val_if_fail (GDA_IS_COMMAND (cmd), 0);

	if (!cmd->command) {
		g_print ("No CORBA command_yet allocated, using pending value\n");
		return cmd->type;
	}
	CORBA_exception_init (&ev);
	type = GDA_Command__get_type (cmd->command, &ev);
	gda_connection_corba_exception (gda_command_get_connection (cmd),
					&ev);
	CORBA_exception_free (&ev);

	return type;
}


GDA_CmdParameterSeq *
__gda_command_get_params (GdaCommand * cmd)
{
	GDA_CmdParameterSeq *corba_parameters;
	gint parameter_count;
	GList *ptr;
	GDA_CmdParameter *corba_parameter;
	gint idx;

	corba_parameters = GDA_CmdParameterSeq__alloc ();
	parameter_count =
		cmd->parameters ? g_list_length (cmd->parameters) : 0;
	corba_parameters->_buffer =
		CORBA_sequence_GDA_CmdParameter_allocbuf (parameter_count);
	corba_parameters->_length = parameter_count;

	if (parameter_count) {
		ptr = cmd->parameters;
		idx = 0;
		while (ptr) {
			GDA_Value *value;
			Parameter *parameter;

			parameter = ptr->data;
			corba_parameter = &(corba_parameters->_buffer[idx]);
			corba_parameter->dir = parameter->inout;
			if (parameter->name) {
				corba_parameter->name =
					CORBA_string_dup (parameter->name);
			}
			else
				corba_parameter->name = 0;

			value = parameter->value;
			if (!(corba_parameter->value._d = value ? 0 : 1)) {
				corba_parameter->value._u.v =
					*((*parameter).value);
			}
			else {
				g_print ("Got NULL param value\n");
			}
			ptr = g_list_next (ptr);
			idx++;
		}
	}
	return corba_parameters;
}

/**
 * gda_command_execute:
 * @cmd: the command object
 * @reccount: a pointer to a gulong variable. In this variable the number
 * of affected records is stored.
 * @flags: flags to categorize the command.
 *
 * This function executes the command which has been set with
 * gda_command_set_text(). It returns a #GdaRecordset pointer which holds the
 * results from this query. If the command doesn't return any results, like
 * insert, or updaste statements in SQL, an empty result set is returnd.
 *
 * Returns: a pointer to a recordset or a NULL pointer if there was an error.
 */
GdaRecordset *
gda_command_execute (GdaCommand * cmd, gulong * reccount, gulong flags)
{
	gint rc;
	GdaRecordset *rs;

	g_return_val_if_fail (GDA_IS_COMMAND (cmd), 0);
	g_return_val_if_fail (reccount != NULL, 0);
	g_return_val_if_fail (cmd->connection != NULL, 0);

	rs = GDA_RECORDSET (gda_recordset_new ());
	rc = gda_recordset_open (rs, cmd, GDA_OPEN_FWDONLY,
				 GDA_LOCK_OPTIMISTIC, flags);
	if (rc < 0) {
		gda_recordset_free (rs);
		return NULL;
	}
	*reccount = rs->affected_rows;
	return rs;
}

/**
 * gda_command_create_parameter:
 * @cmd: the command object
 * @name: the name of the parameter
 * @inout: direction of the parameter
 * @value: value of the parameter
 *
 * This function creates a new parameter for the command. This is 
 * important if you want to use parameterized statements like
 * "select * from table where key = ?"
 * and substitute the value for key at a later time. It also comes
 * handy if you don't want to convert each parameter to it's string
 * representation (floating point values).
 *
 * The @inout parameter defines if the @value parameter is used to
 * pass a value to the statement or to receive a value. In the exampel 
 * abov the @input parameter will have a value of #PARAM_IN. The value 
 * #PARAM_OUT is used for output parameters when the command executes
 * a precoedure.
 *
 * @value points to a GDA_Value variable (there will be helper functions
 * to create such varbales from the basic C types). This variable must
 * hold a valid value when the gda_command_execute() function is called
 * and the @inout parameter is either #PARAM_IN or #PARAM_INOUT.
 * If the @inoput parameter is #PARAM_OUT, memory is allocated by the
 * library and the user must free it using CORBA_free().
 *
 * IMPORTANT: use the CORBA functions to allocate the memory for the
 * value parameter if it's not a simple data type (integer, float, ...)
 */
void
gda_command_create_parameter (GdaCommand * cmd,
			      gchar * name,
			      GDA_ParameterDirection inout, GDA_Value * value)
{
	Parameter *param;

	g_return_if_fail (GDA_IS_COMMAND (cmd));

	param = g_new0 (Parameter, 1);
	param->name = g_strdup (name);
	param->inout = inout;
	param->value = value;

	cmd->parameters = g_list_append (cmd->parameters, param);
}
