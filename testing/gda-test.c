/***********************************************************************/
/*                                                                     */
/*  GNU Data Access                                                    */
/*  Testing program                                                    */
/*  (c) 2000 Free Software Foundation                                  */
/*                                                                     */
/***********************************************************************/
/*                                                                     */
/*  This program is free software; you can redistribute it and/or      */
/*  modify it under the terms of the GNU General Public License as     */
/*  published by the Free Software Foundation; either version 2 of     */
/*  the License, or (at your option) any later version.                */
/*                                                                     */
/*  This program is distributed in the hope that it will be useful,    */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of     */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      */
/*  GNU General Public License for more details.                       */
/*                                                                     */
/*  You should have received a copy of the GNU General Public License  */
/*  along with this program; if not, write to the Free Software        */
/*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.          */
/*                                                                     */
/***********************************************************************/

#include "config.h"
#include <glib.h>
#include <orb/orb.h>
#include <liboaf/liboaf.h>
#include <gda-common.h>
#include <gda-client.h>

static CORBA_ORB orb;

/* ------------------------------------------------------------------------- */
/* Print intro messages
/* ------------------------------------------------------------------------- */
void
intro ()
{
  g_print ("GDA-Test version %s\n", VERSION);
  g_print ("Copyright (c) 2000 Free Software Foundation, Inc.\n");
  g_print ("This program is part of GNU Data Access (GDA) version %s.\n",
	   VERSION);
  g_print ("GDA-Test comes with NO WARRANTY, ");
  g_print ("to the extent permitted by law.\n");
  g_print ("You may redistribute copies of ");
  g_print ("GDA-Test under the terms of the GNU\n");
  g_print ("General Public License. ");
  g_print ("For more information see the file COPYING.\n");
}

/* ------------------------------------------------------------------------- */
/* Print errors and exit program
/* ------------------------------------------------------------------------- */
int
die (Gda_Connection* cnc)
{
  GList* errors;
  GList* node;
  Gda_Error* error;

  errors = gda_connection_get_errors (cnc);
  for (node = g_list_first (errors); node; node = g_list_next (node))
    {
      error = (Gda_Error*) node->data;
      g_print ("%s\n", gda_error_description (error));
    }
  gda_error_list_free (errors);
  exit (1);
}

/* ------------------------------------------------------------------------- */
/* List all providers
/* ------------------------------------------------------------------------- */
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
      g_print ("\n*** Error ***\n");
      g_print ("There are no GDA providers available.\n");
      g_print ("If you installed libgda from a RPM, a DEB, or a Linux\n");
      g_print ("Distribution, you should install one of the providers,\n");
      g_print ("which you can get from the same source as you got libgda.\n");
      g_print ("If you built libgda yourself, you should run ./configure\n");
      g_print ("with one of the options that enable the providers (run\n");
      g_print ("./configure --help for details) and then make and install\n");
      g_print ("again.\n");
      g_print ("If you already installed a provider, the .oafinfo file is\n");
      g_print ("maybe not installed in the right directory.\n");
      exit (1);
    }

  g_print ("\nThe following %d GDA providers are available:\n",
	   g_list_length (list));
  for (node = g_list_first (list); node; node = g_list_next (node))
    {
      provider = (Gda_Provider*) node->data;
      g_print ("%d: %s\n", ++i, GDA_PROVIDER_NAME (provider));
      if (i = 1)
	selected = g_strdup (GDA_PROVIDER_NAME (provider));
    }
  gda_provider_free_list (list);
  return (selected);
}

/* ------------------------------------------------------------------------- */
/* List all tables for a connection
/* ------------------------------------------------------------------------- */
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

/* ------------------------------------------------------------------------- */
/* Main function
/* ------------------------------------------------------------------------- */
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

  intro ();
  g_print ("\ninitializing...\n");
  gtk_init (&argc, &argv);
  orb = oaf_init (argc, argv);
  cnc = gda_connection_new (orb);
  provider = list_providers ();
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
