/* GNOME-DB - Batch execution utility
 * Copyright (c) 2000 by Rodrigo Moya
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

#include <config.h>
#include <gda-client.h>
#include <popt.h>

#define _(s)  (s)
#define N_(s) (s)

static CORBA_ORB         orb;
static CORBA_Environment ev;
static GdaConnection*   cnc;
static gchar*            datasource = 0;
static gchar*            username = 0;
static gchar*            password = 0;
static gchar*            file = 0;
//static struct poptOption options[] = {
//  { "dsn", 'd', POPT_ARG_STRING, &datasource, 0, N_("database name"), N_("DATABASE")},
//  { "user",     'u', POPT_ARG_STRING, &username, 0, N_("user name"), N_("USER-NAME")},
//  { "password", 'p', POPT_ARG_STRING, &password, 0, N_("password"), N_("PASSWORD")},
//  { "file",     'f', POPT_ARG_STRING, &file,     0, N_("file name"), N_("FILE-NAME")},
//  {0, 0, 0, 0, 0, 0, 0}
//};

void
Exception (CORBA_Environment *ev)
{
  switch(ev->_major)
    {
    case CORBA_SYSTEM_EXCEPTION :
      fprintf(stderr, "CORBA system exception %s.\n", CORBA_exception_id(ev));
      exit(1);
    case CORBA_USER_EXCEPTION :
      fprintf(stderr, "CORBA user exception: %s.\n", CORBA_exception_id(ev));
      exit(1);
    default :
      break;
    }
}

static void
cnc_error_cb (GdaConnection *cnc, GList *errors, gpointer data)
{
  GList* node;

  g_return_if_fail(GDA_IS_CONNECTION(cnc));

  node = g_list_first(errors);
  while (node)
    {
      GdaError* err = GDA_ERROR(node->data);
      if (err)
        {
          fprintf(stderr, "%s: error: %s\n", g_get_prgname(), gda_error_description(err));
        }
      node = g_list_next(node);
    }
}

static void
usage (void)
{
  g_print("usage: gda-run -d dsn [ -u user ] [ -p password ] -f file\n");
  exit(-1);
}

int
main (int argc, char *argv[])
{
  GdaBatch* job;
  GList*     list, *node;
  gboolean   found = FALSE;
  gchar*     real_dsn = 0;

  /* initialization */
  gda_init("gda-run", VERSION, &argc, argv);
  orb = gda_corba_get_orb();

  /* check parameters */
  if (!datasource || !file) usage();

  cnc = gda_connection_new(orb);
  list = node = gda_dsn_list();

  while (node)
    {
      if (!g_strcasecmp(datasource, GDA_DSN_GDA_NAME((GdaDsn *) node->data)))
        {
          gda_connection_set_provider(cnc, GDA_DSN_PROVIDER((GdaDsn *) node->data));
          real_dsn = g_strdup(GDA_DSN_DSN((GdaDsn *) node->data));
          if (!real_dsn)
            {
              fprintf(stderr, _("%s: misconfigured DSN entry"), argv[0]);
              exit(-1);
            }
          if (!username)
            username = g_strdup(GDA_DSN_USERNAME((GdaDsn *) node->data));
          if (!password)
            password = g_strdup("");
          found = TRUE;
          break;
        }
      node = g_list_next(node);
    }
  gda_dsn_free_list(list);
  if (!found)
    {
      fprintf(stderr, _("%s: data source '%s' not found\n"), argv[0], datasource);
      exit(-1);
    }

  /* open the connection */
  if (gda_connection_open(cnc, real_dsn, username, password) < 0)
    {
      fprintf(stderr, "%s: error: open connection failed\n", argv[0]);
      return -1;
    }
  g_print("Connected to '%s'", datasource);
#ifndef HAVE_GOBJECT /* FIXME */
  gtk_signal_connect(GTK_OBJECT(cnc), "error",
                     GTK_SIGNAL_FUNC(cnc_error_cb), 0);
#endif
  /* prepare the batch job object */
  job = gda_batch_new();
  gda_batch_set_connection(job, cnc);
  if (gda_batch_load_file(job, file, TRUE))
    {
      /* run batch job */
      if (!gda_batch_start(job))
        fprintf("%s: there were errors running transaction\n", argv[0]);
    }
  else fprintf(stderr, "%s: error loading file %s\n", argv[0], file);

  /* close the connection */
  gda_batch_free(job);
  if (gda_connection_is_open(cnc))
    {
      gda_connection_close(cnc);
    }
  gda_connection_free(cnc);
  return 0;
}
