/* GDA common library
 * Copyright (C) 2007 - 2009 The GNOME Foundation.
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

#ifndef __GDA_DATA_MODEL_DIR_H__
#define __GDA_DATA_MODEL_DIR_H__

#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_DIR            (gda_data_model_dir_get_type())
#define GDA_DATA_MODEL_DIR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_DIR, GdaDataModelDir))
#define GDA_DATA_MODEL_DIR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_DIR, GdaDataModelDirClass))
#define GDA_IS_DATA_MODEL_DIR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_DATA_MODEL_DIR))
#define GDA_IS_DATA_MODEL_DIR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_DATA_MODEL_DIR))

typedef struct _GdaDataModelDir        GdaDataModelDir;
typedef struct _GdaDataModelDirClass   GdaDataModelDirClass;
typedef struct _GdaDataModelDirPrivate GdaDataModelDirPrivate;

struct _GdaDataModelDir {
	GObject                 object;
	GdaDataModelDirPrivate *priv;
};

struct _GdaDataModelDirClass {
	GObjectClass            parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType         gda_data_model_dir_get_type     (void) G_GNUC_CONST;
GdaDataModel *gda_data_model_dir_new          (const gchar *basedir);

const GSList *gda_data_model_dir_get_errors   (GdaDataModelDir *model);
void          gda_data_model_dir_clean_errors (GdaDataModelDir *model);
G_END_DECLS

#endif
