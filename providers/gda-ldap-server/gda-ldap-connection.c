/* GNOME DB LDAP Provider
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

#include "gda-ldap.h"
#include <ctype.h>

gboolean
gda_ldap_connection_new (GdaServerConnection *cnc)
{
  LDAP_Connection* ld_cnc;

  g_return_val_if_fail(cnc != NULL, FALSE);

  ld_cnc = g_new0(LDAP_Connection, 1);
  gda_server_connection_set_user_data(cnc, (gpointer) ld_cnc);
  return TRUE;
}

static gchar*
get_value (gchar* ptr)
{
  while (*ptr && *ptr != '=') ptr++;
  if (!*ptr)
    return 0;
  ptr++;
  if (!*ptr)
    return 0;
  while (*ptr && isspace(*ptr))
    ptr++;
  return (g_strdup(ptr));
}

gint
gda_ldap_connection_open (GdaServerConnection *cnc,
			  const gchar *dsn,
			  const gchar *user,
			  const gchar *password)
{
  LDAP_Connection*     ld_cnc;
  GdaServerError* error;
  gchar*               ptr_s;
  gchar*               ptr_e;

  g_return_val_if_fail(cnc != NULL, -1);

  ld_cnc = (LDAP_Connection *) gda_server_connection_get_user_data(cnc);
  if (ld_cnc)
    {
      gchar* host = NULL;
      gchar* port = NULL;
      gchar* auth = NULL;
      gint   auth_method = LDAP_AUTH_SIMPLE;

      /* parse DSN string */
      ptr_s = (gchar *) dsn;
      while (ptr_s && *ptr_s)
	{
	  ptr_e = strchr(ptr_s, ';');
	  if (ptr_e) *ptr_e = '\0';
	  if (strncasecmp(ptr_s, "HOST", strlen("HOST")) == 0)
	    host = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "PORT", strlen("PORT")) == 0)
	    port = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "AUTHENTICATION", strlen("AUTHENTICATION")) == 0)
	    {
	      auth = get_value(ptr_s);
	      if (!strncasecmp(auth, "KERBEROS41", strlen("KERBEROS41")))
		auth_method = LDAP_AUTH_KRBV41;
	      else if (!strncasecmp(auth, "KERBEROS42", strlen("KERBEROS42")))
		auth_method = LDAP_AUTH_KRBV42;
	      else if (!strncasecmp(auth, "SIMPLE", strlen("SIMPLE")))
		auth_method = LDAP_AUTH_SIMPLE;
	      else auth_method = LDAP_AUTH_NONE;
	      g_free((gpointer) auth);
	    }
	  ptr_s = ptr_e;
	  if (ptr_s)
	    ptr_s++;
	}

      /* open connection to the LDAP server */
      ld_cnc->ldap = ldap_init(host, port ? atoi(port) : LDAP_PORT);
      g_free((gpointer) host);
      g_free((gpointer) port);
      if (ld_cnc->ldap)
	{
	  /* bind to the LDAP server */
	  if (ldap_bind_s(ld_cnc->ldap, user, password, auth_method) == LDAP_SUCCESS)
	    return 0;
	  
	  /* unbind in case of error */
	  ldap_unbind(ld_cnc->ldap);
	}
      
      /* return error to client */
      error = gda_server_error_new();
      gda_server_error_make(error, 0, cnc, __PRETTY_FUNCTION__);
    }
  return -1;
}

void
gda_ldap_connection_close (GdaServerConnection *cnc)
{
  LDAP_Connection* ld_cnc;

  g_return_if_fail(cnc != NULL);

  ld_cnc = (LDAP_Connection *) gda_server_connection_get_user_data(cnc);
  if (ld_cnc)
    {
      ldap_unbind(ld_cnc->ldap);
      ld_cnc->ldap = NULL;
    }
}

