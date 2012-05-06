/*
 * Copyright (C) 2006 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Brecht Sanders <brecht@edustria.be>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
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

#include "gda-data-model-import.h"
#include <unistd.h>
#ifndef G_OS_WIN32
  #include <sys/mman.h>
#else
  #include <windows.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

extern gchar *gda_lang_locale;

#include <glib/gi18n-lib.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-value.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-data-access-wrapper.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-data-model-iter-extra.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-set.h>
#include <libgda/gda-data-model-private.h> /* For gda_data_model_add_data_from_xml_node() */
#include <libgda/gda-util.h>
#include <libgda/gda-data-model-array.h>

#include <libxml/xmlreader.h>
#include "csv.h"

typedef enum {
	FORMAT_XML_DATA,
	FORMAT_CSV,
	FORMAT_XML_NODE
} InternalFormat;

typedef struct {
	gint                nb_cols;
	GdaDataModelImport *model;

	gint                field_next_col;
	GSList             *fields; /* list of GValue */
} CsvParserData;

struct _GdaDataModelImportPrivate {
	/* data access specific variables */
	gboolean             is_mapped; /* TRUE if a file has been mapped */
	union {
		/* mapped file */
		struct {
			gchar            *filename;
			int               fd;
			gpointer          start;
			size_t            length;
		} mapped;

		/* data as a string */
		gchar *string;
	} src;
	gchar               *data_start;
	guint                data_length;

	/* extraction format specific variables */
	InternalFormat       format;
	union {
		struct {
			struct csv_parser *parser;
			gchar            *encoding;
			gchar             delimiter;
			gchar             quote;

			gboolean          ignore_first_line;
			GArray           *rows_read;

			gchar            *start_pos;
			gboolean          initializing;
			guint             text_line; /* line number of the current last line */

			CsvParserData    *pdata;
		} csv;
		struct {
			xmlTextReaderPtr  reader;
		} xml;
		struct {
			xmlNodePtr        node;
		} node;
	} extract;
	GSList              *cursor_values; /* list of GValues for the current row
					     * (might be shorter than the number of columns in the data model)*/

	/* general data */
	gboolean             random_access;
	GSList              *columns;
	GdaDataModel        *random_access_model; /* data is imported into this model if random access is required */
	GSList              *errors; /* list of errors as GError structures */
	GdaSet              *options;

	gint                 iter_row;
	gboolean             init_done;
	gboolean             strict;
};

/* properties */
enum
{
        PROP_0,
	PROP_RANDOM_ACCESS,
        PROP_FILENAME,
	PROP_DATA_STRING,
	PROP_XML_NODE,
	PROP_OPTIONS,
	PROP_STRICT
};

#define CSV_TITLE_BUFFER_SIZE 255
#define CSV_DATA_BUFFER_SIZE 2048

static void gda_data_model_import_class_init (GdaDataModelImportClass *klass);
static void gda_data_model_import_init       (GdaDataModelImport *model,
					      GdaDataModelImportClass *klass);
static void gda_data_model_import_dispose    (GObject *object);
static void gda_data_model_import_finalize   (GObject *object);

static void gda_data_model_import_set_property (GObject *object,
						guint param_id,
						const GValue *value,
						GParamSpec *pspec);
static void gda_data_model_import_get_property (GObject *object,
						guint param_id,
						GValue *value,
						GParamSpec *pspec);

/* GdaDataModel interface */
static void                 gda_data_model_import_data_model_init (GdaDataModelIface *iface);
static gint                 gda_data_model_import_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_import_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_import_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_model_import_get_access_flags(GdaDataModel *model);
static const GValue        *gda_data_model_import_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);
static GdaValueAttribute    gda_data_model_import_get_attributes_at (GdaDataModel *model, gint col, gint row);
static GdaDataModelIter    *gda_data_model_import_create_iter      (GdaDataModel *model);
static gboolean             gda_data_model_import_iter_next       (GdaDataModel *model, GdaDataModelIter *iter);
static gboolean             gda_data_model_import_iter_prev       (GdaDataModel *model, GdaDataModelIter *iter);

static const gchar *find_option_as_string (GdaDataModelImport *model, const gchar *pname);
static gboolean     find_option_as_boolean (GdaDataModelImport *model, const gchar *pname, gboolean defaults);
static void         add_error (GdaDataModelImport *model, const gchar *err);

static GObjectClass *parent_class = NULL;

/**
 * gda_data_model_import_get_type:
 *
 * Returns: the #GType of GdaDataModelImport.
 */
