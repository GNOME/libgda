/* GNOME DB ODBC Provider
 * Copyright (C) Nick Gorham
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

#if !defined(__gda_odbc_h__)
#  define __gda_odbc_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <gda-server.h>

#ifndef SQL_SUCCEEDED
#define SQL_SUCCEEDED(x) (x == SQL_SUCCESS)
#endif

/*
 * Per-object specific structures
 */
typedef struct
{
    SQLHENV     henv;
    SQLHDBC     hdbc;
    int         connected;
} ODBC_Connection;

typedef struct
{
} ODBC_Command;

typedef struct
{
    SQLHSTMT    hstmt;
    int         allocated;
    SQLSMALLINT ncols;
    int         mapped_cols;
    int         map[ 20 ];
    SQLINTEGER  *sizes;
} ODBC_Recordset;

/*
 * Server implementation prototypes
 */
gboolean gda_odbc_connection_new (Gda_ServerConnection *cnc);
gint gda_odbc_connection_open (Gda_ServerConnection *cnc,
				    const gchar *dsn,
				    const gchar *user,
				    const gchar *password);
void gda_odbc_connection_close (Gda_ServerConnection *cnc);
gint gda_odbc_connection_begin_transaction (Gda_ServerConnection *cnc);
gint gda_odbc_connection_commit_transaction (Gda_ServerConnection *cnc);
gint gda_odbc_connection_rollback_transaction (Gda_ServerConnection *cnc);
Gda_ServerRecordset* gda_odbc_connection_open_schema (Gda_ServerConnection *cnc,
							       Gda_ServerError *error,
							       GDA_Connection_QType t,
							       GDA_Connection_Constraint *constraints,
							       gint length);
gint gda_odbc_connection_start_logging (Gda_ServerConnection *cnc,
					     const gchar *filename);
gint gda_odbc_connection_stop_logging (Gda_ServerConnection *cnc);
gchar* gda_odbc_connection_create_table (Gda_ServerConnection *cnc,
					      GDA_RowAttributes *columns);
gboolean gda_odbc_connection_supports (Gda_ServerConnection *cnc,
					    GDA_Connection_Feature feature);
GDA_ValueType gda_odbc_connection_get_gda_type (Gda_ServerConnection *cnc,
						     gulong sql_type);
gshort gda_odbc_connection_get_c_type (Gda_ServerConnection *cnc,
					    GDA_ValueType type);
void gda_odbc_connection_free (Gda_ServerConnection *cnc);

gboolean gda_odbc_command_new (Gda_ServerCommand *cmd);
Gda_ServerRecordset* gda_odbc_command_execute (Gda_ServerCommand *cmd,
							Gda_ServerError *error,
							const GDA_CmdParameterSeq *params,
							gulong *affected,
							gulong options);
void gda_odbc_command_free (Gda_ServerCommand *cmd);

gboolean gda_odbc_recordset_new       (Gda_ServerRecordset *recset);
gint     gda_odbc_recordset_move_next (Gda_ServerRecordset *recset);
gint     gda_odbc_recordset_move_prev (Gda_ServerRecordset *recset);
gint     gda_odbc_recordset_close     (Gda_ServerRecordset *recset);
void     gda_odbc_recordset_free      (Gda_ServerRecordset *recset);

void gda_odbc_error_make (Gda_ServerError *error,
			       Gda_ServerRecordset *recset,
			       Gda_ServerConnection *cnc,
			       gchar *where);

Gda_ServerRecordset*
gda_odbc_describe_recset(Gda_ServerRecordset* recset, ODBC_Recordset* odbc_recset);

#endif
