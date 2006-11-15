#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gda-sql-transaction-parser.h"
#include "gda-sql-transaction-tree.h"

gchar *
dump_transaction (GdaSqlTransaction *trans)
{
	GString *string;
	gchar *str;

	string = g_string_new ("");
	switch (trans->trans_type) {
	case GDA_SQL_TRANSACTION_BEGIN:
		g_string_append (string, "BEGIN");
		break;
        case GDA_SQL_TRANSACTION_COMMIT:
		g_string_append (string, "COMMIT");
		break;
        case GDA_SQL_TRANSACTION_ROLLBACK:
		g_string_append (string, "ROLLBACK");
		break;
        case GDA_SQL_TRANSACTION_SAVEPOINT_ADD:
		g_string_append (string, "SAVEPOINT");
		break;
        case GDA_SQL_TRANSACTION_SAVEPOINT_REMOVE:
		g_string_append (string, "RELEASE SAVEPOINT");
		break;
        case GDA_SQL_TRANSACTION_SAVEPOINT_ROLLBACK:
		g_string_append (string, "ROLLBACK TO SAVEPOINT");
		break;
	default:
		g_assert_not_reached ();
	}

	if (trans->trans_name) {
		g_string_append_c (string, ' ');
		g_string_append (string, trans->trans_name);
	}

	switch (trans->isolation_level) {
	case GDA_TRANSACTION_ISOLATION_UNKNOWN:
		break;
        case GDA_TRANSACTION_ISOLATION_READ_COMMITTED:
		g_string_append (string, " READ COMMITTED");
		break;
        case GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
		g_string_append (string, " READ UNCOMMITTED");
		break;
        case GDA_TRANSACTION_ISOLATION_REPEATABLE_READ:
		g_string_append (string, " REPEATABLE READ");
		break;
        case GDA_TRANSACTION_ISOLATION_SERIALIZABLE:
		g_string_append (string, " SERIALIZABLE");
		break;
	default:
		g_assert_not_reached ();
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

gboolean
test_sql (const gchar *sql, const gchar *expected)
{
	GdaSqlTransaction *result;

	result = gda_sql_transaction_parse (sql);
	if (result) {
		gchar *dump;

		dump = dump_transaction (result);
		g_print ("Parsing '%s':", sql);
		if (expected) {
			if (strcmp (expected, dump))
				g_print (" as '%s', not as expected: '%s'\n", dump, expected);
			else
				g_print (" Ok.\n");	
		}
		else
			g_print (" as '%s'\n", dump);
		g_free (dump);
		gda_sql_transaction_destroy (result);
		return TRUE;
	}
	else {
		g_print ("Error.\n");
		return FALSE;
	}
}

int
main (int argc, char *argv[])
{
	char buffer[1024];

	g_print ("SQL Transaction parser Tester\n\n");
	g_print ("Predefined PostgreSQL tests:\n");
	test_sql ("BEGIN", "BEGIN");
	test_sql ("BEGIN TRANSACTION", "BEGIN");
	test_sql ("START TRANSACTION", "BEGIN");
	test_sql ("BEGIN WORK", "BEGIN");
	test_sql ("BEGIN WORK ISOLATION LEVEL SERIALIZABLE", "BEGIN SERIALIZABLE");
	test_sql ("BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED", "BEGIN READ COMMITTED");
	test_sql ("BEGIN TRANSACTION ISOLATION LEVEL READ UNCOMMITTED", "BEGIN READ UNCOMMITTED");
	test_sql ("BEGIN WORK ISOLATION LEVEL SERIALIZABLE READ WRITE", "BEGIN SERIALIZABLE");
	test_sql ("BEGIN WORK ISOLATION LEVEL SERIALIZABLE READ ONLY", "BEGIN SERIALIZABLE");
	test_sql ("COMMIT", "COMMIT");
	test_sql ("COMMIT WORK", "COMMIT");
	test_sql ("COMMIT TRANSACTION", "COMMIT");
	test_sql ("ROLLBACK", "ROLLBACK");
	test_sql ("ROLLBACK WORK", "ROLLBACK");
	test_sql ("ROLLBACK TRANSACTION", "ROLLBACK");
	test_sql ("SAVEPOINT mysvp", "SAVEPOINT mysvp");
	test_sql ("ROLLBACK TO SAVEPOINT mysvp", "ROLLBACK TO SAVEPOINT mysvp");
	test_sql ("ROLLBACK TO mysvp", "ROLLBACK TO SAVEPOINT mysvp");
	test_sql ("ROLLBACK WORK TO SAVEPOINT mysvp", "ROLLBACK TO SAVEPOINT mysvp");
	test_sql ("ROLLBACK TRANSACTION TO mysvp", "ROLLBACK TO SAVEPOINT mysvp");

	g_print ("\nPredefined SQLite tests:\n");
	test_sql ("BEGIN DEFERRED", "BEGIN");
	test_sql ("BEGIN IMMEDIATE", "BEGIN");
	test_sql ("BEGIN EXCLUSIVE", "BEGIN");
	test_sql ("BEGIN TRANSACTION transname", "BEGIN transname");
	test_sql ("COMMIT TRANSACTION transname", "COMMIT transname");
	test_sql ("END TRANSACTION transname", "COMMIT transname");

	g_print ("\nPredefined Oracle tests:\n");
	test_sql ("COMMIT WORK", "COMMIT");
	test_sql ("COMMIT WORK COMMENT 'my comment'", "COMMIT");
	test_sql ("COMMIT COMMENT 'my comment'", "COMMIT");
	test_sql ("COMMIT WORK FORCE 'my comment'", "COMMIT");
	test_sql ("COMMIT FORCE 'my comment'", "COMMIT");
	test_sql ("ROLLBACK FORCE 'my comment'", "ROLLBACK");
	test_sql ("ROLLBACK WORK TO SAVEPOINT 'mysvp'", "ROLLBACK TO SAVEPOINT mysvp");

	g_print ("\nPredefined SqlServer tests:\n");
	test_sql ("BEGIN TRAN", "BEGIN");
	test_sql ("BEGIN TRAN transname", "BEGIN transname");
	test_sql ("BEGIN TRAN transname WITH MARK 'descr'", "BEGIN transname");
	test_sql ("COMMIT TRAN", "COMMIT");
	test_sql ("COMMIT TRAN transname", "COMMIT transname");
	test_sql ("ROLLBACK TRAN", "ROLLBACK");
	

	g_print ("\nConsole tests:\n");
	while (fgets (buffer, 1023, stdin) != NULL) {
		if (buffer[strlen (buffer) - 1] == '\n')
			buffer[strlen (buffer) - 1] = '\0';
		if (buffer[0] == 0)
			break;
		printf ("parsing: %s\n", buffer);
		test_sql (buffer, NULL);
	}

	return 0;
}
