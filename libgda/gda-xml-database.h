/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_xml_database_h__)
#  define __gda_xml_database_h__

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-table.h>

G_BEGIN_DECLS

#define GDA_TYPE_XML_DATABASE            (gda_xml_database_get_type())
#define GDA_XML_DATABASE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XML_DATABASE, GdaXmlDatabase))
#define GDA_XML_DATABASE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XML_DATABASE, GdaXmlDatabaseClass))
#define GDA_IS_XML_DATABASE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_XML_DATABASE))
#define GDA_IS_XML_DATABASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_XML_DATABASE))

typedef struct _GdaXmlDatabase        GdaXmlDatabase;
typedef struct _GdaXmlDatabaseClass   GdaXmlDatabaseClass;
typedef struct _GdaXmlDatabasePrivate GdaXmlDatabasePrivate;

struct _GdaXmlDatabase {
	GObject object;
	GdaXmlDatabasePrivate *priv;
};

struct _GdaXmlDatabaseClass {
	GObjectClass parent_class;

	/* signals */
	void (*changed) (GdaXmlDatabase * xmldb);
};

GType           gda_xml_database_get_type (void);

GdaXmlDatabase *gda_xml_database_new (void);
GdaXmlDatabase *gda_xml_database_new_from_uri (const gchar *uri);

void            gda_xml_database_changed (GdaXmlDatabase *xmldb);
void            gda_xml_database_reload (GdaXmlDatabase *xmldb);
gboolean        gda_xml_database_save (GdaXmlDatabase *xmldb, const gchar *uri);

GList          *gda_xml_database_get_tables (GdaXmlDatabase *xmldb);
void            gda_xml_database_free_table_list (GList *list);
GdaTable       *gda_xml_database_new_table (GdaXmlDatabase *xmldb, const gchar *name);
GdaTable       *gda_xml_database_new_table_from_node (GdaXmlDatabase *xmldb,
						      xmlNodePtr xmlnode);

G_END_DECLS

#endif
