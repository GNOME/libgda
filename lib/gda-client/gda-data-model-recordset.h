/* GDA client library
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999,2000,2001 Rodrigo Moya
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

#if !defined(__gda_data_model_recordset_h__)
#  define __gda_data_model_recordset_h__

#include <gda-data-model.h>
#include <gda-recordset.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_RECORDSET            (gda_data_model_recordset_get_type())
#define GDA_DATA_MODEL_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_RECORDSET, GdaDataModelRecordset))
#define GDA_DATA_MODEL_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_RECORDSET, GdaDataModelRecordsetClass))
#define GDA_IS_DATA_MODEL_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_MODEL_RECORDSET))
#define GDA_IS_DATA_MODEL_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_DATA_MODEL_RECORDSET))

typedef struct _GdaDataModelRecordset        GdaDataModelRecordset;
typedef struct _GdaDataModelRecordsetClass   GdaDataModelRecordsetClass;
typedef struct _GdaDataModelRecordsetPrivate GdaDataModelRecordsetPrivate;

struct _GdaDataModelRecordset {
	GdaDataModel model;
	GdaDataModelRecordsetPrivate *priv;
};

struct _GdaDataModelRecordsetClass {
	GdaDataModelClass parent_class;
};

GType         gda_data_model_recordset_get_type (void);
GdaDataModel *gda_data_model_recordset_new (void);
GdaDataModel *gda_data_model_recordset_new_with_recordset (GdaRecordset *recset);

GdaRecordset *gda_data_model_recordset_get_recordset (GdaDataModelRecordset *model);
void          gda_data_model_recordset_set_recordset (GdaDataModelRecordset *model,
						      GdaRecordset *recset);

G_END_DECLS

#endif
