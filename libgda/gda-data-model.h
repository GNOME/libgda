/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_data_model_h__)
#  define __gda_data_model_h__

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-command.h>
#include <libgda/gda-row.h>
#include <libgda/gda-value.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL            (gda_data_model_get_type())
#define GDA_DATA_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL, GdaDataModel))
#define GDA_DATA_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL, GdaDataModelClass))
#define GDA_IS_DATA_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_MODEL))
#define GDA_IS_DATA_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_DATA_MODEL))

typedef struct _GdaDataModel        GdaDataModel;
typedef struct _GdaDataModelClass   GdaDataModelClass;
typedef struct _GdaDataModelPrivate GdaDataModelPrivate;

struct _GdaDataModel {
	GObject object;
	GdaDataModelPrivate *priv;
};

struct _GdaDataModelClass {
	GObjectClass parent_class;

	/* signals */
	void (* changed) (GdaDataModel *model);

	void (* begin_edit) (GdaDataModel *model);
	void (* cancel_edit) (GdaDataModel *model);
	void (* end_edit) (GdaDataModel *model);

	/* virtual methods */
	gint (* get_n_rows) (GdaDataModel *model);
	gint (* get_n_columns) (GdaDataModel *model);
	GdaFieldAttributes * (* describe_column) (GdaDataModel *model, gint col);
	const GdaRow * (* get_row) (GdaDataModel *model, gint row);
	const GdaValue * (* get_value_at) (GdaDataModel *model, gint col, gint row);

	gboolean (* is_editable) (GdaDataModel *model);
	const GdaRow * (* append_row) (GdaDataModel *model, const GList *values);
	gboolean (* remove_row) (GdaDataModel *model, const GdaRow *row);
	gboolean (* update_row) (GdaDataModel *model, const GdaRow *row);
};

GType               gda_data_model_get_type (void);

void                gda_data_model_changed (GdaDataModel *model);
void                gda_data_model_freeze (GdaDataModel *model);
void                gda_data_model_thaw (GdaDataModel *model);

gint                gda_data_model_get_n_rows (GdaDataModel *model);
gint                gda_data_model_get_n_columns (GdaDataModel *model);
GdaFieldAttributes *gda_data_model_describe_column (GdaDataModel *model, gint col);
const gchar        *gda_data_model_get_column_title (GdaDataModel *model, gint col);
void                gda_data_model_set_column_title (GdaDataModel *model, gint col, const gchar *title);
gint                gda_data_model_get_column_position (GdaDataModel *model, const gchar *title);
const GdaRow       *gda_data_model_get_row (GdaDataModel *model, gint row);
const GdaValue     *gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row);

gboolean            gda_data_model_is_editable (GdaDataModel *model);
const GdaRow       *gda_data_model_append_row (GdaDataModel *model, const GList *values);
gboolean            gda_data_model_remove_row (GdaDataModel *model, const GdaRow *row);
gboolean            gda_data_model_update_row (GdaDataModel *model, const GdaRow *row);

typedef gboolean (* GdaDataModelForeachFunc) (GdaDataModel *model,
					      GdaRow *row,
					      gpointer user_data);

void                gda_data_model_foreach (GdaDataModel *model,
					    GdaDataModelForeachFunc func,
					    gpointer user_data);

gboolean            gda_data_model_is_editing (GdaDataModel *model);
gboolean            gda_data_model_begin_edit (GdaDataModel *model);
gboolean            gda_data_model_cancel_edit (GdaDataModel *model);
gboolean            gda_data_model_end_edit (GdaDataModel *model);

gchar              *gda_data_model_to_comma_separated (GdaDataModel *model);
gchar              *gda_data_model_to_tab_separated (GdaDataModel *model);
gchar              *gda_data_model_to_xml (GdaDataModel *model, gboolean standalone);
xmlNodePtr          gda_data_model_to_xml_node (GdaDataModel *model, const gchar *name);

const gchar        *gda_data_model_get_command_text (GdaDataModel *recset);
void                gda_data_model_set_command_text (GdaDataModel *recset, const gchar *txt);
GdaCommandType      gda_data_model_get_command_type (GdaDataModel *recset);
void                gda_data_model_set_command_type (GdaDataModel *recset,
						     GdaCommandType type);

G_END_DECLS

#endif
