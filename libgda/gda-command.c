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
 */
GdaCommand *
gda_command_new (const gchar *text, GdaCommandType type)
{
	GdaCommand *cmd;

	cmd = GNOME_Database_Command__alloc ();
	gda_command_set_text (cmd, text);
	gda_command_set_command_type (cmd, type);

	return cmd;
}

/**
 * gda_command_free
 */
void
gda_command_free (GdaCommand *cmd)
{
	g_return_if_fail (cmd != NULL);
	CORBA_free (cmd);
}

/**
 * gda_command_get_text
 */
const gchar *
gda_command_get_text (GdaCommand *cmd)
{
	g_return_val_if_fail (cmd != NULL, NULL);
	return (const gchar *) cmd->text;
}

/**
 * gda_command_set_text
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
 */
GdaCommandType
gda_command_get_command_type (GdaCommand *cmd)
{
	g_return_val_if_fail (cmd != NULL, GDA_COMMAND_TYPE_INVALID);
	return cmd->type;
}

/**
 * gda_command_set_command_type
 */
void
gda_command_set_command_type (GdaCommand *cmd, GdaCommandType type)
{
	g_return_if_fail (cmd != NULL);
	cmd->type = type;
}
