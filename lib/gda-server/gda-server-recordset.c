/* GNOME DB Server Library
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

#include "config.h"
#include "gda-server-impl.h"

/**
 * gda_server_recordset_new
 */
Gda_ServerRecordset *
gda_server_recordset_new (Gda_ServerConnection *cnc)
{
  Gda_ServerRecordset* recset;

  g_return_val_if_fail(cnc != NULL, NULL);

  recset = g_new0(Gda_ServerRecordset, 1);
  recset->cnc = cnc;
  recset->fields = NULL;
  recset->position = -1;
  recset->at_begin = FALSE;
  recset->at_end = FALSE;
  recset->users = 1;
  
  if ((recset->cnc->server_impl != NULL) &&
      (recset->cnc->server_impl->functions.recordset_new != NULL))
    {
      recset->cnc->server_impl->functions.recordset_new(recset);
    }
  return recset;
}

/**
 * gda_server_recordset_get_connection
 */
Gda_ServerConnection *
gda_server_recordset_get_connection (Gda_ServerRecordset *recset)
{
  g_return_val_if_fail(recset != NULL, NULL);
  return recset->cnc;
}

/**
 * gda_server_recordset_add_field
 */
void
gda_server_recordset_add_field (Gda_ServerRecordset *recset, Gda_ServerField *field)
{
  g_return_if_fail(recset != NULL);
  g_return_if_fail(field != NULL);

  recset->fields = g_list_append(recset->fields, (gpointer) field);
}

/**
 * gda_server_recordset_get_fields
 */
GList *
gda_server_recordset_get_fields (Gda_ServerRecordset *recset)
{
  g_return_val_if_fail(recset != NULL, NULL);
  return recset->fields;
}

/**
 * gda_server_recordset_is_at_begin
 */
gboolean
gda_server_recordset_is_at_begin (Gda_ServerRecordset *recset)
{
  g_return_val_if_fail(recset != NULL, FALSE);
  return recset->at_begin;
}

/**
 * gda_server_recordset_set_at_begin
 */
void
gda_server_recordset_set_at_begin (Gda_ServerRecordset *recset, gboolean at_begin)
{
  g_return_if_fail(recset != NULL);
  recset->at_begin = at_begin;
}

/**
 * gda_server_recordset_is_at_end
 */
gboolean
gda_server_recordset_is_at_end (Gda_ServerRecordset *recset)
{
  g_return_val_if_fail(recset != NULL, FALSE);
  return recset->at_end;
}

/**
 * gda_server_recordset_set_at_end
 */
void
gda_server_recordset_set_at_end (Gda_ServerRecordset *recset, gboolean at_end)
{
  g_return_if_fail(recset != NULL);
  recset->at_end = at_end;
}

/**
 * gda_server_recordset_get_user_data
 */
gpointer
gda_server_recordset_get_user_data (Gda_ServerRecordset *recset)
{
  g_return_val_if_fail(recset != NULL, NULL);
  return recset->user_data;
}

/**
 * gda_server_recordset_set_user_data
 */
void
gda_server_recordset_set_user_data (Gda_ServerRecordset *recset, gpointer user_data)
{
  g_return_if_fail(recset != NULL);
  recset->user_data = user_data;
}

/**
 * gda_server_recordset_free
 */
void
gda_server_recordset_free (Gda_ServerRecordset *recset)
{
  g_return_if_fail(recset != NULL);

  if ((recset->cnc != NULL) &&
      (recset->cnc->server_impl != NULL) &&
      (recset->cnc->server_impl->functions.recordset_free != NULL))
    {
      recset->cnc->server_impl->functions.recordset_free(recset);
    }

  recset->users--;
  if (!recset->users)
    {
      g_list_foreach(recset->fields, (GFunc) gda_server_field_free, NULL);
      g_free((gpointer) recset);
    }
}

/**
 * gda_server_recordset_move_next
 */
gint
gda_server_recordset_move_next (Gda_ServerRecordset *recset)
{
  g_return_val_if_fail(recset != NULL, -1);
  g_return_val_if_fail(recset->cnc != NULL, -1);
  g_return_val_if_fail(recset->cnc->server_impl != NULL, -1);
  g_return_val_if_fail(recset->cnc->server_impl->functions.recordset_move_next != NULL, -1);

  return recset->cnc->server_impl->functions.recordset_move_next(recset);
}

/**
 * gda_server_recordset_move_prev
 */
gint
gda_server_recordset_move_prev (Gda_ServerRecordset *recset)
{
  g_return_val_if_fail(recset != NULL, -1);
  g_return_val_if_fail(recset->cnc != NULL, -1);
  g_return_val_if_fail(recset->cnc->server_impl != NULL, -1);
  g_return_val_if_fail(recset->cnc->server_impl->functions.recordset_move_prev != NULL, -1);

  return recset->cnc->server_impl->functions.recordset_move_prev(recset);
}

/**
 * gda_server_recordset_close
 */
gint
gda_server_recordset_close (Gda_ServerRecordset *recset)
{
  g_return_val_if_fail(recset != NULL, -1);
  g_return_val_if_fail(recset->cnc != NULL, -1);
  g_return_val_if_fail(recset->cnc->server_impl != NULL, -1);
  g_return_val_if_fail(recset->cnc->server_impl->functions.recordset_close != NULL, -1);

  return recset->cnc->server_impl->functions.recordset_close(recset);
}
