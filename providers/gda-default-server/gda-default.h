/* GDA Default Provider
 * Copyright (C) 2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_default_h__)
#  define __gda_default_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <sqlite.h>

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
typedef struct {
	sqlite *sqlite;
} DEFAULT_Connection;

typedef struct {
} DEFAULT_Command;

typedef struct {
	gint number_of_cols;
	gint number_of_rows;
	gchar **data;
	gulong position;
} DEFAULT_Recordset;

/*
 * Server implementation prototypes
 */
gboolean gda_default_connection_new (GdaServerConnection *cnc);
gint gda_default_connection_open (GdaServerConnection *cnc,
								  const gchar *dsn,
								  const gchar *user,
								  const gchar *password);
void gda_default_connection_close (GdaServerConnection *cnc);
gint gda_default_connection_begin_transaction (GdaServerConnection *cnc);
gint gda_default_connection_commit_transaction (GdaServerConnection *cnc);
gint gda_default_connection_rollback_transaction (GdaServerConnection *cnc);
GdaServerRecordset* gda_default_connection_open_schema (GdaServerConnection *cnc,
														GdaError *error,
														GDA_Connection_QType t,
														GDA_Connection_Constraint *constraints,
														gint length);
glong gda_default_connection_modify_schema (GdaServerConnection *cnc,
											GDA_Connection_QType t,
											GDA_Connection_Constraint *constraints,
											gint length);
gint gda_default_connection_start_logging (GdaServerConnection *cnc,
										   const gchar *filename);
gint gda_default_connection_stop_logging (GdaServerConnection *cnc);
gchar* gda_default_connection_create_table (GdaServerConnection *cnc,
											GDA_RowAttributes *columns);
gboolean gda_default_connection_supports (GdaServerConnection *cnc,
										  GDA_Connection_Feature feature);
GDA_ValueType gda_default_connection_get_gda_type (GdaServerConnection *cnc,
												   gulong sql_type);
gshort gda_default_connection_get_c_type (GdaServerConnection *cnc,
										  GDA_ValueType type);
gchar* gda_default_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql);
gchar* gda_default_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml);
void gda_default_connection_free (GdaServerConnection *cnc);

gboolean gda_default_command_new (GdaServerCommand *cmd);
GdaServerRecordset* gda_default_command_execute (GdaServerCommand *cmd,
												 GdaError *error,
												 const GDA_CmdParameterSeq *params,
												 gulong *affected,
												 gulong options);
void gda_default_command_free (GdaServerCommand *cmd);

gboolean gda_default_recordset_new       (GdaServerRecordset *recset);
gint     gda_default_recordset_move_next (GdaServerRecordset *recset);
gint     gda_default_recordset_move_prev (GdaServerRecordset *recset);
gint     gda_default_recordset_close     (GdaServerRecordset *recset);
void     gda_default_recordset_free      (GdaServerRecordset *recset);

void gda_default_error_make (GdaError *error,
							 GdaServerRecordset *recset,
							 GdaServerConnection *cnc,
							 gchar *where);

#endif
