/* gda-dict-constraint.h
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
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


#ifndef __GDA_DICT_CONSTRAINT_H_
#define __GDA_DICT_CONSTRAINT_H_

#include <libgda/gda-object.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_DICT_CONSTRAINT          (gda_dict_constraint_get_type())
#define GDA_DICT_CONSTRAINT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_dict_constraint_get_type(), GdaDictConstraint)
#define GDA_DICT_CONSTRAINT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_dict_constraint_get_type (), GdaDictConstraintClass)
#define GDA_IS_DICT_CONSTRAINT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_dict_constraint_get_type ())


/* error reporting */
extern GQuark gda_dict_constraint_error_quark (void);
#define GDA_DICT_CONSTRAINT_ERROR gda_dict_constraint_error_quark ()

typedef enum
{
	GDA_DICT_CONSTRAINT_XML_SAVE_ERROR,
	GDA_DICT_CONSTRAINT_XML_LOAD_ERROR
} GdaDictConstraintError;

typedef enum
{
	CONSTRAINT_PRIMARY_KEY,
	CONSTRAINT_FOREIGN_KEY,
	CONSTRAINT_UNIQUE,
	CONSTRAINT_NOT_NULL,
	CONSTRAINT_CHECK_EXPR,
	CONSTRAINT_CHECK_IN_LIST,
	CONSTRAINT_CHECK_SETOF_LIST,
	CONSTRAINT_UNKNOWN
} GdaDictConstraintType;

typedef enum
{
	CONSTRAINT_FK_ACTION_CASCADE,
	CONSTRAINT_FK_ACTION_SET_NULL,
	CONSTRAINT_FK_ACTION_SET_DEFAULT,
	CONSTRAINT_FK_ACTION_SET_VALUE,
	CONSTRAINT_FK_ACTION_NO_ACTION
} GdaDictConstraintFkAction;

typedef struct
{
	GdaDictField   *fkey;
	GdaDictField   *ref_pkey;
	GdaObjectRef   *ref_pkey_repl; /* can be used instead of ref_pkey, the object will fill ref_pkey itself */
} GdaDictConstraintFkeyPair;

#define GDA_DICT_CONSTRAINT_FK_PAIR(x) ((GdaDictConstraintFkeyPair*) (x))

/* struct for the object's data */
struct _GdaDictConstraint
{
	GdaObject                  object;
	GdaDictConstraintPrivate  *priv;
};

/* struct for the object's class */
struct _GdaDictConstraintClass
{
	GdaObjectClass                    parent_class;
};

GType                  gda_dict_constraint_get_type            (void) G_GNUC_CONST;
GdaDictConstraint     *gda_dict_constraint_new                 (GdaDictTable *table, GdaDictConstraintType type);
GdaDictConstraint     *gda_dict_constraint_new_with_db         (GdaDictDatabase *db);
GdaDictConstraintType  gda_dict_constraint_get_constraint_type (GdaDictConstraint *cstr);
gboolean               gda_dict_constraint_equal               (GdaDictConstraint *cstr1, GdaDictConstraint *cstr2);
GdaDictTable          *gda_dict_constraint_get_table           (GdaDictConstraint *cstr);
gboolean               gda_dict_constraint_uses_field          (GdaDictConstraint *cstr, GdaDictField *field);

/* Primary KEY specific */
void                   gda_dict_constraint_pkey_set_fields     (GdaDictConstraint *cstr, const GSList *fields);
GSList                *gda_dict_constraint_pkey_get_fields     (GdaDictConstraint *cstr);

/* Foreign KEY specific */
void                   gda_dict_constraint_fkey_set_fields     (GdaDictConstraint *cstr, const GSList *pairs);
GdaDictTable          *gda_dict_constraint_fkey_get_ref_table  (GdaDictConstraint *cstr);
GSList                *gda_dict_constraint_fkey_get_fields     (GdaDictConstraint *cstr);
void                   gda_dict_constraint_fkey_set_actions    (GdaDictConstraint *cstr, 
								GdaDictConstraintFkAction on_update, 
								GdaDictConstraintFkAction on_delete);
void                   gda_dict_constraint_fkey_get_actions    (GdaDictConstraint *cstr, 
								GdaDictConstraintFkAction *on_update, 
								GdaDictConstraintFkAction *on_delete);

/* UNIQUE specific */
void                   gda_dict_constraint_unique_set_fields   (GdaDictConstraint *cstr, const GSList *fields);
GSList                *gda_dict_constraint_unique_get_fields   (GdaDictConstraint *cstr);

/* NOT NULL specific */
void                   gda_dict_constraint_not_null_set_field  (GdaDictConstraint *cstr, GdaDictField *field);
GdaDictField          *gda_dict_constraint_not_null_get_field  (GdaDictConstraint *cstr);

/* Check specific */
/* TODO */

G_END_DECLS

#endif
