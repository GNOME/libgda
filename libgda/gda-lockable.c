/* 
 * GDA common library
 * Copyright (C) 2008 - 2010 The GNOME Foundation.
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

#include <libgda/gda-lockable.h>

static GStaticRecMutex init_mutex = G_STATIC_REC_MUTEX_INIT;
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
		
		g_static_rec_mutex_lock (&init_mutex);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_INTERFACE, "GdaLockable", &info, 0);
			g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
		}
		g_static_rec_mutex_unlock (&init_mutex);
	}
	return type;
}

static void
gda_lockable_class_init (G_GNUC_UNUSED gpointer g_class)
{
	static gboolean initialized = FALSE;

	g_static_rec_mutex_lock (&init_mutex);
	if (! initialized) {
		initialized = TRUE;
	}
	g_static_rec_mutex_unlock (&init_mutex);
}

/**
 * gda_lockable_lock:
 * @lockable: a #GdaLockable object.
 *
 * Locks @lockable. If it is already locked by another thread, the current thread will block until it is unlocked 
 * by the other thread.
 *
 * This function can be used even if g_thread_init() has not yet been called, and, in that case, will do nothing.
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
 * This function can be used even if g_thread_init() has not yet been called, and, in that case, will do nothing.
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
 *
 * This function can be used even if g_thread_init() has not yet been called, and, in that case, will do nothing.
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
