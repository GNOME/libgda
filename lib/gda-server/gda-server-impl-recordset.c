/* GNOME DB Server Library
 * Copyright (C) 2000, Rodrigo Moya <rmoya@chez.com>
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

#include "gda-server-impl.h"

/*
 * epv structures
 */

static PortableServer_ServantBase__epv impl_GDA_Recordset_base_epv =
{
  NULL,                        /* _private data */
  (gpointer) & impl_GDA_Recordset__destroy,    /* finalize routine */
  NULL,                        /* default_POA routine */
};

static POA_GDA_Recordset__epv impl_GDA_Recordset_epv =
{
  NULL,                        /* _private */
  (gpointer) & impl_GDA_Recordset__get_currentBookmark,
  (gpointer) & impl_GDA_Recordset__set_currentBookmark,
  (gpointer) & impl_GDA_Recordset__get_cachesize,
  (gpointer) & impl_GDA_Recordset__set_cachesize,
  (gpointer) & impl_GDA_Recordset__get_currentCursorType,
  (gpointer) & impl_GDA_Recordset__set_currentCursorType,
  (gpointer) & impl_GDA_Recordset__get_lockingMode,
  (gpointer) & impl_GDA_Recordset__set_lockingMode,
  (gpointer) & impl_GDA_Recordset__get_maxrecords,
  (gpointer) & impl_GDA_Recordset__set_maxrecords,
  (gpointer) & impl_GDA_Recordset__get_pagecount,
  (gpointer) & impl_GDA_Recordset__get_pagesize,
  (gpointer) & impl_GDA_Recordset__set_pagesize,
  (gpointer) & impl_GDA_Recordset__get_recCount,
  (gpointer) & impl_GDA_Recordset__get_source,
  (gpointer) & impl_GDA_Recordset__get_status,
  (gpointer) & impl_GDA_Recordset_cancelBatch,
  (gpointer) & impl_GDA_Recordset_cancelUpdate,
  (gpointer) & impl_GDA_Recordset_close,
  (gpointer) & impl_GDA_Recordset_deleteCurrent,
  (gpointer) & impl_GDA_Recordset_move,
  (gpointer) & impl_GDA_Recordset_moveFirst,
  (gpointer) & impl_GDA_Recordset_moveLast,
  (gpointer) & impl_GDA_Recordset_reQuery,
  (gpointer) & impl_GDA_Recordset_reSync,
  (gpointer) & impl_GDA_Recordset_supports,
  (gpointer) & impl_GDA_Recordset_update,
  (gpointer) & impl_GDA_Recordset_updateBatch,
  (gpointer) & impl_GDA_Recordset_fetch,
  (gpointer) & impl_GDA_Recordset_describe,
};

/*
 * vepv structures
 */

static POA_GDA_Recordset__vepv impl_GDA_Recordset_vepv =
{
  &impl_GDA_Recordset_base_epv,
  &impl_GDA_Recordset_epv,
};

/*
 * Stub implementations
 */
GDA_Recordset
impl_GDA_Recordset__create (PortableServer_POA poa,
			    Gda_ServerRecordset *recset,
			    CORBA_Environment *ev)
{
  GDA_Recordset retval;
   impl_POA_GDA_Recordset *newservant;
   PortableServer_ObjectId *objid;

   newservant = g_new0(impl_POA_GDA_Recordset, 1);
   newservant->servant.vepv = &impl_GDA_Recordset_vepv;
   newservant->poa = poa;
   newservant->recset = recset;
   POA_GDA_Recordset__init((PortableServer_Servant) newservant, ev);
   objid = PortableServer_POA_activate_object(poa, newservant, ev);
   CORBA_free(objid);
   retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

   return retval;
}

/* You shouldn't call this routine directly without first deactivating the servant... */
void
impl_GDA_Recordset__destroy (impl_POA_GDA_Recordset * servant, CORBA_Environment * ev)
{
  POA_GDA_Recordset__fini((PortableServer_Servant) servant, ev);
  g_free(servant);
}

