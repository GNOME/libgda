/* 
 * $Id$
 *
 * GNOME DB Tds Provider
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

// $Log$
// Revision 1.2  2000/11/21 19:57:14  holger
// 2000-11-21 Holger Thon <holger@gidayu.max.uni-duisburg.de>
//
// 	- Fixed missing parameter in gda-sybase-server
// 	- Removed ct_diag/cs_diag stuff completly from tds server
//
// Revision 1.1  2000/11/04 23:42:17  holger
// 2000-11-05  Holger Thon <gnome-dms@users.sourceforge.net>
//
//   * Added gda-tds-server
//

#include "gda-tds.h"
#include "ctype.h"

typedef Gda_ServerRecordset* (*schema_ops_fn)(Gda_ServerError *,
                                              Gda_ServerConnection *,
                                              GDA_Connection_Constraint *,
                                              gint);

schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] = {
  NULL,
};

// schema definitions
static Gda_ServerRecordset*
schema_cols (Gda_ServerError *,
             Gda_ServerConnection *,
             GDA_Connection_Constraint *,
             gint);
static Gda_ServerRecordset*
schema_procs(Gda_ServerError *,
             Gda_ServerConnection *,
             GDA_Connection_Constraint *,
             gint);
static Gda_ServerRecordset*
schema_tables (Gda_ServerError *,
               Gda_ServerConnection *,
               GDA_Connection_Constraint *,
               gint);
static Gda_ServerRecordset*
schema_types (Gda_ServerError *,
              Gda_ServerConnection *,
              GDA_Connection_Constraint *,
              gint);



// Further non-public definitions
static gint
gda_tds_init_dsn_properties(Gda_ServerConnection *, const gchar *);
static gboolean
gda_tds_set_locale(Gda_ServerConnection *, const gchar *);

//  utility functions
static gchar *
get_value(gchar *ptr);
static gchar *
get_option(const gchar *dsn, const gchar *opt_name);


gboolean
gda_tds_connection_new(Gda_ServerConnection *cnc)
{
  static gboolean initialized;
  tds_Connection *tcnc = NULL;
  gint              i;
  
  if (!initialized) {
    for (i=GDA_Connection_GDCN_SCHEMA_AGGREGATES;
         i<= GDA_Connection_GDCN_SCHEMA_LAST;
         i++) {
      schema_ops[i] = NULL;
    }
   
    schema_ops[GDA_Connection_GDCN_SCHEMA_COLS]       = schema_cols;
    schema_ops[GDA_Connection_GDCN_SCHEMA_PROCS]      = schema_procs;
    schema_ops[GDA_Connection_GDCN_SCHEMA_TABLES]     = schema_tables;
    schema_ops[GDA_Connection_GDCN_SCHEMA_PROV_TYPES] = schema_types;
  }
  initialized = TRUE;
  
  tcnc = g_new0(tds_Connection, 1);
  g_return_val_if_fail(tcnc != NULL, FALSE);
  
  if (tcnc) {
    // Initialize connection struct
	 //   Context and connection
    tcnc->ctx      = (CS_CONTEXT *)NULL;
    tcnc->cnc      = (CS_CONNECTION *)NULL;
	 //   Error handling
    tcnc->ret              = CS_SUCCEED;
    tcnc->serr.udeferr_msg = (gchar *) NULL;
    tcnc->database         = (gchar *) NULL;
    
    // Further initialization, bail out on error
    //   tcnc->error = gda_server_error_new();
    //   if (!tcnc->error) {
    //      g_free((gpointer) tcnc);
    //      gda_log_error(_("Error allocating error structure"));
    //      return FALSE;
    //   }
    
    // Succeeded
    gda_server_connection_set_user_data(cnc, (gpointer) tcnc);
    return TRUE;
  } else {
    gda_log_error(_("Not enough memory for tds_Connection"));
    return FALSE;
  }
  
  return FALSE;
}

gint
gda_tds_connection_open(Gda_ServerConnection *cnc,
                        const gchar* dsn,
                        const gchar* user,
                        const gchar* passwd)
{
  tds_Connection *tcnc = NULL;
  
  g_return_val_if_fail(cnc != NULL, -1);
  gda_server_connection_set_username(cnc, user);
  gda_server_connection_set_password(cnc, passwd);
  gda_server_connection_set_dsn(cnc, dsn);
  tcnc = (tds_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(tcnc != NULL, -1);
  
  // Free any error messages left in user defined connection struct
  if (tcnc->serr.udeferr_msg) {
    g_free((gpointer) tcnc->serr.udeferr_msg);
  }
  if (tcnc->database) {
    g_free((gpointer) tcnc->database);
  }

  // Establish connection
  if (gda_tds_connection_reopen(cnc) != TRUE) {
    return -1;
  }

  return 0;
}

void
gda_tds_connection_close(Gda_ServerConnection *cnc)
{
  tds_Connection *tcnc;
  
  g_return_if_fail(cnc != NULL);
  tcnc = (tds_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_if_fail(tcnc != NULL);
  
  // Close connection
  if (tcnc->cnc) {
    if ((tcnc->ret = ct_close(tcnc->cnc, CS_UNUSED)
        ) != CS_SUCCEED
       ) {
      gda_tds_cleanup(tcnc, tcnc->ret,
                      "Could not close connection\n");
      return;
    }
    if ((tcnc->ret = ct_con_drop(tcnc->cnc)
        ) != CS_SUCCEED
       ) {
      gda_tds_cleanup(tcnc, tcnc->ret,
                      "Could not drop connection structure\n");
      return;
    }
    tcnc->cnc = (CS_CONNECTION *) NULL;
  }
  
  if (tcnc->ctx) {
    // Exit client library and drop context structure
    if ((tcnc->ret = ct_exit(tcnc->ctx, CS_UNUSED)
        ) != CS_SUCCEED
       ) {
      gda_tds_cleanup(tcnc, tcnc->ret, "ct_exit failed");
      return;
    }
    
    if ((tcnc->ret = cs_ctx_drop(tcnc->ctx)
        ) != CS_SUCCEED
       ) {
      gda_tds_cleanup(tcnc, tcnc->ret, "cs_ctx_drop failed");
      return;
    }
    tcnc->ctx = (CS_CONTEXT *) NULL;
  }
}

gint
gda_tds_connection_begin_transaction(Gda_ServerConnection *cnc)
{
  // FIXME: Implement transaction code
  return -1;
}

gint
gda_tds_connection_commit_transaction(Gda_ServerConnection *cnc)
{
  // FIXME: Implement transaction code
  return -1;
}

gint
gda_tds_connection_rollback_transaction(Gda_ServerConnection *cnc)
{
  // FIXME: Implement transaction code
  return -1;
}

Gda_ServerRecordset *
gda_tds_connection_open_schema (Gda_ServerConnection *cnc,
                                Gda_ServerError *error,
                                GDA_Connection_QType t,
                                GDA_Connection_Constraint *constraints,
                                gint length)
{
  schema_ops_fn fn;
  
  g_return_val_if_fail(cnc != NULL, NULL);
  
  fn = schema_ops[(gint) t];
  if (fn) {
    return fn(error, cnc, constraints, length);
  } else {
    gda_log_error(_("Unhandled SCHEMA_QTYPE %d"), (gint) t);
  }
  
  return NULL;
}

gint
gda_tds_connection_start_logging(Gda_ServerConnection *cnc,
                                    const gchar *filename)
{
  // FIXME: Implement gda logging facility
  return -1;
}

gint
gda_tds_connection_stop_logging(Gda_ServerConnection *cnc)
{
  // FIXME: Implement gda logging facility
  return -1;
}

gchar *
gda_tds_connection_create_table (Gda_ServerConnection *cnc,
                                   GDA_RowAttributes *columns)
{
  // FIXME: Implement creation of tables
  return NULL;
}

gboolean
gda_tds_connection_supports(Gda_ServerConnection *cnc,
                               GDA_Connection_Feature feature)
{
  g_return_val_if_fail(cnc != NULL, FALSE);

  switch (feature) {
  case GDA_Connection_FEATURE_SQL:
    return TRUE;
  case GDA_Connection_FEATURE_FOREIGN_KEYS:
  case GDA_Connection_FEATURE_INHERITANCE:
  case GDA_Connection_FEATURE_OBJECT_ID:
  case GDA_Connection_FEATURE_PROCS:
  case GDA_Connection_FEATURE_SEQUENCES:
  case GDA_Connection_FEATURE_SQL_SUBSELECT:
  case GDA_Connection_FEATURE_TRANSACTIONS:
  case GDA_Connection_FEATURE_TRIGGERS:
  case GDA_Connection_FEATURE_VIEWS:
  case GDA_Connection_FEATURE_XML_QUERIES:
  default:
    return FALSE;
  }
  
  return FALSE;
}

const GDA_ValueType
gda_tds_connection_get_gda_type(Gda_ServerConnection *cnc,
                                   gulong sql_type)
{
  return tds_get_gda_type((CS_INT) sql_type);
}

const CS_INT
gda_tds_connection_get_sql_type (Gda_ServerConnection *cnc,
                                    GDA_ValueType gda_type)
{
  return tds_get_sql_type(gda_type);
}

const gshort
gda_tds_connection_get_c_type (Gda_ServerConnection *cnc,
                                  GDA_ValueType gda_type)
{
  return tds_get_c_type(gda_type);
}

void
gda_tds_connection_free (Gda_ServerConnection *cnc)
{
  tds_Connection* tcnc;
  
  g_return_if_fail(cnc != NULL);
  
  tcnc = (tds_Connection *) gda_server_connection_get_user_data(cnc);
  if (tcnc) {
    gda_tds_connection_clear_user_data(cnc, FALSE);
    g_free((gpointer) tcnc);
    gda_server_connection_set_user_data(cnc, (gpointer) NULL);
  }
}

void
gda_tds_connection_clear_user_data(Gda_ServerConnection *cnc,
                                   gboolean force_set_null_pointers) {
  tds_Connection *tcnc = NULL;

  g_return_if_fail(cnc != NULL);
  tcnc = (tds_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_if_fail(tcnc != NULL);
  
  // we do not touch CS_* variables and *_diag variables,
  // as they are essential for the connection
  
  tcnc->serr.err_type = TDS_ERR_NONE;
  if (force_set_null_pointers) {
    tcnc->serr.udeferr_msg = (gchar *) NULL;
    tcnc->database = (gchar *) NULL;
  } else {
      }
}

/*
 * Schema functions
 */

