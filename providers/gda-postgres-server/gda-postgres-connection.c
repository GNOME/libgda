/* GNOME-DB
 * Copyright (c) 1998-2000 by Rodrigo Moya
 * Copyright (c) 2000 by Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include "gda-postgres.h"
#include <ctype.h>

typedef Gda_ServerRecordset* (*schema_ops_fn)(Gda_ServerError *,
					      Gda_ServerConnection *,
					      GDA_Connection_Constraint *,
					      gint );
typedef struct _Gda_connection_data 
{
  gchar *dsn;
  gchar *user;
  guint nreg; /* number of registered users of that structure */
  guint n_types; /* total number of data types 
		    which will have to be destroyed when the struct is 
		    destroyed*/
  POSTGRES_Types_Array *types_array;
} Gda_connection_data;


static void                 initialize_schema_ops (void);
static gint                 execute_command (Gda_ServerConnection *cnc, gchar *cmd);
static gboolean             gda_postgres_connection_is_type_known (POSTGRES_Connection *cnc,
								   gulong sql_type);
static gchar*               gda_postgres_connection_get_type_name(POSTGRES_Connection *cnc, 
								  gulong oid);
static void                 add_replacement_function(Gda_ServerRecordset *recset,
						     POSTGRES_Recordset_Replacement *repl,
						     gchar *sql_type);
static gfloat               get_postmaster_version(PGconn *conn);
static Gda_connection_data *find_connection_data(POSTGRES_Connection *cnc);


schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] =
{
  0,
};





/* hidden data types */
#define IN_PG_TYPES_HIDDEN "('SET', 'cid', 'int2vector', 'oidvector', 'regproc', 'smgr', 'tid', 'unknown', 'xid')"

/* These are the data types for which we are able to make a conversion with 
   C type and GDA type. For the other data types, the C type will be
   SQL_C_CHAR and the Gda type GDA_TypeVarchar as returned by
   gda_postgres_connection_get_c_type() and
   gda_postgres_connection_get_gda_type() 
*/

#define POSTGRES_Types_Array_Nb 27
POSTGRES_Types_Array types_array[POSTGRES_Types_Array_Nb] = 
{
  {"datetime", 0, GDA_TypeDbTimestamp, SQL_C_TIMESTAMP},   /* DO NOT REMOVE */
  {"timestamp", 0, GDA_TypeDbTimestamp, SQL_C_TIMESTAMP},  /* DO NOT REMOVE */
  {"abstime", 0, GDA_TypeDbTimestamp, SQL_C_TIMESTAMP},    /* DO NOT REMOVE */
  {"interval", 0, GDA_TypeDbTimestamp, SQL_C_TIMESTAMP},   /**/
  {"tinterval", 0, GDA_TypeDbTimestamp, SQL_C_TIMESTAMP},  /**/
  {"reltime", 0, GDA_TypeDbTimestamp, SQL_C_TIMESTAMP},    /**/
  {"bool", 0, GDA_TypeBoolean, SQL_C_BIT},                 /* DO NOT REMOVE */
  {"bpchar", 0, GDA_TypeChar, SQL_C_CHAR}, 
  {"char", 0, GDA_TypeChar, SQL_C_CHAR},
  {"date", 0, GDA_TypeDbDate, SQL_C_DATE},
  {"float4", 0, GDA_TypeSingle, SQL_C_FLOAT}, 
  {"float8", 0, GDA_TypeDouble, SQL_C_DOUBLE}, 
  {"int2", 0, GDA_TypeSmallint, SQL_C_SSHORT},             /* DO NOT REMOVE */
  {"int4", 0, GDA_TypeInteger, SQL_C_SLONG},
  {"text", 0, GDA_TypeLongvarchar, SQL_C_CHAR}, 
  {"varchar", 0 , GDA_TypeVarchar, SQL_C_CHAR},            /* DO NOT REMOVE */
  {"char2", 0, GDA_TypeChar, SQL_C_CHAR},
  {"char4", 0, GDA_TypeChar, SQL_C_CHAR},
  {"char8", 0, GDA_TypeChar, SQL_C_CHAR},
  {"char16", 0, GDA_TypeChar, SQL_C_CHAR},
  {"name", 0, GDA_TypeChar, SQL_C_CHAR},
  {"bytea", 0, GDA_TypeVarbin, SQL_C_BINARY},
  {"oid", 0, GDA_TypeInteger, SQL_C_SLONG},
  {"xid", 0, GDA_TypeInteger, SQL_C_SLONG},
  {"time", 0, GDA_TypeDbTime, SQL_C_TIME},
  {"timez", 0, GDA_TypeDbTime, SQL_C_TIME},                /**/
  {"money", 0, GDA_TypeSingle, SQL_C_FLOAT}
};
GSList *global_connection_data_list = NULL;



/*
 * Public functions
 */
gboolean
gda_postgres_connection_new (Gda_ServerConnection *cnc)
{
  static gboolean initialized = FALSE;
  POSTGRES_Connection *c;

  if (!initialized)
    {
      initialize_schema_ops();
      initialized = TRUE;
    }

  /* initialize everything to 0 to avoid core dumped */
  c = g_new0(POSTGRES_Connection, 1);
  fprintf(stderr, "A new gda_Postgres_Connection is at %p\n", c);
  c->pq_host = 0;
  c->pq_port = 0;
  c->pq_options = 0;
  c->pq_tty = 0;
  c->pq_db = 0;
  c->pq_login = 0;
  c->pq_pwd = 0;
  c->pq_conn = 0;
  c->errors = 0;
  c->types_array = NULL;

  gda_server_connection_set_user_data(cnc, (gpointer) c);
  return TRUE;
}

static gchar*
get_value(gchar* ptr)
{
  while (*ptr && *ptr != '=')
    ptr++;
  if (!*ptr)
    return 0;
  ptr++;
  if (!*ptr)
    return 0;
  while (*ptr && isspace(*ptr))
    ptr++;
  return (g_strdup(ptr));
}

/* returns the version as for example 6.53 or 7.02 for 6.5.3 or 7.0.2 
   so that versions can be numerically compared */
static gfloat version_to_float(gchar *str)
{
  gfloat ver=0;
  gchar *ptr = str;
  gfloat mult=1.0;
  gboolean first = TRUE;
  
  while (*ptr != 0)
    {
      if (*ptr == '.')
	{
	  if (first)
	    {
	      mult = 0.1;
	      first = FALSE;
	    }
	}
      else
	{
	  if (mult >= 1)
	    {
	      ver = mult * ver + (*ptr - '0');
	      mult *= 10;
	    }
	  else
	    {
	      ver += (*ptr - '0') *mult;
	      mult /= 10;
	    }
	}

      ptr ++;
    }

  return ver;
}

static gfloat get_postmaster_version(PGconn *conn)
{
  PGresult *res;
  gchar *nver;
  gchar *ptr, *index, *index2;
  gfloat retval;
  gboolean first = TRUE;

  res = PQexec(conn, "SELECT version()");
  if (!res || (PQresultStatus(res) != PGRES_TUPLES_OK))
    {
      if (res) 
	PQclear(res);
      return (-1);
    }  
  nver = g_strdup(PQgetvalue(res, 0, 0));
  PQclear(res);

  ptr = strtok(nver, " ");
  ptr = strtok(NULL, " ");
  retval = version_to_float(ptr);

  g_free(nver);

  return retval;
}

