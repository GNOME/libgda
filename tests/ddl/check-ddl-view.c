/* check-ddl-view.c
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
#include <libgda/gda-ddl-view.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

typedef struct {
    GdaDdlView *view;

    gchar *name;
    gchar *defstring;
    gboolean istemp;
    gboolean ifnoexist;
    gboolean replace;
     
    xmlDocPtr doc;
    xmlTextWriterPtr writer;
    xmlBufferPtr buffer;
    gchar *xmlfile;
} CheckDdlObject;

static void
test_ddl_view_name (CheckDdlObject *self,
                    gconstpointer user_data)
{
  const gchar *name = NULL;

  name = gda_ddl_base_get_name (GDA_DDL_BASE(self->view));

  g_assert_cmpstr (name, ==, "Summary");
}

static void
test_ddl_view_temp (CheckDdlObject *self,
                    gconstpointer user_data)
{
  gboolean istemp = FALSE;

  istemp = gda_ddl_view_get_istemp(self->view);

  g_assert_true (istemp);
}

static void
test_ddl_view_replace (CheckDdlObject *self,
                       gconstpointer user_data)
{
  gboolean replace = FALSE;

  replace = gda_ddl_view_get_replace (self->view);

  g_assert_true (replace);
}

static void
test_ddl_view_ifnoexist (CheckDdlObject *self,
                         gconstpointer user_data)
{
  gboolean ifnoexist = FALSE;

  ifnoexist = gda_ddl_view_get_ifnoexist (self->view);

  g_assert_true (ifnoexist);
}

static void
test_ddl_view_defstr (CheckDdlObject *self,
                      gconstpointer user_data)
{
  const gchar *defstr = NULL;

  defstr = gda_ddl_view_get_defstring (self->view);

  g_assert_cmpstr (defstr, ==, "SELECT id,name FROM CUSTOMER");
}
static void
test_ddl_view_general (void)
{
  GdaDdlView *self = gda_ddl_view_new ();

  gda_ddl_view_free (self);
}

static void
test_ddl_view_write_node (CheckDdlObject *self,
                          gconstpointer user_data)
{
  int res = gda_ddl_buildable_write_node(GDA_DDL_BUILDABLE(self->view),
                                         self->writer,NULL);

  g_assert_true (res >= 0);

  //	res = xmlTextWriterEndDocument (self->writer);

  //	g_assert_true (res >= 0);
  xmlFreeTextWriter (self->writer);

  g_print ("%s\n",(gchar*)self->buffer->content);
}

static void
test_ddl_view_start (CheckDdlObject *self,
                     gconstpointer user_data)
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
                                   "ddl",
                                   "view_test.xml",NULL);

  g_assert_nonnull (self->xmlfile);

  self->doc = xmlParseFile(self->xmlfile);
  g_assert_nonnull (self->doc);

  xmlNodePtr node = xmlDocGetRootElement (self->doc);
  g_assert_nonnull (node);

  self->view = gda_ddl_view_new ();

  g_assert_nonnull(self->view);
  g_print("Before parse node\n");
  gboolean res = gda_ddl_buildable_parse_node(GDA_DDL_BUILDABLE(self->view),
                                              node,NULL);
  g_print("After parse node\n");
  g_assert_true (res);

  self->buffer = xmlBufferCreate ();

  g_assert_nonnull (self->buffer);

  self->writer = xmlNewTextWriterMemory (self->buffer,0);

  g_assert_nonnull (self->writer);

  res = xmlTextWriterStartDocument (self->writer, NULL, NULL, NULL);

  g_assert_true (res >= 0);
}

static void
test_ddl_view_finish (CheckDdlObject *self,
                      gconstpointer user_data)
{
  g_free (self->xmlfile);
  gda_ddl_view_free (self->view);
  xmlFreeDoc (self->doc);
  g_free (self->name);
  g_free (self->defstring);

  xmlBufferFree (self->buffer);
}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add_func ("/test-ddl/view-basic",
                   test_ddl_view_general);

  g_test_add ("/test-ddl/view-name",
              CheckDdlObject,
              NULL,
              test_ddl_view_start,
              test_ddl_view_name,
              test_ddl_view_finish);

  g_test_add ("/test-ddl/view-write",
              CheckDdlObject,
              NULL,
              test_ddl_view_start,
              test_ddl_view_write_node,
              test_ddl_view_finish);

  g_test_add ("/test-ddl/view-temp",
              CheckDdlObject,
              NULL,
              test_ddl_view_start,
              test_ddl_view_temp,
              test_ddl_view_finish);

  g_test_add ("/test-ddl/view-ifnoexist",
              CheckDdlObject,
              NULL,
              test_ddl_view_start,
              test_ddl_view_ifnoexist,
              test_ddl_view_finish);

  g_test_add ("/test-ddl/view-replace",
              CheckDdlObject,
              NULL,
              test_ddl_view_start,
              test_ddl_view_replace,
              test_ddl_view_finish);
  
  g_test_add ("/test-ddl/view-defstr",
              CheckDdlObject,
              NULL,
              test_ddl_view_start,
              test_ddl_view_defstr,
              test_ddl_view_finish);
  
  return g_test_run();
}
