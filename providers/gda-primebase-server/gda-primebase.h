/* GNOME DB primebase Provider
 * Copyright (C) 2000 Holger Thon
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

#if !defined(__gda_primebase_h__)
#  define __gda_primebase_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <dalapi.h>
#include <daltypes.h>

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

#define MAX_DALSIZE (32L * 1024L)
#define GDA_PRIMEBASE_TYPE_CNT 27

/*
 * Per-object specific structures
 */
typedef struct
{
  long perr, serr;
  long snum;
  char  msg[256];
} primebase_Error;

typedef struct
{
  long sid;   // The connections' session id
  long snum;  // The current session number active of the connection

  gint state;

  gchar *host;
  gchar *db;
  gchar *connparm;
  primebase_Error *err;
} primebase_Connection;

typedef struct
{
} primebase_Command;

typedef struct
{
  gboolean initialized;
  gshort   type, len, places, flags;
  gint     col_cnt;
  gchar    *buffer;
} primebase_Recordset;

typedef struct
{
  gchar         *name;
  short         sql_type;
  GDA_ValueType gda_type;
} primebase_Types;

/*
 * Server implementation prototypes
 */
gboolean gda_primebase_connection_new (Gda_ServerConnection *cnc);
gint gda_primebase_connection_open (Gda_ServerConnection *cnc,
				    const gchar *dsn,
				    const gchar *user,
				    const gchar *password);
void gda_primebase_connection_close (Gda_ServerConnection *cnc);
gint gda_primebase_connection_begin_transaction (Gda_ServerConnection *cnc);
gint gda_primebase_connection_commit_transaction (Gda_ServerConnection *cnc);
gint gda_primebase_connection_rollback_transaction (Gda_ServerConnection *cnc);
Gda_ServerRecordset* gda_primebase_connection_open_schema (Gda_ServerConnection *cnc,
							       Gda_ServerError *error,
							       GDA_Connection_QType t,
							       GDA_Connection_Constraint *constraints,
							       gint length);
glong gda_primebase_connection_modify_schema (Gda_ServerConnection *cnc,
                                   GDA_Connection_QType t,
                                   GDA_Connection_Constraint *constraints,
                                   gint length);
gint gda_primebase_connection_start_logging (Gda_ServerConnection *cnc,
					     const gchar *filename);
gint gda_primebase_connection_stop_logging (Gda_ServerConnection *cnc);
gchar* gda_primebase_connection_create_table (Gda_ServerConnection *cnc,
					      GDA_RowAttributes *columns);
gboolean gda_primebase_connection_supports (Gda_ServerConnection *cnc,
					    GDA_Connection_Feature feature);
GDA_ValueType gda_primebase_connection_get_gda_type (Gda_ServerConnection *cnc,
						     gulong sql_type);
gshort gda_primebase_connection_get_c_type (Gda_ServerConnection *cnc,
					    GDA_ValueType type);
gchar* gda_primebase_connection_sql2xml (Gda_ServerConnection *cnc, const gchar *sql);
gchar* gda_primebase_connection_xml2sql (Gda_ServerConnection *cnc, const gchar *xml);
void gda_primebase_connection_free (Gda_ServerConnection *cnc);

gboolean gda_primebase_command_new (Gda_ServerCommand *cmd);
Gda_ServerRecordset* gda_primebase_command_execute (Gda_ServerCommand *cmd,
							Gda_ServerError *error,
							const GDA_CmdParameterSeq *params,
							gulong *affected,
							gulong options);
void gda_primebase_command_free (Gda_ServerCommand *cmd);

gboolean gda_primebase_recordset_new       (Gda_ServerRecordset *recset);
gint     gda_primebase_recordset_move_next (Gda_ServerRecordset *recset);
gint     gda_primebase_recordset_move_prev (Gda_ServerRecordset *recset);
gint     gda_primebase_recordset_close     (Gda_ServerRecordset *recset);
void     gda_primebase_recordset_free      (Gda_ServerRecordset *recset);

primebase_Error *gda_primebase_get_error(long, gboolean);
void gda_primebase_free_error(primebase_Error *);

void gda_primebase_error_make (Gda_ServerError *error,
			       Gda_ServerRecordset *recset,
			       Gda_ServerConnection *cnc,
			       gchar *where);

#endif