/* open new connection to database server */
gint
gda_postgres_connection_open (Gda_ServerConnection *cnc, 
			      const gchar *dsn,
			      const gchar *user, 
			      const gchar *password)
{
  POSTGRES_Connection *pc;
  gchar *ptr_s, *ptr_e;
  PGresult *res;
  gint i, j, cmp, length;
  gboolean found;
  GSList *list;
  Gda_connection_data *data=NULL;

  g_return_val_if_fail(cnc != NULL, -1);

  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
  if (pc)
    {
      /* parse connection string */
      ptr_s = (gchar *) dsn;
      while (ptr_s && *ptr_s)
	{
	  ptr_e = strchr(ptr_s, ';');
	  if (ptr_e) *ptr_e = '\0';
	  if (strncasecmp(ptr_s, "HOST", strlen("HOST")) == 0)
	    pc->pq_host = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "DATABASE", strlen("DATABASE")) == 0)
	    pc->pq_db = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "PORT", strlen("PORT")) == 0)
	    pc->pq_port = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "OPTIONS", strlen("OPTIONS")) == 0)
	    pc->pq_options = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "TTY", strlen("TTY")) == 0)
	    pc->pq_tty = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "LOGIN", strlen("LOGIN")) == 0)
	    pc->pq_login = get_value(ptr_s);
	  else if (strncasecmp(ptr_s, "PASSWORD", strlen("PASSWORD")) == 0)
	    pc->pq_pwd = get_value(ptr_s);
	  ptr_s = ptr_e;
	  if (ptr_s)
	    ptr_s++;
	}
      if (!pc->pq_login) 
	pc->pq_login = user != 0 ? g_strdup(user) : 0;
      if (!pc->pq_pwd)
	pc->pq_pwd = password != 0 ? g_strdup(password) : 0;

      /* actually establish the connection */
      pc->pq_conn = PQsetdbLogin(pc->pq_host, pc->pq_port, pc->pq_options,
				 pc->pq_tty, pc->pq_db, pc->pq_login, 
				 pc->pq_pwd);
      fprintf(stderr, "gda_postgres_connection_open(): DSN=%s\n\tpc=%p and "
	      "pq_conn=%p\n", dsn, pc, pc->pq_conn);
      if (PQstatus(pc->pq_conn) != CONNECTION_OK)
	{
	  gda_server_error_make(gda_server_error_new(), NULL, cnc, 
				__PRETTY_FUNCTION__);
	  return (-1);
	}
      
      /* set the version of postgres for that connection */
      pc->version = get_postmaster_version(pc->pq_conn);

      /*
       * is there already a types_array filled (same dsn, same user)? 
       */
      found = FALSE;
      list = global_connection_data_list;
      while (list && !found)
	{
	  data = (Gda_connection_data *)(list->data);
	  if (!strcmp(data->dsn, dsn) && !strcmp(data->user, user))
	    found = TRUE;
	  else
	    list = g_slist_next(list);
	}
      if (found)
	{
	  /* use the types_array previously defined */
	  pc->types_array = data->types_array;
	  data->nreg++;
	}
      else 
	{
	  /* 
	   * now loads the correspondance between postgres 
	   * data types and GDA types 
	   */
	  guint added_types_index;
       
	  res = PQexec(pc->pq_conn,
		       "SELECT typname, oid FROM pg_type "
		       "WHERE typrelid = 0 AND typname !~ '^_.*' "
		       "AND typname not in " IN_PG_TYPES_HIDDEN " "
		       "ORDER BY typname");

	  if (!res || (PQresultStatus(res) != PGRES_TUPLES_OK))
	    {
	      if (res) 
		PQclear(res);
	      pc->types_array = NULL;
	      gda_server_error_make(gda_server_error_new(), 0, cnc, 
				    __PRETTY_FUNCTION__);
	      return (-1);
	    }

	  length = PQntuples(res);
	  /* allocation for types array (static and dynamic parts togather */
	  pc->types_array = g_new0(POSTGRES_Types_Array, length);

	  /* we don't do any memcopy because some of the declared data types
	     in the array might not exist in the real DB. */
	  added_types_index = 0;

	  /* filling the array */
	  for (i=0; i<length; i++)
	    {
	      j=0;
	      found = FALSE;
	      /* trying to find it in the declared array */
	      while (!found && (j<POSTGRES_Types_Array_Nb))
		{
		  cmp = strcmp(PQgetvalue(res, i, 0), 
			       types_array[j].postgres_type); 
		  if (!cmp) 
		    {
		      POSTGRES_Types_Array *pty;

		      found = TRUE;
		      pty = pc->types_array + added_types_index;
		      pty->postgres_type = g_strdup(PQgetvalue(res, i, 0));
		      pty->oid = atoi(PQgetvalue(res, i, 1));
		      pty->gda_type = types_array[j].gda_type;
		      pty->c_type = types_array[j].c_type;
		      added_types_index ++;
		    }
		  j++;
		}
	      if (!found) /* then insert the type at the end as an add-on */
		{
		  POSTGRES_Types_Array *pty;

		  pty = pc->types_array + added_types_index;
		  pty->postgres_type = g_strdup(PQgetvalue(res, i, 0));
		  pty->oid = atoi(PQgetvalue(res, i, 1));
		  pty->gda_type = GDA_TypeVarchar;
		  pty->c_type = SQL_C_CHAR;
		  added_types_index ++;
		}
	    }


	  PQclear(res);

	  /* create a new struct for other similar connections */
	  data = (Gda_connection_data *) g_malloc(sizeof(Gda_connection_data));
	  data->n_types = added_types_index;
	  data->dsn = g_strdup(dsn);
	  data->user = g_strdup(user);
	  data->nreg = 1;
	  data->types_array = pc->types_array;
	  global_connection_data_list = 
	    g_slist_append(global_connection_data_list, data);
	}
      
      /*
       * Sets the DATE format for all the current session to DD-MM-YYYY
       */
      res = PQexec(pc->pq_conn, "SET DATESTYLE TO 'SQL, US'");
      PQclear(res);
      /*
       * the TIMEZONE is left to what is the default, without trying to impose
       * one. Otherwise the command would be:
       * "SET TIME ZONE '???'" or "SET TIME ZONE DEFAULT"
       */
    }
  else
    return -1;

  return (0);
}


void
gda_postgres_connection_close (Gda_ServerConnection *cnc)
{
  POSTGRES_Connection *c;
  gboolean found;
  GSList *list;
  Gda_connection_data *data=NULL;

  g_return_if_fail(cnc != NULL);

  c = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
  if (c)
    {
      if (c->types_array)
	{
	  list = global_connection_data_list;
	  found = FALSE;
	  while (list && !found)
	    {
	      data = (Gda_connection_data*)(list->data);
	      if (c->types_array == data->types_array)
		found = TRUE;
	      else
		list = g_slist_next(list);
	    }
	  if (found)
	    { 
	      if (data->nreg == 1)
		{
		  guint i;
		  POSTGRES_Types_Array *pty;

		  g_free(data->dsn);
		  g_free(data->user);
		  /* liberating the memory for data ALL the types */
		  for (i=0; i<data->n_types; i++)
		    {
		      pty = c->types_array + i;
		      g_free(pty->postgres_type);
		    }
		  /* now free one chunck because was reallocated as one */
		  g_free(data->types_array); 

		  global_connection_data_list = 
		    g_slist_remove_link(global_connection_data_list, list);
		  g_free(data);
		}
	      else /* some other connections are using it */
		{
		  data->nreg--;
		}
	    }
	  else /* types_array not in list if there was an error while loading */
	    g_free(c->types_array);
	  c->types_array = NULL;
	}
      /* check connection status */
      if (PQstatus(c->pq_conn) == CONNECTION_OK)
	{
	  fprintf(stderr, "gda_postgres_connection_close(%p) pq_conn=%p\n", 
		  c, c->pq_conn);
	  PQfinish(c->pq_conn);
	  c->pq_conn = 0;
	}
    }
}

/* BEGIN TRANS */
gint
gda_postgres_connection_begin_transaction (Gda_ServerConnection *cnc)
{
  return (execute_command(cnc, "BEGIN"));
}

/* COMMIT */
gint
gda_postgres_connection_commit_transaction (Gda_ServerConnection *cnc)
{
  return (execute_command(cnc, "COMMIT"));
}

/* ROLLBACK */
gint
gda_postgres_connection_rollback_transaction (Gda_ServerConnection *cnc)
{
  return (execute_command(cnc, "ROLLBACK"));
}


/* schemas */
Gda_ServerRecordset *
gda_postgres_connection_open_schema (Gda_ServerConnection *cnc,
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
  else
    gda_log_error(_("Unhandled SCHEMA_QTYPE %d"), (gint) t);
  return NULL;
}

glong
gda_postgres_connection_modify_schema (Gda_ServerConnection *cnc,
                                   GDA_Connection_QType t,
                                   GDA_Connection_Constraint *constraints,
                                   gint length)
{
  return -1;
}

/* logging */
gint
gda_postgres_connection_start_logging (Gda_ServerConnection *cnc, 
				       const gchar *filename)
{
  FILE *f;
  gchar *str;
  POSTGRES_Connection *pc;
  
  /* check parameters */
  g_return_val_if_fail(cnc != 0, -1);
  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
  if (pc)
    {
      if (PQstatus(pc->pq_conn) != CONNECTION_OK)
	return (-1);
      
      /* open given file or default one if 0 */
      str = filename ? g_strdup(filename) : 
	g_strdup_printf("%s/gda.log", g_get_home_dir());
      if (!(f = fopen(str, "w")))
	{
	  if (str)
	    g_free(str);
	  return (-1);
	}
      PQtrace(pc->pq_conn, f);
      
      /* free memory */
      if (str != filename)
	g_free(str);
      return (0);
    }
  else
    return -1;
}

gint
gda_postgres_connection_stop_logging (Gda_ServerConnection *cnc)
{
  POSTGRES_Connection *pc;

  /* check parameters */
  g_return_val_if_fail(cnc != 0, -1);
  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
  if (pc)
    {
      if (PQstatus(pc->pq_conn) != CONNECTION_OK)
	return (-1);
      PQuntrace(pc->pq_conn);
    }
  return (0);
}

gchar *
gda_postgres_connection_create_table (Gda_ServerConnection *cnc,
				      GDA_RowAttributes *columns)
{
  return NULL;
}


gboolean
gda_postgres_connection_supports (Gda_ServerConnection* cnc,
				  GDA_Connection_Feature feature)
{
  gboolean retval;

  g_return_val_if_fail(cnc != NULL, FALSE);

  switch (feature)
    {
    case GDA_Connection_FEATURE_TRANSACTIONS:
    case GDA_Connection_FEATURE_SEQUENCES:
    case GDA_Connection_FEATURE_PROCS:
    case GDA_Connection_FEATURE_INHERITANCE:
      retval = TRUE;
      break;

    default :
      retval = FALSE;
    }

  return retval;
}



