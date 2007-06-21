/* gda-dict-table.h
 *
 * Copyright (C) 2003 Vivien Malerba
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


#ifndef __GDA_DICT_TABLE_H_
#define __GDA_DICT_TABLE_H_

#include <libgda/gda-object.h>
#include "gda-decl.h"
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDA_TYPE_DICT_TABLE          (gda_dict_table_get_type())
#define GDA_DICT_TABLE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_dict_table_get_type(), GdaDictTable)
#define GDA_DICT_TABLE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_dict_table_get_type (), GdaDictTableClass)
#define GDA_IS_DICT_TABLE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_dict_table_get_type ())


/* error reporting */
extern GQuark gda_dict_table_error_quark (void);
#define GDA_DICT_TABLE_ERROR gda_dict_table_error_quark ()

typedef enum
{
	GDA_DICT_TABLE_XML_LOAD_ERROR,
	GDA_DICT_TABLE_META_DATA_UPDATE,
	GDA_DICT_FIELDS_ERROR
} GdaDictTableError;


/* struct for the object's data */
struct _GdaDictTable
{
	GdaObject                  object;
	GdaDictTablePrivate       *priv;
};

/* struct for the object's class */
struct _GdaDictTableClass
{
	GdaObjectClass             parent_class;
};

GType              gda_dict_table_get_type          (void) G_GNUC_CONST;
GObject           *gda_dict_table_new               (GdaDict *dict);

GdaDictDatabase   *gda_dict_table_get_database      (GdaDictTable *table);
gboolean           gda_dict_table_is_view           (GdaDictTable *table);
const GSList      *gda_dict_table_get_parents       (GdaDictTable *table);
GSList            *gda_dict_table_get_constraints   (GdaDictTable *table);
GdaDictConstraint *gda_dict_table_get_pk_constraint (GdaDictTable *table);

gboolean           gda_dict_table_update_dbms_data  (GdaDictTable *table, GError **error);

G_END_DECLS

#endif
