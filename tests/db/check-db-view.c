/* check-db-view.c
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
#include <libgda/gda-db-view.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

typedef struct {
    GdaDbView *view;

    gchar *name;
    gchar *defstring;
    gboolean istemp;
    gboolean ifnoexist;
    gboolean replace;

    xmlDocPtr doc;
    gchar *xmlfile;
} CheckDbObject;

static void
test_db_view_name (CheckDbObject *self,
                    G_GNUC_UNUSED gconstpointer user_data)
{
  const gchar *name = NULL;

  name = gda_db_base_get_name (GDA_DB_BASE(self->view));

  g_assert_cmpstr (name, ==, "Summary");
}

static void
test_db_view_temp (CheckDbObject *self,
                    G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean istemp = FALSE;

  istemp = gda_db_view_get_istemp(self->view);

  g_assert_true (istemp);
}

static void
test_db_view_replace (CheckDbObject *self,
                       G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean replace = FALSE;

  replace = gda_db_view_get_replace (self->view);

  g_assert_true (replace);
}

static void
test_db_view_ifnoexist (CheckDbObject *self,
                         G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean ifnoexist = FALSE;

  ifnoexist = gda_db_view_get_ifnoexist (self->view);

  g_assert_true (ifnoexist);
}

static void
test_db_view_defstr (CheckDbObject *self,
                      G_GNUC_UNUSED gconstpointer user_data)
{
  const gchar *defstr = NULL;

  defstr = gda_db_view_get_defstring (self->view);

  g_assert_cmpstr (defstr, ==, "SELECT id,name FROM CUSTOMER");
}
static void
test_db_view_general (void)
{
  GdaDbView *self = gda_db_view_new ();

  g_object_unref (self);
}

static void
test_db_view_start (CheckDbObject *self,
                     G_GNUC_UNUSED gconstpointer user_data)
{
  self->doc = NULL;
  self->xmlfile = NULL;
  self->view = NULL;
  self->name = NULL;
  self->defstring = NULL;

  self->istemp = FALSE;
  self->ifnoexist = FALSE;
  self->replace = FALSE;

  const gchar *topsrcdir = g_getenv ("GDA_TOP_SRC_DIR");

  g_print ("ENV: %s\n",topsrcdir);
  g_assert_nonnull (topsrcdir);

  self->xmlfile = g_build_filename(topsrcdir,
                                   "tests",
                                   "db",
                                   "view_test.xml",NULL);

  g_assert_nonnull (self->xmlfile);

  self->doc = xmlParseFile(self->xmlfile);
  g_assert_nonnull (self->doc);

  xmlNodePtr node = xmlDocGetRootElement (self->doc);
  g_assert_nonnull (node);

  self->view = gda_db_view_new ();

  g_assert_nonnull(self->view);
  g_print("Before parse node\n");
  gboolean res = gda_db_buildable_parse_node(GDA_DB_BUILDABLE(self->view),
                                              node,NULL);
  g_print("After parse node\n");
  g_assert_true (res);
}

static void
test_db_view_finish (CheckDbObject *self,
                      G_GNUC_UNUSED gconstpointer user_data)
{
  g_free (self->xmlfile);
  g_object_unref (self->view);
  xmlFreeDoc (self->doc);
  g_free (self->name);
  g_free (self->defstring);
}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add_func ("/test-db/view-basic",
                   test_db_view_general);

  g_test_add ("/test-db/view-name",
              CheckDbObject,
              NULL,
              test_db_view_start,
              test_db_view_name,
              test_db_view_finish);

  g_test_add ("/test-db/view-temp",
              CheckDbObject,
              NULL,
              test_db_view_start,
              test_db_view_temp,
              test_db_view_finish);

  g_test_add ("/test-db/view-ifnoexist",
              CheckDbObject,
              NULL,
              test_db_view_start,
              test_db_view_ifnoexist,
              test_db_view_finish);

  g_test_add ("/test-db/view-replace",
              CheckDbObject,
              NULL,
              test_db_view_start,
              test_db_view_replace,
              test_db_view_finish);

  g_test_add ("/test-db/view-defstr",
              CheckDbObject,
              NULL,
              test_db_view_start,
              test_db_view_defstr,
              test_db_view_finish);

  return g_test_run();
}
