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

#include "gda-primebase.h"

typedef GdaServerRecordset* (*schema_ops_fn)(GdaError *,
                                              GdaServerConnection *,
                                              GDA_Connection_Constraint *,
                                              gint);

schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] = {
  NULL,
};

static gchar*
  get_value (gchar* ptr);

static void
  gda_primebase_update_dsn_parameters(GdaServerConnection *cnc,
                                      const gchar *dsn);

static const primebase_Types gda_primebase_type_list[GDA_PRIMEBASE_TYPE_CNT] = {
  { "TINYINT",    PB_TINYINT,   GDA_TypeSmallint }, //  8-bit
  { "SMALLINT",   PB_SMINT,     GDA_TypeSmallint }, // 16-bit
  { "SMINT",      PB_SMINT,     GDA_TypeSmallint }, // 16-bit
  { "INTEGER",    PB_INTEGER,   GDA_TypeInteger  }, // 32-bit
  { "INT",        PB_INTEGER,   GDA_TypeInteger  }, // 32-bit   ( 5)
  
  { "DECIMAL",    PB_DECIMAL,   GDA_TypeDecimal     },
  { "MONEY",      PB_MONEY,     GDA_TypeCurrency    },

  { "SMALLFLOAT", PB_SMFLOAT,   GDA_TypeSingle      }, //  4-byte
  { "SMFLOAT",    PB_SMFLOAT,   GDA_TypeSingle      }, //  4-byte
  { "REAL",       PB_SMFLOAT,   GDA_TypeSingle      }, //  4-byte  (10)
  { "FLOAT",      PB_FLOAT,     GDA_TypeDouble      }, //  8-byte

  // What is on $maybe?
  { "BOOLEAN",    PB_BOOLEAN,   GDA_TypeBoolean     },
  
  { "DATE",       PB_DATE,      GDA_TypeDbDate      }, //          (15)
  { "TIME",       PB_TIME,      GDA_TypeDbTime      },
  { "TIMESTAMP",  PB_TIMESTAMP, GDA_TypeDbTimestamp },
  { "DATETIME",   PB_TIMESTAMP, GDA_TypeDbTimestamp },

  { "CHAR",       PB_CHAR,      GDA_TypeChar        },
  { "VARCHAR",    PB_VCHAR,     GDA_TypeVarchar     }, //          (20)
  
  { "BINARY",     PB_BIN,       GDA_TypeVarbin      },
  { "BIN",        PB_BIN,       GDA_TypeVarbin      },
  { "VARBINARY",  PB_VBIN,      GDA_TypeVarbin      },
  { "VARBIN",     PB_VBIN,      GDA_TypeVarbin      },

  // This should ALWAYS be the LAST entry
  { NULL,         PBI_USER_TYPE,GDA_TypeNull        }
};


gboolean
gda_primebase_connection_new (GdaServerConnection *cnc)
{
  static gboolean initialized;
  gint   i = 0;
  primebase_Connection *pcnc = NULL;

  // Clear schema ops array
  if (!initialized) {
    for (i=GDA_Connection_GDCN_SCHEMA_AGGREGATES;
         i<= GDA_Connection_GDCN_SCHEMA_LAST;
         i++) {
      schema_ops[i] = NULL;
    }
  }

  pcnc = g_new0(primebase_Connection, 1);
  g_return_val_if_fail(pcnc != NULL, FALSE);
  gda_server_connection_set_user_data(cnc, (gpointer) pcnc);

  if (!initialized) {
    // Initialize PBI API
#ifdef PRIMEBASE_DEBUG
    i = PBIInit(1);
#else
    i = PBIInit(0);
#endif

    if (i == PB_ERROR) {
      g_free((gpointer) pcnc);
      return FALSE;
    }

    // Initiate api-logging
#ifdef PRIMEBASE_DEBUG
    // FIXME: Change logname to a userdefined value
    PBISetLogName("~/gda-primebase.log");
//	 PBITrace(long session_id, 1);
#endif
	 initialized = TRUE;
  }
  
  return TRUE;
}

