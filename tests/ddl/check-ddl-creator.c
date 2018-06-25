/* check-ddl-creato.c
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
} CheckDdlObject;

static void
test_ddl_creato_start (CheckDdlObject *self,
                     gconstpointer user_data)
{
  gda_init();
  self->xmlfile = NULL;
  self->creator = NULL;
  self->cnc = NULL;

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

}

static void
test_ddl_creato_finish (CheckDdlObject *self,
                      gconstpointer user_data)
{
  g_free (self->xmlfile);
  gda_ddl_creator_free (self->creator);
  gda_connection_close(self->cnc,NULL);
  g_object_unref (self->cnc);
}

static void
test_ddl_creato_parse_xml (CheckDdlObject *self,
                         gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_parse_file_from_path(self->creator,
                                                       self->xmlfile,
                                                       NULL);

  g_assert_true (res);
  
  const GList *tables = NULL;
  tables = gda_ddl_creator_get_tables(self->creator);
  g_assert_nonnull (tables);

  const gchar *name = gda_ddl_base_get_name(GDA_DDL_BASE(tables->data));
  g_assert_nonnull(name);
  g_assert_cmpstr (name, ==, "products");
}

static void
test_ddl_creato_create_db (CheckDdlObject *self,
                           gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_parse_file_from_path(self->creator,
                                                      self->xmlfile,
                                                      NULL);

  g_assert_true (res);
  
  gboolean resop = gda_ddl_creator_perform_operation(self->creator,
                                                     self->cnc,
                                                     NULL);  

  g_assert_true (resop);

  

}

static void
test_ddl_creato_parse_cnc (CheckDdlObject *self,
                           gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_parse_cnc(self->creator,self->cnc,NULL);

  g_assert_true (res);

  

}
gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-ddl/creator-name",
              CheckDdlObject,
              NULL,
              test_ddl_creato_start,
              test_ddl_creato_parse_xml,
              test_ddl_creato_finish);

  g_test_add ("/test-ddl/creator-create-db",
              CheckDdlObject,
              NULL,
              test_ddl_creato_start,
              test_ddl_creato_create_db,
              test_ddl_creato_finish);

  g_test_add ("/test-ddl/creator-parse-cnc",
              CheckDdlObject,
              NULL,
              test_ddl_creato_start,
              test_ddl_creato_parse_cnc,
              test_ddl_creato_finish);
  return g_test_run();
}
