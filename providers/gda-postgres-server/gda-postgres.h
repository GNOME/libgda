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
  Gda_Builtin_Result*           btin_res; 
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
gboolean gda_postgres_connection_new (Gda_ServerConnection *cnc);
gint     gda_postgres_connection_open (Gda_ServerConnection *cnc,
				       const gchar *dsn,
				       const gchar *user,
				       const gchar *password);
void     gda_postgres_connection_close (Gda_ServerConnection *cnc);
gint     gda_postgres_connection_begin_transaction (Gda_ServerConnection *cnc);
gint     gda_postgres_connection_commit_transaction(Gda_ServerConnection *cnc);
gint gda_postgres_connection_rollback_transaction (Gda_ServerConnection *cnc);
Gda_ServerRecordset* gda_postgres_connection_open_schema (Gda_ServerConnection *cnc,
							  Gda_ServerError *error,
							  GDA_Connection_QType t,
							  GDA_Connection_Constraint *constraints,
							  gint length);
gint          gda_postgres_connection_start_logging (Gda_ServerConnection *cnc,
						     const gchar *filename);
gint          gda_postgres_connection_stop_logging (Gda_ServerConnection *cnc);
gchar*        gda_postgres_connection_create_table (Gda_ServerConnection *cnc,
						    GDA_RowAttributes *columns);
gboolean      gda_postgres_connection_supports (Gda_ServerConnection *cnc,
						GDA_Connection_Feature feature);
GDA_ValueType gda_postgres_connection_get_gda_type (Gda_ServerConnection *cnc,
						    gulong sql_type);
gshort        gda_postgres_connection_get_c_type (Gda_ServerConnection *cnc,
						  GDA_ValueType type);
void          gda_postgres_connection_free (Gda_ServerConnection *cnc);


/**/
gboolean             gda_postgres_command_new (Gda_ServerCommand *cmd);
Gda_ServerRecordset* gda_postgres_command_execute (Gda_ServerCommand *cmd,
						   Gda_ServerError *error,
						   const GDA_CmdParameterSeq *params,
						   gulong *affected,
						   gulong options);
void                 gda_postgres_command_free (Gda_ServerCommand *cmd);


/**/
gboolean gda_postgres_recordset_new       (Gda_ServerRecordset *recset);
gint     gda_postgres_recordset_move_next (Gda_ServerRecordset *recset);
gint     gda_postgres_recordset_move_prev (Gda_ServerRecordset *recset);
gint     gda_postgres_recordset_close     (Gda_ServerRecordset *recset);
void     gda_postgres_recordset_free      (Gda_ServerRecordset *recset);

/**/
void gda_postgres_error_make (Gda_ServerError *error,
			   Gda_ServerRecordset *recset,
			   Gda_ServerConnection *cnc,
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
Gda_ServerRecordset *
gda_postgres_command_build_recset_with_builtin (Gda_ServerConnection *cnc,
						Gda_Builtin_Result *res,  
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
