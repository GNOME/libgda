/* gda-xml-storage.c
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gda-xml-storage.h"
#include "gda-object.h"


static void gda_xml_storage_iface_init (gpointer g_class);

GType
gda_xml_storage_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaXmlStorageIface),
			(GBaseInitFunc) gda_xml_storage_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, "GdaXmlStorage", &info, 0);
		g_type_interface_add_prerequisite (type, GDA_TYPE_OBJECT);
	}
	return type;
}


static void
gda_xml_storage_iface_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		initialized = TRUE;
	}
}


/**
 * gda_xml_storage_get_xml_id
 * @iface: an object which implements the #GdaXmlStorage interface
 * 
 * Fetch the xml id string of the object, it's up to the caller to
 * free the string.
 * 
 * Returns: the xml id.
 */
gchar *
gda_xml_storage_get_xml_id (GdaXmlStorage *iface)
{
	g_return_val_if_fail (iface && GDA_IS_XML_STORAGE (iface), NULL);

	if (GDA_XML_STORAGE_GET_IFACE (iface)->get_xml_id)
		return (GDA_XML_STORAGE_GET_IFACE (iface)->get_xml_id) (iface);
	else
		return g_strdup (gda_object_get_id (GDA_OBJECT (iface)));
}

/**
 * gda_xml_storage_save_to_xml
 * @iface: an object which implements the #GdaXmlStorage interface
 * @error: location to store error, or %NULL
 * 
 * Creates a new xmlNodePtr structure and fills it with data representing the
 * object given as argument.
 * 
 * Returns: the new XML node, or NULL if an error occurred.
 */
xmlNodePtr 
gda_xml_storage_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	g_return_val_if_fail (iface && GDA_IS_XML_STORAGE (iface), NULL);

	if (GDA_XML_STORAGE_GET_IFACE (iface)->save_to_xml)
		return (GDA_XML_STORAGE_GET_IFACE (iface)->save_to_xml) (iface, error);
	
	return NULL;
}


/**
 * gda_xml_storage_load_from_xml
 * @iface: an object which implements the #GdaXmlStorage interface
 * @node: an XML node from an XML structure
 * @error: location to store error, or %NULL
 * 
 * Updates the object with data stored in the XML node. The object MUST already
 * exist and be of the correct type before calling this function. 
 * This is a virtual function.
 * 
 * Returns: TRUE if no error occurred.
 */
gboolean 
gda_xml_storage_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	g_return_val_if_fail (iface && GDA_IS_XML_STORAGE (iface), FALSE);

	if (GDA_XML_STORAGE_GET_IFACE (iface)->load_from_xml)
		return (GDA_XML_STORAGE_GET_IFACE (iface)->load_from_xml) (iface, node, error);
	
	return TRUE;
}
