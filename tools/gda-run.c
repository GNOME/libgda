/* GDA - Batch execution utility
 * Copyright (c) 2000-2001 by Rodrigo Moya
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>
#include <libgda/libgda.h>
#include <popt.h>

static void
usage (void)
{
	g_print("usage: gda-run -d dsn [ -u user ] [ -p password ] -f file\n");
	exit(-1);
}

int
main (int argc, char *argv[])
{
	return 0;
}
