/* GNOME DB MsAccess Provider
 * Copyright (C) 2000 Alvaro del Castillo
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

#include "gda-mdb.h"

/*
 * Private functions
 */
void
gda_mdb_init_recset_fields (GdaServerRecordset *recset, Mdb_Recordset *mdb_recset)
{
  gint max_fieldidx;
  gint fieldidx;

  g_return_if_fail(recset != NULL);
  g_return_if_fail(mdb_recset != NULL);
  g_return_if_fail(mdb_recset->mdb_res != NULL);

  max_fieldidx = mdb_num_fields(mdb_recset->mysql_res);
  for (fieldidx = 0; fieldidx < max_fieldidx; fieldidx++)
    {
      MYSQL_FIELD*     mdb_field;
      GdaServerField* field;

      mdb_field = mysql_fetch_field(mdb_recset->mysql_res);
      field = gda_server_field_new();
      gda_server_field_set_name(field, mdb_field->name);
      gda_server_field_set_sql_type(field, mdb_field->type);
      gda_server_field_set_scale(field, mdb_field->decimals);
      gda_server_field_set_actual_length(field, mdb_field->length);
      gda_server_field_set_defined_length(field, mdb_field->max_length);
      gda_server_field_set_user_data(field, (gpointer) mdb_field);

      gda_server_recordset_add_field(recset, field);
    }
}

/*
 * Public functions
 */
gboolean
gda_mdb_command_new (GdaServerCommand *cmd)
{
  return TRUE;
}

GdaServerRecordset *
gda_mdb_command_execute (GdaServerCommand *cmd,
			   GdaServerError *error,
			   const GDA_CmdParameterSeq *params,
			   gulong *affected,
			   gulong options)
{
  gint                  rc;
  gchar*                cmd_string;
  GdaServerConnection* cnc;
  GdaServerRecordset*  recset = NULL;
  Mdb_Connection*     mdb_cnc;

  cnc = gda_server_command_get_connection(cmd);
  if (cnc)
    {
      mdb_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
      if (mdb_cnc)
	{
	  cmd_string = gda_server_command_get_text(cmd);
	  //cmd_string = splice_parameters(gda_server_command_get_text(cmd), params, cnc);
	  if (cmd_string)
	    {
	      /* execute command */
	      gda_log_message(_("executing command '%s'"), cmd_string);
	      rc = mdb_real_query(mysql_cnc->mysql, cmd_string, strlen(cmd_string));
	      gda_log_message(_("info '%s'"), mdb_info(mysql_cnc->mysql));
	      if (rc == 0)
		{
		  MYSQL_Recordset* mdb_recset;

		  /* create the recordset to be returned */
		  recset = gda_server_recordset_new(cnc);
		  mdb_recset = (MYSQL_Recordset *) gda_server_recordset_get_user_data(recset);
		  if (mdb_recset)
		    {
		      mdb_recset->mdb_res = mysql_store_result(mysql_cnc->mysql);
		      gda_mdb_init_recset_fields(recset, mdb_recset);
		    }
		}
	      else gda_server_error_make(error, 0, cnc, __PRETTY_FUNCTION__);
	    }
	}
    }

  return recset;
}

void
gda_mdb_command_free (GdaServerCommand *cmd)
{
}
