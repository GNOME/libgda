/*
 * Copyright (C) 2006 - 2014 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Brecht Sanders <brecht@edustria.be>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2013, 2018-2019 Daniel Espinosa <esodan@gmail.com>
 * Copyright (C) 2013 Miguel Angel Cabrera Moya <madmac2501@gmail.com>
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
#define G_LOG_DOMAIN "GDA-data-model-import"

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


/* GdaDataModel interface */
static void                 gda_data_model_import_data_model_init (GdaDataModelInterface *iface);
static gint                 gda_data_model_import_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_import_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_import_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_model_import_get_access_flags(GdaDataModel *model);
static const GValue        *gda_data_model_import_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);
static GdaValueAttribute    gda_data_model_import_get_attributes_at (GdaDataModel *model, gint col, gint row);
static GdaDataModelIter    *gda_data_model_import_create_iter      (GdaDataModel *model);
static gboolean             gda_data_model_import_iter_next       (GdaDataModel *model, GdaDataModelIter *iter);
static gboolean             gda_data_model_import_iter_prev       (GdaDataModel *model, GdaDataModelIter *iter);


typedef struct {
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
} GdaDataModelImportPrivate;

G_DEFINE_TYPE_WITH_CODE (GdaDataModelImport, gda_data_model_import, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaDataModelImport)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_MODEL, gda_data_model_import_data_model_init))

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

static GObject *gda_data_model_import_constructor (GType type,
						   guint n_construct_properties,
						   GObjectConstructParam *construct_properties);
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

static const gchar *find_option_as_string (GdaDataModelImport *model, const gchar *pname);
static gboolean     find_option_as_boolean (GdaDataModelImport *model, const gchar *pname, gboolean defaults);
static void         add_error (GdaDataModelImport *model, const gchar *err);

G_DEFINE_TYPE(GdaDataModelImportIter, gda_data_model_import_iter, GDA_TYPE_DATA_MODEL_ITER)

static gboolean gda_data_model_import_iter_move_next (GdaDataModelIter *iter);
static gboolean gda_data_model_import_iter_move_prev (GdaDataModelIter *iter);

static void gda_data_model_import_iter_init (GdaDataModelImportIter *iter) {}
static void gda_data_model_import_iter_class_init (GdaDataModelImportIterClass *klass) {
	GdaDataModelIterClass *model_iter_class = GDA_DATA_MODEL_ITER_CLASS (klass);
	model_iter_class->move_next = gda_data_model_import_iter_move_next;
	model_iter_class->move_prev = gda_data_model_import_iter_move_prev;
}

static gboolean
gda_data_model_import_iter_move_next (GdaDataModelIter *iter) {
	GdaDataModel *model;
	g_object_get (G_OBJECT (iter), "data-model", &model, NULL);
	g_return_val_if_fail (model, FALSE);
	return gda_data_model_import_iter_next (model, iter);
}

static gboolean
gda_data_model_import_iter_move_prev (GdaDataModelIter *iter) {
	GdaDataModel *model;
	g_object_get (G_OBJECT (iter), "data-model", &model, NULL);
	g_return_val_if_fail (model, FALSE);
	return gda_data_model_import_iter_prev (model, iter);
}


static void
gda_data_model_import_class_init (GdaDataModelImportClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
	object_class->constructor = gda_data_model_import_constructor;
	object_class->dispose = gda_data_model_import_dispose;
	object_class->finalize = gda_data_model_import_finalize;
}

static void
gda_data_model_import_data_model_init (GdaDataModelInterface *iface)
{
	iface->get_n_rows = gda_data_model_import_get_n_rows;
	iface->get_n_columns = gda_data_model_import_get_n_columns;
	iface->describe_column = gda_data_model_import_describe_column;
        iface->get_access_flags = gda_data_model_import_get_access_flags;
	iface->get_value_at = gda_data_model_import_get_value_at;
	iface->get_attributes_at = gda_data_model_import_get_attributes_at;

	iface->create_iter = gda_data_model_import_create_iter;

	iface->set_value_at = NULL;
	iface->set_values = NULL;
        iface->append_values = NULL;
	iface->append_row = NULL;
	iface->remove_row = NULL;
	iface->find_row = NULL;

	iface->freeze = NULL;
	iface->thaw = NULL;
	iface->get_notify = NULL;
	iface->send_hint = NULL;
}

