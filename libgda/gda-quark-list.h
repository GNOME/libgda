/*
 * Copyright (C) 1998 - 2011 The GNOME Foundation.
 *
 * Authors:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#ifndef __GDA_QUARK_LIST_H__
#define __GDA_QUARK_LIST_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GdaQuarkList GdaQuarkList;

#define GDA_TYPE_QUARK_LIST (gda_quark_list_get_type())

/**
 * SECTION:gda-quark-list
 * @short_description: Manages lists of KEY=VALUE pairs
 * @title: Quark list
 * @stability: Stable
 * @see_also:
 *
 * This object is used mainly by database provider's implementation to parse connection
 * strings into lists of KEY=VALUE pairs.
 */

GType         gda_quark_list_get_type        (void) G_GNUC_CONST;
GdaQuarkList *gda_quark_list_new             (void);
GdaQuarkList *gda_quark_list_new_from_string (const gchar *string);
GdaQuarkList *gda_quark_list_copy            (GdaQuarkList *qlist);
void          gda_quark_list_free            (GdaQuarkList *qlist);

void          gda_quark_list_add_from_string (GdaQuarkList *qlist,
					      const gchar *string,
					      gboolean cleanup);
const gchar  *gda_quark_list_find            (GdaQuarkList *qlist, const gchar *name);
void          gda_quark_list_remove          (GdaQuarkList *qlist, const gchar *name);
void          gda_quark_list_clear           (GdaQuarkList *qlist);
void          gda_quark_list_foreach         (GdaQuarkList *qlist, GHFunc func, gpointer user_data);

G_END_DECLS

#endif
