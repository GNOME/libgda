/*
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 Holger Thon <holger.thon@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@src.gnome.org>
 * Copyright (C) 2003 Paisa Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2003 Philippe CHARLIER <p.charlier@chello.be>
 * Copyright (C) 2004 Dani Baeyens <daniel.baeyens@hispalinux.es>
 * Copyright (C) 2004 Szalai Ferenc <szferi@einstein.ki.iif.hu>
 * Copyright (C) 2004 - 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2005 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 Dan Winship <danw@src.gnome.org>
 * Copyright (C) 2005 Stanislav Brabec <sbrabec@suse.de>
 * Copyright (C) 2005 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2006 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
 * Copyright (C) 2008 Phil Longstaff <plongstaff@rogers.com>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 - 2013,2019 Daniel Espinosa <esodan@gmail.com>
 * Copyright (C) 2013 Miguel Angel Cabrera Moya <madmac2501@gmail.com>
 * Copyright (C) 2015 Corentin Noël <corentin@elementary.io>
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
#define G_LOG_DOMAIN "GDA-data-model"

#include <glib/gi18n-lib.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-data-model-import.h>
#include <libgda/gda-data-access-wrapper.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda/gda-row.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-server-provider-extra.h>
#include <string.h>
#ifdef HAVE_LOCALE_H
#ifndef G_OS_WIN32
#include <sys/ioctl.h>
#include <langinfo.h>
#endif
#include <locale.h>
#endif
#include "csv.h"

static void gda_data_model_default_init (GdaDataModelInterface *iface);

static xmlNodePtr gda_data_model_to_xml_node (GdaDataModel *model, const gint *cols, gint nb_cols, 
					      const gint *rows, gint nb_rows, const gchar *name);

static gchar *real_gda_data_model_dump_as_string (GdaDataModel *model, gboolean dump_attributes, 
						  gboolean dump_rows, gboolean dump_title, gboolean null_as_empty,
						  gint max_width, gboolean dump_separators, gboolean dump_sep_line,
						  gboolean use_data_handlers, gboolean dump_column_titles,
						  const gint *rows, gint nb_rows, GError **error);

G_DEFINE_INTERFACE (GdaDataModel, gda_data_model, G_TYPE_OBJECT)


/* signals */
enum {
	CHANGED,
	ROW_INSERTED,
	ROW_UPDATED,
	ROW_REMOVED,
	RESET,
	ACCESS_CHANGED,
	LAST_SIGNAL
};

static guint gda_data_model_signals[LAST_SIGNAL] = {0, 0, 0, 0, 0};

static void gda_data_model_default_init (GdaDataModelInterface *iface)
{
  /**
	 * GdaDataModel::changed:
	 * @model: the #GdaDataModel
	 *
	 * Gets emitted when any value in @model has been changed
	 */
	gda_data_model_signals[CHANGED] =
		g_signal_new ("changed",
			      GDA_TYPE_DATA_MODEL,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaDataModelInterface, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	/**
	 * GdaDataModel::row-inserted:
	 * @model: the #GdaDataModel
	 * @row: the row number
	 *
	 * Gets emitted when a row has been inserted in @model
	 */
	gda_data_model_signals[ROW_INSERTED] =
		g_signal_new ("row-inserted",
			      GDA_TYPE_DATA_MODEL,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaDataModelInterface, row_inserted),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	/**
	 * GdaDataModel::row-updated:
	 * @model: the #GdaDataModel
	 * @row: the row number
	 *
	 * Gets emitted when a row has been modified in @model
	 */
	gda_data_model_signals[ROW_UPDATED] =
		g_signal_new ("row-updated",
			      GDA_TYPE_DATA_MODEL,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaDataModelInterface, row_updated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	/**
	 * GdaDataModel::row-removed:
	 * @model: the #GdaDataModel
	 * @row: the row number
	 *
	 * Gets emitted when a row has been removed from @model
	 */
	gda_data_model_signals[ROW_REMOVED] =
		g_signal_new ("row-removed",
			      GDA_TYPE_DATA_MODEL,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaDataModelInterface, row_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	/**
	 * GdaDataModel::reset:
	 * @model: the #GdaDataModel
	 *
	 * Gets emitted when @model's contents has been completely reset (the number and
	 * type of columns may also have changed)
	 */
	gda_data_model_signals[RESET] =
		g_signal_new ("reset",
			      GDA_TYPE_DATA_MODEL,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaDataModelInterface, reset),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/**
	 * GdaDataModel::access-changed:
	 * @model: the #GdaDataModel
	 *
	 * Gets emitted when @model's access flags have changed. Use
	 * gda_data_model_get_access_flags() to get the access flags.
	 */
	gda_data_model_signals[ACCESS_CHANGED] =
		g_signal_new ("access-changed",
			      GDA_TYPE_DATA_MODEL,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaDataModelInterface, access_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/* module error */
GQuark gda_data_model_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_data_model_error");
        return quark;
}

static gboolean
do_notify_changes (GdaDataModel *model)
{
	gboolean notify_changes = TRUE;
	if (GDA_DATA_MODEL_GET_IFACE (model)->get_notify)
		notify_changes = (GDA_DATA_MODEL_GET_IFACE (model)->get_notify) (model);
	return notify_changes;
}

/*
 * _gda_data_model_signal_emit_changed
 * @model: a #GdaDataModel object.
 *
 * Notifies listeners of the given data model object of changes
 * in the underlying data. Listeners usually will connect
 * themselves to the "changed" signal in the #GdaDataModel
 * class, thus being notified of any new data being appended
 * or removed from the data model.
 */
static void
_gda_data_model_signal_emit_changed (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	
	if (do_notify_changes (model)) 
		g_signal_emit (model, gda_data_model_signals[CHANGED], 0);
}

/**
 * gda_data_model_row_inserted:
 * @model: a #GdaDataModel object.
 * @row: row number.
 *
 * Emits the 'row_inserted' and 'changed' signals on @model. 
 *
 * This method should only be used by #GdaDataModel implementations to 
 * signal that a row has been inserted.
 */
void
gda_data_model_row_inserted (GdaDataModel *model, gint row)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	/* update column's data types if they are not yet defined */
	if (gda_data_model_get_n_rows (model) == 1) {
		GdaColumn *column;
		gint i, nbcols;
		const GValue *value;

		nbcols = gda_data_model_get_n_columns (model);
		for (i = 0; i < nbcols; i++) {
			column = gda_data_model_describe_column (model, i);
			value = gda_data_model_get_value_at (model, i, 0, NULL);
			if (value && (gda_column_get_g_type (column) == GDA_TYPE_NULL))
				gda_column_set_g_type (column, G_VALUE_TYPE ((GValue *)value));
		}
	}

	/* notify changes */
	if (do_notify_changes (model)) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_INSERTED],
			       0, row);
		_gda_data_model_signal_emit_changed (model);
	}
}

/**
 * gda_data_model_row_updated:
 * @model: a #GdaDataModel object.
 * @row: row number.
 *
 * Emits the 'row_updated' and 'changed' signals on @model.
 *
 * This method should only be used by #GdaDataModel implementations to 
 * signal that a row has been updated.
 */
void
gda_data_model_row_updated (GdaDataModel *model, gint row)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (do_notify_changes (model)) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_UPDATED],
			       0, row);

		_gda_data_model_signal_emit_changed (model);
	}
}

/**
 * gda_data_model_row_removed:
 * @model: a #GdaDataModel object.
 * @row: row number.
 *
 * Emits the 'row_removed' and 'changed' signal on @model.
 *
 * This method should only be used by #GdaDataModel implementations to 
 * signal that a row has been removed
 */
void
gda_data_model_row_removed (GdaDataModel *model, gint row)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (do_notify_changes (model)) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_REMOVED],
			       0, row);

		_gda_data_model_signal_emit_changed (model);
	}
}

/**
 * gda_data_model_reset:
 * @model: a #GdaDataModel object.
 *
 * Emits the 'reset' and 'changed' signal on @model.
 */
void
gda_data_model_reset (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (do_notify_changes (model)) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[RESET], 0);
		
		_gda_data_model_signal_emit_changed (model);
	}
}

/**
 * gda_data_model_freeze:
 * @model: a #GdaDataModel object.
 *
 * Disables notifications of changes on the given data model. To
 * re-enable notifications again, you should call the
 * #gda_data_model_thaw function.
 *
 */
void
gda_data_model_freeze (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	
	if (GDA_DATA_MODEL_GET_IFACE (model)->freeze)
		(GDA_DATA_MODEL_GET_IFACE (model)->freeze) (model);
}

/**
 * gda_data_model_thaw:
 * @model: a #GdaDataModel object.
 *
 * Re-enables notifications of changes on the given data model.
 */
void
gda_data_model_thaw (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (GDA_DATA_MODEL_GET_IFACE (model)->thaw)
		(GDA_DATA_MODEL_GET_IFACE (model)->thaw) (model);
}

/**
 * gda_data_model_get_notify:
 * @model: a #GdaDataModel object.
 *
 * Returns the status of notifications changes on the given data model.
 */
gboolean
gda_data_model_get_notify (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	if (GDA_DATA_MODEL_GET_IFACE (model)->get_notify)
		return (GDA_DATA_MODEL_GET_IFACE (model)->get_notify) (model);
	else
		return TRUE;
}


/**
 * gda_data_model_get_access_flags:
 * @model: a #GdaDataModel object.
 *
 * Get the attributes of @model such as how to access the data it contains if it's modifiable, etc.
 *
 * Returns: (transfer none): an ORed value of #GdaDataModelAccessFlags flags
 */
GdaDataModelAccessFlags
gda_data_model_get_access_flags (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), 0);
	if (GDA_DATA_MODEL_GET_IFACE (model)->get_access_flags) {
		GdaDataModelAccessFlags flags = (GDA_DATA_MODEL_GET_IFACE (model)->get_access_flags) (model);
		if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
			flags |= GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD | GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD;
		return flags;
	}
	else
		return 0;
}

/**
 * gda_data_model_get_n_rows:
 * @model: a #GdaDataModel object.
 *
 * Returns: the number of rows in the given data model, or -1 if the number of rows is not known
 */
gint
gda_data_model_get_n_rows (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);

	if (GDA_DATA_MODEL_GET_IFACE (model)->get_n_rows)
		return (GDA_DATA_MODEL_GET_IFACE (model)->get_n_rows) (model);
	else 
		return -1;
}

/**
 * gda_data_model_get_n_columns:
 * @model: a #GdaDataModel object.
 *
 * Returns: the number of columns in the given data model, or -1 if unknown.
 */
gint
gda_data_model_get_n_columns (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	
	if (GDA_DATA_MODEL_GET_IFACE (model)->get_n_columns)
		return (GDA_DATA_MODEL_GET_IFACE (model)->get_n_columns) (model);
	else {
		/* method not supported */
		return -1;
	}
}

/**
 * gda_data_model_describe_column:
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Queries the underlying data model implementation for a description
 * of a given column. That description is returned in the form of
 * a #GdaColumn structure, which contains all the information
 * about the given column in the data model.
 *
 * WARNING: the returned #GdaColumn object belongs to the @model model and
 * and should not be destroyed; any modification will affect the whole data model.
 *
 * Returns: (transfer none) (nullable): the description of the column.
 */
