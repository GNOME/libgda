/* GDA common library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_LOCKABLE_H__
#define __GDA_LOCKABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_LOCKABLE            (gda_lockable_get_type())
#define GDA_LOCKABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_LOCKABLE, GdaLockable))
#define GDA_IS_LOCKABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_LOCKABLE))
#define GDA_LOCKABLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDA_TYPE_LOCKABLE, GdaLockableIface))

typedef struct _GdaLockableIface   GdaLockableIface;
typedef struct _GdaLockable        GdaLockable;

/* struct for the interface */
struct _GdaLockableIface {
	GTypeInterface           g_iface;

	/* virtual table */
	void                 (* i_lock)       (GdaLockable *lock);
	gboolean             (* i_trylock)    (GdaLockable *lock);
	void                 (* i_unlock)     (GdaLockable *lock);
};

GType      gda_lockable_get_type   (void) G_GNUC_CONST;

void       gda_lockable_lock       (GdaLockable *lockable);
gboolean   gda_lockable_trylock    (GdaLockable *lockable);
void       gda_lockable_unlock     (GdaLockable *lockable);

G_END_DECLS

#endif
