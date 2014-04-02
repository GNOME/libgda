/*
 * Copyright (C) 2013 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <libgda/libgda.h>
#include <glib/gstdio.h>

static guint test1 (void);
static guint test2 (void);
static guint test3 (void);
static guint test4 (void);
static guint test5 (void);


int main(int argc, const char *argv[])
{
	gda_init ();
	guint nfailed = 0;

	nfailed += test1 ();
	nfailed += test2 ();
	nfailed += test3 ();
	nfailed += test4 ();
	nfailed += test5 ();

	g_unlink ("test-cnc-opendb.db");

	if (nfailed > 0)
		g_print ("Test failed %u times\n", nfailed);
	else
		g_print ("Test Ok\n");
	return nfailed ? 1 : 0;
}

static gboolean
idle_incr (guint *ptr)
{
	(*ptr)++;
	return TRUE;
}

static void
setup_main_context (GdaConnection *cnc, guint *ptr_to_incr)
{
	GMainContext *context;
	GSource *idle;

	context = g_main_context_new ();
	idle = g_idle_source_new ();
	g_source_set_priority (idle, G_PRIORITY_DEFAULT);
	g_source_attach (idle, context);
	g_source_set_callback (idle, (GSourceFunc) idle_incr, ptr_to_incr, NULL);
	g_source_unref (idle);
	gda_connection_set_main_context (cnc, NULL, context);
	g_main_context_unref (context);	
}


/*
 * Test 1: gda_connection_open() when there is no error
 */