GdaColumn *
gda_data_model_describe_column (GdaDataModel *model, gint col)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_IFACE (model)->describe_column)
		return (GDA_DATA_MODEL_GET_IFACE (model)->describe_column) (model, col);
	else {
		/* method not supported */
		return NULL;
	}
}

/**
 * gda_data_model_get_column_index:
 * @model: a #GdaDataModel object.
 * @name: a column name
 *
 * Get the index of the first column named @name in @model.
 *
 * Returns: the column index, or -1 if no column named @name was found
 */
gint
gda_data_model_get_column_index (GdaDataModel *model, const gchar *name)
{
	gint nbcols, ncol, ret;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	g_return_val_if_fail (name, -1);
	
	ret = -1;
	nbcols = gda_data_model_get_n_columns (model);
	for (ncol = 0; ncol < nbcols; ncol++) {
		if (g_str_equal (name, gda_data_model_get_column_title (model, ncol))) {
			
			ret = ncol;
			break;
		}
	}
	
	return ret;
}

/**
 * gda_data_model_get_column_name:
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Since: 3.2
 *
 * Returns: the name for the given column in a data model object.
 */
const gchar *
gda_data_model_get_column_name (GdaDataModel *model, gint col)
{
	GdaColumn *column;
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	column = gda_data_model_describe_column (model, col);
	if (column)
		return gda_column_get_name (column);
	else {
		g_warning ("%s(): can't get GdaColumn object for column %d\n", __FUNCTION__, col);
		return NULL;
	}
}

/**
 * gda_data_model_set_column_name:
 * @model: a #GdaDataModel object.
 * @col: column number
 * @name: name for the given column.
 *
 * Sets the @name of the given @col in @model, and if its title is not set, also sets the
 * title to @name.
 *
 * Since: 3.2
 */
void
gda_data_model_set_column_name (GdaDataModel *model, gint col, const gchar *name)
{
	GdaColumn *column;
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	column = gda_data_model_describe_column (model, col);
	if (column) {
		gda_column_set_name (column, name);
		if (!gda_column_get_description (column))
			gda_column_set_description (column, name);
	}
	else 
		g_warning ("%s(): can't get GdaColumn object for column %d\n", __FUNCTION__, col);
}


/**
 * gda_data_model_get_column_title:
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Returns: the title for the given column in a data model object.
 */
const gchar *
gda_data_model_get_column_title (GdaDataModel *model, gint col)
{
	GdaColumn *column;
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	column = gda_data_model_describe_column (model, col);
	if (column)
		return gda_column_get_description (column);
	else {
		g_warning ("%s(): can't get GdaColumn object for column %d\n", __FUNCTION__, col);
		return NULL;
	}
}

/**
 * gda_data_model_set_column_title:
 * @model: a #GdaDataModel object.
 * @col: column number
 * @title: title for the given column.
 *
 * Sets the @title of the given @col in @model.
 */
void
gda_data_model_set_column_title (GdaDataModel *model, gint col, const gchar *title)
{
	GdaColumn *column;
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	column = gda_data_model_describe_column (model, col);
	if (column)
		gda_column_set_description (column, title);
	else 
		g_warning ("%s(): can't get GdaColumn object for column %d\n", __FUNCTION__, col);
}

/**
 * gda_data_model_get_value_at:
 * @model: a #GdaDataModel object.
 * @col: a valid column number.
 * @row: a valid row number.
 * @error: a place to store errors, or %NULL.
 *
 * Retrieves the data stored in the given position (identified by
 * the @col and @row parameters) on a data model.
 *
 * Upon errors %NULL will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * This is the main function for accessing data in a model which allows random access to its data.
 * To access data in a data model using a cursor, use a #GdaDataModelIter object, obtained using
 * gda_data_model_create_iter().
 *
 * Note1: the returned #GValue must not be modified directly (unexpected behaviours may
 * occur if you do so).
 *
 * Note2: the returned value may become invalid as soon as any Libgda part is executed again,
 * which means if you want to keep the value, a copy must be made, however it will remain valid
 * as long as the only Libgda usage is calling gda_data_model_get_value_at() for different values
 * of the same row.
 *
 * If you want to modify a value stored in a #GdaDataModel, use the gda_data_model_set_value_at() or 
 * gda_data_model_set_values() methods.
 *
 * Upon errors %NULL will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: (nullable) (transfer none): a #GValue containing the value stored in the given
 * position, or %NULL on error (out-of-bound position, etc).
 */
const GValue *
gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_IFACE (model)->get_value_at)
		return (GDA_DATA_MODEL_GET_IFACE (model)->get_value_at) (model, col, row, error);
	else {
		/* method not supported */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
			      "%s", _("Data model does not support getting individual value"));
		return NULL;
	}
}

/**
 * gda_data_model_get_typed_value_at:
 * @model: a #GdaDataModel object.
 * @col: a valid column number.
 * @row: a valid row number.
 * @expected_type: the expected data type of the returned value
 * @nullok: if TRUE, then NULL values (value of type %GDA_TYPE_NULL) will not generate any error
 * @error: a place to store errors, or %NULL
 *
 * Upon errors %NULL will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * This method is similar to gda_data_model_get_value_at(), except that it also allows one to specify the expected
 * #GType of the value to get: if the data model returned a #GValue of a type different than the expected one, then
 * this method returns %NULL and an error code.
 *
 * Note: the same limitations and usage instructions apply as for gda_data_model_get_value_at().
 *
 * Upon errors %NULL will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: (nullable) (transfer none): a #GValue containing the value stored in the given
 * position, or %NULL on error (out-of-bound position, wrong data type, etc).
 */
const GValue *
gda_data_model_get_typed_value_at (GdaDataModel *model, gint col, gint row, GType expected_type, gboolean nullok, GError **error)
{
	const GValue *cvalue = NULL;
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_IFACE (model)->get_value_at)
		cvalue = (GDA_DATA_MODEL_GET_IFACE (model)->get_value_at) (model, col, row, error);

	if (cvalue) {
		if (nullok && 
		    (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL) && 
		    (G_VALUE_TYPE (cvalue) != expected_type)) {
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUE_TYPE_ERROR,
				     _("Data model returned value of invalid '%s' type"), 
				     gda_g_type_to_string (G_VALUE_TYPE (cvalue)));
			cvalue = NULL;
		}
		else if (!nullok && (G_VALUE_TYPE (cvalue) != expected_type)) {
			if (G_VALUE_TYPE (cvalue) == GDA_TYPE_NULL)
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUE_TYPE_ERROR,
					      "%s", _("Data model returned invalid NULL value"));
			else
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUE_TYPE_ERROR,
					     _("Data model returned value of invalid '%s' type"), 
					     gda_g_type_to_string (G_VALUE_TYPE (cvalue)));
			cvalue = NULL;
		}
	}
	return cvalue;
}

/**
 * gda_data_model_get_attributes_at:
 * @model: a #GdaDataModel object
 * @col: a valid column number
 * @row: a valid row number, or -1
 *
 * Get the attributes of the value stored at (row, col) in @model, which
 * is an ORed value of #GdaValueAttribute flags. As a special case, if
 * @row is -1, then the attributes returned correspond to a "would be" value
 * if a row was added to @model.
 *
 * Returns: (transfer none): the attributes as an ORed value of #GdaValueAttribute
 */
GdaValueAttribute
gda_data_model_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), 0);

	if (GDA_DATA_MODEL_GET_IFACE (model)->get_attributes_at)
		return (GDA_DATA_MODEL_GET_IFACE (model)->get_attributes_at) (model, col, row);
	else {
		GdaDataModelAccessFlags flags;
		GdaValueAttribute attrs = GDA_VALUE_ATTR_NO_MODIF;
		flags = gda_data_model_get_access_flags (model);
		if (flags & GDA_DATA_MODEL_ACCESS_WRITE)
			attrs = 0;
		return attrs;
	}
}

/**
 * gda_data_model_set_value_at:
 * @model: a #GdaDataModel object.
 * @col: column number.
 * @row: row number.
 * @value: a #GValue (not %NULL)
 * @error: a place to store errors, or %NULL
 *
 * Modifies a value in @model, at (@col, @row).
 *
 * Upon errors FALSE will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: TRUE if the value in the data model has been updated and no error occurred
 */
gboolean
gda_data_model_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *value, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (GDA_DATA_MODEL_GET_IFACE (model)->set_value_at)
		return (GDA_DATA_MODEL_GET_IFACE (model)->set_value_at) (model, col, row, value, error);
	else {
		/* method not supported */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
			      "%s", _("Data model does not support setting individual value"));
		return FALSE;
	}
}

/**
 * gda_data_model_set_values:
 * @model: a #GdaDataModel object.
 * @row: row number.
 * @values: (element-type GObject.Value) (transfer none) (nullable): a list of #GValue (or %NULL), one for at most the number of columns of @model
 * @error: a place to store errors, or %NULL
 *
 * In a similar way to gda_data_model_set_value_at(), this method modifies a data model's contents
 * by setting several values at once.
 *
 * If any value in @values is actually %NULL, then the value in the corresponding column is left
 * unchanged.
 
 * Upon errors FALSE will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: %TRUE if the value in the data model has been updated and no error occurred
 */
gboolean
gda_data_model_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	

	if (GDA_DATA_MODEL_GET_IFACE (model)->set_values)
		return (GDA_DATA_MODEL_GET_IFACE (model)->set_values) (model, row, values, error);
	else if (GDA_DATA_MODEL_GET_IFACE (model)->set_value_at) {
		/* save the values */
		gint col, ncols;
		ncols = gda_data_model_get_n_columns (model);
		if ((gint)g_list_length (values) > ncols) {
			g_set_error (error, GDA_DATA_MODEL_ERROR,  GDA_DATA_MODEL_VALUES_LIST_ERROR,
				     "%s", _("Too many values in list"));
			return FALSE;
		}

		for (col = 0;
		     (col < ncols) && values;
		     col++, values = values->next) {
			const GValue *cvalue;
			cvalue = (const GValue*) values->data;
			if (!cvalue)
				continue;
			if (! gda_data_model_set_value_at (model, col, row, cvalue, error))
				return FALSE;
		}
		return TRUE;
	}
	else {
		/* method not supported */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
			      "%s", _("Data model does not support setting values"));
		return FALSE;
	}
}

/**
 * gda_data_model_create_iter:
 * @model: a #GdaDataModel object.
 *
 * Creates a new iterator object #GdaDataModelIter object which can be used to iterate through
 * rows in @model. The new #GdaDataModelIter does not hold any reference to @model (ie. if @model
 * is destroyed at some point, the new iterator will become useless but in any case it will not prevent
 * the data model from being destroyed).
 *
 * Depending on the data model's implementation, a new #GdaDataModelIter object may be created,
 * or a reference to an already existing #GdaDataModelIter may be returned. For example if @model only
 * supports being accessed using a forward moving cursor (say a the result of a SELECT executed by SQLite
 * with a cursor access mode specified), then this method will always return the same iterator.
 *
 * If a new #GdaDataModelIter is created, then the row it represents is undefined.
 *
 * For models which can be accessed 
 * randomly, any row can be set using gda_data_model_iter_move_to_row(), 
 * and for models which are accessible sequentially only then use
 * gda_data_model_iter_move_next() (and gda_data_model_iter_move_prev() if
 * supported).
 *
 * Note: for the #GdaDataProxy data model (which proxies any #GdaDataModel for modifications and
 * has twice the number of columns of the proxied data model), this method will create an iterator
 * in which only the columns of the proxied data model appear. If you need to have a #GdaDataModelIter
 * in which all the proxy's columns appear, create it using:
 * <programlisting><![CDATA[iter = g_object_new (GDA_TYPE_DATA_MODEL_ITER, "data-model", proxy, NULL);]]></programlisting>
 *
 * Returns: (transfer full): a #GdaDataModelIter object, or %NULL if an error occurred
 */
