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

#ifndef __GDA_PMODEL_H__
#define __GDA_PMODEL_H__

#include <glib-object.h>
#include <libgda/providers-support/gda-prow.h>
#include <libgda/providers-support/gda-pstmt.h>

G_BEGIN_DECLS

#define GDA_TYPE_PMODEL            (gda_pmodel_get_type())
#define GDA_PMODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_PMODEL, GdaPModel))
#define GDA_PMODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_PMODEL, GdaPModelClass))
#define GDA_IS_PMODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_PMODEL))
#define GDA_IS_PMODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_PMODEL))

typedef struct _GdaPModel        GdaPModel;
typedef struct _GdaPModelClass   GdaPModelClass;
typedef struct _GdaPModelPrivate GdaPModelPrivate;

struct _GdaPModel {
	GObject           object;
	GdaPModelPrivate *priv;
	GdaPStmt         *prep_stmt; /* use the "prepared-stmt" property to set this */
	gint              advertized_nrows; /* set when the number of rows becomes known */
};

/*
 * Depending on model access flags, the implementations are:
 *
 * if GDA_DATA_MODEL_ACCESS_RANDOM: 
 *     REQUIRED: fetch_nb_rows, fetch_random
 * if GDA_STATEMENT_MODEL_CURSOR_FORWARD:
 *     REQUIRED: fetch_next
 *     OPTIONAL: fetch_at
 * if GDA_STATEMENT_MODEL_CURSOR_BACKWARD:
 *     REQUIRED: fetch_next
 *     OPTIONAL: fetch_at
 */
struct _GdaPModelClass {
	GObjectClass      parent_class;

	/* GDA_DATA_MODEL_ACCESS_RANDOM */
	gint             (*fetch_nb_rows) (GdaPModel *model);
	GdaPRow         *(*fetch_random)  (GdaPModel *model, gint rownum, GError **error);

	/* GDA_STATEMENT_MODEL_CURSOR */
	GdaPRow         *(*fetch_next)    (GdaPModel *model, gint rownum, GError **error);
	GdaPRow         *(*fetch_prev)    (GdaPModel *model, gint rownum, GError **error);
	GdaPRow         *(*fetch_at)      (GdaPModel *model, gint rownum, GError **error);
};

GType    gda_pmodel_get_type       (void) G_GNUC_CONST;
void     gda_pmodel_take_row       (GdaPModel *model, GdaPRow *row, gint rownum);
GdaPRow *gda_pmodel_get_stored_row (GdaPModel *model, gint rownum);

G_END_DECLS

#endif
