/*
 * Test program for libgda
 */

#include <glib.h>
#include <orb/orb.h>
#include <liboaf/liboaf.h>
#include <gda-common.h>
#include <gda-client.h>

static CORBA_ORB orb;

/*
 * Print errors and exit program
 ********************************/
int
die (Gda_Connection* cnc)
{
  GList* errors;
  GList* node;
  Gda_Error* error;

  errors = gda_connection_get_errors (cnc);
  for (node = g_list_first (errors); node; node = g_list_next (errors))
    {
      error = (Gda_Error*) node->data;
      g_print ("%s\n", gda_error_description (error));
    }
  gda_error_list_free (errors);
  exit (1);
}

/*
 * List all providers
 *********************/
char*
list_providers ()
{
  GList* list;
  GList* node;
  Gda_Provider* provider;
  int i = 0;
  char* selected;

  list = gda_provider_list ();
  if (!list)
    {
      g_print ("\nNo GDA providers are available\n");
      return (NULL);
    }
  else
    {
      g_print ("\nThe following %d GDA providers are available:\n",
	       g_list_length (list));
      for (node = g_list_first (list); node; node = g_list_next (list))
	{
	  provider = (Gda_Provider*) node->data;
	  g_print ("%d: %s\n", ++i, GDA_PROVIDER_NAME (provider));
	  if (i = 1)
	    selected = g_strdup (GDA_PROVIDER_NAME (provider));
	}
      gda_provider_free_list (list);
    }
  return (selected);
}

/*
 * List all tables for a connection
 *****************************************/
void
list_tables (Gda_Connection* cnc)
{
  Gda_Recordset* rs;
  Gda_Field* field;
  gint i;

  g_print ("\nopening table schema...\n");
  rs = gda_connection_open_schema (cnc, GDA_Connection_GDCN_SCHEMA_TABLES,
				   GDA_Connection_no_CONSTRAINT);
  if (!rs) die (cnc);
  g_print ("\nThis database has following tables:\n");
  for (gda_recordset_move_first (rs); !gda_recordset_eof (rs);
       gda_recordset_move_next (rs))
    {
      for (i = 0; i < gda_recordset_rowsize (rs); i++)
	{
	  field = gda_recordset_field_idx (rs, i);
	  g_print ("%s=%s\t", gda_field_name (field),
		   gda_stringify_value (NULL, 0, field));
	}
      g_print ("\n");
    }
  gda_recordset_free (rs);
}

/*
 * Main function
 ****************/
int
main (int argc, char* argv[])
{
  gchar* provider;
  Gda_Connection* cnc;
  gchar* dsn = NULL;
  gchar* user = NULL;
  gchar* password = NULL;
  gint dummy;
  gint length;

  g_print ("\n*** GDA test program ***\n");
  g_print ("\ninitializing...\n");
  gtk_init (&argc, &argv);
  orb = oaf_init (argc, argv);
  provider = list_providers ();
  g_print ("\ncreating connection object...\n");
  cnc = gda_connection_new (orb);
  g_print ("\nchoosing %s...\n", provider);
  gda_connection_set_provider (cnc, provider);
  g_print ("\nPlease enter dsn (like 'DATABASE=test'): ");
  length = getline (&dsn, &dummy, stdin);
  dsn[length-1] = 0; /* remove \n at the end of the string */
  g_print ("Please enter user name: ");
  length = getline (&user, &dummy, stdin);
  user[length-1] = 0;
  g_print ("Please enter password: ");
  length = getline (&password, &dummy, stdin);
  password[length-1] = 0;
  g_print ("\nopening connection...\n");
  !gda_connection_open (cnc, dsn, user, password) || die (cnc);
  list_tables (cnc);
  g_print ("\nclosing connection...\n", provider);
  gda_connection_free (cnc);
  return (0);
}
