/*
 * Copyright (C) 2008 - 2011 The GNOME Foundation.
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

#ifndef __GDA_MUTEX_H__
#define __GDA_MUTEX_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GdaMutex GdaMutex;

/**
 * SECTION:gda-mutex
 * @short_description: Recursive mutex implementation
 * @title: GdaMutex
 * @stability: Stable
 * @see_also: #GdaLockable and #GMutex
 *
 * #GdaMutex implements a recursive mutex (unlike the #GMutex implementation which offers no
 * guarantee about recursiveness). A recursive mutex is a mutex which can be locked several
 * times by the same thread (and needs to be unlocked the same number of times before
 * another thread can lock it).
 *
 * A #GdaMutex can safely be used even in a non multi-threaded environment in which case
 * it does nothing.
 */

GdaMutex*   gda_mutex_new       (void);
void        gda_mutex_lock      (GdaMutex *mutex);
gboolean    gda_mutex_trylock   (GdaMutex *mutex);
void        gda_mutex_unlock    (GdaMutex *mutex);
void        gda_mutex_free      (GdaMutex *mutex);

#ifdef GDA_DEBUG_MUTEX
#include <stdio.h>
void        gda_mutex_debug      (GdaMutex *mutex, gboolean debug);
void        gda_mutex_dump_usage (GdaMutex *mutex, FILE *stream);
#endif

G_END_DECLS

#endif
