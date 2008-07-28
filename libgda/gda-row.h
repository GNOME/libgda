/* GDA library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *      Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_ROW_H__
#define __GDA_ROW_H__

#include <glib-object.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_ROW            (gda_row_get_type())
#define GDA_ROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ROW, GdaRow))
#define GDA_ROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ROW, GdaRowClass))
#define GDA_IS_ROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ROW))
#define GDA_IS_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_ROW))

typedef struct _GdaRow        GdaRow;
typedef struct _GdaRowClass   GdaRowClass;
typedef struct _GdaRowPrivate GdaRowPrivate;

struct _GdaRow {
	GObject        object;
	GdaRowPrivate *priv;
};

struct _GdaRowClass {
	GObjectClass   parent_class;
};

GType         gda_row_get_type       (void) G_GNUC_CONST;

GdaRow       *gda_row_new            (gint count);
gint          gda_row_get_length     (GdaRow *row);
GValue       *gda_row_get_value      (GdaRow *row, gint num);

G_END_DECLS

#endif
