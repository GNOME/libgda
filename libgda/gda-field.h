/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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

#if !defined(__gda_field_h__)
#  define __gda_field_h__

#include <libgda/gda-value.h>
#include <glib/gmacros.h>

G_BEGIN_DECLS

typedef struct {
	gint defined_size;
	gchar *name;
	gchar *caption;
	gint scale;
	GdaValueType gda_type;
	gboolean allow_null;
	gboolean primary_key;
	gboolean unique_key;
	gchar *references;
	gboolean auto_increment;
	glong auto_increment_start;
	glong auto_increment_step;
} GdaFieldAttributes;

typedef struct {
	gint actual_size;
	GdaValue *value;
	GdaFieldAttributes *attributes;
} GdaField;

GdaFieldAttributes *gda_field_attributes_new (void);
void                gda_field_attributes_free (GdaFieldAttributes *fa);
glong               gda_field_attributes_get_defined_size (GdaFieldAttributes *fa);
void                gda_field_attributes_set_defined_size (GdaFieldAttributes *fa, glong size);
const gchar        *gda_field_attributes_get_name (GdaFieldAttributes *fa);
void                gda_field_attributes_set_name (GdaFieldAttributes *fa, const gchar *name);
const gchar        *gda_field_attributes_get_caption (GdaFieldAttributes *fa);
void                gda_field_attributes_set_caption (GdaFieldAttributes *fa, const gchar *caption);
glong               gda_field_attributes_get_scale (GdaFieldAttributes *fa);
void                gda_field_attributes_set_scale (GdaFieldAttributes *fa, glong scale);
GdaValueType        gda_field_attributes_get_gdatype (GdaFieldAttributes *fa);
void                gda_field_attributes_set_gdatype (GdaFieldAttributes *fa,
						      GdaValueType type);
gboolean            gda_field_attributes_get_allow_null (GdaFieldAttributes *fa);
void                gda_field_attributes_set_allow_null (GdaFieldAttributes *fa, gboolean allow);
gboolean            gda_field_attributes_get_primary_key (GdaFieldAttributes *fa);
void                gda_field_attributes_set_primary_key (GdaFieldAttributes *fa, gboolean pk);
gboolean            gda_field_attributes_get_unique_key (GdaFieldAttributes *fa);
void                gda_field_attributes_set_unique_key (GdaFieldAttributes *fa, gboolean uk);
const gchar        *gda_field_attributes_get_references (GdaFieldAttributes *fa);
void                gda_field_attributes_set_references (GdaFieldAttributes *fa, const gchar *ref);

G_END_DECLS

#endif