GdaDataModelIter *
gda_data_model_create_iter (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	GdaDataModelIter *iter = NULL;

	if (GDA_DATA_MODEL_GET_IFACE (model)->create_iter)
		iter = (GDA_DATA_MODEL_GET_IFACE (model)->create_iter) (model);
	else
		/* default method */
		iter = GDA_DATA_MODEL_ITER (g_object_new (GDA_TYPE_DATA_MODEL_ITER,
				      "data-model", model, NULL));
	return iter;
}

/**
 * gda_data_model_append_values:
 * @model: a #GdaDataModel object.
 * @values: (element-type GObject.Value) (nullable): #GList of #GValue* representing the row to add.  The
 *          length must match model's column count.  These #GValue
 *	    are value-copied (the user is still responsible for freeing them).
 * @error: a place to store errors, or %NULL
 *
 * Appends a row to the given data model. If any value in @values is actually %NULL, then 
 * it is considered as a default value. If @values is %NULL then all values are set to their default value.
 *
 * Upon errors -1 will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: the number of the added row, or -1 if an error occurred
 */
gint
gda_data_model_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);

	if (GDA_DATA_MODEL_GET_IFACE (model)->append_values)
		return (GDA_DATA_MODEL_GET_IFACE (model)->append_values) (model, values, error);
	else {
		/* method not supported */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
			      "%s", _("Data model does not support row append"));
		return -1;
	}
}

/**
 * gda_data_model_append_row:
 * @model: a #GdaDataModel object.
 * @error: a place to store errors, or %NULL
 * 
 * Appends a row to the data model (the new row will possibly have NULL values for all columns,
 * or some other values depending on the data model implementation)
 *
 * Upon errors -1 will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: the number of the added row, or -1 if an error occurred
 */
gint
gda_data_model_append_row (GdaDataModel *model, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_INSERT)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			      "%s", _("Model does not allow row insertion"));
		return -1;
	}

	if (GDA_DATA_MODEL_GET_IFACE (model)->append_row)
		return (GDA_DATA_MODEL_GET_IFACE (model)->append_row) (model, error);
	else {
		/* method not supported */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
			      "%s", _("Data model does not support row append"));
		return -1;
	}
}


/**
 * gda_data_model_remove_row:
 * @model: a #GdaDataModel object.
 * @row: the row number to be removed.
 * @error: a place to store errors, or %NULL
 *
 * Removes a row from the data model.
 *
 * Upon errors FALSE will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_remove_row (GdaDataModel *model, gint row, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_DELETE)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			      "%s", _("Model does not allow row deletion"));
		return FALSE;
	}

	if (GDA_DATA_MODEL_GET_IFACE (model)->remove_row)
		return (GDA_DATA_MODEL_GET_IFACE (model)->remove_row) (model, row, error);
	else {
		/* method not supported */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
			      "%s", _("Data model does not support row removal"));
		return FALSE;
	}
}

/**
 * gda_data_model_get_row_from_values:
 * @model: a #GdaDataModel object.
 * @values: (element-type GObject.Value): a list of #GValue values (no %NULL is allowed)
 * @cols_index: (array): an array of #gint containing the column number to match each value of @values
 *
 * Returns the first row where all the values in @values at the columns identified at
 * @cols_index match. If the row can't be identified, then returns -1;
 *
 * NOTE: the @cols_index array MUST contain a column index for each value in @values
 *
 * Returns: the requested row number, of -1 if not found
 */
gint
gda_data_model_get_row_from_values (GdaDataModel *model, GSList *values, gint *cols_index)
{
	gint row = -1;
        gint current_row, n_rows, n_cols;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	g_return_val_if_fail (values, -1);

	if (GDA_DATA_MODEL_GET_IFACE (model)->find_row)
		return (GDA_DATA_MODEL_GET_IFACE (model)->find_row) (model, values, cols_index);

        n_rows = gda_data_model_get_n_rows (model);
        n_cols = gda_data_model_get_n_columns (model);
        current_row = 0;

#ifdef GDA_DEBUG_NO
        {
                GSList *list = values;

                g_print ("%s() find row for values: ", __FUNCTION__);
                while (list) {
                        g_print ("#%s# ", gda_value_stringify ((GValue *)(list->data)));
                        list = g_slist_next (list);
                }
                g_print ("In %d rows\n", n_rows);
        }
#endif

	while ((current_row < n_rows) && (row == -1)) {
                GSList *list;
                gboolean allequal = TRUE;
                const GValue *value;
                gint index;

                for (list = values, index = 0;
		     list;
		     list = list->next, index++) {
                        if (cols_index)
                                g_return_val_if_fail (cols_index [index] < n_cols, FALSE);
                        value = gda_data_model_get_value_at (model, cols_index [index], current_row, NULL);

                        if (!value || !(list->data) ||
			    (G_VALUE_TYPE (value) != G_VALUE_TYPE ((GValue *) list->data)) ||
			    gda_value_compare ((GValue *) (list->data), (GValue *) value)) {
                                allequal = FALSE;
				break;
			}
                }

                if (allequal)
                        row = current_row;

                current_row++;
        }

        return row;
}

/**
 * gda_data_model_send_hint:
 * @model: a #GdaDataModel
 * @hint: (transfer none): a hint to send to the model
 * @hint_value: (nullable): an optional value to specify the hint, or %NULL
 *
 * Sends a hint to the data model. The hint may or may not be handled by the data
 * model, depending on its implementation
 */
void
gda_data_model_send_hint (GdaDataModel *model, GdaDataModelHint hint, const GValue *hint_value)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (GDA_DATA_MODEL_GET_IFACE (model)->send_hint)
		(GDA_DATA_MODEL_GET_IFACE (model)->send_hint) (model, hint, hint_value);
}

/**
 * gda_data_model_get_exceptions:
 * @model: a #GdaDataModel
 *
 * Get the global data model exception(s) that occurred when using @model.
 * This is useful for example for the LDAP related
 * data models where some rows may be missing because the LDAP search has reached a limit
 * imposed by the LDAP server.
 *
 * Returns: (transfer none) (element-type GLib.Error) (array zero-terminated=1): a pointer to a %NULL terminated array of #GError, or %NULL.
 *
 * Since: 4.2.6
 */
GError **
gda_data_model_get_exceptions (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_IFACE (model)->get_exceptions)
		return (GDA_DATA_MODEL_GET_IFACE (model)->get_exceptions) (model);
	else
		return NULL;
}


static gchar *export_to_text_separated (GdaDataModel *model, const gint *cols, gint nb_cols, 
					const gint *rows, gint nb_rows, gchar sep, gchar quote, gboolean field_quotes,
					gboolean null_as_empty, gboolean invalid_as_null);


/**
 * gda_data_model_export_to_string:
 * @model: a #GdaDataModel
 * @format: the format in which to export data
 * @cols: (array length=nb_cols) (nullable): an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @rows: (array length=nb_rows) (nullable): an array containing which rows of @model will be exported, or %NULL for all rows
 * @nb_rows: the number of rows in @rows
 * @options: list of options for the export
 *
 * Exports data contained in @model to a string; the format is specified using the @format argument, see the
 * gda_data_model_export_to_file() documentation for more information about the @options argument (except for the
 * "OVERWRITE" option).
 *
 * Warning: this function uses a #GdaDataModelIter iterator, and if @model does not offer a random access
 * (check using gda_data_model_get_access_flags()), the iterator will be the same as normally used
 * to access data in @model previously to calling this method, and this iterator will be moved (point to
 * another row).
 *
 * See also gda_data_model_dump_as_string();
 *
 * Returns: (transfer full): a new string, use g_free() when no longer needed
 */