static Gda_ServerRecordset *
schema_cols (Gda_ServerError *error,
             Gda_ServerConnection *cnc,
             GDA_Connection_Constraint *constraints,
             gint length)
{
  gchar*                     query = NULL;
  Gda_ServerCommand*         cmd = NULL;
  Gda_ServerRecordset*       recset = NULL;
  GDA_Connection_Constraint* ptr = NULL;
  gint                       cnt;
  gulong                     affected = 0;
  gchar                      *table_name = NULL;
  
  /* process constraints */
  ptr = constraints;
  for (cnt = 0; cnt < length && ptr != NULL; cnt++) {
    switch (ptr->ctype) {
    case GDA_Connection_OBJECT_NAME:
      if (table_name) {
        g_free((gpointer) table_name);
      }
#ifdef TDS_DEBUG
      gda_log_message(_("schema_cols: table_name '%s'"), ptr->value);
#endif
      table_name = g_strdup_printf("%s", ptr->value);
      break;
    default :
    }
  }

  /* build command object and query */
  if (!table_name) {
    gda_log_error(_("No table name given for SCHEMA_COLS"));
    return NULL;
  }
  cmd = gda_server_command_new(cnc);
  if (!cmd) {
    g_free((gpointer) table_name);
    gda_log_error(_("Could not allocate command structure"));
    return NULL;
  }
  query = g_strdup_printf("SELECT c.name AS \"Name\", "
#ifdef TDS_DEBUG
                          "       c.colid, "
#endif
                          "       t.name AS \"Type\", "
                          "       c.length AS \"Size\", "
                          "       c.prec AS \"Precision\", "
                          "       t.allownulls AS \"Nullable\", "
                          "       null AS \"Comments\", "
                          "       c.domain, "
                          "       c.printfmt "
                          "  FROM syscolumns c, sysobjects o "
                          "       ,systypes t "
                          "    WHERE c.id = o.id "
                          "      AND c.usertype = t.usertype "
                          "    HAVING o.name = '%s' "
                          "  ORDER BY c.colid ASC", table_name);
  
  if (!query) {
    gda_log_error(_("Could not allocate enough memory for query"));
    gda_server_command_free(cmd);
    g_free((gpointer) table_name);
    return NULL;
  }
  gda_server_command_set_text(cmd, query);
  recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
  g_free((gpointer) query);
  g_free((gpointer) table_name);
  
  if (recset) {
    return recset;
  } else {
//    gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
    gda_log_error(_("Could not get recordset in schema_cols"));
  }
  
  return NULL;
}

