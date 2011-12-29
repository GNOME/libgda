/*
 * Copyright (C) 2000 Reinhard MÃ¼ller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
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

#ifndef __GDA_MUTEX_H__
#define __GDA_MUTEX_H__

#include <glib.h>

G_BEGIN_DECLS

#if GLIB_CHECK_VERSION(2,31,7)
typedef GRecMutex GdaMutex;
#else
typedef struct _GdaMutex GdaMutex;
#endif

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
