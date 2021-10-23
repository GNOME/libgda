/* check-db-base.c
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
#include <libgda/gda-db-base.h>

#define GDA_BOOL_TO_STRING(x) x ? "TRUE" : "FALSE"

typedef struct {
	GdaDbBase *obj;
}BaseFixture;

typedef struct {
	GdaDbBase *obja;
	GdaDbBase *objb;
} TestBaseCompare;

static void
test_db_base_start (BaseFixture *self,
		     G_GNUC_UNUSED gconstpointer user_data)
{
	self->obj = gda_db_base_new();

}

static void
test_db_base_compare_start (TestBaseCompare *self,
		     G_GNUC_UNUSED gconstpointer user_data)
{
	self->obja = gda_db_base_new ();
	self->objb = gda_db_base_new();
}

static void
test_db_base_compare_finish (TestBaseCompare *self,
		      G_GNUC_UNUSED gconstpointer user_data)
{
	if (self->obja)
		g_object_unref (self->obja);

	if (self->objb)
		g_object_unref (self->objb);
}

static void
test_db_base_finish (BaseFixture *self,
		      G_GNUC_UNUSED gconstpointer user_data)
{
	if (self->obj)
		g_object_unref (self->obj);
}

static void
test_db_base_compare_same_names (TestBaseCompare *self,
		   G_GNUC_UNUSED gconstpointer user_data)
{
    const gchar *name = "name_a";
    gda_db_base_set_name (self->obja, name);
    gda_db_base_set_name (self->objb, name);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), ==, 0);

    gda_db_base_set_name (self->objb, "other");

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);
}

static void
test_db_base_compare_different_names (TestBaseCompare *self,
		   G_GNUC_UNUSED gconstpointer user_data)
{
    gda_db_base_set_name (self->obja, "name_a");
    gda_db_base_set_name (self->objb, "name_b");

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);
}

static void
test_db_base_compare_schema_name (TestBaseCompare *self,
		   G_GNUC_UNUSED gconstpointer user_data)
{
    const gchar *name = "name";

    gda_db_base_set_name (self->obja, name);
    gda_db_base_set_schema (self->obja, "schema_a");

    gda_db_base_set_name (self->objb, name);
/* one schema is NULL */
    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);

    gda_db_base_set_schema (self->objb, "schema_b");
    gda_db_base_set_schema (self->obja, NULL);

/* one schema is NULL */
    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);

    gda_db_base_set_schema (self->obja, name);
    gda_db_base_set_schema (self->objb, name);

/* same schema */
    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), ==, 0);
}

static void
test_db_base_compare_catalog_name (TestBaseCompare *self,
		   G_GNUC_UNUSED gconstpointer user_data)
{
    const gchar *name = "name";
    const gchar *catalog = "catalog";

    gda_db_base_set_name (self->obja, name);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);

    gda_db_base_set_catalog (self->obja, catalog);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);

    gda_db_base_set_name (self->objb, name);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);

    gda_db_base_set_catalog (self->objb, catalog);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), ==, 0);

    gda_db_base_set_catalog (self->obja, NULL);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);

    gda_db_base_set_catalog (self->obja, "catalog_a");

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);
}

static void
test_db_base_compare_catalog_schema_name (TestBaseCompare *self,
		   G_GNUC_UNUSED gconstpointer user_data)
{
    const gchar *name = "name";

    gda_db_base_set_catalog (self->obja, name);
    gda_db_base_set_catalog (self->objb, name);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), ==, 0);

    gda_db_base_set_schema (self->obja, name);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);

    gda_db_base_set_schema (self->objb, name);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), ==, 0);

    gda_db_base_set_name (self->obja, name);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), !=, 0);

    gda_db_base_set_name (self->objb, name);

    g_assert_cmpint (gda_db_base_compare (self->obja, self->objb), ==, 0);
}

static void
test_db_base_run1 (BaseFixture *self,
		   G_GNUC_UNUSED gconstpointer user_data)
{
	const gchar *catalog = "First";
	const gchar *schema = "Second";
	const gchar *name = "Third";

	gda_db_base_set_names (self->obj,catalog,schema,name);

	const gchar *ret_catalog = gda_db_base_get_catalog (self->obj);

	g_assert_cmpstr (ret_catalog, ==, catalog);

	const gchar *ret_schema = gda_db_base_get_schema (self->obj);

	g_assert_cmpstr (ret_schema, ==, schema);

	const gchar *ret_name = gda_db_base_get_name (self->obj);

	g_assert_cmpstr (ret_name, ==, name);

	const gchar *full_name = gda_db_base_get_full_name (self->obj);

	g_assert_cmpstr (full_name, ==, "First.Second.Third");
}

