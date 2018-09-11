/*
 * Copyright (C) 2006 - 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <despinosa@src.gnome.org>
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


#ifndef __GDA_DATA_PROXY_H_
#define __GDA_DATA_PROXY_H_

#include "gda-decl.h"
#include <glib-object.h>
#include <libgda/gda-value.h>
#include <libgda/gda-enums.h>


G_BEGIN_DECLS

#define GDA_TYPE_DATA_PROXY          (gda_data_proxy_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaDataProxy, gda_data_proxy, GDA, DATA_PROXY, GObject)

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

/* struct for the object's class */
struct _GdaDataProxyClass
{
	GObjectClass            parent_class;

	void                 (* row_delete_changed)   (GdaDataProxy *proxy, gint row, gboolean to_be_deleted);

	void                 (* sample_size_changed)  (GdaDataProxy *proxy, gint sample_size);
	void                 (* sample_changed)       (GdaDataProxy *proxy, gint sample_start, gint sample_end);

	GError              *(* validate_row_changes) (GdaDataProxy *proxy, gint row, gint proxied_row);
	void                 (* row_changes_applied)  (GdaDataProxy *proxy, gint row, gint proxied_row);

	void                 (* filter_changed)       (GdaDataProxy *proxy);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-data-proxy
 * @short_description: Proxy to hold modifications for any #GdaDataModel, providing the #GdaDataModel interface itself
 * @title: GdaDataProxy
 * @stability: Stable
 * @see_also: #GdaDataModel
 *
 * This object stores modifications to be made to a #GdaDataModel object which is proxied until the modifications are actually
 *  written to the #GdaDataModel, it can also filter the proxied data model to show only a subset (a defined number of continuous
 *  rows or by a filter to apply).
 *
 *  Specifically, for a proxied data model having <varname>nb_cols</varname> columns and <varname>nb_rows</varname> rows, 
 *  the #GdaDataProxy object has the following attributes:
 *  <itemizedlist>
 *    <listitem>
 *      <para><varname>2 * nb_cols</varname> columns:
 *	<itemizedlist>
 *	  <listitem><para>the first (&gt;= 0) <varname>nb_cols</varname> columns are the current values stored in the 
 *	      proxy (which correspond to the values of the proxied data model if the considered row has not been 
 *	      changed). The associated values are writable.</para></listitem>
 *	  <listitem><para>the last <varname>nb_cols</varname> columns are the values stored in the proxied data model, 
 *	      at column <varname>col - nb_cols</varname></para></listitem>
 *	</itemizedlist>
 *      </para>
 *    </listitem>
 *    <listitem><para>a variable number of rows depending on the following attributes:
 *	<itemizedlist>
 *	  <listitem><para>if the proxy is configured to have an empty row as the first row</para></listitem>
 *	  <listitem><para>if the proxy only displays parts of the proxied data model</para></listitem>
 *	  <listitem><para>if new rows have been added to the proxy</para></listitem>
 *	</itemizedlist>
 *    </para></listitem>
 *    <listitem><para>Any #GdaDataModelIter iterator created will only make appear the colmuns as present in the proxied
 *        data model, not any of the other columns</para></listitem>
 *  </itemizedlist>
 *  This situation is illustrated in the following schema, where there is a direct mapping between the proxy's
 *  rows and the proxied data model's rows:
 *  <mediaobject>
 *    <imageobject role="html">
 *      <imagedata fileref="data_proxy1.png" format="PNG" contentwidth="170mm"/>
 *    </imageobject>
 *    <textobject>
 *      <phrase>GdaDataProxy's values mapping regarding the proxied data model</phrase>
 *    </textobject>
 *  </mediaobject>
 *
 *  Note that unless explicitly mentioned, the columns are read-only.
 *
 *  The following figures illustrate row mappings between the data proxy and the proxied data model in 
 *  several situations (which can be combined, but are shown alone for simplicity):
 *  <itemizedlist>
 *    <listitem><para>situation where rows 1 and 5 have been marked as deleted from the data proxy, using
 *	<link linkend="gda-data-proxy-delete">gda_data_proxy_delete()</link> method, the data
 *	proxy has 2 rows less than the proxied data model:
 *	<mediaobject>
 *	  <imageobject role="html">
 *	    <imagedata fileref="data_proxy2.png" format="PNG" contentwidth="100mm"/>
 *	  </imageobject>
 *	  <textobject>
 *	    <phrase>GdaDataProxy with 2 rows marked as deleted</phrase>
 *	  </textobject>
 *	</mediaobject>
 *    </para></listitem>
 *    <listitem><para>situation where the data proxy only shows a sample of the proxied data model
 *	at any given time, using the 
 *	<link linkend="gda-data-proxy-set-sample-size">gda_data_proxy_set_sample_size()</link> method
 *	(the sample here is 4 rows wide, and starts at row 3):
 *	<mediaobject>
 *	  <imageobject role="html">
 *	    <imagedata fileref="data_proxy3.png" format="PNG" contentwidth="100mm"/>
 *	  </imageobject>
 *	  <textobject>
 *	    <phrase>GdaDataProxy with a sample size of 4</phrase>
 *	  </textobject>
 *	</mediaobject>
 *    </para></listitem>
 *    <listitem><para>situation where the data proxy shows a row of NULL values, using the
 *	<link linkend="GdaDataproxy-prepend-null-entry">"prepend-null-entry"</link> property:
 *	<mediaobject>
 *	  <imageobject role="html">
 *	    <imagedata fileref="data_proxy4.png" format="PNG" contentwidth="100mm"/>
 *	  </imageobject>
 *	  <textobject>
 *	    <phrase>GdaDataProxy with an extra row of NULL values</phrase>
 *	  </textobject>
 *	</mediaobject>
 *    </para></listitem>
 *    <listitem><para>situation where a row has been added to the data proxy, using for example the
 *	<link linkend="gda-data-model-append-row">gda_data_model_append_row()</link> method:
 *	<mediaobject>
 *	  <imageobject role="html">
 *	    <imagedata fileref="data_proxy5.png" format="PNG" contentwidth="100mm"/>
 *	  </imageobject>
 *	  <textobject>
 *	    <phrase>GdaDataProxy where a row has been added</phrase>
 *	  </textobject>
 *	</mediaobject>
 *    </para></listitem>
 *  </itemizedlist>
 *
 *  The #GdaDataProxy objects are thread safe, which means any proxy object can be used from
 *  any thread at the same time as they implement their own locking mechanisms.
 */

GObject          *gda_data_proxy_new                      (GdaDataModel *model);
GdaDataProxy     *gda_data_proxy_new_with_data_model      (GdaDataModel *model);

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
