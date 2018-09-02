/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DATA_MODEL_DSN_LIST_H__
#define __GDA_DATA_MODEL_DSN_LIST_H__

#include "gda-decl.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_DSN_LIST            (gda_data_model_dsn_list_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaDataModelDsnList, gda_data_model_dsn_list, GDA, DATA_MODEL_DSN_LIST, GObject)
struct _GdaDataModelDsnListClass {
	GObjectClass                object_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

G_END_DECLS

#endif
