/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#undef GDA_DISABLE_DEPRECATED
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

static gboolean
identifier_needs_quotes (const gchar *str)
{
	const gchar *ptr;
	gchar icase = 0;

	g_return_val_if_fail (str, FALSE);
	for (ptr = str; *ptr; ptr++) {
		/* quote if 1st char is a number */
		if ((*ptr <= '9') && (*ptr >= '0')) {
			if (ptr == str)
				return TRUE;
			continue;
		}
		if ((*ptr >= 'A') && (*ptr <= 'Z')) {
			if (icase == 0) /* first alpha char encountered */
				icase = 'U';
			else if (icase == 'L') /* @str has mixed case */
				return TRUE;
			continue;
		}
		if ((*ptr >= 'a') && (*ptr <= 'z')) {
			if (icase == 0) /* first alpha char encountered */
				icase = 'L';
			else if (icase == 'U')
				return TRUE; /* @str has mixed case */
			continue;
		}
		if ((*ptr != '$') && (*ptr != '_') && (*ptr != '#'))
			return TRUE;
	}
	return FALSE;
}

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
{
	guint i, nfailed = 0;
	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		ATest *test = &(tests [i]);
		if (identifier_needs_quotes (test->sql_identifier) != test->need_quotes) {
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
