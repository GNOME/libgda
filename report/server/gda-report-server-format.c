/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya
 
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

static PortableServer_ServantBase__epv impl_GDA_ReportFormat_base_epv =
{
  NULL,			/* _private data */
  NULL,			/* finalize routine */
  NULL,			/* default_POA routine */
};
static POA_GDA_ReportFormat__epv impl_GDA_ReportFormat_epv =
{
  NULL,			/* _private */
  (gpointer) & impl_GDA_ReportFormat_getStructure,
  (gpointer) & impl_GDA_ReportFormat_getStream,
};
static POA_GDA_ReportFormat__vepv impl_GDA_ReportFormat_vepv =
{
  &impl_GDA_ReportFormat_base_epv,
  &impl_GDA_ReportFormat_epv,
};

/*
 * Stub implementations
 */
GDA_ReportFormat
impl_GDA_ReportFormat__create (PortableServer_POA poa, CORBA_Environment * ev)
{
  GDA_ReportFormat retval;
  impl_POA_GDA_ReportFormat *newservant;
  PortableServer_ObjectId *objid;

  newservant = g_new0(impl_POA_GDA_ReportFormat, 1);
  newservant->servant.vepv = &impl_GDA_ReportFormat_vepv;
  newservant->poa = poa;
  POA_GDA_ReportFormat__init((PortableServer_Servant) newservant, ev);
  objid = PortableServer_POA_activate_object(poa, newservant, ev);
  CORBA_free(objid);
  retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

  return retval;
}

void
impl_GDA_ReportFormat__destroy (impl_POA_GDA_ReportFormat *servant, CORBA_Environment *ev)
{
  PortableServer_ObjectId *objid;

  objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
  PortableServer_POA_deactivate_object(servant->poa, objid, ev);
  CORBA_free(objid);

  POA_GDA_ReportFormat__fini((PortableServer_Servant) servant, ev);
  g_free(servant);
}

GDA_ReportElementList *
impl_GDA_ReportFormat_getStructure (impl_POA_GDA_ReportFormat *servant, CORBA_Environment *ev)
{
}

GDA_ReportStream
impl_GDA_ReportFormat_getStream (impl_POA_GDA_ReportFormat *servant, CORBA_Environment *ev)
{
}
