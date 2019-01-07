/* check-db-fkey.c
 *
 * Copyright 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <libgda/libgda.h>
#include <libgda/gda-db-fkey.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

typedef struct {
    GdaDbFkey *fkey;

    GList *fkfield;
    GList *reffield;

    gchar *xmlfile;
    gchar *reftable;
    gchar *onupdate;
    gchar *ondelete;

    xmlDocPtr doc;
    xmlTextWriterPtr writer;
} CheckDbObject;

static void
test_db_fkey_run2 (CheckDbObject *self,
                    gconstpointer user_data)
{
  const gchar *reftable = NULL;

  reftable = gda_db_fkey_get_ref_table (self->fkey);

  g_assert_cmpstr (reftable, ==, self->reftable);

  const GList *fkfield = NULL;

  fkfield = gda_db_fkey_get_field_name (self->fkey);

  g_assert_nonnull (fkfield);

  const GList *it = NULL;
  GList *jt = NULL;

  for (it = fkfield, jt = self->fkfield; it && jt; it = it->next, jt = jt->next)
    g_assert_cmpstr (it->data, ==, jt->data);

  const GList *reffield = NULL;

  reffield = gda_db_fkey_get_ref_field(self->fkey);

  g_assert_nonnull (reffield);

  for (it = reffield, jt = self->reffield; it && jt; it = it->next, jt = jt->next)
    g_assert_cmpstr(it->data, ==, jt->data);

  const gchar *onupdate = gda_db_fkey_get_onupdate (self->fkey);

  g_assert_cmpstr(onupdate, ==, self->onupdate);

  const gchar *ondelete = gda_db_fkey_get_ondelete (self->fkey);

  g_assert_cmpstr(ondelete, ==, self->ondelete);
}

static void
test_db_fkey_run1 (void)
{
  GdaDbFkey *self = gda_db_fkey_new ();
  g_object_unref (self);
}

static void
test_db_fkey_run3 (CheckDbObject *self,
                    gconstpointer user_data)
{
  xmlNodePtr node = NULL;
  xmlDocPtr doc = NULL;
  xmlChar *xmlbuffer = NULL;
  int buffersize;

  node = xmlNewNode (NULL, BAD_CAST "root");
  int res = gda_db_buildable_write_node(GDA_DB_BUILDABLE(self->fkey),
                                         node,NULL);

  g_assert_true (res >= 0);

  doc = xmlNewDoc (BAD_CAST "1.0");
  xmlDocSetRootElement (doc,node);

  xmlDocDumpFormatMemory (doc,&xmlbuffer,&buffersize,1);

  g_print ("%s\n",(gchar*)xmlbuffer);

  xmlFree (xmlbuffer);
  xmlFreeDoc (doc);
}

static void
test_db_fkey_start (CheckDbObject *self,
                     gconstpointer user_data)
{
  self->doc = NULL;
  self->xmlfile = NULL;
  self->fkey = NULL;
  self->reffield = NULL;
  self->fkfield = NULL;
  self->reftable = NULL;
  self->onupdate = NULL;
  self->ondelete = NULL;

  self->reffield = g_list_append (self->reffield,"column_name1");
  self->reffield = g_list_append (self->reffield,"column_name2");
  self->reffield = g_list_append (self->reffield,"column_name3");

  self->fkfield = g_list_append(self->fkfield,"column_namefk1");
  self->fkfield = g_list_append(self->fkfield,"column_namefk2");
  self->fkfield = g_list_append(self->fkfield,"column_namefk3");

  self->reftable = g_strdup ("products");
  self->onupdate = g_strdup("NO ACTION");
  self->ondelete = g_strdup("NO ACTION");

  const gchar *topsrcdir = g_getenv ("GDA_TOP_SRC_DIR");

  g_print ("ENV: %s\n",topsrcdir);
  g_assert_nonnull (topsrcdir);

  self->xmlfile = g_build_filename(topsrcdir,
                                   "tests",
                                   "db",
                                   "fkey_test.xml",NULL);

  g_assert_nonnull (self->xmlfile);

  self->doc = xmlParseFile(self->xmlfile);
  g_assert_nonnull (self->doc);

  xmlNodePtr node = xmlDocGetRootElement (self->doc);
  g_assert_nonnull (node);

  self->fkey = gda_db_fkey_new ();

  g_assert_nonnull(self->fkey);

  gboolean res = gda_db_buildable_parse_node(GDA_DB_BUILDABLE(self->fkey),
                                              node,NULL);
  g_assert_true (res);
}

static void
test_db_fkey_finish (CheckDbObject *self,
                      gconstpointer user_data)
{
  g_free (self->xmlfile);
  g_object_unref (self->fkey);
  xmlFreeDoc (self->doc);
  g_list_free (self->reffield);
  g_list_free (self->fkfield);
  g_free (self->reftable);
  g_free (self->ondelete);
  g_free (self->onupdate);
}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add_func ("/test-db/fkey-basic",
                   test_db_fkey_run1);

  g_test_add ("/test-db/fkey-parse",
              CheckDbObject,
              NULL,
              test_db_fkey_start,
              test_db_fkey_run2,
              test_db_fkey_finish);

  g_test_add ("/test-db/fkey-write",
              CheckDbObject,
              NULL,
              test_db_fkey_start,
              test_db_fkey_run3,
              test_db_fkey_finish);

  return g_test_run();
}
