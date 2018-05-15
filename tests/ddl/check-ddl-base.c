/* check-ddl-creator.c
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
#include <libgda/gda-ddl-base.h>

#define GDA_BOOL_TO_STRING(x) x ? "TRUE" : "FALSE"

typedef struct {
	GdaDdlBase *obj;
}BaseFixture;

static void
test_ddl_base_start (BaseFixture *self,
		     gconstpointer user_data)
{
	self->obj = gda_ddl_base_new();

}

static void
test_ddl_base_finish (BaseFixture *self,
		      gconstpointer user_data)
{
	gda_ddl_base_free (self->obj);
}

static void
test_ddl_base_run1 (BaseFixture *self,
		   gconstpointer user_data)
{
	const gchar *catalog = "First";
	const gchar *schema = "Second";
	const gchar *name = "Third";

	gda_ddl_base_set_names (self->obj,catalog,schema,name);

	const gchar *ret_catalog = gda_ddl_base_get_catalog (self->obj);

	g_assert_cmpstr (ret_catalog, ==, catalog);

	const gchar *ret_schema = gda_ddl_base_get_schema (self->obj);

	g_assert_cmpstr (ret_schema, ==, schema);

	const gchar *ret_name = gda_ddl_base_get_name (self->obj);

	g_assert_cmpstr (ret_name, ==, name);

	const gchar *full_name = gda_ddl_base_get_full_name (self->obj);

	g_assert_cmpstr (full_name, ==, "First.Second.Third");
}

static void
test_ddl_base_run2 (BaseFixture *self,
		   gconstpointer user_data)
{
	const gchar *catalog = "First";
	const gchar *schema = "Second";
	const gchar *name = "Third";

	gda_ddl_base_set_catalog (self->obj,catalog);

	const gchar *ret_catalog = gda_ddl_base_get_catalog (self->obj);

	g_assert_cmpstr (ret_catalog, ==, catalog);

	gda_ddl_base_set_schema (self->obj,schema);

	const gchar *ret_schema = gda_ddl_base_get_schema (self->obj);

	g_assert_cmpstr (ret_schema, ==, schema);

	gda_ddl_base_set_name (self->obj,name);

	const gchar *ret_name = gda_ddl_base_get_name (self->obj);

	g_assert_cmpstr (ret_name, ==, name);

	const gchar *full_name = gda_ddl_base_get_full_name (self->obj);

	g_assert_cmpstr (full_name, ==, "First.Second.Third");
}

static void
test_ddl_base_run3 (BaseFixture *self,
		   gconstpointer user_data)
{
	const gchar *schema = "Second";
	const gchar *name = "Third";

	gda_ddl_base_set_names (self->obj,NULL,schema,name);

	const gchar *ret_catalog = gda_ddl_base_get_catalog (self->obj);

	g_assert_cmpstr (ret_catalog, ==, NULL);

	const gchar *ret_schema = gda_ddl_base_get_schema (self->obj);

	g_assert_cmpstr (ret_schema, ==, schema);

	const gchar *ret_name = gda_ddl_base_get_name (self->obj);

	g_assert_cmpstr (ret_name, ==, name);

	const gchar *full_name = gda_ddl_base_get_full_name (self->obj);

	g_assert_cmpstr (full_name, ==, "Second.Third");

}

static void
test_ddl_base_run4 (BaseFixture *self,
		   gconstpointer user_data)
{
	const gchar *name = "Third";

	gda_ddl_base_set_names (self->obj,NULL,NULL,name);

	const gchar *ret_catalog = gda_ddl_base_get_catalog (self->obj);

	g_assert_cmpstr (ret_catalog, ==, NULL);

	const gchar *ret_schema = gda_ddl_base_get_schema (self->obj);

	g_assert_cmpstr (ret_schema, ==, NULL);

	const gchar *ret_name = gda_ddl_base_get_name (self->obj);

	g_assert_cmpstr (ret_name, ==, name);

	const gchar *full_name = gda_ddl_base_get_full_name (self->obj);

	g_assert_cmpstr (full_name, ==, "Third");
}

static void
test_ddl_base_run5 (BaseFixture *self,
		   gconstpointer user_data)
{
	const gchar *catalog = "First";
	const gchar *name = "Third";

	const gchar *full_name = gda_ddl_base_get_full_name (self->obj);

	g_assert_null (full_name);

	gda_ddl_base_set_name (self->obj,name);

	const gchar *ret_name = gda_ddl_base_get_name (self->obj);

	g_assert_cmpstr (ret_name, ==, name);

	gda_ddl_base_set_catalog (self->obj,catalog);

	const gchar *full_name2 = gda_ddl_base_get_full_name (self->obj);

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


	g_test_add ("/test-ddl/base-all",
		    BaseFixture,
		    NULL,
		    test_ddl_base_start,
		    test_ddl_base_run1,
		    test_ddl_base_finish);

	g_test_add ("/test-ddl/base-separate",
		    BaseFixture,
		    NULL,
		    test_ddl_base_start,
		    test_ddl_base_run2,
		    test_ddl_base_finish);

	g_test_add ("/test-ddl/base-schema-name",
		    BaseFixture,
		    NULL,
		    test_ddl_base_start,
		    test_ddl_base_run3,
		    test_ddl_base_finish);

	g_test_add ("/test-ddl/base-name",
		    BaseFixture,
		    NULL,
		    test_ddl_base_start,
		    test_ddl_base_run4,
		    test_ddl_base_finish);

	g_test_add ("/test-ddl/base-one",
		    BaseFixture,
		    NULL,
		    test_ddl_base_start,
		    test_ddl_base_run5,
		    test_ddl_base_finish);

	return g_test_run();
}
