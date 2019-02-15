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

#ifndef __GDA_DATA_MODEL_SELECT_H__
#define __GDA_DATA_MODEL_SELECT_H__

#include <glib-object.h>
#include <gda-connection.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_SELECT            (gda_data_model_select_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaDataModelSelect, gda_data_model_select, GDA, DATA_MODEL_SELECT, GObject)

struct _GdaDataModelSelectClass {
  GObjectClass      parent_class;
  /* signals */
  void              (* updated)       (GdaDataModelSelect *model);
};

GdaDataModelSelect *gda_data_model_select_new             (GdaConnection *cnc, GdaStatement *stm, GdaSet *params);
GdaDataModelSelect *gda_data_model_select_new_from_string (GdaConnection *cnc, const gchar *sql);
gboolean            gda_data_model_select_is_valid        (GdaDataModelSelect *model);
GdaSet             *gda_data_model_select_get_parameters  (GdaDataModelSelect *model);
void                gda_data_model_select_set_parameters  (GdaDataModelSelect *model, GdaSet *params);

G_END_DECLS

#endif
