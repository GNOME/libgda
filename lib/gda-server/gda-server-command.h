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

#include <GDA.h>
#include <gda-error.h>
#include <gda-server-connection.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
	GdaServerConnection* cnc;
	gchar*               text;
	GDA_CommandType      type;
	gint                 users;
	
	gpointer             user_data;
} GdaServerCommand;

GdaServerCommand*    gda_server_command_new  (GdaServerConnection *cnc);
GdaServerConnection* gda_server_command_get_connection (GdaServerCommand *cmd);
gchar*               gda_server_command_get_text (GdaServerCommand *cmd);
void                 gda_server_command_set_text (GdaServerCommand *cmd,
                                                   const gchar *text);
GDA_CommandType      gda_server_command_get_type (GdaServerCommand *cmd);
void                 gda_server_command_set_type (GdaServerCommand *cmd,
                                                   GDA_CommandType type);
gpointer             gda_server_command_get_user_data (GdaServerCommand *cmd);
void                 gda_server_command_set_user_data (GdaServerCommand *cmd,
                                                        gpointer user_data);
void                 gda_server_command_free (GdaServerCommand *cmd);
GdaServerRecordset*  gda_server_command_execute (GdaServerCommand *cmd,
                                                  GdaError *error,
                                                  const GDA_CmdParameterSeq *params,
                                                  gulong *affected,
                                                  gulong options);

#if defined(__cplusplus)
}
#endif

#endif
