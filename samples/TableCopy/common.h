#include <libgda/libgda.h>

GdaConnection *open_source_connection (void);
GdaConnection *open_destination_connection (void);
void           run_sql_non_select (GdaConnection *cnc, const gchar *sql);

