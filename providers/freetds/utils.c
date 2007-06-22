/* GNOME DB FreeTDS Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <tds.h>
#include "gda-freetds.h"

GdaConnectionEvent *
gda_freetds_make_error (TDSSOCKET *tds, const gchar *message)
{
	GdaConnectionEvent *error;

	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	if (error) {
		if (message) {
			gda_connection_event_set_description (error, message);
		} else {
			gda_connection_event_set_description (error, _("NO DESCRIPTION"));
		}
		gda_connection_event_set_code (error, -1);
	        gda_connection_event_set_source (error, "gda-freetds");
	}
		
	return error;
}

gchar
**gda_freetds_split_commandlist(const gchar *cmdlist)
{
	const gchar esc = '\\';
	
	GSList *str_list = NULL, *akt_ptr = NULL;
	gchar **arr = NULL, *str = NULL;

	guint cnt = 0, akt_pos = 0, last_pos = 0;
	gint single_quotes = 0;

	g_return_val_if_fail(cmdlist != NULL, NULL);

	while (akt_pos < strlen(cmdlist)) {
		/* assume \ is escape character */
		if ((akt_pos == 0) || (cmdlist[akt_pos - 1] != esc)) {
			if ((single_quotes == 0) & (cmdlist[akt_pos] == ';')) {
				if (akt_pos > last_pos) {
					str = g_strndup((gchar *) (cmdlist + last_pos),
					                akt_pos - last_pos);
					cnt++;
					str_list = g_slist_prepend(str_list, str);
				}
				last_pos = akt_pos + 1;
			}
			/* semicolons in quotes will not split as they are */
			/* part of the query */
			if (cmdlist[akt_pos] == '\'') {
				single_quotes = 1 - single_quotes;
			}
		}
		akt_pos++;
	}
	if (last_pos < strlen(cmdlist)) {
		str = g_strndup((gchar *) (cmdlist + last_pos),
		                strlen(cmdlist) - last_pos);
		cnt++;
	}
	arr = g_new0(gchar *, cnt + 1);
	arr[cnt--] = NULL;

	for (akt_ptr = str_list; akt_ptr; akt_ptr = akt_ptr->next) {
		arr[cnt--] = akt_ptr->data;
	}

	g_slist_free (str_list);

	return arr;
}
