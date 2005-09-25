/* GDA common library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_data_model_base_h__)
#  define __gda_data_model_base_h__

#include <glib-object.h>
#include <libgda/gda-command.h>
#include <libgda/gda-row.h>
#include <libgda/gda-value.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_BASE            (gda_data_model_base_get_type())
#define GDA_DATA_MODEL_BASE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_BASE, GdaDataModelBase))
#define GDA_DATA_MODEL_BASE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_BASE, GdaDataModelBaseClass))
#define GDA_IS_DATA_MODEL_BASE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_MODEL_BASE))
#define GDA_IS_DATA_MODEL_BASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_DATA_MODEL_BASE))

typedef struct _GdaDataModelBase        GdaDataModelBase;
typedef struct _GdaDataModelBaseClass   GdaDataModelBaseClass;
typedef struct _GdaDataModelBasePrivate GdaDataModelBasePrivate;

struct _GdaDataModelBase {
	GObject object;
	GdaDataModelBasePrivate *priv;
};

struct _GdaDataModelBaseClass {
	GObjectClass parent_class;

	/* virtual methods */
	gint                (* get_n_rows)      (GdaDataModelBase *model);
	gint                (* get_n_columns)   (GdaDataModelBase *model);
	GdaRow             *(* get_row)         (GdaDataModelBase *model, gint row);
	const GdaValue     *(* get_value_at)    (GdaDataModelBase *model, gint col, gint row);
	
	gboolean            (* is_updatable)    (GdaDataModelBase *model);
	GdaRow             *(* append_values)   (GdaDataModelBase *model, const GList *values);
	gboolean            (* append_row)      (GdaDataModelBase *model, GdaRow *row);
	gboolean            (* update_row)      (GdaDataModelBase *model, const GdaRow *row);
	gboolean            (* remove_row)      (GdaDataModelBase *model, const GdaRow *row);
	GdaColumn          *(* append_column)   (GdaDataModelBase *model);
	gboolean            (* remove_column)   (GdaDataModelBase *model, gint col);
};

GType               gda_data_model_base_get_type (void);


G_END_DECLS

#endif
