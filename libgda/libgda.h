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

#if !defined(__libgda_h__)
#  define __libgda_h__

#include <libgda/gda-blob.h>
#include <libgda/gda-client.h>
#include <libgda/gda-command.h>
#include <libgda/gda-config.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-hash.h>
#include <libgda/gda-data-model-list.h>
#include <libgda/gda-error.h>
#include <libgda/gda-field.h>
#include <libgda/gda-log.h>
#include <libgda/gda-parameter.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-row.h>
#include <libgda/gda-select.h>
#include <libgda/gda-table.h>
#include <libgda/gda-transaction.h>
#include <libgda/gda-util.h>
#include <libgda/gda-value.h>
#include <libgda/gda-xml-connection.h>
#include <libgda/gda-xml-document.h>
#include <libgda/gda-xml-database.h>
#include <libgda/gda-enum-types.h>
  

G_BEGIN_DECLS

void gda_init (const gchar *app_id, const gchar *version, gint nargs, gchar *args[]);

typedef void (* GdaInitFunc) (gpointer user_data);

void gda_main_run (GdaInitFunc init_func, gpointer user_data);
void gda_main_quit (void);

G_END_DECLS

#endif
