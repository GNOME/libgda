/* GDA Common Library
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#if !defined(__gda_command_h__)
#  define __gda_command_h__

#include <GNOME_Database.h>
#include <glib/gmacros.h>

G_BEGIN_DECLS

typedef GNOME_Database_Command GdaCommand;
typedef enum {
	GDA_COMMAND_TYPE_SQL = GNOME_Database_COMMAND_TYPE_SQL,
	GDA_COMMAND_TYPE_XML = GNOME_Database_COMMAND_TYPE_XML,
	GDA_COMMAND_TYPE_PROCEDURE = GNOME_Database_COMMAND_TYPE_PROCEDURE,
	GDA_COMMAND_TYPE_TABLE = GNOME_Database_COMMAND_TYPE_TABLE,
	GDA_COMMAND_TYPE_INVALID = GNOME_Database_COMMAND_TYPE_INVALID
} GdaCommandType;

GdaCommand    *gda_command_new (const gchar *text, GdaCommandType type);
void           gda_command_free (GdaCommand *cmd);

const gchar   *gda_command_get_text (GdaCommand *cmd);
void           gda_command_set_text (GdaCommand *cmd, const gchar *text);
GdaCommandType gda_command_get_comamnd_type (GdaCommand *cmd);
void           gda_command_set_command_type (GdaCommand *cmd, GdaCommandType type);

G_END_DECLS

#endif
