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

#if !defined(__gda_server_command_h__)
#  define __gda_server_command_h__

#include <bonobo/bonobo-xobject.h>
#include <GDA.h>
#include <gda-common-defs.h>
#include <gda-error.h>
#include <gda-server-connection.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_COMMAND            (gda_server_command_get_type())
#define GDA_SERVER_COMMAND(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_SERVER_COMMAND, GdaServerCommand)
#define GDA_SERVER_COMMAND_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_SERVER_COMMAND, GdaServerCommandClass)
#define GDA_IS_SERVER_COMMAND(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_SERVER_COMMAND)
#define GDA_IS_SERVER_COMMAND_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_COMMAND))

typedef struct _GdaServerCommand GdaServerCommand;
typedef struct _GdaServerCommandClass GdaServerCommandClass;

struct _GdaServerCommand {
	BonoboXObject object;

	/* data */
	GdaServerConnection *cnc;
	gchar *text;
	GDA_CommandType type;

	gpointer user_data;
};

struct _GdaServerCommandClass {
	BonoboXObjectClass parent_class;

	POA_GDA_Command__epv epv;
};

GtkType gda_server_command_get_type (void);
GdaServerCommand *gda_server_command_new (GdaServerConnection * cnc);
GdaServerConnection *gda_server_command_get_connection (GdaServerCommand * cmd);
gchar *gda_server_command_get_text (GdaServerCommand * cmd);
void gda_server_command_set_text (GdaServerCommand * cmd,
				  const gchar * text);
GDA_CommandType gda_server_command_get_cmd_type (GdaServerCommand *cmd);
void gda_server_command_set_cmd_type (GdaServerCommand * cmd,
				      GDA_CommandType type);
gpointer gda_server_command_get_user_data (GdaServerCommand * cmd);
void gda_server_command_set_user_data (GdaServerCommand * cmd,
				       gpointer user_data);
void gda_server_command_free (GdaServerCommand * cmd);
GdaServerRecordset *gda_server_command_execute (GdaServerCommand *cmd,
						GdaError * error,
						const GDA_CmdParameterSeq *params,
						gulong * affected,
						gulong options);

G_END_DECLS

#endif
