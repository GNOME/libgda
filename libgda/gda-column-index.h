/* GDA library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Bas Driessen <bas.driessen@xobas.com>
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

#if !defined(__gda_column_index_h__)
#  define __gda_column_index_h__

#include <glib-object.h>
#include <libgda/gda-value.h>
#include <glib.h>

G_BEGIN_DECLS

#define GDA_TYPE_COLUMN_INDEX            (gda_column_index_get_type())
#define GDA_COLUMN_INDEX(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_COLUMN_INDEX, GdaColumnIndex))
#define GDA_COLUMN_INDEX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_COLUMN_INDEX, GdaColumnIndexClass))
#define GDA_IS_COLUMN_INDEX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_COLUMN_INDEX))
#define GDA_IS_COLUMN_INDEX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_COLUMN_INDEX))

typedef enum {
	GDA_SORTING_ASCENDING,
	GDA_SORTING_DESCENDING
} GdaSorting;

typedef struct _GdaColumnIndex        GdaColumnIndex;
typedef struct _GdaColumnIndexClass   GdaColumnIndexClass;
typedef struct _GdaColumnIndexPrivate GdaColumnIndexPrivate;

struct _GdaColumnIndex {
	GObject object;
	GdaColumnIndexPrivate *priv;
};

struct _GdaColumnIndexClass {
	GObjectClass parent_class;
};

GType           gda_column_index_get_type (void);
GdaColumnIndex *gda_column_index_new (void);
GdaColumnIndex *gda_column_index_copy (GdaColumnIndex *dmcia);
gboolean        gda_column_index_equal (const GdaColumnIndex *lhs, const GdaColumnIndex *rhs);
const gchar    *gda_column_index_get_column_name (GdaColumnIndex *dmcia);
void            gda_column_index_set_column_name (GdaColumnIndex *dmcia, const gchar *column_name);
glong           gda_column_index_get_defined_size (GdaColumnIndex *dmcia);
void            gda_column_index_set_defined_size (GdaColumnIndex *dmcia, glong size);
GdaSorting      gda_column_index_get_sorting (GdaColumnIndex *dmcia);
void            gda_column_index_set_sorting (GdaColumnIndex *dmcia, GdaSorting sorting);
const gchar    *gda_column_index_get_references (GdaColumnIndex *dmcia);
void            gda_column_index_set_references (GdaColumnIndex *dmcia, const gchar *ref);

G_END_DECLS

#endif
