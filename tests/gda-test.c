#include<libgda/libgda.h>

int main() {
	GdaConnection *cnc;
	gda_init();
	cnc = gda_connection_open_from_string("SQLite", "DB_DIR=.;DB_NAME=test", NULL, GDA_CONNECTION_OPTIONS_NONE, NULL);
	g_object_unref(cnc);
	return EXIT_SUCCESS;
}