gint
gda_ldap_connection_begin_transaction (GdaServerConnection *cnc)
{
  LDAP_Connection* ld_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  ld_cnc = (LDAP_Connection *) gda_server_connection_get_user_data(cnc);
  if (ld_cnc)
    {
    }
  return -1;
}

gint
gda_ldap_connection_commit_transaction (GdaServerConnection *cnc)
{
  LDAP_Connection* ld_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  ld_cnc = (LDAP_Connection *) gda_server_connection_get_user_data(cnc);
  if (ld_cnc)
    {
    }
  return -1;
}

gint
gda_ldap_connection_rollback_transaction (GdaServerConnection *cnc)
{
  LDAP_Connection* ld_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  ld_cnc = (LDAP_Connection *) gda_server_connection_get_user_data(cnc);
  if (ld_cnc)
    {
    }
  return -1;
}

GdaServerRecordset *
gda_ldap_connection_open_schema (GdaServerConnection *cnc,
				 GdaServerError *error,
				 GDA_Connection_QType t,
				 GDA_Connection_Constraint *constraints,
				 gint length)
{
  return NULL;
}

glong
gda_ldap_connection_modify_schema (GdaServerConnection *cnc,
                                   GDA_Connection_QType t,
                                   GDA_Connection_Constraint *constraints,
                                   gint length)
{
  return -1;
}

gint
gda_ldap_connection_start_logging (GdaServerConnection *cnc,
				   const gchar *filename)
{
  return -1;
}

gint
gda_ldap_connection_stop_logging (GdaServerConnection *cnc)
{
  return -1;
}

gchar *
gda_ldap_connection_create_table (GdaServerConnection *cnc,
				       GDA_RowAttributes *columns)
{
  return NULL;
}

gboolean
gda_ldap_connection_supports (GdaServerConnection *cnc,
				   GDA_Connection_Feature feature)
{
  g_return_val_if_fail(cnc != NULL, FALSE);

  if (feature == GDA_Connection_FEATURE_TRANSACTIONS) return (FALSE);
  return FALSE; /* not supported or know nothing about it */
}

GDA_ValueType
gda_ldap_connection_get_gda_type (GdaServerConnection *cnc, gulong sql_type)
{
  g_return_val_if_fail(cnc != NULL, GDA_TypeNull);

  switch (sql_type)
    {
    }
  return GDA_TypeNull;
}

gshort
gda_ldap_connection_get_c_type (GdaServerConnection *cnc, GDA_ValueType type)
{
  g_return_val_if_fail(cnc != NULL, -1);

  //switch (type)
  //  {
  //  }
  return -1;
}

gchar *
gda_ldap_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql)
{
  return NULL;
}

gchar *
gda_ldap_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml)
{
  return NULL;
}

void
gda_ldap_connection_free (GdaServerConnection *cnc)
{
  LDAP_Connection* ld_cnc;

  g_return_if_fail(cnc != NULL);

  ld_cnc = (LDAP_Connection *) gda_server_connection_get_user_data(cnc);
  if (ld_cnc)
    {
      g_free((gpointer) ld_cnc);
    }
}

void
gda_ldap_error_make (GdaServerError *error,
		     GdaServerRecordset *recset,
		     GdaServerConnection *cnc,
		     gchar *where)
{
  gchar*           err_msg;
  LDAP_Connection* ld_cnc;

  ld_cnc = (LDAP_Connection *) gda_server_connection_get_user_data(cnc);
  if (ld_cnc)
    {
      err_msg = ldap_err2string(ld_cnc->ldap->ld_errno);
      gda_log_error(_("error '%s' at %s"), err_msg, where);

      gda_server_error_set_description(error, err_msg);
      gda_server_error_set_number(error, ld_cnc->ldap->ld_errno);
      gda_server_error_set_source(error, "[gda-ldap]");
      gda_server_error_set_help_file(error, _("Not available"));
      gda_server_error_set_help_context(error, _("Not available"));
      gda_server_error_set_sqlstate(error, _("error"));
      gda_server_error_set_native(error, err_msg);
    }
}