CORBA_long
impl_GDA_Recordset__get_currentBookmark (impl_POA_GDA_Recordset * servant,
					 CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

void
impl_GDA_Recordset__set_currentBookmark (impl_POA_GDA_Recordset * servant,
					 CORBA_long value,
					 CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
}

CORBA_long
impl_GDA_Recordset__get_cachesize (impl_POA_GDA_Recordset * servant,
				   CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

void
impl_GDA_Recordset__set_cachesize (impl_POA_GDA_Recordset * servant,
				   CORBA_long value,
				   CORBA_Environment * ev)
{
}

GDA_CursorType
impl_GDA_Recordset__get_currentCursorType (impl_POA_GDA_Recordset * servant,
					   CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

void
impl_GDA_Recordset__set_currentCursorType (impl_POA_GDA_Recordset * servant,
					   GDA_CursorType value,
					   CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
}

GDA_LockType
impl_GDA_Recordset__get_lockingMode (impl_POA_GDA_Recordset * servant,
				     CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

void
impl_GDA_Recordset__set_lockingMode (impl_POA_GDA_Recordset * servant,
				     GDA_LockType value,
				     CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
}

CORBA_long
impl_GDA_Recordset__get_maxrecords (impl_POA_GDA_Recordset * servant,
				    CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

void
impl_GDA_Recordset__set_maxrecords (impl_POA_GDA_Recordset * servant,
				    CORBA_long value,
				    CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
}

CORBA_long
impl_GDA_Recordset__get_pagecount (impl_POA_GDA_Recordset * servant,
				   CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset__get_pagesize (impl_POA_GDA_Recordset * servant,
				  CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

void
impl_GDA_Recordset__set_pagesize (impl_POA_GDA_Recordset * servant,
				  CORBA_long value,
				  CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
}

CORBA_long
impl_GDA_Recordset__get_recCount (impl_POA_GDA_Recordset * servant,
				  CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_char *
impl_GDA_Recordset__get_source (impl_POA_GDA_Recordset * servant,
				CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset__get_status (impl_POA_GDA_Recordset * servant,
				CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset_cancelBatch (impl_POA_GDA_Recordset * servant,
				CORBA_Environment * ev)
{
  gda_log_error(_("%s: not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset_cancelUpdate (impl_POA_GDA_Recordset * servant,
				 CORBA_Environment * ev)
{
  gda_log_error(_("%s: not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset_close (impl_POA_GDA_Recordset * servant,
			  CORBA_Environment * ev)
{
  g_return_val_if_fail(CORBA_Object_is_nil(servant, ev), -1);

  gda_server_recordset_close(servant->recset);
  gda_server_recordset_free(servant->recset);
  PortableServer_POA_deactivate_object(servant->poa,
                                       PortableServer_POA_servant_to_id(servant->poa,servant, ev),
                                       ev);
  gda_server_impl_exception(ev);
  impl_GDA_Recordset__destroy(servant, ev);
  if (gda_server_impl_exception(ev) < 0)
    return -1;
  return 0;
}

CORBA_long
impl_GDA_Recordset_deleteCurrent (impl_POA_GDA_Recordset * servant,
				  CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset_move (impl_POA_GDA_Recordset * servant,
			 CORBA_long count,
			 CORBA_long bookmark,
			 CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset_moveFirst (impl_POA_GDA_Recordset * servant,
			      CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset_moveLast (impl_POA_GDA_Recordset * servant,
			     CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset_reQuery (impl_POA_GDA_Recordset * servant,
			    CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset_reSync (impl_POA_GDA_Recordset * servant,
			   CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_boolean
impl_GDA_Recordset_supports (impl_POA_GDA_Recordset * servant,
			     GDA_Option what,
			     CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset_update (impl_POA_GDA_Recordset * servant,
			   CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

CORBA_long
impl_GDA_Recordset_updateBatch (impl_POA_GDA_Recordset * servant,
				CORBA_Environment * ev)
{
  gda_log_error(_("%s not implemented"), __PRETTY_FUNCTION__);
  return 0;
}

GDA_Recordset_Chunk *
impl_GDA_Recordset_fetch (impl_POA_GDA_Recordset * servant,
			  CORBA_long count,
			  CORBA_Environment * ev)
{
  Gda_ServerRecordset* rs = servant->recset;
  GDA_Row*                 row;
  GDA_Recordset_Chunk*     chunk;
  GDA_Field*               field;
  Gda_ServerField*         server_field;
  GList*                   ptr;
  gint                     rowidx = 0;
  gint                     colidx;
  gint                     rc;
  GList*                   tmp_rows;
  gint                     rowlength;

  if (!servant->recset)
    return CORBA_OBJECT_NIL;

  tmp_rows = 0;
  rowidx = 0;
  rowlength = g_list_length(rs->fields);

  chunk = GDA_Recordset_Chunk__alloc();
  chunk->_buffer = 0;
  chunk->_length = 0;

  if (0 == rowlength)
    return (chunk);

  do
    {
      row = g_new0(GDA_Row, 1);
      row->_buffer = CORBA_sequence_GDA_Field_allocbuf(rowlength);
      row->_length = rowlength;
      ptr = rs->fields;
      
      colidx = 0;

      while (ptr)
	{
	  server_field = ptr->data;
	  server_field->value = &row->_buffer[colidx].realValue._u.v;
	  row->_buffer[colidx].realValue._d = 1;
	  row->_buffer[colidx].shadowValue._d = 1;
	  row->_buffer[colidx].originalValue._d = 1;
	  ptr = g_list_next(ptr);
	  colidx++;
	}

      rc = gda_server_recordset_move_next(rs);

      if (rc != 0)
	{
	  CORBA_free(row->_buffer);
	  g_free(row);
	  break;
	}
      tmp_rows = g_list_append(tmp_rows, row);
      
      ptr = rs->fields;
      field = &row->_buffer[0];
      colidx = 0;
    
      while(ptr)
	{
	  server_field = ptr->data;
	  
	  field->actualSize = server_field->actual_length;
	  field->realValue._d = server_field->actual_length == 0;
	  field->shadowValue._d   = 1;
	  field->originalValue._d = 1;
	  ptr = g_list_next(ptr);
	  field = &row->_buffer[++colidx];
	}
      rowidx++;
    } while (rowidx < count);
  gda_log_message("%s: count = %d, rowidx = %d, fetchresult = %d\n",
		  __PRETTY_FUNCTION__,
		  count,
		  rowidx,
		  rc);

  if (rc < 0)
    {
      GDA_DriverError*          exception = GDA_DriverError__alloc();
      Gda_ServerConnection* cnc = rs->cnc;

      gda_log_error(_("%s: an error ocurred while fetching data"), __PRETTY_FUNCTION__);
      exception->errors._length = g_list_length(cnc->errors);
      exception->errors._buffer = gda_server_impl_make_error_buffer(cnc);
      exception->realcommand = CORBA_string_dup("Fetch");
      CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GDA_DriverError, exception);
      return chunk;
    }

  if (rowidx)
    {
      chunk->_buffer = CORBA_sequence_GDA_Row_allocbuf(rowidx);
    }
  chunk->_length = rowidx;
  for (ptr = tmp_rows, count = 0; count < rowidx; ptr = g_list_next(ptr), count++)
    {
      row = ptr->data;
      chunk->_buffer[count]._length = row->_length;
      chunk->_buffer[count]._buffer = row->_buffer;
      g_free(row);
    }
  g_list_free(tmp_rows);
  return chunk;
}

GDA_RowAttributes *
impl_GDA_Recordset_describe (impl_POA_GDA_Recordset * servant,
			     CORBA_Environment * ev)
{
  Gda_ServerRecordset* rs = servant->recset;
  GDA_RowAttributes*       rc;
  GList*                   ptr;
  Gda_ServerField*     server_field;
  GDA_FieldAttributes*     field;
  gint                     idx;

  g_return_val_if_fail(rs != NULL, NULL);

  rc = GDA_RowAttributes__alloc();
  rc->_length = g_list_length(rs->fields);
  gda_log_message(_("%s: length = %d"), __PRETTY_FUNCTION__, rc->_length);
  rc->_buffer = CORBA_sequence_GDA_FieldAttributes_allocbuf(rc->_length);
  rc->_maximum = 0;

  idx = 0;
  ptr = rs->fields;
  while (ptr)
    {
      field = &rc->_buffer[idx];
      
      server_field = (Gda_ServerField *)(ptr->data);

      field->name = CORBA_string_dup(server_field->name);
      gda_log_message(_("%s: sending field description for '%s'"),
		      __PRETTY_FUNCTION__,
		      field->name);
      field->definedSize = server_field->defined_length;
      field->scale = server_field->num_scale;
      field->gdaType = gda_server_connection_get_gda_type(rs->cnc,
							       server_field->sql_type);
      field->nativeType = server_field->sql_type;
      field->cType = gda_server_connection_get_c_type(rs->cnc, field->gdaType);
      ptr = g_list_next(ptr);
      idx++;
    }
  return rc;
}
