/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gda-report-server.h>

static PortableServer_ServantBase__epv impl_GDA_ReportStream_base_epv =
{
  NULL,			/* _private data */
  NULL,			/* finalize routine */
  NULL,			/* default_POA routine */
};
static POA_GDA_ReportStream__epv impl_GDA_ReportStream_epv =
{
  NULL,			/* _private */
  (gpointer) & impl_GDA_ReportStream_readChunk,
  (gpointer) & impl_GDA_ReportStream_writeChunk,
  (gpointer) & impl_GDA_ReportStream_getLength,
};

static POA_GDA_ReportStream__vepv impl_GDA_ReportStream_vepv =
{
  &impl_GDA_ReportStream_base_epv,
  &impl_GDA_ReportStream_epv,
};

/*
 * Stub implementations
 */
GDA_ReportStream
impl_GDA_ReportStream__create (PortableServer_POA poa, CORBA_Environment * ev)
{
  GDA_ReportStream retval;
  impl_POA_GDA_ReportStream *newservant;
  PortableServer_ObjectId *objid;

  newservant = g_new0(impl_POA_GDA_ReportStream, 1);
  newservant->servant.vepv = &impl_GDA_ReportStream_vepv;
  newservant->poa = poa;
  POA_GDA_ReportStream__init((PortableServer_Servant) newservant, ev);
  objid = PortableServer_POA_activate_object(poa, newservant, ev);
  CORBA_free(objid);
  retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

  return retval;
}

void
impl_GDA_ReportStream__destroy (impl_POA_GDA_ReportStream *servant, CORBA_Environment *ev)
{
  PortableServer_ObjectId *objid;

  g_return_if_fail(servant != NULL);

  /* free memory */
  if (servant->stream_data) g_array_free(servant->stream_data, TRUE);
  
  /* release CORBA object */
  objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
  PortableServer_POA_deactivate_object(servant->poa, objid, ev);
  CORBA_free(objid);

  POA_GDA_ReportStream__fini((PortableServer_Servant) servant, ev);
  g_free(servant);
}

GDA_ReportStreamChunk *
impl_GDA_ReportStream_readChunk (impl_POA_GDA_ReportStream * servant,
                                 CORBA_long start,
                                 CORBA_long size,
                                 CORBA_Environment * ev)
{
  GDA_ReportStreamChunk *retval;
  
  g_return_val_if_fail(servant != NULL, CORBA_OBJECT_NIL);
  
  /* create returned data */
  retval = GDA_ReportStreamChunk__alloc();
  if (servant->stream_data)
    {
      retval->_buffer = g_memdup(servant->stream_data->data, servant->stream_data->len);
      retval->_length = servant->stream_data->len;
    }
  
  return retval;
}

CORBA_long
impl_GDA_ReportStream_writeChunk (impl_POA_GDA_ReportStream * servant,
                                  GDA_ReportStreamChunk * data,
                                  CORBA_long size,
                                  CORBA_Environment * ev)
{
  g_return_val_if_fail(servant != NULL, -1);
  
  /* create the internal array if it's not created yet */
  if (!servant->stream_data) servant->stream_data = g_array_new(FALSE, TRUE, sizeof(CORBA_octet));
  if (data && data->_buffer && data->_length > 0)
    {
      servant->stream_data = g_array_append_vals(servant->stream_data,
                                                 (gconstpointer) data->_buffer,
                                                 data->_length);
    }
  else return 0;
  return size;
}

CORBA_long
impl_GDA_ReportStream_getLength (impl_POA_GDA_ReportStream *servant, CORBA_Environment *ev)
{
  g_return_val_if_fail(servant != NULL, -1);
  return servant->stream_data ? servant->stream_data->len : 0;
}
