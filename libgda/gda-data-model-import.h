/* GDA common library
 * Copyright (C) 2006 The GNOME Foundation.
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

#ifndef __GDA_DATA_MODEL_IMPORT_H__
#define __GDA_DATA_MODEL_IMPORT_H__

#include <libgda/gda-object.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_IMPORT            (gda_data_model_import_get_type())
#define GDA_DATA_MODEL_IMPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_IMPORT, GdaDataModelImport))
#define GDA_DATA_MODEL_IMPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_IMPORT, GdaDataModelImportClass))
#define GDA_IS_DATA_MODEL_IMPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_DATA_MODEL_IMPORT))
#define GDA_IS_DATA_MODEL_IMPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_DATA_MODEL_IMPORT))

typedef struct _GdaDataModelImport        GdaDataModelImport;
typedef struct _GdaDataModelImportClass   GdaDataModelImportClass;
typedef struct _GdaDataModelImportPrivate GdaDataModelImportPrivate;

struct _GdaDataModelImport {
	GdaObject                  object;
	GdaDataModelImportPrivate *priv;
};

struct _GdaDataModelImportClass {
	GdaObjectClass             parent_class;
};

GType         gda_data_model_import_get_type     (void);
GdaDataModel *gda_data_model_import_new_file     (const gchar *filename, gboolean random_access,
						  GdaParameterList *options);
GdaDataModel *gda_data_model_import_new_mem      (const gchar *data, gboolean random_access, 
						  GdaParameterList *options);
GdaDataModel *gda_data_model_import_new_xml_node (xmlNodePtr node);

GSList       *gda_data_model_import_get_errors   (GdaDataModelImport *model);
void          gda_data_model_import_clean_errors (GdaDataModelImport *model);

G_END_DECLS

#endif