GDA_ValueType           
gda_postgres_connection_get_gda_type_psql (POSTGRES_Connection *cnc, 
					   gulong sql_type)
{
  GDA_ValueType gda_type = GDA_TypeVarchar; /* default value */
  gboolean found;
  gint i, max=0;
  Gda_connection_data *data;

  g_return_val_if_fail((cnc != NULL), GDA_TypeNull);
  g_return_val_if_fail((cnc->types_array != NULL), GDA_TypeNull);

  data = find_connection_data(cnc);
  if (data)
    max = data->n_types;

  found = FALSE;
  i = 0;
  while ((i< max) && !found) 
    {
      if (cnc->types_array[i].oid == sql_type) 
	{
	  gda_type = cnc->types_array[i].gda_type;
	  found = TRUE;
	}
      i++;
    }
  return gda_type;
}

GDA_ValueType           
gda_postgres_connection_get_gda_type (Gda_ServerConnection *cnc, 
				      gulong sql_type)
{
  POSTGRES_Connection *pc;

  g_return_val_if_fail(cnc != NULL, GDA_TypeNull);
  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
  return gda_postgres_connection_get_gda_type_psql(pc, sql_type);
}


POSTGRES_CType     
gda_postgres_connection_get_c_type_from_sql (POSTGRES_Connection *cnc, 
					     gulong sql_type)
{
  POSTGRES_CType c_type = SQL_C_CHAR; /* default value */
  gboolean found;
  gint i, max=0;
  Gda_connection_data *data;

  g_return_val_if_fail((cnc != NULL), SQL_C_CHAR);
  g_return_val_if_fail((cnc->types_array != NULL), SQL_C_CHAR);

  data = find_connection_data(cnc);
  if (data)
    max = data->n_types;

  found = FALSE;
  i = 0;
  while ((i< max) && !found) 
    {
      if (cnc->types_array[i].oid == sql_type) 
	{
	  c_type = cnc->types_array[i].c_type;
	  found = TRUE;
	}
      i++;
    }
  return c_type;
}

gshort
gda_postgres_connection_get_c_type_psql (POSTGRES_Connection *cnc, 
					 GDA_ValueType gda_type)
{
  POSTGRES_CType c_type = SQL_C_CHAR; /* default value */
  gboolean found;
  gint i, max=0;
  Gda_connection_data *data;

  g_return_val_if_fail((cnc != NULL), SQL_C_CHAR);
  g_return_val_if_fail((cnc->types_array != NULL), SQL_C_CHAR);

  data = find_connection_data(cnc);
  if (data)
    max = data->n_types;

  found = FALSE;
  i = 0;
  while ((i< max) && !found) 
    {
      if (cnc->types_array[i].gda_type == gda_type) 
	{
	  c_type = cnc->types_array[i].c_type;
	  found = TRUE;
	}
      i++;
    }

  return c_type;
}

gshort
gda_postgres_connection_get_c_type (Gda_ServerConnection *cnc, 
				    GDA_ValueType gda_type)
{
  POSTGRES_Connection *pc;

  g_return_val_if_fail(cnc != NULL, -1);
  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
  return gda_postgres_connection_get_c_type_psql(pc, gda_type);
}

gchar *
gda_postgres_connection_sql2xml (Gda_ServerConnection *cnc, const gchar *sql)
{
  return NULL;
}

gchar *
gda_postgres_connection_xml2sql (Gda_ServerConnection *cnc, const gchar *xml)
{
  return NULL;
}

void
gda_postgres_connection_free (Gda_ServerConnection *cnc)
{
  POSTGRES_Connection *pc;
  g_return_if_fail(cnc != 0);
  gda_postgres_connection_close(cnc);

  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
  if (pc)
    {
      if (pc->pq_host != 0) g_free((gpointer) pc->pq_host);
      if (pc->pq_port != 0) g_free((gpointer) pc->pq_port);
      if (pc->pq_options != 0) g_free((gpointer) pc->pq_options);
      if (pc->pq_tty != 0) g_free((gpointer) pc->pq_tty);
      if (pc->pq_db != 0) g_free((gpointer) pc->pq_db);
      if (pc->pq_login != 0) g_free((gpointer) pc->pq_login);
      if (pc->pq_pwd != 0) g_free((gpointer) pc->pq_pwd);

      fprintf(stderr, "gda_postgres_connection_free(%p)\n", pc);
      g_free((gpointer) pc);
      gda_server_connection_set_user_data(cnc, NULL);
    }
}


void
gda_postgres_error_make (Gda_ServerError *error,
			 Gda_ServerRecordset *recset, /* can be NULL! */
			 Gda_ServerConnection *cnc,
			 gchar *where)
{
  POSTGRES_Connection* pc;
  POSTGRES_Recordset*  pr;
  
  g_return_if_fail(cnc != NULL);

  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);

  if (pc)
    {
      pr = gda_server_recordset_get_user_data(recset);
      if (pr && pr->pq_data)
	gda_server_error_set_description(error, 
					 PQresultErrorMessage(pr->pq_data));
      else
	{
	  if (pc->pq_conn)
	    gda_server_error_set_description(error, 
					     PQerrorMessage(pc->pq_conn));
	  else
	    gda_server_error_set_description(error, "NO DESCRIPTION");
	}
      gda_log_error(_("error '%s' at %s"), 
		    gda_server_error_get_description(error), where);
      if (pr && pr->pq_data)
	gda_server_error_set_number(error, PQresultStatus(pr->pq_data));
      else
	gda_server_error_set_number(error, 0);
      gda_server_error_set_source(error, "[gda-postgres]");
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
               GDA_Connection_Constraint *constraint,
               gint length)
{

  Gda_ServerRecordset*       recset = 0;
  Gda_ServerCommand*         cmd;

  GString*                   query;
  GDA_Connection_Constraint* ptr;
  gboolean                   extra_info = FALSE;
  gint                       cnt;
  GString*                   and_condition = 0;
  gulong                     affected;

  /* process constraints */
  ptr = constraint;
  for (cnt = 0; cnt < length && ptr != 0; cnt++)
    {
      switch (ptr->ctype)
	{
	case GDA_Connection_EXTRA_INFO :
	  extra_info = TRUE;
	  break;
	case GDA_Connection_OBJECT_NAME :
	  if (!and_condition) and_condition = g_string_new("");
	  g_string_sprintfa(and_condition, "AND a.relname='%s' ", ptr->value);
	  fprintf(stderr, "schema_tables: table name = '%s'\n", ptr->value);
	  break;
	case GDA_Connection_OBJECT_SCHEMA :
	  if (!and_condition) and_condition = g_string_new("");
	  g_string_sprintfa(and_condition, "AND b.usename='%s' ", ptr->value);
	  fprintf(stderr, "schema_tables: table_schema = '%s'\n", ptr->value);
	  break;
	case GDA_Connection_OBJECT_CATALOG :
	  fprintf(stderr, "schema_procedures: proc_catalog = '%s' UNUSED!\n", 
		  ptr->value);
	  break;
	default :
	  fprintf(stderr, "schema_tables: invalid constraint type %d\n", 
		  ptr->ctype);
	  g_string_free(and_condition, TRUE);
	  return (0);
	}
      ptr++;
    }
  
  /* build the query */
  query = g_string_new("SELECT a.relname AS \"Name\", ");
  if (extra_info) 
    g_string_append(query, "b.usename AS \"Owner\", "
		    "obj_description(a.oid) AS \"Comments\", "
		    "a.relname AS \"SQL\" ");
  else 
    g_string_append(query, "obj_description(a.oid) AS \"Comments\" ");
  g_string_append(query, " FROM pg_class a, pg_user b "
		  "WHERE ( relkind = 'r') and relname !~ '^pg_' "
		  "AND relname !~ '^xin[vx][0-9]+' AND "
		  "b.usesysid = a.relowner AND "
		  "NOT (EXISTS (SELECT viewname FROM pg_views "
		  "WHERE viewname=a.relname)) ");
  if (and_condition) 
    g_string_sprintfa(query, "%s", and_condition->str);
  
  g_string_append(query, " ORDER BY a.relname");
  if (and_condition) g_string_free(and_condition, TRUE);

  /* build the command object */
  cmd = gda_server_command_new(cnc);
  gda_server_command_set_text(cmd, query->str);
  g_string_free(query, TRUE);
  
  /* execute the command */
  recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
  gda_server_command_free(cmd);

  if (recset && extra_info)
    {
      POSTGRES_Recordset *prc;
      POSTGRES_Recordset_Replacement *repl;

      prc = (POSTGRES_Recordset *) gda_server_recordset_get_user_data(recset);
      repl = g_new0(POSTGRES_Recordset_Replacement, 1); 
      repl->colnum = 3;
      repl->newtype = GDA_TypeVarchar;
      repl->trans_func = replace_TABLE_NAME_with_SQL;
      add_replacement_function(recset, repl, "varchar");
    }
      
  if (!recset)
    gda_server_error_set_description(error, _("postgres_recset is NULL"));
  return (recset);
}



