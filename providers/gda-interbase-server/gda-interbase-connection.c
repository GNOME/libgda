/* GNOME DB Interbase Provider
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

#include "gda-interbase.h"

gboolean
gda_interbase_connection_new (GdaServerConnection *cnc)
{
  INTERBASE_Connection* ib_cnc;

  g_return_val_if_fail(cnc != NULL, FALSE);

  /* FIXME: here it core dumps in the second connection being created */
  ib_cnc = g_new0(INTERBASE_Connection, 1);
  ib_cnc->db = NULL;
  ib_cnc->trans = NULL;
  gda_server_connection_set_user_data(cnc, (gpointer) ib_cnc);
  return TRUE;
}

gint
gda_interbase_connection_open (GdaServerConnection *cnc,
                               const gchar *dsn,
                               const gchar *user,
                               const gchar *password)
{
  INTERBASE_Connection* ib_cnc;
  GdaError*  error;

  g_return_val_if_fail(cnc != NULL, -1);

  ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(cnc);
  if (ib_cnc)
    {
      if (!isc_attach_database(ib_cnc->status, 0, dsn, &ib_cnc->db, 0, NULL))
        {
          return 0;
        }
      
      /* return error to client */
      error = gda_error_new();
      gda_server_error_make(error, 0, cnc, __PRETTY_FUNCTION__);
    }
  return -1;
}

void
gda_interbase_connection_close (GdaServerConnection *cnc)
{
  INTERBASE_Connection* ib_cnc;

  g_return_if_fail(cnc != NULL);

  ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(cnc);
  if (ib_cnc)
    {
      isc_detach_database(ib_cnc->status, &ib_cnc->db);
      g_free((gpointer) ib_cnc);
    }
}

gint
gda_interbase_connection_begin_transaction (GdaServerConnection *cnc)
{
  INTERBASE_Connection* ib_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(cnc);
  if (ib_cnc)
    {
      if (!ib_cnc->trans)
	{
	  if (isc_start_transaction(ib_cnc->status, &ib_cnc->trans, 1, &ib_cnc->db, 0, NULL))
	    {
	      gda_server_error_make(gda_error_new(), 0, cnc, __PRETTY_FUNCTION__);
	      return -1;
	    }
	}
      return 0;
    }
  return -1;
}

gint
gda_interbase_connection_commit_transaction (GdaServerConnection *cnc)
{
  INTERBASE_Connection* ib_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(cnc);
  if (ib_cnc)
    {
      if (ib_cnc->trans)
	{
	  if (isc_commit_transaction(ib_cnc->status, &ib_cnc->trans) == 0) return 0;
	  else gda_server_error_make(gda_error_new(), 0, cnc, __PRETTY_FUNCTION__);
	}
      else return 0;
    }
  return -1;
}

gint
gda_interbase_connection_rollback_transaction (GdaServerConnection *cnc)
{
  INTERBASE_Connection* ib_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(cnc);
  if (ib_cnc)
    {
      if (ib_cnc->trans)
	{
	  if (isc_rollback_transaction(ib_cnc->status, &ib_cnc->trans) == 0) return 0;
	  else gda_server_error_make(gda_error_new(), 0, cnc, __PRETTY_FUNCTION__);
	}
      else return 0;
    }
  return -1;
}

GdaServerRecordset *
gda_interbase_connection_open_schema (GdaServerConnection *cnc,
                                      GdaError *error,
                                      GDA_Connection_QType t,
                                      GDA_Connection_Constraint *constraints,
                                      gint length)
{
  return NULL;
}

glong
gda_interbase_connection_modify_schema (GdaServerConnection *cnc,
                                        GDA_Connection_QType t,
                                        GDA_Connection_Constraint *constraints,
                                        gint length)
{
  return -1;
}

gint
gda_interbase_connection_start_logging (GdaServerConnection *cnc,
                                        const gchar *filename)
{
  return -1;
}

gint
gda_interbase_connection_stop_logging (GdaServerConnection *cnc)
{
  return -1;
}

gchar *
gda_interbase_connection_create_table (GdaServerConnection *cnc,
                                       GDA_RowAttributes *columns)
{
  return NULL;
}

gboolean
gda_interbase_connection_supports (GdaServerConnection *cnc,
                                   GDA_Connection_Feature feature)
{
  g_return_val_if_fail(cnc != NULL, FALSE);

  if (feature == GDA_Connection_FEATURE_TRANSACTIONS)
    return (TRUE);
  return FALSE; /* not supported or know nothing about it */
}

GDA_ValueType
gda_interbase_connection_get_gda_type (GdaServerConnection *cnc, gulong sql_type)
{
  g_return_val_if_fail(cnc != NULL, GDA_TypeNull);

  switch (sql_type)
    {
    case SQL_TEXT :
    case SQL_VARYING :
      return GDA_TypeLongvarchar;
    case SQL_SHORT :
      return GDA_TypeSmallint;
    case SQL_LONG :
      return GDA_TypeInteger;
    case SQL_INT64 :
      return GDA_TypeBigint;
    case SQL_FLOAT :
      return GDA_TypeSingle;
    case SQL_DOUBLE :
      return GDA_TypeDouble;
    case SQL_TIMESTAMP :
      return GDA_TypeDbTimestamp;
    case SQL_TYPE_DATE :
      return GDA_TypeDbDate;
    case SQL_TYPE_TIME :
      return GDA_TypeDbTime;
    case SQL_BLOB :
    case SQL_ARRAY :
      return GDA_TypeBinary;
    }
  return GDA_TypeNull;
}

gshort
gda_interbase_connection_get_c_type (GdaServerConnection *cnc, GDA_ValueType type)
{
  g_return_val_if_fail(cnc != NULL, -1);

  //switch (type)
  //  {
  //  }
  return -1;
}

gchar *
gda_interbase_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql)
{
  return NULL;
}

gchar *
gda_interbase_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml)
{
  return NULL;
}

void
gda_interbase_connection_free (GdaServerConnection *cnc)
{
  INTERBASE_Connection* ib_cnc;

  g_return_if_fail(cnc != NULL);

  ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(cnc);
  if (ib_cnc)
    {
      g_free((gpointer) ib_cnc);
    }
}

void
gda_interbase_error_make (GdaError *error,
                          GdaServerRecordset *recset,
                          GdaServerConnection *cnc,
                          gchar *where)
{
  gchar*                err_msg; /* FIXME: retireve error message from server */
  INTERBASE_Connection* ib_cnc;

  ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(cnc);
  if (ib_cnc)
    {
      err_msg = g_strdup_printf(_("Error code %ld"), isc_sqlcode(ib_cnc->status));
      gda_log_error(_("error '%s' at %s"), err_msg, where);

      gda_error_set_description(error, err_msg);
      gda_error_set_number(error, isc_sqlcode(ib_cnc->status));
      gda_error_set_source(error, "[gda-interbase]");
      gda_error_set_help_url(error, "http://www.interbase.org/");
      gda_error_set_help_context(error, _("Not available"));
      gda_error_set_sqlstate(error, _("error"));
      gda_error_set_native(error, err_msg);

      g_free((gpointer) err_msg);
      isc_print_sqlerror(isc_sqlcode(ib_cnc), ib_cnc->status);
    }
}


