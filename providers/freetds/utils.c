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

#include <config.h>
#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include "gda-freetds.h"

GdaError *
gda_freetds_make_error (gchar *message)
{
	GdaError *error;

	error = gda_error_new ();
	if (error) {
		if (message) {
			gda_error_set_description (error, message)
		} else {
			gda_error_set_description (error, _("NO DESCRIPTION"));
		}
		gda_error_set_number (error, -1);
	        gda_error_set_source (error, "gda-freetds");
	        gda_error_set_sqlstate (error, _("Not available"));
	}
		
	return error;
}

