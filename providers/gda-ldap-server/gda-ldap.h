/* GNOME DB LDAP Provider
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

#if !defined(__gda_ldap_h__)
#  define __gda_ldap_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <lber.h>
#include <ldap.h>

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
  LDAP* ldap;
} LDAP_Connection;

typedef struct
{
} LDAP_Command;

typedef struct
{
  LDAP_Command ld_cmd;
} LDAP_Recordset;

/*
 * Server implementation prototypes
 */
gboolean gda_ldap_connection_new (GdaServerConnection *cnc);
gint gda_ldap_connection_open (GdaServerConnection *cnc,
				    const gchar *dsn,
				    const gchar *user,
				    const gchar *password);
void gda_ldap_connection_close (GdaServerConnection *cnc);
gint gda_ldap_connection_begin_transaction (GdaServerConnection *cnc);
gint gda_ldap_connection_commit_transaction (GdaServerConnection *cnc);
gint gda_ldap_connection_rollback_transaction (GdaServerConnection *cnc);
GdaServerRecordset* gda_ldap_connection_open_schema (GdaServerConnection *cnc,
							       GdaServerError *error,
							       GDA_Connection_QType t,
							       GDA_Connection_Constraint *constraints,
							       gint length);
glong gda_ldap_connection_modify_schema (GdaServerConnection *cnc,
                                         GDA_Connection_QType t,
                                         GDA_Connection_Constraint *constraints,
                                         gint length);
gint gda_ldap_connection_start_logging (GdaServerConnection *cnc,
					     const gchar *filename);
gint gda_ldap_connection_stop_logging (GdaServerConnection *cnc);
gchar* gda_ldap_connection_create_table (GdaServerConnection *cnc,
					      GDA_RowAttributes *columns);
gboolean gda_ldap_connection_supports (GdaServerConnection *cnc,
					    GDA_Connection_Feature feature);
GDA_ValueType gda_ldap_connection_get_gda_type (GdaServerConnection *cnc,
						     gulong sql_type);
gshort gda_ldap_connection_get_c_type (GdaServerConnection *cnc,
					    GDA_ValueType type);
gchar* gda_ldap_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql);
gchar* gda_ldap_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml);
void gda_ldap_connection_free (GdaServerConnection *cnc);

gboolean gda_ldap_command_new (GdaServerCommand *cmd);
GdaServerRecordset* gda_ldap_command_execute (GdaServerCommand *cmd,
							GdaServerError *error,
							const GDA_CmdParameterSeq *params,
							gulong *affected,
							gulong options);
void gda_ldap_command_free (GdaServerCommand *cmd);

gboolean gda_ldap_recordset_new       (GdaServerRecordset *recset);
gint     gda_ldap_recordset_move_next (GdaServerRecordset *recset);
gint     gda_ldap_recordset_move_prev (GdaServerRecordset *recset);
gint     gda_ldap_recordset_close     (GdaServerRecordset *recset);
void     gda_ldap_recordset_free      (GdaServerRecordset *recset);

void gda_ldap_error_make (GdaServerError *error,
			       GdaServerRecordset *recset,
			       GdaServerConnection *cnc,
			       gchar *where);

#endif
