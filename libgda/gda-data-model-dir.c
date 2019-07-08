/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2013, 2018-2019 Daniel Espinosa <esodan@gmail.com>
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
#define G_LOG_DOMAIN "GDA-data-model-dir"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-dir.h>
#include <libgda/gda-data-model-extra.h>

#ifdef HAVE_GIO
#include <gio/gio.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef G_OS_WIN32
#include <sys/mman.h>
#else
#include <windows.h>
#endif

#define __GDA_INTERNAL__
#include "dir-blob-op.h"


/* GdaDataModel interface */
static void                 gda_data_model_dir_data_model_init (GdaDataModelInterface *iface);
static gint                 gda_data_model_dir_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_dir_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_dir_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_model_dir_get_access_flags(GdaDataModel *model);
static const GValue        *gda_data_model_dir_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);
static GdaValueAttribute    gda_data_model_dir_get_attributes_at (GdaDataModel *model, gint col, gint row);

static gboolean             gda_data_model_dir_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *value, GError **error);
static gboolean             gda_data_model_dir_set_values (GdaDataModel *model, gint row, GList *values, GError **error);
static gint                 gda_data_model_dir_append_values (GdaDataModel *model, const GList *values, GError **error);
static gboolean             gda_data_model_dir_remove_row (GdaDataModel *model, gint row, GError **error);


typedef struct {
	gchar     *basedir;
	GSList    *errors; /* list of errors as GError structures */
	GSList    *columns; /* list of GdaColumn objects */

	GPtrArray *rows; /* array of FileRow pointers */
	gint       upd_row; /* internal usage when updating contents */

	GValue    *tmp_value; /* GValue returned by gda_data_model_get_value_at() */
} GdaDataModelDirPrivate;

G_DEFINE_TYPE_WITH_CODE (GdaDataModelDir, gda_data_model_dir, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaDataModelDir)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_MODEL, gda_data_model_dir_data_model_init))

/* Row implementation details */
typedef struct {
	gchar  *reldir;
	gchar  *raw_filename_value; /* UTF-8 on Windows, FS encoding otherwise */
	GValue *filename_value; /* in UTF-8 */
	GValue *size_value;
	GValue *mime_value;
	GValue *md5sum_value;
	GValue *data_value;
} FileRow;

#define FILE_ROW(x) ((FileRow*)(x))
static FileRow *file_row_new (void);
static void     file_row_clean (FileRow *row);
static void     file_row_free (FileRow *row);

static gboolean update_file_size (FileRow *row, const gchar *complete_filename);
static gboolean update_file_md5sum (FileRow *row, const gchar *complete_filename);
static gboolean update_file_mime (FileRow *row, const gchar *complete_filename);

static gboolean dir_equal (const gchar *path1, const gchar *path2);
static gchar *  compute_dirname (GdaDataModelDir *model, FileRow *row);
static gchar *  compute_filename (GdaDataModelDir *model, FileRow *row);

/* properties */
enum
	{
		PROP_0,
		PROP_BASEDIR
	};

/* columns */
enum
	{
		COL_DIRNAME,
		COL_FILENAME,
		COL_SIZE,
		COL_MIME,
		COL_MD5SUM,
		COL_DATA,

		COL_LAST
	};

static void gda_data_model_dir_dispose    (GObject *object);

static void gda_data_model_dir_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void gda_data_model_dir_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);

static void add_error (GdaDataModelDir *model, const gchar *err);
static void update_data_model (GdaDataModelDir *model);

#define CLASS(model) (GDA_DATA_MODEL_DIR_CLASS (G_OBJECT_GET_CLASS (model)))

/*
 * Object init and dispose
 */
static void
gda_data_model_dir_data_model_init (GdaDataModelInterface *iface)
{
        iface->get_n_rows = gda_data_model_dir_get_n_rows;
        iface->get_n_columns = gda_data_model_dir_get_n_columns;
        iface->describe_column = gda_data_model_dir_describe_column;
        iface->get_access_flags = gda_data_model_dir_get_access_flags;
        iface->get_value_at = gda_data_model_dir_get_value_at;
        iface->get_attributes_at = gda_data_model_dir_get_attributes_at;

        iface->create_iter = NULL;

        iface->set_value_at = gda_data_model_dir_set_value_at;
        iface->set_values = gda_data_model_dir_set_values;
        iface->append_values = gda_data_model_dir_append_values;
        iface->append_row = NULL;
        iface->remove_row = gda_data_model_dir_remove_row;
        iface->find_row = NULL;

        iface->freeze = NULL;
        iface->thaw = NULL;
        iface->get_notify = NULL;
        iface->send_hint = NULL;
}

static void
gda_data_model_dir_init (GdaDataModelDir *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_DIR (model));

	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);
	priv->basedir = NULL;
	priv->columns = NULL;
	priv->rows = g_ptr_array_new (); /* array of FileRow pointers */
	priv->tmp_value = NULL;
}

