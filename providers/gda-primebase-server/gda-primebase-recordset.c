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

gboolean
gda_primebase_update_cell (Gda_ServerField *field, primebase_Recordset *prset,
                           gpointer *data)
{
  return FALSE;

/*
  DALDatePtr      ddate = NULL;
  DALTimePtr      dtime = NULL;
  DALTimeStampPtr dts = NULL;
  GDate           date;
  GTime           time = 0;
  
  g_return_val_if_fail(field != NULL, FALSE);
  g_return_val_if_fail(data != NULL, FALSE);

  switch (gda_server_field_get_sql_type(field)) {
  case A_BOOLEAN:
    break;
  case A_SMINT:
  case A_INTEGER:
    break;
  case A_SMFLOAT:
    break;
  case A_FLOAT:
    break;
  case A_DATE:
    ddate = (DALDatePtr) data;
	 g_date_set_dmy(&date, ddate->day, ddate->month, ddate->year);
    gda_server_field_set_date(field, &date);
    break;
  case A_TIME:
	 dtime = (DALTimePtr) data;
	 time = ((dtime->hour * 60) + dtime->min) * 60 + dtime->sec;
	 gda_server_field_set_time(field, time);
    break;
  case A_TIMESTAMP:
    dts = (DALTimeStampPtr) data;
	 gda_server_field_set_timestamp(field, &date, time);
    break;
  case A_CHAR:
	 gda_server_field_set_char(field, (gchar *) data);
    break;
  case A_DECIMAL:
    break;
  case A_MONEY:
    break;
  case A_VARCHAR:
    break;
  case A_VARBIN:
    break;
  case A_LONGCHAR:
    break;
  case A_LONGBIN:
    break;
  default:
  }

  return TRUE;
*/
}

gboolean
gda_primebase_recordset_new (Gda_ServerRecordset *recset)
{
  primebase_Recordset *precset = NULL;

  g_return_val_if_fail(recset != NULL, FALSE);

  precset = g_new0(primebase_Recordset, 1);
  if (precset) {
    gda_server_recordset_set_user_data(recset, (gpointer) precset);
    return TRUE;
  }
  return FALSE;
}

gint
gda_primebase_recordset_move_next (Gda_ServerRecordset *recset)
{
  primebase_Recordset  *prset = NULL;
  Gda_ServerConnection *cnc  = NULL;
  primebase_Connection *pcnc = NULL;
  Gda_ServerField      *field = NULL;
  gint                 col = 0;

  g_return_val_if_fail(recset != NULL, -1);
  prset = (primebase_Recordset *) gda_server_recordset_get_user_data(recset);
  g_return_val_if_fail(prset != NULL, -1);
  cnc = gda_server_recordset_get_connection(recset);
  g_return_val_if_fail(cnc != NULL, -1);
  pcnc = (primebase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(pcnc != NULL, -1);

  return -1;

/*
  if (!prset->initialized) {
    prset->type = A_ANYTYPE;
    while ((pcnc->state = CLGetItem(pcnc->sid, AW_DEFAULT, &prset->type,
                                    &prset->len, &prset->places,
                                    &prset->flags, NULL
                                   )
           )) {
      if (pcnc->state == A_ERROR) {
        gda_server_recordset_set_at_begin(recset, FALSE);
        gda_server_recordset_set_at_end(recset, TRUE);
		  gda_primebase_free_error(pcnc->err);
        pcnc->err = gda_primebase_get_error(pcnc->sid, TRUE);
		  gda_primebase_free_error(pcnc->err);
        return -1;
		}
		if (pcnc->state == A_READY) {
        gda_server_recordset_set_at_begin(recset, FALSE);
        gda_server_recordset_set_at_end(recset, TRUE);
        return -1;
      }
		prset->col_cnt++;

		// Add a new field
      field = gda_server_field_new();
      gda_server_field_set_name(field, "[0]");
      gda_server_field_set_sql_type(field, prset->type);
      gda_server_field_set_actual_length(field, prset->len);
      gda_server_field_set_defined_length(field, prset->len);
      gda_server_field_set_user_data(field, NULL);
      gda_server_field_set_scale(field, prset->places);
      gda_server_recordset_add_field(recset, field);
		
      prset->type = A_ANYTYPE;
      prset->buffer = g_new0(gchar, prset->len + 1);
		// FIXME: Get items of more then MAX_DALSIZE
      pcnc->state = CLGetItem(pcnc->sid, AW_DEFAULT, &prset->type,
                              &prset->len, &prset->places, &prset->flags,
                              prset->buffer);
      gda_primebase_update_cell(field, prset, (gpointer) prset->buffer);
      g_free((gpointer) prset->buffer);
      // If we are at the end of a row, break
      if (prset->flags & AF_RECEND) {
        break;
      }
    }
	 return 0;
  } else {
    prset->type = A_ANYTYPE;
    while ((pcnc->state = CLGetItem(pcnc->sid, AW_DEFAULT, &prset->type,
                                    &prset->len, &prset->places,
                                    &prset->flags, NULL
                                   )
           )) {
      if (pcnc->state == A_ERROR) {
        gda_server_recordset_set_at_begin(recset, FALSE);
        gda_server_recordset_set_at_end(recset, TRUE);
		  gda_primebase_free_error(pcnc->err);
        pcnc->err = gda_primebase_get_error(pcnc->sid, TRUE);
		  gda_primebase_free_error(pcnc->err);
        return -1;
		}
		if (pcnc->state == A_READY) {
        gda_server_recordset_set_at_begin(recset, FALSE);
        gda_server_recordset_set_at_end(recset, TRUE);
        return -1;
      }

		// Update field
      field = g_list_nth(gda_server_recordset_get_fields(recset), col);
		if (field && (gda_server_field_get_sql_type(field) == prset->type)) {
        prset->type = A_ANYTYPE;
        prset->buffer = g_new0(gchar, prset->len + 1);
        // FIXME: Get items of more then MAX_DALSIZE
        pcnc->state = CLGetItem(pcnc->sid, AW_DEFAULT, &prset->type,
                                &prset->len, &prset->places, &prset->flags,
                                prset->buffer);
        gda_primebase_update_cell(field, prset, (gpointer) prset->buffer);
        g_free((gpointer) prset->buffer);
      } else { // Oops, we got no matching field
        gda_log_message(_("No according field found"));

		  // FIXME: Do not abort the whole operation, but skip getting that field
        CLBreak(pcnc->sid, 0);
		  gda_server_recordset_set_at_begin(recset, FALSE);
        gda_server_recordset_set_at_end(recset, TRUE);
        return -1;
      }
      // If we are at the end of a row, break
		
      if (prset->flags & AF_RECEND) {
        break;
      }

		col++;
    }
	 return 0;
  }
  return -1;
*/
}

gint
gda_primebase_recordset_move_prev (Gda_ServerRecordset *recset)
{
  return -1;
}

gint
gda_primebase_recordset_close (Gda_ServerRecordset *recset)
{
  return -1;
}

void
gda_primebase_recordset_free (Gda_ServerRecordset *recset)
{
  primebase_Recordset *precset = NULL;

  g_return_if_fail(recset != NULL);
  precset = (primebase_Recordset *) gda_server_recordset_get_user_data(recset);
  if (precset) {
    g_free((gpointer) precset);
    gda_server_recordset_set_user_data(recset, (gpointer) NULL);
  }
}


