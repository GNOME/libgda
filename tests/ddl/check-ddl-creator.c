/* check-ddl-creator.c
 *
 * Copyright 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <libgda/libgda.h>
#include <libgda/gda-ddl-creator.h>
#include <libgda/gda-ddl-base.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

typedef struct {
    GdaDdlCreator *creator;
    gchar *xmlfile;
    GdaConnection *cnc;
    GFile *file;
} CheckDdlObject;

static void
test_ddl_creator_start (CheckDdlObject *self,
                     gconstpointer user_data)
{
  gda_init();
  self->xmlfile = NULL;
  self->creator = NULL;
  self->cnc = NULL;
  self->file = NULL;

  const gchar *topsrcdir = g_getenv ("GDA_TOP_SRC_DIR");

  g_print ("ENV: %s\n",topsrcdir);
  g_assert_nonnull (topsrcdir);

  self->xmlfile = g_build_filename(topsrcdir,
                                   "tests",
                                   "ddl",
                                   "ddl-db.xml",NULL);

  g_assert_nonnull (self->xmlfile);

  self->creator = gda_ddl_creator_new();
  
  self->cnc = gda_connection_new_from_string("SQLite",
                                             "DB_DIR=.;DB_NAME=ddl_test",
                                             NULL,
                                             GDA_CONNECTION_OPTIONS_NONE,
                                             NULL);

  g_assert_nonnull (self->cnc);

  gboolean openres = gda_connection_open(self->cnc,NULL);
  g_assert_true (openres);

  self->file = g_file_new_for_path (self->xmlfile); 
  g_print ("GFile is %s\n",g_file_get_path(self->file));
}

static void
test_ddl_creator_finish (CheckDdlObject *self,
                      gconstpointer user_data)
{
  gda_connection_close(self->cnc,NULL);
  g_free (self->xmlfile);
  g_object_unref (self->file);
  gda_ddl_creator_free (self->creator);
  g_object_unref (self->cnc);
}

static void
test_ddl_creator_parse_xml_path (CheckDdlObject *self,
                                gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_parse_file_from_path(self->creator,
                                                      self->xmlfile,
                                                      NULL);

  g_assert_true (res);
}

static void
test_ddl_creator_parse_xml_file (CheckDdlObject *self,
                                gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_parse_file (self->creator,self->file,NULL);
  g_assert_true (res);
}

static void
test_ddl_creator_validate_xml (CheckDdlObject *self,
                              gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_validate_file_from_path (self->xmlfile,NULL);
  g_assert_true (res);
}

static void
test_ddl_creator_create_db (CheckDdlObject *self,
                           gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_parse_file_from_path(self->creator,
                                                      self->xmlfile,
                                                      NULL);

  g_assert_true (res);
 
  res = gda_ddl_creator_write_to_path (self->creator,
                                       "ddl-test-out.xml",
                                             NULL);

  g_assert_true (res);

  GError *error = NULL;
  gboolean resop = gda_ddl_creator_perform_operation(self->creator,
                                                     self->cnc,
                                                     &error);  

  if (!resop)
    g_print ("myerr: %s\n",error && error->message ? error->message : "No default");

  g_assert_true (resop);
}

/* static void */
/* test_ddl_creator_parse_cnc (CheckDdlObject *self, */
/*                            gconstpointer user_data) */
/* { */
/*   gboolean res = gda_ddl_creator_parse_cnc(self->creator,self->cnc,NULL); */

/*   g_assert_true (res); */
/* } */

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-ddl/creator-parse-file",
              CheckDdlObject,
              NULL,
              test_ddl_creator_start,
              test_ddl_creator_parse_xml_file,
              test_ddl_creator_finish);

  g_test_add ("/test-ddl/creator-parse-path",
              CheckDdlObject,
              NULL,
              test_ddl_creator_start,
              test_ddl_creator_parse_xml_path,
              test_ddl_creator_finish);

  g_test_add ("/test-ddl/creator-create-db",
              CheckDdlObject,
              NULL,
              test_ddl_creator_start,
              test_ddl_creator_create_db,
              test_ddl_creator_finish);

  g_test_add ("/test-ddl/creator-validate-xml",
              CheckDdlObject,
              NULL,
              test_ddl_creator_start,
              test_ddl_creator_validate_xml,
              test_ddl_creator_finish);
  /*g_test_add ("/test-ddl/creator-parse-cnc",
              CheckDdlObject,
              NULL,
              test_ddl_creator_start,
              test_ddl_creator_parse_cnc,
              test_ddl_creator_finish);*/
 
  return g_test_run();
}