static Gda_ServerRecordset *
schema_columns (Gda_ServerError *error,
                Gda_ServerConnection *cnc, 
		GDA_Connection_Constraint *constraint,
                gint length)
{
  Gda_ServerCommand *cmd;
  Gda_ServerRecordset *recset = 0;
  GDA_Connection_Constraint *ptr;
  gulong affected;
  gchar *table_qualifier = 0, *table_owner = 0, *table_name = 0, 
    *column_name = 0;
  gchar *query;
  
  /* get table name */
  ptr = constraint;
  while (length)
    {
      switch (ptr->ctype)
        {
        case GDA_Connection_OBJECT_CATALOG : /* not used in postgres! */
          table_qualifier = ptr->value;
          fprintf(stderr, "schema_columns: table_qualifier = '%s' UNUSED!\n", 
		  table_qualifier);
          break;
        case GDA_Connection_OBJECT_SCHEMA :
          table_owner = ptr->value;
          fprintf(stderr, "schema_columns: table_owner = '%s'\n", table_owner);
          break;
        case GDA_Connection_OBJECT_NAME :
          table_name = ptr->value;
          fprintf(stderr, "schema_columns: table_name = '%s'\n", table_name);
          break;
        case GDA_Connection_COLUMN_NAME :
          column_name = ptr->value;
          fprintf(stderr, "schema_columns: column_name = '%s'\n", column_name);
          break;
        default:
          fprintf(stderr,"schema_columns: invalid constraint type %d\n", 
		  ptr->ctype);
          return (0);
        }
      length--;
      ptr++;
    }

  if (!table_name)
    {
      fprintf(stderr, "schema_columns: table name is null\n");
      return (0);
    }

  query = g_strdup_printf("SELECT b.attname AS \"Name\", "
			  "c.typname AS \"Type\", "
			  "textcat(b.attlen, textcat('/', b.atttypmod)) "
			  "AS \"Size\", "
			  "0 AS \"Precision\", "
			  "NOT(b.attnotnull) AS \"Nullable\", "
			  "textcat('%s ', b.attnum) AS \"Key\", "
			  "textcat('%s ', b.attname) AS \"Default Value\", "
			  "obj_description(b.oid) AS \"Comments\" "
			  "FROM pg_type c, "
			  "pg_attribute b, "
			  "pg_class a "
			  "WHERE a.relname = '%s' "
			  "AND b.attrelid = a.oid "
			  "AND c.oid = b.atttypid "
			  "AND b.attnum > 0 "
			  "ORDER BY b.attnum", table_name,
			  table_name, table_name);

  /* build the command object */
  cmd = gda_server_command_new(cnc);
  gda_server_command_set_text(cmd, query);
  g_free(query);

  /* execute the command */
  recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
  gda_server_command_free(cmd);
  fprintf(stderr, "Nb of columns: %ld\n", affected);

  if (recset)
    { /* Setting replacements for column length */
      POSTGRES_Recordset *prc;
      POSTGRES_Recordset_Replacement *repl;

      prc = (POSTGRES_Recordset *) gda_server_recordset_get_user_data(recset);
      repl = g_new0(POSTGRES_Recordset_Replacement, 1); 
      repl->colnum = 2;
      repl->newtype = GDA_TypeSmallint;
      repl->trans_func = replace_TABLE_FIELD_with_length;
      add_replacement_function(recset, repl, "int2");
      /* Setting replacement for default value */
      repl = g_new0(POSTGRES_Recordset_Replacement, 1); 
      repl->colnum = 6;
      repl->newtype = GDA_TypeVarchar;
      repl->trans_func = replace_TABLE_FIELD_with_defaultval;
      add_replacement_function(recset, repl, "varchar");
      /* Setting replacements for key or not */
      repl = g_new0(POSTGRES_Recordset_Replacement, 1); 
      repl->colnum = 5;
      repl->newtype = GDA_TypeBoolean;
      repl->trans_func = replace_TABLE_FIELD_with_iskey;
      add_replacement_function(recset, repl, "bool");
    }
  else
    gda_server_error_set_description(error, _("postgres_recset is NULL"));
  return (recset);
}


static Gda_ServerRecordset *
schema_tab_parents (Gda_ServerError *error,
		    Gda_ServerConnection *cnc,
		    GDA_Connection_Constraint *constraint,
		    gint length)
{
  GString*                   query;
  Gda_ServerCommand*         cmd;
  Gda_ServerRecordset*       recset = 0;
  POSTGRES_Connection*       pc;
  GDA_Connection_Constraint* ptr;
  gint                       cnt, oid;
  gchar*                     table_name=NULL;
  PGresult*                  res;
  gulong                     affected;

  /* process constraints */
  ptr = constraint;
  for (cnt = 0; cnt < length && ptr != 0; cnt++)
    {
      switch (ptr->ctype)
        {
        case GDA_Connection_OBJECT_CATALOG : /* not used in postgres */
	  break;
        case GDA_Connection_OBJECT_NAME :
          table_name = ptr->value;
          fprintf(stderr, "schema_tab_parents: table_name = '%s'\n", 
		  table_name);
          break;
        default:
          fprintf(stderr, "schema_tab_parents: invalid constraint type %d\n", 
		  ptr->ctype);
          return (0);
        }
      ptr++;
    }

  if (!table_name)
    {
      fprintf(stderr, "schema_tab_parents: table name is null\n");
      return (0);
    }

  /* find the oid of the table */
  query = g_string_new("SELECT oid from pg_class where ");
  g_string_sprintfa(query, "relname = '%s'", table_name);
  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
  res = PQexec(pc->pq_conn, query->str);
  g_string_free(query, TRUE);
  if (!res || (PQresultStatus(res) != PGRES_TUPLES_OK) ||
      (PQntuples(res) < 1))
    {
      if (res)
	PQclear(res);
      return NULL;
    }
  oid = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  /* build the query */
  query = g_string_new("SELECT a.relname AS \"Table Name\", "
		       "b.inhseqno AS \"Sequence\" FROM "
		       "pg_inherits b, pg_class a WHERE a.oid=b.inhparent "
		       "AND b.inhrel = ");
  g_string_sprintfa(query, "%d order by b.inhseqno", oid);

  /* build the command object */
  cmd = gda_server_command_new(cnc);
  gda_server_command_set_text(cmd, query->str);
  g_string_free(query, TRUE);

  /* execute the command */
  recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
  gda_server_command_free(cmd);
  fprintf(stderr, "Nb of tab parents: %ld\n", affected);

  return (recset);
}


static Gda_ServerRecordset *
schema_procedures (Gda_ServerError *error,
                   Gda_ServerConnection *cnc,
                   GDA_Connection_Constraint *constraint,
                   gint length)
{
  gboolean                   extra_info = FALSE;
  gint                       cnt, i, cntnt, cntntsym;
  GDA_Connection_Constraint* ptr;
  POSTGRES_Connection        *pc;
  Gda_ServerRecordset*       recset = 0;
  gchar*                     proc_name = 0;
  gchar*                     proc_owner = 0;
  GString*                   query=NULL;
  GString*                   and_value=NULL;
  gboolean                   where;
  gulong                     affected;
  Gda_Builtin_Result*        bres;
  PGresult                   *res, *notypes, *notypes_sym;
  gchar                      **row;
  gint                       nbcols; /* nb of cols in the bres */

  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);

  /* process constraints */
  ptr = constraint;
  for (cnt = 0; cnt < length && ptr != 0; cnt++)
    {
      switch (ptr->ctype)
        {
        case GDA_Connection_OBJECT_NAME :
          proc_name = ptr->value;
          fprintf(stderr, "schema_procedures: proc_name = '%s'\n", proc_name);
	  if (!and_value)
	    {
	      and_value = g_string_new("");
	      g_string_sprintfa(and_value, "p.proname = '%s' ", ptr->value);
	    }
	  else
	    g_string_sprintfa(and_value, "AND p.proname = '%s' ", ptr->value);
          break;
        case GDA_Connection_OBJECT_SCHEMA :
          proc_owner = ptr->value;
          fprintf(stderr, "schema_procedures: proc_owner = '%s'\n",proc_owner);
	  if (!and_value)
	    {
	      and_value = g_string_new("");
	      g_string_sprintfa(and_value, "u.usename = '%s' ", ptr->value);
	    }
	  else
	    g_string_sprintfa(and_value, "AND u.usename = '%s' ", ptr->value);
          break;
        case GDA_Connection_OBJECT_CATALOG :
          fprintf(stderr, "schema_procedures: proc_catalog = '%s' UNUSED!\n", 
		  ptr->value);
          break;
        case GDA_Connection_EXTRA_INFO :
          extra_info = TRUE;
          break;
        default:
          fprintf(stderr, "schema_procedures: invalid constraint type %d\n", ptr->ctype);
          return (0);
        }
      ptr++;
    }

  /* build the query */

  /* differences between Postgres V6.5.x and 7.0.x: 
   * the function oid8types is replaced by oidvectortypes and the returned 
   value is changed from say 18 18 0 0 0 0 0 0 to 18 18, and can even be
   empty.
  */
