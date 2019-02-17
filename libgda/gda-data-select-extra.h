/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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
	GSList                 *modif_params[NB_QUERIES]; /* the lists point to holders in @modif_set */
	GdaStatement           *modif_stmts[NB_QUERIES];
	GHashTable             *upd_stmts; /* key = a gboolean vector with TRUEs when the column is used, value = an UPDATE GdaStatement  */
	GHashTable             *ins_stmts; /* key = a gboolean vector with TRUEs when the column is used, value = an INSERT GdaStatement  */
	GdaStatement           *one_row_select_stmt; /* used to retrieve one row after an UPDATE
						      * or INSERT operation */

	gboolean               *cols_mod[NB_QUERIES]; /* each NULL or an array of booleans the same size as
						       * GdaDataSelectPriv's PrivateShareable's @columns's length */
} GdaDataSelectInternals;

GdaDataSelectInternals *_gda_data_select_internals_steal (GdaDataSelect *model);
void                    _gda_data_select_internals_paste (GdaDataSelect *model, GdaDataSelectInternals *inter);
void                    _gda_data_select_internals_free (GdaDataSelectInternals *inter);

G_END_DECLS

#endif
