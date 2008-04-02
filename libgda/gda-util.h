/* GDA common library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
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

#ifndef __GDA_UTIL_H__
#define __GDA_UTIL_H__

#include <glib/ghash.h>
#include <glib/glist.h>
#include "gda-holder.h"
#include "gda-row.h"
#include "gda-connection.h"

G_BEGIN_DECLS

/*
 * Type utilities
 */
const gchar *gda_g_type_to_string (GType type);
GType        gda_g_type_from_string (const gchar *str);

/* 
 * SQL escaping
 */
gchar       *gda_default_escape_string (const gchar *string);
gchar       *gda_default_unescape_string (const gchar *string);

/*
 * Param & model utilities
 */
gboolean     gda_utility_check_data_model (GdaDataModel *model, gint nbcols, ...);
void         gda_utility_data_model_dump_data_to_xml (GdaDataModel *model, xmlNodePtr parent, 
					      const gint *cols, gint nb_cols, const gint *rows, gint nb_rows,
					      gboolean use_col_ids);
void         gda_utility_holder_load_attributes (GdaHolder *holder, xmlNodePtr node, GSList *sources);

/* 
 * translate any text to an alphanumerical text 
 */
gchar       *gda_text_to_alphanum (const gchar *text);
gchar       *gda_alphanum_to_text (gchar *text);

/*
 * Statement computation from meta store 
 */
gboolean     gda_compute_dml_statements (GdaConnection *cnc, GdaStatement *select_stmt, gboolean require_pk, 
					 GdaStatement **insert, GdaStatement **update, GdaStatement **delete, 
					 GError **error);
G_END_DECLS

#endif
