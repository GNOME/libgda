/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#if GLIB_CHECK_VERSION(2,31,7)

/**
 * gda_mutex_new: (skip)
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


#else /* GLIB_CHECK_VERSION */

#ifdef GDA_DEBUG_MUTEX
#define FRAMES_SIZE 10
#include <glib/gprintf.h>
#include <execinfo.h>
#include <stdlib.h>
#include <string.h>
#endif

enum MutexRecStatus {
	UNKNOWN,
	RECURSIVE,
	NON_RECURSIVE,
	NON_SUPPORTED
};

static enum MutexRecStatus impl_status = UNKNOWN;

#ifdef GDA_DEBUG_MUTEX
static GMutex debug_mutex;

typedef enum {
	USAGE_LOCK,
	USAGE_UNLOCK,
} GdaMutexUsageType;

typedef struct {
	GThread            *thread;
	GdaMutexUsageType   usage;
	gchar             **frames; /* array terminated by a NULL */
} GdaMutexUsage;
#endif

struct _GdaMutex {
	GMutex  *mutex; /* internal mutex to access the structure's data */
	GCond   *cond;  /* condition to lock on */
	GThread *owner; /* current owner of the mutex, or NULL if not owned */
	short    depth;
#ifdef GDA_DEBUG_MUTEX
	gboolean debug; /* set tu %TRUE to debug this mutex */
	GArray  *usages; /* Array of GdaMutexUsage */
	gint     nb_locked_unlocked;
#endif
};

#ifdef GDA_DEBUG_MUTEX
void
gda_mutex_debug (GdaMutex *mutex, gboolean debug)
{
	g_mutex_lock (&debug_mutex);
	mutex->debug = debug;
	g_mutex_unlock (&debug_mutex);
}

void
gda_mutex_dump_usage (GdaMutex *mutex, FILE *stream)
{
	guint i;
	FILE *st;
	if (stream)
		st = stream;
	else
		st = stdout;

	g_mutex_lock (&debug_mutex);
	if (mutex->debug) {
		g_fprintf (st, "%s (mutex=>%p): locked&unlocked %d times\n", __FUNCTION__, mutex,
			   mutex->nb_locked_unlocked);
		for (i = mutex->usages->len; i > 0; i--) {
			GdaMutexUsage *usage;
			gint j;
			usage = &g_array_index (mutex->usages, GdaMutexUsage, i - 1);
			g_fprintf (st, "%d\t------ BEGIN GdaMutex %p usage\n", i - 1, mutex);
			g_fprintf (st, "\t%s, thread %p\n",
				   usage->usage == USAGE_LOCK ? "LOCK" : "UNLOCK",
				   usage->thread);
			for (j = 0; usage->frames[j]; j++)
				g_fprintf (st, "\t%s\n", usage->frames[j]);
			g_fprintf (st, "\t------ END GdaMutex %p usage\n", mutex);
		}
	}
	g_mutex_unlock (&debug_mutex);
}

static void
gda_mutex_usage_locked (GdaMutex *mutex)
{
	g_mutex_lock (&debug_mutex);

	if (mutex->debug) {
		GdaMutexUsage usage;
		usage.thread = g_thread_self ();
		usage.usage = USAGE_LOCK;
		
		void *array[FRAMES_SIZE];
		size_t size;
		char **strings;
		size_t i;
		
		size = backtrace (array, 10);
		strings = backtrace_symbols (array, size);
		usage.frames = g_new (gchar *, size + 1);
		usage.frames[size] = NULL;
		for (i = 0; i < size; i++)
			usage.frames[i] = g_strdup (strings[i]);
		free (strings);

		g_array_prepend_val (mutex->usages, usage);
	}
	g_mutex_unlock (&debug_mutex);
	gda_mutex_dump_usage (mutex, NULL);
}

static void
gda_mutex_usage_unlocked (GdaMutex *mutex)
{
	g_mutex_lock (&debug_mutex);
	if (mutex->debug) {
		void *array[FRAMES_SIZE];
		size_t size;
		char **strings;
		size_t i;
		gboolean matched = FALSE;
		
		size = backtrace (array, 10);
		strings = backtrace_symbols (array, size);
		
		if (mutex->usages->len > 0) {
			GdaMutexUsage *last;
			last = &g_array_index (mutex->usages, GdaMutexUsage, 0);
			if ((size > 3) &&
			    (last->usage == USAGE_LOCK) &&
			    (last->thread == g_thread_self ())) {
				for (i = 3; i < size; i++) {
					if (! last->frames[i] || (last->frames[i] &&
								  strcmp (last->frames[i], strings[i])))
						break;
				}
				if ((i == size) && ! last->frames[i]) {
					/* same stack => delete @last */
					g_strfreev (last->frames);
					g_array_remove_index (mutex->usages, 0);
					matched = TRUE;
					mutex->nb_locked_unlocked++;
				}
			}
		}
		
		if (! matched) {
			GdaMutexUsage usage;
			usage.thread = g_thread_self ();
			usage.usage = USAGE_UNLOCK;
			
			usage.frames = g_new (gchar *, size + 1);
			usage.frames[size] = NULL;
			for (i = 0; i < size; i++)
				usage.frames[i] = g_strdup (strings[i]);
			
			g_array_prepend_val (mutex->usages, usage);
		}
		free (strings);
	}
	g_mutex_unlock (&debug_mutex);
	gda_mutex_dump_usage (mutex, NULL);
}

#endif

/**
 * gda_mutex_new: (skip)
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
		static GMutex init_mutex;

		g_mutex_lock (&init_mutex);
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
                g_mutex_unlock (&init_mutex);
	}

	if (impl_status == NON_SUPPORTED) {
		GdaMutex *m;
		m = g_new0 (GdaMutex, 1);
#ifdef GDA_DEBUG_MUTEX
		m->usages = g_array_new (FALSE, FALSE, sizeof (GdaMutexUsage));
		m->debug = FALSE;
#endif
		return m;
	}
	else {
		GdaMutex *m;
		m = g_new0 (GdaMutex, 1);
		m->mutex = g_mutex_new ();
		m->cond = g_cond_new ();
		m->owner = NULL;
		m->depth = 0;
#ifdef GDA_DEBUG_MUTEX
		m->usages = g_array_new (FALSE, FALSE, sizeof (GdaMutexUsage));
		m->debug = FALSE;
#endif
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
#ifdef GDA_DEBUG_MUTEX
	gda_mutex_usage_locked (mutex);
#endif
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
	if (impl_status == RECURSIVE) {
#ifdef GDA_DEBUG_MUTEX
		gboolean retval;
		retval = g_mutex_trylock (mutex->mutex);
		if (retval)
			gda_mutex_usage_locked (mutex);
		return retval;
#else
		return g_mutex_trylock (mutex->mutex);
#endif
	}
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
#ifdef GDA_DEBUG_MUTEX
		if (retval)
			gda_mutex_usage_locked (mutex);
#endif
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
#ifdef GDA_DEBUG_MUTEX
	gda_mutex_usage_unlocked (mutex);
#endif
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
#ifdef GDA_DEBUG_MUTEX
	guint i;
	for (i = 0; i < mutex->usages->len; i++) {
		GdaMutexUsage *usage;
		usage = &g_array_index (mutex->usages, GdaMutexUsage, i);
		g_strfreev (usage->frames);
	}
#endif
	g_free (mutex);
}

#endif /* GLIB_CHECK_VERSION */
