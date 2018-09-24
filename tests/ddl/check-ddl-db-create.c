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
#include <sql-parser/gda-sql-parser.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

typedef struct {
  GdaDdlCreator *creator;
  GdaConnection *cnc;
  gchar *xmlfile;


}CheckCreatedb;

static void
test_ddl_db_create_start(CheckCreatedb *self,
                         gconstpointer user_data)
{
  gda_init ();
  self->creator = NULL;
  self->cnc = NULL;

  const gchar *topsrcdir = g_getenv ("GDA_TOP_SRC_DIR");

  g_print ("ENV: %s\n",topsrcdir);
  g_assert_nonnull (topsrcdir);

  self->xmlfile = g_build_filename(topsrcdir,
                                   "tests",
                                   "ddl",
                                   "check_db.xml",NULL);

  g_assert_nonnull (self->xmlfile);

  self->creator = gda_ddl_creator_new();

  self->cnc = gda_connection_new_from_string("SQLite",
                                             "DB_DIR=.;DB_NAME=ddl_test",
                                             NULL,
                                             GDA_CONNECTION_OPTIONS_NONE,
                                             NULL);

  g_assert_nonnull (self->cnc);

  gboolean res = gda_connection_open(self->cnc,NULL);
  g_assert_true (res);

  res = gda_ddl_creator_validate_file_from_path (self->xmlfile,NULL);
  g_assert_true (res);

  res = gda_ddl_creator_parse_file_from_path(self->creator,
                                                      self->xmlfile,
                                                      NULL);

  g_assert_true (res);
}

static void
test_ddl_db_create_finish (CheckCreatedb *self,
                           gconstpointer user_data)
{
  gda_connection_close (self->cnc, NULL);
  g_free (self->xmlfile);
  g_object_unref (self->creator);
  g_object_unref (self->cnc);
}

static void
test_ddl_db_create_test1 (CheckCreatedb *self,
                          gconstpointer user_data)
{

}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-ddl/CheckCreatedb",
              CheckCreatedb,
              NULL,
              test_ddl_db_create_start,
              test_ddl_db_create_test1,
              test_ddl_db_create_finish);
 
  return g_test_run();
}

