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

typedef struct
{
  short vary_length;
  char  vary_string[1];
} VARY;

static void
fill_field_values (GdaServerRecordset *recset)
{
  XSQLVAR*             var;
  GList*               node;
  INTERBASE_Recordset* ib_recset;

  g_return_if_fail(recset != NULL);

  /* get GdaServerRecordset associated data */
  ib_recset = (INTERBASE_Recordset *) gda_server_recordset_get_user_data(recset);
  if (ib_recset)
    {
      gint cnt = 0;
      for (node = g_list_first(recset->fields); node != NULL; node = g_list_next(node))
	{
	  GdaServerField* field = (GdaServerField *) node->data;
	  if (field)
	    {
	      /* fill field value with the correct attributes */
	      var = (XSQLVAR *) &ib_recset->sqlda->sqlvar[cnt];
	      if (var)
		{
		  VARY* vary;
		  short dtype = var->sqltype & ~1;

		  if ((var->sqltype & 1) && (*var->sqlind < 0))
		    {
		      /* NULL handling */
		    }
		  else
		    {
		      switch (dtype)
			{
			case SQL_TEXT :
			  gda_server_field_set_actual_length(field, var->sqllen);
			  gda_server_field_set_varchar(field, var->sqldata);
			  break;
			case SQL_VARYING :
			  gda_server_field_set_actual_length(field, var->sqllen);
			  vary = (VARY *) var->sqldata;
			  vary->vary_string[vary->vary_length] = '\0';
			  gda_server_field_set_varchar(field, vary->vary_string);
			  break;
			case SQL_SHORT :
			case SQL_LONG :
			case SQL_INT64 :
			  {
			    ISC_INT64 value;
			    short     field_width;

			    if (dtype == SQL_SHORT)
			      {
				value = (ISC_INT64) *(short *) var->sqldata;
				field_width = 6;
			      }
			    else if (dtype == SQL_LONG)
			      {
				value = (ISC_INT64) *(long *) var->sqldata;
				field_width = 11;
			      }
			    else if (dtype == SQL_INT64)
			      {
				value = (ISC_INT64) *(ISC_INT64 *) var->sqldata;
				field_width = 21;
			      }
			    gda_server_field_set_actual_length(field,
							       field_width + 
							       var->sqlscale > 0 ? var->sqlscale : 0);
			    if (var->sqlscale < 0)
			      {
				ISC_INT64 tens;
				short     i;

				tens = 1;
				for (i = 0; i > var->sqlscale; i--) tens *= 10;
				/* FIXME................ */
			      }
			  }
			case SQL_FLOAT :
			  gda_server_field_set_actual_length(field, 15);
			  gda_server_field_set_single(field, *(float *)(var->sqldata));
			  break;
			case SQL_DOUBLE :
			  gda_server_field_set_actual_length(field, 24);
			  gda_server_field_set_double(field, *(double *)(var->sqldata));
			  break;
			case SQL_TIMESTAMP :
			case SQL_TYPE_DATE :
			case SQL_TYPE_TIME :
			case SQL_BLOB :
			case SQL_ARRAY :
			}
		    }
		}
	    }
	  cnt++;
	}
    }
}

gboolean
gda_interbase_recordset_new (GdaServerRecordset *recset)
{
  INTERBASE_Recordset* ib_recset;

  g_return_val_if_fail(recset != NULL, FALSE);

  ib_recset = g_new0(INTERBASE_Recordset, 1);

  /* allocate sqlda area of default size */
  ib_recset->sqlda = (XSQLDA *) g_malloc0(XSQLDA_LENGTH(3));
  ib_recset->sqlda->sqln = 3;
  ib_recset->sqlda->version = 1;

  gda_server_recordset_set_user_data(recset, (gpointer) ib_recset);
  return TRUE;
}

gint
gda_interbase_recordset_move_next (GdaServerRecordset *recset)
{
  INTERBASE_Recordset*  ib_recset;
  INTERBASE_Connection* ib_cnc;

  g_return_val_if_fail(recset != NULL, -1);
  g_return_val_if_fail(recset->cnc != NULL, -1);

  ib_recset = (INTERBASE_Recordset *) gda_server_recordset_get_user_data(recset);
  if (ib_recset)
    {
      ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(recset->cnc);
      if (ib_cnc)
	{
	  glong fetch_stat = isc_dsql_fetch(ib_cnc->status, &ib_recset->ib_cmd, 1, ib_recset->sqlda);
	  if (fetch_stat == 0)
	    {
	      fill_field_values(recset);
	      gda_server_recordset_set_at_begin(recset, FALSE);
	      gda_server_recordset_set_at_end(recset, FALSE);
	      return 0;
	    }
	  else if (fetch_stat == 100L) /* EOF */
	    {
	      gda_server_recordset_set_at_end(recset, TRUE);
	      return 1;
	    }
	  gda_server_error_make(gda_server_error_new(), recset, recset->cnc, __PRETTY_FUNCTION__);
	}
    }
  return -1;
}

gint
gda_interbase_recordset_move_prev (GdaServerRecordset *recset)
{
  INTERBASE_Recordset* ib_recset;

  g_return_val_if_fail(recset != NULL, -1);
  g_return_val_if_fail(recset->cnc != NULL, -1);

  return -1;
}

gint
gda_interbase_recordset_close (GdaServerRecordset *recset)
{
  return -1;
}

void
gda_interbase_recordset_free (GdaServerRecordset *recset)
{
  INTERBASE_Recordset* ib_recset;

  g_return_if_fail(recset != NULL);

  ib_recset = (INTERBASE_Recordset *) gda_server_recordset_get_user_data(recset);
  if (ib_recset)
    {
      if (ib_recset->sqlda) g_free((gpointer) ib_recset->sqlda);
      g_free((gpointer) ib_recset);
    }
}
