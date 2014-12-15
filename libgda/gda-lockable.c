/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <libgda/gda-lockable.h>

static GRecMutex init_rmutex;
#define MUTEX_LOCK() g_rec_mutex_lock(&init_rmutex)
#define MUTEX_UNLOCK() g_rec_mutex_unlock(&init_rmutex)
static void gda_lockable_class_init (gpointer g_class);

GType
gda_lockable_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaLockableIface),
			(GBaseInitFunc) gda_lockable_class_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL,
			0
		};
		
		MUTEX_LOCK();
		if (type == 0) {
			type = g_type_register_static (G_TYPE_INTERFACE, "GdaLockable", &info, 0);
			g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
		}
		MUTEX_UNLOCK();
	}
	return type;
}

static void
gda_lockable_class_init (G_GNUC_UNUSED gpointer g_class)
{
	static gboolean initialized = FALSE;

	MUTEX_LOCK();
	if (! initialized) {
		initialized = TRUE;
	}
	MUTEX_UNLOCK();
}

/**
 * gda_lockable_lock:
 * @lockable: a #GdaLockable object.
 *
 * Locks @lockable. If it is already locked by another thread, the current thread will block until it is unlocked 
 * by the other thread.
 *
 * Note: unlike g_mutex_lock(), this method recursive, which means a thread can lock @lockable several times 
 * (and has to unlock it as many times to actually unlock it).
 */
void
gda_lockable_lock (GdaLockable *lockable)
{
	g_return_if_fail (GDA_IS_LOCKABLE (lockable));
	
	if (GDA_LOCKABLE_GET_CLASS (lockable)->i_lock)
		(GDA_LOCKABLE_GET_CLASS (lockable)->i_lock) (lockable);
	else
		g_warning ("Internal implementation error: %s() method not implemented\n", "i_lock");
}

/**
 * gda_lockable_trylock:
 * @lockable: a #GdaLockable object.
 *
 * Tries to lock @lockable. If it is already locked by another thread, then it immediately returns FALSE, otherwise
 * it locks @lockable.
 *
 * Note: unlike g_mutex_lock(), this method recursive, which means a thread can lock @lockable several times 
 * (and has to unlock it as many times to actually unlock it).
 *
 * Returns: TRUE if the object has successfully been locked.
 */
gboolean
gda_lockable_trylock (GdaLockable *lockable)
{
	g_return_val_if_fail (GDA_IS_LOCKABLE (lockable), FALSE);
	
	if (GDA_LOCKABLE_GET_CLASS (lockable)->i_trylock)
		return (GDA_LOCKABLE_GET_CLASS (lockable)->i_trylock) (lockable);
	else {
		g_warning ("Internal implementation error: %s() method not implemented\n", "i_trylock");
		return FALSE;
	}
}

/**
 * gda_lockable_unlock:
 * @lockable: a #GdaLockable object.
 *
 * Unlocks @lockable. This method should not be called if the current does not already holds a lock on @lockable (having
 * used gda_lockable_lock() or gda_lockable_trylock()).
 */
void
gda_lockable_unlock (GdaLockable *lockable)
{
	g_return_if_fail (GDA_IS_LOCKABLE (lockable));
	
	if (GDA_LOCKABLE_GET_CLASS (lockable)->i_unlock)
		(GDA_LOCKABLE_GET_CLASS (lockable)->i_unlock) (lockable);
	else
		g_warning ("Internal implementation error: %s() method not implemented\n", "i_unlock");
}
