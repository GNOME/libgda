/* gda-object.h
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

#ifndef __GDA_OBJECT_H_
#define __GDA_OBJECT_H_

#include <glib-object.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_OBJECT          (gda_object_get_type())
#define GDA_OBJECT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_object_get_type(), GdaObject)
#define GDA_OBJECT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_object_get_type (), GdaObjectClass)
#define GDA_IS_OBJECT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_object_get_type ())

/* struct for the object's data */
struct _GdaObject
{
	GObject            object;
	GdaObjectPrivate  *priv;
};

/* struct for the object's class */
struct _GdaObjectClass
{
	GObjectClass            parent_class;

	/* signals */
	void        (*changed)           (GdaObject *object);
	void        (*id_changed)        (GdaObject *object);
	void        (*name_changed)      (GdaObject *object);
	void        (*descr_changed)     (GdaObject *object);
	void        (*owner_changed)     (GdaObject *object);

	void        (*to_be_destroyed)   (GdaObject *object);
	void        (*destroyed)         (GdaObject *object);

	/* pure virtual functions */
	void        (*signal_changed) (GdaObject *object, gboolean block_changed_signal);
#ifdef GDA_DEBUG
	void        (*dump)           (GdaObject *object, guint offset);
#endif

	/* class attributes */
	gboolean      id_unique_enforced; /* TRUE if a unique string ID must be enforced for that class */
};

GType        gda_object_get_type        (void);
GdaDict     *gda_object_get_dict        (GdaObject *object);

void         gda_object_set_id   (GdaObject *object, const gchar *strid);
void         gda_object_set_name        (GdaObject *object, const gchar *name);
void         gda_object_set_description (GdaObject *object, const gchar *descr);
void         gda_object_set_owner       (GdaObject *object, const gchar *owner);

const gchar *gda_object_get_id   (GdaObject *object);
const gchar *gda_object_get_name        (GdaObject *object);
const gchar *gda_object_get_description (GdaObject *object);
const gchar *gda_object_get_owner       (GdaObject *object);

void         gda_object_destroy         (GdaObject *object); /* force the object to completely clean itself */
void         gda_object_destroy_check   (GdaObject *object); 
gulong       gda_object_connect_destroy (gpointer object, GCallback callback, gpointer data);

void         gda_object_changed         (GdaObject *object);
void         gda_object_block_changed   (GdaObject *object);
void         gda_object_unblock_changed (GdaObject *object);

#ifdef GDA_DEBUG
void         gda_object_dump            (GdaObject *object, guint offset); /* dump contents on stdout */
#endif

G_END_DECLS

#endif
