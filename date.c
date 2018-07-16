#include <glib.h>

gint main (void) {
	GDateTime *dt = g_date_time_new_now_utc ();
	g_print (g_date_time_format (dt, "%F"));
}
