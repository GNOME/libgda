#include <glib.h>
#include "gda-sql-delimiter.h"

static void test_string (const gchar *str);
int 
main (int argc, char **argv) {
	test_string ("SELECT 'string with a ;' i=10; INSERT hello;");
	test_string ("UPDATE contacts SET name_first = 'Murray;' WHERE contact_id = 0");
	test_string ("select bar from jcw.foo where qux <> ';'");
	test_string ("select bar from jcw.foo where \\qux <> ';'");
	test_string ("select bar from jcw.foo where ; qux <> '\\;'");
	test_string ("select bar from jcw.foo where ; update qux <> '\\;'");
	test_string ("select bar from jcw.foo where \\; qux <> 'String\\;'");

	test_string ("");
	test_string (";");

	test_string ("Select field AS \"my field\" , 'sdfjk', func (sdlkfj + slkdjf) - sdlkf");
	test_string ("update kmj, sdfjk, ##[:name=\"tit;i\"] '+ ##[:name=\"tot;o\"],' ; COMMIT sdlkfj slkdjf sdlkf");
	test_string ("delete kmj, sdfjk [:name=\"titi\"] sdlkfj slkdjf sdlkf");
	test_string ("insert kmj, 'sdfjk [:name=\"titi\"]' sdlkfj slkdjf sdlkf");
	test_string ("SELECT kmj, \"sdfjk [:name=\"titi\"]\" sdlkfj slkdjf sdlkf");
	test_string ("SELECT schema.table.field || ##[:name=\"Postfix\" :type=\"varchar\"]");
	test_string ("SELECT afield; UPDATE a firld");
	test_string ("update customers set salesrep.'id'=10");
	test_string ("abcedf hdqsjh qkjhd qjkhdq ## [:type=\"integer\"]");
	test_string ("SELECT ## [:type=\"int4\"] i=10");

	
	return 0;
}

static void
test_string (const gchar *str)
{
	GList *statements;
	gchar **array;
	GError *error = NULL;

	g_print ("\n\n");
	g_print ("##################################################################\n");
	g_print ("# SQL: %s\n", str);
	g_print ("##################################################################\n");
	statements = gda_delimiter_parse_with_error (str, &error);
	if (statements) {
		GList *list = statements;
		while (list) {
			GdaDelimiterStatement *statement, *copy;
			gchar *str;

			statement = (GdaDelimiterStatement *) list->data;
			g_print ("#### STATEMENT:\n");
			gda_delimiter_display (statement);

			/*
			copy = gda_delimiter_parse_copy_statement (statement, NULL);
			g_print ("#### COPY:\n");
			str = gda_delimiter_to_string (copy);
			g_print ("%s\n", str);
			g_free (str);
			gda_delimiter_destroy (copy);
			*/

			list = g_list_next (list);
		}

		if (g_list_length (statements) > 1) {
			GdaDelimiterStatement *concat;
			gchar *str;

			concat = gda_delimiter_concat_list (statements);
			g_print ("#### CONCAT:\n");
			str = gda_delimiter_to_string (concat);
			g_print ("%s\n", str);
			g_free (str);
			gda_delimiter_destroy (concat);
		}
		gda_delimiter_free_list (statements);
	}
	else 
		g_print ("ERROR: %s\n", error && error->message ? error->message : "Unknown");

	array = gda_delimiter_split_sql (str);
	if (array) {
		g_print ("#### SPLIT:\n");
		gint i;
		gchar **ptr;

		ptr = array;
		i = 0;
		while (*ptr) {
			g_print ("%2d: %s\n", i++, *ptr);
			ptr ++;
		}
		g_strfreev (array);
	}
}
