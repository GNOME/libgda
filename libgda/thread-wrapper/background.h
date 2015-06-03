/*
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2006 - 2007 Murray Cumming <murrayc@murrayc-desktop>
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

#ifndef __BACKGROUND_H__
#define __BACKGROUND_H__

#include "gda-worker.h"
#include "itsignaler.h"

G_BEGIN_DECLS

/* IISignaler caching */
void        bg_set_spare_its (ITSignaler *its);
ITSignaler *bg_get_spare_its (void);

/* GdaWorker caching */
void        bg_set_spare_gda_worker (GdaWorker *worker);
GdaWorker  *bg_get_spare_gda_worker (void);

/* threads joining */
void        bg_join_thread ();


/* stats */
typedef enum {
	BG_STATS_MIN,

	BG_CREATED_WORKER = BG_STATS_MIN,
	BG_DESTROYED_WORKER,
	BG_CACHED_WORKER_REQUESTS,
	BG_REUSED_WORKER_FROM_CACHE,

	BG_STARTED_THREADS,
	BG_JOINED_THREADS,

	BG_CREATED_ITS,
	BG_DESTROYED_ITS,
	BG_CACHED_ITS_REQUESTS,
	BG_REUSED_ITS_FROM_CACHE,

	BG_STATS_MAX
} BackgroundStats;

void bg_update_stats (BackgroundStats type);

G_END_DECLS

#endif
