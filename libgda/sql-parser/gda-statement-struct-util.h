/* 
 * Copyright (C) 2007 Vivien Malerba
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

#ifndef _GDA_STATEMENT_STRUCT_UTIL_H
#define _GDA_STATEMENT_STRUCT_UTIL_H

#include <glib.h>
#include <glib-object.h>

/* utility functions */
gchar *_remove_quotes (gchar *str);
gchar *_add_quotes (const gchar *str);

gchar *_json_quote_string (const gchar *str);
gboolean _string_is_identifier (const gchar *str);

/* to be removed, only here for debug */
gchar *gda_sql_value_stringify (const GValue *value);

#endif
