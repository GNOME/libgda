/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999,2000 Rodrigo Moya
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

#include "config.h"
#include "gda-command.h"

enum
{
  GDA_COMMAND_LAST_SIGNAL
};

typedef struct _Parameter
{
  gchar* name;
  GDA_Value* value;
  GDA_ParameterDirection inout;
} Parameter;

static gint gda_command_signals[GDA_COMMAND_LAST_SIGNAL] = { 0, };

#ifdef HAVE_GOBJECT
static void gda_command_class_init (Gda_CommandClass *klass, gpointer data);
static void gda_command_init       (Gda_Command *cmd, Gda_CommandClass *klass);
#else
static void gda_command_class_init (Gda_CommandClass *klass);
static void gda_command_init       (Gda_Command *cmd);
#endif

static void
release_connection_object (Gda_Command* cmd, Gda_Connection* cnc)
{
  cmd->connection->commands = g_list_remove(cmd->connection->commands, cmd);
}

#ifdef HAVE_GOBJECT
GType
gda_command_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      GTypeInfo info =
      {
        sizeof (Gda_CommandClass),               /* class_size */
	NULL,                                    /* base_init */
	NULL,                                    /* base_finalize */
        (GClassInitFunc) gda_command_class_init, /* class_init */
	NULL,                                    /* class_finalize */
	NULL,                                    /* class_data */
        sizeof (Gda_Command),                    /* instance_size */
	0,                                       /* n_preallocs */
        (GInstanceInitFunc) gda_command_init,    /* instance_init */
	NULL,                                    /* value_table */
      };
      type = g_type_register_static (G_TYPE_OBJECT, "Gda_Command", &info);
    }
  return type;
}
#else
guint
gda_command_get_type (void)
{
  static guint gda_command_type = 0;

  if (!gda_command_type)
    {
      GtkTypeInfo gda_command_info =
      {
        "GdaCommand",
        sizeof (Gda_Command),
        sizeof (Gda_CommandClass),
        (GtkClassInitFunc) gda_command_class_init,
        (GtkObjectInitFunc) gda_command_init,
        (GtkArgSetFunc)NULL,
        (GtkArgSetFunc)NULL,
      };
      gda_command_type = gtk_type_unique(gtk_object_get_type(), &gda_command_info);
    }
  return gda_command_type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_command_class_init (Gda_CommandClass *klass, gpointer data)
{
}
#else
static void
gda_command_class_init (Gda_CommandClass* klass)
{
  GtkObjectClass* object_class;

  object_class = (GtkObjectClass*) klass;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_command_init (Gda_Command *cmd, Gda_CommandClass *klass)
#else
gda_command_init (Gda_Command* cmd)
#endif
{
  g_return_if_fail(IS_GDA_COMMAND(cmd));
}

/**
 * gda_command_new:
 *
 * This function allocates the memory for a command object.
 *
 * Returns: a pointer to the command object
 */
Gda_Command *
gda_command_new (void)
{
#ifdef HAVE_GOBJECT
  return GDA_COMMAND (g_object_new (GDA_TYPE_COMMAND, NULL));
#else
  return GDA_COMMAND(gtk_type_new(gda_command_get_type()));
#endif
}

/**
 * gda_command_free:
 * @cmd: The command object which should be deallocated.
 *
 * This function frees the memory of command object and
 * cuts the association with its connection object.
 *
 */
void
gda_command_free (Gda_Command* cmd)
{
  CORBA_Environment ev;
  
  g_return_if_fail(IS_GDA_COMMAND(cmd));

  if (cmd->connection && cmd->connection->commands)
    release_connection_object(cmd, cmd->connection);
  else
    {
      if (cmd->connection && !cmd->connection->commands)
        g_error("gda_command_free: connection object has no command list");
    }
  if (cmd->text) g_free(cmd->text);

  CORBA_exception_init(&ev);
  if (!CORBA_Object_is_nil(cmd->command, &ev))
    {
      CORBA_Object_release(cmd->command, &ev);
      gda_connection_corba_exception(gda_command_get_connection(cmd), &ev);
    }
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
gint
gda_command_set_connection (Gda_Command* cmd, Gda_Connection* cnc)
{
  CORBA_Environment ev;

  g_return_val_if_fail(IS_GDA_COMMAND(cmd), -1);
  g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
  g_return_val_if_fail(cnc->connection != 0, -1);
  
  if (cmd->connection)
    {
      release_connection_object(cmd, cmd->connection);
    }
  cmd->connection = cnc;
  CORBA_exception_init(&ev);
  
  cmd->command = GDA_Connection_createCommand(cnc->connection, &ev);
  if (gda_connection_corba_exception(gda_command_get_connection(cmd), &ev) < 0)
    {
      cmd->connection = 0;
      return -1;
    }
  cmd->connection->commands = g_list_append(cmd->connection->commands, cmd);
  if (cmd->text_pending)
    {
      GDA_Command__set_text(cmd->command, cmd->text, &ev);
      gda_connection_corba_exception(gda_command_get_connection(cmd), &ev);
      cmd->text_pending = 0;
    }
      
  return 0;
}
  
/**
 * gda_command_get_connection:
 * @cmd: the command object
 *
 * Returns the #gda_Connection object which is used by the command.
 *
 * Returns: a pointer to the #gda_Connection object
 */
Gda_Connection *
gda_command_get_connection (Gda_Command* cmd)
{
  g_return_val_if_fail(IS_GDA_COMMAND(cmd), 0);

  return cmd->connection;
}

/**
 * gda_command_set_text:
 * @cmd: the #Gda_Command object
 * @text: the command to perform. There are some special texts
 * which are reckognized by the servers. See the server documantation
 * for a list of special commands.
 *
 * Sets the command which is executed when the gda_command_execute()
 * function is called.
 *
 */
void
gda_command_set_text (Gda_Command* cmd, gchar* text)
{
  CORBA_Environment ev;
  
  g_return_if_fail(IS_GDA_COMMAND(cmd));
  
  if (cmd->text)
    g_free(cmd->text);
  cmd->text = g_strdup(text);
  if (!cmd->command)
    {
      g_print("No CORBA command yet allocated, setting to pending");
      cmd->text_pending = 1;
    }
  else
    {
      CORBA_exception_init(&ev);
      GDA_Command__set_text(cmd->command, text, &ev);
      gda_connection_corba_exception(gda_command_get_connection(cmd), &ev);
    }
}

/**
 * gda_command_get_text:
 * @cmd: the #Gda_Command object
 *
 * Gets the command string which is executed when the gda_command_execute()
 * function is called.
 *
 * Returns: a reference to the command string.
 */
gchar *
gda_command_get_text (Gda_Command* cmd)
{
  gchar* txt;
  CORBA_Environment ev;
  
  g_return_val_if_fail(IS_GDA_COMMAND(cmd), 0);
  
  if (!cmd->command)
    {
      g_print("No CORBA command_yet allocated, using pending value\n");
      return cmd->text;
    }
  CORBA_exception_init(&ev);
  txt = GDA_Command__get_text(cmd->command, &ev);
  return txt;
}

/**
 * gda_command_set_cmd_type:
 * @cmd: the #Gda_Command object
 * @type: the type of the command. See the provider specification
 * which command type is understood and the semantics of the different 
 * types.
 *
 * Sets the command which is executed when the gda_command_execute()
 * function is called.
 *
 */
void
gda_command_set_cmd_type (Gda_Command* cmd, gulong type)
{
  CORBA_Environment ev;
  
  g_return_if_fail(IS_GDA_COMMAND(cmd));
  
  if (!cmd->command)
    {
      g_print("No CORBA command yet allocated, setting to pending");
      cmd->type_pending = 1;
      cmd->type = type;
    }
  else
    {
      CORBA_exception_init(&ev);
      GDA_Command__set_type(cmd->command, type, &ev);
      gda_connection_corba_exception(gda_command_get_connection(cmd), &ev);
    }
}

/**
 * gda_command_get_cmd_type:
 * @cmd: the #Gda_Command object
 *
 * Gets the type of the command 
 *
 * Returns: the type of the command
 */
gulong
gda_command_get_cmd_type (Gda_Command* cmd)
{
  CORBA_unsigned_long type;
  CORBA_Environment ev;
  
  g_return_val_if_fail(IS_GDA_COMMAND(cmd), 0);
  
  if (!cmd->command)
    {
      g_print("No CORBA command_yet allocated, using pending value\n");
      return cmd->type;
    }
  CORBA_exception_init(&ev);
  type = GDA_Command__get_type(cmd->command, &ev);
  return type;
}


GDA_CmdParameterSeq*
__gda_command_get_params (Gda_Command* cmd)
{
  GDA_CmdParameterSeq*      corba_parameters;
  gint                      parameter_count;
  GList*                    ptr;
  GDA_CmdParameter*         corba_parameter;
  gint                      idx;

  corba_parameters = GDA_CmdParameterSeq__alloc();
  parameter_count = cmd->parameters ? g_list_length(cmd->parameters) : 0;
  corba_parameters->_buffer = CORBA_sequence_GDA_CmdParameter_allocbuf(parameter_count);
  corba_parameters->_length = parameter_count;

  if (parameter_count)
    {
      ptr = cmd->parameters;
      idx = 0;
      while(ptr)
        {
          GDA_Value* value;
          Parameter* parameter;
	  
          parameter = ptr->data;
          corba_parameter = &(corba_parameters->_buffer[idx]);

          corba_parameter->dir = parameter->inout;
          if (parameter->name)
            {
              corba_parameter->name = CORBA_string_dup(parameter->name);
            }
          else corba_parameter->name = 0;

          value = parameter->value;
          if (!(corba_parameter->value._d = value ? 0 : 1))
            {
              corba_parameter->value._u.v = *((*parameter).value);
            }
          else
            {
              g_print("Got NULL param value\n");
            }
          ptr = g_list_next(ptr);
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
 * gda_command_set_text(). It returns a #Gda_Recordset pointer which holds the
 * results from this query. If the command doesn't return any results, like
 * insert, or updaste statements in SQL, an empty result set is returnd.
 *
 * Returns: a pointer to a recordset or a NULL pointer if there was an error.
 */
Gda_Recordset *
gda_command_execute (Gda_Command* cmd, gulong* reccount, gulong flags)
{
  gint rc;
  Gda_Recordset* rs;
  
  g_return_val_if_fail(IS_GDA_COMMAND(cmd), 0);
  g_return_val_if_fail(reccount != NULL, 0);
  g_return_val_if_fail(cmd->connection != NULL, 0);

  rs = GDA_RECORDSET(gda_recordset_new());
  rc = gda_recordset_open(rs, cmd, GDA_OPEN_FWDONLY, GDA_LOCK_OPTIMISTIC, flags);
  if (rc < 0)
    {
      gda_recordset_free(rs);
      return 0;
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
gda_command_create_parameter (Gda_Command* cmd,
                              gchar* name,
                              GDA_ParameterDirection inout,
                              GDA_Value* value)
{
  Parameter* param;
  
  g_return_if_fail(IS_GDA_COMMAND(cmd));
  
  param = g_new0(Parameter, 1);

  param->name = g_strdup(name);
  param->inout = inout;
  param->value = value;

  cmd->parameters = g_list_append(cmd->parameters, param);
}

/**
 * gda_command_get_timeout
 * @cmd the command object
 *
 * This function returns the timeout associated with the given
 * command object
 */
glong
gda_command_get_timeout (Gda_Command *cmd)
{
  CORBA_Environment ev;
  glong ret;
  
  g_return_val_if_fail(IS_GDA_COMMAND(cmd), -1);
  
  CORBA_exception_init(&ev);
  ret = GDA_Command__get_cmdTimeout(cmd->command, &ev);
  gda_connection_corba_exception(gda_command_get_connection(cmd), &ev);
  return (ret);
}

/**
 * gda_command_set_timeout
 * @cmd the command object
 * @timeout timeout
 *
 * Sets the timeout for the given command
 */
void
gda_command_set_timeout (Gda_Command *cmd, glong timeout)
{
  CORBA_Environment ev;
  
  g_return_if_fail(IS_GDA_COMMAND(cmd));
  
  CORBA_exception_init(&ev);
  GDA_Command__set_cmdTimeout(cmd->command, (CORBA_long) timeout, &ev);
  gda_connection_corba_exception(gda_command_get_connection(cmd), &ev);
}
