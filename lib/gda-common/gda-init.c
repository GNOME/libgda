/* GDA Common Library
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
#include "gda-common.h"
#include <gnome.h> // rremove!!!!!

/**
 * gda_init
 */
void
gda_init (const gchar *app_id, const gchar *version, gint nargs, gchar *args[])
{
  static gboolean initialized = FALSE;

  if (initialized)
    {
      gda_log_error(_("Attempt to initialize an already initialized client"));
      return;
    }

  /* FIXME: replace the GNOME call */
  gnome_init_with_popt_table(app_id, version, nargs, args, oaf_popt_options, 0, NULL);
  oaf_init(nargs, args);
  initialized = TRUE;
}