gchar *
gda_data_model_export_to_string (GdaDataModel *model, GdaDataModelIOFormat format, 
				 const gint *cols, gint nb_cols, 
				 const gint *rows, gint nb_rows, GdaSet *options)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (!options || GDA_IS_SET (options), NULL);

	switch (format) {
	case GDA_DATA_MODEL_IO_DATA_ARRAY_XML: {
		const gchar *name = NULL;
		xmlChar *xml_contents;
		xmlNodePtr xml_node;
		gchar *xml;
		gint size;
		xmlDocPtr xml_doc;

		GdaHolder *holder;
		holder = options ? gda_set_get_holder (options, "NAME") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_STRING))
				name = g_value_get_string ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a string value, ignored."), "NAME");
		}
		
		xml_node = gda_data_model_to_xml_node (model, cols, nb_cols, rows, nb_rows, name);
		xml_doc = xmlNewDoc ((xmlChar*)"1.0");
		xmlDocSetRootElement (xml_doc, xml_node);
		
		xmlDocDumpFormatMemory (xml_doc, &xml_contents, &size, 1);
		xmlFreeDoc (xml_doc);
		
		xml = g_strdup ((gchar*)xml_contents);
		xmlFree (xml_contents);
		
		return xml;
	}

	case GDA_DATA_MODEL_IO_TEXT_SEPARATED: {
		GString *retstring;
		gchar sep = ',';
		gchar quote = '"';
		gboolean field_quote = TRUE;
		gboolean null_as_empty = FALSE;
		gboolean invalid_as_null = FALSE;

		retstring = g_string_new ("");
		GdaHolder *holder;

		holder = options ? gda_set_get_holder (options, "SEPARATOR") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_STRING)) {
				const gchar *str;

				str = g_value_get_string ((GValue *) value);
				if (str && *str)
					sep = *str;
			}
			else
				g_warning (_("The '%s' parameter must hold a string value, ignored."), "SEPARATOR");
		}
		holder = options ? gda_set_get_holder (options, "QUOTE") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_STRING)) {
				const gchar *str;

				str = g_value_get_string ((GValue *) value);
				if (str && *str)
					quote = *str;
			}
			else
				g_warning (_("The '%s' parameter must hold a string value, ignored."), "QUOTE");
		}
		holder = options ? gda_set_get_holder (options, "FIELD_QUOTE") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
				field_quote = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "FIELD_QUOTE");
		}

		holder = options ? gda_set_get_holder (options, "NULL_AS_EMPTY") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
				null_as_empty = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "NULL_AS_EMPTY");
		}

		holder = options ? gda_set_get_holder (options, "INVALID_AS_NULL") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
				invalid_as_null = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "INVALID_AS_NULL");
		}

		holder = options ? gda_set_get_holder (options, "NAMES_ON_FIRST_LINE") : NULL;
		if (!holder && options)
			holder = gda_set_get_holder (options, "FIELDS_NAME");
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)) {
				if (g_value_get_boolean (value)) {
					gint col;
					gint *rcols;
					gint rnb_cols;
						
					if (cols) {
						rcols = (gint *)cols;
						rnb_cols = nb_cols;
					}
					else {
						gint i;
							
						rnb_cols = gda_data_model_get_n_columns (model);
						rcols = g_new (gint, rnb_cols);
						for (i = 0; i < rnb_cols; i++)
							rcols[i] = i;
					}
						
					for (col = 0; col < rnb_cols; col++) {
						if (col)
							g_string_append_c (retstring, sep);
						g_string_append_c (retstring, quote);
						g_string_append (retstring,
								 gda_data_model_get_column_name (model, rcols[col]));
						g_string_append_c (retstring, quote);
					}
					g_string_append_c (retstring, '\n');
					if (!cols)
						g_free (rcols);
				}
			}
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."),
					   "FIELDS_NAME");
		}
		
		if (cols) {
			gchar *tmp;
			tmp = export_to_text_separated (model, cols, nb_cols, rows,
							nb_rows, sep, quote, field_quote, null_as_empty, invalid_as_null);
			g_string_append (retstring, tmp);
			g_free (tmp);
		}
		else {
			gint *rcols, rnb_cols, i;
			gchar *tmp;
			rnb_cols = gda_data_model_get_n_columns (model);
			rcols = g_new (gint, rnb_cols);
			for (i = 0; i < rnb_cols; i++)
				rcols[i] = i;
			tmp = export_to_text_separated (model, rcols, rnb_cols, rows, nb_rows,
							sep, quote, field_quote, null_as_empty, invalid_as_null);
			g_string_append (retstring, tmp);
			g_free (tmp);
			g_free (rcols);			
		}
		return g_string_free (retstring, FALSE);
	}

	case GDA_DATA_MODEL_IO_TEXT_TABLE: {
		gboolean dump_rows = FALSE;
		gboolean dump_title = FALSE;
		gboolean dump_column_titles = TRUE;
		gboolean null_as_empty = TRUE;
		gboolean dump_separators = TRUE;
		gboolean dump_title_line = TRUE;
		gint max_width = -1;

		/* analyse options */
		GdaHolder *holder;
		holder = options ? gda_set_get_holder (options, "ROW_NUMBERS") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
				dump_rows = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "ROW_NUMBERS");
		}

		holder = options ? gda_set_get_holder (options, "NAME") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
				dump_title = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "NAME");
		}

		holder = options ? gda_set_get_holder (options, "NAMES_ON_FIRST_LINE") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
				dump_column_titles = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "NAMES_ON_FIRST_LINE");
		}

		holder = options ? gda_set_get_holder (options, "NULL_AS_EMPTY") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
				null_as_empty = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "NULL_AS_EMPTY");
		}
		
		holder = options ? gda_set_get_holder (options, "MAX_WIDTH") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_INT)) {
				max_width = g_value_get_int ((GValue *) value);
#ifdef TIOCGWINSZ
				if (max_width < 0) {
					struct winsize window_size;
					if (ioctl (0,TIOCGWINSZ, &window_size) == 0)
						max_width = (int) window_size.ws_col;
				}
#endif
			}
			else
				g_warning (_("The '%s' parameter must hold an integer value, ignored."), "MAX_WIDHT");
		}

		holder = options ? gda_set_get_holder (options, "COLUMN_SEPARATORS") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
				dump_separators = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "SEPARATORS");
		}

		holder = options ? gda_set_get_holder (options, "SEPARATOR_LINE") : NULL;
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
				dump_title_line = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "SEPARATOR_LINE");
		}

		/* FIXME: handle @rows argument */
		if (cols) {
			GdaDataModel *wrapper;
			wrapper = gda_data_access_wrapper_new (model);
			gda_data_access_wrapper_set_mapping (GDA_DATA_ACCESS_WRAPPER (wrapper), cols, nb_cols);
			gchar *tmp;
			tmp = real_gda_data_model_dump_as_string (wrapper, FALSE, dump_rows,
								  dump_title, null_as_empty, max_width, dump_separators,
								  dump_title_line, TRUE, dump_column_titles, rows, nb_rows, NULL);
			g_object_unref (wrapper);
			return tmp;
		}
		else
			return real_gda_data_model_dump_as_string (model, FALSE, dump_rows,
								   dump_title, null_as_empty, max_width, dump_separators,
								   dump_title_line, TRUE, dump_column_titles, rows, nb_rows, NULL);
	}

	default:
		g_warning (_("Unknown GdaDataModelIOFormat %d value"), format);
	}
	return NULL;
}

/**
 * gda_data_model_export_to_file:
 * @model: a #GdaDataModel
 * @format: the format in which to export data
 * @file: the filename to export to
 * @cols: (array length=nb_cols) (nullable): an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @rows: (array length=nb_rows) (nullable): an array containing which rows of @model will be exported, or %NULL for all rows
 * @nb_rows: the number of rows in @rows
 * @options: list of options for the export
 * @error: a place to store errors, or %NULL
 *
 * Exports data contained in @model to the @file file; the format is specified using the @format argument. Note that
 * the date format used is the one used by the connection from which the data model has been made (as the result of a
 * SELECT statement), or, for other kinds of data models, the default format (refer to gda_data_handler_get_default()) unless
 * the "cnc" property has been set and points to a #GdaConnection to use that connection's date format.
 *
 * Specifically, the parameters in the @options list can be:
 * <itemizedlist>
 *   <listitem><para>"SEPARATOR": a string value of which the first character is used as a separator in case of CSV export
 *             </para></listitem>
 *   <listitem><para>"QUOTE": a string value of which the first character is used as a quote character in case of CSV export. The
 *             default if not specified is the double quote character</para></listitem>
 *   <listitem><para>"FIELD_QUOTE": a boolean value which can be set to FALSE if no quote around the individual fields 
 *             is requeted, in case of CSV export</para></listitem>
 *   <listitem><para>"NAMES_ON_FIRST_LINE": a boolean value which, if set to %TRUE and in case of a CSV or %GDA_DATA_MODEL_IO_TEXT_TABLE export, will add a first line with the name each exported field (note that "FIELDS_NAME" is also accepted as a synonym)</para></listitem>
 *   <listitem><para>"NAME": a string value used to name the exported data if the export format is XML or %GDA_DATA_MODEL_IO_TEXT_TABLE</para></listitem>
 *   <listitem><para>"OVERWRITE": a boolean value which tells if the file must be over-written if it already exists.</para></listitem>
 *   <listitem><para>"NULL_AS_EMPTY": a boolean value which, if set to %TRUE and in case of a CSV or %GDA_DATA_MODEL_IO_TEXT_TABLE export, will render and NULL value as the empty string (instead of the 'NULL' string)</para></listitem>
 *   <listitem><para>"INVALID_AS_NULL": a boolean value which, if set to %TRUE, considers any invalid data (for example for the date related values) as NULL</para></listitem>
 *   <listitem><para>"COLUMN_SEPARATORS": a boolean value which, if set to %TRUE, adds a separators lines between each column, if the export format is %GDA_DATA_MODEL_IO_TEXT_TABLE
 *             </para></listitem>
*   <listitem><para>"SEPARATOR_LINE": a boolean value which, if set to %TRUE, adds an horizontal line between column titles and values, if the export format is %GDA_DATA_MODEL_IO_TEXT_TABLE
 *             </para></listitem>
 *   <listitem><para>"ROW_NUMBERS": a boolean value which, if set to %TRUE, prepends a column with row numbers, if the export format is %GDA_DATA_MODEL_IO_TEXT_TABLE
 *             </para></listitem>
 *   <listitem><para>"MAX_WIDTH": an integer value which, if greater than 0, makes all the lines truncated to have at most that number of characters, if the export format is %GDA_DATA_MODEL_IO_TEXT_TABLE
 *             </para></listitem>
 * </itemizedlist>
 *
 * Warning: this function uses a #GdaDataModelIter iterator, and if @model does not offer a random access
 * (check using gda_data_model_get_access_flags()), the iterator will be the same as normally used
 * to access data in @model previously to calling this method, and this iterator will be moved (point to
 * another row).
 *
 * Upon errors %FALSE will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_export_to_file (GdaDataModel *model, GdaDataModelIOFormat format, 
			       const gchar *file,
			       const gint *cols, gint nb_cols,
			       const gint *rows, gint nb_rows,
			       GdaSet *options, GError **error)
{
	gchar *body;
	gboolean overwrite = FALSE;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (!options || GDA_IS_SET (options), FALSE);
	g_return_val_if_fail (file, FALSE);

	body = gda_data_model_export_to_string (model, format, cols, nb_cols, rows, nb_rows, options);
	GdaHolder *holder;
		
	holder = options ? gda_set_get_holder (options, "OVERWRITE") : NULL;
	if (holder) {
		const GValue *value;
		value = gda_holder_get_value (holder);
		if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
			overwrite = g_value_get_boolean ((GValue *) value);
		else
			g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "OVERWRITE");
	}

	if (g_file_test (file, G_FILE_TEST_EXISTS)) {
		if (! overwrite) {
			g_free (body);
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_FILE_EXIST_ERROR,
				     _("File '%s' already exists"), file);
			return FALSE;
		}
	}
	
	if (! g_file_set_contents (file, body, -1, error)) {
		g_free (body);
		return FALSE;
	}
	g_free (body);
	return TRUE;
}

static gchar *
export_to_text_separated (GdaDataModel *model, const gint *cols, gint nb_cols,
			  const gint *rows, gint nb_rows, 
			  gchar sep, gchar quote, gboolean field_quotes,
			  gboolean null_as_empty, gboolean invalid_as_null)
{
	GString *str;
	gint c;
	GdaDataModelIter *iter;
	gboolean addnl = FALSE;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	str = g_string_new ("");
	iter = gda_data_model_create_iter (model);
	if (!iter)
		return g_string_free (str, FALSE);

	if ((gda_data_model_iter_get_row (iter) == -1) && ! gda_data_model_iter_move_next (iter)) {
		g_object_unref (iter);
		return g_string_free (str, FALSE);
	}

	for (; gda_data_model_iter_is_valid (iter); gda_data_model_iter_move_next (iter)) {
		if (rows) {
			gint r;
			for (r = 0; r < nb_rows; r++) { 
				if (gda_data_model_iter_get_row (iter) == rows[r])
					break;
			}
			if (r == nb_rows)
				continue;
		}
		
		if (addnl)
			str = g_string_append_c (str, '\n');
		else
			addnl = TRUE;

		for (c = 0; c < nb_cols; c++) {
			GValue *value;
			gchar *txt;

			value = (GValue*) gda_data_model_iter_get_value_at (iter, cols[c]);
			if (value && invalid_as_null) {
				if (g_type_is_a (G_VALUE_TYPE (value), G_TYPE_DATE)) {
					GDate *date = (GDate*) g_value_get_boxed (value);
					if (!g_date_valid (date))
						value = NULL;
				}
				else if (g_type_is_a (G_VALUE_TYPE (value), GDA_TYPE_TIME)) {
					const GdaTime *tim = gda_value_get_time (value);
					if (!tim)
						value = NULL;					
				}
				else if (g_type_is_a (G_VALUE_TYPE (value), G_TYPE_DATE_TIME)) {
					GDateTime *ts = g_value_get_boxed (value);
					if (ts == NULL)
						value = NULL;
				}
			}

			if (!value) {
				if (null_as_empty)
					txt = g_strdup ("");
				else
					txt = g_strdup ("NULL");
			}
			else if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
				txt = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
			else if (null_as_empty && gda_value_is_null (value))
				txt = g_strdup ("");
			else {
				gboolean alloc = FALSE;
				gchar *tmp;

				if (value && (G_VALUE_TYPE (value) == G_TYPE_STRING))
					tmp = (gchar*) g_value_get_string (value);
				else {
					tmp = gda_value_stringify (value);
					alloc = TRUE;
				}
				if (tmp) {
					gsize len, size;
					len = strlen (tmp);
					size = 2 * len + 3;
					txt = g_new (gchar, size);

					len = csv_write2 (txt, size, tmp, len, quote);
					txt [len] = 0;
					if (!field_quotes) {
						txt [len - 1] = 0;
						memmove (txt, txt+1, len);
					}
					if (alloc)
						g_free (tmp);
				}
				else {
					if (field_quotes) {
						txt = g_new (gchar, 3);
						txt [0] = quote;
						txt [1] = quote;
						txt [2] = 0;
					}
					else
						txt = g_strdup ("");
				}
			}
			if (c > 0)
				str = g_string_append_c (str, sep);

			str = g_string_append (str, txt);
			g_free (txt);
		}
	}

	g_object_unref (iter);
	return g_string_free (str, FALSE);
}

static void
xml_set_boolean (xmlNodePtr node, const gchar *name, gboolean value)
{
	xmlSetProp (node, (xmlChar*)name, value ? (xmlChar*)"TRUE" : (xmlChar*)"FALSE");
}

/*
 * gda_data_model_to_xml_node
 * @model: a #GdaDataModel object.
 * @cols: (nullable) (array length=nb_cols): an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @rows: (nullable) (array length=nb_rows): an array containing which rows of @model will be exported, or %NULL for all rows
 * @nb_rows: the number of rows in @rows
 * @name: (nullable): name to use for the XML resulting table or %NULL.
 *
 * Converts a #GdaDataModel into a xmlNodePtr (as used in libxml).
 *
 * Returns: a xmlNodePtr representing the whole data model, or %NULL if an error occurred
 */
