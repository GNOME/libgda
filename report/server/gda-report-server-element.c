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

static PortableServer_ServantBase__epv impl_GDA_Report_Element_base_epv =
{
  NULL,			/* _private data */
  NULL,			/* finalize routine */
  NULL,			/* default_POA routine */
};
static POA_GDA_Report_Element__epv impl_GDA_Report_Element_epv =
{
  NULL,			/* _private */
  (gpointer) & impl_GDA_Report_Element__get_attr_list,
  (gpointer) & impl_GDA_Report_Element__get_contents,
  (gpointer) & impl_GDA_Report_Element_addAttribute,
  (gpointer) & impl_GDA_Report_Element_removeAttribute,
  (gpointer) & impl_GDA_Report_Element_getAttribute,
  (gpointer) & impl_GDA_Report_Element_setAttribute,
  (gpointer) & impl_GDA_Report_Element_addChild,
  (gpointer) & impl_GDA_Report_Element_removeChild,
  (gpointer) & impl_GDA_Report_Element_getChildren,
  (gpointer) & impl_GDA_Report_Element_getName,
  (gpointer) & impl_GDA_Report_Element_setName,
};

static POA_GDA_Report_Element__vepv impl_GDA_Report_Element_vepv =
{
   &impl_GDA_Report_Element_base_epv,
   &impl_GDA_Report_Element_epv,
};

/*
 * Stub implementations
 */
GDA_Report_Element
impl_GDA_Report_Element__create (PortableServer_POA poa,
                                xmlNodePtr xmlparent,
                                const gchar *name,
                                CORBA_Environment * ev)
{
  GDA_Report_Element retval;
  impl_POA_GDA_Report_Element *newservant;
  PortableServer_ObjectId *objid;

  newservant = g_new0(impl_POA_GDA_Report_Element, 1);
  newservant->servant.vepv = &impl_GDA_Report_Element_vepv;
  newservant->poa = poa;
  
  /* create XML node */
  newservant->element_xmlnode = xmlNewChild(xmlparent, NULL, name, NULL);
  
  /* activate CORBA object */
  POA_GDA_Report_Element__init((PortableServer_Servant) newservant, ev);
  objid = PortableServer_POA_activate_object(poa, newservant, ev);
  CORBA_free(objid);
  retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

  return retval;
}

void
impl_GDA_Report_Element__destroy (impl_POA_GDA_Report_Element * servant, CORBA_Environment * ev)
{
  PortableServer_ObjectId *objid;

  g_return_if_fail(servant != NULL);
  
  /* free memory */
  xmlFreeNode(servant->element_xmlnode);
  
  /* deactivate CORBA object */
  objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
  PortableServer_POA_deactivate_object(servant->poa, objid, ev);
  CORBA_free(objid);

  POA_GDA_Report_Element__fini((PortableServer_Servant) servant, ev);
  g_free(servant);
}

GDA_Report_AttributeList *
impl_GDA_Report_Element__get_attr_list (impl_POA_GDA_Report_Element * servant, CORBA_Environment * ev)
{
}

GDA_Report_VE *
impl_GDA_Report_Element__get_contents (impl_POA_GDA_Report_Element * servant, CORBA_Environment * ev)
{
}

void
impl_GDA_Report_Element_addAttribute (impl_POA_GDA_Report_Element * servant,
                                     CORBA_char * name,
                                     CORBA_char * value,
                                     CORBA_Environment * ev)
{
  g_return_if_fail(servant != NULL);
  xmlSetProp(servant->element_xmlnode, (const xmlChar *) name, (const xmlChar *) value);
}

void
impl_GDA_Report_Element_removeAttribute (impl_POA_GDA_Report_Element * servant,
                                        CORBA_char * name,
                                        CORBA_Environment * ev)
{
  g_return_if_fail(servant != NULL);
  /* FIXME: how to remove the XML attribute? */
}

GDA_Report_Attribute *
impl_GDA_Report_Element_getAttribute (impl_POA_GDA_Report_Element * servant,
                                     CORBA_char * name,
                                     CORBA_Environment * ev)
{
  xmlChar* value;
  GDA_Report_Attribute* retval;
  
  g_return_val_if_fail(servant != NULL, NULL);
  
  /* create structure to be returned */
  retval = GDA_Report_Attribute__alloc();
  value = xmlGetProp(servant->element_xmlnode, (const xmlChar *) name);
  if (value)
    {
      retval->name = CORBA_string_dup(name);
      retval->value = CORBA_string_dup((CORBA_char *) value);
    }
  return retval;
}

void
impl_GDA_Report_Element_setAttribute (impl_POA_GDA_Report_Element * servant,
                                     CORBA_char * name,
                                     CORBA_char * value,
                                     CORBA_Environment * ev)
{
  impl_GDA_Report_Element_addAttribute(servant, name, value, ev);
}

GDA_Report_Element
impl_GDA_Report_Element_addChild (impl_POA_GDA_Report_Element * servant,
                                 CORBA_char * name,
                                 CORBA_Environment * ev)
{
  GDA_Report_Element retval;
  
  g_return_val_if_fail(servant != NULL, CORBA_OBJECT_NIL);
  
  retval = impl_GDA_Report_Element__create(servant->poa, servant->element_xmlnode, name, ev);
  gda_corba_handle_exception(ev);
  return retval;
}

void
impl_GDA_Report_Element_removeChild (impl_POA_GDA_Report_Element * servant,
                                    GDA_Report_Element child,
                                    CORBA_Environment * ev)
{
  g_return_if_fail(servant != NULL);
  g_return_if_fail(!CORBA_Object_is_nil(child, ev));
}

GDA_Report_ElementList *
impl_GDA_Report_Element_getChildren (impl_POA_GDA_Report_Element *servant, CORBA_Environment *ev)
{
  g_return_val_if_fail(servant != NULL, NULL);
  return NULL;
}

CORBA_char *
impl_GDA_Report_Element_getName (impl_POA_GDA_Report_Element *servant, CORBA_Environment *ev)
{
  g_return_val_if_fail(servant != NULL, NULL);
  return CORBA_string_dup(servant->element_xmlnode->name);
}

void
impl_GDA_Report_Element_setName (impl_POA_GDA_Report_Element *servant,
                                CORBA_char *name,
                                CORBA_Environment *ev)
{
  g_return_if_fail(servant != NULL);
  xmlNodeSetName(servant->element_xmlnode, (const xmlChar *) name);
}