static Gda_ServerRecordset *
schema_procs (Gda_ServerError *error,
              Gda_ServerConnection *cnc,
              GDA_Connection_Constraint *constraints,
              gint length)
{
  GString*                   query = NULL;
  Gda_ServerCommand*         cmd = NULL;
  Gda_ServerRecordset*       recset = NULL;
  GDA_Connection_Constraint* ptr = NULL;
  gboolean                   extra_info = FALSE;
  gint                       cnt;
  GString*                   condition = NULL;
  gulong                     affected = 0;

  /* process constraints */
  ptr = constraints;
  for (cnt = 0; cnt < length && ptr != NULL; cnt++) {
    switch (ptr->ctype) {
      //   case GDA_Connection_EXTRA_INFO :
      //               extra_info = TRUE;
      //            break;
      //         case GDA_Connection_OBJECT_NAME :
      //               if (!condition) condition = g_string_new("");
      //               g_string_sprintfa(condition, "AND a.relname='%s' ", ptr->value);
      //#ifdef TDS_DEBUG
      //               gda_log_message(_("schema_tables: table name = '%s'"),
      //                               ptr->value);
      //#endif
      //            break;
      //         case GDA_Connection_OBJECT_SCHEMA :
      //               if (!condition) condition = g_string_new("");
      //               g_string_sprintfa(condition, "AND b.usename='%s' ", ptr->value);
      //#ifdef TDS_DEBUG
      //               gda_log_message(_("schema_tables: table_schema = '%s'"),
      //                               ptr->value);
      //#endif
      //            break;
      //         case GDA_Connection_OBJECT_CATALOG :
      //               gda_log_error(_("schema_procedures: proc_catalog = '%s' UNUSED!\n"), 
      //                       ptr->value);
      //            break;
    default :
      gda_log_error(_("schema_procs: invalid constraint type %d"), 
                    ptr->ctype);
      g_string_free(condition, TRUE);
      return NULL;
    }
    ptr++;
  }
  
  cmd = gda_server_command_new(cnc);
  if (!cmd) {
    gda_log_error(_("Could not allocate cmd structure"));
    return NULL;
  }
  
  // Get all usertables and systables
  query = g_string_new("SELECT o.name AS \"Name\" "
//                       "       null   AS \"Comment\" "
                       "FROM sysobjects o "
                       "  WHERE o.type = \"P\" "
                       "ORDER BY o.type DESC, "
                       "         o.name ASC");
  
  if (!query) {
    gda_server_command_free(cmd);
    return NULL;
  }
  gda_server_command_set_text(cmd, query->str);
  recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
  g_string_free(query, TRUE);
//  gda_server_command_free(cmd);
  
  return recset; 
}