static void
gda_data_model_dir_class_init (GdaDataModelDirClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* properties */
        object_class->set_property = gda_data_model_dir_set_property;
        object_class->get_property = gda_data_model_dir_get_property;
        g_object_class_install_property (object_class, PROP_BASEDIR,
                                         g_param_spec_string ("basedir", NULL, "Base directory", NULL,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));

	/* virtual functions */
	object_class->dispose = gda_data_model_dir_dispose;
}

static void
file_row_foreach_func (FileRow *row, G_GNUC_UNUSED gpointer data)
{
	file_row_free (row);
}

static void
gda_data_model_dir_dispose (GObject * object)
{
	GdaDataModelDir *model = (GdaDataModelDir *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_DIR (model));
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);

	if (priv->tmp_value) {
		gda_value_free (priv->tmp_value);
		priv->tmp_value = NULL;
	}

	if (priv->basedir) {
		g_free (priv->basedir);
		priv->basedir = NULL;
	}

	if (priv->errors) {
		g_slist_free_full (priv->errors, (GDestroyNotify) g_error_free);
	}

	if (priv->columns) {
		g_slist_free_full (priv->columns, (GDestroyNotify) g_object_unref);
		priv->columns = NULL;
	}

	g_ptr_array_foreach (priv->rows, (GFunc) file_row_foreach_func, NULL);
	g_ptr_array_free (priv->rows, TRUE);

	G_OBJECT_CLASS (gda_data_model_dir_parent_class)->dispose (object);
}

static void
add_error (GdaDataModelDir *model, const gchar *err)
{
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);
	GError *error = NULL;

        g_set_error (&error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
		     "%s", err);
        priv->errors = g_slist_append (priv->errors, error);
}

static void
gda_data_model_dir_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
        GdaDataModelDir *model;
        const gchar *string;

        model = GDA_DATA_MODEL_DIR (object);
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);
	switch (param_id) {
	case PROP_BASEDIR:
		if (priv->basedir) {
			g_free (priv->basedir);
			priv->basedir = NULL;
		}
		string = g_value_get_string (value);
		if (string)
			priv->basedir = g_strdup (string);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}

	if (priv->basedir) {
		/* create columns */
		priv->columns = NULL;
		GdaColumn *column;

		/* COL_DIRNAME */
		column = gda_column_new ();
		priv->columns = g_slist_append (priv->columns , column);
		gda_column_set_name (column, "dir_name");
		gda_column_set_description (column, "dir_name");
		gda_column_set_g_type (column, G_TYPE_STRING);

		/* COL_FILENAME */
		column = gda_column_new ();
		priv->columns = g_slist_append (priv->columns , column);
		gda_column_set_name (column, "file_name");
		gda_column_set_description (column, "file_name");
		gda_column_set_g_type (column, G_TYPE_STRING);

		/* COL_SIZE */
		column = gda_column_new ();
		priv->columns = g_slist_append (priv->columns , column);
		gda_column_set_name (column, "size");
		gda_column_set_description (column, "size");
		gda_column_set_g_type (column, G_TYPE_UINT);

		/* COL_MIME */
		column = gda_column_new ();
		priv->columns = g_slist_append (priv->columns , column);
		gda_column_set_name (column, "mime_type");
		gda_column_set_description (column, "mime_type");
		gda_column_set_g_type (column, G_TYPE_STRING);

		/* COL_MD5SUM */
		column = gda_column_new ();
		priv->columns = g_slist_append (priv->columns , column);
		gda_column_set_name (column, "md5sum");
		gda_column_set_description (column, "md5sum");
		gda_column_set_g_type (column, G_TYPE_STRING);

		/* COL_DATA */
		column = gda_column_new ();
		priv->columns = g_slist_append (priv->columns , column);
		gda_column_set_name (column, "data");
		gda_column_set_description (column, "data");
		gda_column_set_g_type (column, GDA_TYPE_BLOB);

		/* number of rows */
		update_data_model (model);
	}
}

