/* GDA Common Library
 * Copyright (C) 2000,2001 Rodrigo Moya
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

#include "gda-common-private.h"
#include "gda-common.h"
#include <bonobo-activation/bonobo-activation.h>

/**
 * gda_init
 */
void
gda_init (const gchar * app_id, const gchar * version, gint nargs,
	  gchar * args[])
{
	static gboolean initialized = FALSE;

	if (initialized) {
		gda_log_error (_
			       ("Attempt to initialize an already initialized client"));
		return;
	}

	g_type_init ();
	g_set_prgname (app_id);

	bonobo_activation_init (nargs, args);
	if (!bonobo_init (gda_corba_get_orb (), NULL, NULL))
		g_error (_("Could not initialize Bonobo"));

	initialized = TRUE;
}
