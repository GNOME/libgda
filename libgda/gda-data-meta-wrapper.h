/*
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DATA_META_WRAPPER_H__
#define __GDA_DATA_META_WRAPPER_H__

#include <libgda/gda-data-model.h>
#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_META_WRAPPER            (gda_data_meta_wrapper_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaDataMetaWrapper, gda_data_meta_wrapper, GDA, DATA_META_WRAPPER, GObject)

struct _GdaDataMetaWrapperClass {
	GObjectClass                   parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

typedef enum {
	GDA_DATA_META_WRAPPER_MODE_LC, /* quote identifier if there are some non lower case chars */
	GDA_DATA_META_WRAPPER_MODE_UC  /* quote identifier if there are some non upper case chars */
} GdaDataMetaWrapperMode;


GdaDataModel *_gda_data_meta_wrapper_new         (GdaDataModel *model, gboolean reusable,
						  gint *cols, gint size, GdaSqlIdentifierStyle mode,
						  GdaSqlReservedKeywordsFunc reserved_keyword_func);

GValue       *_gda_data_meta_wrapper_compute_value (const GValue *value, GdaSqlIdentifierStyle mode,
						    GdaSqlReservedKeywordsFunc reserved_keyword_func);
G_END_DECLS

#endif
