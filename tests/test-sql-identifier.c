/* 
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <libgda/sql-parser/gda-sql-parser.h>

typedef struct {
	gchar *sql_identifier;
	gboolean need_quotes;
} ATest;

ATest tests[] = {
	{"\"Solution\"", TRUE},
	{"mytable", FALSE},
	{"MYTABLE", FALSE},
	{"MyTable", TRUE},
	{"my.blob", TRUE},
	{"_table", FALSE},
	{"$table", FALSE},
	{"#table", FALSE},
	{"8table", TRUE},
	{"t8ble", FALSE},
	{"t8ble_1", FALSE},
	{"t8ble_A", TRUE},
	{"T8BLE_A", FALSE},
	{"T8BLE_a", TRUE},
};

int
main (int argc, char** argv)
{
	gint i, nfailed = 0;
	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		ATest *test = &(tests [i]);
		if (gda_sql_identifier_needs_quotes (test->sql_identifier) != test->need_quotes) {
			g_print ("Failed for %s: reported %s\n", test->sql_identifier,
				 test->need_quotes ? "no quotes needed" : "quotes needed");
			nfailed++;
		}
	}

	g_print ("%d tests executed, ", i);
	if (nfailed > 0)
		g_print ("%d failed\n", nfailed);
	else
		g_print ("Ok\n");
	return EXIT_SUCCESS;
}
