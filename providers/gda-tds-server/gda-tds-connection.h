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

typedef struct _tds_Error
{
	guint err_type;
	CS_CLIENTMSG cslib_msg;
	CS_CLIENTMSG client_msg;
	CS_SERVERMSG server_msg;
	gchar *udeferr_msg;
}
tds_Error;

typedef struct _tds_Connection
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *cnc;
	CS_RETCODE ret;

	gboolean cs_diag;	// wether we can use cs_diag for cslib messages
	gboolean ct_diag;	// wether we can use ct_diag for ctlib messages

	GdaServerError *error;
	tds_Error serr;

	gchar *database;
}
tds_Connection;

/*
 * Server implementation prototypes
 */
gboolean gda_tds_connection_new (GdaServerConnection * cnc);
gint gda_tds_connection_open (GdaServerConnection * cnc, const gchar * dsn,
			      const gchar * user, const gchar * passwd);
void gda_tds_connection_close (GdaServerConnection * cnc);
gint gda_tds_connection_begin_transaction (GdaServerConnection * cnc);
gint gda_tds_connection_commit_transaction (GdaServerConnection * cnc);
gint gda_tds_connection_rollback_transaction (GdaServerConnection * cnc);
GdaServerRecordset *gda_tds_connection_open_schema (GdaServerConnection * cnc,
						    GdaServerError * error,
						    GDA_Connection_QType t,
						    GDA_Connection_Constraint
						    * constraints,
						    gint length);
glong gda_tds_connection_modify_schema (GdaServerConnection * cnc,
					GDA_Connection_QType t,
					GDA_Connection_Constraint *
					constraints, gint length);
gint gda_tds_connection_start_logging (GdaServerConnection * cnc,
				       const gchar * filename);
gint gda_tds_connection_stop_logging (GdaServerConnection * cnc);
gchar *gda_tds_connection_create_table (GdaServerConnection * cnc,
					GDA_RowAttributes * columns);
gboolean gda_tds_connection_supports (GdaServerConnection *,
				      GDA_Connection_Feature feature);
const GDA_ValueType gda_tds_connection_get_gda_type (GdaServerConnection *,
						     gulong);
const gshort gda_tds_connection_get_c_type (GdaServerConnection *,
					    GDA_ValueType type);
gchar *gda_tds_connection_sql2xml (GdaServerConnection * cnc,
				   const gchar * sql);
gchar *gda_tds_connection_xml2sql (GdaServerConnection * cnc,
				   const gchar * xml);
void gda_tds_connection_free (GdaServerConnection * cnc);
void gda_tds_connection_clear_user_data (GdaServerConnection *, gboolean);

gboolean gda_tds_connection_dead (GdaServerConnection *);
gboolean gda_tds_connection_reopen (GdaServerConnection *);
CS_RETCODE gda_tds_connection_select_database (GdaServerConnection *,
					       const gchar * dbname);

#endif
