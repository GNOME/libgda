/* GDA Common Library
 * Copyright (C) 2001, The Free Software Foundation
 *
 * Authors:
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

#if !defined(__gda_quark_list_h__)
#  define __gda_quark_list_h__

#include <glib/gmacros.h>
#include <glib/gtypes.h>

G_BEGIN_DECLS

typedef struct _GdaQuarkList GdaQuarkList;

GdaQuarkList *gda_quark_list_new (void);
GdaQuarkList *gda_quark_list_new_from_string (const gchar * string);
void          gda_quark_list_free (GdaQuarkList * qlist);

void          gda_quark_list_add_from_string (GdaQuarkList * qlist,
					      const gchar * string,
					      gboolean cleanup);
const gchar *gda_quark_list_find (GdaQuarkList * qlist,
				  const gchar * name);

G_END_DECLS

#endif
