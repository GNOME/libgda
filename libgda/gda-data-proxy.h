/* gda-data-proxy.h
 *
 * Copyright (C) 2005 - 2008 Vivien Malerba
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


#ifndef __GDA_DATA_PROXY_H_
#define __GDA_DATA_PROXY_H_

#include "gda-decl.h"
#include <glib-object.h>
#include <libgda/gda-value.h>
#include <libgda/gda-enums.h>


G_BEGIN_DECLS

#define GDA_TYPE_DATA_PROXY          (gda_data_proxy_get_type())
#define GDA_DATA_PROXY(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_data_proxy_get_type(), GdaDataProxy)
#define GDA_DATA_PROXY_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_data_proxy_get_type (), GdaDataProxyClass)
#define GDA_IS_DATA_PROXY(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_data_proxy_get_type ())

/* error reporting */
extern GQuark gda_data_proxy_error_quark (void);
#define GDA_DATA_PROXY_ERROR gda_data_proxy_error_quark ()

typedef enum {
	GDA_DATA_PROXY_COMMIT_ERROR,
	GDA_DATA_PROXY_COMMIT_CANCELLED,
	GDA_DATA_PROXY_READ_ONLY_VALUE,
	GDA_DATA_PROXY_READ_ONLY_ROW,
	GDA_DATA_PROXY_FILTER_ERROR
} GdaDataProxyError;

/* struct for the object's data */
struct _GdaDataProxy
{
	GObject                 object;
	GdaDataProxyPrivate    *priv;
};


/* struct for the object's class */
struct _GdaDataProxyClass
{
	GObjectClass            parent_class;

	void                 (* row_delete_changed)   (GdaDataProxy *proxy, gint row, gboolean to_be_deleted);

	void                 (* sample_size_changed)  (GdaDataProxy *proxy, gint sample_size);
	void                 (* sample_changed)       (GdaDataProxy *proxy, gint sample_start, gint sample_end);

	GError              *(* validate_row_changes) (GdaDataProxy *proxy, gint row, gint proxied_row);
	void                 (* row_changes_applied)  (GdaDataProxy *proxy, gint row, gint proxied_row);
};

GType             gda_data_proxy_get_type                 (void) G_GNUC_CONST;
GObject          *gda_data_proxy_new                      (GdaDataModel *model);

GdaDataModel     *gda_data_proxy_get_proxied_model        (GdaDataProxy *proxy);
gint              gda_data_proxy_get_proxied_model_n_cols (GdaDataProxy *proxy);
gint              gda_data_proxy_get_proxied_model_n_rows (GdaDataProxy *proxy);
gboolean          gda_data_proxy_is_read_only             (GdaDataProxy *proxy);
GSList           *gda_data_proxy_get_values               (GdaDataProxy *proxy, gint proxy_row, 
						           gint *cols_index, gint n_cols);
GdaValueAttribute gda_data_proxy_get_value_attributes     (GdaDataProxy *proxy, gint proxy_row, gint col);
void              gda_data_proxy_alter_value_attributes   (GdaDataProxy *proxy, gint proxy_row, gint col, GdaValueAttribute alter_flags);
gint              gda_data_proxy_get_proxied_model_row    (GdaDataProxy *proxy, gint proxy_row);

void              gda_data_proxy_delete                   (GdaDataProxy *proxy, gint proxy_row);
void              gda_data_proxy_undelete                 (GdaDataProxy *proxy, gint proxy_row);
gboolean          gda_data_proxy_row_is_deleted           (GdaDataProxy *proxy, gint proxy_row);
gboolean          gda_data_proxy_row_is_inserted          (GdaDataProxy *proxy, gint proxy_row);

gboolean          gda_data_proxy_row_has_changed          (GdaDataProxy *proxy, gint proxy_row);
gboolean          gda_data_proxy_has_changed              (GdaDataProxy *proxy);
gint              gda_data_proxy_get_n_new_rows           (GdaDataProxy *proxy);
gint              gda_data_proxy_get_n_modified_rows      (GdaDataProxy *proxy);

gboolean          gda_data_proxy_apply_row_changes        (GdaDataProxy *proxy, gint proxy_row, GError **error);
void              gda_data_proxy_cancel_row_changes       (GdaDataProxy *proxy, gint proxy_row, gint col);

gboolean          gda_data_proxy_apply_all_changes        (GdaDataProxy *proxy, GError **error);
gboolean          gda_data_proxy_cancel_all_changes       (GdaDataProxy *proxy);

void              gda_data_proxy_set_sample_size          (GdaDataProxy *proxy, gint sample_size);
gint              gda_data_proxy_get_sample_size          (GdaDataProxy *proxy);
void              gda_data_proxy_set_sample_start         (GdaDataProxy *proxy, gint sample_start);
gint              gda_data_proxy_get_sample_start         (GdaDataProxy *proxy);
gint              gda_data_proxy_get_sample_end           (GdaDataProxy *proxy);

gboolean          gda_data_proxy_set_filter_expr          (GdaDataProxy *proxy, const gchar *filter_expr, GError **error);
const gchar      *gda_data_proxy_get_filter_expr          (GdaDataProxy *proxy);
gboolean          gda_data_proxy_set_ordering_column      (GdaDataProxy *proxy, gint col, GError **error);
gint              gda_data_proxy_get_filtered_n_rows      (GdaDataProxy *proxy);

G_END_DECLS

#endif
