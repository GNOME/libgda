/* GNOME DB MYSQL Provider
 * Copyright (C) 1998 Michael Lausch
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

#include "gda-mysql.h"
#include <ctype.h>

typedef Gda_ServerRecordset* (*schema_ops_fn)(Gda_ServerError *,
					      Gda_ServerConnection *,
					      GDA_Connection_Constraint *,
					      gint );

static Gda_ServerRecordset* schema_tables (Gda_ServerError *error,
					   Gda_ServerConnection *cnc,
					   GDA_Connection_Constraint *constraints,
					   gint length);
static Gda_ServerRecordset* schema_columns (Gda_ServerError *error,
					    Gda_ServerConnection *cnc,
					    GDA_Connection_Constraint *constraints,
					    gint length);

schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] =
{
  0,
};

/*
 * Public functions
 */
gboolean
gda_mysql_connection_new (Gda_ServerConnection *cnc)
{
  static gboolean   initialized = FALSE;
  MYSQL_Connection* mysql_cnc;

  /* initialize schema functions */
  if (!initialized)
    {
      schema_ops[GDA_Connection_GDCN_SCHEMA_TABLES] = schema_tables;
      schema_ops[GDA_Connection_GDCN_SCHEMA_COLS] = schema_columns;
    }

  mysql_cnc = g_new0(MYSQL_Connection, 1);
  gda_server_connection_set_user_data(cnc, (gpointer) mysql_cnc);

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
gda_mysql_connection_open (Gda_ServerConnection *cnc,
			   const gchar *dsn,
			   const gchar *user,
			   const gchar *password)
{
  MYSQL_Connection* mysql_cnc;
  gchar*            ptr_s;
  gchar*            ptr_e;
  MYSQL*            rc;
  gchar*            t_host = NULL;
  gchar*            t_db = NULL;
  gchar*            t_user = NULL;
  gchar*            t_password = NULL;
  gchar*            t_port = NULL;
  gchar*            t_unix_socket = NULL;
  gchar*            t_flags = NULL;
#if MYSQL_VERSION_ID < 32200
  gint              err;
#endif

  g_return_val_if_fail(cnc != NULL, -1);

  mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
  if (mysql_cnc)
    {
      /* parse connection string */
      ptr_s = (gchar *) dsn;
      while (ptr_s && *ptr_s)
	{
	  ptr_e = strchr(ptr_s, ';');
	  
	  if (ptr_e)
	    *ptr_e = '\0';
	  if (strncasecmp(ptr_s, "HOST", strlen("HOST")) == 0)
	    t_host = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "DATABASE", strlen("DATABASE")) == 0)
	    t_db = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "USERNAME", strlen("USERNAME")) == 0)
	    t_user = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "PASSWORD", strlen("PASSWORD")) == 0)
	    t_password = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "PORT", strlen("PORT")) == 0)
	    t_port = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "UNIX_SOCKET", strlen("UNIX_SOCKET")) == 0)
	    t_unix_socket = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "FLAGS", strlen("FLAGS")) == 0)
	    t_flags = get_value(ptr_s);
	  
	  ptr_s = ptr_e;
	  if (ptr_s)
	    ptr_s++;
	}
      
      /* okay, now let's copy the overriding user and/or password if applicable */
      if (user)
	{
	  if (t_user) g_free((gpointer) t_user);
	  t_user = g_strdup(user);
	}
      if (password)
	{
	  if (t_password) g_free((gpointer) t_password);
	  t_password = g_strdup(password);
	}

      /* we can't have both a host/pair AND a unix_socket */
      /* if both are provided, error out now... */
      if ((t_host || t_port) && t_unix_socket)
	{
	  gda_log_error(_("%s:%d: You cannot provide a UNIX_SOCKET if you also provide"
			  " either a HOST or a PORT. Please remove the UNIX_SOCKET, or"
			  " remove both the HOST and PORT options"),
			__FILE__, __LINE__);
	  return -1;
	}

      /* set the default of localhost:3306 if neither is provided */
      if (!t_unix_socket)
	{
	  if (!t_port) t_port = g_strdup("3306");
	  if (!t_host) t_host = g_strdup("localhost");
	}

      gda_log_message(_("Opening connection with user=%s, password=%s, "
			"host=%s, port=%s, unix_socket=%s"), t_user, t_password,
		      t_host, t_port, t_unix_socket);
      mysql_cnc->mysql = g_new0(MYSQL, 1);
      
      rc = mysql_real_connect(mysql_cnc->mysql,
			      t_host,
			      t_user,
			      t_password,
#if MYSQL_VERSION_ID >= 32200
			      t_db,
#endif
			      t_port ? atoi(t_port) : 0,
			      t_unix_socket,
			      t_flags ? atoi(t_flags) : 0);
      if (!rc)
	{
	  gda_server_error_make(gda_server_error_new(), 0, cnc, __PRETTY_FUNCTION__);
	  return -1;
	}
