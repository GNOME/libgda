#include <libgda/libgda.h>
#include <string.h>

#ifndef _COMMON_H_
#define _COMMON_H_

void   tests_common_display_value (const gchar *prefix, const GValue *value);
gchar *tests_common_holder_serialize (GdaHolder *h);
gchar *tests_common_set_serialize (GdaSet *set);

GHashTable *tests_common_load_data (const gchar *filename);
gboolean tests_common_check_holder (GHashTable *data, const gchar *id, GdaHolder *h, GError **error);
gboolean tests_common_check_set (GHashTable *data, const gchar *id, GdaSet *set, GError **error);

#endif