static xmlNodePtr
gda_data_model_to_xml_node (GdaDataModel *model, const gint *cols, gint nb_cols, 
			    const gint *rows, gint nb_rows, const gchar *name)
{
	xmlNodePtr node;
	gint i;
	gint *rcols, rnb_cols;
	const gchar *cstr;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	node = xmlNewNode (NULL, BAD_CAST "gda_array");
	cstr = g_object_get_data (G_OBJECT (model), "id");
	if (cstr)
		xmlSetProp (node, BAD_CAST "id", BAD_CAST cstr);
	else
		xmlSetProp (node, BAD_CAST "id", BAD_CAST "EXPORT");

	if (name)
		xmlSetProp (node, BAD_CAST "name", BAD_CAST name);
	else {
		cstr = g_object_get_data (G_OBJECT (model), "name");
		if (cstr)
			xmlSetProp (node, BAD_CAST "name", BAD_CAST cstr);
		else
			xmlSetProp (node, BAD_CAST "name", BAD_CAST _("Exported Data"));
	}

	/* compute columns if not provided */
	if (!cols) {
		rnb_cols = gda_data_model_get_n_columns (model);
		rcols = g_new (gint, rnb_cols);
		for (i = 0; i < rnb_cols; i++)
			rcols [i] = i;
	}
	else {
		rcols = (gint *) cols;
		rnb_cols = nb_cols;
	}

	/* set the table structure */
	for (i = 0; i < rnb_cols; i++) {
		GdaColumn *column;
		xmlNodePtr field;
		const gchar *cstr;
		gchar *str;

		column = gda_data_model_describe_column (model, rcols [i]);
		if (!column) {
			xmlFreeNode (node);
			return NULL;
		}

		field = xmlNewChild (node, NULL, BAD_CAST "gda_array_field", NULL);
		g_object_get (G_OBJECT (column), "id", &str, NULL);
		if (!str)
			str = g_strdup_printf ("FI%d", i);
		xmlSetProp (field, BAD_CAST "id", BAD_CAST str);
		g_free (str);
		xmlSetProp (field, BAD_CAST "name", BAD_CAST gda_column_get_name (column));
		cstr = gda_column_get_description (column);
		if (cstr && *cstr)
			xmlSetProp (field, BAD_CAST "title", BAD_CAST cstr);
		cstr = gda_column_get_dbms_type (column);
		if (cstr && *cstr)
			xmlSetProp (field, BAD_CAST "dbms_type", BAD_CAST cstr);
		xmlSetProp (field, BAD_CAST "gdatype", BAD_CAST gda_g_type_to_string (gda_column_get_g_type (column)));
		if (gda_column_get_allow_null (column))
			xml_set_boolean (field, "nullok", gda_column_get_allow_null (column));
		if (gda_column_get_auto_increment (column))
			xml_set_boolean (field, "auto_increment", gda_column_get_auto_increment (column));
	}
	
	/* add the model data to the XML output */
	if (!gda_utility_data_model_dump_data_to_xml (model, node, cols, nb_cols, rows, nb_rows, FALSE)) {
		xmlFreeNode (node);
		node = NULL;
	}

	if (!cols)
		g_free (rcols);

	return node;
}

static GdaColumn *
find_column_from_id (GdaDataModel *model, const gchar *colid, gint *pos)
{
	GdaColumn *column = NULL;
	gint c, nbcols;
	
	/* assume @colid is the ID of a column */
	nbcols = gda_data_model_get_n_columns (model);
	for (c = 0; !column && (c < nbcols); c++) {
		gchar *id;
		column = gda_data_model_describe_column (model, c);
		g_object_get (column, "id", &id, NULL);
		if (!id || strcmp (id, colid)) 
			column = NULL;
		else
			*pos = c;
		g_free (id);
	}

	/* if no column has been found, assumr @colid is like "_%d" where %d is a column number */
	if (!column && (*colid == '_')) {
		gint i;
		i = atoi (colid + 1); /* Flawfinder: ignore */
		if (i >= 0) {
			column = gda_data_model_describe_column (model, i);
			if (column)
				*pos = i;
		}
	}

	return column;
}

static gboolean
add_xml_row (GdaDataModel *model, xmlNodePtr xml_row, GError **error)
{
	xmlNodePtr xml_field;
	GList *value_list = NULL;
	GPtrArray *values;
	gsize i;
	gboolean retval = TRUE;
	gint pos = 0;

	const gchar *lang = setlocale(LC_ALL, NULL);

	values = g_ptr_array_new ();
	g_ptr_array_set_size (values, gda_data_model_get_n_columns (model));
	for (xml_field = xml_row->xmlChildrenNode; xml_field != NULL; xml_field = xml_field->next) {
		GValue *value = NULL;
		GdaColumn *column = NULL;
		GType gdatype;
		gchar *isnull = NULL;
		xmlChar *this_lang = NULL;
		xmlChar *colid = NULL;

		if (xmlNodeIsText (xml_field))
			continue;

		if (strcmp ((gchar*)xml_field->name, "gda_value") && 
		    strcmp ((gchar*)xml_field->name, "gda_array_value")) {
			continue;
		}
		
		this_lang = xmlGetProp (xml_field, BAD_CAST "lang");
		if (this_lang && strncmp ((gchar*)this_lang, lang, strlen ((gchar*)this_lang))) {
			xmlFree (this_lang);
			continue;
		}

		colid = xmlGetProp (xml_field, BAD_CAST "colid");

		/* create the value for this field */
		if (!colid) {
			if (this_lang)
				column = gda_data_model_describe_column (model, pos - 1);
			else
				column = gda_data_model_describe_column (model, pos);
		}
		else {
			column = find_column_from_id (model, (gchar*)colid, &pos);
			xmlFree (colid);
			if (!column)
				continue;
		}

		gdatype = gda_column_get_g_type (column);
		if ((gdatype == G_TYPE_INVALID) || (gdatype == GDA_TYPE_NULL)) {
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_XML_FORMAT_ERROR,
				      "%s", _("Cannot retrieve column data type (type is UNKNOWN or not specified)"));
			retval = FALSE;
			break;
		}

		gboolean nullforced = FALSE;
		isnull = (gchar*)xmlGetProp (xml_field, BAD_CAST "isnull");
		if (isnull) {
			if ((*isnull == 't') || (*isnull == 'T'))
				nullforced = TRUE;
			g_free (isnull);
		}

		if (!nullforced) {
			value = g_new0 (GValue, 1);
			gchar* nodeval = (gchar*)xmlNodeGetContent (xml_field);
			if (nodeval) {
				if (!gda_value_set_from_string (value, nodeval, gdatype))
					gda_value_set_null (value);
				xmlFree(nodeval);
			}
			else
				gda_value_set_null (value);
		}

		g_ptr_array_index (values, pos) = value;
		if (this_lang)
			xmlFree (this_lang);
		else
			pos ++;
	}

	if (retval) {
		for (i = 0; i < values->len; i++) {
			GValue *value = (GValue *) g_ptr_array_index (values, i);

			if (!value) {
				value = gda_value_new_null ();
				g_ptr_array_index (values, i) = value;
			}

			value_list = g_list_append (value_list, value);
		}

		if (retval)
			gda_data_model_append_values (model, value_list, NULL);

		g_list_free (value_list);
	}

	for (i = 0; i < values->len; i++)
		if (g_ptr_array_index (values, i))
			gda_value_free ((GValue *) g_ptr_array_index (values, i));

	g_ptr_array_free (values, TRUE);

	return retval;
}

