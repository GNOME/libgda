/*
 * Copyright (C) 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DATA_PIVOT_H__
#define __GDA_DATA_PIVOT_H__

#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_PIVOT            (gda_data_pivot_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaDataPivot, gda_data_pivot, GDA, DATA_PIVOT, GObject)

struct _GdaDataPivotClass {
	GObjectClass           parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/* error reporting */
extern GQuark gda_data_pivot_error_quark (void);
#define GDA_DATA_PIVOT_ERROR gda_data_pivot_error_quark ()

/**
 * GdaDataPivotError:
 *
 * Possible #GdaDataPivot related errors.
 */
typedef enum {
	GDA_DATA_PIVOT_INTERNAL_ERROR,
	GDA_DATA_PIVOT_SOURCE_MODEL_ERROR,
        GDA_DATA_PIVOT_FIELD_FORMAT_ERROR,
	GDA_DATA_PIVOT_USAGE_ERROR,
	GDA_DATA_PIVOT_OVERFLOW_ERROR
} GdaDataPivotError;

/**
 * GdaDataPivotAggregate:
 * @GDA_DATA_PIVOT_AVG:
 * @GDA_DATA_PIVOT_COUNT:
 * @GDA_DATA_PIVOT_MAX:
 * @GDA_DATA_PIVOT_MIN:
 * @GDA_DATA_PIVOT_SUM:
 *
 * Possible operations for the data fields.
 */
typedef enum {
	GDA_DATA_PIVOT_AVG,
	GDA_DATA_PIVOT_COUNT,
	GDA_DATA_PIVOT_MAX,
	GDA_DATA_PIVOT_MIN,
	GDA_DATA_PIVOT_SUM
} GdaDataPivotAggregate;

/**
 * SECTION:gda-data-pivot
 * @short_description: A data model for data summarisation
 * @title: GdaDataPivot
 * @stability: Stable
 *
 * The #GdaDataPivot data model allows one to do some analysis and summarisation on the contents
 * of a data model.
 *
 * 
 */

/**
 * GdaDataPivotFieldType:
 * @GDA_DATA_PIVOT_FIELD_ROW:
 * @GDA_DATA_PIVOT_FIELD_COLUMN:
 *
 * Define types of field to be used when defining a #GdaDataPivot analysis.
 */
typedef enum {
	GDA_DATA_PIVOT_FIELD_ROW,
	GDA_DATA_PIVOT_FIELD_COLUMN
} GdaDataPivotFieldType;

GdaDataModel *gda_data_pivot_new         (GdaDataModel *model);

gboolean      gda_data_pivot_add_field   (GdaDataPivot *pivot, GdaDataPivotFieldType field_type,
					  const gchar *field, const gchar *alias, GError **error);
gboolean      gda_data_pivot_add_data    (GdaDataPivot *pivot, GdaDataPivotAggregate aggregate_type,
					  const gchar *field, const gchar *alias, GError **error);

gboolean      gda_data_pivot_populate    (GdaDataPivot *pivot, GError **error);


G_END_DECLS

#endif
