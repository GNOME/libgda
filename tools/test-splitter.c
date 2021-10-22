/*
 * Copyright (C) 2014 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include "common/t-utils.h"

typedef struct {
	gchar *commands;
	gchar *exp_result[];
} TestData;

TestData td0 = {
	"\\cmde \\\\\"and arg\"",
	{"\\cmde \\\\\"and arg\"", NULL}
};
TestData td1 = {
	".d\nSELECT * from table;\n.cmd \"arg\narg more\"",
	{".d", "SELECT * from table;\n", ".cmd \"arg\narg more\"", NULL}
};
TestData td2 = {
	".cmde and arg",
	{".cmde and arg", NULL}
};
TestData td3 = {
	"\\cmde \"and arg",
	{NULL}
};
TestData td4 = {
	".cmde and arg\n\\othercommand",
	{".cmde and arg", "\\othercommand", NULL}
};
TestData td5 = {
	"\\cmde \"and arg\"",
	{"\\cmde \"and arg\"", NULL}
};
TestData td6 = {
	"\\cmde \\\"and arg\"",
	{NULL}
};
TestData td7 = {
	"\\cmde \\\\\"and arg",
	{NULL}
};
TestData td8 = {
	".d",
	{".d", NULL}
};
TestData td9 = {
	"    ",
	{NULL}
};


TestData *data[] = {&td0, &td1, &td2, &td3, &td4, &td5, &td6, &td7, &td8, &td9, NULL};

int
main (int argc, char * argv[])
{
	gchar **parts;
	GError *error = NULL;
	guint nb, nfailed = 0;

	for (nb = 0; data[nb]; nb++) {
		TestData *td;
		td = data[nb];
		g_print ("== Test %u ==\n", nb);
		parts = t_utils_split_text_into_single_commands (NULL, td->commands, &error);
		if (parts) {
			guint i;
			for (i = 0; td->exp_result[i] && parts[i]; i++) {
				if (strcmp (parts[i], td->exp_result[i])) {
					g_print ("Error: got [%s] and expected [%s]\n", parts[i], td->exp_result[i]);
					nfailed ++;
				}
			}
			if (td->exp_result[i] || parts[i]) {
				g_print ("Error: got [%s] and expected [%s]\n", parts[i], td->exp_result[i]);
				nfailed ++;
			}

			g_strfreev (parts);
		}
		else {
			if (td->exp_result [0]) {
				g_print ("Error: got a parser error and expected [%s] at first\n", td->exp_result[0]);
				nfailed ++;
			}
		}
		g_clear_error (&error);
	}

	if (nfailed)
		g_print ("%u test(s) failed out of %u\n", nfailed, nb);
	else
		g_print ("%u tests Ok\n", nb);

	return nfailed ? 1 : 0;
}
