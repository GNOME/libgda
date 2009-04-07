/* GDA common library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
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

#ifndef __GDA_DATA_SELECT_PRIV_H__
#define __GDA_DATA_SELECT_PRIV_H__

#include <glib-object.h>
#include <libgda/gda-row.h>
#include <libgda/providers-support/gda-pstmt.h>
#include <sql-parser/gda-sql-statement.h>
#include <libgda/gda-data-select.h>

G_BEGIN_DECLS


GType          gda_data_select_get_type                     (void) G_GNUC_CONST;

/* API reserved to provider's implementations */
void           gda_data_select_take_row                     (GdaDataSelect *model, GdaRow *row, gint rownum);
GdaRow        *gda_data_select_get_stored_row               (GdaDataSelect *model, gint rownum);
GdaConnection *gda_data_select_get_connection               (GdaDataSelect *model);


/* internal API */
void           _gda_data_select_share_private_data (GdaDataSelect *master, GdaDataSelect *slave);

G_END_DECLS

#endif