static void
update_data_model_real (GdaDataModelDir *model, const gchar *rel_path)
{
	GDir *dir;
	GError *error = NULL;
	const gchar *raw_filename;
	gchar *complete_dir;
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);

	complete_dir = g_build_path (G_DIR_SEPARATOR_S, priv->basedir, rel_path, NULL);
	dir = g_dir_open (complete_dir, 0, &error);
	if (!dir) {
		add_error (model, error && error->message ? error->message : _("No detail"));
		g_error_free (error);
		g_free (complete_dir);
		return;
	}

	raw_filename = g_dir_read_name (dir);
	while (raw_filename) {
		gchar *complete_filename;
		complete_filename = g_build_filename (complete_dir, raw_filename, NULL);

		if (g_file_test (complete_filename, G_FILE_TEST_IS_DIR)) {
			/* the . and .. directories are omitted by g_dir_read_name() */
			/* ignore hidden directories */
			if (*raw_filename != '.') {
				gchar *path;

				path = g_build_path (G_DIR_SEPARATOR_S, rel_path, raw_filename, NULL);
				update_data_model_real (model, path);
				g_free (path);
			}
		}
		else {
			/* ignore hidden files */
			if (*raw_filename != '.') {
				gchar *utf8_filename;
#ifndef G_OS_WIN32
				/* FIXME: correctly do the conversion */
				utf8_filename = g_strdup (raw_filename);
#else
				utf8_filename = g_strdup (raw_filename);
#endif
				FileRow *row;
				priv->upd_row ++;

				if (priv->upd_row < (int)priv->rows->len) {
					row = g_ptr_array_index (priv->rows, priv->upd_row);
					file_row_clean (row);
				}
				else
					row = file_row_new ();
				row->reldir = g_strdup (rel_path);
#ifndef G_OS_WIN32
				row->raw_filename_value = g_strdup (raw_filename);
#else
				row->raw_filename_value = NULL; /* no need top copy on Windows */
#endif
				g_value_take_string (row->filename_value = gda_value_new (G_TYPE_STRING),
						     utf8_filename);

				/* file size */
				update_file_size (row, complete_filename);

				/* other attributes, computed only when needed */
				row->mime_value = NULL;
				row->md5sum_value = NULL;
				row->data_value = NULL;

				/* add row */
				if (priv->upd_row < (int)priv->rows->len)
					gda_data_model_row_updated ((GdaDataModel *) model, priv->upd_row);
				else {
					g_ptr_array_add (priv->rows, row);
					gda_data_model_row_inserted ((GdaDataModel *) model,
								     priv->rows->len - 1);
				}
			}
		}
		g_free (complete_filename);

		raw_filename = g_dir_read_name (dir);
	}

	g_free (complete_dir);
	g_dir_close (dir);
}

/* Returns: TRUE if FileRow value has been changed */
static gboolean
update_file_size (FileRow *row, const gchar *complete_filename)
{
	struct stat filestat;
	gboolean changed = TRUE;

	if (! g_stat (complete_filename, &filestat)) {
		if (row->size_value && (G_VALUE_TYPE (row->size_value) == G_TYPE_UINT)
		    && (g_value_get_uint (row->size_value) == (guint)filestat.st_size))
			changed = FALSE;
		else {
			if (row->size_value)
				gda_value_free (row->size_value);
			g_value_set_uint (row->size_value = gda_value_new (G_TYPE_UINT), filestat.st_size);
		}
	}
	else {
		if (row->size_value && gda_value_is_null (row->size_value))
			changed = FALSE;
		else {
			if (row->size_value)
				gda_value_free (row->size_value);
			row->size_value = gda_value_new_null ();
		}
	}

	return changed;
}

/* Returns: TRUE if FileRow value has been changed */
static gboolean
update_file_md5sum (FileRow *row, const gchar *complete_filename)
{
	gboolean changed = TRUE;
	GValue *value = NULL;
	int fd;
        gpointer map;
        guint length;

	/* file mapping in mem */
	length = g_value_get_uint (row->size_value);
	if (length == 0)
		goto md5end;
	fd = open (complete_filename, O_RDONLY); /* Flawfinder: ignore */
	if (fd < 0)
		goto md5end;
#ifndef G_OS_WIN32
	map = mmap (NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED) {
		close (fd);
		goto md5end;
	}
#else
	HANDLE view = CreateFileMapping ((HANDLE) fd,
					 NULL, PAGE_READONLY|SEC_COMMIT, 0,0 , NULL);
	if (!view) {
		close (fd);
		goto md5end;
	}
	map = MapViewOfFile (view, FILE_MAP_READ, 0, 0, length);
	if (!map) {
		close (fd);
		goto md5end;
	}
#endif /* !G_OS_WIN32 */

	gchar *md5str;
	md5str = g_compute_checksum_for_data (G_CHECKSUM_MD5, map, length);
	value = gda_value_new (G_TYPE_STRING);
	g_value_take_string (value, md5str);

#ifndef G_OS_WIN32
	munmap (map, length);
#else
	UnmapViewOfFile (map);
#endif /* !G_OS_WIN32 */
	close (fd);

 md5end:
	if (value) {
		if (row->md5sum_value && (G_VALUE_TYPE (row->md5sum_value) == G_TYPE_STRING)
		    && !gda_value_compare (row->md5sum_value, value))
			changed = FALSE;
		else {
			if (row->md5sum_value)
				gda_value_free (row->md5sum_value);
			row->md5sum_value = value;
		}
	}
	else {
		if (row->md5sum_value && gda_value_is_null (row->md5sum_value))
			changed = FALSE;
		else {
			if (row->md5sum_value)
				gda_value_free (row->md5sum_value);
			row->md5sum_value = gda_value_new_null ();
		}
	}

	return changed;
}

