/*
 * Copyright (C) 2010 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include <string.h>

typedef struct {
	gchar *in_string;
	gchar *exp_provider;
	gchar *exp_cnc_params;
	gchar *exp_user;
	gchar *exp_pass;
} ATest;

ATest the_tests[] = {
	{"PostgreSQL://meme:pass@DB_NAME=mydb;HOST=server",
	 "PostgreSQL", "DB_NAME=mydb;HOST=server",
	 "meme", "pass"},
	{"PostgreSQL://meme@DB_NAME=mydb;HOST=server;PASSWORD=pass",
	 "PostgreSQL", "DB_NAME=mydb;HOST=server",
	 "meme", "pass"},
	{"PostgreSQL://meme@DB_NAME=mydb;PASSWORD=pass;HOST=server",
	 "PostgreSQL", "DB_NAME=mydb;HOST=server",
	 "meme", "pass"},
	{"PostgreSQL://meme@PASSWORD=pass;DB_NAME=mydb;HOST=server",
	 "PostgreSQL", "DB_NAME=mydb;HOST=server",
	 "meme", "pass"},
	{"PostgreSQL://DB_NAME=mydb;HOST=server;USERNAME=meme;PASSWORD=pass",
	 "PostgreSQL", "DB_NAME=mydb;HOST=server",
	 "meme", "pass"},
	{"PostgreSQL://DB_NAME=mydb;HOST=server;PASSWORD=pass;USERNAME=meme",
	 "PostgreSQL", "DB_NAME=mydb;HOST=server",
	 "meme", "pass"},
	{"PostgreSQL://DB_NAME=mydb;USERNAME=meme;PASSWORD=pass;HOST=server",
	 "PostgreSQL", "DB_NAME=mydb;HOST=server",
	 "meme", "pass"},
	{"PostgreSQL://PASSWORD=pass;USERNAME=meme;DB_NAME=mydb;HOST=server",
	 "PostgreSQL", "DB_NAME=mydb;HOST=server",
	 "meme", "pass"},
	{"PostgreSQL://:pass@USERNAME=meme;DB_NAME=mydb;HOST=server",
	 "PostgreSQL", "DB_NAME=mydb;HOST=server",
	 "meme", "pass"},
	{"PostgreSQL://:pass@DB_NAME=mydb;HOST=server;USERNAME=meme",
	 "PostgreSQL", "DB_NAME=mydb;HOST=server",
	 "meme", "pass"},
	{"PORT=5432;ADM_LOGIN=gdauser;HOST=gdatester;ADM_PASSWORD=GdaUser;DB_NAME=testcheckdb",
	 NULL,
	 "PORT=5432;ADM_LOGIN=gdauser;HOST=gdatester;ADM_PASSWORD=GdaUser;DB_NAME=testcheckdb",
	 NULL, NULL},
	{"PORT=5432;ADM_LOGIN=gdauser;HOST=gdatester;ADM_PASSWORD=GdaUser;DB_NAME=testcheckdb;USERNAME=meme;PASSWORD=pass",
	 NULL,
	 "PORT=5432;ADM_LOGIN=gdauser;HOST=gdatester;ADM_PASSWORD=GdaUser;DB_NAME=testcheckdb",
	 "meme", "pass"},
	{"PORT=5432;ADM_LOGIN=gdauser;HOST=gdatester;USERNAME=meme;PASSWORD=pass;ADM_PASSWORD=GdaUser;DB_NAME=testcheckdb",
	 NULL,
	 "PORT=5432;ADM_LOGIN=gdauser;HOST=gdatester;ADM_PASSWORD=GdaUser;DB_NAME=testcheckdb",
	 "meme", "pass"}
};

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
#if GLIB_CHECK_VERSION(2,36,0)
#else
	g_type_init();
#endif
	gda_init ();

	guint i;
	for (i = 0; i < sizeof (the_tests) / sizeof (ATest); i++) {
		ATest test = the_tests[i];
		gchar *cnc_params, *prov, *user, *pass;
		gda_connection_string_split (test.in_string, &cnc_params, &prov, &user, &pass);
		g_print ("[%s]\n  cnc_params=[%s]\n  prov      =[%s]\n  user      =[%s]\n  pass      =[%s]\n",
			test.in_string, cnc_params, prov, user, pass);
		if (g_strcmp0 (cnc_params, test.exp_cnc_params)) {
			g_print ("Wrong cnc_params result\n");
			goto onerror;
		}
		if (g_strcmp0 (prov, test.exp_provider)) {
			g_print ("Wrong provider result\n");
			goto onerror;
		}
		if (g_strcmp0 (user, test.exp_user)) {
			g_print ("Wrong username result\n");
			goto onerror;
		}
		if (g_strcmp0 (pass, test.exp_pass)) {
			g_print ("Wrong password result\n");
			goto onerror;
		}
		g_free (cnc_params);
		g_free (prov);
		g_free (user);
		g_free (pass);

	}

	return 0;
 onerror:
	g_print ("Error, aborting\n");
	return 1;
}

