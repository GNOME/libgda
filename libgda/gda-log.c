/* GDA Common Library
 * Copyright (C) 1998-2002 The GNOME Foundation.
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

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <glib/gutils.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>

static gboolean log_enabled = TRUE;
static gboolean log_opened = FALSE;

/*
 * Private functions
 */

/**
 * gda_log_enable
 *
 * Enables GDA logs.
 */
void
gda_log_enable (void)
{
	log_enabled = TRUE;
	if (!log_opened) {
		openlog (g_get_prgname (), LOG_CONS | LOG_NOWAIT | LOG_PID, LOG_USER);
		log_opened = TRUE;
	}
}

/**
 * gda_log_disable
 *
 * Disables GDA logs.
 */
void
gda_log_disable (void)
{
	log_enabled = FALSE;
	if (log_opened) {
		closelog ();
		log_opened = FALSE;
	}
}

/**
 * gda_log_is_enabled
 *
 * Return: whether GDA logs are enabled (%TRUE or %FALSE).
 */
gboolean
gda_log_is_enabled (void)
{
	return log_enabled;
}

/**
 * gda_log_message
 * @format: format string (see the printf(3) documentation).
 * @...: arguments to insert in the message.
 *
 * Logs the given message in the GDA log file.
 */
void
gda_log_message (const gchar *format, ...)
{
	va_list args;
	gchar *msg;

	g_return_if_fail (format != NULL);

	if (!log_enabled)
		return;

	if (!log_opened)
		gda_log_enable ();

	/* build the message string */
	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	syslog (LOG_USER | LOG_INFO, msg);
	g_free (msg);
}

/**
 * gda_log_error
 * @format: format string (see the printf(3) documentation).
 * @...: arguments to insert in the error.
 *
 * Logs the given error in the GDA log file.
 */
void
gda_log_error (const gchar * format, ...)
{
	va_list args;
	gchar *msg;

	g_return_if_fail (format != NULL);

	if (!log_enabled)
		return;

	if (!log_opened)
		gda_log_enable ();

	/* build the message string */
	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	syslog (LOG_USER | LOG_ERR, msg);
	g_free (msg);
}
