#include <libgda/libgda.h>

#ifndef _COMMON_H_
#define _COMMON_H_

gboolean create_sqlite_db (const gchar *dir, const gchar *dbname, const gchar *sqlfile, GError **error);
GdaDataModel *run_sql_select (GdaConnection *cnc, const gchar *sql);
void run_sql_non_select (GdaConnection *cnc, const gchar *sql);

#endif