static Gda_ServerRecordset *
schema_tables (Gda_ServerError *error,
               Gda_ServerConnection *cnc,
               GDA_Connection_Constraint *constraints,
               gint length)
{
  GString*                   query = NULL;
  Gda_ServerCommand*         cmd = NULL;
  Gda_ServerRecordset*       recset = NULL;
  GDA_Connection_Constraint* ptr = NULL;
  gboolean                   extra_info = FALSE;
  gint                       cnt;
  GString*                   condition = NULL;
  gulong                     affected = 0;
  
  /* process constraints */
  ptr = constraints;
  for (cnt = 0; cnt < length && ptr != NULL; cnt++) {
    switch (ptr->ctype) {
      //         case GDA_Connection_EXTRA_INFO :
      //               extra_info = TRUE;
      //            break;
      //         case GDA_Connection_OBJECT_NAME :
      //               if (!condition) condition = g_string_new("");
      //               g_string_sprintfa(condition, "AND a.relname='%s' ", ptr->value);
      //#ifdef TDS_DEBUG
      //               gda_log_message(_("schema_tables: table name = '%s'"),
      //                               ptr->value);
      //#endif
      //            break;
      //         case GDA_Connection_OBJECT_SCHEMA :
      //               if (!condition) condition = g_string_new("");
      //               g_string_sprintfa(condition, "AND b.usename='%s' ", ptr->value);
      //#ifdef TDS_DEBUG
      //               gda_log_message(_("schema_tables: table_schema = '%s'"),
      //                               ptr->value);
      //#endif
      //            break;
      //         case GDA_Connection_OBJECT_CATALOG :
      //               gda_log_error(_("schema_procedures: proc_catalog = '%s' UNUSED!"), 
      //                             ptr->value);
      //            break;
    default :
      gda_log_error(_("schema_tables: invalid constraint type %d"), 
          ptr->ctype);
      g_string_free(condition, TRUE);
      return NULL;
    }
    ptr++;
  }
  
  cmd = gda_server_command_new(cnc);
  if (!cmd) {
    return NULL;
  }
  
  // Get all usertables and systables
  query = g_string_new("SELECT o.name AS \"Name\" "
             //                        "       null   AS \"Comment\" "
             "FROM sysobjects o "
             "  WHERE o.type = \"U\" "
             "     OR o.type = \"S\" "
             "ORDER BY o.type DESC, "
             "         o.name ASC");
  
  if (!query) {
    gda_server_command_free(cmd);
    return NULL;
  }
  gda_server_command_set_text(cmd, query->str);
  recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
  g_string_free(query, TRUE);
  //   gda_server_command_free(cmd);
  
  return recset; 
}

