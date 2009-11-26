/* GDA web provider
 * Copyright (C) 2009 The GNOME Foundation.
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

#ifndef __GDA_WEB_H__
#define __GDA_WEB_H__

/*
 * Provider name
 */
#define WEB_PROVIDER_NAME "Web"

#include <libsoup/soup.h>
#include <libgda/gda-mutex.h>
#include <libgda/gda-connection.h>
#include "../reuseable/gda-provider-reuseable.h"

/*
 * Provider's specific connection data
 */
typedef struct {
	GdaProviderReuseable *reuseable; /* pointer to GdaProviderReuseable, not inherited! */
	GdaMutex *mutex; /* protected access */

	gchar *server_id; /* PostgreSQL, MySQL, ... */
	gchar *server_version; /* native representation */

	gboolean forced_closing;
	gchar *server_base_url;
	gchar *front_url;
	gchar *worker_url;

	gchar *server_secret;
	gchar *key;
	gchar *next_challenge;

	gchar *session_id; /* as defined by the server */
	
	/* worker attributes */
	SoupSession *worker_session;
	gboolean worker_needed;
	gboolean worker_running;
	guint worker_counter; /* incremented each time the worker is run */

	/* front and others attributes */
	SoupSession *front_session;
	guint last_exec_counter; /* the worker counter which replied to the last EXEC command */
} WebConnectionData;

#endif
