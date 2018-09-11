/*
 * Copyright (C) 2006 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_DATA_MODEL_IMPORT_H__
#define __GDA_DATA_MODEL_IMPORT_H__

#include <glib-object.h>
#include <libxml/tree.h>
#include <libgda/gda-set.h>
#include <libgda/gda-data-model-iter.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_IMPORT            (gda_data_model_import_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaDataModelImport, gda_data_model_import, GDA, DATA_MODEL_IMPORT, GObject)
struct _GdaDataModelImportClass {
	GObjectClass               parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-data-model-import
 * @short_description: Importing data from a string or a file
 * @title: GdaDataModelImport
 * @stability: Stable
 * @see_also: #GdaDataModel
 *
 * The #GdaDataModelImport data model imports data from a string or a file. The data can either be
 * in a CSV (comma separated values) format or in an XML format as described by the libgda-array.dtd DTD (as a side
 * way it is also possible to import data from an already-build XML tree validated against that DTD).
 *
 * The caller must decide, upon construction, if the new #GdaDataModelImport must support random access or simply
 * a cursor based access. Random access makes it easier to use the resulting data model but consumes more memory as
 * all the data is copied in memory, and is thus not suitable for large data sets. Note that importing from an 
 * already-build XML tree will always result in a random access data model.
 *
 * Various import options can be specified using parameters in a #GdaSet object. The available options
 * depend on the format of the imported data listed here:
 * <itemizedlist>
 *   <listitem><para>"SEPARATOR" (string, CVS import only): specifies the separator to consider</para></listitem>
 *   <listitem><para>"ESCAPE_CHAR" (string, CVS import only): specifies the character used to "escape" the strings
 *       contained between two separators</para></listitem>
 *   <listitem><para>"ENCODING" (string, CVS import only): specifies the character set used in the imported data</para></listitem>
 *   <listitem><para>"TITLE_AS_FIRST_LINE" (boolean, CVS import only): TRUE to specify that the first line of the 
 *       imported data contains the column names</para></listitem>
 *   <listitem><para>"G_TYPE_&lt;col number&gt;" (GType, CVS import only): specifies the requested GType type for the column
 * 	numbered "col number"</para></listitem>
 * </itemizedlist>
 */


GdaDataModel *gda_data_model_import_new_file     (const gchar *filename, gboolean random_access, GdaSet *options);
GdaDataModel *gda_data_model_import_new_mem      (const gchar *data, gboolean random_access, GdaSet *options);
GdaDataModel *gda_data_model_import_new_xml_node (xmlNodePtr node);

GSList       *gda_data_model_import_get_errors   (GdaDataModelImport *model);
void          gda_data_model_import_clean_errors (GdaDataModelImport *model);


#define GDA_TYPE_DATA_MODEL_IMPORT_ITER gda_data_model_import_iter_get_type()

G_DECLARE_DERIVABLE_TYPE(GdaDataModelImportIter, gda_data_model_import_iter, GDA, DATA_MODEL_IMPORT_ITER, GdaDataModelIter)

struct _GdaDataModelImportIterClass {
	GdaDataModelIterClass parent_class;
};

G_END_DECLS

#endif
