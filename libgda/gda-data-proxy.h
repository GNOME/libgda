/* gda-data-proxy.h
 *
 * Copyright (C) 2005 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef __GDA_DATA_PROXY_H_
#define __GDA_DATA_PROXY_H_

#include "gda-decl.h"
#include <libgda/gda-object.h>
#include <libgda/gda-value.h>


G_BEGIN_DECLS

#define GDA_TYPE_DATA_PROXY          (gda_data_proxy_get_type())
#define GDA_DATA_PROXY(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_data_proxy_get_type(), GdaDataProxy)
#define GDA_DATA_PROXY_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_data_proxy_get_type (), GdaDataProxyClass)
#define GDA_IS_DATA_PROXY(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_data_proxy_get_type ())

/* error reporting */
extern GQuark gda_data_proxy_error_quark (void);
#define GDA_DATA_PROXY_ERROR gda_data_proxy_error_quark ()

enum {
	GDA_DATA_PROXY_COMMIT_ERROR
};

enum {
	PROXY_COL_MODEL_N_COLUMNS = -5, /* number of columns in the GdaDataModel */
	PROXY_COL_MODEL_POINTER   = -4, /* pointer to the GdaDataModel */
	PROXY_COL_MODEL_ROW       = -3, /* row number in the GdaDataModel, or -1 for new rows */
	PROXY_COL_MODIFIED        = -2, /* TRUE if row has been modified */
	PROXY_COL_TO_DELETE       = -1, /* TRUE if row is marked to be deleted */
};
#define PROXY_NB_GEN_COLUMNS -PROXY_COL_MODEL_N_COLUMNS

/* struct for the object's data */
struct _GdaDataProxy
{
	GdaObject                  object;
	GdaDataProxyPrivate       *priv;
};


/* struct for the object's class */
struct _GdaDataProxyClass
{
	GdaObjectClass             parent_class;
};

GType             gda_data_proxy_get_type                 (void);
GObject          *gda_data_proxy_new                      (GdaDataModel *model);

GdaDataModel     *gda_data_proxy_get_proxied_model        (GdaDataProxy *proxy);
gint              gda_data_proxy_get_proxied_model_n_cols (GdaDataProxy *proxy);
gboolean          gda_data_proxy_is_read_only             (GdaDataProxy *proxy);
GSList           *gda_data_proxy_get_values               (GdaDataProxy *proxy, gint proxy_row, 
						           gint *cols_index, gint n_cols);
void              gda_data_proxy_delete                   (GdaDataProxy *proxy, gint proxy_row);
void              gda_data_proxy_undelete                 (GdaDataProxy *proxy, gint proxy_row);

gint              gda_data_proxy_find_row_from_values     (GdaDataProxy *proxy, GSList *values, 
						           gint *cols_index);
gboolean          gda_data_proxy_has_changed              (GdaDataProxy *proxy);

gboolean          gda_data_proxy_apply_row_changes        (GdaDataProxy *proxy, gint proxy_row, GError **error);
void              gda_data_proxy_cancel_row_changes       (GdaDataProxy *proxy, gint proxy_row, gint col);

gboolean          gda_data_proxy_apply_all_changes        (GdaDataProxy *proxy, GError **error);
gboolean          gda_data_proxy_cancel_all_changes       (GdaDataProxy *proxy);

void              gda_data_proxy_set_sample_size          (GdaDataProxy *proxy, gint sample_size);
gint              gda_data_proxy_get_sample_size          (GdaDataProxy *proxy);
void              gda_data_proxy_set_sample_start         (GdaDataProxy *proxy, gint sample_start);
gint              gda_data_proxy_get_sample_start         (GdaDataProxy *proxy);
gint              gda_data_proxy_get_sample_end           (GdaDataProxy *proxy);

void              gda_data_proxy_store_model_row_value    (GdaDataProxy *proxy, GdaDataModel *model, gint extra_col, 
							   const GdaValue *value);
const GdaValue   *gda_data_proxy_get_model_row_value      (GdaDataProxy *proxy, GdaDataModel *model, gint extra_col);
void              gda_data_proxy_assign_model_col         (GdaDataProxy *proxy, GdaDataModel *model, gint model_col, gint proxy_col);

G_END_DECLS

#endif
