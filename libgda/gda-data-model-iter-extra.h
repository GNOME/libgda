/* gda-data-model-iter-extra.h
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


#ifndef __GDA_DATA_MODEL_ITER_EXTRA_H_
#define __GDA_DATA_MODEL_ITER_EXTRA_H_

#include "gda-data-model-iter.h"

G_BEGIN_DECLS

gboolean gda_data_model_iter_move_at_row_default (GdaDataModel *model, GdaDataModelIter *iter, gint row);
gboolean gda_data_model_iter_move_next_default   (GdaDataModel *model, GdaDataModelIter *iter);
gboolean gda_data_model_iter_move_prev_default   (GdaDataModel *model, GdaDataModelIter *iter);

G_END_DECLS

#endif
