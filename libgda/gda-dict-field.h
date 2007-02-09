/* gda-dict-field.h
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


#ifndef __GDA_DICT_FIELD_H_
#define __GDA_DICT_FIELD_H_

#include <libgda/gda-object.h>
#include "gda-decl.h"
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDA_TYPE_DICT_FIELD          (gda_dict_field_get_type())
#define GDA_DICT_FIELD(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_dict_field_get_type(), GdaDictField)
#define GDA_DICT_FIELD_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_dict_field_get_type (), GdaDictFieldClass)
#define GDA_IS_DICT_FIELD(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_dict_field_get_type ())


/* error reporting */
extern GQuark gda_dict_field_error_quark (void);
#define GDA_DICT_FIELD_ERROR gda_dict_field_error_quark ()

typedef enum
{
	GDA_DICT_FIELD_XML_LOAD_ERROR
} GdaDictFieldError;


/* struct for the object's data */
struct _GdaDictField
{
	GdaObject               object;
	GdaDictFieldPrivate    *priv;
};

/* struct for the object's class */
struct _GdaDictFieldClass
{
	GdaObjectClass          parent_class;
};

typedef enum {
	FIELD_AUTO_INCREMENT = 1 << 0
} GdaDictFieldAttribute;

GType           gda_dict_field_get_type          (void);
GObject        *gda_dict_field_new               (GdaDict *dict, GdaDictType *type);

void            gda_dict_field_set_length        (GdaDictField *field, gint length);
gint            gda_dict_field_get_length        (GdaDictField *field);
void            gda_dict_field_set_scale         (GdaDictField *field, gint length);
gint            gda_dict_field_get_scale         (GdaDictField *field);
GSList         *gda_dict_field_get_constraints   (GdaDictField *field);
void            gda_dict_field_set_dict_type     (GdaDictField *field, GdaDictType *type);

void            gda_dict_field_set_default_value (GdaDictField *field, const GValue *value);
const GValue   *gda_dict_field_get_default_value (GdaDictField *field);

gboolean        gda_dict_field_is_null_allowed   (GdaDictField *field);
gboolean        gda_dict_field_is_pkey_part      (GdaDictField *field);
gboolean        gda_dict_field_is_pkey_alone     (GdaDictField *field);
gboolean        gda_dict_field_is_fkey_part      (GdaDictField *field);
gboolean        gda_dict_field_is_fkey_alone     (GdaDictField *field);

void            gda_dict_field_set_attributes    (GdaDictField *field, GdaDictFieldAttribute attributes);
GdaValueAttribute gda_dict_field_get_attributes    (GdaDictField *field);

G_END_DECLS

#endif
