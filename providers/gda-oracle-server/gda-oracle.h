/* GNOME DB ORACLE Provider
 * Copyright (C) 2000 Rodrigo Moya
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

#if !defined(__gda_oracle_h__)
#  define __gda_oracle_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <oci.h>
#include <oratypes.h>
#include <ocidfn.h>
#if defined __STDC__
#  include <ociapr.h>
#else
#  include <ocikpr.h>
#endif

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
  OCIEnv*     henv;
  OCIError*   herr;
  OCIServer*  hserver;
  OCISvcCtx*  hservice;
  OCISession* hsession;
} ORACLE_Connection;

typedef struct
{
  ORACLE_Connection* ora_cnc;
  OCIStmt*           hstmt;
  sword              stmt_type;
} ORACLE_Command;

typedef struct
{
  ORACLE_Connection* ora_cnc;
  OCIStmt*           hstmt;      /* shared with the ORACLE_Command struct */
} ORACLE_Recordset;

typedef struct
{
  OCIDefine* hdef;
  sb2        indicator;
  gpointer   real_value;
} ORACLE_Field;

/*
 * Server implementation prototypes
 */
gboolean gda_oracle_connection_new (Gda_ServerConnection *cnc);
gint gda_oracle_connection_open (Gda_ServerConnection *cnc,
				 const gchar *dsn,
				 const gchar *user,
				 const gchar *password);
void gda_oracle_connection_close (Gda_ServerConnection *cnc);
gint gda_oracle_connection_begin_transaction (Gda_ServerConnection *cnc);
gint gda_oracle_connection_commit_transaction (Gda_ServerConnection *cnc);
gint gda_oracle_connection_rollback_transaction (Gda_ServerConnection *cnc);
Gda_ServerRecordset* gda_oracle_connection_open_schema (Gda_ServerConnection *cnc,
							Gda_ServerError *error,
							GDA_Connection_QType t,
							GDA_Connection_Constraint *constraints,
							       gint length);
glong gda_oracle_connection_modify_schema (Gda_ServerConnection *cnc,
                                   GDA_Connection_QType t,
                                   GDA_Connection_Constraint *constraints,
                                   gint length);
gint gda_oracle_connection_start_logging (Gda_ServerConnection *cnc,
					  const gchar *filename);
gint gda_oracle_connection_stop_logging (Gda_ServerConnection *cnc);
gchar* gda_oracle_connection_create_table (Gda_ServerConnection *cnc,
					   GDA_RowAttributes *columns);
gboolean gda_oracle_connection_supports (Gda_ServerConnection *cnc,
					 GDA_Connection_Feature feature);
GDA_ValueType gda_oracle_connection_get_gda_type (Gda_ServerConnection *cnc,
						  gulong sql_type);
gshort gda_oracle_connection_get_c_type (Gda_ServerConnection *cnc,
					 GDA_ValueType type);
gchar* gda_oracle_connection_sql2xml (Gda_ServerConnection *cnc, const gchar *sql);
gchar* gda_oracle_connection_xml2sql (Gda_ServerConnection *cnc, const gchar *xml);
void gda_oracle_connection_free (Gda_ServerConnection *cnc);

gboolean gda_oracle_command_new (Gda_ServerCommand *cmd);
Gda_ServerRecordset* gda_oracle_command_execute (Gda_ServerCommand *cmd,
						 Gda_ServerError *error,
						 const GDA_CmdParameterSeq *params,
						 gulong *affected,
						 gulong options);
void gda_oracle_command_free (Gda_ServerCommand *cmd);

gboolean gda_oracle_recordset_new       (Gda_ServerRecordset *recset);
gint     gda_oracle_recordset_move_next (Gda_ServerRecordset *recset);
gint     gda_oracle_recordset_move_prev (Gda_ServerRecordset *recset);
gint     gda_oracle_recordset_close     (Gda_ServerRecordset *recset);
void     gda_oracle_recordset_free      (Gda_ServerRecordset *recset);

void gda_oracle_error_make (Gda_ServerError *error,
			    Gda_ServerRecordset *recset,
			    Gda_ServerConnection *cnc,
			    gchar *where);

#endif
