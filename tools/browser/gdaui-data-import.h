/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GDAUI_DATA_IMPORT__
#define __GDAUI_DATA_IMPORT__

#include <gtk/gtk.h>
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_IMPORT          (gdaui_data_import_get_type())
#define GDAUI_DATA_IMPORT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_data_import_get_type(), GdauiDataImport)
#define GDAUI_DATA_IMPORT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_data_import_get_type (), GdauiDataImportClass)
#define GDAUI_IS_DATA_IMPORT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_data_import_get_type ())


typedef struct _GdauiDataImport      GdauiDataImport;
typedef struct _GdauiDataImportClass GdauiDataImportClass;
typedef struct _GdauiDataImportPriv  GdauiDataImportPriv;

/* struct for the object's data */
struct _GdauiDataImport
{
	GtkPaned              object;

	GdauiDataImportPriv *priv;
};

/* struct for the object's class */
struct _GdauiDataImportClass
{
	GtkPanedClass         parent_class;
};

/* 
 * Generic widget's methods 
*/
GType         gdaui_data_import_get_type     (void) G_GNUC_CONST; 
GtkWidget    *gdaui_data_import_new          (void);
GdaDataModel *gdaui_data_import_get_model    (GdauiDataImport *import);


G_END_DECLS

#endif