#define IN_PG_PROCS_NON_ALLOWED "('array_assgn', 'array_clip', 'array_in', 'array_ref', 'array_set', 'bpcharin', 'btabstimecmp', 'btcharcmp', 'btfloat4cmp', 'btfloat8cmp', 'btint24cmp', 'btint2cmp', 'btint42cmp', 'btint4cmp', 'btnamecmp', 'bttextcmp', 'byteaGetBit', 'byteaGetByte', 'byteaGetSize', 'byteaSetBit', 'byteaSetByte', 'date_cmp', 'datetime_cmp', 'float8', 'hashbpchar', 'hashchar', 'hashfloat4', 'hashfloat8', 'hashint2', 'hashint4', 'hashname', 'hashtext', 'hashvarchar', 'int', 'int2', 'int4', 'lo_close', 'lo_lseek', 'lo_tell', 'loread', 'lowrite', 'pg_get_ruledef', 'pg_get_userbyid', 'pg_get_viewdef', 'pqtest', 'time_cmp', 'userfntest', 'varcharcmp', 'varcharin', 'xideq', 'keyfirsteq')"
  where = FALSE;
  if (extra_info)
    {
      if (pc->version < 7.0)
	query = g_string_new("SELECT p.proname AS \"Name\", "
			     "p.oid AS \"Object Id\", "
			     "u.usename AS \"Owner\", "
			     "obj_description(p.oid) AS \"Comments\", "
			     "p.pronargs AS \"Number of args\", "
			     "p.oid AS \"SQL Def.\", "
			     "p.prorettype, p.proargtypes " /* for checking */
			     "FROM pg_proc p, pg_user u "
			     "WHERE u.usesysid=p.proowner AND "
			     "(pronargs = 0 or oid8types(p.proargtypes)!= '') ");
      else
	query = g_string_new("SELECT p.proname AS \"Name\", "
			     "p.oid AS \"Object Id\", "
			     "u.usename AS \"Owner\", "
			     "obj_description(p.oid) AS \"Comments\", "
			     "p.pronargs AS \"Number of args\", "
			     "p.oid AS \"SQL Def.\", "
			     "p.prorettype, p.proargtypes " /* for checking */
			     "FROM pg_proc p, pg_user u "
			     "WHERE u.usesysid=p.proowner AND "
			     "(pronargs = 0 or oidvectortypes(p.proargtypes)!= '') ");
      where = TRUE;
      nbcols = 6;
    }
  else 
    {
      query = g_string_new("SELECT p.proname AS \"Name\", "
			   "p.oid AS \"Object Id\", "
			   "obj_description(p.oid) AS \"Comments\", "
			   "p.prorettype, p.proargtypes " /* for checking */
			   "FROM pg_proc p ");
      if (proc_owner)
	{
	  g_string_append(query, ", pg_user u WHERE u.usesysid=p.proowner ");
	  where = TRUE;
	}

      if (!where)
	{
	  g_string_append(query, "WHERE ");
	  where = TRUE;
	}
      else
	g_string_append(query, "AND ");
      if (pc->version < 7.0)
	g_string_append(query, "(pronargs = 0 or oid8types(p.proargtypes)!= '') ");
      else
	g_string_append(query, "(pronargs = 0 or oidvectortypes(p.proargtypes)!= '') ");
      
      nbcols = 3;
    }

  if (and_value)
    {
      if (!where)
	{
	  g_string_append(query, "WHERE ");
	  where = TRUE;
	}
      else
	g_string_append(query, "AND ");
      g_string_sprintfa(query, "%s", and_value->str);
      g_string_free(and_value, TRUE);
    }
  if (!where)
    g_string_append(query, "WHERE ");
  else
    g_string_append(query, "AND ");
  g_string_append(query, "p.proname NOT IN " IN_PG_PROCS_NON_ALLOWED " ");
  g_string_append(query, "ORDER BY proname, prorettype");

  /* build the builtin result and tell the columns types */
  bres = Gda_Builtin_Result_new(nbcols, "result", 0, -1);
  Gda_Builtin_Result_set_att(bres, 0, "Name", 
	        gda_postgres_connection_get_sql_type(pc, "varchar"), -1);
  Gda_Builtin_Result_set_att(bres, 1, "Object Id", 
		gda_postgres_connection_get_sql_type(pc, "varchar"), -1);
  if (extra_info)
    {
      Gda_Builtin_Result_set_att(bres, 2, "Owner", 
		gda_postgres_connection_get_sql_type(pc, "varchar"), -1);
      Gda_Builtin_Result_set_att(bres, 3, "Comments", 
		gda_postgres_connection_get_sql_type(pc, "varchar"), -1);
      Gda_Builtin_Result_set_att(bres, 4, "Number of Args.", 
		gda_postgres_connection_get_sql_type(pc, "int2"), 4);
      Gda_Builtin_Result_set_att(bres, 5, "Sql Def.", 
		gda_postgres_connection_get_sql_type(pc, "varchar"), -1);
    }
  else
    Gda_Builtin_Result_set_att(bres, 2, "Comments", 
		gda_postgres_connection_get_sql_type(pc, "varchar"), -1);

  /* run the query, get a PGresult, and for each tuple:
     - see if the function is authorized (not in the result of notypes)
     - see if all the parameters and return type are in the known postgres 
       types. If that is the case, add the tuple to the builtin result. 
     Then clear the PGresult */
  res = PQexec(pc->pq_conn, query->str);
  fprintf(stderr, "Query: %s\n", query->str);
  g_string_free(query, TRUE);

  /* REM: contents of PGresults notypes and of notype_sym have en empty intersection */
  /* notypes is the list of functions' oid which can be obtained 
     using operators */
  /* notypes_sym is the list of functions' oid which are synonyms with other 
     functions */

  /* before grouping here is what we had:
     notypes =PQexec(pc->pq_conn, 
                     "SELECT distinct text(oprcode) as oprcode FROM "
                     "pg_operator ORDER BY oprcode");
     notypes_sym = PQexec(pc->pq_conn, 
                     "select p.oid from pg_proc p where text(p.proname)!=p.prosrc and "
                     "p.prosrc in (select text(proname) from "
                     "pg_proc where text(proname)= p.prosrc) order by p.oid");
  */

  /* notypes and notypes_sym are unioned so we have one PGresult which is already sorted */

  notypes =PQexec(pc->pq_conn, 
		  "select distinct int4(text(oprcode)) as code FROM "
		  "pg_operator UNION "
		  "select p.oid as code from pg_proc p where "
		  "text(p.proname)!=p.prosrc and p.prosrc IN "
		  "(select text(proname) from pg_proc where text(proname)= "
		  "p.prosrc) order by code");

  if ((!res || (PQresultStatus(res) != PGRES_TUPLES_OK) ||
       (PQntuples(res) < 1)) ||
      (!notypes || (PQresultStatus(notypes) != PGRES_TUPLES_OK) ||
       (PQntuples(notypes) < 1)))
      /* || (!notypes_sym || (PQresultStatus(notypes_sym) != PGRES_TUPLES_OK) ||
       (PQntuples(notypes_sym) < 1))*/
    {

      /*fprintf(stderr, "Error: res=%p and notypes=%p and notypes_syn=%p\n", 
	      res, notypes, notypes_sym);*/
      fprintf(stderr, "Error: res=%p and notypes=%p\n", res, notypes);
      if (res)
	PQclear(res);
      if (notypes)
	PQclear(notypes);
      /*if (notypes_sym)
	PQclear(notypes_sym);*/
      return NULL;
    }
  
  cnt = PQntuples(res);
  cntnt = PQntuples(notypes);
  /*cntntsym = PQntuples(notypes_sym);*/
  row = g_new(gchar*, nbcols);

  for (i=0; i<cnt; i++) /* for every funtion returned */
    {
      gboolean insert = TRUE, cont;
      GSList *list;
      int j;

      /* check if the proc is not in the list of notypes */
      cont = TRUE;
      j = 0;
      while ((j<cntnt) && insert && cont)
	{
	  gint cmpres;
	  cmpres = atoi(PQgetvalue(res, i, 1)) - /* object id is tested */
	    atoi(PQgetvalue(notypes, j, 0));
	  if (!cmpres)
	    insert = FALSE;
	  if (cmpres < 0)
	    cont = FALSE;
	  j++;
	}

      /* check if the proc is not in the list of notypes_sym */
      /* cont = TRUE; */
/*       j = 0; */
/*       while ((j<cntntsym) && insert && cont) */
/* 	{ */
/* 	  gint cmpres; */
/* 	  cmpres = atoi(PQgetvalue(res, i, 1)) -  */
/* 	    atoi(PQgetvalue(notypes_sym, j, 0)); */
/* 	  if (!cmpres) */
/* 	    insert = FALSE; */
/* 	  if (cmpres < 0) */
/* 	    cont = FALSE; */
/* 	  j++; */
/* 	} */


      /* check if PROC is ok:
       * the return type is in col nbcols 
       * and the arg types is in col nbcols+1 
       */
      /* checking on the OUT params */
      if (insert && !gda_postgres_connection_is_type_known(pc, 
				  atoi(PQgetvalue(res, i, nbcols))))
	insert = FALSE;
      if (insert) /* checking on the IN params */
	{
	  list = convert_tabular_to_list((PQgetvalue(res, i, nbcols+1)));
	  while (list)
	    {
	      if (!gda_postgres_connection_is_type_known(pc,
				       atoi((gchar*)(list->data))))
		insert = FALSE;
	      g_free(list->data);
	      list = g_slist_remove_link(list, list);
	    }
	}
      
      /* if OK, we put it into builtin result */
      if (insert)
	{
	  for (j=0; j<nbcols; j++)
	    row[j] = PQgetvalue(res, i, j);
	  Gda_Builtin_Result_add_row(bres, row);      
	}
    }
  PQclear(res);
  PQclear(notypes);
  /*PQclear(notypes_sym);*/
  g_free(row);

  /* build the recset */
  recset = gda_postgres_command_build_recset_with_builtin (cnc,
							   bres,  
							   &affected);
  fprintf(stderr, "Nb of procs: %ld\n", affected);
  if (recset && extra_info)
    { /* Setting replacements for nb of params */
      POSTGRES_Recordset *prc;
      POSTGRES_Recordset_Replacement *repl; 

      prc = (POSTGRES_Recordset *) gda_server_recordset_get_user_data(recset);
      /* Setting replacements for SQL definition */
      repl = g_new0(POSTGRES_Recordset_Replacement, 1);
      repl->colnum = 5;
      repl->newtype = GDA_TypeVarchar;
      repl->trans_func = replace_FUNCTION_OID_with_SQL;
      add_replacement_function(recset, repl, "varchar");
    }
  return (recset);
}

