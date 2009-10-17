/*
 * Copyright (C) 2009 Vivien Malerba
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

#ifndef __BROWSER_CONNECTION_PRIVATE_H__
#define __BROWSER_CONNECTION_PRIVATE_H__

#include <libgda/thread-wrapper/gda-thread-wrapper.h>

struct _BrowserConnectionPrivate {
	GdaThreadWrapper *wrapper;
	GSList           *wrapper_jobs;
	guint             wrapper_results_timer;
	GHashTable       *executed_statements; /* key = guint exec ID, value = a StatementResult pointer */

	gchar         *name;
	GdaConnection *cnc;
	gchar         *dict_file_name;
        GdaSqlParser  *parser;
        GSList        *variables; /* list of BrowserVariable pointer, owned here */

	GdaDsnInfo     dsn_info;
	GMutex        *p_mstruct_mutex;
	GdaMetaStruct *p_mstruct; /* private GdaMetaStruct: while it is being created */
	GdaMetaStruct *mstruct; /* public GdaMetaStruct: once it has been created and is no more modified */

	BrowserFavorites *bfav;

	gboolean  busy;
	gchar    *busy_reason;
};

void browser_connection_set_busy_state (BrowserConnection *bcnc, gboolean busy, const gchar *busy_reason);

#endif