static void
gda_data_model_import_init (GdaDataModelImport *model)
{
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	priv->random_access = FALSE; /* cursor mode is the default */
	priv->columns = NULL;
	priv->random_access_model = NULL;
	priv->errors = NULL;
	priv->cursor_values = NULL;

	priv->is_mapped = TRUE;
	priv->src.mapped.filename = NULL;
	priv->src.mapped.fd = -1;
	priv->src.mapped.start = NULL;

	priv->format = FORMAT_CSV;
	priv->data_start = NULL;
	priv->data_length = 0;

	priv->iter_row = -1;
	priv->init_done = FALSE;
	priv->strict = FALSE;
}

static void init_csv_import  (GdaDataModelImport *model);
static void init_xml_import  (GdaDataModelImport *model);
static void init_node_import (GdaDataModelImport *model);

static GObject *
gda_data_model_import_constructor (GType type,
				   guint n_construct_properties,
				   GObjectConstructParam *construct_properties)
{
	GObject *object;

	/* construct parent */
	object = G_OBJECT_CLASS (gda_data_model_import_parent_class)->constructor (type,
							     n_construct_properties,
							     construct_properties);

	/* parse construct properties */
	GdaDataModelImport *model;
	guint i;
	model = GDA_DATA_MODEL_IMPORT (object);
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	for (i = 0; i< n_construct_properties; i++) {
		GObjectConstructParam *prop = &(construct_properties[i]);
		if (!strcmp (g_param_spec_get_name (prop->pspec), "random-access")) {
			priv->random_access = g_value_get_boolean (prop->value);
			if (priv->format == FORMAT_XML_NODE)
				priv->random_access = TRUE;
		}
		else if (!strcmp (g_param_spec_get_name (prop->pspec), "filename")) {
			const gchar *string;
			string = g_value_get_string (prop->value);
			if (!string)
				continue;

			priv->is_mapped = TRUE;
			priv->src.mapped.filename = g_strdup (g_value_get_string (prop->value));

			/* file opening */
			priv->src.mapped.fd = open (priv->src.mapped.filename, O_RDONLY); /* Flawfinder: ignore */
			if (priv->src.mapped.fd < 0) {
				/* error */
				add_error (model, strerror(errno));
				continue;
			}

			/* file mmaping */
			struct stat _stat;

			if (fstat (priv->src.mapped.fd, &_stat) < 0) {
				/* error */
				add_error (model, strerror(errno));
				continue;
			}
			priv->src.mapped.length = _stat.st_size;
#ifndef G_OS_WIN32
			priv->src.mapped.start = mmap (NULL, priv->src.mapped.length,
							      PROT_READ, MAP_PRIVATE,
							      priv->src.mapped.fd, 0);
			if (priv->src.mapped.start == MAP_FAILED) {
				/* error */
				add_error (model, strerror(errno));
				priv->src.mapped.start = NULL;
				continue;
			}
#else
			HANDLE hFile = CreateFile (priv->src.mapped.filename,
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
				continue;
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
				continue;
			}
			priv->src.mapped.start = MapViewOfFile(view, FILE_MAP_READ, 0, 0,
								      priv->src.mapped.length);
			if (!priv->src.mapped.start) {
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
				continue;
			}
#endif
			priv->data_start = priv->src.mapped.start;
			priv->data_length = priv->src.mapped.length;
		}
		else if (!strcmp (g_param_spec_get_name (prop->pspec), "data-string")) {
			const gchar *string;
			string = g_value_get_string (prop->value);
			if (!string)
				continue;
			priv->is_mapped = FALSE;
			priv->src.string = g_strdup (g_value_get_string (prop->value));
			priv->data_start = priv->src.string;
			priv->data_length = strlen (priv->src.string);
		}
		else if (!strcmp (g_param_spec_get_name (prop->pspec), "xml-node")) {
			gpointer data = g_value_get_pointer (prop->value);
			if (!data)
				continue;
			priv->format = FORMAT_XML_NODE;
			priv->extract.node.node = data;
		}
		else if (!strcmp (g_param_spec_get_name (prop->pspec), "options")) {
			if (priv->options)
				g_object_unref(priv->options);

			priv->options = g_value_get_object (prop->value);
			if (priv->options) {
				if (!GDA_IS_SET (priv->options)) {
					g_warning (_("\"options\" property is not a GdaSet object"));
					priv->options = NULL;
				}
				else
					g_object_ref (priv->options);
			}
		}
		else if (!strcmp (g_param_spec_get_name (prop->pspec), "strict")) {
			priv->strict = g_value_get_boolean (prop->value);
		}
	}

	if (priv->errors)
		return object;

	/* finish construction */
	/* determine the real kind of data (CVS text of XML) */
	if (priv->format != FORMAT_XML_NODE
	    && priv->data_start) {
		if (!strncmp (priv->data_start, "<?xml", 5))
			priv->format = FORMAT_XML_DATA;
		else
			priv->format = FORMAT_CSV;
	}

	/* analyze common options and init */
	if (! priv->init_done) {
		priv->init_done = TRUE;
		switch (priv->format) {
		case FORMAT_XML_DATA:
			init_xml_import (model);
			break;

		case FORMAT_CSV:
			priv->extract.csv.quote = '"';
			if (priv->options) {
				const gchar *option;
				option = find_option_as_string (model, "ENCODING");
				if (option)
					priv->extract.csv.encoding = g_strdup (option);
				option = find_option_as_string (model, "SEPARATOR");
				if (option)
					priv->extract.csv.delimiter = *option;
				priv->extract.csv.quote = '"';
				option = find_option_as_string (model, "QUOTE");
				if (option)
					priv->extract.csv.quote = *option;
			}
			init_csv_import (model);
			break;

		case FORMAT_XML_NODE:
			priv->random_access = TRUE;
			init_node_import (model);
			break;
		default:
			g_assert_not_reached ();
		}

		/* for random access, create a new GdaDataModelArray model and copy the contents
		   from this model */
		if (priv->random_access && priv->columns && !priv->random_access_model) {
			GdaDataModel *ramodel;

			ramodel = gda_data_access_wrapper_new ((GdaDataModel *) model);
			priv->random_access_model = ramodel;
		}
	}

	return object;
}