static Gda_ServerRecordset *
schema_proc_params (Gda_ServerError *error,
		    Gda_ServerConnection *cnc,
		    GDA_Connection_Constraint *constraint,
		    gint length)
{
  gint                       cnt;
  GDA_Connection_Constraint* ptr;
  Gda_ServerRecordset*       recset = 0;
  gchar*                     proc_name = 0;
  gchar*                     query;
  gulong                     affected;
  Gda_Builtin_Result*        bres;
  PGresult*                  res;
  gchar                      *row[2];
  GSList*                    list;
  POSTGRES_Connection        *pc;

  /* process constraints */
  ptr = constraint;
  for (cnt = 0; cnt < length && ptr != 0; cnt++)
    {
      switch (ptr->ctype)
        {
        case GDA_Connection_OBJECT_NAME :
          proc_name = ptr->value;
          fprintf(stderr, "schema_proc_params: proc_name = '%s'\n", proc_name);
          break;
        default:
          fprintf(stderr, "schema_procedures: invalid constraint type %d\n", 
		  ptr->ctype);
          return (0);
        }
      ptr++;
    }

  if (!proc_name)
    {
      fprintf(stderr, "schema_proc_params: proc name is null\n");
      return (0);
    }  

  /* building initial builtin result */
  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
  bres = Gda_Builtin_Result_new(2, "result", 0, -1);
  Gda_Builtin_Result_set_att(bres, 0, "InOut", 
			     gda_postgres_connection_get_sql_type(pc, "varchar"),
			     -1);
  Gda_Builtin_Result_set_att(bres, 1, "Type", 
			  gda_postgres_connection_get_sql_type(pc, "varchar"),
			     -1);


  /* fetch what we want, and put it into out builtin result */
  if (pc->version < 7.0)  
    query = g_strdup_printf("SELECT prorettype, proargtypes FROM pg_proc "
			    "WHERE (pronargs = 0 or "
			    "oid8types(proargtypes)!= '')"
			    " AND oid = %s", proc_name);
  else
    query = g_strdup_printf("SELECT prorettype, proargtypes FROM pg_proc "
			    "WHERE (pronargs = 0 or "
			    "oidvectortypes(proargtypes)!= '')"
			    " AND oid = %s", proc_name);
  res = PQexec(pc->pq_conn, query);
  g_free(query);
  if (!res || (PQresultStatus(res) != PGRES_TUPLES_OK) ||
      (PQntuples(res) < 1))
    {
      if (res)
	PQclear(res);
      return NULL;
    }
  row[0] = "out";
  row[1] = gda_postgres_connection_get_type_name(pc, 
						 atoi(PQgetvalue(res, 0, 0)));
  if (row[1])
    Gda_Builtin_Result_add_row(bres, row);
  list = convert_tabular_to_list(PQgetvalue(res, 0, 1));
  PQclear(res);
  row[0] = "in";
  while (list)
    {
      row[1] = gda_postgres_connection_get_type_name(pc, 
						 atoi((gchar*)(list->data)));
      if (row[1])
	Gda_Builtin_Result_add_row(bres, row);
      if (list->data)
	g_free(list->data);
      list = g_slist_remove_link(list, list);
    }

  /* final step */
  recset = gda_postgres_command_build_recset_with_builtin (cnc,
							   bres,  
							   &affected);
  fprintf(stderr, "Nb of proc params: %ld\n", affected);
  return (recset);

  /* COMPLETE QUERY:
     SELECT p.proname as function, t.typname, u.usename, p.proargtypes as arguments,  obj_description(p.oid) as description FROM pg_proc p, pg_type t, pg_user u WHERE u.usesysid=p.proowner AND p.prorettype = t.oid and (pronargs = 0 or oid8types(p.proargtypes)!= '') ORDER BY function, typname;
  */
}

/* Updated to work with Postgres 7.0.x */
static Gda_ServerRecordset *
schema_aggregates (Gda_ServerError *error,
                   Gda_ServerConnection *cnc,
                   GDA_Connection_Constraint *constraint,
                   gint length)
{
  GString*                   query;
  Gda_ServerCommand*         cmd;
  Gda_ServerRecordset*       recset = 0;
  GDA_Connection_Constraint* ptr;
  gint                       cnt;
  GString*                   and_condition = 0;
  gulong                     affected;
  gboolean                   extra_info=FALSE;

  /* process constraints */
  ptr = constraint;
  for (cnt = 0; cnt < length && ptr != 0; cnt++)
    {
      switch (ptr->ctype)
        {
        case GDA_Connection_EXTRA_INFO :
	  extra_info = TRUE;
	  break;
        case GDA_Connection_OBJECT_NAME :
          if (!and_condition) and_condition = g_string_new("");
          g_string_sprintfa(and_condition, "AND a.oid='%s' ", ptr->value);
          fprintf(stderr, "schema_tables: aggregate name = '%s'\n", 
		  ptr->value);
          break;
        case GDA_Connection_OBJECT_SCHEMA :
          if (!and_condition) and_condition = g_string_new("");
          g_string_sprintfa(and_condition, "AND b.usename='%s' ", ptr->value);
          fprintf(stderr, "schema_tables: table_schema = '%s'\n", ptr->value);
          break;
        case GDA_Connection_OBJECT_CATALOG :
          fprintf(stderr, "schema_procedures: proc_catalog = '%s' UNUSED!\n", 
		  ptr->value);
          break;
        default :
          fprintf(stderr, "schema_tables: invalid constraint type %d\n", 
		  ptr->ctype);
          g_string_free(and_condition, TRUE);
          return (0);
        }
      ptr++;
    }

  /* build the query: no difference between postgres versions. */
  /* 1st: aggregates that work on a particular type */
  query = g_string_new("SELECT a.aggname AS \"Name\", "
		       "a.oid AS \"Object Id\", "
		       "t.typname as \"IN Type\", ");
  if (extra_info) 
    g_string_append(query, "b.usename AS \"Owner\", "
                    "obj_description(a.oid) AS \"Comments\", "
                    "a.oid AS \"SQL\" ");
  else 
    g_string_append(query, "obj_description(a.oid) AS \"Comments\" ");

  g_string_append(query, "FROM pg_aggregate a, pg_type t, pg_user b "
		  "WHERE a.aggbasetype = t.oid AND b.usesysid=a.aggowner ");
  if (and_condition)
    {
      g_string_append(query, and_condition->str);
      g_string_free(and_condition, TRUE);
    }

  /* 2nd: aggregates that work on any type */
  g_string_append(query, " UNION ");
  g_string_append(query, "SELECT a.aggname AS \"Name\", "
		  "a.oid AS \"Object Id\", "
		  "'---' as \"IN Type\", ");
  if (extra_info) 
    g_string_append(query, "b.usename AS \"Owner\", "
                    "obj_description(a.oid) AS \"Comments\", "
                    "a.oid AS \"SQL\" ");
  else 
    g_string_append(query, "obj_description(a.oid) AS \"Comments\" ");

  g_string_append(query, "FROM pg_aggregate a, pg_user b "
		  "WHERE a.aggbasetype = 0 AND b.usesysid=a.aggowner ");
  if (and_condition)
    {
      g_string_append(query, and_condition->str);
      g_string_free(and_condition, TRUE);
    }

  /* then ordering */
  g_string_append(query, "ORDER BY aggname, typname");		  


  /* build the command object */
  cmd = gda_server_command_new(cnc);
  gda_server_command_set_text(cmd, query->str);
  g_string_free(query, TRUE);

  /* execute the command */
  recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
  gda_server_command_free(cmd);
  fprintf(stderr, "Nb of tables: %ld\n", affected);

  if (recset && extra_info)
    {
      POSTGRES_Recordset *prc;
      POSTGRES_Recordset_Replacement *repl;

      prc = (POSTGRES_Recordset *) gda_server_recordset_get_user_data(recset);
      repl = g_new0(POSTGRES_Recordset_Replacement, 1); 
      repl->colnum = 5;
      repl->newtype = GDA_TypeVarchar;
      repl->trans_func = replace_AGGREGATE_OID_with_SQL;
      add_replacement_function(recset, repl, "varchar");
    }
  
  return (recset);
}




