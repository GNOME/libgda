/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 Xabier Rodriguez Calvar <xrcalvar@igalia.com>
 * Copyright (C) 2002 Zbigniew Chyla <cyba@gnome.pl>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2006 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
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
#define G_LOG_DOMAIN "GDA-init"

#include <glib.h>
#include <gmodule.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <libgda/binreloc/gda-binreloc.h>
#include <sql-parser/gda-sql-parser.h>


/**
 * gda_init:
 *
 * Initializes the GDA library, must be called prior to any Libgda usage.
 */
void
gda_init (void)
{
	static GMutex init_mutex;
	static gboolean initialized = FALSE;

	GType type;
	gchar *file;

	g_mutex_lock (&init_mutex);
	if (initialized) {
		g_mutex_unlock (&init_mutex);
		gda_log_error (_("Ignoring attempt to re-initialize GDA library."));
		return;
	}

	file = gda_gbr_get_file_path (GDA_LOCALE_DIR, NULL);
	bindtextdomain (GETTEXT_PACKAGE, file);
	g_free (file);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

#if GLIB_CHECK_VERSION(2,36,0)
#else
	g_type_init ();
#endif

	if (!g_module_supported ())
		g_error (_("libgda needs GModule. Finishing..."));

	/* create the required Gda types, to avoid multiple simultaneous type creation when
	 * multi threads are used */
	type = GDA_TYPE_NULL;
	g_assert (type);
	type = G_TYPE_DATE;
	g_assert (type);
	type = GDA_TYPE_BINARY;
	g_assert (type);
	type = GDA_TYPE_BLOB;
	g_assert (type);
	type = GDA_TYPE_GEOMETRIC_POINT;
	g_assert (type);
	type = GDA_TYPE_NUMERIC;
	g_assert (type);
	type = GDA_TYPE_SHORT;
	g_assert (type);
	type = GDA_TYPE_USHORT;
	g_assert (type);
	type = GDA_TYPE_TIME;
	g_assert (type);
	type = GDA_TYPE_TEXT;
	g_assert (type);
	type = G_TYPE_DATE;
	g_assert (type);
	type = G_TYPE_DATE_TIME;
	g_assert (type);
	type = G_TYPE_ERROR;
	g_assert (type);

	/* force TZ init */
	tzset ();


	/* binreloc */
	gda_gbr_init ();

  initialized = TRUE;
	g_mutex_unlock (&init_mutex);
}

