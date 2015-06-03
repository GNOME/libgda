/*
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@src.gnome.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2002 Tim Coleman <tim@timcoleman.com>
 * Copyright (C) 2003 Steve Fosdick <fozzy@src.gnome.org>
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_WEB_H__
#define __GDA_WEB_H__

/*
 * Provider name
 */
#define WEB_PROVIDER_NAME "Web"

#include <libsoup/soup.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-connection-private.h>
#include "../reuseable/gda-provider-reuseable.h"

/*
 * Provider's specific connection data
 */
typedef struct {
	GdaServerProviderConnectionData parent;
	GdaProviderReuseable *reuseable; /* pointer to GdaProviderReuseable, not inherited! */
	GRecMutex mutex; /* protected access */

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
