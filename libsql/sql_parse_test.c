#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sql_parser.h"
#include "mem.h"

static int
display (GList * list)
{
	for (; list != NULL; list = list->next)
		printf ("%s%s", (char *) list->data, list->next ? ", " : "");
	printf ("\n");
	return 0;
}

static void
list_free_item (void *data, void *nothing)
{
	g_free (data);
}

int
main (int argc, char *argv[])
{
	char buffer[1024];
	sql_statement *result;
	int error = 0;
	char *rebuilt;
	GList *tables;
	GList *fields;

	printf ("SQL Parse Tester\n");

	while (fgets (buffer, 1023, stdin) != NULL) {
		if (buffer[strlen (buffer) - 1] == '\n')
			buffer[strlen (buffer) - 1] = '\0';
		if (buffer[0] == 0)
			break;
		printf ("parsing: %s\n", buffer);
		result = sql_parse (buffer);
		if (result) {
			/* sql_statement_append_field (result, NULL, "newfield"); */
			rebuilt = sql_stringify (result);
			printf ("rebuilt: %s\n", rebuilt);
			free (rebuilt);

			sql_display (result);
			printf ("\n");

			tables = sql_statement_get_tables (result);
			printf ("Tables: ");
			display (tables);

			g_list_foreach (tables, list_free_item, NULL);
			g_list_free (tables);

			fields = sql_statement_get_fields (result);
			printf ("Fields: ");
			display (fields);

			g_list_foreach (fields, list_free_item, NULL);
			g_list_free (fields);

			sql_destroy (result);
		}
		else {
			error = 1;
			break;
		}
	}
	memsql_display ();

	return error;
}
