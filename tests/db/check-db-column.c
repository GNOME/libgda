/* check-db-column.c
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
#include <libgda/gda-db-column.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

#define GDA_BOOL_TO_STRING(x) x ? "TRUE" : "FALSE"

typedef struct {
    GdaDbColumn *column;
    gchar *xmlfile;
    xmlDocPtr doc;
    xmlNodePtr node;
} CheckDbObject;

static void
test_db_column_check_name (CheckDbObject *self,
                            G_GNUC_UNUSED gconstpointer user_data)
{
  const gchar *cname = gda_db_column_get_name (self->column);
  g_assert_cmpstr (cname, ==,"column_name");

  const gchar *inval = "TestName";

  gda_db_column_set_name (self->column,inval);

  const gchar *resval = gda_db_column_get_name (self->column);

  g_assert_cmpstr (inval, ==, resval);
}

static void
test_db_column_check_default (CheckDbObject *self,
                               G_GNUC_UNUSED gconstpointer user_data)
{
  const gchar *cname = gda_db_column_get_default (self->column);
  g_assert_cmpstr (cname, ==,"12.34");

  const gchar *inval = "default value";

  gda_db_column_set_default (self->column,inval);

  const gchar *resval = gda_db_column_get_default (self->column);

  g_assert_cmpstr (inval, ==, resval);
}

static void
test_db_column_check_check (CheckDbObject *self,
                             G_GNUC_UNUSED gconstpointer user_data)
{
  const gchar *cname = gda_db_column_get_check (self->column);
  g_assert_cmpstr (cname, ==,"compound_id > 0");

  const gchar *inval = "check string";

  gda_db_column_set_check (self->column,inval);

  const gchar *resval = gda_db_column_get_check (self->column);

  g_assert_cmpstr (inval, ==, resval);
}


static void
test_db_column_check_type (CheckDbObject *self,
                            G_GNUC_UNUSED gconstpointer user_data)
{
  const gchar *ctype = gda_db_column_get_ctype (self->column);
  g_assert_cmpstr (ctype, ==,"string");

  GType gtype = gda_db_column_get_gtype (self->column);
  g_assert_true (gtype == G_TYPE_STRING);

  GType inval = G_TYPE_STRING;

  gda_db_column_set_type (self->column,inval);

  const GType resval = gda_db_column_get_gtype (self->column);

  g_assert_true(inval == resval);
}

static void
test_db_column_check_unique (CheckDbObject *self,
                              G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean res = FALSE;
  res = gda_db_column_get_unique (self->column);
  g_assert_true (res);

  gboolean inval = TRUE;

  gda_db_column_set_unique (self->column,inval);

  gboolean resval = gda_db_column_get_unique (self->column);

  g_assert_true(inval && resval);
}

static void
test_db_column_check_pkey (CheckDbObject *self,
                            G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean res = FALSE;
  res = gda_db_column_get_pkey (self->column);
  g_assert_true (res); // it is true in the column_test.xml

  gboolean inval = TRUE;

  gda_db_column_set_pkey (self->column,inval);

  gboolean resval = gda_db_column_get_pkey(self->column);

  g_assert_true(inval && resval);
}

static void
test_db_column_check_nnul (CheckDbObject *self,
                            G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean res = TRUE;

  res = gda_db_column_get_nnul (self->column);
  g_assert_false (res);

  gboolean inval = TRUE;

  gda_db_column_set_nnul (self->column,inval);

  gboolean resval = gda_db_column_get_nnul(self->column);

  g_assert_true(inval && resval);
}

static void
test_db_column_check_autoinc (CheckDbObject *self,
                               G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean res = TRUE;

  res = gda_db_column_get_autoinc (self->column);
  g_assert_false (res);

  gboolean inval = TRUE;

  gda_db_column_set_autoinc (self->column,inval);

  gboolean resval = gda_db_column_get_autoinc (self->column);

  g_assert_true(inval && resval);
}


static void
test_db_column_check_comment (CheckDbObject *self,
                               G_GNUC_UNUSED gconstpointer user_data)
{
  const gchar* comstr = gda_db_column_get_comment (self->column);

  g_assert_cmpstr (comstr, ==, "Simple_Comment");

  const gchar *inval = "TestName";

  gda_db_column_set_comment (self->column,inval);

  const gchar *resval = gda_db_column_get_comment(self->column);

  g_assert_cmpstr (inval, ==, resval);
}

static void
test_db_column_check_size (CheckDbObject *self,
                            G_GNUC_UNUSED gconstpointer user_data)
{
  guint size = gda_db_column_get_size (self->column);

  g_assert_cmpuint (size, ==, 5);

  guint inval = 81;

  gda_db_column_set_size (self->column,inval);

  guint resval = gda_db_column_get_size(self->column);

  g_assert_cmpuint (inval, ==, resval);
}

static void
test_db_column_startup (CheckDbObject *self,
                         G_GNUC_UNUSED gconstpointer user_data)
{
  self->doc = NULL;
  self->xmlfile = NULL;
  self->column = NULL;
  self->node = NULL;

  const gchar *topsrcdir = g_getenv ("GDA_TOP_SRC_DIR");

  g_print ("ENV: %s\n",topsrcdir);
  g_assert_nonnull (topsrcdir);

  self->xmlfile = g_build_filename(topsrcdir,
                                   "tests",
                                   "db",
                                   "column_test.xml",NULL);

  g_assert_nonnull (self->xmlfile);

  self->doc = xmlParseFile(self->xmlfile);
  g_assert_nonnull (self->doc);

  self->node = xmlDocGetRootElement (self->doc);
  g_assert_nonnull (self->node);

  self->column = gda_db_column_new ();
  g_assert_nonnull(self->column);

  gboolean res = gda_db_buildable_parse_node (GDA_DB_BUILDABLE(self->column),
                                               self->node,NULL);
  g_assert_true (res);
}

static void
test_db_column_cleanup (CheckDbObject *self,
                         G_GNUC_UNUSED gconstpointer user_data)
{
  g_free (self->xmlfile);
  g_object_unref (self->column);
  xmlFreeDoc (self->doc);
}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);


  g_test_add ("/test-db/column-name",
              CheckDbObject,
              NULL,
              test_db_column_startup,
              test_db_column_check_name,
              test_db_column_cleanup);

  g_test_add ("/test-db/column-type",
              CheckDbObject,
              NULL,
              test_db_column_startup,
              test_db_column_check_type,
              test_db_column_cleanup);

  g_test_add ("/test-db/column-pkey",
              CheckDbObject,
              NULL,
              test_db_column_startup,
              test_db_column_check_pkey,
              test_db_column_cleanup);

  g_test_add ("/test-db/column-unique",
              CheckDbObject,
              NULL,
              test_db_column_startup,
              test_db_column_check_unique,
              test_db_column_cleanup);

  g_test_add ("/test-db/column-autoinc",
              CheckDbObject,
              NULL,
              test_db_column_startup,
              test_db_column_check_autoinc,
              test_db_column_cleanup);

  g_test_add ("/test-db/column-nnul",
              CheckDbObject,
              NULL,
              test_db_column_startup,
              test_db_column_check_nnul,
              test_db_column_cleanup);

  g_test_add ("/test-db/column-comment",
              CheckDbObject,
              NULL,
              test_db_column_startup,
              test_db_column_check_comment,
              test_db_column_cleanup);

  g_test_add ("/test-db/column-size",
              CheckDbObject,
              NULL,
              test_db_column_startup,
              test_db_column_check_size,
              test_db_column_cleanup);

  g_test_add ("/test-db/column-default",
              CheckDbObject,
              NULL,
              test_db_column_startup,
              test_db_column_check_default,
              test_db_column_cleanup);

  g_test_add ("/test-db/column-check",
              CheckDbObject,
              NULL,
              test_db_column_startup,
              test_db_column_check_check,
              test_db_column_cleanup);

  return g_test_run();
}
