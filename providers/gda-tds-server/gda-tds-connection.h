/* 
 * $Id$
 *
 * GNOME DB tds Provider
 * Copyright (C) 2000 Holger Thon
 * Copyright (C) 2000 Stephan Heinze
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

#if !defined(__gda_tds_connection_h__)
#  define __gda_tds_connection_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <glib.h>
#include <ctpublic.h>
#include "gda-tds.h"

// The application name we tell tds we are
// 
#define GDA_TDS_DEFAULT_APPNAME "gda-tds-srv"


/*
 * Per-object specific structures
 */

typedef struct _tds_Error {
  guint           err_type;
  CS_CLIENTMSG    cslib_msg;
  CS_CLIENTMSG    client_msg;
  CS_SERVERMSG    server_msg;
  gchar           *udeferr_msg;
} tds_Error;

typedef struct _tds_Connection {
  CS_CONTEXT      *ctx;
  CS_CONNECTION   *cnc;
  CS_RETCODE      ret;
  
  gboolean        cs_diag; // wether we can use cs_diag for cslib messages
  gboolean        ct_diag; // wether we can use ct_diag for ctlib messages
  
  Gda_ServerError *error;
  tds_Error    serr;
  
  gchar           *database;
} tds_Connection;

/*
 * Server implementation prototypes
 */
gboolean gda_tds_connection_new (Gda_ServerConnection *cnc);
gint gda_tds_connection_open (Gda_ServerConnection *cnc, const gchar *dsn,
                                 const gchar *user, const gchar *passwd);
void gda_tds_connection_close (Gda_ServerConnection *cnc);
gint gda_tds_connection_begin_transaction (Gda_ServerConnection *cnc);
gint gda_tds_connection_commit_transaction (Gda_ServerConnection *cnc);
gint gda_tds_connection_rollback_transaction (Gda_ServerConnection *cnc);
Gda_ServerRecordset* gda_tds_connection_open_schema (Gda_ServerConnection *cnc,
                                     Gda_ServerError *error,
                                     GDA_Connection_QType t,
                                     GDA_Connection_Constraint *constraints,
                                     gint length);
glong gda_tds_connection_modify_schema (Gda_ServerConnection *cnc,
                                   GDA_Connection_QType t,
                                   GDA_Connection_Constraint *constraints,
                                   gint length);
gint gda_tds_connection_start_logging (Gda_ServerConnection *cnc,
                                       const gchar *filename);
gint gda_tds_connection_stop_logging (Gda_ServerConnection *cnc);
gchar* gda_tds_connection_create_table (Gda_ServerConnection *cnc,
                                        GDA_RowAttributes *columns);
gboolean gda_tds_connection_supports(Gda_ServerConnection *,
                                     GDA_Connection_Feature feature);
const GDA_ValueType gda_tds_connection_get_gda_type(Gda_ServerConnection *,
                                                    gulong);
const gshort gda_tds_connection_get_c_type(Gda_ServerConnection *,
                                           GDA_ValueType type);
void gda_tds_connection_free (Gda_ServerConnection *cnc);
void gda_tds_connection_clear_user_data(Gda_ServerConnection *, gboolean);

gboolean gda_tds_connection_dead(Gda_ServerConnection *);
gboolean gda_tds_connection_reopen(Gda_ServerConnection *);
CS_RETCODE gda_tds_connection_select_database(Gda_ServerConnection *,
                                              const gchar * dbname);

#endif
