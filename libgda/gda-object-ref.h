/* gda-object-ref.h
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


#ifndef __GDA_OBJECT_REF_H_
#define __GDA_OBJECT_REF_H_

#include <libgda/gda-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_OBJECT_REF          (gda_object_ref_get_type())
#define GDA_OBJECT_REF(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_object_ref_get_type(), GdaObjectRef)
#define GDA_OBJECT_REF_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_object_ref_get_type (), GdaObjectRefClass)
#define GDA_IS_OBJECT_REF(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_object_ref_get_type ())

/* error reporting */
extern GQuark gda_object_ref_error_quark (void);
#define GDA_OBJECT_REF_ERROR gda_object_ref_error_quark ()

typedef enum
{
	REFERENCE_BY_XML_ID,
	REFERENCE_BY_NAME
} GdaObjectRefType;

enum
{
	GDA_OBJECT_REF_XML_LOAD_ERROR
};


/* struct for the object's data */
struct _GdaObjectRef
{
	GdaObject                  object;
	GdaObjectRefPrivate       *priv;
};

/* struct for the object's class */
struct _GdaObjectRefClass
{
	GdaObjectClass                    parent_class;

	/* signals */
	void   (*ref_found)           (GdaObjectRef *obj);
	void   (*ref_lost)            (GdaObjectRef *obj);
};

GType           gda_object_ref_get_type           (void);
GObject        *gda_object_ref_new                (GdaDict *dict);
GObject        *gda_object_ref_new_no_ref_count   (GdaDict *dict);
GObject        *gda_object_ref_new_copy           (GdaObjectRef *orig);

void            gda_object_ref_set_ref_name       (GdaObjectRef *ref, GType ref_type, 
						   GdaObjectRefType type, const gchar *name);
const gchar    *gda_object_ref_get_ref_name       (GdaObjectRef *ref, GType *ref_type, GdaObjectRefType *type);
const gchar    *gda_object_ref_get_ref_object_name (GdaObjectRef *ref);
GType           gda_object_ref_get_ref_type       (GdaObjectRef *ref);

void            gda_object_ref_set_ref_object     (GdaObjectRef *ref, GdaObject *object);
void            gda_object_ref_set_ref_object_type(GdaObjectRef *ref, GdaObject *object, GType type);
void            gda_object_ref_replace_ref_object (GdaObjectRef *ref, GHashTable *replacements);
GdaObject      *gda_object_ref_get_ref_object     (GdaObjectRef *ref);

gboolean        gda_object_ref_activate           (GdaObjectRef *ref);
void            gda_object_ref_deactivate         (GdaObjectRef *ref);
gboolean        gda_object_ref_is_active          (GdaObjectRef *ref);

G_END_DECLS

#endif
