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

#include <glib.h>
#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtkobject.h>
#endif
#include <gda-xml-document.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _GdaXmlDatabase      GdaXmlDatabase;
typedef struct _GdaXmlDatabaseClass GdaXmlDatabaseClass;

#define GDA_TYPE_XML_DATABASE            (gda_xml_database_get_type())
#ifdef HAVE_GOBJECT
#define GDA_XML_DATABASE(obj)            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XML_DATABASE, GdaXmlDatabase)
#define GDA_XML_DATABASE_CLASS(klass)    G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XML_DATABASE, GdaXmlDatabaseClass)
#define GDA_IS_XML_DATABASE(obj)         G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_XML_DATABASE)
#define GDA_IS_XML_DATABASE_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_XML_DATABASE)
#else
#define GDA_XML_DATABASE(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XML_DATABASE, GdaXmlDatabase)
#define GDA_XML_DATABASE_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XML_DATABASE, GdaXmlDatabaseClass)
#define GDA_IS_XML_DATABASE(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XML_DATABASE)
#define GDA_IS_XML_DATABASE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XML_DATABASE))
#endif

struct _GdaXmlDatabase {
	GdaXmlDocument document;

	xmlNodePtr  tables;
	xmlNodePtr  views;
};

struct _GdaXmlDatabaseClass {
	GdaXmlDocumentClass parent_class;

	/* signals */
	void (*changed)(GdaXmlDatabase *xmldb);
};

typedef xmlNode GdaXmlField;
typedef xmlNode GdaXmlTable;
typedef xmlNode GdaXmlView;

#ifdef HAVE_GOBJECT
GType           gda_xml_database_get_type         (void);
#else
GtkType         gda_xml_database_get_type         (void);
#endif

GdaXmlDatabase* gda_xml_database_new              (void);
GdaXmlDatabase* gda_xml_database_new_from_file    (const gchar *filename);
void            gda_xml_database_free             (GdaXmlDatabase *xmldb);

void            gda_xml_database_save             (GdaXmlDatabase *xmldb, const gchar *filename);
void            gda_xml_database_changed          (GdaXmlDatabase *xmldb);

#define gda_xml_database_get_tables(_xmldb_) (GDA_IS_XML_DATABASE((_xmldb_)) ? (_xmldb_)->tables : NULL)
#define gda_xml_database_get_views(_xmldb_)  (GDA_IS_XML_DATABASE((_xmldb_)) ? (_xmldb_)->views : NULL)

/*
 * Table management
 */
GdaXmlTable* gda_xml_database_table_new            (GdaXmlDatabase *xmldb, gchar *tname);
void         gda_xml_database_table_remove         (GdaXmlDatabase *xmldb, const gchar *tname);
GdaXmlTable* gda_xml_database_table_find           (GdaXmlDatabase *xmldb, const gchar *tname);
const gchar* gda_xml_database_table_get_name       (GdaXmlDatabase *xmldb, xmlNodePtr table);
void         gda_xml_database_table_set_name       (GdaXmlDatabase *xmldb,
                                                    GdaXmlTable *table,
                                                    const gchar *tname);
const gchar* gda_xml_database_table_get_owner      (GdaXmlDatabase *xmldb, GdaXmlTable *table);
void         gda_xml_database_table_set_owner      (GdaXmlDatabase *xmldb,
                                                    GdaXmlTable *table,
                                                    const gchar *owner);

/*
 * Table field management
 */
gint         gda_xml_database_table_field_count    (GdaXmlDatabase *xmldb, GdaXmlTable *table);

GdaXmlField* gda_xml_database_table_add_field      (GdaXmlDatabase *xmldb,
                                                    GdaXmlTable *table,
                                                    const gchar *fname);
void         gda_xml_database_table_remove_field   (GdaXmlDatabase *xmldb,
                                                    GdaXmlTable *table,
                                                    const gchar *fname);
GdaXmlField* gda_xml_database_table_get_field      (GdaXmlDatabase *xmldb, GdaXmlTable *table, gint pos);
GdaXmlField* gda_xml_database_table_find_field     (GdaXmlDatabase *xmldb,
                                                    GdaXmlTable *table,
                                                    const gchar *fname);
const gchar* gda_xml_database_field_get_name       (GdaXmlDatabase *xmldb, GdaXmlField *field);
void         gda_xml_database_field_set_name       (GdaXmlDatabase *xmldb, GdaXmlField *field, const gchar *name);
const gchar* gda_xml_database_field_get_gdatype    (GdaXmlDatabase *xmldb, GdaXmlField *field);
void         gda_xml_database_field_set_gdatype    (GdaXmlDatabase *xmldb, GdaXmlField *field, const gchar *type);
gint         gda_xml_database_field_get_size       (GdaXmlDatabase *xmldb, GdaXmlField *field);
void         gda_xml_database_field_set_size       (GdaXmlDatabase *xmldb, GdaXmlField *field, gint size);
gint         gda_xml_database_field_get_scale      (GdaXmlDatabase *xmldb, GdaXmlField *field);
void         gda_xml_database_field_set_scale      (GdaXmlDatabase *xmldb, GdaXmlField *field, gint scale);

#if defined(__cplusplus)
}
#endif

#endif
