/* GNOME DB Postgres Provider
 * Copyright (C) 2000 Vivien Malerba
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

#if !defined(__gda_postgres_h__)
#define __gda_postgres_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <glib.h>
#include <libpq-fe.h>
#include "gda-builtin-res.h"

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
} POSTGRES_CType;

typedef struct POSTGRES_Types_Array
{
  gchar *postgres_type;
  gulong oid;
  GDA_ValueType gda_type;
  POSTGRES_CType c_type;
} POSTGRES_Types_Array;

typedef struct POSTGRES_Connection
{
  gchar                    *pq_host;
  gchar                    *pq_port;
  gchar                    *pq_options;
  gchar                    *pq_tty;
  gchar                    *pq_db;
  gchar                    *pq_login;
  gchar                    *pq_pwd;
  PGconn                   *pq_conn;
  GList                    *errors;
  POSTGRES_Types_Array *types_array;
  gfloat                   version; /* Postgres version like 6.53 or 7.02, etc */
} POSTGRES_Connection;

/* unused so far! */
typedef struct POSTGRES_Command
{
  struct POSTGRES_Connection* cnc;
  gchar*                           cmd;
  gulong                           type;
} POSTGRES_Command;

typedef struct POSTGRES_Recordset
{
  /*  struct POSTGRES_Command *cmd;*/
  PGresult*                     pq_data;
  GdaBuiltin_Result*           btin_res;
  POSTGRES_Connection*          cnc;
  gulong                        pos;
  GSList*                       replacements;
} POSTGRES_Recordset;

/* struct to replace values within Recordsets at fil value time */
typedef struct POSTGRES_Recordset_Replacement
{
  guint colnum;
  gchar* (* trans_func) (POSTGRES_Recordset *recset, gchar *value);
  GDA_ValueType newtype;
} POSTGRES_Recordset_Replacement;


/*
 * Server implementation prototypes
 */
gboolean gda_postgres_connection_new (GdaServerConnection *cnc);
gint     gda_postgres_connection_open (GdaServerConnection *cnc,
                                       const gchar *dsn,
                                       const gchar *user,
                                       const gchar *password);
void     gda_postgres_connection_close (GdaServerConnection *cnc);
gint     gda_postgres_connection_begin_transaction (GdaServerConnection *cnc);
gint     gda_postgres_connection_commit_transaction(GdaServerConnection *cnc);
gint gda_postgres_connection_rollback_transaction (GdaServerConnection *cnc);
GdaServerRecordset* gda_postgres_connection_open_schema (GdaServerConnection *cnc,
                                                          GdaServerError *error,
                                                          GDA_Connection_QType t,
                                                          GDA_Connection_Constraint *constraints,
                                                          gint length);
glong     gda_postgres_connection_modify_schema (GdaServerConnection *cnc,
                                   GDA_Connection_QType t,
                                   GDA_Connection_Constraint *constraints,
                                   gint length);
gint          gda_postgres_connection_start_logging (GdaServerConnection *cnc,
                                                     const gchar *filename);
gint          gda_postgres_connection_stop_logging (GdaServerConnection *cnc);
gchar*        gda_postgres_connection_create_table (GdaServerConnection *cnc,
                                                    GDA_RowAttributes *columns);
gboolean      gda_postgres_connection_supports (GdaServerConnection *cnc,
                                                GDA_Connection_Feature feature);
GDA_ValueType gda_postgres_connection_get_gda_type (GdaServerConnection *cnc,
                                                    gulong sql_type);
gshort        gda_postgres_connection_get_c_type (GdaServerConnection *cnc,
                                                  GDA_ValueType type);
gchar*        gda_postgres_connection_sql2xml (GdaServerConnection *cnc,
                                               const gchar *sql);
gchar*        gda_postgres_connection_xml2sql (GdaServerConnection *cnc,
                                               const gchar *xml);
void          gda_postgres_connection_free (GdaServerConnection *cnc);


/**/
gboolean             gda_postgres_command_new (GdaServerCommand *cmd);
GdaServerRecordset* gda_postgres_command_execute (GdaServerCommand *cmd,
                                                   GdaServerError *error,
                                                   const GDA_CmdParameterSeq *params,
                                                   gulong *affected,
                                                   gulong options);
void                 gda_postgres_command_free (GdaServerCommand *cmd);


/**/
gboolean gda_postgres_recordset_new       (GdaServerRecordset *recset);
gint     gda_postgres_recordset_move_next (GdaServerRecordset *recset);
gint     gda_postgres_recordset_move_prev (GdaServerRecordset *recset);
gint     gda_postgres_recordset_close     (GdaServerRecordset *recset);
void     gda_postgres_recordset_free      (GdaServerRecordset *recset);

/**/
void gda_postgres_error_make (GdaServerError *error,
                           GdaServerRecordset *recset,
                           GdaServerConnection *cnc,
                           gchar *where);

/*
 * utility functions
 */

/* types conversion utilities */
gulong        gda_postgres_connection_get_sql_type(POSTGRES_Connection *cnc,
                                                   gchar *postgres_type);
GDA_ValueType gda_postgres_connection_get_gda_type_psql (POSTGRES_Connection *cnc,
                                                         gulong sql_type);
gshort        gda_postgres_connection_get_c_type_psql (POSTGRES_Connection *cnc,
                                                       GDA_ValueType type);
GSList *      convert_tabular_to_list(gchar *tab);
GdaServerRecordset *
gda_postgres_command_build_recset_with_builtin (GdaServerConnection *cnc,
                                                GdaBuiltin_Result *res,
                                                gulong *affected);

/* data replacement utilities */
gchar * replace_PROV_TYPES_with_gdatype(POSTGRES_Recordset *recset,
                                        gchar * oid);
gchar * replace_TABLE_NAME_with_SQL(POSTGRES_Recordset *recset,
                                    gchar *table);
gchar * replace_SEQUENCE_NAME_with_SQL(POSTGRES_Recordset *recset,
                                       gchar *seq);
gchar * replace_FUNCTION_OID_with_SQL(POSTGRES_Recordset *recset,
                                      gchar *fnoid);
gchar * replace_AGGREGATE_OID_with_SQL(POSTGRES_Recordset *recset,
                                       gchar *oid);
gchar * replace_TABLE_FIELD_with_length(POSTGRES_Recordset *recset,
                                        gchar *tf);
gchar * replace_TABLE_FIELD_with_defaultval(POSTGRES_Recordset *recset,
                                            gchar *tf);
gchar * replace_TABLE_FIELD_with_iskey(POSTGRES_Recordset *recset,
                                       gchar *tf);

#endif

