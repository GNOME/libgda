/*
 * Copyright (C) 2000 Reinhard MÃ¼ller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_VCONNECTION_DATA_MODEL_PRIVATE_H__
#define __GDA_VCONNECTION_DATA_MODEL_PRIVATE_H__

G_BEGIN_DECLS

typedef enum {
	PARAMS_INSERT,
	PARAMS_UPDATE,
	PARAMS_DELETE,
	PARAMS_NB
} ParamType;

typedef struct {
	GdaVconnectionDataModelSpec *spec;
	GDestroyNotify               spec_free_func;

	GdaDataModel                *real_model; /* data model really being used, a reference is kept on it */
	GList                       *columns;
	gchar                       *table_name;
	gchar                       *unique_name;
	gint                         n_columns;

	GdaStatement                *modif_stmt[PARAMS_NB];
	GdaSet                      *modif_params[PARAMS_NB];
} GdaVConnectionTableData;

void                     gda_vconnection_data_model_table_data_free (GdaVConnectionTableData *td);

GdaVConnectionTableData *gda_vconnection_get_table_data_by_name (GdaVconnectionDataModel *cnc, const gchar *table_name);
GdaVConnectionTableData *gda_vconnection_get_table_data_by_model (GdaVconnectionDataModel *cnc, GdaDataModel *model);
GdaVConnectionTableData *gda_vconnection_get_table_data_by_unique_name (GdaVconnectionDataModel *cnc, const gchar *unique_name);

G_END_DECLS

#endif
