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

#if !defined(__gda_data_model_h__)
#  define __gda_data_model_h__

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-command.h>
#include <libgda/global-decl.h>
#include <libgda/gda-column.h>
#include <libgda/gda-value.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL            (gda_data_model_get_type())
#define GDA_DATA_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL, GdaDataModel))
#define GDA_IS_DATA_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_MODEL))
#define GDA_DATA_MODEL_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDA_TYPE_DATA_MODEL, GdaDataModelIface))

typedef gboolean (* GdaDataModelForeachFunc) (GdaDataModel *model,
					      GdaRow *row,
					      gpointer user_data);

/* struct for the interface */
struct _GdaDataModelIface {
	GTypeInterface           g_iface;

	/* virtual table */
	gint                 (* i_get_n_rows)      (GdaDataModel *model);
	gint                 (* i_get_n_columns)   (GdaDataModel *model);

	GdaColumn           *(* i_describe_column) (GdaDataModel *model, gint col);
	GdaRow              *(* i_get_row)         (GdaDataModel *model, gint row);
	const GdaValue      *(* i_get_value_at)    (GdaDataModel *model, gint col, gint row);

	gboolean             (* i_is_updatable)    (GdaDataModel *model);
	gboolean             (* i_has_changed)     (GdaDataModel *model);
	void                 (* i_begin_changes)   (GdaDataModel *model);
	gboolean             (* i_commit_changes)  (GdaDataModel *model);
	gboolean             (* i_cancel_changes)  (GdaDataModel *model);

	GdaRow              *(* i_append_values)   (GdaDataModel *model, const GList *values);
	gboolean             (* i_append_row)      (GdaDataModel *model, GdaRow *row);
	gboolean             (* i_update_row)      (GdaDataModel *model, GdaRow *row);
	gboolean             (* i_remove_row)      (GdaDataModel *model, GdaRow *row);

	GdaColumn           *(* i_append_column)   (GdaDataModel *model);
	gboolean             (* i_remove_column)   (GdaDataModel *model, gint col);

	void                 (* i_set_notify)      (GdaDataModel *model, gboolean do_notify_changes);
	gboolean             (* i_get_notify)      (GdaDataModel *model);

	gboolean             (* i_set_command)     (GdaDataModel *model, const gchar *txt, GdaCommandType type);
	const gchar         *(* i_get_command)     (GdaDataModel *model, GdaCommandType *type);
	

	/* signals */
	void                 (* changed)         (GdaDataModel *model);
	void                 (* row_inserted)    (GdaDataModel *model, gint row);
	void                 (* row_updated)     (GdaDataModel *model, gint row);
	void                 (* row_removed)     (GdaDataModel *model, gint row);
	void                 (* column_inserted) (GdaDataModel *model, gint col);
	void                 (* column_updated)  (GdaDataModel *model, gint col);
	void                 (* column_removed)  (GdaDataModel *model, gint col);

	void                 (* begin_update)    (GdaDataModel *model);
	void                 (* cancel_update)   (GdaDataModel *model);
	void                 (* commit_update)   (GdaDataModel *model);
};

GType                         gda_data_model_get_type               (void);

void                          gda_data_model_freeze                 (GdaDataModel *model);
void                          gda_data_model_thaw                   (GdaDataModel *model);

gint                          gda_data_model_get_n_rows             (GdaDataModel *model);
gint                          gda_data_model_get_n_columns          (GdaDataModel *model);
GdaColumn                    *gda_data_model_describe_column        (GdaDataModel *model, gint col);
const gchar                  *gda_data_model_get_column_title       (GdaDataModel *model, gint col);
void                          gda_data_model_set_column_title       (GdaDataModel *model, gint col, const gchar *title);
GdaRow                       *gda_data_model_get_row                (GdaDataModel *model, gint row);
const GdaValue               *gda_data_model_get_value_at           (GdaDataModel *model, gint col, gint row);

gboolean                      gda_data_model_is_updatable           (GdaDataModel *model);
GdaRow                       *gda_data_model_append_values          (GdaDataModel *model, const GList *values);
gboolean                      gda_data_model_append_row             (GdaDataModel *model, GdaRow *row);
gboolean                      gda_data_model_update_row             (GdaDataModel *model, GdaRow *row);
gboolean                      gda_data_model_remove_row             (GdaDataModel *model, GdaRow *row);
GdaColumn	             *gda_data_model_append_column          (GdaDataModel *model);
gboolean	              gda_data_model_remove_column          (GdaDataModel *model, gint col);

void                          gda_data_model_foreach                (GdaDataModel *model, GdaDataModelForeachFunc func,
								     gpointer user_data);

gboolean                      gda_data_model_has_changed            (GdaDataModel *model);
gboolean                      gda_data_model_begin_update           (GdaDataModel *model);
gboolean                      gda_data_model_cancel_update          (GdaDataModel *model);
gboolean                      gda_data_model_commit_update          (GdaDataModel *model);

gchar                        *gda_data_model_to_text_separated      (GdaDataModel *model, const gint *cols, gint nb_cols,
								     gchar sep);
gchar                        *gda_data_model_to_xml                 (GdaDataModel *model, const gint *cols, gint nb_cols,
								     const gchar *name);
xmlNodePtr                    gda_data_model_to_xml_node            (GdaDataModel *model, const gint *cols, gint nb_cols, 
								     const gchar *name);
gboolean                      gda_data_model_add_data_from_xml_node (GdaDataModel *model, xmlNodePtr node);

void                          gda_data_model_dump                   (GdaDataModel *model, FILE *to_stream);
gchar                        *gda_data_model_dump_as_string         (GdaDataModel *model);

G_END_DECLS

#endif
