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

typedef Gda_ServerRecordset* (*schema_ops_fn)(Gda_ServerError *,
                                              Gda_ServerConnection *,
                                              GDA_Connection_Constraint *,
                                              gint);

schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] = {
  NULL,
};

static gchar*
  get_value (gchar* ptr);

static void
  gda_primebase_update_dsn_parameters(Gda_ServerConnection *cnc,
                                      const gchar *dsn);

static const primebase_Types gda_primebase_type_list[GDA_PRIMEBASE_TYPE_CNT] = {
  { "TINYINT",    A_TINYINT,   GDA_TypeSmallint }, //  8-bit
  { "SMALLINT",   A_SMINT,     GDA_TypeSmallint }, // 16-bit
  { "SMINT",      A_SMINT,     GDA_TypeSmallint }, // 16-bit
  { "INTEGER",    A_INTEGER,   GDA_TypeInteger  }, // 32-bit
  { "INT",        A_INTEGER,   GDA_TypeInteger  }, // 32-bit   ( 5)
  
  { "DECIMAL",    A_DECIMAL,   GDA_TypeDecimal     },
  { "MONEY",      A_MONEY,     GDA_TypeCurrency    },

  { "SMALLFLOAT", A_SMFLOAT,   GDA_TypeSingle      }, //  4-byte
  { "SMFLOAT",    A_SMFLOAT,   GDA_TypeSingle      }, //  4-byte
  { "REAL",       A_SMFLOAT,   GDA_TypeSingle      }, //  4-byte  (10)
  { "FLOAT",      A_FLOAT,     GDA_TypeDouble      }, //  8-byte
  /* Just on 68k macs */
  { "REAL10",     A_FLOAT10,   GDA_TypeDouble      }, // 10-byte
  { "REAL12",     A_FLOAT12,   GDA_TypeDouble      }, // 12-byte

  /* What's on $maybe? */
  { "BOOLEAN",    A_BOOLEAN,   GDA_TypeBoolean     },
  
  { "DATE",       A_DATE,      GDA_TypeDbDate      }, //          (15)
  { "TIME",       A_TIME,      GDA_TypeDbTime      },
  { "TIMESTAMP",  A_TIMESTAMP, GDA_TypeDbTimestamp },
  { "DATETIME",   A_TIMESTAMP, GDA_TypeDbTimestamp },

  { "CHAR",       A_CHAR,      GDA_TypeChar        },
  { "VARCHAR",    A_VARCHAR,   GDA_TypeVarchar     }, //          (20)
  
  { "BINARY",     A_BIN,       GDA_TypeVarbin      },
  { "BIN",        A_BIN,       GDA_TypeVarbin      },
  { "VARBINARY",  A_VARBIN,    GDA_TypeVarbin      },
  { "VARBIN",     A_VARBIN,    GDA_TypeVarbin      },

  { "GENERIC",    A_GENERIC,   GDA_TypeVarchar     },
  { "OBJNAME",    A_OBJNAME,   GDA_TypeVarchar     },
  
  // This should ALWAYS be the LAST entry
  { NULL,         A_GENERIC,   GDA_TypeNull        }
};


gboolean
gda_primebase_connection_new (Gda_ServerConnection *cnc)
{
  static gboolean initialized;
  gint   i = 0;
  primebase_Connection *pcnc = NULL;
  
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
  
  return TRUE;
}

gint
gda_primebase_connection_open (Gda_ServerConnection *cnc,
                               const gchar *dsn,
                               const gchar *user,
                               const gchar *password)
{
  primebase_Connection *pcnc = NULL;
  primebase_Error      *perr = NULL;
  long                  psid = 0;
  gchar                *t_user = NULL; // DSN user override
  gchar                *t_pass = NULL; // DSN pass override
  gchar                *t_copt = NULL; // DSN connection options
  gchar                *c_user = NULL; // CONINFO user
  gchar                *c_pass = NULL; // CONINFO pass
  gchar                *c_copt = NULL; // CONINFO connection options
  int                  status;
  long                 s_num, s_id, vid, start, state;
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
  
  if (CLInit(&pcnc->sid, pcnc->host,
             t_user, t_pass, pcnc->connparm
            ) == A_OK
     ) {
    pcnc->snum = CLGetSn(pcnc->sid);
    gda_log_message(_("%2ld: Connected to '%s@%s' with connparm: '%s'"),
                    pcnc->sid, t_user, pcnc->host, pcnc->connparm);
	 status = CLConInfo(0, pcnc->snum, &s_id, &vid,
                       pcnc->host, c_user, network, pcnc->connparm,
                       &start, &state);
    gda_log_message(_("%2ld: ConInfo: %s@%s, Client API v%x, status: %x"),
                    pcnc->sid, t_user, pcnc->host, vid, status);
  } else {
    gda_log_error(_("Could not connect to '%s'"), pcnc->host);
	 perr = gda_primebase_get_error(pcnc->sid, TRUE);
	 gda_primebase_free_error(perr);
	 CLEnd(pcnc->sid);
	 return -1;
  }
  return 0;
}

