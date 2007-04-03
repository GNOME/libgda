/* gda-data-model-iter.h
 *
 * Copyright (C) 2005 ( 2006 Vivien Malerba
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


#ifndef __GDA_DATA_MODEL_ITER_H_
#define __GDA_DATA_MODEL_ITER_H_

#include "gda-decl.h"
#include "gda-parameter-list.h"

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_ITER          (gda_data_model_iter_get_type())
#define GDA_DATA_MODEL_ITER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_data_model_iter_get_type(), GdaDataModelIter)
#define GDA_DATA_MODEL_ITER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_data_model_iter_get_type (), GdaDataModelIterClass)
#define GDA_IS_DATA_MODEL_ITER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_data_model_iter_get_type ())


/* error reporting */
extern GQuark gda_data_model_iter_error_quark (void);
#define GDA_DATA_MODEL_ITER_ERROR gda_data_model_iter_error_quark ()

typedef enum
{
	GDA_DATA_MODEL_ITER_WHAT_ERROR,
} GdaDataModelIterError;


/* struct for the object's data */
struct _GdaDataModelIter
{
	GdaParameterList           object;
	GdaDataModelIterPrivate   *priv;
};

/* struct for the object's class */
struct _GdaDataModelIterClass
{
	GdaParameterListClass      parent_class;

	gboolean                (* row_to_change)    (GdaDataModelIter *iter, gint row);
	void                    (* row_changed)      (GdaDataModelIter *iter, gint row);
	void                    (* end_of_data)      (GdaDataModelIter *iter);
};

GType             gda_data_model_iter_get_type             (void);
GdaDataModelIter *gda_data_model_iter_new                  (GdaDataModel *model);

gboolean          gda_data_model_iter_set_at_row           (GdaDataModelIter *iter, gint row);
gboolean          gda_data_model_iter_move_next            (GdaDataModelIter *iter);
gboolean          gda_data_model_iter_move_prev            (GdaDataModelIter *iter);
gint              gda_data_model_iter_get_row              (GdaDataModelIter *iter);

gboolean          gda_data_model_iter_can_be_moved       (GdaDataModelIter *iter);
void              gda_data_model_iter_invalidate_contents  (GdaDataModelIter *iter);
gboolean          gda_data_model_iter_is_valid             (GdaDataModelIter *iter);

gint              gda_data_model_iter_get_column_for_param (GdaDataModelIter *iter, GdaParameter *param);
GdaParameter     *gda_data_model_iter_get_param_for_column (GdaDataModelIter *iter, gint col);

G_END_DECLS

#endif