/* Returns: TRUE if FileRow value has been changed */
static gboolean
update_file_mime (FileRow *row, const gchar *complete_filename)
{
	gboolean changed = TRUE;
	GValue *value = NULL;
#ifdef HAVE_GIO
	GFile *file;
	GFileInfo *info;
	file = g_file_new_for_path (complete_filename);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
				  G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (info) {
		value = gda_value_new (G_TYPE_STRING);
                g_value_set_string (value, g_file_info_get_content_type (info));
		g_object_unref (info);
	}
	else
		value = gda_value_new_null ();
	g_object_unref (file);
#else
	value = gda_value_new_null ();
#endif

	if (value) {
		if (row->mime_value && (G_VALUE_TYPE (row->mime_value) == G_TYPE_STRING)
		    && !gda_value_compare (row->mime_value, value))
			changed = FALSE;
		else {
			if (row->mime_value)
				gda_value_free (row->mime_value);
			row->mime_value = value;
		}
	}
	else {
		if (row->mime_value && gda_value_is_null (row->mime_value))
			changed = FALSE;
		else {
			if (row->mime_value)
				gda_value_free (row->mime_value);
			row->mime_value = gda_value_new_null ();
		}
	}

	return changed;
}

static void
update_data_model (GdaDataModelDir *model)
{
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);
	priv->upd_row = -1;
	update_data_model_real (model, "");

	/* clean extra rows */
	gsize i;
	for (i = priv->upd_row + 1; i < priv->rows->len; i++) {
		FileRow *row = g_ptr_array_index (priv->rows, priv->rows->len - 1);
		file_row_free (row);
		g_ptr_array_remove_index (priv->rows, priv->rows->len - 1);
		gda_data_model_row_removed ((GdaDataModel *) model, priv->rows->len - 1);
	}
}

static void
gda_data_model_dir_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	GdaDataModelDir *model;

	model = GDA_DATA_MODEL_DIR (object);
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);
	if (priv) {
		switch (param_id) {
		case PROP_BASEDIR:
			g_value_set_string (value, priv->basedir);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * gda_data_model_dir_new:
 * @basedir: a directory
 *
 * Creates a new #GdaDataModel object to list all the files starting from @basedir
 *
 * Returns: (transfer full): a new #GdaDataModel
 */
GdaDataModel *
gda_data_model_dir_new (const gchar *basedir)
{
	GdaDataModel *model;

	g_return_val_if_fail (basedir && *basedir, NULL);

	model = (GdaDataModel *) g_object_new (GDA_TYPE_DATA_MODEL_DIR, "basedir", basedir, NULL);

	return model;
}

/**
 * gda_data_model_dir_get_errors:
 * @model: a #GdaDataModelDir object
 *
 * Get the list of errors which have occurred while using @model
 *
 * Returns: (transfer none) (element-type GLib.Error) : a read-only list of #GError pointers, or %NULL if no error has occurred
 */
const GSList *
gda_data_model_dir_get_errors (GdaDataModelDir *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_DIR (model), NULL);
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);

	return priv->errors;
}

/**
 * gda_data_model_dir_clean_errors:
 * @model: a #GdaDataModelDir object
 *
 * Reset the list of errors which have occurred while using @model
 */
void
gda_data_model_dir_clean_errors (GdaDataModelDir *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_DIR (model));
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);

	if (priv->errors) {
		g_slist_free_full (priv->errors, (GDestroyNotify) g_error_free);
		priv->errors = NULL;
	}
}

static gint
gda_data_model_dir_get_n_rows (GdaDataModel *model)
{
	GdaDataModelDir *imodel = (GdaDataModelDir *) model;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_DIR (imodel), 0);
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (imodel);

	return priv->rows->len;
}

static gint
gda_data_model_dir_get_n_columns (GdaDataModel *model)
{
	GdaDataModelDir *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_DIR (model), 0);
	imodel = GDA_DATA_MODEL_DIR (model);
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (imodel);
	g_return_val_if_fail (priv, 0);

	return COL_LAST;
}

static GdaColumn *
gda_data_model_dir_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelDir *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_DIR (model), NULL);
	imodel = GDA_DATA_MODEL_DIR (model);
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (imodel);

	return g_slist_nth_data (priv->columns, col);
}

static GdaDataModelAccessFlags
gda_data_model_dir_get_access_flags (GdaDataModel *model)
{
	return GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD |
		GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD |
		GDA_DATA_MODEL_ACCESS_RANDOM |
		GDA_DATA_MODEL_ACCESS_WRITE;

}