#if MYSQL_VERSION_ID < 32200
      err = mysql_select_db(mysql_cnc->mysql, t_db);
      if (err != 0)
	{
	  gda_server_error_make(gda_server_error_new(), 0, cnc, __PRETTY_FUNCTION__);
	  return -1;
	}
#endif

      /* free memory */
      g_free((gpointer) t_host);
      g_free((gpointer) t_db);
      g_free((gpointer) t_user);
      g_free((gpointer) t_password);
      g_free((gpointer) t_port);
      g_free((gpointer) t_unix_socket);
      g_free((gpointer) t_flags);

      return 0;
    }
  return -1;
}

void
gda_mysql_connection_close (Gda_ServerConnection *cnc)
{
  MYSQL_Connection* mysql_cnc;

  g_return_if_fail(cnc != NULL);

  mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
  if (mysql_cnc)
    {
      mysql_close(mysql_cnc->mysql);
    }
}

gint
gda_mysql_connection_begin_transaction (Gda_ServerConnection *cnc)
{
  return -1;
}

gint
gda_mysql_connection_commit_transaction (Gda_ServerConnection *cnc)
{
  return -1;
}

gint
gda_mysql_connection_rollback_transaction (Gda_ServerConnection *cnc)
{
  return -1;
}

Gda_ServerRecordset *
gda_mysql_connection_open_schema (Gda_ServerConnection *cnc,
				 Gda_ServerError *error,
				 GDA_Connection_QType t,
				 GDA_Connection_Constraint *constraints,
				 gint length)
{
  schema_ops_fn fn;

  g_return_val_if_fail(cnc != NULL, NULL);

  fn = schema_ops[(gint) t];
  if (fn)
    return fn(error, cnc, constraints, length);
  else gda_log_error(_("Unhandled SCHEMA_QTYPE %d"), (gint) t);
  return NULL;
}

glong
gda_mysql_connection_modify_schema (Gda_ServerConnection *cnc,
                                   GDA_Connection_QType t,
                                   GDA_Connection_Constraint *constraints,
                                   gint length)
{
  return -1;
}

gint
gda_mysql_connection_start_logging (Gda_ServerConnection *cnc,
				   const gchar *filename)
{
  return -1;
}

gint
gda_mysql_connection_stop_logging (Gda_ServerConnection *cnc)
{
  return -1;
}

gchar *
gda_mysql_connection_create_table (Gda_ServerConnection *cnc,
				       GDA_RowAttributes *columns)
{
  return NULL;
}

gboolean
gda_mysql_connection_supports (Gda_ServerConnection *cnc,
				   GDA_Connection_Feature feature)
{
  g_return_val_if_fail(cnc != NULL, FALSE);

  if (feature == GDA_Connection_FEATURE_TRANSACTIONS) return (FALSE);
  return FALSE; /* not supported or know nothing about it */
}

GDA_ValueType
gda_mysql_connection_get_gda_type (Gda_ServerConnection *cnc, gulong sql_type)
{
  g_return_val_if_fail(cnc != NULL, GDA_TypeNull);

  switch (sql_type)
    {
    case FIELD_TYPE_DECIMAL:
      return GDA_TypeDecimal;
    case FIELD_TYPE_TINY:
      return GDA_TypeTinyint;
    case FIELD_TYPE_SHORT:
      return GDA_TypeSmallint;
    case FIELD_TYPE_LONG:
      return GDA_TypeBigint;
    case FIELD_TYPE_FLOAT:
      return GDA_TypeSingle;
    case FIELD_TYPE_DOUBLE:
      return GDA_TypeDouble;
    case FIELD_TYPE_NULL:
      return GDA_TypeNull;
    case FIELD_TYPE_NEWDATE:
    case FIELD_TYPE_TIMESTAMP:
      return GDA_TypeDbTimestamp;
    case FIELD_TYPE_LONGLONG:
      return GDA_TypeUBigint;
    case FIELD_TYPE_INT24:
      return GDA_TypeBigint;
    case FIELD_TYPE_DATE:
      return GDA_TypeDbDate;
    case FIELD_TYPE_TIME:
      return GDA_TypeDbTime;
    case FIELD_TYPE_DATETIME:
      return GDA_TypeDbTimestamp;
    case FIELD_TYPE_YEAR:
      return GDA_TypeInteger;
    case FIELD_TYPE_ENUM:
      return GDA_TypeVarbin;
    case FIELD_TYPE_SET:
      return GDA_TypeVarbin;
    case FIELD_TYPE_TINY_BLOB:
    case FIELD_TYPE_MEDIUM_BLOB:
      return GDA_TypeVarbin;
    case FIELD_TYPE_LONG_BLOB:
    case FIELD_TYPE_BLOB:
      return GDA_TypeLongvarbin;
    case FIELD_TYPE_VAR_STRING:
    case FIELD_TYPE_STRING:
      return GDA_TypeVarchar;
    }
  return GDA_TypeVarchar;
}

