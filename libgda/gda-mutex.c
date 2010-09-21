/* GDA Library
 * Copyright (C) 2008 The GNOME Foundation.
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

#include <libgda/gda-mutex.h>

enum MutexRecStatus {
	UNKNOWN,
	RECURSIVE,
	NON_RECURSIVE,
	NON_SUPPORTED
};

static enum MutexRecStatus impl_status = UNKNOWN;

struct _GdaMutex {
	GMutex  *mutex; /* internal mutex to access the structure's data */
	GCond   *cond;  /* condition to lock on */
	GThread *owner; /* current owner of the mutex, or NULL if not owned */
	short    depth;
};

/**
 * gda_mutex_new:
 *
 * Creates a new #GdaMutex.
 *
 * Note: Unlike g_mutex_new(), this function will return %NULL if g_thread_init() has not been called yet.
 *
 * Returns: (transfer full): a new #GdaMutex
 */
GdaMutex*
gda_mutex_new ()
{
	if (G_UNLIKELY (impl_status == UNKNOWN)) {
		static GStaticMutex init_mutex = G_STATIC_MUTEX_INIT;

		g_static_mutex_lock (&init_mutex);
		if (impl_status == UNKNOWN) {
			if (!g_thread_supported ()) 
				impl_status = NON_SUPPORTED;
			else {
				GMutex *m;
				m = g_mutex_new ();
				g_mutex_lock (m);
				if (g_mutex_trylock (m)) {
					impl_status = RECURSIVE;
					g_mutex_unlock (m);
				}
				else
					impl_status = NON_RECURSIVE;
				g_mutex_unlock (m);
				g_mutex_free (m);
#ifdef GDA_DEBUG_NO
				g_message ("GMutex %s recursive\n", (impl_status == RECURSIVE) ? "is" : "isn't");
#endif
			}
		}
                g_static_mutex_unlock (&init_mutex);
	}

	if (impl_status == NON_SUPPORTED) {
		GdaMutex *m;
		m = g_new0 (GdaMutex, 1);
		return m;
	}
	else {
		GdaMutex *m;
		m = g_new0 (GdaMutex, 1);
		m->mutex = g_mutex_new ();
		m->cond = g_cond_new ();
		m->owner = NULL;
		m->depth = 0;
		
		return m;
	}
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
 */
void
gda_mutex_lock (GdaMutex *mutex)
{
	if (impl_status == RECURSIVE)
		g_mutex_lock (mutex->mutex);
	else if (impl_status == NON_SUPPORTED)
		return;
	else {
		GThread *th = g_thread_self ();
		g_mutex_lock (mutex->mutex);
		while (1) {
			if (!mutex->owner) {
				mutex->owner = th;
				mutex->depth = 1;
				break;
			}
			else if (mutex->owner == th) {
				mutex->depth++;
				break;
			}
			else {
				g_cond_wait (mutex->cond, mutex->mutex);
			}
                }
		g_mutex_unlock (mutex->mutex);
	}
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
 */
gboolean
gda_mutex_trylock (GdaMutex *mutex)
{
	if (impl_status == RECURSIVE)
		return g_mutex_trylock (mutex->mutex);
	else if (impl_status == NON_SUPPORTED)
		return TRUE;
	else {
		GThread *th = g_thread_self ();
		gboolean retval;
		g_mutex_lock (mutex->mutex);
		if (!mutex->owner) {
			mutex->owner = th;
			mutex->depth = 1;
			retval = TRUE;
		}
		else if (mutex->owner == th) {
			mutex->depth++;
			retval = TRUE;
		}
		else
			retval = FALSE;
		g_mutex_unlock (mutex->mutex);
		return retval;
	}
}

/**
 * gda_mutex_unlock:
 * @mutex: a #GdaMutex
 *
 * Unlocks @mutex. If another thread is blocked in a gda_mutex_lock() call for @mutex, it wil
 * be woken and can lock @mutex itself.
 * This function can be used even if g_thread_init() has not yet been called, and, in that case, will do nothing. 
 */
void
gda_mutex_unlock (GdaMutex *mutex)
{
	if (impl_status == RECURSIVE)
		g_mutex_unlock (mutex->mutex);
	else if (impl_status == NON_SUPPORTED)
		return;
	else {
		GThread *th = g_thread_self ();
		g_mutex_lock (mutex->mutex);
		g_assert (th == mutex->owner);
		mutex->depth--;
                if (mutex->depth == 0) {
                        mutex->owner = NULL;
			g_cond_signal (mutex->cond);
                }
		g_mutex_unlock (mutex->mutex);
	}
}

/**
 * gda_mutex_free:
 * @mutex: (transfer full): a #GdaMutex
 *
 * Destroys @mutex.
 */
void
gda_mutex_free (GdaMutex *mutex)
{
	g_assert (mutex);
	if (mutex->cond)
		g_cond_free (mutex->cond);
	mutex->cond = NULL;
	if (mutex->mutex)
		g_mutex_free (mutex->mutex);
	mutex->mutex = NULL;
	g_free (mutex);
}
