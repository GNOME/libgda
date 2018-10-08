/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_LOCKABLE_H__
#define __GDA_LOCKABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_LOCKABLE            (gda_lockable_get_type())
G_DECLARE_INTERFACE (GdaLockable, gda_lockable, GDA, LOCKABLE, GObject)
/* struct for the interface */
struct _GdaLockableInterface {
	GTypeInterface           g_iface;

	/* virtual table */
	void                 (* lock)       (GdaLockable *lockable);
	gboolean             (* trylock)    (GdaLockable *lockable);
	void                 (* unlock)     (GdaLockable *lockable);
};

/**
 * SECTION:gda-lockable
 * @short_description: Interface for locking objects in a multi threaded environment
 * @title: GdaLockable
 * @stability: Stable
 * @see_also: #GRecMutex and #GMutex
 *
 * This interface is implemented by objects which are thread safe (ie. can be used by several threads at
 * the same time). Before using an object from a thread, one has to call gda_lockable_lock() or
 * gda_lockable_trylock() and call gda_lockable_unlock() when the object is not used anymore.
 */

void       gda_lockable_lock       (GdaLockable *lockable);
gboolean   gda_lockable_trylock    (GdaLockable *lockable);
void       gda_lockable_unlock     (GdaLockable *lockable);

G_END_DECLS

#endif
