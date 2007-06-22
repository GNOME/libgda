/* GDA Berkeley-DB Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Laurent Sansonetti <lrz@gnome.org>
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
#include <stdlib.h>
#include <string.h>
#include "gda-bdb.h"

GdaConnectionEvent *
gda_bdb_make_error (int ret)
{
	GdaConnectionEvent *error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);

	gda_connection_event_set_description (error, db_strerror (ret));
	gda_connection_event_set_code (error, -1);
	gda_connection_event_set_source (error, "gda-bdb");
	
	return error;
}
