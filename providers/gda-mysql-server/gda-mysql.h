/* GNOME DB MySQL Provider
 * Copyright (C) 1998 Michael Lausch
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2001 Vivien Malerba
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

#if !defined(__gda_mysql_h__)
#  define __gda_mysql_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <mysql.h>
#include <mysql_com.h>
#include <gda-builtin-res.h>

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

/* The following constant defines the "bool" types, which IS NOT a MySQL data
 * type; it is used by the schema functions to be able to return boolean
 * values when necessary. It WILL NOT appear as a possible data type 
 */
#define FIELD_TYPE_BOOL 100001

typedef enum {
  SQL_C_DATE,
  SQL_C_TIME,
  SQL_C_TIMESTAMP,
  SQL_C_BINARY,
  SQL_C_BIT,
  SQL_C_TINYINT,
  SQL_C_CHAR,
  SQL_C_LONG,
  SQL_C_SHORT,
  SQL_C_FLOAT,
  SQL_C_DOUBLE,
  SQL_C_SLONG,
  SQL_C_SSHORT,
  SQL_C_ULONG,
  SQL_C_USHORT,
  SQL_C_STINYINT,
  SQL_C_UTINYINT,
  SQL_C_BOOKMARK
} MYSQL_CType;

typedef struct MYSQL_Types_Array
{
  gchar *mysql_type;
  gulong oid;
  GDA_ValueType gda_type;
  MYSQL_CType c_type;
  gchar *description;
  gchar *owner;
} MYSQL_Types_Array;


/*
 * Per-object specific structures
 */
typedef struct
{
  MYSQL             *mysql;
  MYSQL_Types_Array *types_array;
} MYSQL_Connection;

typedef struct
{
} MYSQL_Command;

typedef struct _MYSQL_Recordset MYSQL_Recordset;
typedef MYSQL_ROW (*MYSQL_FixFieldsFunc)(MYSQL_Recordset *, MYSQL_ROW );
struct _MYSQL_Recordset
{
  MYSQL_RES*          mysql_res;
  gint*               order;
  gint                order_count;
  MYSQL_FixFieldsFunc fix_fields;
  MYSQL_ROW           array;
  unsigned long       lengths;
  gulong              pos;

  /* in case we have a dummy recordset */
  GdaBuiltin_Result*  btin_res;
};

/*
 * Server implementation prototypes
 */
gboolean gda_mysql_connection_new (GdaServerConnection *cnc);
gint gda_mysql_connection_open (GdaServerConnection *cnc,
				const gchar *dsn,
				const gchar *user,
				const gchar *password);
void gda_mysql_connection_close (GdaServerConnection *cnc);
gint gda_mysql_connection_begin_transaction (GdaServerConnection *cnc);
gint gda_mysql_connection_commit_transaction (GdaServerConnection *cnc);
gint gda_mysql_connection_rollback_transaction (GdaServerConnection *cnc);
GdaServerRecordset* gda_mysql_connection_open_schema (GdaServerConnection *cnc,
						       GdaServerError *error,
						       GDA_Connection_QType t,
						       GDA_Connection_Constraint *constraints,
						       gint length);
glong gda_mysql_connection_modify_schema (GdaServerConnection *cnc,
                                   GDA_Connection_QType t,
                                   GDA_Connection_Constraint *constraints,
                                   gint length);
gint gda_mysql_connection_start_logging (GdaServerConnection *cnc,
					     const gchar *filename);
gint gda_mysql_connection_stop_logging (GdaServerConnection *cnc);
gchar* gda_mysql_connection_create_table (GdaServerConnection *cnc,
					  GDA_RowAttributes *columns);
gboolean gda_mysql_connection_supports (GdaServerConnection *cnc,
					GDA_Connection_Feature feature);
GDA_ValueType gda_mysql_connection_get_gda_type (GdaServerConnection *cnc,
						 gulong sql_type);
gshort gda_mysql_connection_get_c_type (GdaServerConnection *cnc,
					GDA_ValueType type);
gchar* gda_mysql_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql);
gchar* gda_mysql_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml);
void gda_mysql_connection_free (GdaServerConnection *cnc);

gboolean gda_mysql_command_new (GdaServerCommand *cmd);
GdaServerRecordset* gda_mysql_command_execute (GdaServerCommand *cmd,
						GdaServerError *error,
						const GDA_CmdParameterSeq *params,
						gulong *affected,
						gulong options);
void gda_mysql_command_free (GdaServerCommand *cmd);

gboolean gda_mysql_recordset_new       (GdaServerRecordset *recset);
gint     gda_mysql_recordset_move_next (GdaServerRecordset *recset);
gint     gda_mysql_recordset_move_prev (GdaServerRecordset *recset);
gint     gda_mysql_recordset_close     (GdaServerRecordset *recset);
void     gda_mysql_recordset_free      (GdaServerRecordset *recset);

void gda_mysql_error_make (GdaServerError *error,
			   GdaServerRecordset *recset,
			   GdaServerConnection *cnc,
			   gchar *where);

/*
 * Utility functions
 */
void gda_mysql_init_recset_fields (GdaServerRecordset *recset,
				   MYSQL_Recordset *mysql_recset);

/* types conversion utilities */
gulong        gda_mysql_connection_get_sql_type(MYSQL_Connection *cnc,
						gchar *mysql_type);
GDA_ValueType gda_mysql_connection_get_gda_type_psql (MYSQL_Connection *cnc,
						      gulong sql_type);
gshort        gda_mysql_connection_get_c_type_psql (MYSQL_Connection *cnc,
						    GDA_ValueType type);
GdaServerRecordset *
gda_mysql_command_build_recset_with_builtin (GdaServerConnection *cnc,
					     GdaBuiltin_Result *res,
					     gulong *affected);
#endif
