/* 
 * $Id$
 *
 * GNOME DB sybase Provider
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2000 Stephan Heinze
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

#if !defined(__gda_sybase_connection_h__)
#  define __gda_sybase_connection_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <glib.h>
#include <ctpublic.h>
#include "gda-sybase.h"

// The application name we tell sybase we are
// 
#define GDA_SYBASE_DEFAULT_APPNAME "gda-sybase-srv"


/*
 * Per-object specific structures
 */

typedef struct _sybase_Error
{
	guint err_type;
	CS_CLIENTMSG cslib_msg;
	CS_CLIENTMSG client_msg;
	CS_SERVERMSG server_msg;
	gchar *udeferr_msg;
}
sybase_Error;

typedef struct _sybase_Connection
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *cnc;
	CS_RETCODE ret;

	gboolean cs_diag;	// wether we can use cs_diag for cslib messages
	gboolean ct_diag;	// wether we can use ct_diag for ctlib messages

	GdaError *error;
	sybase_Error serr;

	gchar *database;
}
sybase_Connection;

/*
 * Server implementation prototypes
 */
gboolean gda_sybase_connection_new (GdaServerConnection * cnc);
gint gda_sybase_connection_open (GdaServerConnection * cnc, const gchar * dsn,
				 const gchar * user, const gchar * passwd);
void gda_sybase_connection_close (GdaServerConnection * cnc);
gint gda_sybase_connection_begin_transaction (GdaServerConnection * cnc);
gint gda_sybase_connection_commit_transaction (GdaServerConnection * cnc);
gint gda_sybase_connection_rollback_transaction (GdaServerConnection * cnc);
GdaServerRecordset *gda_sybase_connection_open_schema (GdaServerConnection *
						       cnc,
						       GdaError * error,
						       GDA_Connection_QType t,
						       GDA_Connection_Constraint
						       * constraints,
						       gint length);
glong gda_sybase_connection_modify_schema (GdaServerConnection * cnc,
					   GDA_Connection_QType t,
					   GDA_Connection_Constraint *
					   constraints, gint length);
gint gda_sybase_connection_start_logging (GdaServerConnection * cnc,
					  const gchar * filename);
gint gda_sybase_connection_stop_logging (GdaServerConnection * cnc);
gchar *gda_sybase_connection_create_table (GdaServerConnection * cnc,
					   GDA_RowAttributes * columns);
gboolean gda_sybase_connection_supports (GdaServerConnection *,
					 GDA_Connection_Feature feature);
const GDA_ValueType gda_sybase_connection_get_gda_type (GdaServerConnection *,
							gulong);
const gshort gda_sybase_connection_get_c_type (GdaServerConnection *,
					       GDA_ValueType type);
gchar *gda_sybase_connection_sql2xml (GdaServerConnection * cnc,
				      const gchar * sql);
gchar *gda_sybase_connection_xml2sql (GdaServerConnection * cnc,
				      const gchar * xml);
void gda_sybase_connection_free (GdaServerConnection * cnc);
void gda_sybase_connection_clear_user_data (GdaServerConnection *, gboolean);

gboolean gda_sybase_connection_dead (GdaServerConnection *);
gboolean gda_sybase_connection_reopen (GdaServerConnection *);
CS_RETCODE gda_sybase_connection_select_database (GdaServerConnection *,
						  const gchar * dbname);

#endif