static Gda_ServerRecordset *
schema_sequences (Gda_ServerError *error,
		  Gda_ServerConnection *cnc,
		  GDA_Connection_Constraint *constraint,
		  gint length)
{
  GString*                   query;
  Gda_ServerCommand*         cmd;
  Gda_ServerRecordset*       recset = 0;
  GDA_Connection_Constraint* ptr;
  gboolean                   extra_info = FALSE;
  gint                       cnt;
  GString*                   and_condition = 0;
  gulong                     affected;

  /* process constraints */
  ptr = constraint;
  for (cnt = 0; cnt < length && ptr != 0; cnt++)
    {
      switch (ptr->ctype)
        {
        case GDA_Connection_EXTRA_INFO :
          extra_info = TRUE;
          break;
        case GDA_Connection_OBJECT_CATALOG : /* not used in postgres */
	  break;
        case GDA_Connection_OBJECT_NAME :
          if (!and_condition) and_condition = g_string_new("");
          g_string_sprintfa(and_condition, "AND a.relname='%s' ", ptr->value);
          fprintf(stderr, "schema_sequences: seq name = '%s'\n", ptr->value);
          break;
        case GDA_Connection_OBJECT_SCHEMA :
          if (!and_condition) and_condition = g_string_new("");
          g_string_sprintfa(and_condition, "AND b.usename='%s' ", ptr->value);
          fprintf(stderr, "schema_sequences: seq schema = '%s'\n", ptr->value);
          break;
        default :
          fprintf(stderr, "schema_sequences: invalid constraint type %d\n", 
		  ptr->ctype);
          g_string_free(and_condition, TRUE);
          return (0);
        }
      ptr++;
    }

  /* build the query */
  query = g_string_new("SELECT a.relname AS \"Name\", ");
  if (extra_info) 
    g_string_append(query, "b.usename AS \"Owner\", "
                    "obj_description(a.oid) AS \"Comments\", "
                    "a.relname AS \"SQL\" ");
  else 
    g_string_append(query, "obj_description(a.oid) AS \"Comments\" ");
  g_string_append(query, " FROM pg_class a, pg_user b "
                  "WHERE ( relkind = 'S') and relname !~ '^pg_' "
                  "AND relname !~ '^xin[vx][0-9]+' AND "
                  "b.usesysid = a.relowner ");
  if (and_condition) 
    g_string_sprintfa(query, "%s", and_condition->str);
 
  g_string_append(query, " ORDER BY a.relname");
  if (and_condition) g_string_free(and_condition, TRUE);

  /* build the command object */
  cmd = gda_server_command_new(cnc);
  gda_server_command_set_text(cmd, query->str);
  g_string_free(query, TRUE);

  /* execute the command */
  recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
  gda_server_command_free(cmd);
  fprintf(stderr, "Nb of sequences: %ld\n", affected);

  if (recset && extra_info)
    {
      POSTGRES_Recordset *prc;
      POSTGRES_Recordset_Replacement *repl;

      prc = (POSTGRES_Recordset *) gda_server_recordset_get_user_data(recset);
      repl = g_new0(POSTGRES_Recordset_Replacement, 1); 
      repl->colnum = 3;
      repl->newtype = GDA_TypeVarchar;
      repl->trans_func = replace_SEQUENCE_NAME_with_SQL;
      add_replacement_function(recset, repl, "varchar");
    }
  
  return (recset);
}

static Gda_ServerRecordset *
schema_types (Gda_ServerError *error,
              Gda_ServerConnection *cnc,
              GDA_Connection_Constraint *constraint,
              gint length)
{
  gboolean                   extra_info = FALSE;
  gint                       cnt;
  GDA_Connection_Constraint* ptr;
  Gda_ServerCommand*         cmd;
  Gda_ServerRecordset*       recset;
  GString *query=NULL, *and_condition=NULL;
  gulong                     affected;
  POSTGRES_Connection       *pc;

  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);

  /* added 'timez' for V7.0.x */
/*#define IN_PG_TYPES_ALLOWED "('abstime', 'bool', 'box', 'bpchar', 'bytea', 'char', 'cidr', 'circle', 'date', 'datetime', 'filename', 'float4', 'float8', 'inet', 'int2', 'int4', 'int8', 'line', 'lseg', 'macaddr', 'money', 'name', 'numeric', 'path', 'point', 'polygon', 'reltime', 'text', 'time', 'timespan', 'timestamp', 'tinterval', 'varchar', 'timez')"*/


  /* process constraints */
  ptr = constraint;
  for (cnt = 0; cnt < length && ptr != 0; cnt++)
    {
      switch (ptr->ctype)
        {
        case GDA_Connection_OBJECT_NAME :
          if (!and_condition) and_condition = g_string_new("");
          g_string_sprintfa(and_condition, "AND a.typname='%s' ", ptr->value);
          fprintf(stderr, "schema_tables: type name = '%s'\n", ptr->value);
          break;
        case GDA_Connection_OBJECT_SCHEMA :
          if (!and_condition) and_condition = g_string_new("");
          g_string_sprintfa(and_condition, "AND b.usename='%s' ", ptr->value);
          fprintf(stderr, "schema_tables: owner     = '%s'\n", ptr->value);
          break;
        case GDA_Connection_OBJECT_CATALOG : /* not used in postgres */
	  break;
        case GDA_Connection_EXTRA_INFO :
          extra_info = TRUE;
          break;
        default :
          fprintf(stderr, "schema_types: invalid constraint type %d\n", 
		  ptr->ctype);
          return (0);
        }
      ptr++;
    }

  /* build the query */
  if (extra_info)
    {
      query = g_string_new("SELECT a.typname AS \"Name\", b.usename "
			   "AS \"Owner\", "
			   "obj_description(a.oid) AS \"Comments\", "
			   "a.oid AS \"Gda Type\", "
			   "a.oid as \"Local Type\" "
			   "FROM pg_type a, pg_user b WHERE typrelid = 0 "
			   "AND a.typowner=b.usesysid ");
    }
  else
    {
      query = g_string_new("SELECT a.typname AS \"Name\", "
			   "obj_description(a.oid) AS \"Comments\" "
			   "FROM pg_type a, pg_user b "
			   "WHERE typrelid = 0 ");
    }
  if (and_condition)
    {
      g_string_sprintfa(query, "%s", and_condition->str);
      g_string_free(and_condition, TRUE);
    }
  g_string_append(query, "AND typname not in " IN_PG_TYPES_HIDDEN 
		  " AND b.usesysid=a.typowner ");
  
  g_string_append(query, "AND typname !~ '^_.*' ");
 
  g_string_append(query, /*"AND obj_description(a.oid) is not null "*/
		  "ORDER BY typname");

  /* build the command object */
  cmd = gda_server_command_new(cnc);
  gda_server_command_set_text(cmd, query->str);
  g_string_free(query, TRUE);

  /* execute the command */
  recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
  gda_server_command_free(cmd);
  fprintf(stderr, "Nb of types: %ld\n", affected);

  /* completion with gda types if extra_info */
  if (extra_info)
    {
            POSTGRES_Recordset *prc;
      POSTGRES_Recordset_Replacement *repl;

      prc = (POSTGRES_Recordset *) gda_server_recordset_get_user_data(recset);
      repl = g_new0(POSTGRES_Recordset_Replacement, 1); 
      repl->colnum = 3;
      repl->newtype = GDA_TypeSmallint;
      repl->trans_func = replace_PROV_TYPES_with_gdatype;
      add_replacement_function(recset, repl, "int2");
    }

  return (recset);
}


