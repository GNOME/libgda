/* GDA
 * Copyright (C) 2007 The GNOME Foundation.
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

#ifndef __GDA_VCONNECTION_DATA_MODEL_PRIVATE_H__
#define __GDA_VCONNECTION_DATA_MODEL_PRIVATE_H__

G_BEGIN_DECLS

typedef struct {
	GdaVconnectionDataModelSpec *spec;
	GDestroyNotify               spec_free_func;

	GdaDataModel                *real_model; /* data model really being used, a reference count is kept on it */
	GList                       *columns;
	gchar                       *table_name;
	gchar                       *unique_name;

	gpointer _gda_reserved_1;
	gpointer _gda_reserved_2;
} GdaVConnectionTableData;

void                     gda_vconnection_data_model_table_data_free (GdaVConnectionTableData *td);

GdaVConnectionTableData *gda_vconnection_get_table_data_by_name (GdaVconnectionDataModel *cnc, const gchar *table_name);
GdaVConnectionTableData *gda_vconnection_get_table_data_by_model (GdaVconnectionDataModel *cnc, GdaDataModel *model);
GdaVConnectionTableData *gda_vconnection_get_table_data_by_unique_name (GdaVconnectionDataModel *cnc, const gchar *unique_name);

G_END_DECLS

#endif
