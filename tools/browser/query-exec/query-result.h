/* GNOME DB library
 * Copyright (C) 2009 The GNOME Foundation.
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

#ifndef __QUERY_RESULT_H__
#define __QUERY_RESULT_H__

#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include "query-editor.h"

G_BEGIN_DECLS

#define QUERY_TYPE_RESULT            (query_result_get_type())
#define QUERY_RESULT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, QUERY_TYPE_RESULT, QueryResult))
#define QUERY_RESULT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, QUERY_TYPE_RESULT, QueryResultClass))
#define IS_QUERY_RESULT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, QUERY_TYPE_RESULT))
#define IS_QUERY_RESULT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), QUERY_TYPE_RESULT))

typedef struct _QueryResult        QueryResult;
typedef struct _QueryResultClass   QueryResultClass;
typedef struct _QueryResultPrivate QueryResultPrivate;

struct _QueryResult {
	GtkVBox parent;
	QueryResultPrivate *priv;
};

struct _QueryResultClass {
	GtkVBoxClass parent_class;
};


GType      query_result_get_type (void) G_GNUC_CONST;
GtkWidget *query_result_new (QueryEditor *history);

void       query_result_show_history_batch (QueryResult *qres, QueryEditorHistoryBatch *hbatch);
void       query_result_show_history_item (QueryResult *qres, QueryEditorHistoryItem *hitem);

G_END_DECLS

#endif