/**
 * gda_data_model_add_data_from_xml_node:
 * @model: a #GdaDataModel.
 * @node: an XML node representing a &lt;gda_array_data&gt; XML node.
 * @error: a place to store errors, or %NULL
 *
 * Adds the data from an XML node to the given data model (see the DTD for that node
 * in the $prefix/share/libgda/dtd/libgda-array.dtd file).
 *
 * Upon errors FALSE will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_add_data_from_xml_node (GdaDataModel *model, xmlNodePtr node, GError **error)
{
	xmlNodePtr children;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);

	while (xmlNodeIsText (node))
		node = node->next;

	if (strcmp ((gchar*)node->name, "gda_array_data")) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_XML_FORMAT_ERROR,
			     _("Expected tag <gda_array_data>, got <%s>"), node->name);
		return FALSE;
	}

	for (children = node->xmlChildrenNode; children != NULL; children = children->next) {
		if (!strcmp ((gchar*)children->name, "gda_array_row")) {
			if (!add_xml_row (model, children, error))
				return FALSE;
		}
	}

	return TRUE;
}

/**
 * gda_data_model_import_from_model:
 * @to: the destination #GdaDataModel
 * @from: the source #GdaDataModel
 * @overwrite: TRUE if @to is completely overwritten by @from's data, and FALSE if @from's data is appended to @to
 * @cols_trans: (element-type gint gint) (nullable): a #GHashTable for columns translating, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Copy the contents of the @from data model to the @to data model. The copy stops as soon as an error
 * orrurs.
 *
 * The @cols_trans is a hash table for which keys are @to columns numbers and the values are
 * the corresponding column numbers in the @from data model. To set the values of a column in @to to NULL,
 * create an entry in the hash table with a negative value. For example:
 * <programlisting><![CDATA[GHashTable *hash;
 * gint *ptr;
 * hash = g_hash_table_new_full (g_int_hash, g_int_equal, g_free, NULL);
 * ptr = g_new (gint, 1);
 * *ptr = 2;
 * g_hash_table_insert (hash, ptr, GINT_TO_POINTER (3));
 * gda_data_model_import_from_model (...);
 * g_hash_table_free (hash);
 * ]]></programlisting>
 *
 * Upon errors FALSE will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_model_import_from_model (GdaDataModel *to, GdaDataModel *from, 
				  gboolean overwrite, GHashTable *cols_trans, GError **error)
{
	GdaDataModelIter *from_iter;
	gboolean retval = TRUE;
	gint to_nb_cols, to_nb_rows=-1, to_row = -1;
	gint from_nb_cols;
	GSList *copy_params = NULL;
	gint i;
	GList *append_values = NULL; /* list of #GValue values to add to the @to model */
	GType *append_types = NULL; /* array of the Glib type of the values to append */
	GSList *plist;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (to), FALSE);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (from), FALSE);

	/* return if @to does not have any column */
	to_nb_cols = gda_data_model_get_n_columns (to);
	if (to_nb_cols == 0)
		return TRUE;
	from_nb_cols = gda_data_model_get_n_columns (from);
	if (from_nb_cols == 0)
		return TRUE;

	/* obtain an iterator */
	from_iter = gda_data_model_create_iter (from);
	if (!from_iter) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			      "%s", _("Could not get an iterator for source data model"));
		return FALSE;
	}

	/* make a list of the parameters which will be used during copy (stored in reverse order: last column of data model
	 * represented by the 1st GdaHolder) ,
	 * an some tests */
	for (i = 0; i < to_nb_cols; i++) {
		GdaHolder *param = NULL;
		gint col; /* source column number */
		GdaColumn *column;

		if (cols_trans) {
			col = GPOINTER_TO_INT (g_hash_table_lookup (cols_trans, &i));
			if ((col < 0) || (col >= from_nb_cols)) {
				g_slist_free (copy_params);
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
					     _("Inexistent column in source data model: %d"), col);
				return FALSE;
			}
		}
		else
			col = i;
		if (col >= 0)
			param = gda_data_model_iter_get_holder_for_field (from_iter, col);

		/* tests */
		column = gda_data_model_describe_column (to, i);
		if (! gda_column_get_allow_null (column) && !param) {
			g_slist_free (copy_params);
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUE_TYPE_ERROR,
				     _("Destination column %d can't be NULL but has no correspondence in the "
				       "source data model"), i);
			return FALSE;
		}
		if (param) {
			if ((gda_column_get_g_type (column) != G_TYPE_INVALID) &&
			    (gda_holder_get_g_type (param) != G_TYPE_INVALID) &&
			    !g_value_type_transformable (gda_holder_get_g_type (param), 
							 gda_column_get_g_type (column))) {
			  g_slist_free (copy_params);
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUE_TYPE_ERROR,
					     _("Destination column %d has a gda type (%s) incompatible with "
					       "source column %d type (%s)"), i,
					     gda_g_type_to_string (gda_column_get_g_type (column)),
					     col,
					     gda_g_type_to_string (gda_holder_get_g_type (param)));
				return FALSE;
			}
		}
		
		copy_params = g_slist_prepend (copy_params, param);
	}

#ifdef GDA_DEBUG_NO
	{
		GSList *list;
		for (list = copy_params; list; list = list->next) {
			GdaHolder *param = GDA_HOLDER (list->data);
			g_print ("Copy Param: %s (%s)\n", gda_holder_get_id (param), 
				 gda_g_type_to_string (gda_holder_get_g_type (param)));
		}
	}
#endif

	/* build the append_values list (stored in reverse order!)
	 * node->data is:
	 * - NULL if the value must be replaced by the value of the copied model
	 * - a GValue of type GDA_VALYE_TYPE_NULL if a null value must be inserted in the dest data model
	 * - a GValue of a different type if the value must be converted from the src data model
	 */
	append_types = g_new0 (GType, (guint)to_nb_cols);
	for (plist = copy_params, i = to_nb_cols - 1; plist; plist = plist->next, i--) {
		GdaColumn *column;

		column = gda_data_model_describe_column (to, i);
		if (plist->data) {
			if ((gda_holder_get_g_type (GDA_HOLDER (plist->data)) != gda_column_get_g_type (column)) &&
			    (gda_column_get_g_type (column) != GDA_TYPE_NULL)) {
				GValue *newval;
				
				newval = g_new0 (GValue, 1);
				append_types [i] = gda_column_get_g_type (column);
				g_value_init (newval, append_types [i]);
				append_values = g_list_prepend (append_values, newval);
			}
			else
				append_values = g_list_prepend (append_values, NULL);
		}
		else
			append_values = g_list_prepend (append_values, gda_value_new_null ());
	}
	append_values = g_list_reverse (append_values);
#ifdef GDA_DEBUG_NO
	{
		GList *list;
		for (list = append_values; list; list = list->next) 
			g_print ("Append Value: %s\n", list->data ? gda_g_type_to_string (G_VALUE_TYPE ((GValue *) (list->data))) : "NULL");
	}
#endif
	
	/* actual data copy (no memory allocation is needed here) */
	gda_data_model_send_hint (to, GDA_DATA_MODEL_HINT_START_BATCH_UPDATE, NULL);
	
	if (overwrite) {
		to_row = 0;
		to_nb_rows = gda_data_model_get_n_rows (to);
	}

	gboolean mstatus;
	mstatus = gda_data_model_iter_move_next (from_iter); /* move to first row */
	if (!mstatus) {
		gint crow;
		g_object_get (from_iter, "current-row", &crow, NULL);
		if (crow >= 0)
			retval = FALSE;
	}
	while (retval && gda_data_model_iter_is_valid (from_iter)) {
		GList *values = NULL;
		GList *avlist = append_values;
		plist = copy_params;
		i = to_nb_cols - 1;

		while (plist && avlist && retval) {
			GValue *value;

			value = (GValue *) (avlist->data);
			if (plist->data) {
				value = (GValue *) gda_holder_get_value (GDA_HOLDER (plist->data));
				if (avlist->data) {
					if (append_types [i] && gda_value_is_null ((GValue *) (avlist->data))) 
						gda_value_reset_with_type ((GValue *) (avlist->data), append_types [i]);
					if (!gda_value_is_null (value) && 
					    !g_value_transform (value, (GValue *) (avlist->data))) {
						gchar *str;

						str = gda_value_stringify (value);
						g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUE_TYPE_ERROR,
							     _("Can't transform '%s' from GDA type %s to GDA type %s"),
							     str, 
							     gda_g_type_to_string (G_VALUE_TYPE (value)),
							     gda_g_type_to_string (G_VALUE_TYPE ((GValue *) (avlist->data))));
						g_free (str);
						retval = FALSE;
					}
					value = (GValue *) (avlist->data);
				}
			}

			values = g_list_prepend (values, value);

			plist = plist->next;
			avlist = avlist->next;
			i--;
		}

		if (retval) {
			if (to_row >= to_nb_rows)
				/* we have finished modifying the existing rows */
				to_row = -1;

			if (to_row >= 0) {
				if (!gda_data_model_set_values (to, to_row, values, error))
					retval = FALSE;
				else 
					to_row ++;
			}
			else {
				if (gda_data_model_append_values (to, values, error) < 0) 
					retval = FALSE;
			}
		}
		
		g_list_free (values);
		mstatus = gda_data_model_iter_move_next (from_iter);
		if (!mstatus) {
			gint crow;
			g_object_get (from_iter, "current-row", &crow, NULL);
			if (crow >= 0)
				retval = FALSE;
		}
	}
	
	/* free memory */
  if (copy_params) {
	  g_slist_free (copy_params);
  }
  g_list_free_full (append_values, (GDestroyNotify) gda_value_free);
	g_free (append_types);

	if (retval && (to_row >= 0)) {
		/* remove extra rows */
		for (; retval && (to_row < to_nb_rows); to_row++) {
			if (!gda_data_model_remove_row (to, to_row, error))
				retval = FALSE;
		}
	}

	gda_data_model_send_hint (to, GDA_DATA_MODEL_HINT_END_BATCH_UPDATE, NULL);
	return retval;
}

/**
 * gda_data_model_import_from_string:
 * @model: a #GdaDataModel
 * @string: the string to import data from
 * @cols_trans: (element-type gint gint) (nullable): a hash table containing which columns of @model will be imported, or %NULL for all columns, see gda_data_model_import_from_model()
 * @options: list of options for the export
 * @error: a place to store errors, or %NULL
 *
 * Loads the data from @string into @model.
 *
 * Upon errors FALSE will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_model_import_from_string (GdaDataModel *model,
				   const gchar *string, GHashTable *cols_trans,
				   GdaSet *options, GError **error)
{
	GdaDataModel *import;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (!options || GDA_IS_SET (options), FALSE);
	if (!string)
		return TRUE;

	import = gda_data_model_import_new_mem (string, FALSE, options);
	retval = gda_data_model_import_from_model (model, import, FALSE, cols_trans, error);
	g_object_unref (import);

	return retval;
}

/**
 * gda_data_model_import_from_file:
 * @model: a #GdaDataModel
 * @file: the filename to import from
 * @cols_trans: (element-type gint gint) (nullable): a #GHashTable for columns translating, or %NULL, see gda_data_model_import_from_model()
 * @options: list of options for the export
 * @error: a place to store errors, or %NULL
 *
 * Imports data contained in the @file file into @model; the format is detected.
 *
 * Upon errors FALSE will be returned and @error will be assigned a
 * #GError from the #GDA_DATA_MODEL_ERROR domain.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_import_from_file (GdaDataModel *model, 
				 const gchar *file, GHashTable *cols_trans,
				 GdaSet *options, GError **error)
{
	GdaDataModel *import;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (!options || GDA_IS_SET (options), FALSE);
	if (!file)
		return TRUE;

	import = gda_data_model_import_new_file (file, FALSE, options);
	retval = gda_data_model_import_from_model (model, import, FALSE, cols_trans, error);
	g_object_unref (import);

	return retval;
}

/**
 * gda_data_model_dump:
 * @model: a #GdaDataModel.
 * @to_stream: where to dump the data model
 *
 * Dumps a textual representation of the @model to the @to_stream stream
 *
 * The following environment variables can affect the resulting output:
 * <itemizedlist>
 *   <listitem><para>GDA_DATA_MODEL_DUMP_ROW_NUMBERS: if set, the first column of the output will contain row numbers</para></listitem>
 *   <listitem><para>GDA_DATA_MODEL_DUMP_ATTRIBUTES: if set, also dump the data model's columns' types and value's attributes</para></listitem>
 *   <listitem><para>GDA_DATA_MODEL_DUMP_TITLE: if set, also dump the data model's title</para></listitem>
 *   <listitem><para>GDA_DATA_MODEL_NULL_AS_EMPTY: if set, replace the 'NULL' string with an empty string for NULL values </para></listitem>
 *   <listitem><para>GDA_DATA_MODEL_DUMP_TRUNCATE: if set to a numeric value, truncates the output to the width specified by the value. If the value is -1 then the actual terminal size (if it can be determined) is used</para></listitem>
 * </itemizedlist>
 */
