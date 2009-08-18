/* GDA common library
 * Copyright (C) 2008 - 2009 The GNOME Foundation.
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

#ifndef __GDA_DATA_SELECT_EXTRA_H__
#define __GDA_DATA_SELECT_EXTRA_H__

#include <glib-object.h>
#include <libgda/gda-row.h>
#include <libgda/providers-support/gda-pstmt.h>
#include <sql-parser/gda-sql-statement.h>
#include <libgda/gda-data-select.h>

G_BEGIN_DECLS

typedef enum
{
	FIRST_QUERY = 0,
        INS_QUERY  = 0,
        UPD_QUERY  = 1,
        DEL_QUERY  = 2,
	NB_QUERIES = 3
} ModType;

typedef struct {
	gboolean                safely_locked;
	GdaSqlExpr             *unique_row_condition;
	gint                   *insert_to_select_mapping; /* see compute_insert_select_params_mapping() */
	
	GdaSet                 *exec_set; /* owned by this object (copied) */
	GdaSet                 *modif_set; /* owned by this object */
	GdaStatement           *modif_stmts[NB_QUERIES];
	GHashTable             *upd_stmts; /* key = a gboolean vector with TRUEs when the column is used, value = an UPDATE GdaStatement  */
	GHashTable             *ins_stmts; /* key = a gboolean vector with TRUEs when the column is used, value = an INSERT GdaStatement  */
	GdaStatement           *one_row_select_stmt; /* used to retrieve one row after an UPDATE
						      * or INSERT operation */

	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
} GdaDataSelectInternals;

GdaDataSelectInternals *_gda_data_select_internals_steal (GdaDataSelect *model);
void                    _gda_data_select_internals_paste (GdaDataSelect *model, GdaDataSelectInternals *inter);
void                    _gda_data_select_internals_free (GdaDataSelectInternals *inter);

G_END_DECLS

#endif