static void
csv_free_stored_rows (GdaDataModelImport *model)
{
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	gsize i;
	g_assert (priv->format == FORMAT_CSV);
	for (i = 0; i < priv->extract.csv.rows_read->len; i++) {
		GSList *list = g_array_index (priv->extract.csv.rows_read,
					      GSList *, i);
		g_slist_free_full (list, (GDestroyNotify) gda_value_free);
	}

	if (priv->extract.csv.pdata) {
		if (priv->extract.csv.pdata->fields) {
			g_slist_free_full (priv->extract.csv.pdata->fields, (GDestroyNotify) gda_value_free);
		}
		g_free (priv->extract.csv.pdata);
	}

	g_array_free (priv->extract.csv.rows_read, FALSE);
	priv->extract.csv.rows_read = NULL;
}

static void
gda_data_model_import_dispose (GObject *object)
{
	GdaDataModelImport *model = (GdaDataModelImport *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_IMPORT (model));
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);

	/* free memory */
	if (priv->options) {
		g_object_unref (priv->options);
		priv->options = NULL;
	}

	if (priv->columns) {
		g_slist_free_full (priv->columns, (GDestroyNotify) g_object_unref);
		priv->columns = NULL;
	}

	/* data access mem free */
	if (priv->is_mapped) {
		if (priv->src.mapped.start) {
#ifndef G_OS_WIN32
			munmap (priv->src.mapped.start, priv->src.mapped.length);
#else
			UnmapViewOfFile (priv->src.mapped.start);
#endif
			priv->src.mapped.start = NULL;
		}

		g_free (priv->src.mapped.filename);
		if (priv->src.mapped.fd >= 0) {
			close (priv->src.mapped.fd);
			priv->src.mapped.fd = -1;
		}
	}
	else {
		g_free (priv->src.string);
		priv->src.string = NULL;
	}

	/* extraction data free */
	switch (priv->format) {
	case FORMAT_XML_DATA:
		if (priv->extract.xml.reader) {
			xmlFreeTextReader (priv->extract.xml.reader);
			priv->extract.xml.reader = NULL;
		}
		break;
	case FORMAT_CSV:
		if (priv->extract.csv.parser) {
			csv_fini (priv->extract.csv.parser, NULL, NULL, NULL);
			priv->extract.csv.parser = NULL;
		}
		if (priv->extract.csv.rows_read)
			csv_free_stored_rows (model);
		if (priv->extract.csv.encoding) {
			g_free (priv->extract.csv.encoding);
			priv->extract.csv.encoding = NULL;
		}
		break;
	case FORMAT_XML_NODE:
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	/* random access model free */
	if (priv->random_access_model) {
		g_object_unref (priv->random_access_model);
		priv->random_access_model = NULL;
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_data_model_import_parent_class)->dispose (object);
}

