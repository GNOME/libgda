/*
 * Copyright (C) 2006 - 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2012 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __GDA_DATA_MODEL_ITER_H_
#define __GDA_DATA_MODEL_ITER_H_

#include "gda-decl.h"
#include "gda-set.h"

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_ITER          (gda_data_model_iter_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaDataModelIter, gda_data_model_iter, GDA, DATA_MODEL_ITER, GdaSet)

/* struct for the object's class */
struct _GdaDataModelIterClass
{
	GdaSetClass                parent_class;

	/* Virtual methods */
	gboolean             (* move_to_row)      (GdaDataModelIter *iter, gint row);
	gboolean             (* move_next)        (GdaDataModelIter *iter);
	gboolean             (* move_prev)        (GdaDataModelIter *iter);
	gboolean             (* set_value_at)     (GdaDataModelIter *iter, gint col,
                      const GValue *value, GError **error);

	/* Signals */
	void                    (* row_changed)      (GdaDataModelIter *iter, gint row);
	void                    (* end_of_data)      (GdaDataModelIter *iter);

	/*< private >*/
	/* Padding for future expansion */
	gpointer padding[12];
};

/* error reporting */
extern GQuark gda_data_model_iter_error_quark (void);
#define GDA_DATA_MODEL_ITER_ERROR gda_data_model_iter_error_quark ()

typedef enum
{
	GDA_DATA_MODEL_ITER_COLUMN_OUT_OF_RANGE_ERROR
} GdaDataModelIterError;


/**
 * SECTION:gda-data-model-iter
 * @short_description: Data model iterator
 * @title: GdaDataModelIter
 * @stability: Stable
 * @see_also: #GdaDataModel
 *
 * A #GdaDataModelIter object is used to iterate through the rows of a #GdaDataModel. If the data model is accessible
 * in a random access way then any number of #GdaDataModelIter objects can be created on the same data model, and
 * if the data model only supports a cursor based access then only one #GdaDataModelIter can be created. In any case
 * creating a #GdaDataModelIter should be done using the gda_data_model_create_iter() method. Note that if 
 * the data model only supports a cursor based access, then calling this method several times will always return
 * the same #GdaDataModelIter, but with its reference count increased by 1 (so you should call g_object_unref() when
 * finished with it).
 *
 * When a #GdaDataModelIter is valid (that is when it points to an existing row in the data model it iterates through),
 * the individual values (corresponding to each column of the data model, at the pointer row) can be accessed 
 * using the gda_data_model_iter_get_value_at() or gda_data_model_iter_get_value_for_field() methods
 * (or in the same way #GdaSet's values are accessed as #GdaDataModelIter inherits the #GdaSet).
 *
 * Right after being created, a #GdaDataModelIter is invalid (does not point to any row of its data model). To read the
 * first row of the data model, use the gda_data_model_iter_move_next() method. Calling this method several times will
 * move the iterator forward, up to when the data model has no more rows and the #GdaDataModelIter will be declared invalid
 * (and gda_data_model_iter_move_next() has returned FALSE). Note that at this point, the number of rows in the data
 * model will be known.
 *
 * If the data model supports it, a #GdaDataModelIter can be moved backwards using the gda_data_model_iter_move_prev()
 * method. However if the iterator is invalid, moving backwards will not be possible (on the contrary to 
 * gda_data_model_iter_move_next() which moves to the first row).
 *
 * The gda_data_model_iter_move_to_row() method, if the iterator can be moved both forward and backwards, can move the 
 * iterator to a specific row (sometimes faster than moving it forward or backwards a number of times).
 *
 * The following figure illustrates the #GdaDataModelIter usage:
 * <mediaobject>
 *   <imageobject role="html">
 *     <imagedata fileref="GdaDataModelIter.png" format="PNG" contentwidth="190mm"/>
 *   </imageobject>
 *   <textobject>
 *     <phrase>GdaDataModelIter's usage</phrase>
 *   </textobject>
 * </mediaobject>
 *
 * Note: the new #GdaDataModelIter does not hold any reference to the data model it iterates through (ie.
 * if this data model is destroyed at some point, the new iterator will become useless but in
 * any case it will not prevent the data model from being destroyed).
 *
 * Note: when the data model emits the "reset" signal, then:
 * <itemizedlist>
 *  <listitem><para>the number of columns of the iterator can change to reflect the new data model
 *    being itered on. In this case the iterator's position is reset as if it was
 *    just created</para></listitem>
 *  <listitem><para>some column types which were unknown (i.e. GDA_TYPE_NULL type), can change
 *    to their correct type. In this case there is no other iterator change</para></listitem>
 *  <listitem><para>some column types which were not GDA_TYPE_NULL can change, and in this case
 *    the iterator's position is reset as if it was just created</para></listitem>
 * </itemizedlist>
 */

const GValue     *gda_data_model_iter_get_value_at         (GdaDataModelIter *iter, gint col);
const GValue     *gda_data_model_iter_get_value_at_e       (GdaDataModelIter *iter, gint col, GError **error);
const GValue     *gda_data_model_iter_get_value_for_field  (GdaDataModelIter *iter, const gchar *field_name);
gboolean          gda_data_model_iter_set_value_at         (GdaDataModelIter *iter, gint col, 
							    const GValue *value, GError **error);

gboolean          gda_data_model_iter_move_to_row          (GdaDataModelIter *iter, gint row);
gboolean          gda_data_model_iter_move_next            (GdaDataModelIter *iter);
gboolean          gda_data_model_iter_move_prev            (GdaDataModelIter *iter);
gint              gda_data_model_iter_get_row              (GdaDataModelIter *iter);

void              gda_data_model_iter_invalidate_contents  (GdaDataModelIter *iter);
gboolean          gda_data_model_iter_is_valid             (GdaDataModelIter *iter);

GdaHolder        *gda_data_model_iter_get_holder_for_field (GdaDataModelIter *iter, gint col);

#define gda_data_model_iter_move_at_row gda_data_model_iter_move_to_row

G_END_DECLS

#endif