gint
gda_primebase_connection_open (GdaServerConnection *cnc,
                               const gchar *dsn,
                               const gchar *user,
                               const gchar *password)
{
  primebase_Connection *pcnc = NULL;
  primebase_Error      *perr = NULL;
  long                 psid = 0;
  gchar                *t_user = NULL; // DSN user override
  gchar                *t_pass = NULL; // DSN pass override
  gchar                *t_copt = NULL; // DSN connection options
  gchar                *c_user = NULL; // CONINFO user
  gchar                *c_pass = NULL; // CONINFO pass
  gchar                *c_copt = NULL; // CONINFO connection options
  short                hstmt;
  
  int                  status;
  long                 s_id;
  char                 network[255];


  g_return_val_if_fail(cnc != NULL, -1);
  gda_server_connection_set_username(cnc, user);
  gda_server_connection_set_password(cnc, password);
  gda_server_connection_set_dsn(cnc, dsn);
  pcnc = (primebase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(pcnc != NULL, -1);

  gda_primebase_update_dsn_parameters(cnc, dsn);
  t_user = (gchar *) gda_server_connection_get_username(cnc);
  t_pass = (gchar *) gda_server_connection_get_password(cnc);
  
  // FIXME: Implement PB_OPEN_SERVER Connections
  // FIXME: Perhaps PB_SHM Connections also make sense
  // Connect to the server via tcp
  hstmt = PBIConnect(&s_id, pcnc->host, PB_DATA_SERVER, PB_TCP, "PB_TCP",
                     t_user, t_pass, pcnc->db);
  pcnc->sid = s_id;
  if (hstmt == PB_ERROR) {
    gda_log_message(_("Could not connect to '%s@%s:%s' with connparm '%s'"),
                    t_user, pcnc->host, pcnc->db, pcnc->connparm);
    return -1;
  }
  gda_log_message(_("%2ld: Connected to '%s@%s:%s' with connparm: '%s'"),
                  pcnc->sid, t_user, pcnc->host, pcnc->db, pcnc->connparm);

  // Collect information about the connection we just made
  hstmt = PBIConnectionInfo(s_id, &pcnc->info);
  if (hstmt == PB_ERROR) {
    gda_log_message(_("Could not get PBIConnectionInfo(), disconnecting"));
	 PBIDisconnect(pcnc->sid);
	 return -1;
  }
  gda_log_message(_("%2ld: ConInfo: '%s@%s', Cl v%x, Srv '%s' v%x, Brand '%s'"),
                  pcnc->sid, t_user, pcnc->host, pcnc->info.client_version,
                  pcnc->info.server_name, pcnc->info.server_version,
                  pcnc->info.server_brand);
  
  return 0;
}

void
gda_primebase_connection_close (GdaServerConnection *cnc)
{
  primebase_Connection *pcnc = NULL;
  
  g_return_if_fail(cnc != NULL);
  pcnc = (primebase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_if_fail(pcnc != NULL);

  // Disconnect from the server
  PBIDisconnect(pcnc->sid);
}

gint
gda_primebase_connection_begin_transaction (GdaServerConnection *cnc)
{
  return -1;
}

gint
gda_primebase_connection_commit_transaction (GdaServerConnection *cnc)
{
  return -1;
}

gint
gda_primebase_connection_rollback_transaction (GdaServerConnection *cnc)
{
  return -1;
}

GdaServerRecordset *
gda_primebase_connection_open_schema (GdaServerConnection *cnc,
				 GdaError *error,
				 GDA_Connection_QType t,
				 GDA_Connection_Constraint *constraints,
				 gint length)
{
  return NULL;
}

glong
gda_primebase_connection_modify_schema (GdaServerConnection *cnc,
                                        GDA_Connection_QType t,
                                        GDA_Connection_Constraint *constraints,
                                        gint length)
{
  return -1;
}

gint
gda_primebase_connection_start_logging (GdaServerConnection *cnc,
				   const gchar *filename)
{
  primebase_Connection *pcnc = NULL;
  
  g_return_val_if_fail(cnc != NULL, -1);
  pcnc = (primebase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(pcnc != NULL, -1);
  
//  g_return_val_if_fail(filename != NULL, -1);

  // Note that a NULL value wouldl NOT initiate tracing
  
  PBISetLogName(filename);
  PBITrace(pcnc->sid, 1);

  return 1;
}

gint
gda_primebase_connection_stop_logging (GdaServerConnection *cnc)
{
  primebase_Connection *pcnc = NULL;

  g_return_val_if_fail(cnc != NULL, -1);
  pcnc = (primebase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(pcnc != NULL, -1);

  PBITrace(pcnc->sid, 0);
  
  return 1;
}

gchar *
gda_primebase_connection_create_table (GdaServerConnection *cnc,
				       GDA_RowAttributes *columns)
{
}

gboolean
gda_primebase_connection_supports (GdaServerConnection *cnc,
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

GDA_ValueType
gda_primebase_connection_get_gda_type (GdaServerConnection *cnc, gulong sql_type)
{
  gint i = 0;
        
  while ((i < GDA_PRIMEBASE_TYPE_CNT)) {
    if (gda_primebase_type_list[i].sql_type == sql_type) {
      return gda_primebase_type_list[i].gda_type;
    }
    i++;
  }
        
  return gda_primebase_type_list[GDA_PRIMEBASE_TYPE_CNT - 1].gda_type;
}

gshort
gda_primebase_connection_get_c_type (GdaServerConnection *cnc, GDA_ValueType type)
{
  return -1;
}

gchar *
gda_primebase_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql)
{
  return NULL;
}

gchar *
gda_primebase_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml)
{
  return NULL;
}

void
gda_primebase_connection_free (GdaServerConnection *cnc)
{
}

void
gda_primebase_error_make (GdaError *error,
                          GdaServerRecordset *recset,
                          GdaServerConnection *cnc,
                          gchar *where)
{
  primebase_Connection *pcnc  = NULL;
  primebase_Recordset  *prset = NULL;
  long perr, serr;
  gchar msg[256];
  gchar *errtxt;

  pcnc = (primebase_Connection *) gda_server_connection_get_user_data(cnc);

  if (pcnc) {
//    prset = gda_server_recordset_get_user_data(recset);
    // Did we get any error data?

    PBIGetError(pcnc->sid, &perr, &serr, msg, 256);
    errtxt = g_strdup_printf("%2ld: ERROR: %ld (%ld) : %s.",
                             pcnc->sid, perr, serr, msg);
    if (errtxt) {
      gda_error_set_description(error, errtxt);
      g_free((gpointer) errtxt);
    } else {
      gda_error_set_description(error, msg);
    }
    gda_error_set_number(error, perr);
    gda_error_set_source(error, "[gda-primebase]");
    gda_error_set_help_url(error, _("Not available"));
    gda_error_set_help_context(error, _("Not available"));
    gda_error_set_sqlstate(error, _("error"));
    gda_error_set_native(error, gda_error_get_description(error));
  }
}

primebase_Error *
gda_primebase_get_error(long sid, gboolean log)
{
  primebase_Error *perr = g_new0(primebase_Error, 1);
  
  if (perr == NULL) {
    gda_log_message(_("Not enough memory for primebase_Error structure."));
	 return NULL;
  }
 
  PBIGetError(sid, &perr->perr, &perr->serr, perr->msg, 256);
  perr->snum = sid;
  if (log) {
    gda_log_message(_("%2ld: ERROR: %ld (%ld) : %s.\n"),
                    perr->snum, perr->perr, perr->serr, perr->msg);
  }
  return perr;
}

void gda_primebase_free_error(primebase_Error *perr)
{
  if (perr) {
    g_free((gpointer) perr);
	 perr = NULL;
  }
}

void
gda_primebase_update_dsn_parameters(GdaServerConnection *cnc,
                                    const gchar *dsn)
{
  primebase_Connection *pcnc = NULL;
  gchar *user                = NULL;
  gchar *password            = NULL;
  gchar *ptr_s = NULL, *ptr_e = NULL;
  
  g_return_if_fail(cnc != NULL);
  pcnc = (primebase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_if_fail(pcnc != NULL);
  gda_server_connection_set_dsn(cnc, dsn);
  
  ptr_s = (gchar *) dsn;
  while (ptr_s && *ptr_s) {
    ptr_e = strchr(ptr_s, ';');
          
    if (ptr_e)
      *ptr_e = '\0';
    if (strncasecmp(ptr_s, "HOST", strlen("HOST")) == 0) {
      if (pcnc->host) g_free((gpointer) pcnc->host);
      pcnc->host = get_value(ptr_s);
    } else if (strncasecmp(ptr_s, "DATABASE", strlen("DATABASE")) == 0) {
      if (pcnc->db) g_free((gpointer) pcnc->db);
      pcnc->db = get_value(ptr_s);
	 } else if (strncasecmp(ptr_s, "USERNAME", strlen("USERNAME")) == 0) {
      user = get_value(ptr_s);
	 } else if (strncasecmp(ptr_s, "PASSWORD", strlen("PASSWORD")) == 0) {
      password = get_value(ptr_s);
    }

    ptr_s = ptr_e;
    if (ptr_s)
      ptr_s++;
  }

  if (user) {
    gda_server_connection_set_username(cnc, user);
  }
  if (password) {
    gda_server_connection_set_password(cnc, password);
  }
}

gchar*
get_value (gchar* ptr) {
  while (*ptr && *ptr != '=')
    ptr++;
  
  if (!*ptr)
    return NULL;

  ptr++;
  if (!*ptr)
    return NULL;

  while (*ptr && isspace(*ptr))
    ptr++;

  return (g_strdup(ptr));
}