static Gda_ServerRecordset *
schema_types(Gda_ServerError *error,
             Gda_ServerConnection *cnc,
             GDA_Connection_Constraint *constraints,
             gint length)
{
  Gda_ServerCommand         *cmd = NULL;
  Gda_ServerRecordset       *rec = NULL;
  GDA_Connection_Constraint *ptr = NULL;
  gint                      i   = 0;
  GString                   *query = NULL;
  GString                   *condition = NULL;
  gulong                    affected = 0;
  
  ptr = constraints;
  
  for (i = 0; i < length && ptr != NULL; i++) {
    switch (ptr->ctype) {
    case GDA_Connection_OBJECT_NAME:
      if (!condition) {
   condition = g_string_new("  WHERE ");
      } else {
   g_string_sprintfa(condition, "    AND ");
      }
      g_string_sprintfa(condition, "a.name='%s' ", ptr->value);
      break;
    default:
      gda_log_error(_("Invalid or unsupported constraint type: %d"),
          ptr->ctype);
      break;
    }
    ptr++;
  }
  
  cmd = gda_server_command_new(cnc);
  if (!cmd) {
    g_string_free(condition, TRUE);
    return NULL;
  }
  
  query = g_string_new("SELECT a.name"
             //                        "       a.usertype,"
             //                        "       a.variable,"
             //                        "       a.allownulls,"
             //                        "       a.type,"
             //                        "       a.length,"
             //                        "       a.tdefault,"
             //                        "       a.domain,"
             //                        "       a.printfmt"
             "  FROM systypes a ");
  
  if (!query) {
    gda_server_command_free(cmd);
    g_string_free(condition, TRUE);
    return NULL;
  }
  
  if (condition) {
    g_string_free(condition, TRUE);
  }
  g_string_append(query, "  ORDER BY a.name");
  cmd = gda_server_command_new(cnc);
  if (cmd) {
    gda_server_command_set_text(cmd, query->str);
    g_string_free(query, TRUE);
    rec = gda_server_command_execute(cmd, error, NULL, &affected, 0);
    return rec;
  } else {
    gda_log_error(_("Could not allocate Gda_ServerCommand"));
    g_string_free(query, TRUE);
  }
  
  return rec;
}

/*
schema_views
{
select o.name AS Name 
from sysobjects o
where o.type = 'V'
}
*/