void
gda_primebase_connection_close (Gda_ServerConnection *cnc)
{
  primebase_Connection *pcnc = NULL;
  
  g_return_if_fail(cnc != NULL);
  pcnc = (primebase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_if_fail(pcnc != NULL);
  
  CLEnd(pcnc->sid);
}

gint
gda_primebase_connection_begin_transaction (Gda_ServerConnection *cnc)
{
  return -1;
}

gint
gda_primebase_connection_commit_transaction (Gda_ServerConnection *cnc)
{
  return -1;
}

gint
gda_primebase_connection_rollback_transaction (Gda_ServerConnection *cnc)
{
  return -1;
}

Gda_ServerRecordset *
gda_primebase_connection_open_schema (Gda_ServerConnection *cnc,
				 Gda_ServerError *error,
				 GDA_Connection_QType t,
				 GDA_Connection_Constraint *constraints,
				 gint length)
{
  return NULL;
}

glong
gda_primebase_connection_modify_schema (Gda_ServerConnection *cnc,
                                        GDA_Connection_QType t,
                                        GDA_Connection_Constraint *constraints,
                                        gint length)
{
  return -1;
}

gint
gda_primebase_connection_start_logging (Gda_ServerConnection *cnc,
				   const gchar *filename)
{
  return -1;
}

gint
gda_primebase_connection_stop_logging (Gda_ServerConnection *cnc)
{
  return -1;
}

gchar *
gda_primebase_connection_create_table (Gda_ServerConnection *cnc,
				       GDA_RowAttributes *columns)
{
}

gboolean
gda_primebase_connection_supports (Gda_ServerConnection *cnc,
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
gda_primebase_connection_get_gda_type (Gda_ServerConnection *cnc, gulong sql_type)
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
gda_primebase_connection_get_c_type (Gda_ServerConnection *cnc, GDA_ValueType type)
{
  return -1;
}

void
gda_primebase_connection_free (Gda_ServerConnection *cnc)
{
}

void
gda_primebase_error_make (Gda_ServerError *error,
                          Gda_ServerRecordset *recset,
                          Gda_ServerConnection *cnc,
                          gchar *where)
{
  primebase_Connection *pcnc  = NULL;
  primebase_Recordset  *prset = NULL;
  long perr, serr, snum;
  gchar msg[256];
  gchar *errtxt;

  pcnc = (primebase_Connection *) gda_server_connection_get_user_data(cnc);

  if (pcnc) {
//    prset = gda_server_recordset_get_user_data(recset);
    if (CLGetErr(pcnc->sid, &perr, &serr, NULL, NULL, msg) == A_OK)	{
      snum = CLGetSn(pcnc->sid);
      errtxt = g_strdup_printf("%2ld: ERROR: %ld (%ld) : %s.",
                               snum, perr, serr, msg);
		if (errtxt) {
        gda_server_error_set_description(error, errtxt);
		  g_free((gpointer) errtxt);
		} else {
        gda_server_error_set_description(error, msg);
      }
		gda_server_error_set_number(error, perr);
      gda_server_error_set_source(error, "[gda-primebase]");
      gda_server_error_set_help_file(error, _("Not available"));
      gda_server_error_set_help_context(error, _("Not available"));
      gda_server_error_set_sqlstate(error, _("error"));
      gda_server_error_set_native(error, gda_server_error_get_description(error));
    }
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
  if (CLGetErr(sid, &perr->perr, &perr->serr, NULL, NULL, perr->msg
              ) == A_OK
     ) {
    perr->snum = CLGetSn(sid);
	 if (log) {
      gda_log_message(_("%2ld: ERROR: %ld (%ld) : %s.\n"),
                      perr->snum, perr->perr, perr->serr, perr->msg);
    }
	 return perr;
  }
  gda_primebase_free_error(perr);
  return NULL;
}

void gda_primebase_free_error(primebase_Error *perr)
{
  if (perr) {
    g_free((gpointer) perr);
	 perr = NULL;
  }
}

void
gda_primebase_update_dsn_parameters(Gda_ServerConnection *cnc,
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
