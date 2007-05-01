#include <glib.h>
#include "gda-sql-delimiter.h"

static void test_string (const gchar *str);
int 
main (int argc, char **argv) {
	test_string ("SELECT 'foobar\\\\';");
        return 0;
	test_string ("SELECT \"foobar\\\\\";");
	return 0;
	test_string ("CREATE OR REPLACE PROCEDURE cs_create_job(v_job_id IN INTEGER) IS\n"
"    a_running_job_count INTEGER;\n"
"    PRAGMA AUTONOMOUS_TRANSACTION;\n"
"BEGIN\n"
"    LOCK TABLE cs_jobs IN EXCLUSIVE MODE;\n"
"\n"
"    SELECT count(*) INTO a_running_job_count FROM cs_jobs WHERE end_stamp IS NULL;\n"
"\n"
"    IF a_running_job_count > 0 THEN\n"
"        COMMIT; -- free lock\n"
"        raise_application_error(-20000, 'Unable to create a new job: a job is currently running.');\n"
"    END IF;\n"
"\n"
"    DELETE FROM cs_active_job;\n"
"    INSERT INTO cs_active_job(job_id) VALUES (v_job_id);\n"
"\n"
"    BEGIN\n"
"        INSERT INTO cs_jobs (job_id, start_stamp) VALUES (v_job_id, sysdate);\n"
"    EXCEPTION\n"
"        WHEN dup_val_on_index THEN NULL; -- don't worry if it already exists\n"
"    END;\n"
"    COMMIT;\n"
"END;");
	return 0;
	test_string ("SELECT id FROM table where id IS NULL; SELECT id FROM table where id IS NOT NULL; SELECT id FROM table where id = 5;");
	return 0;
	test_string ("CREATE OR REPLACE FUNCTION cs_fmt_browser_version(v_name varchar,\n"
"                                                  v_version varchar)\n"
"RETURN varchar IS\n"
"int val1;\n"
"int val2;\n"
"BEGIN\n"
"    IF v_version IS NULL THEN\n"
"        RETURN v_name;\n"
"    END IF;\n"
"    RETURN v_name || '/' || v_version;\n"
"END;\n"
"/\n"
"show errors");
	return 0;
	test_string ("CREATE OR REPLACE PROCEDURE cs_update_referrer_type_proc IS\n"
"    CURSOR referrer_keys IS\n"
"        SELECT * FROM cs_referrer_keys\n"
"        ORDER BY try_order;\n"
"\n"
"    func_cmd VARCHAR(4000);\n"
"BEGIN\n"
"    func_cmd := 'CREATE OR REPLACE FUNCTION cs_find_referrer_type(v_host IN VARCHAR,\n"
"                 v_domain IN VARCHAR, v_url IN VARCHAR) RETURN VARCHAR IS BEGIN';\n"
"\n"
"    FOR referrer_key IN referrer_keys LOOP\n"
"        func_cmd := func_cmd ||\n"
"          ' IF v_' || referrer_key.kind\n"
"          || ' LIKE ''' || referrer_key.key_string\n"
"          || ''' THEN RETURN ''' || referrer_key.referrer_type\n"
"          || '''; END IF;';\n"
"    END LOOP;\n"
"\n"
"    func_cmd := func_cmd || ' RETURN NULL; END;';\n"
"\n"
"    EXECUTE IMMEDIATE func_cmd;\n"
"END;\n"
"/\n"
"show errors");
	return 0;
	test_string ("CREATE OR REPLACE TRIGGER aff_discount BEFORE INSERT OR UPDATE ON clients FOR EACH ROW WHEN (new.no_cli > 0)\n"
"DECLARE evol_discount number;\n"
"BEGIN evol_discount := :new.discount - :old.discount;\n"
"DBMS_OUTPUT.PUT_LINE(' evolution : ' || evol_discount); END;");
	return 0;
	test_string ("CREATE OR REPLACE TRIGGER aff_discount BEFORE INSERT OR UPDATE ON clients FOR EACH ROW WHEN (new.no_cli > 0)\n"
"DECLARE evol_discount number;\n"
"BEGIN evol_discount := :new.discount - :old.discount;\n"
"DBMS_OUTPUT.PUT_LINE(' evolution : ' || evol_discount); END;\n"
"CREATE FUNCTION lit_solde (no IN NUMBER)\n"
"RETURN NUMBER\n"
"IS solde NUMBER (11,2);\n"
"BEGIN\n"
"SELECT balance INTO solde FROM compte\n"
"WHERE no_compte = no;\n"
"RETURN (solde);\n"
"END");
	return 0;
	test_string ("$$Diannes; horse$$");
	test_string ("$SomeTag$Diannes; horse$SomeTag$");
	return 0;
	test_string ("CREATE OR REPLACE FUNCTION increment(i integer) RETURNS integer AS $$\n"
"        BEGIN\n"
"                RETURN i + 1;\n"
"        END;\n"
		     "$$ LANGUAGE plpgsql;");
	return 0;
	test_string ("CREATE sequence \"ACTOR_SEQ\";\n"
"  	begin;\n"
"\nCREATE trigger \"BI_ACTOR\"\n"
"  before insert on \"ACTOR\"\n"
"  for each row\n"
"begin\n"
"    select \"ACTOR_SEQ\".nextval into :NEW.ACTOR_ID from dual;\n"
"	end;\n"
"ROLLBACK;");

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
