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
#  include <gtk/gtk.h>
#endif
#include <gda-xml-file.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _Gda_XmlDatabase      Gda_XmlDatabase;
typedef struct _Gda_XmlDatabaseClass Gda_XmlDatabaseClass;

#define GDA_TYPE_XML_DATABASE            (gda_xml_database_get_type())
#ifdef HAVE_GOBJECT
#  define GDA_XML_DATABASE(obj) \
       G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XML_DATABASE, Gda_XmlDatabase)
#  define GDA_XML_DATABASE_CLASS(klass) \
   G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XML_DATABASE, Gda_XmlDatabaseClass)
#  define GDA_IS_XML_DATABASE(obj) \
          G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_XML_DATABASE)
#  define GDA_IS_XML_DATABASE_CLASS(klass) \
          G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_XML_DATABASE)
#else
#  define GDA_XML_DATABASE(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XML_DATABASE, Gda_XmlDatabase)
#  define GDA_XML_DATABASE_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XML_DATABASE, Gda_XmlDatabaseClass)
#  define GDA_IS_XML_DATABASE(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XML_DATABASE)
#  define GDA_IS_XML_DATABASE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XML_DATABASE))
#endif

struct _Gda_XmlDatabase
{
  Gda_XmlFile file;

  xmlNodePtr  tables;
  xmlNodePtr  views;
};

struct _Gda_XmlDatabaseClass
{
  Gda_XmlFileClass parent_class;

  /* signals */
  void (*changed)(Gda_XmlDatabase *xmldb);
};

#ifdef HAVE_GOBJECT
GType            gda_xml_database_get_type         (void);
#else
GtkType          gda_xml_database_get_type         (void);
#endif

Gda_XmlDatabase* gda_xml_database_new              (void);
Gda_XmlDatabase* gda_xml_database_new_from_file    (const gchar *filename);
void             gda_xml_database_free             (Gda_XmlDatabase *xmldb);

void             gda_xml_database_save             (Gda_XmlDatabase *xmldb, const gchar *filename);
void             gda_xml_database_changed          (Gda_XmlDatabase *xmldb);

#define gda_xml_database_get_tables(_xmldb_) (GDA_IS_XML_DATABASE((_xmldb_)) ? (_xmldb_)->tables : NULL)
#define gda_xml_database_get_views(_xmldb_)  (GDA_IS_XML_DATABASE((_xmldb_)) ? (_xmldb_)->views : NULL)

/*
 * Table management
 */
xmlNodePtr   gda_xml_database_table_new            (Gda_XmlDatabase *xmldb, gchar *tname);
void         gda_xml_database_table_remove         (Gda_XmlDatabase *xmldb, const gchar *tname);
xmlNodePtr   gda_xml_database_table_find           (Gda_XmlDatabase *xmldb, const gchar *tname);
const gchar* gda_xml_database_table_get_name       (Gda_XmlDatabase *xmldb, xmlNodePtr table);
void         gda_xml_database_table_set_name       (Gda_XmlDatabase *xmldb,
                                                    xmlNodePtr table,
                                                    const gchar *tname);
const gchar* gda_xml_database_table_get_owner      (Gda_XmlDatabase *xmldb, xmlNodePtr table);
void         gda_xml_database_table_set_owner      (Gda_XmlDatabase *xmldb,
                                                    xmlNodePtr table,
                                                    const gchar *owner);

/*
 * Table field management
 */
gint         gda_xml_database_table_field_count    (Gda_XmlDatabase *xmldb, xmlNodePtr table);

xmlNodePtr   gda_xml_database_table_add_field      (Gda_XmlDatabase *xmldb,
                                                    xmlNodePtr table,
                                                    const gchar *fname);
void         gda_xml_database_table_remove_field   (Gda_XmlDatabase *xmldb,
                                                    xmlNodePtr table,
                                                    const gchar *fname);
xmlNodePtr   gda_xml_database_table_get_field      (Gda_XmlDatabase *xmldb, xmlNodePtr table, gint pos);
xmlNodePtr   gda_xml_database_table_find_field     (Gda_XmlDatabase *xmldb,
                                                    xmlNodePtr table,
                                                    const gchar *fname);
const gchar* gda_xml_database_field_get_name       (Gda_XmlDatabase *xmldb, xmlNodePtr field);
void         gda_xml_database_field_set_name       (Gda_XmlDatabase *xmldb, xmlNodePtr field, const gchar *name);
const gchar* gda_xml_database_field_get_gdatype    (Gda_XmlDatabase *xmldb, xmlNodePtr field);
void         gda_xml_database_field_set_gdatype    (Gda_XmlDatabase *xmldb, xmlNodePtr field, const gchar *type);
gint         gda_xml_database_field_get_size       (Gda_XmlDatabase *xmldb, xmlNodePtr field);
void         gda_xml_database_field_set_size       (Gda_XmlDatabase *xmldb, xmlNodePtr field, gint size);
gint         gda_xml_database_field_get_scale      (Gda_XmlDatabase *xmldb, xmlNodePtr field);
void         gda_xml_database_field_set_scale      (Gda_XmlDatabase *xmldb, xmlNodePtr field, gint scale);

#if defined(__cplusplus)
}
#endif

#endif
