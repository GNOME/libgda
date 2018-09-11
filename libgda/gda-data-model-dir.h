/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DATA_MODEL_DIR_H__
#define __GDA_DATA_MODEL_DIR_H__

#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_DIR            (gda_data_model_dir_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaDataModelDir, gda_data_model_dir, GDA, DATA_MODEL_DIR, GObject)
struct _GdaDataModelDirClass {
	GObjectClass            parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-data-model-dir
 * @short_description: GdaDataModel to list files in filesystem
 * @title: GdaDataModelDir
 * @stability: Stable
 * @see_also: #GdaDataModel
 *
 * The #GdaDataModelDir object lists files on a filesystem which are located
 * below a "basedir" directory, one file per row. The data model has the following columns:
 * <itemizedlist>
 *   <listitem><para>the "dir_name" column (G_TYPE_STRING): contains the dirname part of the file</para></listitem>
 *   <listitem><para>the "file_name" column (G_TYPE_STRING): contains the file name part of the file</para></listitem>
 *   <listitem><para>the "size" column (G_TYPE_UINT): contains the size in bytes of the file</para></listitem>
 *   <listitem><para>the "mime_type" column (G_TYPE_STRING): contains the mime type of the file (if GnomeVFS has been found, and NULL otherwise)</para></listitem>
 *   <listitem><para>the "md5sum" column (G_TYPE_STRING): contains the MD5 hash of each file (if LibGCrypt has been found, and NULL otherwise)</para></listitem>
 *   <listitem><para>the "data" column (GDA_TYPE_BLOB): contains the contents of each file</para></listitem>
 * </itemizedlist>
 *
 * Note that the actual values of the "mime_type", "md5sum" and "data" columns are computed only when they
 *  are requested to help with performances.
 */

GdaDataModel *gda_data_model_dir_new          (const gchar *basedir);

const GSList *gda_data_model_dir_get_errors   (GdaDataModelDir *model);
void          gda_data_model_dir_clean_errors (GdaDataModelDir *model);
G_END_DECLS

#endif
