/* GDA common library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_value_h__)
#  define __gda_value_h__

#include <glib/gmacros.h>
#include <libgda/GNOME_Database.h>

G_BEGIN_DECLS

typedef GNOME_Database_Value GdaValue;

GdaValue     *gda_value_new (void);
void          gda_value_free (GdaValue *value);

GdaValue     *gda_value_copy (GdaValue *value);

long long     gda_value_get_bigint (GdaValue *value);
void          gda_value_set_bigint (GdaValue *value, long long val);
gconstpointer gda_value_get_binary (GdaValue *value);
void          gda_value_set_binary (GdaValue *value, gconstpointer val, glong size);
gboolean      gda_value_get_boolean (GdaValue *value);
void          gda_value_set_boolean (GdaValue *value, gboolean val);
GDate        *gda_value_get_date (GdaValue *value);
void          gda_value_set_date (GdaValue *value, GDate *val);
gdouble       gda_value_get_double (GdaValue *value);
void          gda_value_set_double (GdaValue *value, gdouble val);
gint          gda_value_get_integer (GdaValue *value);
void          gda_value_set_integer (GdaValue *value, gint val);
gfloat        gda_value_get_single (GdaValue *value);
void          gda_value_set_single (GdaValue *value, gfloat val);
gshort        gda_value_get_smallint (GdaValue *value);
void          gda_value_set_smallint (GdaValue *value, gshort val);
const gchar  *gda_value_get_string (GdaValue *value);
void          gda_value_set_string (GdaValue *value, const gchar *val);
GTime         gda_value_get_time (GdaValue *value);
void          gda_value_set_time (GdaValue *value, GTime val);
time_t        gda_value_get_timestamp (GdaValue *value);
void          gda_value_set_timestamp (GdaValue *value, time_t val);
gchar         gda_value_get_tinyint (GdaValue *value);
void          gda_value_set_tinyint (GdaValue *value, gchar val);

gchar        *gda_value_stringify (GdaValue *value);

G_END_DECLS

#endif
