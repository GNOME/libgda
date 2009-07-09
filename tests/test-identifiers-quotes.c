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

#include <string.h>
#include <libgda/libgda.h>

typedef struct {
	gchar *sql_identifier;
	gchar *provider;
	gboolean meta_store_convention;
	gboolean force_quotes;
	gchar *expected_result;
} ATest;

ATest tests[] = {
	/* generic SQL */
	{"\"Solution\"", NULL, FALSE, FALSE, "\"Solution\""},
	{"\"Solution\"", NULL, FALSE, TRUE, "\"Solution\""},
	{"\"solution\"", NULL, FALSE, FALSE, "\"solution\""},
	{"\"solution\"", NULL, FALSE, TRUE, "\"solution\""},
	{"solution", NULL, FALSE, FALSE, "solution"},
	{"solution", NULL, FALSE, TRUE, "\"solution\""},
	{"solution", NULL, TRUE, FALSE, "solution"},
	{"solution", NULL, TRUE, TRUE, "\"solution\""},

	{"SOLUTION", NULL, FALSE, TRUE, "\"SOLUTION\""},
	{"SOLUTION", NULL, FALSE, FALSE, "SOLUTION"},

	{"Solution", NULL, FALSE, FALSE, "Solution"}, /* mixed case */
	{"Solution", NULL, FALSE, TRUE, "\"Solution\""},

	{"5olution", NULL, FALSE, FALSE, "\"5olution\""}, /* test 1st digit */
	{"5olution", NULL, FALSE, TRUE, "\"5olution\""},
	{"5olution", NULL, TRUE, FALSE, "\"5olution\""}, /* this should not happen, but just in case */
	{"5olution", NULL, TRUE, TRUE, "\"5olution\""}, /* this should not happen, but just in case */

	{"$solu_t#ion", NULL, FALSE, FALSE, "$solu_t#ion"}, /* other allowed characters */
	{"$solu_t#ion", NULL, FALSE, TRUE, "\"$solu_t#ion\""},
	{"$solu_t#ion", NULL, TRUE, FALSE, "$solu_t#ion"},
	{"$solu_t#ion", NULL, TRUE, TRUE, "\"$solu_t#ion\""},
	
	{"solu tion", NULL, FALSE, FALSE, "\"solu tion\""}, /* non allowed characters */
	{"solu tion", NULL, FALSE, TRUE, "\"solu tion\""},
	{"solu tion", NULL, TRUE, FALSE, "\"solu tion\""},
	{"solu tion", NULL, TRUE, TRUE, "\"solu tion\""},

	{"select", NULL, FALSE, FALSE, "\"select\""}, /* SQL reserved keyword */
	{"select", NULL, FALSE, TRUE, "\"select\""},
	{"select", NULL, TRUE, FALSE, "\"select\""},
	{"select", NULL, TRUE, TRUE, "\"select\""},

	/* MySQL specific */
	{"`Solution`", "MySQL", FALSE, FALSE, "`Solution`"},
	{"`Solution`", "MySQL", FALSE, TRUE, "`Solution`"},
	{"`solution`", "MySQL", FALSE, FALSE, "`solution`"},
	{"`solution`", "MySQL", FALSE, TRUE, "`solution`"},
	{"solution", "MySQL", FALSE, FALSE, "solution"},
	{"solution", "MySQL", FALSE, TRUE, "`solution`"},
	{"solution", "MySQL", TRUE, FALSE, "solution"},
	{"solution", "MySQL", TRUE, TRUE, "`solution`"},

	{"SOLUTION", "MySQL", FALSE, TRUE, "`SOLUTION`"},
	{"SOLUTION", "MySQL", FALSE, FALSE, "SOLUTION"},

	{"Solution", "MySQL", FALSE, FALSE, "Solution"}, /* mixed case */
	{"Solution", "MySQL", FALSE, TRUE, "`Solution`"},

	{"5olution", "MySQL", FALSE, FALSE, "`5olution`"}, /* test 1st digit */
	{"5olution", "MySQL", FALSE, TRUE, "`5olution`"},
	{"5olution", "MySQL", TRUE, FALSE, "`5olution`"}, /* this should not happen, but just in case */
	{"5olution", "MySQL", TRUE, TRUE, "`5olution`"}, /* this should not happen, but just in case */

	{"$solu_t#ion", "MySQL", FALSE, FALSE, "$solu_t#ion"}, /* other allowed characters */
	{"$solu_t#ion", "MySQL", FALSE, TRUE, "`$solu_t#ion`"},
	{"$solu_t#ion", "MySQL", TRUE, FALSE, "$solu_t#ion"},
	{"$solu_t#ion", "MySQL", TRUE, TRUE, "`$solu_t#ion`"},
	
	{"solu tion", "MySQL", FALSE, FALSE, "`solu tion`"}, /* non allowed characters */
	{"solu tion", "MySQL", FALSE, TRUE, "`solu tion`"},
	{"solu tion", "MySQL", TRUE, FALSE, "`solu tion`"},
	{"solu tion", "MySQL", TRUE, TRUE, "`solu tion`"},

	{"select", "MySQL", FALSE, FALSE, "`select`"}, /* SQL reserved keyword */
	{"select", "MySQL", FALSE, TRUE, "`select`"},
	{"select", "MySQL", TRUE, FALSE, "`select`"},
	{"select", "MySQL", TRUE, TRUE, "`select`"},

	/* PostgreSQL specific */
	{"\"Solution\"", "PostgreSQL", FALSE, FALSE, "\"Solution\""},
	{"\"Solution\"", "PostgreSQL", FALSE, TRUE, "\"Solution\""},
	{"\"solution\"", "PostgreSQL", FALSE, FALSE, "\"solution\""},
	{"\"solution\"", "PostgreSQL", FALSE, TRUE, "\"solution\""},
	{"solution", "PostgreSQL", FALSE, FALSE, "solution"},
	{"solution", "PostgreSQL", FALSE, TRUE, "\"solution\""},
	{"solution", "PostgreSQL", TRUE, FALSE, "solution"},
	{"solution", "PostgreSQL", TRUE, TRUE, "\"solution\""},

	{"SOLUTION", "PostgreSQL", FALSE, TRUE, "\"SOLUTION\""},
	{"SOLUTION", "PostgreSQL", FALSE, FALSE, "SOLUTION"},

	{"Solution", "PostgreSQL", FALSE, FALSE, "Solution"}, /* mixed case */
	{"Solution", "PostgreSQL", FALSE, TRUE, "\"Solution\""},

	{"5olution", "PostgreSQL", FALSE, FALSE, "\"5olution\""}, /* test 1st digit */
	{"5olution", "PostgreSQL", FALSE, TRUE, "\"5olution\""},
	{"5olution", "PostgreSQL", TRUE, FALSE, "\"5olution\""}, /* this should not happen, but just in case */
	{"5olution", "PostgreSQL", TRUE, TRUE, "\"5olution\""}, /* this should not happen, but just in case */

	{"$solu_t#ion", "PostgreSQL", FALSE, FALSE, "$solu_t#ion"}, /* other allowed characters */
	{"$solu_t#ion", "PostgreSQL", FALSE, TRUE, "\"$solu_t#ion\""},
	{"$solu_t#ion", "PostgreSQL", TRUE, FALSE, "$solu_t#ion"},
	{"$solu_t#ion", "PostgreSQL", TRUE, TRUE, "\"$solu_t#ion\""},
	
	{"solu tion", "PostgreSQL", FALSE, FALSE, "\"solu tion\""}, /* non allowed characters */
	{"solu tion", "PostgreSQL", FALSE, TRUE, "\"solu tion\""},
	{"solu tion", "PostgreSQL", TRUE, FALSE, "\"solu tion\""},
	{"solu tion", "PostgreSQL", TRUE, TRUE, "\"solu tion\""},

	{"select", "PostgreSQL", FALSE, FALSE, "\"select\""}, /* SQL reserved keyword */
	{"select", "PostgreSQL", FALSE, TRUE, "\"select\""},
	{"select", "PostgreSQL", TRUE, FALSE, "\"select\""},
	{"select", "PostgreSQL", TRUE, TRUE, "\"select\""},
};

