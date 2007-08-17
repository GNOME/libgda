#include <libgda/libgda.h>

GdaConnection *open_source_connection (GdaClient *client);
GdaConnection *open_destination_connection (GdaClient *client);
void           run_sql_non_select (GdaConnection *cnc, const gchar *sql);

