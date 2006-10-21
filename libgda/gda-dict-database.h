/* gda-dict-database.h
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


#ifndef __GDA_DICT_DATABASE_H_
#define __GDA_DICT_DATABASE_H_

#include <glib-object.h>
#include "gda-decl.h"
#include <libgda/gda-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_DICT_DATABASE          (gda_dict_database_get_type())
#define GDA_DICT_DATABASE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_dict_database_get_type(), GdaDictDatabase)
#define GDA_DICT_DATABASE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_dict_database_get_type (), GdaDictDatabaseClass)
#define GDA_IS_DICT_DATABASE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_dict_database_get_type ())


/* error reporting */
extern GQuark gda_dict_database_error_quark (void);
#define GDA_DICT_DATABASE_ERROR gda_dict_database_error_quark ()

typedef enum
{
	GDA_DICT_DATABASE_XML_SAVE_ERROR,
	GDA_DICT_DATABASE_XML_LOAD_ERROR,
	GDA_DICT_DATABASE_META_DATA_UPDATE,
	GDA_DICT_DATABASE_META_DATA_UPDATE_USER_STOPPED,
	GDA_DICT_DATABASE_TABLES_ERROR,
	GDA_DICT_DATABASE_SEQUENCES_ERROR
} GdaDictDatabaseError;

/* struct for the object's data */
struct _GdaDictDatabase
{
	GdaObject                   object;
	GdaDictDatabasePrivate       *priv;
};

/* struct for the object's class */
struct _GdaDictDatabaseClass
{
	GdaObjectClass parent_class;

	/* signals */
	void   (*table_added)               (GdaDictDatabase *obj, GdaDictTable *table);
	void   (*table_removed)             (GdaDictDatabase *obj, GdaDictTable *table);
	void   (*table_updated)             (GdaDictDatabase *obj, GdaDictTable *table);

	void   (*field_added)               (GdaDictDatabase *obj, GdaDictField *field);
	void   (*field_removed)             (GdaDictDatabase *obj, GdaDictField *field);
	void   (*field_updated)             (GdaDictDatabase *obj, GdaDictField *field);

	void   (*constraint_added)          (GdaDictDatabase *obj, GdaDictConstraint *cstr);
	void   (*constraint_removed)        (GdaDictDatabase *obj, GdaDictConstraint *cstr);
	void   (*constraint_updated)        (GdaDictDatabase *obj, GdaDictConstraint *cstr);
	
	void   (*data_update_started)       (GdaDictDatabase *obj);
	void   (*update_progress)           (GdaDictDatabase *obj, gchar * msg, guint now, guint total);
	void   (*data_update_finished)      (GdaDictDatabase *obj);

	gpointer reserved1;
	gpointer reserved2;
};

GType              gda_dict_database_get_type                  (void);
GObject           *gda_dict_database_new                       (GdaDict *dict);

GdaDict            *gda_dict_database_get_dict                  (GdaDictDatabase *db);

gboolean           gda_dict_database_update_dbms_data          (GdaDictDatabase *db, 
								GType limit_to_type, const gchar *limit_obj_name, 
								GError **error);
void               gda_dict_database_stop_update_dbms_data     (GdaDictDatabase *db);

GSList            *gda_dict_database_get_tables                (GdaDictDatabase *db);
GdaDictTable      *gda_dict_database_get_table_by_name         (GdaDictDatabase *db, const gchar *name);
GdaDictTable      *gda_dict_database_get_table_by_xml_id       (GdaDictDatabase *db, const gchar *xml_id);
GdaDictField      *gda_dict_database_get_field_by_name         (GdaDictDatabase *db, const gchar *fullname);
GdaDictField      *gda_dict_database_get_field_by_xml_id       (GdaDictDatabase *db, const gchar *xml_id);

void               gda_dict_database_add_constraint            (GdaDictDatabase *db, GdaDictConstraint *cstr);
GSList            *gda_dict_database_get_all_constraints       (GdaDictDatabase *db);
GSList            *gda_dict_database_get_all_fk_constraints    (GdaDictDatabase *db);
GSList            *gda_dict_database_get_table_constraints     (GdaDictDatabase *db, GdaDictTable *table);
GSList            *gda_dict_database_get_tables_fk_constraints (GdaDictDatabase *db, GdaDictTable *table1, 
								GdaDictTable *table2, gboolean table1_has_fk);

G_END_DECLS

#endif
