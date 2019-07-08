/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2003 Paisa Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2004 Alan Knowles <alan@akbkhome.com>
 * Copyright (C) 2004 Dani Baeyens <daniel.baeyens@hispalinux.es>
 * Copyright (C) 2005 Martin Schulze <joey@infodrom.org>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#define G_LOG_DOMAIN "GDA-log"

#include <stdarg.h>
#include <stdio.h>
#include <glib.h>
#ifndef G_OS_WIN32
#include <syslog.h>
#endif
#include <time.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>

static GRecMutex gda_rmutex;
#define GDA_LOG_LOCK() g_rec_mutex_lock(&gda_rmutex)
#define GDA_LOG_UNLOCK() g_rec_mutex_unlock(&gda_rmutex)
static gboolean log_enabled = TRUE;
static gboolean log_opened = FALSE;

/*
 * Private functions
 */

/**
 * gda_log_enable:
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
 * gda_log_disable:
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
 * gda_log_is_enabled:
 *
 * Returns: whether GDA logs are enabled (%TRUE or %FALSE).
 */
gboolean
gda_log_is_enabled (void)
{
	return log_enabled;
}

/**
 * gda_log_message:
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
 * gda_log_error:
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