static void
gda_data_model_import_finalize (GObject *object)
{
	GdaDataModelImport *model = (GdaDataModelImport *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_IMPORT (model));
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);

	/* free memory */
	if (priv->errors) {
		g_slist_free_full (priv->errors, (GDestroyNotify) g_error_free);
		priv->errors = NULL;
	}

	if (priv->cursor_values) {
		g_slist_free_full (priv->cursor_values, (GDestroyNotify) gda_value_free);
		priv->cursor_values = NULL;
	}


	/* chain to parent class */
	G_OBJECT_CLASS (gda_data_model_import_parent_class)->finalize (object);
}


static const gchar *
find_option_as_string (GdaDataModelImport *model, const gchar *pname)
{
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	const GValue *value;

	value = gda_set_get_holder_value (priv->options, pname);
	if (value && !gda_value_is_null ((GValue *) value)) {
		if (!gda_value_isa ((GValue *) value, G_TYPE_STRING))
			g_warning (_("The %s option must hold a string value, ignored."), pname);
		else
			return g_value_get_string ((GValue *) value);
	}
	return NULL;
}

static gboolean
find_option_as_boolean (GdaDataModelImport *model, const gchar *pname, gboolean defaults)
{
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	const GValue *value;

	value = gda_set_get_holder_value (priv->options, pname);
	if (value && !gda_value_is_null ((GValue *) value)) {
		if (!gda_value_isa ((GValue *) value, G_TYPE_BOOLEAN))
			g_warning (_("The '%s' option must hold a "
				     "boolean value, ignored."), pname);
		else
			return g_value_get_boolean ((GValue *) value);
	}

	return defaults;
}

