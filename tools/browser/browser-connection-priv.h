/*
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __BROWSER_CONNECTION_PRIVATE_H__
#define __BROWSER_CONNECTION_PRIVATE_H__

struct _BrowserConnectionPrivate {
	GHashTable       *executed_statements; /* key = guint exec ID, value = a StatementResult pointer */

	gulong            meta_store_signal;
	gulong            transaction_status_signal;

	gchar         *name;
	GdaConnection *cnc;
	gchar         *dict_file_name;
        GdaSqlParser  *parser;

	GdaDsnInfo     dsn_info;
	GdaMetaStruct *mstruct;

	ToolsFavorites *bfav;

	gboolean  busy;
	gchar    *busy_reason;

	GdaConnection *store_cnc;

	GdaSet        *variables;
};

void browser_connection_set_busy_state (BrowserConnection *bcnc, gboolean busy, const gchar *busy_reason);

#endif
