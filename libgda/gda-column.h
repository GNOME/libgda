/* GDA library
 * Copyright (C) 1998 - 2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Álvaro Peña <alvaropg@telefonica.net>
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

#if !defined(__gda_column_h__)
#  define __gda_column_h__

#include <glib-object.h>
#include <libgda/gda-value.h>
#include <glib/gmacros.h>
#include <libgda/global-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_COLUMN            (gda_column_get_type())
#define GDA_COLUMN(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_COLUMN, GdaColumn))
#define GDA_COLUMN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_COLUMN, GdaColumnClass))
#define GDA_IS_COLUMN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_COLUMN))
#define GDA_IS_COLUMN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_COLUMN))

typedef struct _GdaColumn        GdaColumn;
typedef struct _GdaColumnClass   GdaColumnClass;
typedef struct _GdaColumnPrivate GdaColumnPrivate;

struct _GdaColumn {
	GObject object;
	GdaColumnPrivate *priv;
};

struct _GdaColumnClass {
	GObjectClass parent_class;
	
	/* signals */
	void (* name_changed) (GdaColumn *column, const gchar *old_name);
};

GType           gda_column_get_type (void);
GdaColumn      *gda_column_new (void);
GdaColumn      *gda_column_copy (GdaColumn *column);
void            gda_column_free (GdaColumn *column);
gboolean        gda_column_equal (const GdaColumn *lhs, const GdaColumn *rhs);
glong           gda_column_get_defined_size (GdaColumn *column);
void            gda_column_set_defined_size (GdaColumn *column, glong size);
const gchar    *gda_column_get_name (GdaColumn *column);
void            gda_column_set_name (GdaColumn *column, const gchar *name);
const gchar    *gda_column_get_table (GdaColumn *column);
void            gda_column_set_table (GdaColumn *column, const gchar *table);
const gchar    *gda_column_get_caption (GdaColumn *column);
void            gda_column_set_caption (GdaColumn *column, const gchar *caption);
glong           gda_column_get_scale (GdaColumn *column);
void            gda_column_set_scale (GdaColumn *column, glong scale);
const gchar*    gda_column_get_dbms_type (GdaColumn *column);
void            gda_column_set_dbms_type (GdaColumn *column, const gchar *dbms_type);
GdaValueType    gda_column_get_gdatype (GdaColumn *column);
void            gda_column_set_gdatype (GdaColumn *column, GdaValueType type);
gboolean        gda_column_get_allow_null (GdaColumn *column);
void            gda_column_set_allow_null (GdaColumn *column, gboolean allow);
gboolean        gda_column_get_primary_key (GdaColumn *column);
void            gda_column_set_primary_key (GdaColumn *column, gboolean pk);
gboolean        gda_column_get_unique_key (GdaColumn *column);
void            gda_column_set_unique_key (GdaColumn *column, gboolean uk);
const gchar    *gda_column_get_references (GdaColumn *column);
void            gda_column_set_references (GdaColumn *column, const gchar *ref);
gboolean        gda_column_get_auto_increment (GdaColumn *column);
void            gda_column_set_auto_increment (GdaColumn *column, gboolean is_auto);
gint            gda_column_get_position (GdaColumn *column);
void            gda_column_set_position (GdaColumn *column, gint position);

const GdaValue *gda_column_get_default_value (GdaColumn *column);
void            gda_column_set_default_value (GdaColumn *column, const GdaValue *default_value);

G_END_DECLS

#endif