static void
gda_data_model_import_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdaDataModelImport *model;

	model = GDA_DATA_MODEL_IMPORT (object);
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	switch (param_id) {
	case PROP_OPTIONS:
	case PROP_XML_NODE:
	case PROP_DATA_STRING:
	case PROP_FILENAME:
	case PROP_RANDOM_ACCESS:
		/* handled in the constructor */
		break;
	case PROP_STRICT:
		priv->strict = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	switch (param_id) {
	case PROP_RANDOM_ACCESS:
		g_value_set_boolean (value, priv->random_access);
		break;
	case PROP_FILENAME:
		if (priv->is_mapped)
			g_value_set_string (value, priv->src.mapped.filename);
		else
			g_value_set_string (value, NULL);
		break;
	case PROP_DATA_STRING:
		if (priv->is_mapped)
			g_value_set_string (value, NULL);
		else
			g_value_set_string (value, priv->src.string);
		break;
	case PROP_STRICT:
		g_value_set_boolean (value, priv->strict);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gda_data_model_import_new_file:
 * @filename: the file to import data from
 * @random_access: TRUE if random access will be required
 * @options: (transfer none) (nullable): importing options
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
 * @options: (transfer none) (nullable): importing options, see gda_data_model_import_new_file() for more information
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	gboolean title_first_line = FALSE;
	gint nbcols;

	if (priv->options)
		title_first_line = find_option_as_boolean (model, "NAMES_ON_FIRST_LINE", FALSE) ||
			find_option_as_boolean (model, "TITLE_AS_FIRST_LINE", FALSE);

	g_assert (priv->format == FORMAT_CSV);

	if (!priv->extract.csv.delimiter)
		priv->extract.csv.delimiter = ',';

	priv->extract.csv.ignore_first_line = FALSE;
	priv->extract.csv.start_pos = priv->data_start;
	priv->extract.csv.text_line = 1; /* start line numbering at 1 */
	priv->extract.csv.rows_read = g_array_new (FALSE, TRUE, sizeof (GSList *));

	/* parser init */
	if (!csv_init_csv_parser (model))
		return;

	/* set parser parameters */
	csv_set_delim (priv->extract.csv.parser, priv->extract.csv.delimiter);
	csv_set_quote (priv->extract.csv.parser, priv->extract.csv.quote);

	/* fill in at least a row to determine the number of columns */
	priv->extract.csv.initializing = TRUE;
	csv_fetch_some_lines (model);
	priv->extract.csv.initializing = FALSE;

	/* computing columns */
	if (priv->extract.csv.rows_read->len == 0)
		return;

	GSList *row;
	gint col;
	const GValue *cvalue;

	row = g_array_index (priv->extract.csv.rows_read, GSList *, 0);
	g_assert (row);
	nbcols = g_slist_length (row);

	for (col = 0; col < nbcols; col++) {
		GdaColumn *column;
		gchar *str = NULL;

		column = gda_column_new ();
		priv->columns = g_slist_append (priv->columns,
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
		if (priv->options) {
			gchar *pname;
			const GValue *value;

			pname = g_strdup_printf ("G_TYPE_%d", col);
			value = gda_set_get_holder_value (priv->options, pname);
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
	csv_fini (priv->extract.csv.parser, NULL, NULL, NULL);
	csv_init_csv_parser (model);

	priv->extract.csv.start_pos = priv->data_start;
	priv->extract.csv.text_line = 1; /* start line numbering at 1 */
	priv->extract.csv.rows_read = g_array_new (FALSE, TRUE, sizeof (GSList *));
	if (title_first_line)
		priv->extract.csv.ignore_first_line = TRUE;
	csv_fetch_some_lines (model);
}

static gboolean
csv_init_csv_parser (GdaDataModelImport *model)
{
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	if (csv_init (&(priv->extract.csv.parser), 0) != 0) {
		priv->extract.csv.parser = NULL;
		return FALSE;
	}

	priv->extract.csv.pdata = g_new0 (CsvParserData, 1);
	priv->extract.csv.pdata->nb_cols = gda_data_model_get_n_columns ((GdaDataModel*) model);
	priv->extract.csv.pdata->model = model;
	priv->extract.csv.pdata->field_next_col = 0;
	priv->extract.csv.pdata->fields = NULL;

	csv_set_delim (priv->extract.csv.parser, priv->extract.csv.delimiter);
	csv_set_quote (priv->extract.csv.parser, priv->extract.csv.quote);
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (pdata->model);

	if (priv->extract.csv.ignore_first_line)
		return;

	/* convert to correct encoding */
	if (priv->extract.csv.encoding) {
		GError *error = NULL;
		copy = g_convert (s, len, "UTF-8", priv->extract.csv.encoding,
				  NULL, NULL, &error);
		if (!copy) {
			gchar *str;
			str = g_strdup_printf (_("Character conversion at line %d, error: %s"),
					       priv->extract.csv.text_line,
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
	if (! priv->extract.csv.initializing) {
		if (pdata->field_next_col >= pdata->nb_cols) {
			/* ignore extra fields */
			g_free (copy);
			return;
		}
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (pdata->model);

	if (priv->extract.csv.ignore_first_line)
		priv->extract.csv.ignore_first_line = FALSE;
	else {
		row = g_slist_reverse (pdata->fields);
		pdata->fields = NULL;
		pdata->field_next_col = 0;

		size = g_slist_length (row);
		g_assert (size <= pdata->nb_cols);
		/*g_print ("===========ROW %d (%d cols)===========\n", priv->extract.csv.text_line, size);*/

		g_array_append_val (priv->extract.csv.rows_read, row);
		priv->extract.csv.text_line ++;
	}
}

static gboolean
csv_fetch_some_lines (GdaDataModelImport *model)
{
	size_t size;
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);

	if (!priv->extract.csv.initializing)
		size = MIN (CSV_TITLE_BUFFER_SIZE,
			    priv->data_start + priv->data_length -
			    priv->extract.csv.start_pos);
	else
		size = MIN (CSV_DATA_BUFFER_SIZE,
			    priv->data_start + priv->data_length -
			    priv->extract.csv.start_pos);

	if (csv_parse (priv->extract.csv.parser,
		       priv->extract.csv.start_pos, size,
		       csv_parser_field_read_cb,
		       csv_parser_row_read_cb, priv->extract.csv.pdata) != size) {
		gchar *str = g_strdup_printf (_("Error while parsing CSV file: %s"),
					      csv_strerror (csv_error (priv->extract.csv.parser)));
		add_error (model, str);
		g_free (str);
		priv->extract.csv.start_pos = priv->data_start + priv->data_length;
		return FALSE;
	}
	else {
		priv->extract.csv.start_pos += size;

		/* try to finish the line if it has not been read entirely */
		if (priv->extract.csv.rows_read->len == 0) {
			if ((priv->extract.csv.start_pos !=
			     priv->data_start + priv->data_length))
				return csv_fetch_some_lines (model);
			else {
				/* end of data */
				csv_fini (priv->extract.csv.parser,
					  csv_parser_field_read_cb,
					  csv_parser_row_read_cb, priv->extract.csv.pdata);
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);

	g_assert (priv->format == FORMAT_XML_DATA);

	/* init extraction specific variables */
	reader = xmlReaderForMemory (priv->data_start,
				     priv->data_length,
				     NULL, NULL, 0);
	priv->extract.xml.reader = reader;

	if (! reader) {
		/* error, try to switch to csv */
		priv->format = FORMAT_CSV;
		init_csv_import (model);
		return;
	}

	/* create columns */
	ret = xmlTextReaderRead (reader);
	if (ret < 0) {
		/* error */
		add_error (model, _("Failed to read node in XML file"));
		xmlFreeTextReader (reader);
		priv->extract.xml.reader = NULL;
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
			priv->extract.xml.reader = NULL;
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
				priv->extract.xml.reader = NULL;
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
			priv->extract.xml.reader = NULL;
			return;
		}

		for (list = fields, pos = 0;
		     list;
		     list = list->next, pos++) {
			GdaColumn *column;
			XmlColumnSpec *spec;

			spec = (XmlColumnSpec *)(list->data);
			column = gda_column_new ();
			priv->columns = g_slist_append (priv->columns, column);
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
			priv->extract.xml.reader = NULL;
			return;
		}

		/* get ready for the data reading */
		ret = xml_fetch_next_xml_node (reader);
		if (ret <= 0) {
			add_error (model, _("Can't read the contents of the <gda_array_data> node"));
			xmlFreeTextReader (reader);
			priv->extract.xml.reader = NULL;
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	xmlTextReaderPtr reader;
	const xmlChar *name;
	gint ret;

	const gchar *lang = setlocale(LC_ALL, NULL);

	GSList *columns = priv->columns;
	GdaColumn *last_column = NULL;
	GSList *values = NULL;

	if (priv->cursor_values) {
		g_slist_free_full (priv->cursor_values, (GDestroyNotify) gda_value_free);
		priv->cursor_values = NULL;
	}

	reader = priv->extract.xml.reader;
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
		priv->extract.xml.reader = NULL;
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
		priv->cursor_values = g_slist_reverse (values);
#ifdef GDA_DEBUG_NO
	GSList *l;
	gint c;

	g_print ("======== GdaDataModelImport => next XML row ========\n");
	for (c = 0, l = priv->cursor_values; l; l = l->next, c++) {
		GValue *val = (GValue*)(l->data);
		g_print ("#%d %s (type:%s)\n", c, gda_value_stringify (val),
			 val ? g_type_name (G_VALUE_TYPE (val)) : "no type");
	}
#endif

	if (ret <= 0) {
		/* destroy the reader, nothing to read anymore */
		xmlFreeTextReader (reader);
		priv->extract.xml.reader = NULL;
	}
}

static void
init_node_import (GdaDataModelImport *model)
{
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	GdaDataModel *ramodel;
	xmlNodePtr node, cur;
	gint nbfields = 0;
	GSList *fields = NULL;
	GSList *list;
	gint pos;
	gchar *str;

	node = priv->extract.node.node;
	if (!node)
		return;

	if (strcmp ((gchar*)node->name, "gda_array")) {
		gchar *str;

		str = g_strdup_printf (_("Expected <gda_array> node but got <%s>"), node->name);
		add_error (model, str);
		g_free (str);

		node = priv->extract.node.node = NULL;
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
				node = priv->extract.node.node = NULL;
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
			spec->nullok = FALSE;
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
		node = priv->extract.node.node = NULL;
		clean_field_specs (fields);
		return;
	}

	/* random access model creation */
	ramodel = gda_data_model_array_new (nbfields);
	priv->random_access_model = ramodel;
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

		priv->columns = g_slist_prepend (priv->columns, gda_column_copy (column));
	}
	clean_field_specs (fields);
	priv->columns = g_slist_reverse (priv->columns);

	GError *error = NULL;
	if (cur && ! gda_data_model_add_data_from_xml_node (ramodel, cur, &error)) {
		add_error (model, error && error->message ? error->message : _("No detail"));
		g_clear_error (&error);
	}
}



static void
add_error (GdaDataModelImport *model, const gchar *err)
{
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	GError *error = NULL;

	g_set_error (&error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR, "%s", err);
	priv->errors = g_slist_append (priv->errors, error);
}

/**
 * gda_data_model_import_get_errors:
 * @model: a #GdaDataModelImport object
 *
 * Get the list of errors which @model has to report. The returned list is a list of
 * #GError structures, and must not be modified
 *
 * Returns: (transfer none) (element-type GLib.Error): the list of errors (which must not be modified), or %NULL
 */
GSList *
gda_data_model_import_get_errors (GdaDataModelImport *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), NULL);
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);

	return priv->errors;
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);

	g_slist_free_full (priv->errors, (GDestroyNotify) g_error_free);
	priv->errors = NULL;
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (imodel);

	if (!priv->random_access)
		return -1;
	else {
		if (priv->random_access_model)
			return gda_data_model_get_n_rows (priv->random_access_model);
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (imodel);

	if (priv->columns)
		return g_slist_length (priv->columns);
	else
		return 0;
}

static GdaColumn *
gda_data_model_import_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelImport *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), NULL);
	imodel = GDA_DATA_MODEL_IMPORT (model);
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (imodel);

	if (priv->columns)
		return g_slist_nth_data (priv->columns, col);
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (imodel);

	if (priv->format == FORMAT_CSV) {
		/*flags |= GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD*/;
  }

	if (priv->random_access && priv->random_access_model)
		flags |= GDA_DATA_MODEL_ACCESS_RANDOM;

	return flags;
}

static const GValue *
gda_data_model_import_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaDataModelImport *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), NULL);
	imodel = GDA_DATA_MODEL_IMPORT (model);
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (imodel);

	if (priv->random_access_model)
		/* if there is a random access model, then use it */
		return gda_data_model_get_value_at (priv->random_access_model, col, row, error);
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (imodel);
	g_return_val_if_fail (priv, 0);

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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (imodel);
	g_return_val_if_fail (priv, NULL);

	if (priv->random_access_model) {
		iter = gda_data_model_create_iter (priv->random_access_model);
		/* REM: don't do:
		 * g_object_set (G_OBJECT (iter), "forced-model", model, NULL);
		 * because otherwise data fetch will come back on @model, which is not what is wanted.
		 * However the problem is that getting the "model" property from the returned iterator will
		 * give a pointer to priv->random_access_model and not @model...
		 */
	}
	else
		iter = (GdaDataModelIter *) g_object_new (GDA_TYPE_DATA_MODEL_IMPORT_ITER,
							  "data-model", model, NULL);
	return iter;
}

