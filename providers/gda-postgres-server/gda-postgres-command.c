/* GNOME DB libary
 * Copyright (C) 1999 Rodrigo Moya
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

#include "gda-postgres.h"

static Gda_ServerRecordset *init_recset_fields (Gda_ServerRecordset *recset, 
						POSTGRES_Recordset *prc);

/*
 * Public functions
 */
gboolean
gda_postgres_command_new (Gda_ServerCommand *cmd)
{
  /* no structure POSTGRES_Command because everything inside this struct is held
     in the Gda_ServerCommand object */
  return TRUE;
}

Gda_ServerRecordset *
gda_postgres_command_execute (Gda_ServerCommand *cmd,
			      Gda_ServerError *error,
			      const GDA_CmdParameterSeq *params,
			      gulong *affected,
			      gulong options)
{
  Gda_ServerConnection* cnc;
  POSTGRES_Connection *pc;
  gchar*                cmd_string;
  Gda_ServerRecordset*  recset = NULL;

  cnc = gda_server_command_get_connection(cmd);
  if (cnc)
    {
      pc = (POSTGRES_Connection *) gda_server_connection_get_user_data(cnc);
      if (pc)
	{
	  cmd_string = gda_server_command_get_text(cmd);
	  if (cmd_string)
	    {
	      PGresult *res;
	      POSTGRES_Recordset *prc;
	      
	      /* execute query */
	      res = PQexec(pc->pq_conn, cmd_string);
	      if (res)
		{
		  switch (PQresultStatus(res))
		    {
		    case PGRES_EMPTY_QUERY :
		    case PGRES_TUPLES_OK :
		      recset = gda_server_recordset_new(cnc);
		      prc = (POSTGRES_Recordset *) 
			gda_server_recordset_get_user_data(recset);
		      prc->pq_data = res;
		      gda_server_recordset_set_at_begin(recset, TRUE);
		      gda_server_recordset_set_at_end(recset, FALSE);
		      prc->pos = 0;
		      *affected = PQntuples(prc->pq_data);
		      /*recset->affected = *affected */
		      return (init_recset_fields(recset, prc));
		    case PGRES_COMMAND_OK :
		    case PGRES_COPY_OUT :
		    case PGRES_COPY_IN :
		      recset = gda_server_recordset_new(cnc);
		      prc = (POSTGRES_Recordset *) 
			gda_server_recordset_get_user_data(recset);
		      prc->pq_data = res;
		      gda_server_recordset_set_at_begin(recset, TRUE);
		      gda_server_recordset_set_at_end(recset, TRUE);
		      recset->fields = 0;
		      prc->pos = 0;
		      *affected = 0;
		      return (recset);
		    default :
		      gda_postgres_error_make(error, NULL, cmd->cnc, "PQexec");
		      *affected = 0;
		      return (0);
		    }
		}
	      else
		gda_postgres_error_make(error, NULL, cmd->cnc, "PQexec");  
	    }
	}
    }  
  return (0);
}

Gda_ServerRecordset *
gda_postgres_command_build_recset_with_builtin (Gda_ServerConnection *cnc,
						Gda_Builtin_Result *res,  
						gulong *affected)
{
  Gda_ServerRecordset *recset;
  POSTGRES_Recordset  *prc;

  recset = gda_server_recordset_new(cnc);
  prc = (POSTGRES_Recordset *) 
    gda_server_recordset_get_user_data(recset);
  prc->pq_data = NULL;
  prc->btin_res = res;
  gda_server_recordset_set_at_begin(recset, TRUE);
  gda_server_recordset_set_at_end(recset, FALSE);
  prc->pos = 0;
  *affected = Gda_Builtin_Result_get_nbtuples(res);
  return (init_recset_fields(recset, prc));
}

POSTGRES_Connection *
gda_postgres_cmd_set_connection (POSTGRES_Command *cmd,
                                 POSTGRES_Connection *cnc)
{
  POSTGRES_Connection *rc = cmd->cnc;
  cmd->cnc = cnc;
  return (rc);
}

static Gda_ServerRecordset *
init_recset_fields (Gda_ServerRecordset *recset, POSTGRES_Recordset *prc)
{
  register gint cnt;
  guint ncols;

  if (prc->pq_data)
    ncols = PQnfields(prc->pq_data);
  else
    ncols = Gda_Builtin_Result_get_nbfields(prc->btin_res);
  /* check parameters */
  g_return_val_if_fail(recset != NULL, NULL);
  for (cnt = 0; cnt < ncols; cnt++)
    {
      Gda_ServerField *f = gda_server_field_new();
      if (prc->pq_data)
	{
	  gda_server_field_set_name(f, PQfname(prc->pq_data, cnt));
	  gda_server_field_set_sql_type(f, PQftype(prc->pq_data, cnt));
	}
      else
	{
	  gda_server_field_set_name(f, Gda_Builtin_Result_get_fname
				    (prc->btin_res, cnt));
	  /* The Sql Type will be modified when there is 
	     a replacement function*/
	  gda_server_field_set_sql_type(f, Gda_Builtin_Result_get_ftype(prc->btin_res, cnt));
	}

      f->c_type = 
	gda_postgres_connection_get_c_type_psql(prc->cnc, f->sql_type);
      /* f->nullable can not be set because no way of getting it
	 from the beckend! */
      f->value = NULL;
      f->precision = 0;
      gda_server_field_set_scale(f, 0); /* ???? */

      /* these things will be set when the field is filled */
      /*if (prc->pq_data)
	f->defined_length = f->actual_length = PQfsize(prc->pq_data, cnt);
	else
	f->defined_length = f->actual_length = Gda_Builtin_Result_get_fsize
	(prc->btin_res, cnt);;*/
      /*f->malloced = 0;*/


      /* add field definition to list */
      recset->fields = g_list_append(recset->fields, f);
    }
  return (recset);
}


void
gda_postgres_command_free (Gda_ServerCommand *cmd)
{
}
