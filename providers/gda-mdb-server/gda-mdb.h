/* GNOME DB MsAccess Provider
 * Copyright (C) 2000 Alvaro del Castillo
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

#if !defined(__gda_mdb_h__)
#  define __gda_mdb_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include "mdbtools.h"
/*#include <mdb_com.h>*/

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
  MdbHandle* mdb;
} MdbHandle_Connection;

typedef struct
{
} Mdb_Command;

typedef struct _Mdb_Recordset Mdb_Recordset;
struct _Mdb_Recordset
{
  MdbTableDef 	      *table;
  MdbHandle            *mdb;
  MdbCatalogEntry     entry;
  /*gint*               order;
  gint                order_count;
  MYSQL_ROW           array;
  unsigned long       lengths;*/
};

/*
 * Server implementation prototypes
 */
gboolean gda_mdb_connection_new (GdaServerConnection *cnc);
gint gda_mdb_connection_open (GdaServerConnection *cnc,
				const gchar *dsn,
				const gchar *user,
				const gchar *password);
void gda_mdb_connection_close (GdaServerConnection *cnc);
gint gda_mdb_connection_begin_transaction (GdaServerConnection *cnc);
gint gda_mdb_connection_commit_transaction (GdaServerConnection *cnc);
gint gda_mdb_connection_rollback_transaction (GdaServerConnection *cnc);
GdaServerRecordset* gda_mdb_connection_open_schema (GdaServerConnection *cnc,
						       GdaError *error,
						       GDA_Connection_QType t,
						       GDA_Connection_Constraint *constraints,
						       gint length);
glong gda_mdb_connection_modfy_schema (GdaServerConnection *cnc,
                                       GDA_Connection_QType t,
                                       GDA_Connection_Constraint *constraints,
                                       gint length);
gint gda_mdb_connection_start_logging (GdaServerConnection *cnc,
					     const gchar *filename);
gint gda_mdb_connection_stop_logging (GdaServerConnection *cnc);
gchar* gda_mdb_connection_create_table (GdaServerConnection *cnc,
					  GDA_RowAttributes *columns);
gboolean gda_mdb_connection_supports (GdaServerConnection *cnc,
					GDA_Connection_Feature feature);
GDA_ValueType gda_mdb_connection_get_gda_type (GdaServerConnection *cnc,
						 gulong sql_type);
gshort gda_mdb_connection_get_c_type (GdaServerConnection *cnc,
					GDA_ValueType type);
gchar* gda_mdb_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql);
gchar* gda_mdb_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml);
void gda_mdb_connection_free (GdaServerConnection *cnc);

gboolean gda_mdb_command_new (GdaServerCommand *cmd);
GdaServerRecordset* gda_mdb_command_execute (GdaServerCommand *cmd,
						GdaError *error,
						const GDA_CmdParameterSeq *params,
						gulong *affected,
						gulong options);
void gda_mdb_command_free (GdaServerCommand *cmd);

gboolean gda_mdb_recordset_new       (GdaServerRecordset *recset);
gint     gda_mdb_recordset_move_next (GdaServerRecordset *recset);
gint     gda_mdb_recordset_move_prev (GdaServerRecordset *recset);
gint     gda_mdb_recordset_close     (GdaServerRecordset *recset);
void     gda_mdb_recordset_free      (GdaServerRecordset *recset);

void gda_mdb_error_make (GdaError *error,
			   GdaServerRecordset *recset,
			   GdaServerConnection *cnc,
			   gchar *where);

/*
 * Utility functions
 */
void gda_mdb_init_recset_fields (GdaServerRecordset *recset,
				   Mdb_Recordset *mdb_recset);

#endif
