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
#define G_LOG_DOMAIN "GDA-lockable"

#include <libgda/gda-lockable.h>

G_DEFINE_INTERFACE(GdaLockable, gda_lockable, G_TYPE_OBJECT)

void
gda_lockable_default_init (GdaLockableInterface *iface) {}

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
	GdaLockableInterface *iface = GDA_LOCKABLE_GET_IFACE (lockable);
	if (iface->lock)
		(iface->lock) (lockable);
	else
		g_warning ("Internal implementation error: %s() method not implemented\n", "lock");
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
	GdaLockableInterface *iface = GDA_LOCKABLE_GET_IFACE (lockable);
	
	if (iface->trylock)
		return (iface->trylock) (lockable);

	g_warning ("Internal implementation error: %s() method not implemented\n", "trylock");
	return FALSE;
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
	GdaLockableInterface *iface = GDA_LOCKABLE_GET_IFACE (lockable);

	if (iface->unlock)
		(iface->unlock) (lockable);
	else
		g_warning ("Internal implementation error: %s() method not implemented\n", "unlock");
}