void
gda_data_model_dump (GdaDataModel *model, FILE *to_stream)
{
	gchar *str;
	gboolean dump_attrs = FALSE;
	gboolean dump_rows = FALSE;
	gboolean dump_title = FALSE;
	gboolean null_as_empty = FALSE;
	gint max_width = 0; /* no truncate */
	GError *error = NULL;
	const gchar *cstr;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	if (!to_stream)
		to_stream = stdout;

	if (getenv ("GDA_DATA_MODEL_DUMP_ATTRIBUTES")) /* Flawfinder: ignore */
		dump_attrs = TRUE;
	if (getenv ("GDA_DATA_MODEL_DUMP_ROW_NUMBERS")) /* Flawfinder: ignore */
		dump_rows = TRUE;
	if (getenv ("GDA_DATA_MODEL_DUMP_TITLE")) /* Flawfinder: ignore */
		dump_title = TRUE;
	if (getenv ("GDA_DATA_MODEL_NULL_AS_EMPTY")) /* Flawfinder: ignore */
		null_as_empty = TRUE;
	cstr = getenv ("GDA_DATA_MODEL_DUMP_TRUNCATE");
	if (cstr) {
		max_width = atoi (cstr);
#ifdef TIOCGWINSZ
		if (max_width < 0) {
			struct winsize window_size;
			if (ioctl (0,TIOCGWINSZ, &window_size) == 0)
				max_width = (int) window_size.ws_col;
		}
#endif
		if (max_width < 0)
			max_width = 0;
	}

	str = real_gda_data_model_dump_as_string (model, FALSE, dump_rows, dump_title, null_as_empty, max_width,
						  TRUE, TRUE, FALSE, TRUE, NULL, 0, &error);
	if (str) {
		g_fprintf (to_stream, "%s", str);
		g_free (str);
		if (dump_attrs) {
			str = real_gda_data_model_dump_as_string (model, TRUE, dump_rows, dump_title, null_as_empty, max_width,
								  TRUE, TRUE, FALSE, TRUE, NULL, 0, &error);
			if (str) {
				g_fprintf (to_stream, "%s", str);
				g_free (str);
			}
			else {
				g_warning (_("Could not dump data model's attributes: %s"),
					   error && error->message ? error->message : _("No detail"));
				if (error)
					g_error_free (error);
			}
		}
	}
	else {
		g_warning (_("Could not dump data model's contents: %s"),
			   error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
	}
}

/**
 * gda_data_model_dump_as_string:
 * @model: a #GdaDataModel.
 *
 * Dumps a textual representation of the @model into a new string. The main differences with gda_data_model_export_to_string() are that
 * the formatting options are passed using environment variables, and that the data is dumped regardless of the user locale (e.g. dates
 * are not formatted according to the locale).
 *
 * The following environment variables can affect the resulting output:
 * <itemizedlist>
 *   <listitem><para>GDA_DATA_MODEL_DUMP_ROW_NUMBERS: if set, the first column of the output will contain row numbers</para></listitem>
 *   <listitem><para>GDA_DATA_MODEL_DUMP_TITLE: if set, also dump the data model's title</para></listitem>
 *   <listitem><para>GDA_DATA_MODEL_NULL_AS_EMPTY: if set, replace the 'NULL' string with an empty string for NULL values </para></listitem>
 *   <listitem><para>GDA_DATA_MODEL_DUMP_TRUNCATE: if set to a numeric value, truncates the output to the width specified by the value. If the value is -1 then the actual terminal size (if it can be determined) is used</para></listitem>
 * </itemizedlist>
 *
 * Returns: (transfer full): a new string.
 */
gchar *
gda_data_model_dump_as_string (GdaDataModel *model)
{
	gboolean dump_attrs = FALSE;
	gboolean dump_rows = FALSE;
	gboolean dump_title = FALSE;
	gboolean null_as_empty = FALSE;
	gint max_width = 0; /* no truncate */
	const gchar *cstr;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (getenv ("GDA_DATA_MODEL_DUMP_ATTRIBUTES")) /* Flawfinder: ignore */
		dump_attrs = TRUE;
	if (getenv ("GDA_DATA_MODEL_DUMP_ROW_NUMBERS")) /* Flawfinder: ignore */
		dump_rows = TRUE;
	if (getenv ("GDA_DATA_MODEL_DUMP_TITLE")) /* Flawfinder: ignore */
		dump_title = TRUE;
	if (getenv ("GDA_DATA_MODEL_NULL_AS_EMPTY")) /* Flawfinder: ignore */
		null_as_empty = TRUE;
	cstr = getenv ("GDA_DATA_MODEL_DUMP_TRUNCATE");
	if (cstr) {
		max_width = atoi (cstr);
#ifdef TIOCGWINSZ
		if (max_width < 0) {
			struct winsize window_size;
			if (ioctl (0,TIOCGWINSZ, &window_size) == 0)
				max_width = (int) window_size.ws_col;
		}
#endif
		if (max_width < 0)
			max_width = 0;
	}
	if (dump_attrs) {
		GString *string;
		gchar *tmp;
		tmp = real_gda_data_model_dump_as_string (model, FALSE, dump_rows, dump_title, null_as_empty, max_width,
							  TRUE, TRUE, FALSE, TRUE, NULL, 0, NULL);
		string = g_string_new (tmp);
		g_free (tmp);
		tmp = real_gda_data_model_dump_as_string (model, TRUE, dump_rows, dump_title, null_as_empty, max_width,
							  TRUE, TRUE, FALSE, TRUE, NULL, 0, NULL);
		g_string_append_c (string, '\n');
		g_string_append (string, tmp);
		g_free (tmp);
		return g_string_free (string, FALSE);
	}
	else
		return real_gda_data_model_dump_as_string (model, FALSE, dump_rows, dump_title, null_as_empty, max_width,
							   TRUE, TRUE, FALSE, TRUE, NULL, 0, NULL);
}

static void
string_get_dimensions (const gchar *string, gint *width, gint *rows)
{
	const gchar *ptr;
	gint w = 0, h = 0, maxw = 0;
	for (ptr = string; *ptr; ptr = g_utf8_next_char (ptr)) {
		if (*ptr == '\n') {
			h++;
			maxw = MAX (maxw, w);
			w = 0;
		}
		else
			w++;
	}
	maxw = MAX (maxw, w);

	if (width)
		*width = maxw;
	if (rows)
		*rows = h;
}

/*
 * Returns: a new string, or %NULL if @model does not support random access
 */
static gchar *
real_gda_data_model_dump_as_string (GdaDataModel *model, gboolean dump_attributes, 
				    gboolean dump_rows, gboolean dump_title, gboolean null_as_empty,
				    gint max_width, gboolean dump_separators, gboolean dump_sep_line,
				    gboolean use_data_handlers, gboolean dump_column_titles,
				    const gint *rows, gint nb_rows,
				    GError **error)
{
#define ERROR_STRING "####"
#define MULTI_LINE_NO_SEPARATOR

	gboolean allok = TRUE;
	GString *string = NULL;
	gchar *str;
	gint n_cols, n_rows, real_n_rows;
	gint *cols_size;
	gboolean *cols_is_num;
	gchar *sep_col  = " | ";
#ifdef MULTI_LINE_NO_SEPARATOR
	gchar *sep_col_e  = "   ";
#endif
	gchar *sep_row  = "-+-";
	gchar *sep_fill = "-";
	gint i, j;
	const GValue *value;

	gint col_offset = dump_rows ? 1 : 0;
	GdaDataModel *ramodel = NULL;

	if (!dump_separators) {
		sep_col = " ";
		sep_row = "-";
#ifdef MULTI_LINE_NO_SEPARATOR
		sep_col_e  = " ";
#endif
	}

#ifdef HAVE_LOCALE_H
#ifndef G_OS_WIN32
	if (dump_separators || dump_sep_line) {
		int utf8_mode;
		utf8_mode = (strcmp (nl_langinfo (CODESET), "UTF-8") == 0);
		if (utf8_mode) {
			if (dump_separators) {
				sep_col = " │ ";
				sep_row = "─┼─";
			}
			else
				sep_row = "─";
			sep_fill = "─";
		}
	}
#endif
#endif

	/* compute the columns widths: using column titles... */
	n_cols = gda_data_model_get_n_columns (model);
	n_rows = gda_data_model_get_n_rows (model);
	real_n_rows = n_rows;
	if (rows) {
		/* no error checking is done here, row'existence is checked when used */
		n_rows = nb_rows;
	}
	cols_size = g_new0 (gint, n_cols + col_offset);
	cols_is_num = g_new0 (gboolean, n_cols + col_offset);
	
	if (dump_rows) {
		str = g_strdup_printf ("%d", n_rows);
		cols_size [0] = MAX (strlen (str), strlen ("#row"));
		cols_is_num [0] = TRUE;
		g_free (str);
	}

	for (i = 0; i < n_cols; i++) {
		GdaColumn *gdacol;
		GType coltype;

		if (dump_attributes) {
			GdaColumn *col = gda_data_model_describe_column (model, i);
			g_print ("Model col %d has type %s\n", i,
				 gda_g_type_to_string (gda_column_get_g_type (col)));
		}

		if (dump_column_titles) {
			str = (gchar *) gda_data_model_get_column_title (model, i);
			if (str)
				cols_size [i + col_offset] = g_utf8_strlen (str, -1);
			else
				cols_size [i + col_offset] = 6; /* for "<none>" */
		}

		if (! dump_attributes) {
			gdacol = gda_data_model_describe_column (model, i);
			coltype = gda_column_get_g_type (gdacol);
			if ((coltype == G_TYPE_INT64) ||
			    (coltype == G_TYPE_UINT64) ||
			    (coltype == G_TYPE_INT) ||
			    (coltype == GDA_TYPE_NUMERIC) ||
			    (coltype == G_TYPE_FLOAT) ||
			    (coltype == G_TYPE_DOUBLE) ||
			    (coltype == GDA_TYPE_SHORT) ||
			    (coltype == GDA_TYPE_USHORT) ||
			    (coltype == G_TYPE_CHAR) ||
			    (coltype == G_TYPE_UCHAR) ||
			    (coltype == G_TYPE_UINT))
				cols_is_num [i + col_offset] = TRUE;
			else
				cols_is_num [i + col_offset] = FALSE;
		}
		else
			cols_is_num [i + col_offset] = TRUE;
	}

	/* ... and using column data */
	if (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM)
		ramodel = g_object_ref (model);
	else {
		if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD)) {
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
				      "%s", _("Data model does not support backward cursor move"));
			allok = FALSE;
			goto out;
		}
		ramodel = gda_data_access_wrapper_new (model);
	}

	for (j = 0; j < n_rows; j++) {
		if (rows && (rows [j] >= real_n_rows))
			continue; /* ignore that row */
		for (i = 0; i < n_cols; i++) {
			if (! dump_attributes) {
				if (rows)
					value = gda_data_model_get_value_at (ramodel, i, rows[j], NULL);
				else
					value = gda_data_model_get_value_at (ramodel, i, j, NULL);
				if (!value) {
					cols_size [i + col_offset] = MAX ((guint)cols_size [i + col_offset], strlen (ERROR_STRING));
				}
				else {
					gboolean alloc = FALSE;
					str = NULL;
					if (null_as_empty) {
						if (!value || gda_value_is_null (value))
							str = "";
					}
					if (!str) {
						if (value) {
							if (G_VALUE_TYPE (value) == G_TYPE_STRING)
								str = (gchar*) g_value_get_string (value);
							else {
								if (use_data_handlers) {
									GdaDataHandler *dh = NULL;
									GdaConnection *cnc;
									GdaServerProvider *prov;
									cnc = g_object_get_data (G_OBJECT (model), "cnc");
									if (!cnc && GDA_IS_DATA_SELECT (model))
										cnc = gda_data_select_get_connection (GDA_DATA_SELECT (model));
									if (cnc) {
										prov = gda_connection_get_provider (cnc);
										dh = gda_server_provider_get_data_handler_g_type (prov, cnc,
																  G_VALUE_TYPE (value));
									}
									if (!dh)
										dh = gda_data_handler_get_default (G_VALUE_TYPE (value));
									else
										g_object_ref (dh);

									if (dh) {
										str = gda_data_handler_get_str_from_value (dh, value);
										g_object_unref (dh);
									}
									else
										str = gda_value_stringify ((GValue*)value);
								}
								else
									str = gda_value_stringify ((GValue*)value);
								alloc = TRUE;
							}
						}
						else
							str = "NULL";
					}
					if (str) {
						gint w;
						string_get_dimensions (str, &w, NULL);
						cols_size [i + col_offset] = MAX (cols_size [i + col_offset], w);
						if (alloc)
							g_free (str);
					}
				}
			}
			else {
				GdaValueAttribute attrs;
				gint w;
				if (rows)
					attrs = gda_data_model_get_attributes_at (ramodel, i, rows[j]);
				else
					attrs = gda_data_model_get_attributes_at (ramodel, i, j);
				str = g_strdup_printf ("%u", attrs);
				string_get_dimensions (str, &w, NULL);
				cols_size [i + col_offset] = MAX (cols_size [i + col_offset], w);
				g_free (str);
			}
		}
	}
	
	string = g_string_new ("");

	/* actual dumping of the contents: data model's title */
	if (dump_title) {
		const gchar *title;
		title = g_object_get_data (G_OBJECT (model), "name");
		if (title) {
			gsize total_width = n_cols -1;

			for (i = 0; i < n_cols; i++)
				total_width += cols_size [i];
			if (total_width > strlen (title))
				i = (total_width - strlen (title))/2.;
			else
				i = 0;
			for (; i > 0; i--)
				g_string_append_c (string, ' ');
			g_string_append (string, title);
			g_string_append_c (string, '\n');
		}
	}

	/* ...column titles...*/
	if (dump_rows) 
		g_string_append_printf (string, "%*s", cols_size [0], "#row");

	if (dump_column_titles) {
		for (i = 0; i < n_cols; i++) {
			gint j, max;
			str = (gchar *) gda_data_model_get_column_title (model, i);
			if (dump_rows || (i != 0))
				g_string_append_printf (string, "%s", sep_col);

			if (str) {
				g_string_append_printf (string, "%s", str);
				max = cols_size [i + col_offset] - g_utf8_strlen (str, -1);
			}
			else {
				g_string_append (string, "<none>");
				max = cols_size [i + col_offset] - 6;
			}
			for (j = 0; j < max; j++)
				g_string_append_c (string, ' ');
		}
		g_string_append_c (string, '\n');
		
		/* ... separation line ... */
		if (dump_sep_line) {
			for (i = 0; i < n_cols + col_offset; i++) {
				if (i != 0)
					g_string_append_printf (string, "%s", sep_row);
				for (j = 0; j < cols_size [i]; j++)
					g_string_append (string, sep_fill);
			}
			g_string_append_c (string, '\n');
		}
	}

	/* ... and data */
	for (j = 0; j < n_rows; j++) {
		if (rows && (rows [j] >= real_n_rows))
			continue; /* ignore that row */

		/* determine height for each column in that row */
		gint *cols_height = g_new (gint, n_cols + col_offset);
		gchar ***cols_str = g_new (gchar **, n_cols + col_offset);
		gint k, kmax = 1;
		
		if (dump_rows) {
			str = g_strdup_printf ("%d", j);
			cols_str [0] = g_strsplit (str, "\n", -1);
			cols_height [0] = 1;
			kmax = 1;
		}
		
		for (i = 0; i < n_cols; i++) {
			gboolean alloc = FALSE;
			str = NULL;
			if (!dump_attributes) {
				if (rows)
					value = gda_data_model_get_value_at (ramodel, i, rows[j], NULL);
				else
					value = gda_data_model_get_value_at (ramodel, i, j, NULL);
				if (!value)
					str = ERROR_STRING;
				else {
					if (null_as_empty) {
						if (!value || gda_value_is_null (value))
							str = "";
					}
					if (!str) {
						if (value) {
							if (G_VALUE_TYPE (value) == G_TYPE_STRING)
								str = (gchar*) g_value_get_string (value);
							else {
								if (use_data_handlers) {
									GdaDataHandler *dh = NULL;
									GdaConnection *cnc;
									GdaServerProvider *prov;
									cnc = g_object_get_data (G_OBJECT (model), "cnc");
									if (!cnc && GDA_IS_DATA_SELECT (model))
										cnc = gda_data_select_get_connection (GDA_DATA_SELECT (model));
									if (cnc) {
										prov = gda_connection_get_provider (cnc);
										dh = gda_server_provider_get_data_handler_g_type (prov, cnc,
																  G_VALUE_TYPE (value));
									}
									if (!dh)
										dh = gda_data_handler_get_default (G_VALUE_TYPE (value));
									else
										g_object_ref (dh);

									if (dh) {
										str = gda_data_handler_get_str_from_value (dh, value);
										g_object_unref (dh);
									}
									else
										str = gda_value_stringify ((GValue*)value);
								}
								else
									str = gda_value_stringify ((GValue*)value);
								alloc = TRUE;
							}
						}
						else
							str = "NULL";
					}
				}
			}
			else {
				GdaValueAttribute attrs;
				if (rows)
					attrs = gda_data_model_get_attributes_at (ramodel, i, rows[j]);
				else
					attrs = gda_data_model_get_attributes_at (ramodel, i, j);
				str = g_strdup_printf ("%u", attrs);
				alloc = TRUE;
			}
			if (str) {
				cols_str [i + col_offset] = g_strsplit (str, "\n", -1);
				if (alloc)
					g_free (str);
				cols_height [i + col_offset] = g_strv_length (cols_str [i + col_offset]);
				kmax = MAX (kmax, cols_height [i + col_offset]);
			}
			else {
				cols_str [i + col_offset] = NULL;
				cols_height [i + col_offset] = 0;
			}
		}
		
		for (k = 0; k < kmax; k++) {
			for (i = 0; i < n_cols + col_offset; i++) {
				gboolean align_center = FALSE;
				if (i != 0) {
#ifdef MULTI_LINE_NO_SEPARATOR
					if (k != 0)
						g_string_append_printf (string, "%s", sep_col_e);
					else
#endif
						g_string_append_printf (string, "%s", sep_col);
				}
				if (k < cols_height [i]) 
					str = (cols_str[i])[k];
				else {
#ifdef MULTI_LINE_NO_SEPARATOR
					str = "";
#else
					if (cols_height [i] == 0)
						str = "";
					else
						str = ".";
					align_center = TRUE;
#endif
				}
				
				if (cols_is_num [i])
					g_string_append_printf (string, "%*s", cols_size [i], str);
				else {
					gint j, max;
					if (str) 
						max = cols_size [i] - g_utf8_strlen (str, -1);
					else
						max = cols_size [i];
					
					if (!align_center) {
						g_string_append_printf (string, "%s", str);
						for (j = 0; j < max; j++)
							g_string_append_c (string, ' ');
					}
					else {
						for (j = 0; j < max / 2; j++)
							g_string_append_c (string, ' ');
						g_string_append_printf (string, "%s", str);
						for ( j+= g_utf8_strlen (str, -1); j < cols_size [i]; j++)
							g_string_append_c (string, ' ');
					}
					/*
					  gint j, max;
					  if (str) {
					  g_string_append_printf (string, "%s", str);
					  max = cols_size [i] - g_utf8_strlen (str, -1);
					  }
					  else
					  max = cols_size [i];
					  for (j = 0; j < max; j++)
					  g_string_append_c (string, ' ');
					*/
				}
			}
			g_string_append_c (string, '\n');
		}
		
		g_free (cols_height);
		for (i = 0; i < n_cols; i++) 
			g_strfreev (cols_str [i]);
		g_free (cols_str);
	}

	/* status message */
	g_string_append_c (string, '(');
	if (n_rows > 0)
		g_string_append_printf (string, ngettext("%d row", "%d rows", n_rows), n_rows);
	else
		g_string_append_printf (string, _("0 row"));

	GError **exceptions;
	exceptions = gda_data_model_get_exceptions (model);
	if (exceptions) {
		gint i;
		for (i = 0; exceptions[i]; i++) {
			GError *ex;
			ex = exceptions[i];
			if (ex && ex->message) {
				g_string_append (string, ", ");
				g_string_append (string, ex->message);
			}
		}
	}
	g_string_append (string, ")\n");


 out:
	if (ramodel)
		g_object_unref (ramodel);
	g_free (cols_size);
	g_free (cols_is_num);

	if (allok) {
		str = string->str;
		g_string_free (string, FALSE);
		if (max_width > 0) {
			/* truncate all lines */
			GString *ns;
			ns = g_string_sized_new (strlen (str));

			gchar *ptr, *sptr;
			gint len; /* number of characters since start of line */
			for (ptr = str, sptr = ptr, len=0;
			     *ptr;
			     ptr = g_utf8_next_char (ptr), len++) {
				if (*ptr == '\n') {
					*ptr = 0;
					len = 0;
					g_string_append (ns, sptr);
					g_string_append_c (ns, '\n');
					*ptr = '\n';
					sptr = ptr+1;
				}
				else if (len >= max_width) {
					gchar c;
					c = *ptr;
					*ptr = 0;
					len = 0;
					g_string_append (ns, sptr);
					g_string_append_c (ns, '\n');
					*ptr = c;
					for (; *ptr && (*ptr != '\n');
					     ptr = g_utf8_next_char (ptr));
					if (!*ptr)
						break;
					sptr = ptr+1;
				}
			}
			g_free (str);
			str = ns->str;
			g_string_free (ns, FALSE);
		}
	}
	else {
		str = NULL;
		g_string_free (string, TRUE);
	}
	return str;
}