gshort
gda_mysql_connection_get_c_type (Gda_ServerConnection *cnc, GDA_ValueType type)
{
  g_return_val_if_fail(cnc != NULL, -1);

  switch (type)
    {
    }
  return -1;
}

void
gda_mysql_connection_free (Gda_ServerConnection *cnc)
{
  MYSQL_Connection* mysql_cnc;

  g_return_if_fail(cnc != NULL);

  mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
  if (mysql_cnc)
    {
      g_free((gpointer) mysql_cnc);
      gda_server_connection_set_user_data(cnc, NULL);
    }
}

void
gda_mysql_error_make (Gda_ServerError *error,
		      Gda_ServerRecordset *recset,
		      Gda_ServerConnection *cnc,
		      gchar *where)
{
  MYSQL_Connection* mysql_cnc;

  g_return_if_fail(cnc != NULL);

  mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
  if (mysql_cnc)
    {
      gda_server_error_set_description(error, mysql_error(mysql_cnc->mysql));
      gda_log_error(_("error '%s' at %s"), gda_server_error_get_description(error), where);

      gda_server_error_set_number(error, mysql_errno(mysql_cnc->mysql));
      gda_server_error_set_source(error, "[gda-mysql]");
      gda_server_error_set_help_file(error, _("Not available"));
      gda_server_error_set_help_context(error, _("Not available"));
      gda_server_error_set_sqlstate(error, _("error"));
      gda_server_error_set_native(error, gda_server_error_get_description(error));
    }
}

/*
 * Schema functions
 */
static Gda_ServerRecordset *
schema_tables (Gda_ServerError *error,
	       Gda_ServerConnection *cnc,
	       GDA_Connection_Constraint *constraints,
	       gint length)
{
  Gda_ServerRecordset* recset;
  MYSQL_Recordset*     mysql_recset;
  MYSQL_Connection*    mysql_cnc;

  recset = gda_server_recordset_new(cnc);
  mysql_recset = (MYSQL_Recordset *) gda_server_recordset_get_user_data(recset);
  mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
  if (mysql_recset && mysql_cnc)
    {
      gchar*                     table_name = NULL;
      GDA_Connection_Constraint* ptr = NULL;
      gint                       cnt;

      /* process constraints */
      ptr = constraints;
      for (cnt = 0; cnt < length && ptr != NULL; cnt++)
	{
	  switch (ptr->ctype)
	    {
	    case GDA_Connection_OBJECT_NAME :
	      table_name = ptr->value;
	      break;
	    default :
	    }
	}

      /* get list of tables from MySQL */
      mysql_recset->mysql_res = mysql_list_tables(mysql_cnc->mysql, table_name);
      if (mysql_recset->mysql_res)
	{
	  gda_mysql_init_recset_fields(recset, mysql_recset);
	  return recset;
	}
      else gda_server_error_make(error, 0, cnc, __PRETTY_FUNCTION__);
    }
  else gda_server_error_set_description(error, _("mysql_recset is NULL"));
  return NULL;
}

static Gda_ServerRecordset *
schema_columns (Gda_ServerError *error,
	       Gda_ServerConnection *cnc,
	       GDA_Connection_Constraint *constraints,
	       gint length)
{
  Gda_ServerCommand*         cmd;
  Gda_ServerRecordset*       recset;
  gchar*                     query;
  gchar*                     table_name = NULL;
  GDA_Connection_Constraint* ptr;
  gint                       cnt;

  /* process constraints */
  ptr = constraints;
  for (cnt = 0; cnt < length && ptr != NULL; cnt++)
    {
      switch (ptr->ctype)
	{
	case GDA_Connection_OBJECT_NAME :
	  table_name = ptr->value;
	  break;
	default :
	}
    }

  /* build command object and query */
  if (table_name)
    {
      cmd = gda_server_command_new(cnc);
      query = g_strdup_printf("SHOW COLUMNS FROM %s", table_name);
      
      /* execute command */
      gda_server_command_set_text(cmd, query);
      recset = gda_server_command_execute(cmd, error, NULL, NULL, 0);
      if (recset) return recset;
      else gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
    }
  else gda_server_error_set_description(error, _("No table name given for SCHEMA_COLS"));
  return NULL;
}