GType
gda_data_model_import_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaDataModelImportClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_import_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelImport),
			0,
			(GInstanceInitFunc) gda_data_model_import_init,
			0
		};

		static const GInterfaceInfo data_model_info = {
			(GInterfaceInitFunc) gda_data_model_import_data_model_init,
			NULL,
			NULL
		};

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaDataModelImport", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_data_model_import_class_init (GdaDataModelImportClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* properties */
	object_class->set_property = gda_data_model_import_set_property;
        object_class->get_property = gda_data_model_import_get_property;
	/**
	 * GdaDataModelImport:random-access:
	 *
	 * Defines if the data model will be accessed randomly or through a cursor. If set to %FALSE,
	 * access will have to be done using a cursor.
	 */
	g_object_class_install_property (object_class, PROP_RANDOM_ACCESS,
                                         g_param_spec_boolean ("random-access", NULL, "Random access to the data model "
							       "is possible",
							       FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY));
	/**
	 * GdaDataModelImport:filename:
	 *
	 * Name of the file to import.
	 */
	g_object_class_install_property (object_class, PROP_FILENAME,
                                         g_param_spec_string ("filename", NULL, "File to import", NULL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
	/**
	 * GdaDataModelImport:data-string:
	 *
	 * Data to import, as a string.
	 */
	g_object_class_install_property (object_class, PROP_DATA_STRING,
                                         g_param_spec_string ("data-string", NULL, "String to import", NULL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
	/**
	 * GdaDataModelImport:xml-node:
	 *
	 * Data to import, as a pointer to an XML node (a #xmlNodePtr).
	 */
	g_object_class_install_property (object_class, PROP_XML_NODE,
                                         g_param_spec_pointer ("xml-node", NULL, "XML node to import from",
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
	/**
	 * GdaDataModelImport:options:
	 *
	 * Data model options.
	 */
	g_object_class_install_property (object_class, PROP_OPTIONS,
                                         g_param_spec_object ("options", NULL, "Options to configure the import",
                                                               GDA_TYPE_SET,
							       G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY));
	/**
	 * GdaDataModelImport:strict:
	 *
	 * Defines the behaviour in case the imported data contains recoverable errors (usually too
	 * many or too few data per row). If set to %TRUE, an error will be reported and the import
	 * will stop, and if set to %FALSE, then the error will be reported but the import will not stop.
	 *
	 * Since: 4.2.1
	 */
	g_object_class_install_property (object_class, PROP_STRICT,
                                         g_param_spec_boolean ("strict", NULL, "Consider missing or too much values an error",
							       FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT));

	/* virtual functions */
	object_class->dispose = gda_data_model_import_dispose;
	object_class->finalize = gda_data_model_import_finalize;
}

static void
gda_data_model_import_data_model_init (GdaDataModelIface *iface)
{
	iface->i_get_n_rows = gda_data_model_import_get_n_rows;
	iface->i_get_n_columns = gda_data_model_import_get_n_columns;
	iface->i_describe_column = gda_data_model_import_describe_column;
        iface->i_get_access_flags = gda_data_model_import_get_access_flags;
	iface->i_get_value_at = gda_data_model_import_get_value_at;
	iface->i_get_attributes_at = gda_data_model_import_get_attributes_at;

	iface->i_create_iter = gda_data_model_import_create_iter;
	iface->i_iter_at_row = NULL;
	iface->i_iter_next = gda_data_model_import_iter_next;
	iface->i_iter_prev = gda_data_model_import_iter_prev;

	iface->i_set_value_at = NULL;
	iface->i_iter_set_value = NULL;
	iface->i_set_values = NULL;
        iface->i_append_values = NULL;
	iface->i_append_row = NULL;
	iface->i_remove_row = NULL;
	iface->i_find_row = NULL;

	iface->i_set_notify = NULL;
	iface->i_get_notify = NULL;
	iface->i_send_hint = NULL;
}

static void
gda_data_model_import_init (GdaDataModelImport *model, G_GNUC_UNUSED GdaDataModelImportClass *klass)
{
	model->priv = g_new0 (GdaDataModelImportPrivate, 1);
	model->priv->random_access = FALSE; /* cursor mode is the default */
	model->priv->columns = NULL;
	model->priv->random_access_model = NULL;
	model->priv->errors = NULL;
	model->priv->cursor_values = NULL;

	model->priv->is_mapped = TRUE;
	model->priv->src.mapped.filename = NULL;
	model->priv->src.mapped.fd = -1;
	model->priv->src.mapped.start = NULL;

	model->priv->format = FORMAT_CSV;
	model->priv->data_start = NULL;
	model->priv->data_length = 0;

	model->priv->iter_row = -1;
	model->priv->init_done = FALSE;
	model->priv->strict = FALSE;
}

static void
csv_free_stored_rows (GdaDataModelImport *model)
{
	gsize i;
	g_assert (model->priv->format == FORMAT_CSV);
	for (i = 0; i < model->priv->extract.csv.rows_read->len; i++) {
		GSList *list = g_array_index (model->priv->extract.csv.rows_read,
					      GSList *, i);
		g_slist_foreach (list, (GFunc) gda_value_free, NULL);
		g_slist_free (list);
	}

	if (model->priv->extract.csv.pdata) {
		if (model->priv->extract.csv.pdata->fields) {
			g_slist_foreach (model->priv->extract.csv.pdata->fields, (GFunc) gda_value_free, NULL);
			g_slist_free (model->priv->extract.csv.pdata->fields);
		}
		g_free (model->priv->extract.csv.pdata);
	}

	g_array_free (model->priv->extract.csv.rows_read, FALSE);
	model->priv->extract.csv.rows_read = NULL;
}

static void
gda_data_model_import_dispose (GObject *object)
{
	GdaDataModelImport *model = (GdaDataModelImport *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_IMPORT (model));

	/* free memory */
	if (model->priv) {
		if (model->priv->options) {
			g_object_unref (model->priv->options);
			model->priv->options = NULL;
		}

		if (model->priv->columns) {
			g_slist_foreach (model->priv->columns, (GFunc) g_object_unref, NULL);
			g_slist_free (model->priv->columns);
			model->priv->columns = NULL;
		}

		/* data access mem free */
		if (model->priv->is_mapped) {
			if (model->priv->src.mapped.start) {
#ifndef G_OS_WIN32
				munmap (model->priv->src.mapped.start, model->priv->src.mapped.length);
#else
				UnmapViewOfFile (model->priv->src.mapped.start);
#endif
				model->priv->src.mapped.start = NULL;
			}

			g_free (model->priv->src.mapped.filename);
			if (model->priv->src.mapped.fd >= 0) {
				close (model->priv->src.mapped.fd);
				model->priv->src.mapped.fd = -1;
			}
		}
		else {
			g_free (model->priv->src.string);
			model->priv->src.string = NULL;
		}

		/* extraction data free */
		switch (model->priv->format) {
		case FORMAT_XML_DATA:
			if (model->priv->extract.xml.reader) {
				xmlFreeTextReader (model->priv->extract.xml.reader);
				model->priv->extract.xml.reader = NULL;
			}
			break;
		case FORMAT_CSV:
			if (model->priv->extract.csv.parser) {
				csv_fini (model->priv->extract.csv.parser, NULL, NULL, NULL);
				model->priv->extract.csv.parser = NULL;
			}
			if (model->priv->extract.csv.rows_read)
				csv_free_stored_rows (model);
			if (model->priv->extract.csv.encoding) {
				g_free (model->priv->extract.csv.encoding);
				model->priv->extract.csv.encoding = NULL;
			}
			break;
		case FORMAT_XML_NODE:
			break;
		default:
			g_assert_not_reached ();
			break;
		}

		/* random access model free */
		if (model->priv->random_access_model) {
			g_object_unref (model->priv->random_access_model);
			model->priv->random_access_model = NULL;
		}
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_data_model_import_finalize (GObject *object)
{
	GdaDataModelImport *model = (GdaDataModelImport *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_IMPORT (model));

	/* free memory */
	if (model->priv) {
		if (model->priv->errors) {
			g_slist_foreach (model->priv->errors, (GFunc) g_error_free, NULL);
			g_slist_free (model->priv->errors);
		}

		if (model->priv->cursor_values) {
			g_slist_foreach (model->priv->cursor_values, (GFunc) gda_value_free, NULL);
			g_slist_free (model->priv->cursor_values);
			model->priv->cursor_values = NULL;
		}

		g_free (model->priv);
		model->priv = NULL;
	}

	/* chain to parent class */
	parent_class->finalize (object);
}


static const gchar *
find_option_as_string (GdaDataModelImport *model, const gchar *pname)
{
	const GValue *value;

	value = gda_set_get_holder_value (model->priv->options, pname);
	if (value && !gda_value_is_null ((GValue *) value)) {
		if (!gda_value_isa ((GValue *) value, G_TYPE_STRING))
			g_warning (_("The '%s' option must hold a "
				     "string value, ignored."), pname);
		else
			return g_value_get_string ((GValue *) value);
	}
	return NULL;
}

static gboolean
find_option_as_boolean (GdaDataModelImport *model, const gchar *pname, gboolean defaults)
{
	const GValue *value;

	value = gda_set_get_holder_value (model->priv->options, pname);
	if (value && !gda_value_is_null ((GValue *) value)) {
		if (!gda_value_isa ((GValue *) value, G_TYPE_BOOLEAN))
			g_warning (_("The '%s' option must hold a "
				     "boolean value, ignored."), pname);
		else
			return g_value_get_boolean ((GValue *) value);
	}

	return defaults;
}

static void init_csv_import  (GdaDataModelImport *model);
static void init_xml_import  (GdaDataModelImport *model);
static void init_node_import (GdaDataModelImport *model);

static void
gda_data_model_import_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdaDataModelImport *model;
	const gchar *string;

	model = GDA_DATA_MODEL_IMPORT (object);
	if (model->priv) {
		switch (param_id) {
		case PROP_RANDOM_ACCESS:
			model->priv->random_access = g_value_get_boolean (value);
			if (model->priv->format == FORMAT_XML_NODE)
				model->priv->random_access = TRUE;
			return;
			break;
		case PROP_FILENAME:
			string = g_value_get_string (value);
			if (!string)
				return;
			model->priv->is_mapped = TRUE;
			model->priv->src.mapped.filename = g_strdup (g_value_get_string (value));

			/* file opening */
			model->priv->src.mapped.fd = open (model->priv->src.mapped.filename, O_RDONLY); /* Flawfinder: ignore */
			if (model->priv->src.mapped.fd < 0) {
				/* error */
				add_error (model, strerror(errno));
				return;
			}
			else {
				/* file mmaping */
				struct stat _stat;

				if (fstat (model->priv->src.mapped.fd, &_stat) < 0) {
					/* error */
					add_error (model, strerror(errno));
					return;
				}
				model->priv->src.mapped.length = _stat.st_size;
#ifndef G_OS_WIN32
				model->priv->src.mapped.start = mmap (NULL, model->priv->src.mapped.length,
								      PROT_READ, MAP_PRIVATE,
								      model->priv->src.mapped.fd, 0);
				if (model->priv->src.mapped.start == MAP_FAILED) {
					/* error */
					add_error (model, strerror(errno));
					model->priv->src.mapped.start = NULL;
					return;
				}
#else
				HANDLE hFile = CreateFile (model->priv->src.mapped.filename,
				                           GENERIC_READ,
										   FILE_SHARE_READ,
										   NULL,
										   OPEN_EXISTING,
										   FILE_ATTRIBUTE_NORMAL,
										   NULL);
				if (!hFile) {
						LPVOID lpMsgBuf;
						DWORD dw = GetLastError();
						FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
									   FORMAT_MESSAGE_FROM_SYSTEM |
									   FORMAT_MESSAGE_IGNORE_INSERTS,
									   NULL,
									   dw,
									   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
									   (LPTSTR) &lpMsgBuf,
									   0, NULL);
						add_error (model, (gchar *)lpMsgBuf);
						return;
				}
				HANDLE view = CreateFileMapping(hFile,
								NULL, PAGE_READONLY|SEC_COMMIT, 0,0 , NULL);
				if (!view) {
					/* error */
					LPVOID lpMsgBuf;
					DWORD dw = GetLastError();
					FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
					               FORMAT_MESSAGE_FROM_SYSTEM |
								   FORMAT_MESSAGE_IGNORE_INSERTS,
								   NULL,
								   dw,
								   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
								   (LPTSTR) &lpMsgBuf,
								   0, NULL);
					add_error (model, (gchar *)lpMsgBuf);
					return;
				}
				model->priv->src.mapped.start = MapViewOfFile(view, FILE_MAP_READ, 0, 0,
									      model->priv->src.mapped.length);
				if (!model->priv->src.mapped.start) {
					/* error */
					LPVOID lpMsgBuf;
					DWORD dw = GetLastError();
					FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
					               FORMAT_MESSAGE_FROM_SYSTEM |
								   FORMAT_MESSAGE_IGNORE_INSERTS,
								   NULL,
								   dw,
								   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
								   (LPTSTR) &lpMsgBuf,
								   0, NULL);
					add_error (model, (gchar *)lpMsgBuf);
					return;
				}
#endif
				model->priv->data_start = model->priv->src.mapped.start;
				model->priv->data_length = model->priv->src.mapped.length;
			}
			break;
		case PROP_DATA_STRING:
			string = g_value_get_string (value);
			if (!string)
				return;
			model->priv->is_mapped = FALSE;
			model->priv->src.string = g_strdup (g_value_get_string (value));
			model->priv->data_start = model->priv->src.string;
			model->priv->data_length = strlen (model->priv->src.string);
			break;
		case PROP_XML_NODE: {
			gpointer data = g_value_get_pointer (value);
			if (!data)
				return;
			model->priv->format = FORMAT_XML_NODE;
			model->priv->extract.node.node = data;
			break;
                }
		case PROP_OPTIONS:
                        if (model->priv->options)
                          g_object_unref(model->priv->options);

			model->priv->options = g_value_get_object (value);
			if (model->priv->options) {
				if (!GDA_IS_SET (model->priv->options)) {
					g_warning (_("\"options\" property is not a GdaSet object"));
					model->priv->options = NULL;
				}
				else
					g_object_ref (model->priv->options);
			}
			return;
		case PROP_STRICT:
			model->priv->strict = g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}

	/* here we now have a valid data to analyze, try to determine the real kind of data
	 * (CVS text of XML) */
	if (model->priv->format != FORMAT_XML_NODE
	    && model->priv->data_start) {
		if (!strncmp (model->priv->data_start, "<?xml", 5))
			model->priv->format = FORMAT_XML_DATA;
		else
			model->priv->format = FORMAT_CSV;
	}

	/* analyze common options and init.
	 * WARNING when adding properties: we need to avoid double initialization here... */
	if (! model->priv->init_done) {
		model->priv->init_done = TRUE;
		switch (model->priv->format) {
		case FORMAT_XML_DATA:
			init_xml_import (model);
			break;

		case FORMAT_CSV:
			model->priv->extract.csv.quote = '"';
			if (model->priv->options) {
				const gchar *option;
				option = find_option_as_string (model, "ENCODING");
				if (option)
					model->priv->extract.csv.encoding = g_strdup (option);
				option = find_option_as_string (model, "SEPARATOR");
				if (option)
					model->priv->extract.csv.delimiter = *option;
				model->priv->extract.csv.quote = '"';
				option = find_option_as_string (model, "QUOTE");
				if (option)
					model->priv->extract.csv.quote = *option;
			}
			init_csv_import (model);
			break;

		case FORMAT_XML_NODE:
			model->priv->random_access = TRUE;
			init_node_import (model);
			break;
		default:
			g_assert_not_reached ();
		}

		/* for random access, create a new GdaDataModelArray model and copy the contents
		   from this model */
		if (model->priv->random_access && model->priv->columns && !model->priv->random_access_model) {
			GdaDataModel *ramodel;

			ramodel = gda_data_access_wrapper_new ((GdaDataModel *) model);
			model->priv->random_access_model = ramodel;
		}
	}
}

static void
gda_data_model_import_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	GdaDataModelImport *model;

	model = GDA_DATA_MODEL_IMPORT (object);
	if (model->priv) {
		switch (param_id) {
		case PROP_RANDOM_ACCESS:
			g_value_set_boolean (value, model->priv->random_access);
			break;
		case PROP_FILENAME:
			if (model->priv->is_mapped)
				g_value_set_string (value, model->priv->src.mapped.filename);
			else
				g_value_set_string (value, NULL);
			break;
		case PROP_DATA_STRING:
			if (model->priv->is_mapped)
				g_value_set_string (value, NULL);
			else
				g_value_set_string (value, model->priv->src.string);
			break;
		case PROP_STRICT:
			g_value_set_boolean (value, model->priv->strict);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * gda_data_model_import_new_file:
 * @filename: the file to import data from
 * @random_access: TRUE if random access will be required
 * @options: (transfer none) (allow-none): importing options
 *
 * Creates a new #GdaDataModel object which contains the data stored within the @filename file.
 *
 * The options are the following ones:
 * <itemizedlist>
 *   <listitem><para>For the CSV format:
 *      <itemizedlist>
 *         <listitem><para>ENCODING (string): specifies the encoding of the data in the file</para></listitem>
 *         <listitem><para>SEPARATOR (string): specifies the CSV separator (comma as default)</para></listitem>
 *         <listitem><para>QUOTE (string): specifies the character used as quote (double quote as default)</para></listitem>
 *         <listitem><para>NAMES_ON_FIRST_LINE (boolean): consider that the first line of the file contains columns' titles (note that the TITLE_AS_FIRST_LINE option is also accepted as a synonym)</para></listitem>
 *         <listitem><para>G_TYPE_&lt;column number&gt; (GType): specifies the type of value expected in column &lt;column number&gt;</para></listitem>
 *      </itemizedlist>
 *   </para></listitem>
 *   <listitem><para>Other formats: no option</para></listitem>
 * </itemizedlist>
 *
 * Note: after the creation, please use gda_data_model_import_get_errors() to check any error.
 *
 * Returns: (transfer full): a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_import_new_file   (const gchar *filename, gboolean random_access, GdaSet *options)
{
	GdaDataModelImport *model;

	g_return_val_if_fail (filename, NULL);

	model = g_object_new (GDA_TYPE_DATA_MODEL_IMPORT,
			      "random-access", random_access,
			      "options", options,
			      "filename", filename, NULL);

	return GDA_DATA_MODEL (model);
}

/**
 * gda_data_model_import_new_mem:
 * @data: a string containing the data to import
 * @random_access: TRUE if random access will be required
 * @options: (transfer none) (allow-none): importing options, see gda_data_model_import_new_file() for more information
 *
 * Creates a new #GdaDataModel object which contains the data stored in the @data string.
 *
 * Important note: the @data string is not copied for memory efficiency reasons and should not
 * therefore be altered in any way as long as the returned data model exists.
 *
 * Returns: (transfer full): a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_import_new_mem (const gchar *data, gboolean random_access, GdaSet *options)
{
	GdaDataModelImport *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_IMPORT,
			      "random-access", random_access,
			      "options", options,
			      "data-string", data, NULL);

	return GDA_DATA_MODEL (model);
}

/**
 * gda_data_model_import_new_xml_node:
 * @node: (transfer none): an XML node corresponding to a &lt;data-array&gt; tag
 *
 * Creates a new #GdaDataModel and loads the data in @node. The resulting data model
 * can be accessed in a random way.
 *
 * Returns: (transfer full): a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_import_new_xml_node (xmlNodePtr node)
{
	GdaDataModelImport *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_IMPORT,
			      "xml-node", node, NULL);

	return GDA_DATA_MODEL (model);
}


/*
 *
 * CSV treatments
 *
 */

static gboolean csv_init_csv_parser (GdaDataModelImport *model);
static gboolean csv_fetch_some_lines (GdaDataModelImport *model);

static void
init_csv_import (GdaDataModelImport *model)
{
	gboolean title_first_line = FALSE;
	gint nbcols;

	if (model->priv->options)
		title_first_line = find_option_as_boolean (model, "NAMES_ON_FIRST_LINE", FALSE) ||
			find_option_as_boolean (model, "TITLE_AS_FIRST_LINE", FALSE);

	g_assert (model->priv->format == FORMAT_CSV);

	if (!model->priv->extract.csv.delimiter)
		model->priv->extract.csv.delimiter = ',';

	model->priv->extract.csv.ignore_first_line = FALSE;
	model->priv->extract.csv.start_pos = model->priv->data_start;
	model->priv->extract.csv.text_line = 1; /* start line numbering at 1 */
	model->priv->extract.csv.rows_read = g_array_new (FALSE, TRUE, sizeof (GSList *));

	/* parser init */
	if (!csv_init_csv_parser (model))
		return;

	/* set parser parameters */
	csv_set_delim (model->priv->extract.csv.parser, model->priv->extract.csv.delimiter);
	csv_set_quote (model->priv->extract.csv.parser, model->priv->extract.csv.quote);

	/* fill in at least a row to determine the number of columns */
	model->priv->extract.csv.initializing = TRUE;
	csv_fetch_some_lines (model);
	model->priv->extract.csv.initializing = FALSE;

	/* computing columns */
	if (model->priv->extract.csv.rows_read->len == 0)
		return;

	GSList *row;
	gint col;
	const GValue *cvalue;

	row = g_array_index (model->priv->extract.csv.rows_read, GSList *, 0);
	g_assert (row);
	nbcols = g_slist_length (row);

	for (col = 0; col < nbcols; col++) {
		GdaColumn *column;
		gchar *str = NULL;

		column = gda_column_new ();
		model->priv->columns = g_slist_append (model->priv->columns,
						       column);
		if (title_first_line) {
			cvalue = g_slist_nth_data (row, col);
			if (cvalue && !gda_value_is_null (cvalue))
				str = gda_value_stringify (cvalue);
		}
		if (!str)
			str = g_strdup_printf ("column_%d", col);
		gda_column_set_name (column, str);
		gda_column_set_description (column, str);
		g_free (str);

		gda_column_set_g_type (column, G_TYPE_STRING);
		if (model->priv->options) {
			gchar *pname;
			const GValue *value;

			pname = g_strdup_printf ("G_TYPE_%d", col);
			value = gda_set_get_holder_value (model->priv->options, pname);
			if (value && !gda_value_is_null ((GValue *) value)) {
				if (!gda_value_isa ((GValue *) value, G_TYPE_GTYPE))
					g_warning (_("The '%s' option must hold a "
						     "GType value, ignored."), pname);
				else {
					GType gtype;

					gtype = g_value_get_gtype ((GValue *) value);
					gda_column_set_g_type (column, gtype);
				}
			}
			g_free (pname);
		}
	}

	/* reset */
	/*g_print ("CSV parser RESET...................................................\n");*/
	csv_free_stored_rows (model);
	csv_fini (model->priv->extract.csv.parser, NULL, NULL, NULL);
	csv_init_csv_parser (model);

	model->priv->extract.csv.start_pos = model->priv->data_start;
	model->priv->extract.csv.text_line = 1; /* start line numbering at 1 */
	model->priv->extract.csv.rows_read = g_array_new (FALSE, TRUE, sizeof (GSList *));
	if (title_first_line)
		model->priv->extract.csv.ignore_first_line = TRUE;
	csv_fetch_some_lines (model);
}

static gboolean
csv_init_csv_parser (GdaDataModelImport *model)
{
	if (csv_init (&(model->priv->extract.csv.parser), 0) != 0) {
		model->priv->extract.csv.parser = NULL;
		return FALSE;
	}

	model->priv->extract.csv.pdata = g_new0 (CsvParserData, 1);
	model->priv->extract.csv.pdata->nb_cols = gda_data_model_get_n_columns ((GdaDataModel*) model);
	model->priv->extract.csv.pdata->model = model;
	model->priv->extract.csv.pdata->field_next_col = 0;
	model->priv->extract.csv.pdata->fields = NULL;

	csv_set_delim (model->priv->extract.csv.parser, model->priv->extract.csv.delimiter);
	csv_set_quote (model->priv->extract.csv.parser, model->priv->extract.csv.quote);
	return TRUE;
}

static void
csv_parser_field_read_cb (char *s, size_t len, void *data)
{
	CsvParserData *pdata = (CsvParserData* ) data;
	GdaDataModelImport *model = pdata->model;
	GValue *value = NULL;
	GdaColumn *column;
	GType type = GDA_TYPE_NULL;
	gchar *copy;

	if (pdata->model->priv->extract.csv.ignore_first_line)
		return;

	/* convert to correct encoding */
	if (model->priv->extract.csv.encoding) {
		GError *error = NULL;
		copy = g_convert (s, len, "UTF-8", model->priv->extract.csv.encoding,
				  NULL, NULL, &error);
		if (!copy) {
			gchar *str;
			str = g_strdup_printf (_("Character conversion at line %d, error: %s"),
					       model->priv->extract.csv.text_line,
					       error && error->message ? error->message:
					       _("no detail"));
			add_error (model, str);
			g_free (str);
			g_error_free (error);
		}
	}
	else
		copy = g_locale_to_utf8 (s, len, NULL, NULL, NULL);
	if (!copy)
		copy = g_strndup (s, len);
	/*g_print ("FIELD: #%s# ", copy);*/

	/* compute column's type */
	if (! model->priv->extract.csv.initializing) {
		if (pdata->field_next_col >= pdata->nb_cols)
			/* ignore extra fields */
			return;
		column = gda_data_model_describe_column ((GdaDataModel *) model,
							 pdata->field_next_col);
		pdata->field_next_col++;

		if (!column) {
			g_free (copy);
			return;
		}
		type = gda_column_get_g_type (column);
	}
	else
		type = G_TYPE_STRING;

	/* create a GValue */
	if (type != GDA_TYPE_BINARY) {
		if (! g_ascii_strcasecmp (copy, "NULL"))
			value = gda_value_new_null ();
		else {
			value = gda_value_new_from_string (copy, type);
			if (!value) {
				gchar *str;
				str = g_strdup_printf (_("Could not convert string '%s' to a '%s' value"), copy,
						       g_type_name (type));
				add_error (model, str);
				g_free (str);
			}
		}
	}
	else
		value = gda_value_new_binary ((guchar*) s, len);
	g_free (copy);
	pdata->fields = g_slist_prepend (pdata->fields, value);
	pdata->nb_cols ++;
	/*g_print ("=> %p (cols so far: %d)\n", value, g_slist_length (pdata->fields));*/
}

static void
csv_parser_row_read_cb (G_GNUC_UNUSED char c, void *data)
{
	CsvParserData *pdata = (CsvParserData* ) data;
	GSList *row;
	gint size;

	if (pdata->model->priv->extract.csv.ignore_first_line)
		pdata->model->priv->extract.csv.ignore_first_line = FALSE;
	else {
		row = g_slist_reverse (pdata->fields);
		pdata->fields = NULL;
		pdata->field_next_col = 0;

		size = g_slist_length (row);
		g_assert (size <= pdata->nb_cols);
		/*g_print ("===========ROW %d (%d cols)===========\n", pdata->model->priv->extract.csv.text_line, size);*/

		g_array_append_val (pdata->model->priv->extract.csv.rows_read, row);
		pdata->model->priv->extract.csv.text_line ++;
	}
}

static gboolean
csv_fetch_some_lines (GdaDataModelImport *model)
{
	size_t size;

	if (!model->priv->extract.csv.initializing)
		size = MIN (CSV_TITLE_BUFFER_SIZE,
			    model->priv->data_start + model->priv->data_length -
			    model->priv->extract.csv.start_pos);
	else
		size = MIN (CSV_DATA_BUFFER_SIZE,
			    model->priv->data_start + model->priv->data_length -
			    model->priv->extract.csv.start_pos);

	if (csv_parse (model->priv->extract.csv.parser,
		       model->priv->extract.csv.start_pos, size,
		       csv_parser_field_read_cb,
		       csv_parser_row_read_cb, model->priv->extract.csv.pdata) != size) {
		gchar *str = g_strdup_printf (_("Error while parsing CSV file: %s"),
					      csv_strerror (csv_error (model->priv->extract.csv.parser)));
		add_error (model, str);
		g_free (str);
		model->priv->extract.csv.start_pos = model->priv->data_start + model->priv->data_length;
		return FALSE;
	}
	else {
		model->priv->extract.csv.start_pos += size;

		/* try to finish the line if it has not been read entirely */
		if (model->priv->extract.csv.rows_read->len == 0) {
			if ((model->priv->extract.csv.start_pos !=
			     model->priv->data_start + model->priv->data_length))
				return csv_fetch_some_lines (model);
			else {
				/* end of data */
				csv_fini (model->priv->extract.csv.parser,
					  csv_parser_field_read_cb,
					  csv_parser_row_read_cb, model->priv->extract.csv.pdata);
				return TRUE;
			}
		}
		else
			return TRUE;
	}
}


/*
 *
 * XML treatments
 *
 */

static gint xml_fetch_next_xml_node (xmlTextReaderPtr reader);
static void xml_fetch_next_row (GdaDataModelImport *model);

typedef struct {
	xmlChar      *id;
	xmlChar      *name;
	xmlChar      *title;
	xmlChar      *caption;
	xmlChar      *dbms_type;
	GType         gdatype;
	gint          size;
	gint          scale;
	gboolean      pkey;
	gboolean      unique;
	gboolean      nullok;
	gboolean      autoinc;
	xmlChar      *table;
	xmlChar      *ref;
} XmlColumnSpec;

static void
clean_field_specs (GSList *fields)
{
	GSList *list;
	XmlColumnSpec *spec;

	for (list = fields; list; list = list->next) {
		spec = (XmlColumnSpec*)(list->data);
		xmlFree (spec->id);
		xmlFree (spec->name);
		xmlFree (spec->title);
		xmlFree (spec->caption);
		xmlFree (spec->dbms_type);
		xmlFree (spec->table);
		xmlFree (spec->ref);
		xmlFree (spec);
	}
	g_slist_free (fields);
}

static void
init_xml_import (GdaDataModelImport *model)
{
	int ret;
	xmlTextReaderPtr reader;

	g_assert (model->priv->format == FORMAT_XML_DATA);

	/* init extraction specific variables */
	reader = xmlReaderForMemory (model->priv->data_start,
				     model->priv->data_length,
				     NULL, NULL, 0);
	model->priv->extract.xml.reader = reader;

	if (! reader) {
		/* error, try to switch to csv */
		model->priv->format = FORMAT_CSV;
		init_csv_import (model);
		return;
	}

	/* create columns */
	ret = xmlTextReaderRead (reader);
	if (ret < 0) {
		/* error */
		add_error (model, _("Failed to read node in XML file"));
		xmlFreeTextReader (reader);
		model->priv->extract.xml.reader = NULL;
	}
	else {
		const xmlChar *name;
		xmlNodePtr node;
		gchar *prop;
		GSList *list, *fields = NULL;
		gint pos, nbfields = 0;

		if (ret == 0)
			return;

		name = xmlTextReaderConstName (reader);
		if (strcmp ((gchar*)name, "gda_array")) {
			/* error */
			gchar *str;

			str = g_strdup_printf (_("Expected <gda_array> node in XML file, got <%s>"), name);
			add_error (model, str);
			g_free (str);
			xmlFreeTextReader (reader);
			model->priv->extract.xml.reader = NULL;
			return;
		}
		node = xmlTextReaderCurrentNode (reader);

		prop = (gchar*)xmlGetProp (node, (xmlChar*)"id");
		if (prop)
			g_object_set_data_full (G_OBJECT (model), "id", prop, xmlFree);
		prop = (gchar*)xmlGetProp (node, (xmlChar*)"name");
		if (prop)
			g_object_set_data_full (G_OBJECT (model), "name", prop, xmlFree);

		prop = (gchar*)xmlGetProp (node, (xmlChar*)"descr");
		if (prop)
			g_object_set_data_full (G_OBJECT (model), "descr", prop, xmlFree);

		/* compute fields */
		ret = xml_fetch_next_xml_node (reader);
		name = (ret > 0) ? xmlTextReaderConstName (reader) : NULL;
		while (name && !strcmp ((char*)name, "gda_array_field")) {
			XmlColumnSpec *spec;
			gchar *str;

			spec = g_new0 (XmlColumnSpec, 1);
			fields = g_slist_append (fields, spec);

			spec->id = xmlTextReaderGetAttribute (reader, (xmlChar*)"id");
			spec->name = xmlTextReaderGetAttribute (reader, (xmlChar*)"name");
			spec->title = xmlTextReaderGetAttribute (reader, (xmlChar*)"title");
			if (!spec->title && spec->name)
				spec->title = xmlStrdup (spec->name);

			spec->caption = xmlTextReaderGetAttribute (reader, (xmlChar*)"caption");
			spec->dbms_type = xmlTextReaderGetAttribute (reader, (xmlChar*)"dbms_type");
			str = (gchar*)xmlTextReaderGetAttribute (reader, (xmlChar*)"gdatype");
			if (str) {
				spec->gdatype = gda_g_type_from_string (str);
				xmlFree (str);
				if (spec->gdatype == G_TYPE_INVALID)
					spec->gdatype = GDA_TYPE_NULL;
			}
			else {
				add_error (model, _("No \"gdatype\" attribute specified in <gda_array_field>"));
				clean_field_specs (fields);
				xmlFreeTextReader (reader);
				model->priv->extract.xml.reader = NULL;
				return;
			}
			str = (gchar*)xmlTextReaderGetAttribute (reader, (xmlChar*)"size");
			if (str) {
				spec->size = atoi (str); /* Flawfinder: ignore */
				if (spec->size < 0)
					spec->size = 0;
				xmlFree (str);
			}
			str = (gchar*)xmlTextReaderGetAttribute (reader, (xmlChar*)"scale");
			if (str) {
				spec->scale = atoi (str); /* Flawfinder: ignore */
				if (spec->scale < 0)
					spec->scale = 0;
				xmlFree (str);
			}
			str = (gchar*)xmlTextReaderGetAttribute (reader, (xmlChar*)"pkey");
			if (str) {
				spec->pkey = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = (gchar*)xmlTextReaderGetAttribute (reader, (xmlChar*)"unique");
			if (str) {
				spec->unique = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = (gchar*)xmlTextReaderGetAttribute (reader, (xmlChar*)"nullok");
			spec->nullok = FALSE;
			if (str) {
				spec->nullok = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = (gchar*)xmlTextReaderGetAttribute (reader, (xmlChar*)"auto_increment");
			if (str) {
				spec->autoinc = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			spec->table = xmlTextReaderGetAttribute (reader, (xmlChar*)"table");
			spec->ref = xmlTextReaderGetAttribute (reader, (xmlChar*)"ref");

			nbfields ++;

			ret = xml_fetch_next_xml_node (reader);
			name = (ret > 0) ? xmlTextReaderConstName (reader) : NULL;
		}

		if (nbfields == 0) {
			add_error (model, _("Expected <gda_array_field> in <gda_array>"));
			clean_field_specs (fields);
			xmlFreeTextReader (reader);
			model->priv->extract.xml.reader = NULL;
			return;
		}

		for (list = fields, pos = 0;
		     list;
		     list = list->next, pos++) {
			GdaColumn *column;
			XmlColumnSpec *spec;

			spec = (XmlColumnSpec *)(list->data);
			column = gda_column_new ();
			model->priv->columns = g_slist_append (model->priv->columns, column);
			g_object_set (G_OBJECT (column), "id", spec->id, NULL);
			gda_column_set_description (column, (gchar*)spec->title);
			gda_column_set_name (column, (gchar*)spec->name);
			gda_column_set_dbms_type (column, (gchar*)spec->dbms_type);
			gda_column_set_g_type (column, spec->gdatype);
			gda_column_set_allow_null (column, spec->nullok);
		}
		clean_field_specs (fields);

		/* move to find the <gda_array_data> tag */
		name = (ret > 0) ? xmlTextReaderConstName (reader) : NULL;
		if (!name || strcmp ((gchar*)name, "gda_array_data")) {
			gchar *str;

			if (name)
				str = g_strdup_printf (_("Expected <gda_array_data> in <gda_array>, got <%s>"), name);
			else
				str = g_strdup (_("Expected <gda_array_data> in <gda_array>"));
			add_error (model, str);
			g_free (str);

			xmlFreeTextReader (reader);
			model->priv->extract.xml.reader = NULL;
			return;
		}

		/* get ready for the data reading */
		ret = xml_fetch_next_xml_node (reader);
		if (ret <= 0) {
			add_error (model, _("Can't read the contents of the <gda_array_data> node"));
			xmlFreeTextReader (reader);
			model->priv->extract.xml.reader = NULL;
			return;
		}
	}
}

static gint
xml_fetch_next_xml_node (xmlTextReaderPtr reader)
{
	gint ret;

	ret = xmlTextReaderRead (reader);
	while ((ret > 0) && (xmlTextReaderNodeType (reader) != XML_ELEMENT_NODE)) {
		ret = xmlTextReaderRead (reader);
	}

	return ret;
}

static void
xml_fetch_next_row (GdaDataModelImport *model)
{
	xmlTextReaderPtr reader;
	const xmlChar *name;
	gint ret;

	const gchar *lang = gda_lang_locale;

	GSList *columns = model->priv->columns;
	GdaColumn *last_column = NULL;
	GSList *values = NULL;

	if (model->priv->cursor_values) {
		g_slist_foreach (model->priv->cursor_values, (GFunc) gda_value_free, NULL);
		g_slist_free (model->priv->cursor_values);
		model->priv->cursor_values = NULL;
	}

	reader = model->priv->extract.xml.reader;
	if (!reader)
		return;

	/* we must now have a <gda_array_row> tag */
	name = xmlTextReaderConstName (reader);
	if (!name || strcmp ((gchar*)name, "gda_array_row")) {
		gchar *str;

		str = g_strdup_printf (_("Expected <gda_array_row> in <gda_array_data>, got <%s>"), name);
		add_error (model, str);
		g_free (str);
		xmlFreeTextReader (reader);
		model->priv->extract.xml.reader = NULL;
		return;
	}

	/* compute values */
	ret = xml_fetch_next_xml_node (reader);
	name = (ret > 0) ? xmlTextReaderConstName (reader) : NULL;
	while (name && !strcmp ((gchar*)name, "gda_value")) {
		/* ignore this <gda_value> if the "lang" is there and is not the user locale */
		xmlChar *this_lang;
		this_lang = xmlTextReaderGetAttribute (reader, (xmlChar*)"xml:lang");
		if (this_lang && strncmp ((gchar*)this_lang, (gchar*)lang, strlen ((gchar*)this_lang))) {
			xmlFree (this_lang);
			ret = xml_fetch_next_xml_node (reader);
			name = (ret > 0) ? xmlTextReaderConstName (reader) : NULL;
			continue;
		}

		/* use this <gda_value> */
		if (!columns)
			add_error (model, _("Row has too many values (which are ignored)"));
		else {
			gboolean value_is_null = FALSE;
			xmlChar *isnull;
			GdaColumn *column;

			isnull = xmlTextReaderGetAttribute (reader, (xmlChar*)"isnull");
			if (isnull) {
				if ((*isnull == 't') || (*isnull == 'T'))
					value_is_null = TRUE;
				xmlFree (isnull);
			}

			if (this_lang)
				column = last_column;
			else {
				column = (GdaColumn *) columns->data;
				last_column = column;
				columns = g_slist_next (columns);
			}

			if (value_is_null) {
				values = g_slist_prepend (values, gda_value_new_null ());
				ret = -1;
			}
			else {
				ret = xmlTextReaderRead (reader);
				if (ret > 0) {
					GValue *value;
					GType gtype;

					ret = -1;

					gtype = gda_column_get_g_type (column);
					/*g_print ("TYPE: %s\n", xmlTextReaderConstName (reader));*/
					if (xmlTextReaderNodeType (reader) == XML_TEXT_NODE) {
						const xmlChar *xmlstr;

						xmlstr = xmlTextReaderConstValue (reader);
						value = gda_value_new_from_string ((gchar *) xmlstr, gtype);
						/*g_print ("Convert #%s# (type:%s) => %s (type:%s)\n", (gchar *) xmlstr,
							 gda_g_type_to_string (gtype),
							 gda_value_stringify (value),
							 value ? gda_g_type_to_string (G_VALUE_TYPE (value)) : "no type");*/
						if (!value) {
							gchar *str;

							str = g_strdup_printf (_("Could not convert '%s' to a value of type %s"),
									       (gchar *) xmlstr, gda_g_type_to_string (gtype));
							add_error (model, str);
							g_free (str);
							value = gda_value_new_null ();
						}
					}
					else {
						if (xmlTextReaderNodeType (reader) == XML_ELEMENT_NODE)
							ret = 1; /* don't read another node in the next loop,
								    use the current one */
						if (value_is_null)
							value = gda_value_new_null ();
						else
							value = gda_value_new_from_string ("", gtype);
					}

					if (this_lang) {
						/* replace the last value (which did not have any "lang" attribute */
						gda_value_free ((GValue *) values->data);
						values->data = value;
						xmlFree (this_lang);
					}
					else
						values = g_slist_prepend (values, value);
				}
			}
		}
		if (ret < 0) /* read another node ? */
			ret = xml_fetch_next_xml_node (reader);
		name = (ret > 0) ? xmlTextReaderConstName (reader) : NULL;
	}

	if (values)
		model->priv->cursor_values = g_slist_reverse (values);
#ifdef GDA_DEBUG_NO
	GSList *l;
	gint c;

	g_print ("======== GdaDataModelImport => next XML row ========\n");
	for (c = 0, l = model->priv->cursor_values; l; l = l->next, c++) {
		GValue *val = (GValue*)(l->data);
		g_print ("#%d %s (type:%s)\n", c, gda_value_stringify (val),
			 val ? g_type_name (G_VALUE_TYPE (val)) : "no type");
	}
#endif

	if (ret <= 0) {
		/* destroy the reader, nothing to read anymore */
		xmlFreeTextReader (reader);
		model->priv->extract.xml.reader = NULL;
	}
}

static void
init_node_import (GdaDataModelImport *model)
{
	GdaDataModel *ramodel;
	xmlNodePtr node, cur;
	gint nbfields = 0;
	GSList *fields = NULL;
	GSList *list;
	gint pos;
	gchar *str;
	GError *error = NULL;

	node = model->priv->extract.node.node;
	if (!node)
		return;

	if (strcmp ((gchar*)node->name, "gda_array")) {
		gchar *str;

		str = g_strdup_printf (_("Expected <gda_array> node but got <%s>"), node->name);
		add_error (model, str);
		g_free (str);

		node = model->priv->extract.node.node = NULL;
		return;
	}

	for (cur = node->children; cur; cur=cur->next) {
		if (xmlNodeIsText (cur))
			continue;
		if (!strcmp ((gchar*)cur->name, "gda_array_field")) {
			XmlColumnSpec *spec;

			spec = g_new0 (XmlColumnSpec, 1);
			fields = g_slist_append (fields, spec);

			spec->id = xmlGetProp (cur, (xmlChar*)"id");
			spec->name = xmlGetProp (cur, (xmlChar*)"name");
			spec->title = xmlGetProp (cur, (xmlChar*)"title");
			if (!spec->title && spec->name)
				spec->title = xmlStrdup (spec->name);

			spec->caption = xmlGetProp (cur, (xmlChar*)"caption");
			spec->dbms_type = xmlGetProp (cur, (xmlChar*)"dbms_type");
			str = (gchar*)xmlGetProp (cur, (xmlChar*)"gdatype");
			if (str) {
				spec->gdatype = gda_g_type_from_string (str);
				xmlFree (str);
				if (spec->gdatype == G_TYPE_INVALID)
					spec->gdatype = GDA_TYPE_NULL;
			}
			else {
				add_error (model, _("No \"gdatype\" attribute specified in <gda_array_field>"));
				clean_field_specs (fields);
				node = model->priv->extract.node.node = NULL;
				return;
			}
			str = (gchar*)xmlGetProp (cur, (xmlChar*)"size");
			if (str) {
				spec->size = atoi (str); /* Flawfinder: ignore */
				if (spec->size < 0)
					spec->size = 0;
				xmlFree (str);
			}
			str = (gchar*)xmlGetProp (cur, (xmlChar*)"scale");
			if (str) {
				spec->scale = atoi (str); /* Flawfinder: ignore */
				if (spec->scale < 0)
					spec->scale = 0;
				xmlFree (str);
			}
			str = (gchar*)xmlGetProp (cur, (xmlChar*)"pkey");
			if (str) {
				spec->pkey = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = (gchar*)xmlGetProp (cur, (xmlChar*)"unique");
			if (str) {
				spec->unique = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = (gchar*)xmlGetProp (cur, (xmlChar*)"nullok");
			spec->nullok = TRUE;
			if (str) {
				spec->nullok = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = (gchar*)xmlGetProp (cur, (xmlChar*)"auto_increment");
			if (str) {
				spec->autoinc = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			spec->table = xmlGetProp (cur, (xmlChar*)"table");
			spec->ref = xmlGetProp (cur, (xmlChar*)"ref");

			nbfields ++;
			continue;
		}
		if (!strcmp ((gchar*)cur->name, "gda_array_data"))
			break;
	}

	if (nbfields == 0) {
		add_error (model, _("No <gda_array_field> specified in <gda_array>"));
		node = model->priv->extract.node.node = NULL;
		clean_field_specs (fields);
		return;
	}

	/* random access model creation */
	ramodel = gda_data_model_array_new (nbfields);
	model->priv->random_access_model = ramodel;
	str = (gchar*)xmlGetProp (node, (xmlChar*)"id");
	if (str)
		g_object_set_data_full (G_OBJECT (model), "id", str, xmlFree);
	str = (gchar*)xmlGetProp (node, (xmlChar*)"name");
	if (str)
		g_object_set_data_full (G_OBJECT (model), "name", str, xmlFree);

	str = (gchar*)xmlGetProp (node, (xmlChar*)"descr");
	if (str)
		g_object_set_data_full (G_OBJECT (model), "descr", str, xmlFree);

	for (list = fields, pos = 0;
	     list;
	     list = list->next, pos++) {
		GdaColumn *column;
		XmlColumnSpec *spec;

		spec = (XmlColumnSpec *)(list->data);
		column = gda_data_model_describe_column (ramodel, pos);
		g_object_set (G_OBJECT (column), "id", spec->id, NULL);
		gda_column_set_description (column, (gchar*)spec->title);
		gda_column_set_name (column, (gchar*)spec->name);
		gda_column_set_dbms_type (column, (gchar*)spec->dbms_type);
		gda_column_set_g_type (column, spec->gdatype);
		gda_column_set_allow_null (column, spec->nullok);

		model->priv->columns = g_slist_prepend (model->priv->columns, gda_column_copy (column));
	}
	clean_field_specs (fields);
	model->priv->columns = g_slist_reverse (model->priv->columns);

	if (cur && ! gda_data_model_add_data_from_xml_node (ramodel, cur, &error))
		add_error (model, error && error->message ? error->message : _("No detail"));
}



static void
add_error (GdaDataModelImport *model, const gchar *err)
{
	GError *error = NULL;

	g_set_error (&error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR, "%s", err);
	model->priv->errors = g_slist_append (model->priv->errors, error);
}

/**
 * gda_data_model_import_get_errors:
 * @model: a #GdaDataModelImport object
 *
 * Get the list of errors which @model has to report. The returned list is a list of
 * #GError structures, and must not be modified
 *
 * Returns: (transfer none) (element-type GObject.Error): the list of errors (which must not be modified), or %NULL
 */
GSList *
gda_data_model_import_get_errors (GdaDataModelImport *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), NULL);
	g_return_val_if_fail (model->priv, NULL);

	return model->priv->errors;
}

/**
 * gda_data_model_import_clean_errors:
 * @model: a #GdaDataModelImport object
 *
 * Clears the history of errors @model has to report
 */
void
gda_data_model_import_clean_errors (GdaDataModelImport *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_IMPORT (model));
	g_return_if_fail (model->priv);

	g_slist_foreach (model->priv->errors, (GFunc) g_error_free, NULL);
	g_slist_free (model->priv->errors);
	model->priv->errors = NULL;
}



/*
 * GdaDataModel interface implementation
 */
static gint
gda_data_model_import_get_n_rows (GdaDataModel *model)
{
	GdaDataModelImport *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), 0);
	imodel = GDA_DATA_MODEL_IMPORT (model);
	g_return_val_if_fail (imodel->priv, 0);

	if (!imodel->priv->random_access)
		return -1;
	else {
		if (imodel->priv->random_access_model)
			return gda_data_model_get_n_rows (imodel->priv->random_access_model);
		else
			/* number of rows is not known */
			return -1;
	}
}

static gint
gda_data_model_import_get_n_columns (GdaDataModel *model)
{
	GdaDataModelImport *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), 0);
	imodel = GDA_DATA_MODEL_IMPORT (model);
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->columns)
		return g_slist_length (imodel->priv->columns);
	else
		return 0;
}

static GdaColumn *
gda_data_model_import_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelImport *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), NULL);
	imodel = GDA_DATA_MODEL_IMPORT (model);
	g_return_val_if_fail (imodel->priv, NULL);

	if (imodel->priv->columns)
		return g_slist_nth_data (imodel->priv->columns, col);
	else
		return NULL;
}

static GdaDataModelAccessFlags
gda_data_model_import_get_access_flags (GdaDataModel *model)
{
	GdaDataModelImport *imodel;
	GdaDataModelAccessFlags flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), 0);
	imodel = GDA_DATA_MODEL_IMPORT (model);
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->format == FORMAT_CSV) {
		/*flags |= GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD*/;
  }

	if (imodel->priv->random_access && imodel->priv->random_access_model)
		flags |= GDA_DATA_MODEL_ACCESS_RANDOM;

	return flags;
}