static Gda_ServerRecordset *
schema_views (Gda_ServerError *error,
              Gda_ServerConnection *cnc,
              GDA_Connection_Constraint *constraint,
              gint length)
{
  gchar*                     query = 0;
  gchar*                     view_name = 0;
  gint cnt;
  Gda_ServerCommand*         cmd;
  Gda_ServerRecordset*       recset = 0;
  GDA_Connection_Constraint* ptr;
  gboolean                   extra_info = FALSE;
  gulong                     affected;

  /* process constraints */
  ptr = constraint;
  for (cnt = 0; cnt < length && ptr != 0; cnt++)
    {
      switch (ptr->ctype)
        {
        case GDA_Connection_EXTRA_INFO :
          extra_info = TRUE;
          break;
        case GDA_Connection_OBJECT_NAME :
          view_name = ptr->value;
          fprintf(stderr, "schema_views: view name = '%s'\n", view_name);
          break;
        default :
          fprintf(stderr, "schema_views: invalid constraint type %d\n", ptr->ctype);
          return (0);
        }
      ptr++;
    }

  /* build the query: it is taken from the definition of pg_views and adapted
     because oid can't be got from pg_views. */
  if (extra_info)
    {
      gchar *where_clause;

      if (view_name != 0)
        {
          where_clause = g_strdup_printf("AND c.relname = '%s'", view_name);
        }
      else 
	where_clause = g_strdup(" ");
      query = g_strdup_printf("SELECT c.relname AS \"Name\", "
			      "pg_get_userbyid(c.relowner) AS \"Owner\", "
			      "obj_description(c.oid) as \"Comments\", "
			      "pg_get_viewdef(c.relname) AS \"Definition\" "
			      "FROM pg_class c WHERE (c.relhasrules AND "
			      "(EXISTS (SELECT r.rulename FROM pg_rewrite "
			      "r WHERE ((r.ev_class = c.oid) AND "
			      "(r.ev_type = '1'::\"char\"))))) "
			      "AND c.relname !~ '^pg_' AND "
			      "c.relname !~ '^xin[vx][0-9]+' "
			      "%s ORDER BY c.relname", where_clause);
      g_free((gpointer) where_clause);
    }
  else
    {
      query = g_strdup("SELECT c.relname AS \"Name\", "
		       "obj_description(c.oid) as \"Comments\" "
		       "FROM pg_class c WHERE (c.relhasrules AND "
		       "(EXISTS (SELECT r.rulename FROM pg_rewrite "
		       "r WHERE ((r.ev_class = c.oid) AND "
		       "(r.ev_type = '1'::\"char\"))))) "
		       "AND c.relname !~ '^pg_' AND "
		       "c.relname !~ '^xin[vx][0-9]+' "
		       "ORDER BY c.relname");
      /* This was the old and simple query:
	query = g_strdup("SELECT viewname AS \"Name\", "
	"       null AS \"Comments\" "
	" FROM pg_views "
	" ORDER BY viewname");*/
    }

  /* build the command object */
  cmd = gda_server_command_new(cnc);
  gda_server_command_set_text(cmd, query);
  g_free(query);

  /* execute the command */
  recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
  gda_server_command_free(cmd);
  fprintf(stderr, "Nb of views: %ld\n", affected);

  return (recset);
}


/* 
 * Other functions
 */
static gint
execute_command (Gda_ServerConnection *cnc, gchar *cmd)
{
  POSTGRES_Connection *pc;
  g_return_val_if_fail(cnc != 0, -1);
  g_return_val_if_fail(cmd != 0, -1);

  pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
  if (pc)
    {
      if (PQstatus(pc->pq_conn) == CONNECTION_OK)
	{
	  PGresult *rc = PQexec(pc->pq_conn, cmd);
	  switch (PQresultStatus(rc))
	    {
	    case PGRES_COMMAND_OK :
	    case PGRES_EMPTY_QUERY :
	    case PGRES_TUPLES_OK :
	    case PGRES_COPY_OUT :
	    case PGRES_COPY_IN :
	      PQclear(rc);
	      return (0);
	    case PGRES_BAD_RESPONSE :
	    case PGRES_NONFATAL_ERROR :
	    case PGRES_FATAL_ERROR :
	      break;
	    }
	  if (rc != 0)
	    PQclear(rc);
	  gda_server_error_make(gda_server_error_new(), 0, cnc, __PRETTY_FUNCTION__);
	}
    }
  return (-1);
}


static void
initialize_schema_ops (void)
{
  gint i;

  /* sane init */
  for (i=GDA_Connection_GDCN_SCHEMA_AGGREGATES; i<= GDA_Connection_GDCN_SCHEMA_LAST; 
       i++)
    schema_ops[i] = 0;

  /* what is implemented */
  schema_ops[GDA_Connection_GDCN_SCHEMA_TABLES] = schema_tables;
  schema_ops[GDA_Connection_GDCN_SCHEMA_COLS] = schema_columns; 
  schema_ops[GDA_Connection_GDCN_SCHEMA_PROCS] = schema_procedures; 
  schema_ops[GDA_Connection_GDCN_SCHEMA_PROC_PARAMS] = schema_proc_params; 
  schema_ops[GDA_Connection_GDCN_SCHEMA_AGGREGATES] = schema_aggregates; 
  schema_ops[GDA_Connection_GDCN_SCHEMA_PROV_TYPES] = schema_types; 
  schema_ops[GDA_Connection_GDCN_SCHEMA_VIEWS] = schema_views; 
  schema_ops[GDA_Connection_GDCN_SCHEMA_SEQUENCES] = schema_sequences; 
  schema_ops[GDA_Connection_GDCN_SCHEMA_TAB_PARENTS] = schema_tab_parents; 
}




gulong
gda_postgres_connection_get_sql_type(POSTGRES_Connection *cnc, 
				     gchar *postgres_type)
{
  gulong oid = 0; /* default return value */
  gboolean found;
  gint i, max=0;
  Gda_connection_data *data;

  g_return_val_if_fail((cnc != NULL), 0);
  g_return_val_if_fail((cnc->types_array != NULL), 0);

  data = find_connection_data(cnc);
  if (data)
    max = data->n_types;

  found = FALSE;
  i = 0;
  while ((i< max) && !found) 
    {
      if (!strcmp(cnc->types_array[i].postgres_type, postgres_type)) 
	{
	  oid = cnc->types_array[i].oid;
	  found = TRUE;
	}
      i++;
    }

  return oid;
}

/* returns the connection data or NULL if none */
static Gda_connection_data *find_connection_data(POSTGRES_Connection *cnc)
{
  GSList *list;
  Gda_connection_data *data, *retval = NULL;
  g_return_val_if_fail((cnc != NULL), NULL);

  if (cnc->types_array)
    {
      list = global_connection_data_list;
      while (list && !retval)
	{
	  data = (Gda_connection_data*)(list->data);
	  if (cnc->types_array == data->types_array)
	    retval = data;
	  else
	    list = g_slist_next(list);
	}
    }

  return retval;
}

static gchar* gda_postgres_connection_get_type_name(POSTGRES_Connection *cnc, 
						    gulong oid)
{
  gchar* str=NULL; /* default value */
  gboolean found;
  gint i, max=0;
  Gda_connection_data *data;

  g_return_val_if_fail((cnc != NULL), NULL);
  g_return_val_if_fail((cnc->types_array != NULL), NULL);

  data = find_connection_data(cnc);
  if (data)
    max = data->n_types;

  found = FALSE;
  i = 0;
  while ((i< max) && !found) 
    {
      if (cnc->types_array[i].oid == oid) 
	{
	  str = cnc->types_array[i].postgres_type;
	  found = TRUE;
	}
      i++;
    }

  return str;
}

static gboolean
gda_postgres_connection_is_type_known (POSTGRES_Connection *cnc,
                                       gulong sql_type)
{
  gboolean found;
  gint i, max=0;
  Gda_connection_data *data;

  g_return_val_if_fail((cnc != NULL), FALSE);
  g_return_val_if_fail((cnc->types_array != NULL), FALSE);

  data = find_connection_data(cnc);
  if (data)
    max = data->n_types;

  found = FALSE;

  i = 0;
  while ((i< max) && !found)
    {
      if (cnc->types_array[i].oid == sql_type)
        found = TRUE;
      i++;
    }
  return found;
}


static void add_replacement_function(Gda_ServerRecordset *recset,
				     POSTGRES_Recordset_Replacement *repl,
				     gchar *sql_type)
{
  POSTGRES_Recordset *prc;
  POSTGRES_Connection *pc;
  gulong sqltype;
  Gda_ServerField *field;

  g_return_if_fail(sql_type != NULL);
  g_return_if_fail(repl != NULL);
  g_return_if_fail(recset != NULL);

  pc = (POSTGRES_Connection *)gda_server_connection_get_user_data(recset->cnc);
  prc = (POSTGRES_Recordset *) gda_server_recordset_get_user_data(recset);
  sqltype = gda_postgres_connection_get_sql_type(pc, sql_type);
  g_return_if_fail(sqltype != 0);

  /* add the replacement function in the list */
  prc->replacements = g_slist_append(prc->replacements, repl);

  /* Now set the right sql_type value of the field */
  field = (Gda_ServerField *) g_list_nth_data(recset->fields, repl->colnum);
  gda_server_field_set_sql_type(field, sqltype);
}
