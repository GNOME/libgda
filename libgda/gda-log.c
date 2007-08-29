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
#include <glib.h>
#ifndef G_OS_WIN32
#include <syslog.h>
#endif
#include <time.h>
#include <glib/gmem.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <glib/gutils.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>

static GStaticRecMutex gda_mutex = G_STATIC_REC_MUTEX_INIT;
#define GDA_LOG_LOCK() g_static_rec_mutex_lock(&gda_mutex)
#define GDA_LOG_UNLOCK() g_static_rec_mutex_unlock(&gda_mutex)
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
	GDA_LOG_LOCK ();
	log_enabled = TRUE;
	if (!log_opened) {
#ifndef G_OS_WIN32	
		openlog (g_get_prgname (), LOG_CONS | LOG_NOWAIT | LOG_PID, LOG_USER);
#endif		
		log_opened = TRUE;
	}
	GDA_LOG_UNLOCK ();
}

/**
 * gda_log_disable
 *
 * Disables GDA logs.
 */
void
gda_log_disable (void)
{
	GDA_LOG_LOCK ();
	log_enabled = FALSE;
	if (log_opened) {
#ifndef G_OS_WIN32		
		closelog ();
#endif
		log_opened = FALSE;
	}
	GDA_LOG_UNLOCK ();
}

/**
 * gda_log_is_enabled
 *
 * Returns: whether GDA logs are enabled (%TRUE or %FALSE).
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

	GDA_LOG_LOCK ();
	if (!log_opened)
		gda_log_enable ();

	/* build the message string */
	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);
#ifdef G_OS_WIN32
	g_log ("Gda", G_LOG_LEVEL_INFO, "%s", msg);
#else
	syslog (LOG_USER | LOG_INFO, "%s", msg);
#endif
	g_free (msg);
	GDA_LOG_UNLOCK ();
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

	GDA_LOG_LOCK ();
	if (!log_opened)
		gda_log_enable ();

	/* build the message string */
	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);
#ifdef G_OS_WIN32
	g_log ("Gda", G_LOG_LEVEL_DEBUG, "%s", msg);
#else
	syslog (LOG_USER | LOG_ERR, "%s", msg);
#endif
 	g_free (msg);
	GDA_LOG_UNLOCK ();
}