static const GValue *
gda_data_model_import_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaDataModelImport *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), NULL);
	imodel = GDA_DATA_MODEL_IMPORT (model);
	g_return_val_if_fail (imodel->priv, NULL);

	if (imodel->priv->random_access_model)
		/* if there is a random access model, then use it */
		return gda_data_model_get_value_at (imodel->priv->random_access_model, col, row, error);
	else {
		/* otherwise, bail out */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			      "%s", _("Data model does not support random access"));
		return NULL;
	}
}

static GdaValueAttribute
gda_data_model_import_get_attributes_at (GdaDataModel *model, gint col, G_GNUC_UNUSED gint row)
{
	GdaValueAttribute flags = 0;
	GdaDataModelImport *imodel;
	GdaColumn *column;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), 0);
	imodel = (GdaDataModelImport *) model;
	g_return_val_if_fail (imodel->priv, 0);

	flags = GDA_VALUE_ATTR_NO_MODIF;
	column = gda_data_model_describe_column (model, col);
	if (gda_column_get_allow_null (column))
		flags |= GDA_VALUE_ATTR_CAN_BE_NULL;

	return flags;
}

static GdaDataModelIter *
gda_data_model_import_create_iter (GdaDataModel *model)
{
	GdaDataModelImport *imodel;
	GdaDataModelIter *iter;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), NULL);
	imodel = (GdaDataModelImport *) model;
	g_return_val_if_fail (imodel->priv, NULL);

	if (imodel->priv->random_access_model) {
		iter = gda_data_model_create_iter (imodel->priv->random_access_model);
		/* REM: don't do:
		 * g_object_set (G_OBJECT (iter), "forced-model", model, NULL);
		 * because otherwise data fetch will come back on @model, which is not what is wanted.
		 * However the problem is that getting the "model" property from the returned iterator will
		 * give a pointer to imodel->priv->random_access_model and not @model...
		 */
	}
	else
		iter = (GdaDataModelIter *) g_object_new (GDA_TYPE_DATA_MODEL_ITER,
							  "data-model", model, NULL);
	return iter;
}

