/*
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
 * @title: GdaQuarkList
 * @stability: Stable
 * @see_also:
 *
 * This object is used to store KEY=VALUE pairs. It is mainly used internally by Libgda to store connection
 * parameters.
 *
 * Authentification values are kept in a mangled form in memory, and unmangled when
 * they are requested using gda_quark_list_find(), and when you don't need them anymore,
 * call gda_quark_list_protect_values() to remove the unmangled version.
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
void          gda_quark_list_protect_values  (GdaQuarkList *qlist);

void          gda_quark_list_remove          (GdaQuarkList *qlist, const gchar *name);
void          gda_quark_list_clear           (GdaQuarkList *qlist);
void          gda_quark_list_foreach         (GdaQuarkList *qlist, GHFunc func, gpointer user_data);

G_END_DECLS

#endif
