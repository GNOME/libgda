/* GDA Common Library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
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

#if !defined(__gda_common_h__)
#  define __gda_common_h__

#include <gda-data-model.h>
#include <gda-quark-list.h>
#include <gda-value.h>

G_BEGIN_DECLS

void gda_init (const gchar *app_id, const gchar *version, gint nargs, gchar *args[]);

typedef void (* GdaInitFunc) (gpointer user_data);

void gda_main_run  (GdaInitFunc init_func, gpointer user_data);
void gda_main_quit (void);

G_END_DECLS

#endif
