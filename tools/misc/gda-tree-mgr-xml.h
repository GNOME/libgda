/*
 * Copyright (C) 2010 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_TREE_MGR_XML_H__
#define __GDA_TREE_MGR_XML_H__

#include <libgda/gda-connection.h>
#include "gda-tree-manager.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_MGR_XML            (gda_tree_mgr_xml_get_type())
#define GDA_TREE_MGR_XML(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TREE_MGR_XML, GdaTreeMgrXml))
#define GDA_TREE_MGR_XML_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TREE_MGR_XML, GdaTreeMgrXmlClass))
#define GDA_IS_TREE_MGR_XML(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_TREE_MGR_XML))
#define GDA_IS_TREE_MGR_XML_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_TREE_MGR_XML))
#define GDA_TREE_MGR_XML_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GDA_TYPE_TREE_MGR_XML, GdaTreeMgrXmlClass))

typedef struct _GdaTreeMgrXml GdaTreeMgrXml;
typedef struct _GdaTreeMgrXmlPriv GdaTreeMgrXmlPriv;
typedef struct _GdaTreeMgrXmlClass GdaTreeMgrXmlClass;

struct _GdaTreeMgrXml {
	GdaTreeManager        object;
	GdaTreeMgrXmlPriv *priv;
};

struct _GdaTreeMgrXmlClass {
	GdaTreeManagerClass   object_class;
};

GType              gda_tree_mgr_xml_get_type  (void) G_GNUC_CONST;
GdaTreeManager*    gda_tree_mgr_xml_new       (xmlNodePtr rootnode, const gchar *xml_attributes);

G_END_DECLS

#endif