static void
test_db_base_run2 (BaseFixture *self,
		   G_GNUC_UNUSED gconstpointer user_data)
{
	const gchar *catalog = "First";
	const gchar *schema = "Second";
	const gchar *name = "Third";

	gda_db_base_set_catalog (self->obj,catalog);

	const gchar *ret_catalog = gda_db_base_get_catalog (self->obj);

	g_assert_cmpstr (ret_catalog, ==, catalog);

	gda_db_base_set_schema (self->obj,schema);

	const gchar *ret_schema = gda_db_base_get_schema (self->obj);

	g_assert_cmpstr (ret_schema, ==, schema);

	gda_db_base_set_name (self->obj,name);

	const gchar *ret_name = gda_db_base_get_name (self->obj);

	g_assert_cmpstr (ret_name, ==, name);

	const gchar *full_name = gda_db_base_get_full_name (self->obj);

	g_assert_cmpstr (full_name, ==, "First.Second.Third");
}

static void
test_db_base_run3 (BaseFixture *self,
		   G_GNUC_UNUSED gconstpointer user_data)
{
	const gchar *schema = "Second";
	const gchar *name = "Third";

	gda_db_base_set_names (self->obj,NULL,schema,name);

	const gchar *ret_catalog = gda_db_base_get_catalog (self->obj);

	g_assert_cmpstr (ret_catalog, ==, NULL);

	const gchar *ret_schema = gda_db_base_get_schema (self->obj);

	g_assert_cmpstr (ret_schema, ==, schema);

	const gchar *ret_name = gda_db_base_get_name (self->obj);

	g_assert_cmpstr (ret_name, ==, name);

	const gchar *full_name = gda_db_base_get_full_name (self->obj);
	g_assert_cmpstr (full_name, ==, "Second.Third");
}

static void
test_db_base_run4 (BaseFixture *self,
		   G_GNUC_UNUSED gconstpointer user_data)
{
	const gchar *name = "Third";

	gda_db_base_set_names (self->obj,NULL,NULL,name);

	const gchar *ret_catalog = gda_db_base_get_catalog (self->obj);

	g_assert_cmpstr (ret_catalog, ==, NULL);

	const gchar *ret_schema = gda_db_base_get_schema (self->obj);

	g_assert_cmpstr (ret_schema, ==, NULL);

	const gchar *ret_name = gda_db_base_get_name (self->obj);

	g_assert_cmpstr (ret_name, ==, name);

	const gchar *full_name = gda_db_base_get_full_name (self->obj);

	g_assert_cmpstr (full_name, ==, "Third");
}

static void
test_db_base_run5 (BaseFixture *self,
		   G_GNUC_UNUSED gconstpointer user_data)
{
	const gchar *catalog = "First";
	const gchar *name = "Third";

	const gchar *full_name = gda_db_base_get_full_name (self->obj);

	g_assert_null (full_name);

	gda_db_base_set_name (self->obj,name);

	const gchar *ret_name = gda_db_base_get_name (self->obj);

	g_assert_cmpstr (ret_name, ==, name);

	gda_db_base_set_catalog (self->obj,catalog);

	const gchar *full_name2 = gda_db_base_get_full_name (self->obj);

	 /* Only catalog and name are set.
	  * In this case only name should be returned.
	  */
	g_assert_cmpstr (full_name2, ==, name);
}

gint
main (gint   argc,
      gchar *argv[])
{
	setlocale (LC_ALL,"");

	g_test_init (&argc,&argv,NULL);


	g_test_add ("/test-db/base-all",
		    BaseFixture,
		    NULL,
		    test_db_base_start,
		    test_db_base_run1,
		    test_db_base_finish);

	g_test_add ("/test-db/base-separate",
		    BaseFixture,
		    NULL,
		    test_db_base_start,
		    test_db_base_run2,
		    test_db_base_finish);

	g_test_add ("/test-db/base-schema-name",
		    BaseFixture,
		    NULL,
		    test_db_base_start,
		    test_db_base_run3,
		    test_db_base_finish);

	g_test_add ("/test-db/base-name",
		    BaseFixture,
		    NULL,
		    test_db_base_start,
		    test_db_base_run4,
		    test_db_base_finish);

	g_test_add ("/test-db/base-one",
		    BaseFixture,
		    NULL,
		    test_db_base_start,
		    test_db_base_run5,
		    test_db_base_finish);

	g_test_add ("/test-db/base-compare-same-name",
		    TestBaseCompare,
		    NULL,
		    test_db_base_compare_start,
		    test_db_base_compare_same_names,
		    test_db_base_compare_finish);

	g_test_add ("/test-db/base-compare-different-name",
		    TestBaseCompare,
		    NULL,
		    test_db_base_compare_start,
		    test_db_base_compare_different_names,
		    test_db_base_compare_finish);

	g_test_add ("/test-db/base-compare-schema-name",
		    TestBaseCompare,
		    NULL,
		    test_db_base_compare_start,
		    test_db_base_compare_schema_name,
		    test_db_base_compare_finish);

	g_test_add ("/test-db/base-compare-catalog-name",
		    TestBaseCompare,
		    NULL,
		    test_db_base_compare_start,
		    test_db_base_compare_catalog_name,
		    test_db_base_compare_finish);

	g_test_add ("/test-db/base-compare-catalog-schema-name",
		    TestBaseCompare,
		    NULL,
		    test_db_base_compare_start,
		    test_db_base_compare_catalog_schema_name,
		    test_db_base_compare_finish);
	return g_test_run();
}
