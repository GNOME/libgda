/* GDA Common Library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gmem.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>

#undef GDA_DISABLE_DEPRECATED
#include <libgda/gda-command.h>

GType
gda_command_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0)
		our_type = g_boxed_type_register_static ("GdaCommand",
			(GBoxedCopyFunc) gda_command_copy,
			(GBoxedFreeFunc) gda_command_free);
	return our_type;
}

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
 *
 * Deprecated: 3.2:
 */
GdaCommand *
gda_command_new (const gchar *text, GdaCommandType type, GdaCommandOptions options)
{
	GdaCommand *cmd;

	cmd = g_new0 (GdaCommand, 1);
	gda_command_set_text (cmd, text);
	gda_command_set_command_type (cmd, type);

	cmd->options = GDA_COMMAND_OPTION_IGNORE_ERRORS;
	gda_command_set_options (cmd, options);

	return cmd;
}

/**
 * gda_command_free
 * @cmd: a #GdaCommand.
 *
 * Frees the resources allocated by #gda_command_new.
 *
 * Deprecated: 3.2:
 */
void
gda_command_free (GdaCommand *cmd)
{
	g_return_if_fail (cmd != NULL);

	g_free (cmd->text);
	g_free (cmd);
}

/**
 * gda_command_copy
 * @cmd: command to get a copy from.
 *
 * Creates a new #GdaCommand from an existing one.
 * 
 * Returns: a newly allocated #GdaCommand with a copy of the data in @cmd.
 *
 * Deprecated: 3.2:
 */
GdaCommand *
gda_command_copy (GdaCommand *cmd)
{
	GdaCommand *new_cmd;
	
	g_return_val_if_fail (cmd != NULL, NULL);
	new_cmd = gda_command_new (gda_command_get_text (cmd),
				   gda_command_get_command_type (cmd),
				   gda_command_get_options (cmd));
	return new_cmd;
}

/**
 * gda_command_get_text
 * @cmd: a #GdaCommand.
 *
 * Gets the command text held by @cmd.
 * 
 * Returns: the command string of @cmd.
 *
 * Deprecated: 3.2:
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
 *
 * Deprecated: 3.2:
 */
void
gda_command_set_text (GdaCommand *cmd, const gchar *text)
{
	g_return_if_fail (cmd != NULL);

	if (cmd->text != NULL) {
		g_free (cmd->text);
		cmd->text = NULL;
	}

	if (text != NULL)
		cmd->text = g_strdup (text);
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
 *
 * Deprecated: 3.2:
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
 *
 * Deprecated: 3.2:
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
 * Sets the command options of @cmd. 
 *
 * Deprecated: 3.2:
 */
void
gda_command_set_options (GdaCommand *cmd, GdaCommandOptions options)
{
	g_return_if_fail (cmd != NULL);

	if (options & GDA_COMMAND_OPTION_STOP_ON_ERRORS)
		cmd->options = GDA_COMMAND_OPTION_STOP_ON_ERRORS;
	else
		cmd->options = GDA_COMMAND_OPTION_IGNORE_ERRORS;
}
