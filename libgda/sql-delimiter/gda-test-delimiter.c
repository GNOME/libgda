#include <glib.h>
#include "gda-sql-delimiter.h"

static gboolean test_string (const gchar *str);
int 
main (int argc, char **argv) {
	test_string ("Select field AS \"my field\" , 'sdfjk', func (sdlkfj + slkdjf) - sdlkf");
	test_string ("update kmj, sdfjk, ##[:name=\"titi\"] + ##[:name=\"toto\"], sdlkfj slkdjf sdlkf");
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

static gboolean
test_string (const gchar *str)
{
	GdaDelimiterStatement *statement, *copy;

	g_print ("\n\n");
	g_print ("##################################################################\n");
	g_print ("# SQL: %s\n", str);
	g_print ("##################################################################\n");
	statement = gda_delimiter_parse (str);
	if (statement) {
		gda_delimiter_display (statement);
		copy = gda_delimiter_parse_copy_statement (statement, NULL);
		gda_delimiter_destroy (statement);
		g_print ("#### COPY:\n");
		gda_delimiter_display (copy);
		gda_delimiter_destroy (copy);
		return TRUE;
	}
	else {
		g_print ("ERROR\n");
		return FALSE;
	}
}
