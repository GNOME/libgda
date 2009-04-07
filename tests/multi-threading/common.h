#include <libgda/libgda.h>

#ifndef _COMMON_H_
#define _COMMON_H_

gboolean create_sqlite_db (const gchar *dir, const gchar *dbname, const gchar *sqlfile, GError **error);
GdaDataModel *run_sql_select (GdaConnection *cnc, const gchar *sql);
GdaDataModel *run_sql_select_cursor (GdaConnection *cnc, const gchar *sql);
gboolean run_sql_non_select (GdaConnection *cnc, const gchar *sql);
gboolean data_models_equal (GdaDataModel *m1, GdaDataModel *m2);

#endif
