/* GDA Common Library
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gda-log.h"
#include <time.h>
#include <gda-config.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough.  */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

static gboolean log_enabled = TRUE;

/*
 * Private functions
 */
static gboolean
save_log_cb (gpointer user_data)
{
	g_print (_("Saving log...x"));
	gda_config_commit ();
	return TRUE;
}

static void
write_to_log (const gchar * str, gboolean error)
{
	gchar *msg;
	static gboolean initialized = FALSE;

	g_return_if_fail (str != NULL);

	/* initialize log */
	if (!initialized) {
		g_timeout_add (30000, (GSourceFunc) save_log_cb, NULL);
		initialized = TRUE;
	}

	/* compose message */
	msg = g_strdup_printf ("%s%s", error ? _("ERROR: ") : _("MESSAGE: "),
			       str);
	if (log_enabled) {
		time_t t;
		struct tm *now;
		gchar *config_entry;

		/* retrieve current date */
		t = time (NULL);
		now = localtime (&t);
		if (now) {
			config_entry =
				g_strdup_printf
				("%s/%s/%04d-%02d-%02d/%02d:%02d:%02d.%ld",
				 GDA_CONFIG_SECTION_LOG, g_get_prgname (),
				 now->tm_year + 1900, now->tm_mon + 1,
				 now->tm_mday, now->tm_hour, now->tm_min,
				 now->tm_sec, clock ());
			gda_config_set_string (config_entry, msg);
			g_free ((gpointer) config_entry);
		}
	}
	g_warning (msg);
	g_free ((gpointer) msg);
}

/**
 * gda_log_enable
 *
 * Enables GDA logs
 */
void
gda_log_enable (void)
{
	log_enabled = TRUE;
}

/**
 * gda_log_disable
 */
void
gda_log_disable (void)
{
	log_enabled = FALSE;
}

/**
 * gda_log_is_enabled
 */
gboolean
gda_log_is_enabled (void)
{
	return log_enabled;
}

/**
 * gda_log_message
 * @format: message string
 *
 * Logs the given message in the GDA log file
 */
void
gda_log_message (const gchar * format, ...)
{
	va_list args;
	gchar buffer[512];

	g_return_if_fail (format != NULL);

	va_start (args, format);
	vsprintf (buffer, format, args);
	va_end (args);

	write_to_log (buffer, FALSE);
}

/**
 * gda_log_error
 */
void
gda_log_error (const gchar * format, ...)
{
	va_list args;
	gchar buffer[512];

	g_return_if_fail (format != NULL);

	va_start (args, format);
	vsprintf (buffer, format, args);
	va_end (args);

	write_to_log (buffer, TRUE);
}

/**
 * gda_log_clean_all
 * @prgname: program name
 *
 * Clear the entire log for the given program
 */
void
gda_log_clean_all (const gchar * prgname)
{
}
