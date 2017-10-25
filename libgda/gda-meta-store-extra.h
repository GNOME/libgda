/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
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

#ifndef __GDA_META_STORE_EXTRA_H_
#define __GDA_META_STORE_EXTRA_H_

G_BEGIN_DECLS

gboolean  _gda_meta_store_begin_data_reset (GdaMetaStore *store, GError **error);
gboolean  _gda_meta_store_cancel_data_reset (GdaMetaStore *store, GError **error);
gboolean  _gda_meta_store_finish_data_reset (GdaMetaStore *store, GError **error);

GdaMetaContext *_gda_meta_store_validate_context (GdaMetaStore *store, GdaMetaContext *context, GError **error);
GSList   *_gda_meta_store_schema_get_upstream_contexts (GdaMetaStore *store, GdaMetaContext *context, GError **error);
GSList   *_gda_meta_store_schema_get_downstream_contexts (GdaMetaStore *store, GdaMetaContext *context, GError **error);

G_END_DECLS

#endif