static void
add_error_too_few_values (GdaDataModelImport *model)
{
	gchar *str;

	switch (model->priv->format){
	case FORMAT_CSV:
		if (model->priv->strict)
			str = g_strdup_printf (_("Row at line %d does not have enough values"),
					       model->priv->extract.csv.text_line > 1 ?
					       model->priv->extract.csv.text_line - 1 :
					       model->priv->extract.csv.text_line);
		else
			str = g_strdup_printf (_("Row at line %d does not have enough values, "
						 "completed with NULL values"),
					       model->priv->extract.csv.text_line > 1 ?
					       model->priv->extract.csv.text_line - 1 :
					       model->priv->extract.csv.text_line);
		add_error (model, str);
		g_free (str);
		break;
	default:
		if (model->priv->strict)
			add_error (model, ("Row does not have enough values"));
		else
			add_error (model, ("Row does not have enough values, "
					   "completed with NULL values"));
		break;
	}
}

static void
add_error_too_many_values (GdaDataModelImport *model)
{
	gchar *str;

	switch (model->priv->format){
	case FORMAT_CSV:
		str = g_strdup_printf (_("Row at line %d does not have enough values "
					 "(which are thus ignored)"),
				       model->priv->extract.csv.text_line);
		add_error (model, str);
		g_free (str);
		break;
	default:
		add_error (model, ("Row does not have enough values (which are thus ignored)"));
		break;
	}
}