static guint
test1 (void)
{
	g_print ("============= %s started =============\n", __FUNCTION__);
	GdaConnection *cnc;
	GError *error = NULL;
	cnc = gda_connection_new_from_string ("SQLite", "DB_NAME=test-cnc-opendb", NULL,
					      GDA_CONNECTION_OPTIONS_AUTO_META_DATA, &error);

	if (!cnc) {
		g_print ("gda_connection_new_from_string() failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}

	guint counter = 0;
	setup_main_context (cnc, &counter);

	if (! gda_connection_open (cnc, &error)) {
		g_print ("gda_connection_open() failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}

	if (counter == 0) {
		g_print ("gda_connection_open() failed: did not make GMainContext 'run'\n");
		return 1;
	}
	else
		g_print ("Counter incremented to %u\n", counter);

	g_object_unref (cnc);

	return 0;
}

/*
 * Test 2: gda_connection_open_async() when there is no error
 */
static void
test2_open_func (GdaConnection *cnc, guint job_id, gboolean result, GError *error, GMainLoop *loop)
{
	g_print ("Test connection %p is now %s, job was %u and error [%s]\n", cnc, result ? "OPENED" : "CLOSED",
		 job_id, error ? (error->message ? error->message : "No detail") : "No error");
	if (!result)
		exit (1);
	g_main_loop_quit (loop);
}

static guint
test2 (void)
{
	g_print ("============= %s started =============\n", __FUNCTION__);
	GdaConnection *cnc;
	GError *error = NULL;
	cnc = gda_connection_new_from_string ("SQLite", "DB_NAME=test-cnc-opendb", NULL,
					      GDA_CONNECTION_OPTIONS_AUTO_META_DATA, &error);

	if (!cnc) {
		g_print ("gda_connection_new_from_string() failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}

	guint counter = 0;
	setup_main_context (cnc, &counter);

	GMainLoop *loop;
	loop = g_main_loop_new (gda_connection_get_main_context (cnc, NULL), FALSE);

	guint job_id;
	job_id = gda_connection_open_async (cnc, (GdaConnectionOpenFunc) test2_open_func, loop, &error);

	if (!job_id) {
		g_print ("gda_connection_open_async() failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}
	else {
		g_print ("Connection opening job is %u\n", job_id);
	}

	g_main_loop_run (loop);

	if (counter == 0) {
		g_print ("gda_connection_open() failed: did not make GMainContext 'run'\n");
		return 1;
	}
	else
		g_print ("Counter incremented to %u\n", counter);

	gboolean opened;
	opened = gda_connection_is_opened (cnc);
	g_object_unref (cnc);

	return opened ? 0 : 1;
}

/*
 * Test 3: gda_connection_open() when there is an error
 */
static guint
test3 (void)
{
	g_print ("============= %s started =============\n", __FUNCTION__);
	GdaConnection *cnc;
	GError *error = NULL;
	cnc = gda_connection_new_from_string ("SQLite", "DB_NAMEEEE=test-cnc-opendb", NULL,
					      GDA_CONNECTION_OPTIONS_AUTO_META_DATA, &error);

	if (!cnc) {
		g_print ("gda_connection_new_from_string() failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}

	guint counter = 0;
	setup_main_context (cnc, &counter);

	if (! gda_connection_open (cnc, &error)) {
		g_print ("gda_connection_open() failed: %s\n", error && error->message ? error->message : "No detail");
		g_object_unref (cnc);

		if (counter == 0) {
			g_print ("gda_connection_open() failed: did not make GMainContext 'run'\n");
			return 1;
		}
		else
			g_print ("Counter incremented to %u\n", counter);

		return 0;
	}
	else {
		g_print ("Connection opened when it should not have!\n");
		g_object_unref (cnc);
		return 1;
	}
}

/*
 * Test 4: gda_connection_open_async() when there is an error
 */
static void
test4_open_func (GdaConnection *cnc, guint job_id, gboolean result, GError *error, GMainLoop *loop)
{
	g_print ("Test connection %p is now %s, job was %u and error [%s]\n", cnc, result ? "OPENED" : "CLOSED",
		 job_id, error ? (error->message ? error->message : "No detail") : "No error");
	if (!error || (error->domain != GDA_CONNECTION_ERROR) || (error->code != GDA_CONNECTION_OPEN_ERROR))
		exit (1);
	g_main_loop_quit (loop);
}

static guint
test4 (void)
{
	g_print ("============= %s started =============\n", __FUNCTION__);
	GdaConnection *cnc;
	GError *error = NULL;
	cnc = gda_connection_new_from_string ("SQLite", "DB_NAMEEEE=test-cnc-opendb", NULL,
					      GDA_CONNECTION_OPTIONS_AUTO_META_DATA, &error);

	if (!cnc) {
		g_print ("gda_connection_new_from_string() failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}

	guint counter = 0;
	setup_main_context (cnc, &counter);

	GMainLoop *loop;
	loop = g_main_loop_new (gda_connection_get_main_context (cnc, NULL), FALSE);

	guint job_id;
	job_id = gda_connection_open_async (cnc, (GdaConnectionOpenFunc) test4_open_func, loop, &error);

	if (!job_id) {
		g_print ("gda_connection_open_async() failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}
	else {
		g_print ("Connection opening job is %u\n", job_id);
	}

	g_main_loop_run (loop);

	gboolean opened;
	opened = gda_connection_is_opened (cnc);
	g_object_unref (cnc);

	if (counter == 0) {
		g_print ("gda_connection_open() failed: did not make GMainContext 'run'\n");
		return 1;
	}
	else
		g_print ("Counter incremented to %u\n", counter);

	return opened ? 1 : 0;
}

/*
 * Test 5: open, close and open again
 */
static guint
test5 (void)
{
	g_print ("============= %s started =============\n", __FUNCTION__);
	GdaConnection *cnc;
	GError *error = NULL;
	cnc = gda_connection_new_from_string ("SQLite", "DB_NAME=test-cnc-opendb", NULL,
					      GDA_CONNECTION_OPTIONS_AUTO_META_DATA, &error);

	if (!cnc) {
		g_print ("gda_connection_new_from_string() failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}

	guint counter = 0;
	setup_main_context (cnc, &counter);

	/* open */
	if (! gda_connection_open (cnc, &error)) {
		g_print ("gda_connection_open() 1 failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}
	if (counter == 0) {
		g_print ("gda_connection_open() 1 failed: did not make GMainContext 'run'\n");
		return 1;
	}
	else
		g_print ("Counter incremented to %u\n", counter);

	/* open */
	counter = 0;
	if (! gda_connection_close (cnc, &error)) {
		g_print ("gda_connection_close() failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}
	if (counter == 0) {
		g_print ("gda_connection_close() failed: did not make GMainContext 'run'\n");
		return 1;
	}
	else
		g_print ("Counter incremented to %u\n", counter);

	/* open */
	counter = 0;
	if (! gda_connection_open (cnc, &error)) {
		g_print ("gda_connection_open() 2 failed: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}
	if (counter == 0) {
		g_print ("gda_connection_open() 2 failed: did not make GMainContext 'run'\n");
		return 1;
	}
	else
		g_print ("Counter incremented to %u\n", counter);

	g_object_unref (cnc);

	return 0;
}
