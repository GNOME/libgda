/*
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peńa <alvaropg@telefonica.net>
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

#ifndef __GDA_COLUMN_H__
#define __GDA_COLUMN_H__

#include <glib-object.h>
#include <libgda/gda-value.h>
#include <glib.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_COLUMN            (gda_column_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaColumn, gda_column, GDA, COLUMN, GObject)

struct _GdaColumnClass {
	GObjectClass          parent_class;
	
	/* signals */
	void (* name_changed)   (GdaColumn *column, const gchar *old_name);
	void (* g_type_changed) (GdaColumn *column, GType old_type, GType new_type);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-column
 * @short_description: Management of #GdaDataModel column attributes
 * @title: GdaDataModel columns
 * @stability: Stable
 * @see_also: #GdaDataModel
 *
 * The #GdaColumn object represents a #GdaDataModel's column and handle all its properties.
 */

GdaColumn      *gda_column_new                (void);
GdaColumn      *gda_column_copy               (GdaColumn *column);

const gchar    *gda_column_get_description          (GdaColumn *column);
void            gda_column_set_description          (GdaColumn *column, const gchar *descr);

const gchar    *gda_column_get_name           (GdaColumn *column);
void            gda_column_set_name           (GdaColumn *column, const gchar *name);

const gchar*    gda_column_get_dbms_type      (GdaColumn *column);
void            gda_column_set_dbms_type      (GdaColumn *column, const gchar *dbms_type);

GType           gda_column_get_g_type         (GdaColumn *column);
void            gda_column_set_g_type         (GdaColumn *column, GType type);

gboolean        gda_column_get_allow_null     (GdaColumn *column);
void            gda_column_set_allow_null     (GdaColumn *column, gboolean allow);

gboolean        gda_column_get_auto_increment (GdaColumn *column);
void            gda_column_set_auto_increment (GdaColumn *column, gboolean is_auto);

gint            gda_column_get_position       (GdaColumn *column);
void            gda_column_set_position       (GdaColumn *column, gint position);

const GValue   *gda_column_get_default_value  (GdaColumn *column);
void            gda_column_set_default_value  (GdaColumn *column, const GValue *default_value);


/**
 * gda_column_set_attribute_static:
 * @holder: a #GdaHolder
 * @attribute: attribute's name
 * @value: (nullable): the value to set the attribute to, or %NULL
 *
 * This function is similar to gda_column_set_attribute() but for static strings
 */
#define gda_column_set_attribute_static(holder,attribute,value) gda_column_set_attribute((holder),(attribute),(value),NULL)

G_END_DECLS

#endif
