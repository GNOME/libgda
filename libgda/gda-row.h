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

#if !defined(__gda_row_h__)
#  define __gda_row_h__

#include <libgda/gda-data-model-column-attributes.h>
#include <libgda/global-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_ROW (gda_row_get_type())

GType         gda_row_get_type (void);
GdaRow       *gda_row_new (GdaDataModel *model, gint count);
GdaRow       *gda_row_new_from_list (GdaDataModel *model, const GList *values);
GdaRow       *gda_row_copy (GdaRow *row);
void          gda_row_free (GdaRow *row);
GdaDataModel *gda_row_get_model (GdaRow *row);
gint          gda_row_get_number (GdaRow *row);
void          gda_row_set_number (GdaRow *row, gint number);
const gchar  *gda_row_get_id (GdaRow *row);
void          gda_row_set_id (GdaRow *row, const gchar *id);
GdaValue     *gda_row_get_value (GdaRow *row, gint num);
gint          gda_row_get_length (GdaRow *row);

G_END_DECLS

#endif
