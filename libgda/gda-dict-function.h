/* gda-dict-function.h
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


#ifndef __GDA_DICT_FUNCTION_H_
#define __GDA_DICT_FUNCTION_H_

#include <libgda/gda-object.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_DICT_FUNCTION          (gda_dict_function_get_type())
#define GDA_DICT_FUNCTION(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_dict_function_get_type(), GdaDictFunction)
#define GDA_DICT_FUNCTION_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_dict_function_get_type (), GdaDictFunctionClass)
#define GDA_IS_DICT_FUNCTION(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_dict_function_get_type ())


/* error reporting */
extern GQuark gda_dict_function_error_quark (void);
#define GDA_DICT_FUNCTION_ERROR gda_dict_function_error_quark ()

enum
{
	GDA_DICT_FUNCTION_XML_LOAD_ERROR
};


/* struct for the object's data */
struct _GdaDictFunction
{
	GdaObject                         object;
	GdaDictFunctionPrivate       *priv;
};

/* struct for the object's class */
struct _GdaDictFunctionClass
{
	GdaObjectClass                    parent_class;
};

GType             gda_dict_function_get_type      (void);
GObject          *gda_dict_function_new           (GdaDict *dict);
void              gda_dict_function_set_dbms_id   (GdaDictFunction *func, const gchar *id);
gchar            *gda_dict_function_get_dbms_id   (GdaDictFunction *func);
void              gda_dict_function_set_sqlname   (GdaDictFunction *func, const gchar *sqlname);
const gchar      *gda_dict_function_get_sqlname   (GdaDictFunction *func);
void              gda_dict_function_set_arg_types (GdaDictFunction *func, const GSList *arg_types);
const GSList     *gda_dict_function_get_arg_types (GdaDictFunction *func);
void              gda_dict_function_set_ret_type  (GdaDictFunction *func, GdaDictType *dt);
GdaDictType      *gda_dict_function_get_ret_type  (GdaDictFunction *func);

gboolean          gda_dict_function_accepts_args  (GdaDictFunction *func, const GSList *arg_types);

G_END_DECLS

#endif
