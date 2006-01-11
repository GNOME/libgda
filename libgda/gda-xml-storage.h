/* gda-xml-storage.h
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef __GDA_XML_STORAGE_H_
#define __GDA_XML_STORAGE_H_

#include <glib-object.h>
#include <libxml/tree.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_XML_STORAGE          (gda_xml_storage_get_type())
#define GDA_XML_STORAGE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XML_STORAGE, GdaXmlStorage)
#define GDA_IS_XML_STORAGE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_XML_STORAGE)
#define GDA_XML_STORAGE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDA_TYPE_XML_STORAGE, GdaXmlStorageIface))

/* struct for the interface */
struct _GdaXmlStorageIface
{
	GTypeInterface           g_iface;

	/* virtual table */
	gchar      *(* get_xml_id)      (GdaXmlStorage *iface);
	xmlNodePtr  (* save_to_xml)     (GdaXmlStorage *iface, GError **error);
	gboolean    (* load_from_xml)   (GdaXmlStorage *iface, xmlNodePtr node, GError **error);
};

GType           gda_xml_storage_get_type        (void) G_GNUC_CONST;

gchar          *gda_xml_storage_get_xml_id      (GdaXmlStorage *iface);
xmlNodePtr      gda_xml_storage_save_to_xml     (GdaXmlStorage *iface, GError **error);
gboolean        gda_xml_storage_load_from_xml   (GdaXmlStorage *iface, xmlNodePtr node, GError **error);


G_END_DECLS

#endif
