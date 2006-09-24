/* gda-dict-type.h
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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


#ifndef __GDA_DICT_TYPE_H_
#define __GDA_DICT_TYPE_H_

#include "gda-decl.h"
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDA_TYPE_DICT_TYPE          (gda_dict_type_get_type())
#define GDA_DICT_TYPE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_dict_type_get_type(), GdaDictType)
#define GDA_DICT_TYPE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_dict_type_get_type (), GdaDictTypeClass)
#define GDA_IS_DICT_TYPE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_dict_type_get_type ())


/* error reporting */
extern GQuark gda_dict_type_error_quark (void);
#define GDA_DICT_TYPE_ERROR gda_dict_type_error_quark ()

enum
{
	GDA_DICT_TYPE_XML_LOAD_ERROR
};


/* struct for the object's data */
struct _GdaDictType
{
	GdaObject                 object;
	GdaDictTypePrivate       *priv;
};

/* struct for the object's class */
struct _GdaDictTypeClass
{
	GdaObjectClass                    parent_class;
};

GType               gda_dict_type_get_type      (void);
GdaDictType        *gda_dict_type_new           (GdaDict *dict);

void                gda_dict_type_set_sqlname   (GdaDictType *dt, const gchar *sqlname);
const gchar        *gda_dict_type_get_sqlname   (GdaDictType *dt);

void                gda_dict_type_set_g_type    (GdaDictType *dt, GType g_type);
GType               gda_dict_type_get_g_type    (GdaDictType *dt);

void                gda_dict_type_add_synonym   (GdaDictType *dt, const gchar *synonym);
const GSList       *gda_dict_type_get_synonyms  (GdaDictType *dt);
void                gda_dict_type_clear_synonyms(GdaDictType *dt);

G_END_DECLS

#endif
