/* GDA library
 * Copyright (C) 1999, 2000 Rodrigo Moya
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

#include <libgda/gda-xml-document.h>

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
	GdaXmlDocument document;
	GdaXmlDatabasePrivate *priv;
};

struct _GdaXmlDatabaseClass {
	GdaXmlDocumentClass parent_class;

	/* signals */
	void (*changed) (GdaXmlDatabase * xmldb);
};

GType           gda_xml_database_get_type (void);

GdaXmlDatabase *gda_xml_database_new (void);
GdaXmlDatabase *gda_xml_database_new_from_file (const gchar *filename);
void            gda_xml_database_free (GdaXmlDatabase * xmldb);

gboolean        gda_xml_database_save (GdaXmlDatabase * xmldb,
				       const gchar * filename);
void            gda_xml_database_changed (GdaXmlDatabase * xmldb);

/*
 * Table management
 */
typedef xmlNode GdaXmlDatabaseTable;

GList               *gda_xml_database_get_tables (GdaXmlDatabase *xmldb);
GdaXmlDatabaseTable *gda_xml_database_table_new (GdaXmlDatabase *xmldb, const gchar *name);
void                 gda_xml_database_table_remove (GdaXmlDatabase *xmldb,
						    GdaXmlDatabaseTable *table);
GdaXmlDatabaseTable *gda_xml_database_table_find (GdaXmlDatabase *xmldb,
						  const gchar *name);
const gchar         *gda_xml_database_table_get_name (GdaXmlDatabase * xmldb,
						      GdaXmlDatabaseTable *table);
void                 gda_xml_database_table_set_name (GdaXmlDatabase *xmldb,
						      GdaXmlDatabaseTable *table,
						      const gchar *name);
const gchar         *gda_xml_database_table_get_owner (GdaXmlDatabase *xmldb,
						       GdaXmlDatabaseTable *table);
void                 gda_xml_database_table_set_owner (GdaXmlDatabase *xmldb,
						       GdaXmlDatabaseTable *table,
						       const gchar *owner);

/*
 * Table field management
 */
typedef xmlNode GdaXmlDatabaseField;

gint                 gda_xml_database_table_field_count (GdaXmlDatabase *xmldb,
							 GdaXmlDatabaseTable *table);
GdaXmlDatabaseField *gda_xml_database_table_add_field (GdaXmlDatabase *xmldb,
						       GdaXmlDatabaseTable *table,
						       const gchar *fname);
void                 gda_xml_database_table_remove_field (GdaXmlDatabase *xmldb,
							  GdaXmlDatabaseTable *table,
							  const gchar *fname);
GdaXmlDatabaseField *gda_xml_database_table_get_field (GdaXmlDatabase *xmldb,
						       GdaXmlDatabaseTable *table,
						       gint pos);
GdaXmlDatabaseField *gda_xml_database_table_find_field (GdaXmlDatabase *xmldb,
							GdaXmlDatabaseTable *table,
							const gchar *fname);
const gchar         *gda_xml_database_field_get_name (GdaXmlDatabase *xmldb,
						      GdaXmlDatabaseField *field);
void                 gda_xml_database_field_set_name (GdaXmlDatabase *xmldb,
						      GdaXmlDatabaseField *field,
						      const gchar *name);
const gchar         *gda_xml_database_field_get_gdatype (GdaXmlDatabase *xmldb,
							 GdaXmlDatabaseField *field);
void                 gda_xml_database_field_set_gdatype (GdaXmlDatabase *xmldb,
							 GdaXmlDatabaseField *field,
							 const gchar *type);
gint                 gda_xml_database_field_get_size (GdaXmlDatabase *xmldb,
						      GdaXmlDatabaseField *field);
void                 gda_xml_database_field_set_size (GdaXmlDatabase *xmldb,
						      GdaXmlDatabaseField *field,
						      gint size);
gint                 gda_xml_database_field_get_scale (GdaXmlDatabase *xmldb,
						       GdaXmlDatabaseField *field);
void                 gda_xml_database_field_set_scale (GdaXmlDatabase *xmldb,
						       GdaXmlDatabaseField *field,
						       gint scale);

G_END_DECLS

#endif