int
main (int argc, char** argv)
{
	gda_init ();
	gint i, nfailed = 0;
	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		ATest *test = &(tests [i]);
		gchar *result;
		GdaServerProvider *prov = NULL;
		gboolean fail = FALSE;
		if (test->provider) {
			prov = gda_config_get_provider (test->provider, NULL);
			if (!prov) {
				g_print ("Can't find provider for %s, ignoring test\n", test->provider);
				continue;
			}
		}
		result = gda_sql_identifier_quote (test->sql_identifier, NULL, prov, test->meta_store_convention,
						   test->force_quotes);
		if (!result) {
			if (test->expected_result) {
				g_print ("Failed for %s: result is NULL when %s was expected\n",
					 test->sql_identifier,
					 test->expected_result);
				fail = TRUE;
			}
		}
		else {
			if (!test->expected_result) {
				g_print ("Failed for %s: result is %s when NULL was expected\n",
					 test->sql_identifier,
					 result);
				fail = TRUE;
			}
			else if (strcmp (result, test->expected_result)) {
				g_print ("Failed for %s: result is %s when %s was expected\n",
					 test->sql_identifier,
					 result, test->expected_result);
				fail = TRUE;
			}
			g_free (result);
		}

		if (fail) {
			g_print ("\tprovider: %s\n", test->provider);
			g_print ("\tmeta store convention: %s\n", test->meta_store_convention ? "Yes" : "No");
			g_print ("\tforce quotes: %s\n", test->force_quotes ? "Yes" : "No");
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
