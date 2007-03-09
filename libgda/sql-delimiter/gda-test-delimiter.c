#include <glib.h>
#include "gda-sql-delimiter.h"

static void test_string (const gchar *str);
int 
main (int argc, char **argv) {
	test_string ("SELECT ## /* type:\"int4\" */ i=10");
	test_string ("SELECT 123 /* type:\"int4\" */ i=10");
	test_string ("SELECT 1.23 /* type:\"int4\" */ i=10");
	test_string ("SELECT 'a string' /* type:\"int4\" */ i=10");
	test_string ("SELECT \"a textual\" /* type:\"int4\" */ i=10");

	test_string ("SELECT * FROM \"MYTABLE\"");
	test_string ("INSERT INTO products (ref, category, name, price, wh_stored) VALUES (##/*name:'+0' type:gchararray*/,  ##/*name:'+1' type:gint*/, ##/*name:'+2' type:gchararray*/, 31.4159, .1234 /*name:'+4' type:gfloat*/)");

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
			GdaDelimiterStatement *statement;

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
