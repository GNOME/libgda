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

static PortableServer_ServantBase__epv impl_GDA_Report_Report_base_epv =
{
  NULL,			/* _private data */
  NULL,			/* finalize routine */
  NULL,			/* default_POA routine */
};
static POA_GDA_Report_Report__epv impl_GDA_Report_Report_epv =
{
  NULL,			/* _private */
  (gpointer) & impl_GDA_Report_Report__get_name,
  (gpointer) & impl_GDA_Report_Report__set_name,
  (gpointer) & impl_GDA_Report_Report__get_description,
  (gpointer) & impl_GDA_Report_Report__set_description,
  (gpointer) & impl_GDA_Report_Report__get_format,
  (gpointer) & impl_GDA_Report_Report__get_isLocked,
  (gpointer) & impl_GDA_Report_Report_run,
  (gpointer) & impl_GDA_Report_Report_lock,
  (gpointer) & impl_GDA_Report_Report_unlock,
};
static POA_GDA_Report_Report__vepv impl_GDA_Report_Report_vepv =
{
  &impl_GDA_Report_Report_base_epv,
  &impl_GDA_Report_Report_epv,
};

/*
 * Stub implementations
 */
GDA_Report_Report
impl_GDA_Report_Report__create (PortableServer_POA poa, CORBA_Environment * ev)
{
  GDA_Report_Report retval;
  impl_POA_GDA_Report_Report *newservant;
  PortableServer_ObjectId *objid;

  newservant = g_new0(impl_POA_GDA_Report_Report, 1);
  newservant->servant.vepv = &impl_GDA_Report_Report_vepv;
  newservant->poa = poa;
  POA_GDA_Report_Report__init((PortableServer_Servant) newservant, ev);
  objid = PortableServer_POA_activate_object(poa, newservant, ev);
  CORBA_free(objid);
  retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

  return retval;
}

void
impl_GDA_Report_Report__destroy (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev)
{
  PortableServer_ObjectId *objid;

  objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
  PortableServer_POA_deactivate_object(servant->poa, objid, ev);
  CORBA_free(objid);

  POA_GDA_Report_Report__fini((PortableServer_Servant) servant, ev);
  g_free(servant);
}

CORBA_char *
impl_GDA_Report_Report__get_name (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev)
{
  return CORBA_string_dup(servant->attr_name);
}

void
impl_GDA_Report_Report__set_name(impl_POA_GDA_Report_Report * servant, CORBA_char * value, CORBA_Environment * ev)
{
}

CORBA_char *
impl_GDA_Report_Report__get_description (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev)
{
}

void
impl_GDA_Report_Report__set_description (impl_POA_GDA_Report_Report * servant, CORBA_char * value, CORBA_Environment * ev)
{
}

GDA_Report_Format
impl_GDA_Report_Report__get_format (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev)
{
}

CORBA_boolean
impl_GDA_Report_Report__get_isLocked (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev)
{
}

GDA_Report_Output
impl_GDA_Report_Report_run (impl_POA_GDA_Report_Report * servant,
			    GDA_Report_ParamList * params,
			    CORBA_long flags,
			    CORBA_Environment * ev)
{
}

void
impl_GDA_Report_Report_lock (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev)
{
}

void
impl_GDA_Report_Report_unlock (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev)
{
}
