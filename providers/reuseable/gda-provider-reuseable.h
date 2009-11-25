/* GDA provider
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __GDA_PROVIDER_REUSEABLE_H__
#define __GDA_PROVIDER_REUSEABLE_H__

#include <glib.h>
#include <glib-object.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

typedef struct _GdaProviderReuseable GdaProviderReuseable;

typedef GdaProviderReuseable *(*GdaProviderReuseable_NewDataFunc) (void);
typedef void (*GdaProviderReuseable_ResetDataFunc) (GdaProviderReuseable *rdata);
typedef GType (*GdaProviderReuseable_GetGTypeFunc) (GdaConnection *cnc, GdaProviderReuseable *rdata, const gchar *db_type);
typedef GdaSqlReservedKeywordsFunc (*GdaProviderReuseable_GetReservedKeywordsFunc) (GdaProviderReuseable *rdata);
typedef GdaSqlParser *(*GdaProviderReuseable_CreateParserFunc) (GdaProviderReuseable *rdata);

/**
 * GdaProviderReuseable: Reuseable provider part
 *
 * Defines the interface which a database provider has to implement to be reused
 */
typedef struct {
	GdaProviderReuseable_NewDataFunc             re_new_data;
	GdaProviderReuseable_ResetDataFunc           re_reset_data;

	GdaProviderReuseable_GetGTypeFunc            re_get_type; /* Returns GDA_TYPE_NULL if conversion failed */
	GdaProviderReuseable_GetReservedKeywordsFunc re_get_reserved_keyword_func;

	GdaProviderReuseable_CreateParserFunc        re_create_parser;
	GdaServerProviderMeta                        re_meta_funcs;
} GdaProviderReuseableOperations;

/*
 * Declaration for provider's abstract operations
 */
struct _GdaProviderReuseable {
	GdaProviderReuseableOperations *operations;
	guint                           major; /* major version */
	guint                           minor; /* minor version */
	guint                           micro; /* micro version */
};


G_END_DECLS

#endif