static const GValue *
gda_data_model_dir_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaDataModelDir *imodel;
	GValue *value = NULL;
	FileRow *frow;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_DIR (model), NULL);
	g_return_val_if_fail (row >= 0, NULL);
	imodel = GDA_DATA_MODEL_DIR (model);
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (imodel);

	if ((col < 0) || (col > COL_LAST)) {
		gchar *tmp;
		tmp = g_strdup_printf (_("Column %d out of range (0-%d)"), col, COL_LAST-1);
		add_error (imodel, tmp);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
			      "%s", tmp);
		g_free (tmp);
		return NULL;
	}

	if ((guint)row >= priv->rows->len) {
		gchar *str;
		if (priv->rows->len > 0)
			str = g_strdup_printf (_("Row %d out of range (0-%d)"), row,
					       priv->rows->len - 1);
		else
			str = g_strdup_printf (_("Row %d not found (empty data model)"), row);
		add_error (imodel, str);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
			      "%s", str);
		g_free (str);
                return NULL;
        }

	frow =  g_ptr_array_index (priv->rows, row);
	if (frow) {
		switch (col) {
		case COL_DIRNAME: {
			gchar *tmp;
			tmp = compute_dirname (imodel, frow);
			if (!priv->tmp_value)
				priv->tmp_value = gda_value_new (G_TYPE_STRING);
			g_value_take_string (priv->tmp_value, tmp);
			value = priv->tmp_value;
			break;
		}
		case COL_FILENAME:
			value = frow->filename_value;
			break;
		case COL_SIZE:
			value = frow->size_value;
			break;
		case COL_MIME:
			if (! frow->mime_value) {
				gchar *filename = compute_filename (imodel, frow);
				update_file_mime (frow, filename);
				g_free (filename);
			}
			value = frow->mime_value;
			break;
		case COL_MD5SUM:
			if (! frow->md5sum_value) {
				gchar *filename = compute_filename (imodel, frow);
				update_file_md5sum (frow, filename);
				g_free (filename);
			}
			value = frow->md5sum_value;
			break;
		case COL_DATA:
			value = frow->data_value;
			if (! value) {
				value = gda_value_new (GDA_TYPE_BLOB);
				GdaBlob *blob;
				GdaBlobOp *op;
				gchar *filename;

				blob = gda_blob_new ();

				/* file mapping in mem */
				filename = compute_filename (imodel, frow);
				op = _gda_dir_blob_op_new (filename);
				g_free (filename);
				gda_blob_set_op (blob, op);
				g_object_unref (op);

				gda_value_take_blob (value, blob);
				frow->data_value = value;
			}
			break;
		default:
			break;
		}
	}
	else
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
			      "%s", _("Row not found"));

	return value;
}

static GdaValueAttribute
gda_data_model_dir_get_attributes_at (GdaDataModel *model, gint col, G_GNUC_UNUSED gint row)
{
	GdaDataModelDir *imodel;
	GdaValueAttribute flags = 0;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_DIR (model), 0);
	imodel = GDA_DATA_MODEL_DIR (model);
	if ((col < 0) || (col > COL_LAST)) {
		gchar *tmp;
		tmp = g_strdup_printf (_("Column %d out of range (0-%d)"), col, COL_LAST-1);
		add_error (imodel, tmp);
		g_free (tmp);
		return 0;
	}

	switch (col) {
	case COL_DIRNAME:
		flags = GDA_VALUE_ATTR_CAN_BE_NULL;
		break;
	case COL_FILENAME:
		break;
	case COL_SIZE:
	case COL_MIME:
	case COL_MD5SUM:
		flags = GDA_VALUE_ATTR_CAN_BE_NULL | GDA_VALUE_ATTR_NO_MODIF;
		break;
	case COL_DATA:
		flags = GDA_VALUE_ATTR_CAN_BE_NULL;
		break;
	default:
		flags = GDA_VALUE_ATTR_NO_MODIF;
		break;
	}

	return flags;
}

static gboolean
gda_data_model_dir_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *value, GError **error)
{
	GList *values = NULL;
	gint i;
	gboolean retval;
	GdaDataModelDir *imodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_DIR (model), FALSE);
	imodel = GDA_DATA_MODEL_DIR (model);

	if ((col < 0) || (col > COL_LAST)) {
		gchar *tmp;
		tmp = g_strdup_printf (_("Column %d out of range (0-%d)"), col, COL_LAST-1);
		add_error (imodel, tmp);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
			     "%s", tmp);
		g_free (tmp);
		return FALSE;
	}

	/* start padding with default values */
	for (i = 0; i < col; i++)
		values = g_list_append (values, NULL);

	values = g_list_append (values, (gpointer) value);

	/* add extra padding */
	for (i++; i < COL_LAST; i++)
		values = g_list_append (values, NULL);

	retval = gda_data_model_dir_set_values (model, row, values, error);
	g_list_free (values);

	return retval;
}


