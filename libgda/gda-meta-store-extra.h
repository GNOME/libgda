/* gda-meta-store-extra.h
 *
 * Copyright (C) 2008 Vivien Malerba
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

#ifndef __GDA_META_STORE_EXTRA_H_
#define __GDA_META_STORE_EXTRA_H_

G_BEGIN_DECLS

gboolean  _gda_meta_store_begin_data_reset (GdaMetaStore *store, GError **error);
gboolean  _gda_meta_store_cancel_data_reset (GdaMetaStore *store, GError **error);
gboolean  _gda_meta_store_finish_data_reset (GdaMetaStore *store, GError **error);

GSList   *_gda_meta_store_schema_get_upstream_contexts (GdaMetaStore *store, GdaMetaContext *context, GError **error);
GSList   *_gda_meta_store_schema_get_downstream_contexts (GdaMetaStore *store, GdaMetaContext *context, GError **error);

G_END_DECLS

#endif
