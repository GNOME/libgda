/* GDA library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
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

#ifndef __GDA_PROW_H__
#define __GDA_PROW_H__

#include <glib-object.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_PROW            (gda_prow_get_type())
#define GDA_PROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_PROW, GdaPRow))
#define GDA_PROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_PROW, GdaPRowClass))
#define GDA_IS_PROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_PROW))
#define GDA_IS_PROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_PROW))

typedef struct _GdaPRow        GdaPRow;
typedef struct _GdaPRowClass   GdaPRowClass;
typedef struct _GdaPRowPrivate GdaPRowPrivate;

struct _GdaPRow {
	GObject        object;
	GdaPRowPrivate *priv;
};

struct _GdaPRowClass {
	GObjectClass   parent_class;
};

GType         gda_prow_get_type           (void) G_GNUC_CONST;

GdaPRow      *gda_prow_new            (gint count);
gint          gda_prow_get_length     (GdaPRow *prow);
GValue       *gda_prow_get_value      (GdaPRow *prow, gint num);

G_END_DECLS

#endif