static gboolean
gda_data_model_dir_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	GdaDataModelDir *imodel;
	GList *list;
	gint col;
	FileRow *frow;
	gboolean has_changed = FALSE;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_DIR (model), FALSE);
	g_return_val_if_fail (row >= 0, FALSE);
	imodel = (GdaDataModelDir *) model;
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (imodel);
	if (!values)
		return TRUE;

	if ((guint)row >= priv->rows->len) {
		gchar *str;
		if (priv->rows->len > 0)
			str = g_strdup_printf (_("Row %d out of range (0-%d)"), row,
					       priv->rows->len - 1);
		else
			str = g_strdup_printf (_("Row %d not found (empty data model)"), row);
		add_error (imodel, str);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
			     "%s", str);
		g_free (str);
                return FALSE;
        }

	frow =  g_ptr_array_index (priv->rows, row);

	for (col = 0, list = values; list; list = list->next, col++) {
		GValue *value = (GValue *) list->data;
		const GValue *cvalue = gda_data_model_get_value_at (model, col, row, error);
		if (!cvalue)
			return FALSE;
		if (!value || !gda_value_compare (value, cvalue))
			continue;

		switch (col) {
		case COL_SIZE:
		case COL_MIME:
		case COL_MD5SUM:
		default:
			add_error (imodel, _("Column cannot be modified"));
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
				     "%s", _("Column cannot be modified"));
			return FALSE;
		case COL_DIRNAME: {
			/* check that the new dir still starts with the basedir */
			const gchar *new_path;
			gchar *old_path;
			gint len, base_len;

			new_path = value ? g_value_get_string (value) : "";
			len = strlen (new_path);
			base_len = strlen (priv->basedir);
			if ((len < base_len) ||
			    (strncmp (new_path, priv->basedir, base_len))) {
				add_error (imodel,
					   _("New path must be a subpath of the base directory"));
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
					     "%s",
					     _("New path must be a subpath of the base directory"));
				return FALSE;
			}

			old_path = compute_dirname (imodel, frow);
			if (dir_equal (new_path, old_path)) {
				g_free (old_path);
				g_print ("Paths are equal...\n");
				break;
			}

			if (!g_mkdir_with_parents (new_path, 0755)) {
				gchar *new_filename;
				GMappedFile *old_file;
				gboolean allok = FALSE;
				gchar *filename;

				new_filename = g_build_filename (new_path,
								 frow->raw_filename_value ? frow->raw_filename_value :
								 g_value_get_string (frow->filename_value), NULL);
				filename = compute_filename (imodel, frow);
				old_file = g_mapped_file_new (filename, FALSE, NULL);
				if (old_file) {
					if (g_file_set_contents (new_filename, g_mapped_file_get_contents (old_file),
								 g_mapped_file_get_length (old_file), NULL)) {
						g_unlink (filename);
						allok = TRUE;

						if (frow->data_value) {
							GdaBlob *blob;
							blob = (GdaBlob *) gda_value_get_blob (frow->data_value);
							if (blob && gda_blob_get_op (blob))
								_gda_dir_blob_set_filename (GDA_DIR_BLOB_OP (gda_blob_get_op (blob)),
											   new_filename);
						}
					}
					g_mapped_file_unref (old_file);
				}
				if (!allok) {
					gchar *str;
					str = g_strdup_printf (_("Could not rename file '%s' to '%s'"),
							       filename, new_filename);
					add_error (imodel, str);
					g_set_error (error,
						     GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
						     "%s", str);
					g_free (str);
					g_free (new_filename);
					g_free (filename);
					g_free (old_path);
					return FALSE;
				}
				else {
					/* renaming succeeded => update FileRow */
#ifndef G_OS_WIN32
					g_rmdir (old_path);
#endif
					g_free (frow->reldir);
					frow->reldir = g_strdup (new_path + base_len);
				}
				g_free (filename);
				g_free (new_filename);
				has_changed = TRUE;
			}
			else {
				gchar *str;
				str = g_strdup_printf (_("Could not create directory '%s'"), new_path);
				add_error (imodel, str);
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
					     "%s", str);
				g_free (str);
				g_free (old_path);
				return FALSE;
			}
			g_free (old_path);
			break;
		}
		case COL_FILENAME: {
			gchar *new_filename;
			gchar *filename;

			new_filename = g_build_filename (priv->basedir,
							 frow->reldir,
							 g_value_get_string (value), NULL);
			filename = compute_filename (imodel, frow);
			if (g_rename (filename, new_filename)) {
				gchar *str;
				str = g_strdup_printf (_("Could not rename file '%s' to '%s'"), filename, new_filename);
				add_error (imodel, str);
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
					     "%s", str);
				g_free (str);
				g_free (new_filename);
				g_free (filename);
				return FALSE;
			}
			else {
				/* renaming succeeded => update FileRow */
				gda_value_free (frow->filename_value);
				frow->filename_value = gda_value_copy (value);
				if (frow->raw_filename_value) {
					g_free (frow->raw_filename_value);
					frow->raw_filename_value = g_strdup (g_value_get_string (value));
				}
				if (frow->data_value) {
					GdaBlob *blob;
					blob = (GdaBlob *) gda_value_get_blob (frow->data_value);
					if (blob && gda_blob_get_op (blob))
						_gda_dir_blob_set_filename (GDA_DIR_BLOB_OP (gda_blob_get_op (blob)),
									   new_filename);
				}
			}
			g_free (new_filename);
			g_free (filename);
			has_changed = TRUE;
			break;
		}
		case COL_DATA: {
			GdaBlob *blob = NULL;
			if (gda_value_isa (value, GDA_TYPE_BLOB)) {
				blob = (GdaBlob *) gda_value_get_blob (value);
			}
			else if (gda_value_isa (value, GDA_TYPE_BINARY)) {
				blob = (GdaBlob *) gda_value_get_binary (value);
			}
			else if (gda_value_is_null (value)) {
				/* create a new empty blob */
				blob = gda_blob_new ();
			}

			if (blob) {
				GdaBlobOp *op;
				gchar *filename;
				filename = compute_filename (imodel, frow);
				op = _gda_dir_blob_op_new (filename);
				if (gda_blob_op_write_all (op, blob) < 0) {
					gchar *str;
					str = g_strdup_printf (_("Could not overwrite contents of file '%s'"), filename);
					add_error (imodel, str);
					g_set_error (error, GDA_DATA_MODEL_ERROR,
						     GDA_DATA_MODEL_ACCESS_ERROR,
						     "%s", str);
					g_free (str);
					g_object_unref (op);
					g_free (filename);
					return FALSE;
				}
				g_object_unref (op);
				if (gda_value_is_null (value))
					g_free (blob);
				has_changed = FALSE;
				has_changed = update_file_size (frow, filename);
				has_changed = update_file_md5sum (frow, filename) || has_changed;
				has_changed = update_file_mime (frow, filename) || has_changed;
				g_free (filename);
			}
			else {
				add_error (imodel, _("Wrong type of data"));
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
					     "%s", _("Wrong type of data"));
				return FALSE;
			}
			break;
		}
		}
	}

	if (has_changed)
		/* signal changes to data model */
		gda_data_model_row_updated ((GdaDataModel *) model, row);

	return TRUE;
}

