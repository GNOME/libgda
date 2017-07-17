/*
 * Copyright (C) 2005 Dan Winship <danw@src.gnome.org>
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2007 - 2009 Murray Cumming <murrayc@murrayc.com>
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

#ifndef _GDA_STATEMENT_STRUCT_UTIL_H
#define _GDA_STATEMENT_STRUCT_UTIL_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* utility functions */
gchar    *_remove_quotes (gchar *str);
gchar    *_json_quote_string (const gchar *str);
gboolean  _string_is_identifier (const gchar *str);
gboolean  _split_identifier_string (gchar *str, gchar **remain, gchar **last);

gchar    *gda_sql_identifier_force_quotes (const gchar *str);
gchar    *gda_sql_identifier_prepare_for_compare (gchar *str);

/* to be removed, only here for debug */
gchar    *gda_sql_value_stringify (const GValue *value);

G_END_DECLS

#endif
