/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#include "gda-server.h"
#include "gda-server-private.h"

/**
 * gda_server_error_make
 */
void
gda_server_error_make (GdaError *error,
		       GdaServerRecordset *recset,
		       GdaServerConnection *cnc,
		       gchar *where)
{
	GdaServerConnection* cnc_to_use = NULL;

	g_return_if_fail(error != NULL);

	if (cnc)
		cnc_to_use = cnc;
	else if (recset)
		cnc_to_use = recset->cnc;

	if (!cnc_to_use) {
		gda_log_message(_("Could not get pointer to server implementation"));
		return;
	}

	g_return_if_fail(cnc_to_use->server_impl != NULL);
	g_return_if_fail(cnc_to_use->server_impl->functions.error_make != NULL);

	cnc_to_use->server_impl->functions.error_make(error, recset, cnc, where);

	gda_server_connection_add_error(cnc_to_use, error);
}
