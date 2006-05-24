/* GDA library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Bas Driessen <bas.driessen@xobas.com>
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

#ifndef __LIBGDA_H__
#define __LIBGDA_H__

#include <libgda/gda-blob.h>
#include <libgda/gda-client.h>
#include <libgda/gda-column-index.h>
#include <libgda/gda-column.h>
#include <libgda/gda-command.h>
#include <libgda/gda-config.h>
#include <libgda/gda-connection-event.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-hash.h>
#include <libgda/gda-data-model-index.h>
#include <libgda/gda-data-model-query.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-data-model-import.h>
#include <libgda/gda-data-access-wrapper.h>
#include <libgda/gda-data-proxy.h>
#include <libgda/gda-log.h>
#include <libgda/gda-parameter.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-row.h>
#include <libgda/gda-data-model-filter-sql.h>
#include <libgda/gda-server-operation.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-threader.h>
#include <libgda/gda-transaction.h>
#include <libgda/gda-util.h>
#include <libgda/gda-value.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-object.h>
#include <libgda/gda-dict.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-graphviz.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/graph/gda-graph-query.h>
#include <libgda/graph/gda-graph.h>
#include <libgda/graph/gda-graph-item.h>
#include <libgda/gda-data-handler.h>
#include <libgda/handlers/gda-handler-bin.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-type.h>
#include <libgda/gda-entity-field.h>
#include <libgda/gda-dict-database.h>
#include <libgda/gda-dict-table.h>
#include <libgda/gda-renderer.h>
#include <libgda/gda-dict-field.h>
#include <libgda/gda-object-ref.h>
#include <libgda/gda-parameter-list.h>
#include <libgda/gda-xml-storage.h>
#include <libgda/gda-referer.h>
#include <libgda/gda-dict-type.h>
#include <libgda/gda-entity.h>
#include <libgda/gda-dict-function.h>
#include <libgda/gda-dict-aggregate.h>
#include <libgda/gda-dict-constraint.h>
#include <libgda/gda-query.h>
#include <libgda/gda-query-object.h>
#include <libgda/gda-query-target.h>
#include <libgda/gda-query-join.h>
#include <libgda/gda-query-condition.h>
#include <libgda/gda-query-field-agg.h>
#include <libgda/gda-query-field-all.h>
#include <libgda/gda-query-field-field.h>
#include <libgda/gda-query-field-func.h>
#include <libgda/gda-query-field-value.h>
#include <libgda/gda-query-field.h>
#include <libgda/gda-query-parsing.h>
#include <libgda/gda-query-private.h>

G_BEGIN_DECLS

void     gda_init             (const gchar *app_id, const gchar *version, gint nargs, gchar *args[]);
GdaDict *gda_get_default_dict (void);

typedef  void (* GdaInitFunc) (gpointer user_data);
void     gda_main_run         (GdaInitFunc init_func, gpointer user_data);
void     gda_main_quit        (void);

G_END_DECLS

#endif
