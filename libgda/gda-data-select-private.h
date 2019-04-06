/*
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_DATA_SELECT_PRIVATE_H__
#define __GDA_DATA_SELECT_PRIVATE_H__

/**
 * gda_data_select_get_prep_stmt:
 *
 * Returns: (transfer none):
 */
GdaPStmt               *gda_data_select_get_prep_stmt (GdaDataSelect *model);
gint                    gda_data_select_get_nb_stored_rows (GdaDataSelect *model);
gint                    gda_data_select_get_advertized_nrows (GdaDataSelect *model);
void                    gda_data_select_set_advertized_nrows (GdaDataSelect *model, gint n);


G_END_DECLS

#endif
