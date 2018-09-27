/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef __DUMMY_OBJECT_H_
#define __DUMMY_OBJECT_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define DUMMY_TYPE_OBJECT          (dummy_object_get_type())
G_DECLARE_DERIVABLE_TYPE (DummyObject, dummy_object, DUMMY, OBJECT, GObject)

/* struct for the object's class */
struct _DummyObjectClass
{
	GObjectClass               parent_class;
	void                     (*sig0)   (DummyObject *object);
	void                     (*sig1)   (DummyObject *object, gint i);
	void                     (*sig2)   (DummyObject *object, gint i, gchar *str);
	gchar                   *(*sig3)   (DummyObject *object, gchar *str, gint i);
};

DummyObject        *dummy_object_new                     (void);

G_END_DECLS

#endif
