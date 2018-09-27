/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
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

#include <libgda/gda-mutex.h>

/**
 * gda_mutex_new:
 *
 * Creates a new #GdaMutex.
 *
 * Deprecated: 5.2.0: use #GRecMutex instead.
 *
 * Returns: (transfer full): a new #GdaMutex
 */
GdaMutex*
gda_mutex_new (void)
{
	GdaMutex *mutex;
	mutex = g_new0 (GdaMutex, 1);
	g_rec_mutex_init (mutex);
	return mutex;
}

/**
 * gda_mutex_lock:
 * @mutex: a #GdaMutex
 *
 * Locks @mutex. If @mutex is already locked by another thread, the current thread will block until @mutex is unlocked by the other thread.
 *
 * This function can be used even if g_thread_init() has not yet been called, and, in that case, will do nothing.
 *
 * Note: unlike g_mutex_lock(), the #GdaMutex is recursive, which means a thread can lock it several times (and has
 * to unlock it as many times to actually unlock it).
 *
 * Deprecated: 5.2.0: use #GRecMutex instead.
 */
void
gda_mutex_lock (GdaMutex *mutex)
{
	g_rec_mutex_lock (mutex);
}

/**
 * gda_mutex_trylock:
 * @mutex: a #GdaMutex
 * 
 * Tries to lock @mutex. If @mutex is already locked by another thread, it immediately returns FALSE.
 * Otherwise it locks @mutex and returns TRUE
 *
 * This function can be used even if g_thread_init() has not yet been called, and, in that case, will immediately return TRUE.
 *
 * Note: Unlike g_mutex_trylock(), the #GdaMutex is recursive, which means a thread can lock it several times (and has
 * to unlock it as many times to actually unlock it)
 *
 * Returns: TRUE, if @mutex could be locked.
 *
 * Deprecated: 5.2.0: use #GRecMutex instead.
 */
gboolean
gda_mutex_trylock (GdaMutex *mutex)
{
	return g_rec_mutex_trylock (mutex);
}

/**
 * gda_mutex_unlock:
 * @mutex: a #GdaMutex
 *
 * Unlocks @mutex. If another thread is blocked in a gda_mutex_lock() call for @mutex, it wil
 * be woken and can lock @mutex itself.
 * This function can be used even if g_thread_init() has not yet been called, and, in that case, will do nothing. 
 *
 * Deprecated: 5.2.0: use #GRecMutex instead.
 */
void
gda_mutex_unlock (GdaMutex *mutex)
{
	g_rec_mutex_unlock (mutex);
}

/**
 * gda_mutex_free:
 * @mutex: (transfer full): a #GdaMutex
 *
 * Destroys @mutex.
 *
 * Deprecated: 5.2.0: use #GRecMutex instead.
 */
void
gda_mutex_free (GdaMutex *mutex)
{
	g_rec_mutex_clear (mutex);
	g_free (mutex);
}

#ifdef GDA_DEBUG_MUTEX
void
gda_mutex_debug (GdaMutex *mutex, gboolean debug)
{
	g_warning ("Nothing to debug, GdaMutex is a GRecMutex.");
}

void
gda_mutex_dump_usage (GdaMutex *mutex, FILE *stream)
{
	g_warning ("Nothing to debug, GdaMutex is a GRecMutex.");
	g_fprintf (stream, "%s", "Nothing to debug, GdaMutex is a GRecMutex.");
}
#endif
