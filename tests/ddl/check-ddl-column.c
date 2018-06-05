/* check-ddl-column.c
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
#include <libgda/gda-ddl-column.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#define GDA_BOOL_TO_STRING(x) x ? "TRUE" : "FALSE"

typedef struct {
    GdaDdlColumn *column;
    gchar *xmlfile;
    xmlDocPtr doc;
    xmlBufferPtr buffer;
} CheckDdlObject;

static void
test_ddl_column_check_name (CheckDdlObject *self,
                            gconstpointer user_data)
{
  const gchar *cname = gda_ddl_column_get_name (self->column);
  g_assert_cmpstr (cname, ==,"column_name");

  const gchar *inval = "TestName";

  gda_ddl_column_set_name (self->column,inval);

  const gchar *resval = gda_ddl_column_get_name (self->column);

  g_assert_cmpstr (inval, ==, resval);
}

static void
test_ddl_column_check_default (CheckDdlObject *self,
                               gconstpointer user_data)
{
  const gchar *cname = gda_ddl_column_get_default (self->column);
  g_assert_cmpstr (cname, ==,"0");

  const gchar *inval = "1";

  gda_ddl_column_set_name (self->column,inval);

  const gchar *resval = gda_ddl_column_get_name (self->column);

  g_assert_cmpstr (inval, ==, resval);
}

static void
test_ddl_column_check_check (CheckDdlObject *self,
                             gconstpointer user_data)
{
  const gchar *cname = gda_ddl_column_get_check (self->column);
  g_assert_cmpstr (cname, ==,"compound_id > 0");

  const gchar *inval = "check string";

  gda_ddl_column_set_check (self->column,inval);

  const gchar *resval = gda_ddl_column_get_check (self->column);

  g_assert_cmpstr (inval, ==, resval);
}


static void
test_ddl_column_check_type (CheckDdlObject *self,
                            gconstpointer user_data)
{
  const gchar *ctype = gda_ddl_column_get_ctype (self->column);
  g_assert_cmpstr (ctype, ==,"string");

  GType gtype = gda_ddl_column_get_gtype (self->column);
  g_assert_true (gtype == G_TYPE_STRING);

  GType inval = G_TYPE_STRING;

  gda_ddl_column_set_type (self->column,inval);

  const GType resval = gda_ddl_column_get_gtype (self->column);

  g_assert_true(inval == resval);
}

static void
test_ddl_column_check_unique (CheckDdlObject *self,
                              gconstpointer user_data)
{
  gboolean res = FALSE;
  res = gda_ddl_column_get_unique (self->column);
  g_assert_true (res);

  gboolean inval = TRUE;

  gda_ddl_column_set_unique (self->column,inval);

  gboolean resval = gda_ddl_column_get_unique (self->column);

  g_assert_true(inval && resval);
}

static void
test_ddl_column_check_pkey (CheckDdlObject *self,
                            gconstpointer user_data)
{
  gboolean res = FALSE;
  res = gda_ddl_column_get_pkey (self->column);
  g_assert_true (res); // it is true in the column_test.xml

  gboolean inval = TRUE;

  gda_ddl_column_set_pkey (self->column,inval);

  gboolean resval = gda_ddl_column_get_pkey(self->column);

  g_assert_true(inval && resval);
}

static void
test_ddl_column_check_nnul (CheckDdlObject *self,
                            gconstpointer user_data)
{
  gboolean res = TRUE;

  res = gda_ddl_column_get_nnul (self->column);
  g_assert_false (res);

  gboolean inval = TRUE;

  gda_ddl_column_set_nnul (self->column,inval);

  gboolean resval = gda_ddl_column_get_nnul(self->column);

  g_assert_true(inval && resval);
}

static void
test_ddl_column_check_autoinc (CheckDdlObject *self,
                               gconstpointer user_data)
{
  gboolean res = TRUE;

  res = gda_ddl_column_get_autoinc (self->column);
  g_assert_false (res);

  gboolean inval = TRUE;

  gda_ddl_column_set_autoinc (self->column,inval);

  gboolean resval = gda_ddl_column_get_autoinc (self->column);

  g_assert_true(inval && resval);
}


static void
test_ddl_column_check_comment (CheckDdlObject *self,
                               gconstpointer user_data)
{
  const gchar* comstr = gda_ddl_column_get_comment (self->column);

  g_assert_cmpstr (comstr, ==, "Simple_Comment");

  const gchar *inval = "TestName";

  gda_ddl_column_set_comment (self->column,inval);

  const gchar *resval = gda_ddl_column_get_comment(self->column);

  g_assert_cmpstr (inval, ==, resval);
}

static void
test_ddl_column_check_size (CheckDdlObject *self,
                            gconstpointer user_data)
{
  guint size = gda_ddl_column_get_size (self->column);

  g_assert_cmpuint (size, ==, 5);

  guint inval = 81;

  gda_ddl_column_set_size (self->column,inval);

  guint resval = gda_ddl_column_get_size(self->column);

  g_assert_cmpuint (inval, ==, resval);
}

static void
test_ddl_column_startup (CheckDdlObject *self,
                         gconstpointer user_data)
{
  self->doc = NULL;
  self->xmlfile = NULL;
  self->column = NULL;

  const gchar *topsrcdir = g_getenv ("GDA_TOP_SRC_DIR");

  g_print ("ENV: %s\n",topsrcdir);
  g_assert_nonnull (topsrcdir);

  self->xmlfile = g_build_filename(topsrcdir,
                                   "tests",
                                   "ddl",
                                   "column_test.xml",NULL);

  g_assert_nonnull (self->xmlfile);

  self->doc = xmlParseFile(self->xmlfile);
  g_assert_nonnull (self->doc);

  xmlNodePtr node = xmlDocGetRootElement (self->doc);
  g_assert_nonnull (node);

  self->column = gda_ddl_column_new ();
  g_assert_nonnull(self->column);

  gboolean res = gda_ddl_buildable_parse_node (self->column,
                                               node,NULL);
  g_assert_true (res);
}

static void
test_ddl_column_cleanup (CheckDdlObject *self,
                         gconstpointer user_data)
{
  g_free (self->xmlfile);
  gda_ddl_column_free (self->column);
  xmlFreeDoc (self->doc);
}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);


  g_test_add ("/test-ddl/column-name",
              CheckDdlObject,
              NULL,
              test_ddl_column_startup,
              test_ddl_column_check_name,
              test_ddl_column_cleanup);

  g_test_add ("/test-ddl/column-type",
              CheckDdlObject,
              NULL,
              test_ddl_column_startup,
              test_ddl_column_check_type,
              test_ddl_column_cleanup);

  g_test_add ("/test-ddl/column-pkey",
              CheckDdlObject,
              NULL,
              test_ddl_column_startup,
              test_ddl_column_check_pkey,
              test_ddl_column_cleanup);

  g_test_add ("/test-ddl/column-unique",
              CheckDdlObject,
              NULL,
              test_ddl_column_startup,
              test_ddl_column_check_unique,
              test_ddl_column_cleanup);

  g_test_add ("/test-ddl/column-autoinc",
              CheckDdlObject,
              NULL,
              test_ddl_column_startup,
              test_ddl_column_check_autoinc,
              test_ddl_column_cleanup);

  g_test_add ("/test-ddl/column-nnul",
              CheckDdlObject,
              NULL,
              test_ddl_column_startup,
              test_ddl_column_check_nnul,
              test_ddl_column_cleanup);

  g_test_add ("/test-ddl/column-comment",
              CheckDdlObject,
              NULL,
              test_ddl_column_startup,
              test_ddl_column_check_comment,
              test_ddl_column_cleanup);

  g_test_add ("/test-ddl/column-size",
              CheckDdlObject,
              NULL,
              test_ddl_column_startup,
              test_ddl_column_check_size,
              test_ddl_column_cleanup);

  g_test_add ("/test-ddl/column-default",
              CheckDdlObject,
              NULL,
              test_ddl_column_startup,
              test_ddl_column_check_default,
              test_ddl_column_cleanup);

  g_test_add ("/test-ddl/column-check",
              CheckDdlObject,
              NULL,
              test_ddl_column_startup,
              test_ddl_column_check_check,
              test_ddl_column_cleanup);

  return g_test_run();
}