gboolean
gda_tds_connection_dead(Gda_ServerConnection *cnc)
{
  tds_Connection *tcnc;
  CS_INT            con_status = 0;
  
  g_return_val_if_fail(cnc != NULL, FALSE);
  tcnc = (tds_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(tcnc != NULL, FALSE);
  g_return_val_if_fail(tcnc->cnc != NULL, FALSE);
  
  if((tcnc->ret = ct_con_props(tcnc->cnc, CS_GET, CS_CON_STATUS,
                &con_status, CS_UNUSED,
                NULL)) != CS_SUCCEED) {
    gda_log_message(_("connection status: %x"), con_status);
    gda_log_message(_("return code: %x"), tcnc->ret);
    return TRUE;
  }
  
  gda_log_message(_("connection status: %x"), con_status);
  
  // check for CS_CONSTAT_CONNECTED bit
  if ((con_status && CS_CONSTAT_CONNECTED) == CS_CONSTAT_CONNECTED) {
    return FALSE;
  }
  
  return TRUE;
}

// do NOT call this before having freed Gda_ServerCommand structures
// associated to the given connection
gboolean
gda_tds_connection_reopen(Gda_ServerConnection *cnc)
{
  tds_Connection    *tcnc;
  gboolean             first_connection = TRUE;
  CS_CHAR              buf[CS_MAX_CHAR];
  gchar                *dsn;
  
  // At least we need valid connection structs
  g_return_val_if_fail(cnc != NULL, FALSE);
  tcnc = (tds_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(tcnc != NULL, FALSE);
  
  ////////////////////////////////////////////////////////
  // Drop old connection data, if any
  ////////////////////////////////////////////////////////
  
  if (tcnc->cnc) {
    first_connection = FALSE;
    gda_log_error(_("attempting reconnection"));
    gda_tds_connection_close(cnc);
  }
  
  ////////////////////////////////////////////////////////
  // Initialize connection
  ////////////////////////////////////////////////////////
  
  // Create context info and initialize ctlib
  if ((tcnc->ret = cs_ctx_alloc(CS_VERSION_100, &tcnc->ctx)) != CS_SUCCEED) {
    gda_tds_cleanup(tcnc, tcnc->ret, "Could not allocate context");
    return FALSE;
  }
  if ((tcnc->ret = ct_init(tcnc->ctx, CS_VERSION_100)) != CS_SUCCEED) {
    gda_tds_cleanup(tcnc, tcnc->ret, "Could not initialize client library");
    return FALSE;
  }
  
  // locales must be set up before we have any connection,
  // so get dsn and initialize locale if given
  dsn = gda_server_connection_get_dsn(cnc);
  if ((gda_tds_set_locale(cnc, dsn)) != TRUE) {
    gda_tds_cleanup(tcnc, tcnc->ret, "Could not set locale");
    return FALSE;
  }
  
  // Initialize connection
  if ((tcnc->ret = ct_con_alloc(tcnc->ctx, &tcnc->cnc)) != CS_SUCCEED) {
    gda_tds_cleanup(tcnc, tcnc->ret,
             "Could not allocate connection info");
    return FALSE;
  }
  
  // Now we've got an initialized CS_CONTEXT and an allocated CS_COMMAND
  // and maybe setup the locale, so we can setup error handling
  
  if (gda_tds_install_error_handlers(cnc) != 0) {
    return FALSE;
  }
  
  
  // We go on proceeding connection properties and dsn arguments
  // set up application name, username and password
  if ((tcnc->ret = ct_con_props(tcnc->cnc, CS_SET, CS_APPNAME,
                                (CS_CHAR *) GDA_TDS_DEFAULT_APPNAME,
                                CS_NULLTERM, NULL
                               )
      ) != CS_SUCCEED
     ) {
    gda_tds_cleanup(tcnc, tcnc->ret, "Could not set application name");
    return FALSE;
  }
  if ((tcnc->ret = ct_con_props(tcnc->cnc, CS_SET, CS_USERNAME,
                           (CS_CHAR*) gda_server_connection_get_username(cnc),
                                CS_NULLTERM, NULL
                               )
      ) != CS_SUCCEED
     ) {
    gda_tds_cleanup(tcnc, tcnc->ret, "Could not set user name");
    return FALSE;
  }
  if ((tcnc->ret = ct_con_props(tcnc->cnc, CS_SET, CS_PASSWORD,
                           (CS_CHAR *) gda_server_connection_get_password(cnc),
                                CS_NULLTERM, NULL
                               )
      ) != CS_SUCCEED
     ) {
    gda_tds_cleanup(tcnc, tcnc->ret, "Could not set password");
    return FALSE;
  }
  
  // Proceed further parameters given in dsn
  if (((dsn = gda_server_connection_get_dsn(cnc)) != NULL) &&
      (gda_tds_init_dsn_properties(cnc, dsn) != 0)) {
    return FALSE;
  }
  
  // Connect
  if ((tcnc->ret = ct_connect(tcnc->cnc, (CS_CHAR *) NULL, 0)
      ) != CS_SUCCEED
     ) {
    gda_tds_cleanup(tcnc, tcnc->ret, 
             (first_connection == FALSE)
             ? "Could not reconnect"
             : "Could not connect"
             );
    return FALSE;
  }

  
  // To test the connection, we ask for the servers name
  // FIXME: freetds unimplemented con_props: (CS_INT)9146 = CS_SERVERNAME
//  if ((tcnc->ret = ct_con_props(tcnc->cnc, CS_GET, (CS_INT)9146,
//                                &buf, CS_MAX_CHAR, NULL
//                               )
//      ) != CS_SUCCEED
//     ) {
//    gda_tds_cleanup(tcnc, tcnc->ret,
//             "Could not request servername");
//    return FALSE;
//  } else {
    if (first_connection == FALSE)
      // Fixme: replace "dummy" with buf when CS_GET CS_SERVERNAME works
      gda_log_message(_("Reconnected to %s"), "dummy");
    else
      gda_log_message(_("Connected to %s"), "dummy");

	 gda_tds_connection_select_database(cnc, "pubs2");
	 gda_tds_connection_select_database(cnc, "pubs3");
    if (tcnc->database) {
      tcnc->ret = gda_tds_connection_select_database(cnc, tcnc->database);
      if (tcnc->ret != CS_SUCCEED) {
        gda_tds_cleanup(tcnc, tcnc->ret, "Could not select database");
        return FALSE;
      } else {
        gda_log_message(_("Selected database '%s'"), tcnc->database);
      }
    }
//  }

  // We have connected successfully
  return TRUE;
}

CS_RETCODE
gda_tds_connection_select_database(Gda_ServerConnection *cnc,
                                   const gchar *database)
{
  tds_Connection *tcnc = NULL;
  CS_COMMAND        *cmd = NULL;
  gchar             *sqlcmd = NULL;
  CS_INT            result_type;
  gboolean          okay = FALSE;
  
  g_return_val_if_fail(cnc != NULL, CS_FAIL);
  tcnc = (tds_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(tcnc != NULL, CS_FAIL);
  g_return_val_if_fail(tcnc->cnc != NULL, CS_FAIL);
  
  if (ct_cmd_alloc(tcnc->cnc, &cmd) != CS_SUCCEED) {
    gda_log_error(_("%s: Could not allocate cmd structure"),
        __PRETTY_FUNCTION__
        );
    return CS_FAIL;
  }
  
  // Prepare cmd string
  sqlcmd = g_strdup_printf("use %s", database);
  if (!sqlcmd) {
    gda_log_error(_("%s: Could not allocate memory for cmd string"),
        __PRETTY_FUNCTION__
        );
    ct_cmd_drop(cmd);
    return CS_FAIL;
  }
  // Pass cmd string to CS_COMMAND structure
  if (ct_command(cmd, CS_LANG_CMD, sqlcmd, CS_NULLTERM, CS_UNUSED
                ) != CS_SUCCEED
     ) {
    gda_log_error(_("%s: ct_command failed"), __PRETTY_FUNCTION__);
    g_free((gpointer) sqlcmd);
    ct_cmd_drop(cmd);
    return CS_FAIL;
  }
  g_free((gpointer) sqlcmd);
  
  // Send the command
  if (ct_send(cmd) != CS_SUCCEED) {
    gda_log_error(_("%s: sending command failed"), __PRETTY_FUNCTION__);
    ct_cmd_drop(cmd);
    return CS_FAIL;
  }

/*
 * freetds bug: no ct_results() after non-result query
 * 
  while (CS_SUCCEED == ct_results(cmd, &result_type)) {
    gda_log_message(_("Result: %d"), result_type);
    switch (result_type) {
    case CS_CMD_FAIL:
      gda_log_error(_("%s: selecting database failed"),
          __PRETTY_FUNCTION__
          );
      break;
    case CS_CMD_SUCCEED:
      okay = TRUE;
      break;
    default:
      break; 
    }
  }
  
  ct_cmd_drop(cmd);
  
  if (okay) {
*/
    return CS_SUCCEED;


/* freetds bug: no ct_results() after non-result query
  }
  
  gda_log_error(_("%s: use command failed"), __PRETTY_FUNCTION__);
  return CS_FAIL;
*/
}

// Select locale for connection; we just need a CS_CONTEXT at this time
static gboolean
gda_tds_set_locale(Gda_ServerConnection *cnc, const gchar *dsn)
{
  tds_Connection *tcnc;
  CS_LOCALE         *locale;
  CS_RETCODE        ret = CS_FAIL;
  gchar             *targ_locale = NULL;
  
  g_return_val_if_fail(cnc != NULL, FALSE);
  tcnc = (tds_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(tcnc != NULL, FALSE);
  g_return_val_if_fail(tcnc->ctx != NULL, FALSE);
  
  if ((targ_locale = get_option(dsn, "LOCALE=")) == NULL) {
    return TRUE;
  }
  
#ifdef TDS_DEBUG
  gda_log_message("locale = '%s'", targ_locale);
#endif

  if ((ret = cs_loc_alloc(tcnc->ctx, &locale)) != CS_SUCCEED) {
    gda_log_error(_("Could not allocate locale structure"));
    return FALSE;
  }
  if ((ret = cs_locale(tcnc->ctx, CS_SET, locale, CS_LC_ALL,
             (CS_CHAR *) targ_locale, CS_NULLTERM, NULL))
      != CS_SUCCEED) {
    gda_log_error(_("Could not set locale to '%s'"), targ_locale);
    cs_loc_drop(tcnc->ctx, locale);
    return FALSE;
  }
  if ((ret = cs_config(tcnc->ctx, CS_SET, CS_LOC_PROP, locale,
             CS_UNUSED, NULL)) != CS_SUCCEED) {
    gda_log_error(_("Could not configure locale to be '%s'"), targ_locale);
    cs_loc_drop(tcnc->ctx, locale);
    return FALSE;
  }
  if ((ret = cs_loc_drop(tcnc->ctx, locale)) != CS_SUCCEED) {
    gda_log_error(_("Could not drop locale structure"));
    return FALSE;
  }
  gda_log_message("Selected locale '%s'", targ_locale);
  return TRUE;
}

// drop remaining cs/ctlib structures and exit
void
gda_tds_cleanup(tds_Connection *tcnc, CS_RETCODE ret, const gchar *msg)
{
  g_return_if_fail(tcnc != NULL);
  
  gda_log_error(_("Fatal error: %s\n"), msg);
  
  if (tcnc->cnc != NULL) {
    ct_con_drop(tcnc->cnc);
    tcnc->cnc = NULL;
  }
  
  if (tcnc->ctx != NULL) {
    ct_exit(tcnc->ctx, CS_FORCE_EXIT);
    cs_ctx_drop(tcnc->ctx);
    tcnc->ctx = NULL;
  }
}

/*
 * Private functions
 *
 *
 */

static gint
gda_tds_init_dsn_properties(Gda_ServerConnection *cnc, const gchar *dsn)
{
  tds_Connection *tcnc;
  gchar             *ptr_s, *ptr_e;
  
  g_return_val_if_fail(cnc != NULL, -1);
  tcnc = (tds_Connection *) gda_server_connection_get_user_data(cnc);
  
  if (tcnc) {
    /* parse connection string */
    ptr_s = (gchar *) dsn;
    while (ptr_s && *ptr_s)   {
      ptr_e = strchr(ptr_s, ';');
      
      if (ptr_e)
   *ptr_e = '\0';
      
      // HOST implies HOSTNAME ;-)
      if (strncasecmp(ptr_s, "HOST", strlen("HOST")) == 0)
        tcnc->ret = ct_con_props(tcnc->cnc, CS_SET, CS_HOSTNAME,
                                 (CS_CHAR *) get_value(ptr_s),
                                 CS_NULLTERM, NULL);
      else if (strncasecmp(ptr_s, "USERNAME", strlen("USERNAME")) == 0)
        tcnc->ret = ct_con_props(tcnc->cnc, CS_SET, CS_USERNAME,
                                 (CS_CHAR *) get_value(ptr_s),
                                 CS_NULLTERM, NULL);
      else if (strncasecmp(ptr_s, "APPNAME", strlen("APPNAME")) == 0)
        tcnc->ret = ct_con_props(tcnc->cnc, CS_SET, CS_APPNAME,
                                 (CS_CHAR *) get_value(ptr_s),
                                 CS_NULLTERM, NULL);
      else if (strncasecmp(ptr_s, "PASSWORD", strlen("PASSWORD")) == 0)
        tcnc->ret = ct_con_props(tcnc->cnc, CS_SET, CS_PASSWORD,
                                 (CS_CHAR *) get_value(ptr_s),
                                 CS_NULLTERM, NULL);
      else if (strncasecmp(ptr_s, "DATABASE", strlen("DATABASE")) == 0) {
        tcnc->ret = CS_SUCCEED;
        if (tcnc->database) {
          g_free((gpointer) tcnc->database);
        }
        tcnc->database = g_strdup_printf("%s", get_value(ptr_s));
      }
      
      if (tcnc->ret != CS_SUCCEED) {
        gda_tds_cleanup(tcnc, tcnc->ret, "Could not proceed dsn settings.\n");
        return -1;
      }
      
      ptr_s = ptr_e;
      if (ptr_s)
        ptr_s++;
    }
  }
  
  return 0;
}

/*
 * Utility functions
 */

static gchar *
get_value(gchar* ptr)
{
  while (*ptr && (*ptr != '='))
    ptr++;
   
  if (!*ptr) {
    return NULL;
  }
  ptr++;
  if (!*ptr) {
    return NULL;
  }
  while (*ptr && isspace(*ptr))
    ptr++;
  
  return (g_strdup(ptr));
}

static gchar *
get_option(const gchar *dsn, const gchar *opt_name)
{
  gchar *ptr = NULL;
  gchar *opt = NULL;
  
  if (!(opt = strstr(dsn, opt_name))) {
    return NULL;
  }
  opt += strlen(opt_name);
  ptr = strchr(opt, ';');
  
  if (ptr)
    *ptr = '\0';
  
  return (g_strdup(opt));
}