static gboolean
gda_data_model_import_iter_next (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaDataModelImport *imodel;
	GSList *next_values = NULL;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), FALSE);
	imodel = (GdaDataModelImport *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	/* if there is a random access model, then use it */
	if (imodel->priv->format == FORMAT_XML_NODE)
		return gda_data_model_iter_move_next_default (model, iter);

	/* fetch the next row if necessary */
	switch (imodel->priv->format) {
	case FORMAT_XML_DATA:
		xml_fetch_next_row (imodel);
		next_values = imodel->priv->cursor_values;
		break;

	case FORMAT_CSV:
		if (! imodel->priv->extract.csv.rows_read) {
			gda_data_model_iter_invalidate_contents (iter);
			return FALSE;
		}

		if (gda_data_model_iter_is_valid (iter) &&
		    (imodel->priv->extract.csv.rows_read->len > 0)) {
			/* get rid of row pointer by iter */
			GSList *list = g_array_index (imodel->priv->extract.csv.rows_read,
						      GSList *, 0);
			g_assert (list);
			g_slist_foreach (list, (GFunc) gda_value_free, NULL);
			g_slist_free (list);
			g_array_remove_index (imodel->priv->extract.csv.rows_read, 0);
		}

		/* fetch some more rows if necessary */
		if (imodel->priv->extract.csv.rows_read->len == 0)
			csv_fetch_some_lines (imodel);
		if (imodel->priv->extract.csv.rows_read->len != 0)
			next_values = g_array_index (imodel->priv->extract.csv.rows_read,
						     GSList *, 0);
		break;
	default:
		g_assert_not_reached ();
	}

	/* set values in iter */
	if (next_values) {
		GSList *plist;
		GSList *vlist;
		gboolean update_model;

		g_object_get (G_OBJECT (iter), "update-model", &update_model, NULL);
		g_object_set (G_OBJECT (iter), "update-model", FALSE, NULL);
		for (plist = ((GdaSet *) iter)->holders, vlist = next_values;
		     plist && vlist;
		     plist = plist->next, vlist = vlist->next) {
			GError *lerror = NULL;
			if (! gda_holder_take_value ((GdaHolder*) plist->data,
						     (GValue *) vlist->data, &lerror))
				gda_holder_force_invalid_e (GDA_HOLDER (plist->data), lerror);
			vlist->data = NULL;
		}
		if (plist || vlist) {
			if (plist) {
				add_error_too_few_values (imodel);
				for (; plist; plist = plist->next) {
					if (imodel->priv->strict)
						gda_holder_force_invalid (GDA_HOLDER (plist->data));
					else
						gda_holder_set_value (GDA_HOLDER (plist->data), NULL, NULL);
				}
			}
			else
				add_error_too_many_values (imodel);
		}
		if (gda_data_model_iter_is_valid (iter))
			imodel->priv->iter_row ++;
		else
			imodel->priv->iter_row = 0;

		g_object_set (G_OBJECT (iter), "current-row", imodel->priv->iter_row,
			      "update-model", update_model, NULL);

		return TRUE;
	}
	else {
		gda_data_model_iter_invalidate_contents (iter);
		g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
		g_signal_emit_by_name (iter, "end-of-data");
		return FALSE;
	}
}

