/* GDA firebird provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
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
