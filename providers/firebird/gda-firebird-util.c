/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include "gda-firebird-util.h"

GdaConnectionEvent *
_gda_firebird_make_error (GdaConnection *cnc, const gint statement_type)
{
	FirebirdConnectionData *cdata;
	GdaConnectionEvent *error_ev;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return NULL;

	error_ev = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
	gda_connection_event_set_code (error_ev, isc_sqlcode (cdata->status));
	ISC_SCHAR *description;
	const ISC_STATUS *p = cdata->status;

	description = g_new0 (ISC_SCHAR, 512);
	fb_interpret (description, 511, &p);
	g_print ("MAKE_ERROR [%s]\n", description);
	gda_connection_event_set_source (error_ev, "[GDA Firebird]");
	gda_connection_event_set_description (error_ev, description);
	gda_connection_add_event (cnc, error_ev);
	g_free (description);

	return error_ev;
}