static gint
gda_data_model_dir_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	GdaDataModelDir *imodel;
	const gchar *dirname = NULL, *filename = NULL;
	GdaBinary *bin_data = NULL;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_DIR (model), -1);
	imodel = (GdaDataModelDir *) model;
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (imodel);

	GList *list;
	gint col;

	if (!values)
		return -1;
	for (col = 0, list = (GList *) values; list; list = list->next, col++) {
		GValue *value = (GValue *) list->data;
		if (!value || gda_value_is_null (value))
			continue;

		switch (col) {
		case COL_SIZE:
		case COL_MIME:
		case COL_MD5SUM:
		default:
			add_error (imodel, _("Column cannot be set"));
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
				     "%s",
				     _("Column cannot be set"));
			return -1;
		case COL_DIRNAME:
			if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
				gint len, base_len;
				base_len = strlen (priv->basedir);
				dirname = g_value_get_string (value);
				len = strlen (dirname);
				if ((len < base_len) ||
				    (strncmp (dirname, priv->basedir, base_len))) {
					add_error (imodel, _("New path must be a subpath of the base directory"));
					g_set_error (error, GDA_DATA_MODEL_ERROR,
						     GDA_DATA_MODEL_ACCESS_ERROR,
						     "%s",
						     _("New path must be a subpath of the base directory"));
					return -1;
				}
			}
			break;
		case COL_FILENAME:
			if (G_VALUE_TYPE (value) == G_TYPE_STRING)
				filename = g_value_get_string (value);
			break;
		case COL_DATA:
			if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB)
				bin_data = (GdaBinary *) gda_value_get_blob (value);
			else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY)
				bin_data = (GdaBinary *) gda_value_get_binary (value);
			break;
		}
	}

	if (dirname && filename && *filename) {
		if (!g_mkdir_with_parents (dirname, 0755)) {
			gchar *complete_filename;
			gboolean bin_to_free = FALSE;
			complete_filename = g_build_filename (dirname, filename, NULL);
			if (!bin_data) {
				bin_data = gda_binary_new ();
				bin_to_free = TRUE;
			}
			if (g_file_set_contents (complete_filename, (gchar *) gda_binary_get_data (bin_data),
						 gda_binary_get_size (bin_data), NULL)) {
				FileRow *row;

				row = file_row_new ();
				row->reldir = g_strdup (dirname + strlen (priv->basedir));
#ifndef G_OS_WIN32
				row->raw_filename_value = g_strdup (filename);
#else
				row->raw_filename_value = NULL; /* no need top copy on Windows */
#endif
				g_value_set_string (row->filename_value = gda_value_new (G_TYPE_STRING),
						    filename);

				/* file size */
				update_file_size (row, complete_filename);

				/* other attributes, computed only when needed */
				row->mime_value = NULL;
				row->md5sum_value = NULL;
				row->data_value = NULL;
				if (bin_to_free)
					g_free (bin_data);
				g_ptr_array_add (priv->rows, row);
				gda_data_model_row_inserted (model, priv->rows->len - 1);
				return priv->rows->len - 1;
			}
			else {
#ifndef G_OS_WIN32
				g_rmdir (dirname);
#endif
				gchar *str;
				str = g_strdup_printf (_("Cannot set contents of filename '%s'"), complete_filename);
				add_error (imodel, str);
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
					     "%s", str);
				g_free (str);
				if (bin_to_free)
					g_free (bin_data);
				return -1;
			}
		}
		else {
			gchar *str;
			str = g_strdup_printf (_("Cannot create directory '%s'"), dirname);
			add_error (imodel, str);
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
				     "%s", str);
			g_free (str);
			return -1;
		}
	}
	else {
		add_error (imodel, _("Cannot add row: filename missing"));
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "%s", _("Cannot add row: filename missing"));
		return -1;
	}

	return -1;
}

