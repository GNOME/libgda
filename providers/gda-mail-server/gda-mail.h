/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; fill-column: 160 -*- */
/* GNOME DB Mail Provider
 * Copyright (C) 2000 Akira TAGOH
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

#if !defined(__gda_mail_h__)
#  define __gda_mail_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough.  */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

/*
 * Per-object specific structures
 */
typedef struct
{
} Mail_Connection;

typedef struct
{
} Mail_Command;

typedef struct
{
	Mail_Command ml_cmd;
} Mail_Recordset;

/*
 * Server implementation prototypes
 */
gboolean gda_mail_connection_new (Gda_ServerConnection * cnc);
gint gda_mail_connection_open (Gda_ServerConnection * cnc,
							   const gchar * dsn,
							   const gchar * user,
							   const gchar * password);
void gda_mail_connection_close (Gda_ServerConnection * cnc);
gint gda_mail_connection_begin_transaction (Gda_ServerConnection * cnc);
gint gda_mail_connection_commit_transaction (Gda_ServerConnection * cnc);
gint gda_mail_connection_rollback_transaction (Gda_ServerConnection * cnc);
Gda_ServerRecordset* gda_mail_connection_open_schema (Gda_ServerConnection * cnc,
													  Gda_ServerError * error,
													  GDA_Connection_QType t,
													  GDA_Connection_Constraint * constraints,
													  gint length);
gint gda_mail_connection_start_logging (Gda_ServerConnection * cnc,
										const gchar * filename);
gint gda_mail_connection_stop_logging (Gda_ServerConnection * cnc);
gchar* gda_mail_connection_create_table (Gda_ServerConnection * cnc,
										 GDA_RowAttributes * columns);
gboolean gda_mail_connection_supports (Gda_ServerConnection * cnc,
									   GDA_Connection_Feature feature);
GDA_ValueType gda_mail_connection_get_gda_type (Gda_ServerConnection * cnc,
												gulong sql_type);
gshort gda_mail_connection_get_c_type (Gda_ServerConnection * cnc,
									   GDA_ValueType type);
void gda_mail_connection_free (Gda_ServerConnection * cnc);

gboolean gda_mail_command_new (Gda_ServerCommand * cmd);
Gda_ServerRecordset* gda_mail_command_execute (Gda_ServerCommand * cmd,
											   Gda_ServerError * error,
											   const GDA_CmdParameterSeq * params,
											   gulong * affected,
											   gulong options);
void gda_mail_command_free (Gda_ServerCommand * cmd);

gboolean gda_mail_recordset_new       (Gda_ServerRecordset * recset);
gint     gda_mail_recordset_move_next (Gda_ServerRecordset * recset);
gint     gda_mail_recordset_move_prev (Gda_ServerRecordset * recset);
gint     gda_mail_recordset_close     (Gda_ServerRecordset * recset);
void     gda_mail_recordset_free      (Gda_ServerRecordset * recset);

void gda_mail_error_make (Gda_ServerError * error,
						  Gda_ServerRecordset * recset,
						  Gda_ServerConnection * cnc,
						  gchar * where);

#endif
