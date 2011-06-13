/*
 * Copyright (C) 2008 Vivien Malerba <malerba@gnome-db.org>
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
	GdaConnectionEvent *error;
        gchar *description;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

        error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
        gda_connection_event_set_code (error, isc_sqlcode (cdata->status));
        description = fb_sqlerror_get_description (cdata, statement_type);
        gda_connection_event_set_source (error, "[GDA Firebird]");
        gda_connection_event_set_description (error, description);
        gda_connection_add_event (cnc, error);
        g_free (description);

        return error;
}