static gboolean
gda_data_model_dir_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaDataModelDir *imodel;
	gchar *filename;
	FileRow *frow;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_DIR (model), FALSE);
	g_return_val_if_fail (row >=0, FALSE);
	imodel = (GdaDataModelDir *) model;
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (imodel);

	if ((guint)row >= priv->rows->len) {
		gchar *str;
		if (priv->rows->len > 0)
			str = g_strdup_printf (_("Row %d out of range (0-%d)"), row,
					       priv->rows->len - 1);
		else
			str = g_strdup_printf (_("Row %d not found (empty data model)"), row);
		add_error (imodel, str);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "%s", str);
		g_free (str);
                return FALSE;
        }

	frow =  g_ptr_array_index (priv->rows, row);

	/* remove filename */
	filename = g_build_filename (priv->basedir,
				     frow->reldir,
				     frow->raw_filename_value ? frow->raw_filename_value :
				     g_value_get_string (frow->filename_value), NULL);
	if (g_unlink (filename)) {
		gchar *str;
		str = g_strdup_printf (_("Cannot remove file '%s'"), filename);
		add_error (imodel, str);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "%s", str);
		g_free (str);
		g_free (filename);
		return FALSE;
	}
	g_free (filename);

	/* remove dir */
#ifndef G_OS_WIN32
	gchar *path;

	path = g_build_path (G_DIR_SEPARATOR_S, priv->basedir,
			     frow->reldir, NULL);
	g_rmdir (path);
	g_free (path);
#endif

	/* remove row from data model */
	file_row_free (frow);
	g_ptr_array_remove_index (priv->rows, row);
	gda_data_model_row_removed (model, row);

	return TRUE;
}

/*
 * Returns: TRUE if @path1 and @path2 relate in fact to the same dir
 */
static gboolean
dir_equal (const gchar *path1, const gchar *path2)
{
	g_assert (path1);
	g_assert (path2);
	const gchar *p1, *p2;
	for (p1 = path1, p2 = path2; *p1 && *p2; ) {
		if (*p1 != *p2)
			return FALSE;
		if (*p1 == G_DIR_SEPARATOR) {
			/* skip all the separators from p1 and p2 */
			while (*p1 == G_DIR_SEPARATOR)
				p1++;
			while (*p2 == G_DIR_SEPARATOR)
				p2++;
		}
		else {
			p1++;
			p2++;
		}
	}
	if (*p1) {
		while (*p1 == G_DIR_SEPARATOR)
				p1++;
	}
	if (*p2) {
		while (*p2 == G_DIR_SEPARATOR)
				p2++;
	}
	if (*p1 || *p2)
		return FALSE;
	else
		return TRUE;
}

static gchar *
compute_dirname (GdaDataModelDir *model, FileRow *row)
{
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);
	return g_build_path (G_DIR_SEPARATOR_S, priv->basedir, row->reldir, NULL);
}

static gchar *
compute_filename (GdaDataModelDir *model, FileRow *row)
{
	GdaDataModelDirPrivate *priv = gda_data_model_dir_get_instance_private (model);
	return g_build_filename (priv->basedir, row->reldir,
				 row->raw_filename_value ? row->raw_filename_value :
				 g_value_get_string (row->filename_value), NULL);
}

/*
 * FileRow implementation details
 */
static FileRow *
file_row_new (void)
{
	FileRow *fr;

	fr = g_new0 (FileRow, 1);
	return fr;
}

static void
file_row_clean (FileRow *row)
{
	if (!row)
		return;

	g_free (row->reldir);
	g_free (row->raw_filename_value);
	gda_value_free (row->filename_value);
	gda_value_free (row->size_value);
	if (row->mime_value)
		gda_value_free (row->mime_value);
	if (row->md5sum_value)
		gda_value_free (row->md5sum_value);
	if (row->data_value)
		gda_value_free (row->data_value);
}

static void
file_row_free (FileRow *row)
{
	if (!row)
		return;
	file_row_clean (row);
	g_free (row);
}
