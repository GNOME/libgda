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

#ifndef __GDA_COMMAND_H__
#define __GDA_COMMAND_H__

#include <glib-object.h>
#include <glib/gmacros.h>
#include <glib/gtypes.h>

G_BEGIN_DECLS

typedef enum {
	GDA_COMMAND_OPTION_IGNORE_ERRORS  = 1,
	GDA_COMMAND_OPTION_STOP_ON_ERRORS = 1 << 1,
	GDA_COMMAND_OPTION_BAD_OPTION     = 1 << 2
} GdaCommandOptions;

#define GDA_COMMAND_DEFAULT_OPTION GDA_COMMAND_OPTION_IGNORE_ERRORS

typedef enum {
	GDA_COMMAND_TYPE_SQL,
	GDA_COMMAND_TYPE_XML,
	GDA_COMMAND_TYPE_PROCEDURE,
	GDA_COMMAND_TYPE_TABLE,
	GDA_COMMAND_TYPE_SCHEMA,
	GDA_COMMAND_TYPE_INVALID
} GdaCommandType;

typedef struct _GdaCommand GdaCommand;

struct _GdaCommand {
	gchar             *text;
	GdaCommandType     type;
	GdaCommandOptions  options;
};

#define GDA_TYPE_COMMAND (gda_command_get_type ())

GType             gda_command_get_type (void) G_GNUC_CONST;
GdaCommand       *gda_command_new (const gchar *text, GdaCommandType type,
				   GdaCommandOptions options);
void              gda_command_free (GdaCommand *cmd);
GdaCommand       *gda_command_copy (GdaCommand *cmd);

const gchar      *gda_command_get_text (GdaCommand *cmd);
void              gda_command_set_text (GdaCommand *cmd, const gchar *text);
GdaCommandType    gda_command_get_command_type (GdaCommand *cmd);
void              gda_command_set_command_type (GdaCommand *cmd, GdaCommandType type);
GdaCommandOptions gda_command_get_options (GdaCommand *cmd);
void              gda_command_set_options (GdaCommand *cmd, GdaCommandOptions options);

G_END_DECLS

#endif
