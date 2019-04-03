/*
 * Copyright (C) 2010 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __DATA_MODEL_ERRORS_H__
#define __DATA_MODEL_ERRORS_H__

#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_ERRORS            (gda_data_model_errors_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaDataModelErrors, gda_data_model_errors, GDA, DATA_MODEL_ERRORS, GObject)

struct _GdaDataModelErrorsClass {
	GObjectClass            parent_class;
};

GdaDataModel *gda_data_model_errors_new          (void);

G_END_DECLS

#endif
