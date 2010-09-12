#include <libgda/libgda.h>
#include <string.h>
int
main (int argc, char *argv[])
{
	g_type_init();
	gda_init ();

	gchar *str[] = {
	"PostgreSQL://meme:pass@DB_NAME=mydb;HOST=server",
	"PostgreSQL://meme@DB_NAME=mydb;HOST=server;PASSWORD=pass",
	"PostgreSQL://meme@DB_NAME=mydb;PASSWORD=pass;HOST=server",
	"PostgreSQL://meme@PASSWORD=pass;DB_NAME=mydb;HOST=server",
	"PostgreSQL://DB_NAME=mydb;HOST=server;USERNAME=meme;PASSWORD=pass",
	"PostgreSQL://DB_NAME=mydb;HOST=server;PASSWORD=pass;USERNAME=meme",
        "PostgreSQL://DB_NAME=mydb;USERNAME=meme;PASSWORD=pass;HOST=server",
        "PostgreSQL://PASSWORD=pass;USERNAME=meme;DB_NAME=mydb;HOST=server",
        "PostgreSQL://:pass@USERNAME=meme;DB_NAME=mydb;HOST=server",
        "PostgreSQL://:pass@DB_NAME=mydb;HOST=server;USERNAME=meme",
	NULL};

	gint i;
	for (i = 0; ;i++) {
		if (!str[i])
			break;
		gchar *cnc_params, *prov, *user, *pass;
		gda_connection_string_split (str[i], &cnc_params, &prov, &user, &pass);
		g_print ("[%s]\n  cnc_params=[%s]\n  prov      =[%s]\n  user      =[%s]\n  pass      =[%s]\n",
			str[i], cnc_params, prov, user, pass);
		if (strcmp (cnc_params, "DB_NAME=mydb;HOST=server")) {
			g_print ("Wrong cnc_params result\n");
			return 1;
		}
		if (strcmp (prov, "PostgreSQL")) {
			g_print ("Wrong provider result\n");
			return 1;
		}
		if (strcmp (user, "meme")) {
			g_print ("Wrong username result\n");
			return 1;
		}
		if (strcmp (pass, "pass")) {
			g_print ("Wrong password result\n");
			return 1;
		}
		g_free (cnc_params);
		g_free (prov);
		g_free (user);
		g_free (pass);
	}

	return 0;
}

