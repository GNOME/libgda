/* GDA - Test suite
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <stdlib.h>
#include "gda-test.h"

static void
run_gda_test (gpointer user_data)
{
	test_config ();
	test_values ();
	test_models ();
	test_client ();

	gda_main_quit ();
}

int
main(int argc, char *argv[])
{
	gda_init ("gda-test", VERSION, argc, argv);

	gda_main_run ((GdaInitFunc) run_gda_test, NULL);

	return (0);
}