static gboolean
gda_data_model_import_iter_prev (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaDataModelImport *imodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), FALSE);
	imodel = (GdaDataModelImport *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	if (imodel->priv->format == FORMAT_XML_DATA)
		return FALSE;

	/* if there is a random access model, then use it */
	if (imodel->priv->format == FORMAT_XML_NODE)
		return gda_data_model_iter_move_prev_default (model, iter);

	/* fetch the previous row if necessary */
	switch (imodel->priv->format) {
	case FORMAT_CSV:
	default:
		g_assert_not_reached ();
	}

	/* set values in iter */
	if (imodel->priv->cursor_values) {
		GSList *plist;
		GSList *vlist;
		gboolean update_model;

		g_object_get (G_OBJECT (iter), "update-model", &update_model, NULL);
		g_object_set (G_OBJECT (iter), "update-model", FALSE, NULL);
		for (plist = ((GdaSet *) iter)->holders, vlist = imodel->priv->cursor_values;
		     plist && vlist;
		     plist = plist->next, vlist = vlist->next) {
			GError *lerror = NULL;
			if (! gda_holder_set_value ((GdaHolder*) plist->data,
						    (GValue *) vlist->data, &lerror))
				gda_holder_force_invalid_e (GDA_HOLDER (plist->data), lerror);
		}
		if (plist || vlist) {
			if (plist) {
				add_error_too_few_values (imodel);
				for (; plist; plist = plist->next) {
					if (imodel->priv->strict)
						gda_holder_force_invalid (GDA_HOLDER (plist->data));
					else
						gda_holder_set_value (GDA_HOLDER (plist->data), NULL, NULL);
				}
			}
			else
				add_error_too_many_values (imodel);
		}

		if (gda_data_model_iter_is_valid (iter))
			imodel->priv->iter_row --;

		g_assert (imodel->priv->iter_row >= 0);
		g_object_set (G_OBJECT (iter), "current-row", imodel->priv->iter_row,
			      "update-model", update_model, NULL);
		return TRUE;
	}
	else {
		gda_data_model_iter_invalidate_contents (iter);
		g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
		return FALSE;
	}
}
