/* GDA Common Library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <libgda/gda-command.h>

/**
 * gda_command_new
 * @text: the text of the command.
 * @type: a #GdaCommandType value.
 * @options: a #GdaCommandOptions value.
 *
 * Creates a new #GdaCommand from the parameters that should be freed by
 * calling #gda_command_free.
 *
 * If there are conflicting options, this will set @options to
 * #GDA_COMMAND_OPTION_DEFAULT.
 *
 * Returns: a newly allocated #GdaCommand.
 */
GdaCommand *
gda_command_new (const gchar *text, GdaCommandType type, 
				    GdaCommandOptions options)
{
	GdaCommand *cmd;

	cmd = GNOME_Database_Command__alloc ();
	gda_command_set_text (cmd, text);
	gda_command_set_command_type (cmd, type);
	cmd->options = GDA_COMMAND_OPTION_BAD_OPTION;
	gda_command_set_options (cmd, options);
	if (cmd->options == GDA_COMMAND_OPTION_BAD_OPTION)
		cmd->options = GDA_COMMAND_DEFAULT_OPTION;

	return cmd;
}

/**
 * gda_command_free
 * @cmd: a #GdaCommand.
 *
 * Frees the resources allocated by #gda_command_new.
 */
void
gda_command_free (GdaCommand *cmd)
{
	g_return_if_fail (cmd != NULL);
	CORBA_free (cmd);
}

/**
 * gda_command_get_text
 * @cmd: a #GdaCommand.
 *
 * Get the command text held by @cmd.
 * 
 * Returns: the command string of @cmd.
 */
const gchar *
gda_command_get_text (GdaCommand *cmd)
{
	g_return_val_if_fail (cmd != NULL, NULL);
	return (const gchar *) cmd->text;
}

/**
 * gda_command_set_text
 * @cmd: a #GdaCommand
 * @text: the command text.
 *
 * Sets the command text of @cmd.
 */
void
gda_command_set_text (GdaCommand *cmd, const gchar *text)
{
	g_return_if_fail (cmd != NULL);

	if (cmd->text != NULL) {
		CORBA_free (cmd->text);
		cmd->text = NULL;
	}

	if (text != NULL)
		cmd->text = CORBA_string_dup (text);
}

/**
 * gda_command_get_command_type
 * @cmd: a #GdaCommand.
 *
 * Gets the command type of @cmd.
 * 
 * Returns: the command type of @cmd.
 */
GdaCommandType
gda_command_get_command_type (GdaCommand *cmd)
{
	g_return_val_if_fail (cmd != NULL, GDA_COMMAND_TYPE_INVALID);
	return cmd->type;
}

/**
 * gda_command_set_command_type
 * @cmd: a #GdaCommand
 * @type: the command type.
 *
 * Sets the command type of @cmd.
 */
void
gda_command_set_command_type (GdaCommand *cmd, GdaCommandType type)
{
	g_return_if_fail (cmd != NULL);
	cmd->type = type;
}

/**
 * gda_command_get_options
 * @cmd: a #GdaCommand.
 *
 * Gets the command options of @cmd.
 * 
 * Returns: the command options of @cmd.
 */
GdaCommandOptions
gda_command_get_options (GdaCommand *cmd)
{
	g_return_val_if_fail (cmd != NULL, GDA_COMMAND_OPTION_BAD_OPTION);
	return cmd->options;
}

/**
 * gda_command_set_options
 * @cmd: a #GdaCommand
 * @options: the command options.
 *
 * Sets the command options of @cmd. If there conflicting options, it will just
 * leave the value as before.
 */
void
gda_command_set_options (GdaCommand *cmd, GdaCommandOptions options)
{
	GdaCommandOptions err_mask;

	g_return_if_fail (cmd != NULL);

	err_mask = GDA_COMMAND_OPTION_IGNORE_ERRORS |
		   GDA_COMMAND_OPTION_STOP_ON_ERRORS;

	if ((options & err_mask) == err_mask)
		return;	// Conflicting options!

	cmd->options = options;
}

