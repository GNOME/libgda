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

#include <config.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <glib/gmessages.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>

static gboolean log_enabled = TRUE;

/*
 * Private functions
 */

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
gda_log_message (const gchar *format, ...)
{
	va_list args;
	gchar *msg;

	g_return_if_fail (format != NULL);

	/* build the message string */
	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	g_message (_("MESSAGE: %s"), msg);
	g_free (msg);
}

/**
 * gda_log_error
 */
void
gda_log_error (const gchar * format, ...)
{
	va_list args;
	gchar *msg;

	g_return_if_fail (format != NULL);

	/* build the message string */
	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	g_message (_("ERROR: %s"), msg);
	g_free (msg);
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