static void
add_error_too_few_values (GdaDataModelImport *model)
{
	gchar *str;
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);

	switch (priv->format){
	case FORMAT_CSV:
		if (priv->strict)
			str = g_strdup_printf (_("Row at line %d does not have enough values"),
					       priv->extract.csv.text_line > 1 ?
					       priv->extract.csv.text_line - 1 :
					       priv->extract.csv.text_line);
		else
			str = g_strdup_printf (_("Row at line %d does not have enough values, "
						 "completed with NULL values"),
					       priv->extract.csv.text_line > 1 ?
					       priv->extract.csv.text_line - 1 :
					       priv->extract.csv.text_line);
		add_error (model, str);
		g_free (str);
		break;
	default:
		if (priv->strict)
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (model);
	gchar *str;

	switch (priv->format){
	case FORMAT_CSV:
		str = g_strdup_printf (_("Row at line %d does not have enough values "
					 "(which are thus ignored)"),
				       priv->extract.csv.text_line);
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (imodel);

	/* if there is a random access model, then use it */
	if (priv->format == FORMAT_XML_NODE)
		return gda_data_model_iter_move_next_default (model, iter);

	/* fetch the next row if necessary */
	switch (priv->format) {
	case FORMAT_XML_DATA:
		xml_fetch_next_row (imodel);
		next_values = priv->cursor_values;
		break;

	case FORMAT_CSV:
		if (! priv->extract.csv.rows_read) {
			gda_data_model_iter_invalidate_contents (iter);
			return FALSE;
		}

		if (gda_data_model_iter_is_valid (iter) &&
		    (priv->extract.csv.rows_read->len > 0)) {
			/* get rid of row pointer by iter */
			GSList *list = g_array_index (priv->extract.csv.rows_read,
						      GSList *, 0);
			g_assert (list);
			g_slist_free_full (list, (GDestroyNotify) gda_value_free);
			g_array_remove_index (priv->extract.csv.rows_read, 0);
		}

		/* fetch some more rows if necessary */
		if (priv->extract.csv.rows_read->len == 0)
			csv_fetch_some_lines (imodel);
		if (priv->extract.csv.rows_read->len != 0)
			next_values = g_array_index (priv->extract.csv.rows_read,
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
		for (plist = gda_set_get_holders (((GdaSet *) iter)), vlist = next_values;
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
					if (priv->strict)
						gda_holder_force_invalid (GDA_HOLDER (plist->data));
					else
						gda_holder_set_value (GDA_HOLDER (plist->data), NULL, NULL);
				}
			}
			else
				add_error_too_many_values (imodel);
		}
		if (gda_data_model_iter_is_valid (iter))
			priv->iter_row ++;
		else
			priv->iter_row = 0;

		g_object_set (G_OBJECT (iter), "current-row", priv->iter_row,
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
	GdaDataModelImportPrivate *priv = gda_data_model_import_get_instance_private (imodel);

	if (priv->format == FORMAT_XML_DATA)
		return FALSE;

	/* if there is a random access model, then use it */
	if (priv->format == FORMAT_XML_NODE)
		return gda_data_model_iter_move_prev_default (model, iter);

	/* fetch the previous row if necessary */
	switch (priv->format) {
	case FORMAT_CSV:
	default:
		g_assert_not_reached ();
	}

	/* set values in iter */
	if (priv->cursor_values) {
		GSList *plist;
		GSList *vlist;
		gboolean update_model;

		g_object_get (G_OBJECT (iter), "update-model", &update_model, NULL);
		g_object_set (G_OBJECT (iter), "update-model", FALSE, NULL);
		for (plist = gda_set_get_holders (((GdaSet *) iter)), vlist = priv->cursor_values;
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
					if (priv->strict)
						gda_holder_force_invalid (GDA_HOLDER (plist->data));
					else
						gda_holder_set_value (GDA_HOLDER (plist->data), NULL, NULL);
				}
			}
			else
				add_error_too_many_values (imodel);
		}

		if (gda_data_model_iter_is_valid (iter))
			priv->iter_row --;

		g_assert (priv->iter_row >= 0);
		g_object_set (G_OBJECT (iter), "current-row", priv->iter_row,
			      "update-model", update_model, NULL);
		return TRUE;
	}
	else {
		gda_data_model_iter_invalidate_contents (iter);
		g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
		return FALSE;
	}
}
