/* GDA library
 * Copyright (C) 2006 The GNOME Foundation.
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

#include "gda-data-model-import.h"
#include <unistd.h>
#include <sys/mman.h>
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
#include <libgda/gda-parameter.h>
#include <libgda/gda-parameter-list.h>
#include <libgda/gda-dict.h>
#include <libgda/gda-dict-type.h>

#include <libxml/xmlreader.h>

typedef enum {
	CSV_AT_START,
	CSV_IN_DATA,
	CSV_AT_END
} CsvPosition;

typedef enum {
	FORMAT_XML_DATA,
	FORMAT_CSV,
	FORMAT_XML_NODE
} InternalFormat;

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
			gchar            *encoding;
			gchar            *delimiter;
			gchar             escape_char;

			gchar            *start_pos;
			gchar            *end_pos; /* character at this position MUST NOT be read */
			gchar            *line_start;
			gchar            *line_end;

			guint             text_line;
			CsvPosition       pos;     
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
	GSList              *errors; /* list of error strings */
	GdaParameterList    *options;

	GdaDataModelIter    *iter; /* iterator to be returned when a new iter is asked */
	gint                 iter_row;
};

/* properties */
enum
{
        PROP_0,
	PROP_RANDOM_ACCESS,
        PROP_FILENAME,
	PROP_DATA,
	PROP_XML_NODE,
	PROP_OPTIONS,
};

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
static void                 gda_data_model_import_data_model_init (GdaDataModelClass *iface);
static gint                 gda_data_model_import_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_import_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_import_describe_column (GdaDataModel *model, gint col);
static guint                gda_data_model_import_get_access_flags(GdaDataModel *model);
static const GValue      *gda_data_model_import_get_value_at    (GdaDataModel *model, gint col, gint row);
static guint                gda_data_model_import_get_attributes_at (GdaDataModel *model, gint col, gint row);
static GdaDataModelIter    *gda_data_model_impor_create_iter      (GdaDataModel *model);
static gboolean             gda_data_model_import_iter_next       (GdaDataModel *model, GdaDataModelIter *iter); 
static gboolean             gda_data_model_import_iter_prev       (GdaDataModel *model, GdaDataModelIter *iter);

#ifdef GDA_DEBUG
static void gda_data_model_import_dump (GdaDataModelImport *model, guint offset);
#endif

static const gchar *find_option_as_string (GdaDataModelImport *model, const gchar *pname);
static gboolean     find_option_as_boolean (GdaDataModelImport *model, const gchar *pname, gboolean defaults);
static void         add_error (GdaDataModelImport *model, const gchar *err);

static GObjectClass *parent_class = NULL;

/**
 * gda_data_model_import_get_type
 *
 * Returns: the #GType of GdaDataModelImport.
 */
GType
gda_data_model_import_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelImportClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_import_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelImport),
			0,
			(GInstanceInitFunc) gda_data_model_import_init
		};

		static const GInterfaceInfo data_model_info = {
			(GInterfaceInitFunc) gda_data_model_import_data_model_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDataModelImport", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
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
	g_object_class_install_property (object_class, PROP_RANDOM_ACCESS,
                                         g_param_spec_boolean ("random_access", "Features random access", NULL,
							       FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_FILENAME,
                                         g_param_spec_string ("filename", "File to import", NULL, NULL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_DATA,
                                         g_param_spec_string ("data", "String to import", NULL, NULL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_XML_NODE,
                                         g_param_spec_pointer ("xml_node", "XML node to import from", NULL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_OPTIONS,
                                         g_param_spec_pointer ("options", "Options to configure the import", NULL,
							       G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY));

	/* virtual functions */
	object_class->dispose = gda_data_model_import_dispose;
	object_class->finalize = gda_data_model_import_finalize;
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (klass)->dump = (void (*)(GdaObject *, guint)) gda_data_model_import_dump;
#endif

	/* class attributes */
	GDA_OBJECT_CLASS (klass)->id_unique_enforced = FALSE;
}

static void
gda_data_model_import_data_model_init (GdaDataModelClass *iface)
{
	iface->i_get_n_rows = gda_data_model_import_get_n_rows;
	iface->i_get_n_columns = gda_data_model_import_get_n_columns;
	iface->i_describe_column = gda_data_model_import_describe_column;
        iface->i_get_access_flags = gda_data_model_import_get_access_flags;
	iface->i_get_value_at = gda_data_model_import_get_value_at;
	iface->i_get_attributes_at = gda_data_model_import_get_attributes_at;

	iface->i_create_iter = gda_data_model_impor_create_iter;
	iface->i_iter_at_row = NULL;
	iface->i_iter_next = gda_data_model_import_iter_next;
	iface->i_iter_prev = gda_data_model_import_iter_prev;

	iface->i_set_value_at = NULL;
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
gda_data_model_import_init (GdaDataModelImport *model, GdaDataModelImportClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_IMPORT (model));
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

	model->priv->iter = NULL;
	model->priv->iter_row = -1;
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
			g_free (model->priv->src.mapped.filename);
			if (model->priv->src.mapped.fd >= 0) {
				close (model->priv->src.mapped.fd);
				model->priv->src.mapped.fd = -1;
			}
			if (model->priv->src.mapped.start) {
				munmap (model->priv->src.mapped.start, model->priv->src.mapped.length);
				model->priv->src.mapped.start = NULL;
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
				/* xmlCleanupParser (); to be included? */
			}
			break;
		case FORMAT_CSV:
			if (model->priv->extract.csv.encoding) {
				g_free (model->priv->extract.csv.encoding);
				model->priv->extract.csv.encoding = NULL;
			}
			if (model->priv->extract.csv.delimiter) {
				g_free (model->priv->extract.csv.delimiter);
				model->priv->extract.csv.delimiter = NULL;
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

		if (model->priv->iter) {
			g_object_unref (model->priv->iter);
			model->priv->iter = NULL;
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
	GdaParameter *param;

	param = gda_parameter_list_find_param (model->priv->options, pname);
	if (param) {
		const GValue *value;
		
		value = gda_parameter_get_value (param);
		if (value && !gda_value_is_null ((GValue *) value)) {
			if (!gda_value_isa ((GValue *) value, G_TYPE_STRING))
				g_warning (_("The '%s' parameter must hold a "
					     "string value, ignored."), pname);
			else
				return g_value_get_string ((GValue *) value);	
		}
	}

	return NULL;
}

static gboolean
find_option_as_boolean (GdaDataModelImport *model, const gchar *pname, gboolean defaults)
{
	GdaParameter *param;
	param = gda_parameter_list_find_param (model->priv->options, pname);
	if (param) {
		const GValue *value;
		
		value = gda_parameter_get_value (param);
		if (value && !gda_value_is_null ((GValue *) value)) {
			if (!gda_value_isa ((GValue *) value, G_TYPE_BOOLEAN))
				g_warning (_("The '%s' parameter must hold a "
					     "boolean value, ignored."), pname);
			else
				return g_value_get_boolean ((GValue *) value);	
		}
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
	gpointer data;

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
			model->priv->src.mapped.fd = open (model->priv->src.mapped.filename, O_RDONLY);
			if (model->priv->src.mapped.fd < 0) {
				/* error */
				add_error (model, sys_errlist [errno]);
				return;
			}
			else {
				/* file mmaping */
				struct stat _stat;

				if (fstat (model->priv->src.mapped.fd, &_stat) < 0) {
					/* error */
					add_error (model, sys_errlist [errno]);
					return;
				}
				model->priv->src.mapped.length = _stat.st_size;
				model->priv->src.mapped.start = mmap (NULL, model->priv->src.mapped.length,
								      PROT_READ, MAP_PRIVATE, 
								      model->priv->src.mapped.fd, 0);
				if (model->priv->src.mapped.start == MAP_FAILED) {
					/* error */
					add_error (model, sys_errlist [errno]);
					model->priv->src.mapped.start = NULL;
					return;
				}
				model->priv->data_start = model->priv->src.mapped.start;
				model->priv->data_length = model->priv->src.mapped.length;
			}
			break;
		case PROP_DATA:
			string = g_value_get_string (value);
			if (!string)
				return;
			model->priv->is_mapped = FALSE;
			model->priv->src.string = g_strdup (g_value_get_string (value));
			model->priv->data_start = model->priv->src.string;
			model->priv->data_length = strlen (model->priv->src.string);
			break;
		case PROP_XML_NODE:
			data = g_value_get_pointer (value);
			if (!data)
				return;
			model->priv->format = FORMAT_XML_NODE;
			model->priv->extract.node.node = g_value_get_pointer (value);
			break;
		case PROP_OPTIONS:
			model->priv->options = g_value_get_pointer (value);
			if (model->priv->options) {
				if (!GDA_IS_PARAMETER_LIST (model->priv->options)) {
					g_warning (_("\"options\" property is not a GdaParameterList object"));
					model->priv->options = NULL;
				}
				else
					g_object_ref (model->priv->options);
			}
			return;
		default:
			g_assert_not_reached ();
			break;
		}
	}

	/* here we now have a valid data to analyse, try to determine the real kind of data 
	 * (CVS text of XML) */
	if (model->priv->format != FORMAT_XML_NODE) {
		g_assert (model->priv->data_start);
		if (!strncmp (model->priv->data_start, "<?xml", 5)) 
			model->priv->format = FORMAT_XML_DATA;
		else 
			model->priv->format = FORMAT_CSV;
	}

	/* analyse common options and init. */
	switch (model->priv->format) {
	case FORMAT_XML_DATA:
			init_xml_import (model);
			break;
			
	case FORMAT_CSV:
		model->priv->extract.csv.escape_char = '"';
		if (model->priv->options) {
			const gchar *option;
			option = find_option_as_string (model, "ENCODING");
			if (option) 
				model->priv->extract.csv.encoding = g_strdup (option);
			option = find_option_as_string (model, "SEPARATOR");
			if (option)
				model->priv->extract.csv.delimiter = g_strdup (option);
			model->priv->extract.csv.escape_char = '"';
			option = find_option_as_string (model, "ESCAPE_CHAR");
			if (option)
				model->priv->extract.csv.escape_char = *option;
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

	/* for random access, create a new GdaDataModelArray model and copy the contents from this model */
	if (model->priv->random_access && model->priv->columns && !model->priv->random_access_model) {
		GdaDataModel *ramodel;

		ramodel = gda_data_access_wrapper_new ((GdaDataModel *) model);		
		model->priv->random_access_model = ramodel;
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
		case PROP_DATA:
			if (model->priv->is_mapped)
				g_value_set_string (value, NULL);
			else
				g_value_set_string (value, model->priv->src.string);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
}

/**
 * gda_data_model_import_new_file
 * @filename: the file to import data from
 * @random_access: TRUE if a random acces will be required
 * @options: list of options for the export
 *
 * Creates a new #GdaDataModel object which contains the data stored within the @filename file.
 *
 * Returns: a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_import_new_file   (const gchar *filename, gboolean random_access, GdaParameterList *options)
{
	GdaDataModelImport *model;

	g_return_val_if_fail (filename, NULL);
	
	model = g_object_new (GDA_TYPE_DATA_MODEL_IMPORT,
			      "dict", ASSERT_DICT (NULL),
			      "random_access", random_access,
			      "options", options,
			      "filename", filename, NULL);

	return GDA_DATA_MODEL (model);
}

/**
 * gda_data_model_import_new_mem
 * @data: a string containng the data to import
 *
 * Creates a new #GdaDataModel object which contains the data stored in the @data string. 
 *
 * Important note: the @data string is not copied for memory efficiency reasons and should not
 * therefore be altered in any way as long as the returned data model exists.
 *
 * Returns: a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_import_new_mem (const gchar *data, gboolean random_access, GdaParameterList *options)
{
	GdaDataModelImport *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_IMPORT,
			      "dict", ASSERT_DICT (NULL),
			      "random_access", random_access,
			      "options", options,
			      "data", data, NULL);

	return GDA_DATA_MODEL (model);
}

/**
 * gda_data_model_import_new_xml_node
 * @node: an XML node corresponding to a &lt;data-array&gt; tag
 *
 * Creates a new #GdaDataModel and loads the data in @node. The resulting data model
 * can be accessed in a random way.
 *
 * Returns: a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_import_new_xml_node (xmlNodePtr node)
{
	GdaDataModelImport *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_IMPORT,
			      "dict", ASSERT_DICT (NULL),
			      "xml_node", node, NULL);

	return GDA_DATA_MODEL (model);
}

#ifdef GDA_DEBUG

static void
_dump_row (GdaDataModelIter *iter, gint n_cols, gchar *stroff)
{
	gint i;
	gchar *sep_col  = " | ";

	for (i = 0; i < n_cols; i++) {
		GdaParameter *param;
		gchar *str;

		param = gda_data_model_iter_get_param_for_column (iter, i);
		str = gda_value_stringify ((GValue *) gda_parameter_get_value (param));
		if (i != 0)
			g_print ("%s%s", sep_col, str);
		else
			g_print ("%s%s", stroff, str);		
		g_free (str);
	}
	g_print ("\n");
}

static void
gda_data_model_import_dump (GdaDataModelImport *model, guint offset)
{
	gchar *stroff;

	stroff = g_new0 (gchar, offset+1);
	memset (stroff, ' ', offset);

	if (model->priv) {
		GdaDataModelIter *iter;
		gint n_cols, i;
		gchar *sep_col  = " | ";
		gchar *sep_row  = "-+-";
		gchar sep_fill = '-';
		
		g_print ("%s" D_COL_H1 "GdaDataModelImport %p (name=%s, id=%s)\n" D_COL_NOR,
			 stroff, model, gda_object_get_name (GDA_OBJECT (model)), 
			 gda_object_get_id (GDA_OBJECT (model)));
		
		/* columns */
		n_cols = gda_data_model_get_n_columns (GDA_DATA_MODEL (model));
		for (i = 0; i < n_cols; i++) {
			GdaColumn *col = gda_data_model_describe_column (GDA_DATA_MODEL (model), i);
                        g_print ("%sModel col %d has type %s\n", stroff, i, 
				 gda_type_to_string (gda_column_get_gda_type (col)));
		}

		for (i = 0; i < n_cols; i++) {
			const gchar *str;

			str = (gchar *) gda_data_model_get_column_title (GDA_DATA_MODEL (model), i);
			if (i != 0)
				g_print ("%s%s", sep_col, str);
			else
				g_print ("%s%s", stroff, str);
		}
		g_print ("\n");

		/* data */
		for (i = 0; i < n_cols; i++) {
			gint j;
			if (i != 0)
				g_print ("%s", sep_row);
			else
				g_print ("%s", stroff);
			for (j = 0; j < 10; j++)
				g_print ("%c", sep_fill);
		}
		g_print ("\n");

		iter = gda_data_model_create_iter (GDA_DATA_MODEL (model));
		if (gda_data_model_iter_is_valid (iter)) {
			g_print ("%sSome parts of the model may be missing...\n", stroff);
			_dump_row (iter, n_cols, stroff);
		}
		while (gda_data_model_iter_move_next (iter))
			_dump_row (iter, n_cols, stroff);
		g_object_unref (iter);
        }
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, stroff, model);

	g_free (stroff);
}
#endif


/*
 *
 * CSV treatments
 *
 */

static gchar **csv_split_line (GdaDataModelImport *model);
static void    csv_find_EOL (GdaDataModelImport *model);
static gint    cvs_count_nb_columns (GdaDataModelImport *model);
static void    csv_compute_row_values (GdaDataModelImport *model);
static void    csv_fetch_next_row (GdaDataModelImport *model);
static void    csv_fetch_prev_row (GdaDataModelImport *model);

static void
init_csv_import (GdaDataModelImport *model)
{
	gboolean title_first_line = FALSE;
	gint nbcols;
	
	if (model->priv->options) 
		title_first_line = find_option_as_boolean (model, "TITLE_AS_FIRST_LINE", FALSE);

	g_assert (model->priv->format == FORMAT_CSV);

	if (!model->priv->extract.csv.delimiter) 
		model->priv->extract.csv.delimiter = g_strdup (",");

	model->priv->extract.csv.text_line = 1; /* start line numbering at 1 */
	model->priv->extract.csv.pos = CSV_AT_START;

	/* columns count */
	nbcols = 0;
	
	model->priv->extract.csv.start_pos = model->priv->data_start;
	model->priv->extract.csv.end_pos = model->priv->data_start + model->priv->data_length;
	
	model->priv->extract.csv.line_start = model->priv->extract.csv.start_pos;
	csv_find_EOL (model);
	nbcols = cvs_count_nb_columns (model);

	/* computing columns */
	if (nbcols > 0) {
		gchar **arr = NULL, **arrvalue = NULL;
		gint col;
		GdaDict *dict;

		dict = gda_object_get_dict (GDA_OBJECT (model));

		if (title_first_line) {
			model->priv->extract.csv.line_start = model->priv->extract.csv.start_pos;
			csv_find_EOL (model);
			
			arr = csv_split_line (model);
			arrvalue = arr;
		}

		for (col = 0; col < nbcols; col++) {
			GdaColumn *column;

			column = gda_column_new ();
			model->priv->columns = g_slist_append (model->priv->columns , column);

			if (arrvalue) {
				gda_column_set_name (column, *arrvalue);
				gda_column_set_title (column, *arrvalue);
				gda_column_set_caption (column, *arrvalue);
				arrvalue ++;
			}

			gda_column_set_gda_type (column, G_TYPE_STRING);
			if (model->priv->options) {
				gchar *pname;
				GdaParameter *param;
				const gchar *dbms_t;

				pname = g_strdup_printf ("GDA_TYPE_%d", col);
				param = gda_parameter_list_find_param (model->priv->options, pname);
				if (param) {
					const GValue *value;
					
					value = gda_parameter_get_value (param);
					if (value && !gda_value_is_null ((GValue *) value)) {
						if (!gda_value_isa ((GValue *) value, G_TYPE_ULONG))
							g_warning (_("The '%s' parameter must hold a "
								     "gda type value, ignored."), pname);
						else {
							GType gtype;
							
							gtype = g_value_get_ulong ((GValue *) value);
							gda_column_set_gda_type (column, gtype);
						}
					}
				}
				g_free (pname);

				pname = g_strdup_printf ("DBMS_TYPE_%d", col);
				dbms_t = find_option_as_string (model, pname);
				if (dbms_t) {
					GdaDictType *dtype;

					gda_column_set_dbms_type (column, dbms_t);
					dtype = gda_dict_get_dict_type_by_name (dict, dbms_t);
					if (dtype) {
						GType gtype;
						
						gtype = gda_dict_type_get_gda_type (dtype);
						gda_column_set_gda_type (column, gtype);
					}
				}
				g_free (pname);
			}
		}

		if (title_first_line) {
			g_strfreev (arr);

			model->priv->extract.csv.start_pos = model->priv->extract.csv.line_end + 1;
			model->priv->extract.csv.line_start = model->priv->extract.csv.start_pos;
			model->priv->extract.csv.text_line ++;
		}

		model->priv->extract.csv.line_end = model->priv->extract.csv.line_start - 1;
	}
}

/* splits the string between model->priv->extract.csv.line_start and model->priv->extract.csv.line_end
 * into chuncks. use g_strfreev() to free it.
 */
static gchar **
csv_split_line (GdaDataModelImport *model)
{
	gchar *line = NULL;
	gchar **arr = NULL;
	gchar *ptr, *ch_start;
	gchar esc = model->priv->extract.csv.escape_char; /* 0 if not set */
	gchar *delim = model->priv->extract.csv.delimiter;
	GSList *chuncks = NULL;
	gint nb_chuncks = 0;
	gboolean in;

	g_assert (delim);
	if (model->priv->extract.csv.line_end == model->priv->extract.csv.line_start)
		return NULL;

	if (model->priv->extract.csv.encoding) {
		GError *error = NULL;
		line = g_convert (model->priv->extract.csv.line_start,
				  model->priv->extract.csv.line_end - model->priv->extract.csv.line_start,
				  "UTF-8", model->priv->extract.csv.encoding,
				  NULL, NULL, &error);
		if (!line) {
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
		line = g_locale_to_utf8 (model->priv->extract.csv.line_start,
					 model->priv->extract.csv.line_end - model->priv->extract.csv.line_start,
					 NULL, NULL, NULL);
	
	if (!line)
		line = g_strndup (model->priv->extract.csv.line_start, 
				  model->priv->extract.csv.line_end - model->priv->extract.csv.line_start);
	
	/* g_print ("\n\n(delim=%c/esc=%d) line = #%s#\n", *delim, esc, line); */

	ptr = line;
	ch_start = ptr;
	in = FALSE;
	while (*ptr) {
		if (!in) {
			if (*ptr == esc) {
				ptr ++;
				in = TRUE;
				ch_start = ptr;
			}
			else {
				if (*ptr == *delim) {
					in = FALSE;
					if (ptr > ch_start)
						chuncks = g_slist_prepend (chuncks,
									   g_strndup (ch_start, ptr - ch_start));
					else
						chuncks = g_slist_prepend (chuncks, g_strdup (""));
					nb_chuncks ++;

					ptr++;
					ch_start = ptr;
				}
				else
					ptr++;
			}
		}
		else {
			if (*ptr == esc) {
				if (*(ptr+1) == esc)
					ptr += 2;
				else {
					ptr++;
					in = FALSE;
					if (ptr -1 > ch_start)
						chuncks = g_slist_prepend (chuncks,
									   g_strndup (ch_start, ptr - 1 - ch_start));
					else
						chuncks = g_slist_prepend (chuncks, g_strdup (""));
					nb_chuncks ++;
					/* ignore everything up to the next separator */
					while (*ptr && (*ptr != *delim))
						ptr++;
					if (*ptr)
						ptr++;
					ch_start = ptr;
				}
			}
			else
				ptr++;
		}
	}
	if (ch_start < ptr) {
		chuncks = g_slist_prepend (chuncks,
					   g_strndup (ch_start, ptr - ch_start));
		nb_chuncks ++;
	}

	if (*(ptr-1) == *delim) {
		chuncks = g_slist_prepend (chuncks, g_strdup (""));
		nb_chuncks ++;
	}
	
	/* g_print ("nb_chuncks: %d\n", nb_chuncks); */
	if (chuncks) {
		GSList *list = chuncks;
		gint index = nb_chuncks - 1;
		arr = g_new0 (gchar *, nb_chuncks+1);

		while (list) {
			gchar *ch = (gchar *) list->data;
			gint len = strlen (ch);
			
			ptr = ch;
			while (*ptr) {
				/* convert double escape char to a single one */
				if ((*ptr == esc) && (*(ptr+1) == esc))
					g_memmove (ptr, ptr+1, len);

				len--;
				ptr++;
			}

			arr [index] = ch;
			/* g_print ("Chunck%02d=#%s#\n", index, ch); */

			list = g_slist_next (list);
			index --;
		}

		g_slist_free (chuncks);
	}

	g_free (line);

	return arr;
}

/* may modify model->priv->extract.csv.line_start AND model->priv->extract.csv.line_end */
static void
csv_find_EOL (GdaDataModelImport *model)
{
	gchar *ptr;
	
	ptr = model->priv->extract.csv.line_start;
	if (ptr >= model->priv->extract.csv.end_pos)
		ptr = model->priv->extract.csv.end_pos;

	while ((ptr < model->priv->extract.csv.end_pos) && (*ptr == '\n')) {
		ptr++;
		model->priv->extract.csv.text_line ++;
	}
	model->priv->extract.csv.line_start = ptr;

	if (ptr < model->priv->extract.csv.end_pos)
		ptr++;
	while ((ptr < model->priv->extract.csv.end_pos) && (*ptr != '\n'))
		ptr++;
	model->priv->extract.csv.line_end = ptr;
}

static gint
cvs_count_nb_columns (GdaDataModelImport *model)
{
	gint nb_columns = -1;

	if (model->priv->extract.csv.line_start != model->priv->extract.csv.line_end) {
		gchar **value;
		gchar **arr;
		nb_columns = 0;
		
		arr = csv_split_line (model);
		
		value = arr;
		while (*value) {
			nb_columns ++;
			value ++;
		}
		
		g_strfreev (arr);
	}

	return nb_columns;
}

/* the model->priv->extract.csv.line_start and model->priv->extract.csv.line_end pointers
 * MUST be set to represent a whole line */
static void
csv_compute_row_values (GdaDataModelImport *model)
{
	gchar **arr = NULL, **arrvalue = NULL;
	GSList *columns = model->priv->columns;
	GSList *values = NULL;

	if (model->priv->cursor_values) {
		g_slist_foreach (model->priv->cursor_values, (GFunc) gda_value_free, NULL);
		g_slist_free (model->priv->cursor_values);
		model->priv->cursor_values = NULL;
	}

	if (model->priv->extract.csv.line_start == model->priv->extract.csv.line_end)
		return;

	arr = csv_split_line (model);

	arrvalue = arr;
	while (*arrvalue && columns) {
		GType gtype;
		GValue *value;

		gtype = gda_column_get_gda_type ((GdaColumn *) columns->data);
		value = gda_value_new_from_string (*arrvalue, gtype);
		if (!value) {
			gchar *str;

			str = g_strdup_printf (_("Could not convert '%s' to a value of type %s"), 
					       *arrvalue, gda_type_to_string (gtype));
			add_error (model, str);
			g_free (str);
			value = gda_value_new_null ();
		}
		values = g_slist_prepend (values, value);
		
		arrvalue ++;
		columns = g_slist_next (columns);
	}

	if (*arrvalue) {
		/* there are more value than columns, error */
		gchar *str;
		
		str = g_strdup_printf (_("Row has more values than detected at line %d"),
				       model->priv->extract.csv.text_line);
		add_error (model, str);
		g_free (str);
	}
	
	g_strfreev (arr);
	model->priv->cursor_values = g_slist_reverse (values);
}

static void
csv_fetch_next_row (GdaDataModelImport *model)
{
	model->priv->extract.csv.line_start = model->priv->extract.csv.line_end + 1;
	csv_find_EOL (model);
	model->priv->extract.csv.text_line ++;
	
	csv_compute_row_values (model);
}

static void
csv_fetch_prev_row (GdaDataModelImport *model)
{
	gchar *ptr;

	ptr = model->priv->extract.csv.line_start - 1;
	if (ptr < model->priv->extract.csv.start_pos) {
		if (model->priv->cursor_values) {
			g_slist_foreach (model->priv->cursor_values, (GFunc) gda_value_free, NULL);
			g_slist_free (model->priv->cursor_values);
			model->priv->cursor_values = NULL;
		}
		model->priv->extract.csv.line_end = model->priv->extract.csv.line_start - 1;
	}
	else {
		while ((ptr >= model->priv->extract.csv.start_pos) && (*ptr == '\n')) {
			model->priv->extract.csv.text_line --;
			ptr --;
		}
		while ((ptr >= model->priv->extract.csv.start_pos) && (*ptr != '\n'))
			ptr --;
		model->priv->extract.csv.line_start = ptr;
		csv_find_EOL (model);
		model->priv->extract.csv.text_line --;

		csv_compute_row_values (model);
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

	list = fields;
	while (list) {
		spec = (XmlColumnSpec*)(list->data);
		xmlFree (spec->id);
		xmlFree (spec->name);
		xmlFree (spec->title);
		xmlFree (spec->caption);
		xmlFree (spec->dbms_type);
		xmlFree (spec->table);
		xmlFree (spec->ref);
		xmlFree (spec);

		list = g_slist_next (list);
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
		if (strcmp (name, "gda_array")) {
			/* error */
			gchar *str;

			str = g_strdup_printf (_("Expected <gda_array> node in XML file, got <%s>"), name);
			add_error (model, str);
			g_free (str);
			xmlFreeTextReader (reader);
			model->priv->extract.xml.reader = NULL;
		}
		node = xmlTextReaderCurrentNode (reader);
		
		prop = xmlGetProp (node, "id");
		if (prop) {
			gda_object_set_id (GDA_OBJECT (model), prop);
			xmlFree (prop);
		}
		prop = xmlGetProp (node, "name");
		if (prop) {
			gda_object_set_name (GDA_OBJECT (model), prop);
			xmlFree (prop);
		}

		prop = xmlGetProp (node, "descr");
		if (prop) {
			gda_object_set_description (GDA_OBJECT (model), prop);
			xmlFree (prop);
		}
		
		/* compute fields */
		ret = xml_fetch_next_xml_node (reader);
		name = (ret > 0) ? xmlTextReaderConstName (reader) : NULL;
		while (name && !strcmp (name, "gda_array_field")) {
			XmlColumnSpec *spec;
			gchar *str;

			spec = g_new0 (XmlColumnSpec, 1);
			fields = g_slist_append (fields, spec);
			
			spec->id = xmlTextReaderGetAttribute (reader, "id");
			spec->name = xmlTextReaderGetAttribute (reader, "name");
			spec->title = xmlTextReaderGetAttribute (reader, "title");
			if (!spec->title && spec->name)
				spec->title = g_strdup (spec->name);
			
			spec->caption = xmlTextReaderGetAttribute (reader, "caption");
			spec->dbms_type = xmlTextReaderGetAttribute (reader, "dbms_type");
			str = xmlTextReaderGetAttribute (reader, "gdatype");
			if (str) {
				spec->gdatype = gda_type_from_string (str);
				xmlFree (str);
			}
			else {
				add_error (model, _("No \"gdatype\" attribute specified in <gda_array_field>"));
				clean_field_specs (fields);
				xmlFreeTextReader (reader);
				model->priv->extract.xml.reader = NULL;
				return;
			}
			str = xmlTextReaderGetAttribute (reader, "size");
			if (str) {
				spec->size = atoi (str);
				xmlFree (str);
			}
			str = xmlTextReaderGetAttribute (reader, "scale");
			if (str) {
				spec->scale = atoi (str);
				xmlFree (str);
			}
			str = xmlTextReaderGetAttribute (reader, "pkey");
			if (str) {
				spec->pkey = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = xmlTextReaderGetAttribute (reader, "unique");
			if (str) {
				spec->unique = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = xmlTextReaderGetAttribute (reader, "nullok");
			spec->nullok = TRUE;
			if (str) {
				spec->nullok = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = xmlTextReaderGetAttribute (reader, "auto_increment");
			if (str) {
				spec->autoinc = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			spec->table = xmlTextReaderGetAttribute (reader, "table");
			spec->ref = xmlTextReaderGetAttribute (reader, "ref");
			
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
		
		list = fields;
		pos = 0;
		while (list) {
			GdaColumn *column;
			XmlColumnSpec *spec;
			
			spec = (XmlColumnSpec *)(list->data);
			column = gda_column_new ();
			model->priv->columns = g_slist_append (model->priv->columns, column);
			g_object_set (G_OBJECT (column), "id", spec->id, NULL);
			gda_column_set_title (column, spec->title);
			gda_column_set_name (column, spec->name);
			gda_column_set_defined_size (column, spec->size);
			gda_column_set_caption (column, spec->caption);
			gda_column_set_dbms_type (column, spec->dbms_type);
			gda_column_set_scale (column, spec->scale);
			gda_column_set_gda_type (column, spec->gdatype);
			gda_column_set_allow_null (column, spec->nullok);
			gda_column_set_primary_key (column, spec->pkey);
			gda_column_set_unique_key (column, spec->unique);
			gda_column_set_table (column, spec->table);
			gda_column_set_references (column, spec->ref);
			
			list = g_slist_next (list);
			pos++;
		}
		clean_field_specs (fields);

		/* move to find the <gda_array_data> tag */
		name = (ret > 0) ? xmlTextReaderConstName (reader) : NULL;
		if (!name || strcmp (name, "gda_array_data")) {
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

	const gchar *lang = NULL;

	GSList *columns = model->priv->columns;
	GdaColumn *last_column = NULL;
	GSList *values = NULL;

#ifdef HAVE_LC_MESSAGES
	lang = setlocale (LC_MESSAGES, NULL);
#else
	lang = setlocale (LC_CTYPE, NULL);
#endif

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
	if (!name || strcmp (name, "gda_array_row")) {
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
	while (name && !strcmp (name, "gda_value")) {
		/* ignore this <gda_value> if the "lang" is there and is not the user locale */
		xmlChar *this_lang;
		this_lang = xmlTextReaderGetAttribute (reader, "xml:lang");
		if (this_lang && strncmp (this_lang, lang, strlen (this_lang))) {
			xmlFree (this_lang);
			ret = xml_fetch_next_xml_node (reader);
			name = (ret > 0) ? xmlTextReaderConstName (reader) : NULL;
			continue;
		}

		/* use this <gda_value> */
		if (!columns) 
			add_error (model, _("Row has too many values (which are ignored)"));
		else {
			ret = xmlTextReaderRead (reader);
			if (ret > 0) {
				GValue *value;
				GdaColumn *column;

				if (this_lang)
					column = last_column;
				else {
					column = (GdaColumn *) columns->data;
					last_column = column;
					columns = g_slist_next (columns);
				}
				if (xmlTextReaderNodeType (reader) == XML_TEXT_NODE) {
					GType gtype;
					const xmlChar *xmlstr;

					gtype = gda_column_get_gda_type (column);
					xmlstr = xmlTextReaderConstValue (reader);
					/* g_print ("Convert #%s# to %s\n", (gchar *) xmlstr, gda_type_to_string (gtype)); */
					value = gda_value_new_from_string ((gchar *) xmlstr, gtype);
					if (!value) {
						gchar *str;
						
						str = g_strdup_printf (_("Could not convert '%s' to a value of type %s"), 
								       (gchar *) xmlstr, gda_type_to_string (gtype));
						add_error (model, str);
						g_free (str);
						value = gda_value_new_null ();
					}
				}
				else 
					/* no contents => this is a NULL value */
					value = gda_value_new_null ();

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
		ret = xml_fetch_next_xml_node (reader);
		name = (ret > 0) ? xmlTextReaderConstName (reader) : NULL;
	}

	if (values)
		model->priv->cursor_values = g_slist_reverse (values);

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

	if (strcmp (node->name, "gda_array")) {
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
		if (!strcmp (cur->name, "gda_array_field")) {
			XmlColumnSpec *spec;

			spec = g_new0 (XmlColumnSpec, 1);
			fields = g_slist_append (fields, spec);
			
			spec->id = xmlGetProp (cur, "id");
			spec->name = xmlGetProp (cur, "name");
			spec->title = xmlGetProp (cur, "title");
			if (!spec->title && spec->name)
				spec->title = g_strdup (spec->name);

			spec->caption = xmlGetProp (cur, "caption");
			spec->dbms_type = xmlGetProp (cur, "dbms_type");
			str = xmlGetProp (cur, "gdatype");
			if (str) {
				spec->gdatype = gda_type_from_string (str);
				xmlFree (str);
			}
			else {
				add_error (model, _("No \"gdatype\" attribute specified in <gda_array_field>"));
				clean_field_specs (fields);
				node = model->priv->extract.node.node = NULL;
				return;
			}
			str = xmlGetProp (cur, "size");
			if (str) {
				spec->size = atoi (str);
				xmlFree (str);
			}
			str = xmlGetProp (cur, "scale");
			if (str) {
				spec->scale = atoi (str);
				xmlFree (str);
			}
			str = xmlGetProp (cur, "pkey");
			if (str) {
				spec->pkey = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = xmlGetProp (cur, "unique");
			if (str) {
				spec->unique = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = xmlGetProp (cur, "nullok");
			spec->nullok = TRUE;
			if (str) {
				spec->nullok = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			str = xmlGetProp (cur, "auto_increment");
			if (str) {
				spec->autoinc = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				xmlFree (str);
			}
			spec->table = xmlGetProp (cur, "table");
			spec->ref = xmlGetProp (cur, "ref");

			nbfields ++;
			continue;
		}
		if (!strcmp (cur->name, "gda_array_data"))
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
	str = xmlGetProp (node, "id");
	if (str) {
		/* FIXME: use str+2 only if str is like "DA%d", otherwise simply use str */
		gda_object_set_id (GDA_OBJECT (model), str+2);
		xmlFree (str);
	}
	str = xmlGetProp (node, "name");
	if (str) {
		gda_object_set_name (GDA_OBJECT (model), str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "descr");
	if (str) {
		gda_object_set_description (GDA_OBJECT (model), str);
		xmlFree (str);
	}

	list = fields;
	pos = 0;
	while (list) {
		GdaColumn *column;
		XmlColumnSpec *spec;

		spec = (XmlColumnSpec *)(list->data);
		column = gda_data_model_describe_column (ramodel, pos);
		g_object_set (G_OBJECT (column), "id", spec->id, NULL);
		gda_column_set_title (column, spec->title);
		gda_column_set_name (column, spec->name);
		gda_column_set_defined_size (column, spec->size);
		gda_column_set_caption (column, spec->caption);
		gda_column_set_dbms_type (column, spec->dbms_type);
		gda_column_set_scale (column, spec->scale);
		gda_column_set_gda_type (column, spec->gdatype);
		gda_column_set_allow_null (column, spec->nullok);
		gda_column_set_primary_key (column, spec->pkey);
		gda_column_set_unique_key (column, spec->unique);
		gda_column_set_table (column, spec->table);
		gda_column_set_references (column, spec->ref);

		model->priv->columns = g_slist_prepend (model->priv->columns, gda_column_copy (column));

		list = g_slist_next (list);
		pos++;
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

	g_set_error (&error, 0, 0, err);
	model->priv->errors = g_slist_append (model->priv->errors, error);
}

/**
 * gda_data_model_import_get_errors
 * @model: a #GdaDataModelImport object
 *
 * Get the list of errors which @model has to report. The returned list is a list of
 * #GError structures, and must not be modified
 *
 * Returns: the list of errors (which must not be modified), or %NULL
 */
GSList *
gda_data_model_import_get_errors (GdaDataModelImport *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), NULL);
	g_return_val_if_fail (model->priv, NULL);

	return model->priv->errors;
}

/**
 * gda_data_model_import_clean_errors
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

static guint
gda_data_model_import_get_access_flags (GdaDataModel *model)
{
	GdaDataModelImport *imodel;
	guint flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), 0);
	imodel = GDA_DATA_MODEL_IMPORT (model);
	g_return_val_if_fail (imodel->priv, 0);
	
	if (imodel->priv->format == FORMAT_CSV)
		flags |= GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD;

	if (imodel->priv->random_access && imodel->priv->random_access_model)
		flags |= GDA_DATA_MODEL_ACCESS_RANDOM;

	return flags;
}

static const GValue *
gda_data_model_import_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaDataModelImport *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), NULL);
	imodel = GDA_DATA_MODEL_IMPORT (model);
	g_return_val_if_fail (imodel->priv, NULL);

	if (imodel->priv->random_access_model)
		/* if there is a random access model, then use it */
		return gda_data_model_get_value_at (imodel->priv->random_access_model, col, row);
	else
		/* otherwise, bail out */
		return NULL;
}

static guint
gda_data_model_import_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	guint flags = 0;
	GdaDataModelImport *imodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), 0);
	imodel = (GdaDataModelImport *) model;
	g_return_val_if_fail (imodel->priv, 0);
	
	flags = GDA_VALUE_ATTR_NO_MODIF;
	
	return flags;
}

static GdaDataModelIter *
gda_data_model_impor_create_iter (GdaDataModel *model)
{
	GdaDataModelImport *imodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), NULL);
	imodel = (GdaDataModelImport *) model;
	g_return_val_if_fail (imodel->priv, NULL);
	
	if (! imodel->priv->iter) {
		imodel->priv->iter = (GdaDataModelIter *) g_object_new (GDA_TYPE_DATA_MODEL_ITER, 
									"dict", gda_object_get_dict (GDA_OBJECT (model)), 
									"data_model", model, NULL);
		g_object_ref (imodel->priv->iter);
	}

	return imodel->priv->iter;
}

static void 
add_error_too_few_values (GdaDataModelImport *model)
{
	gchar *str;

	switch (model->priv->format){
	case FORMAT_CSV:
		str = g_strdup_printf (_("Row at line %d does not have enough values, "
					 "completed with NULL values"),
				       model->priv->extract.csv.text_line);
		add_error (model, str);
		g_free (str);
		break;
	default:
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
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL_IMPORT (model), FALSE);
	imodel = (GdaDataModelImport *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	/* if there is a random access model, then use it */
	if (imodel->priv->format == FORMAT_XML_NODE)
		return gda_data_model_move_iter_next_default (model, iter);

	/* fetch the next row if necessary */
	switch (imodel->priv->format) {
	case FORMAT_XML_DATA:
		xml_fetch_next_row (imodel);
		break;

	case FORMAT_CSV:
		if (gda_data_model_iter_is_valid (iter) || (imodel->priv->extract.csv.pos == CSV_AT_START))
			csv_fetch_next_row (imodel);
		break;
	default:
		g_assert_not_reached ();
	}

	/* set values in iter */
	if (imodel->priv->cursor_values) {
		GSList *plist;
		GSList *vlist;
		gboolean update_model;
		
		g_object_get (G_OBJECT (iter), "update_model", &update_model, NULL);
		g_object_set (G_OBJECT (iter), "update_model", FALSE, NULL);
		plist = ((GdaParameterList *) iter)->parameters;
		vlist = imodel->priv->cursor_values;
		while (plist && vlist) {
			gda_parameter_set_value (GDA_PARAMETER (plist->data), 
						 (GValue *) vlist->data);
			plist = g_slist_next (plist);
			vlist = g_slist_next (vlist);
		}
		if (plist || vlist) {
			if (plist) {
				add_error_too_few_values (imodel);
				while (plist) {
					gda_parameter_set_value (GDA_PARAMETER (plist->data), NULL);
					plist = g_slist_next (plist);
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
			      "update_model", update_model, NULL);
		if (imodel->priv->format == FORMAT_CSV)
			imodel->priv->extract.csv.pos = CSV_IN_DATA;
		return TRUE;
	}
	else {
		g_signal_emit_by_name (iter, "end_of_data");
		g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
		if (imodel->priv->format == FORMAT_CSV)
			imodel->priv->extract.csv.pos = CSV_AT_END;
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
		return gda_data_model_move_iter_prev_default (model, iter);

	/* fetch the previous row if necessary */
	switch (imodel->priv->format) {
	case FORMAT_CSV:
		if (gda_data_model_iter_is_valid (iter) || (imodel->priv->extract.csv.pos == CSV_AT_END))
			csv_fetch_prev_row (imodel);
		break;
	default:
		g_assert_not_reached ();
	}

	/* set values in iter */
	if (imodel->priv->cursor_values) {
		GSList *plist;
		GSList *vlist;
		gboolean update_model;
		
		g_object_get (G_OBJECT (iter), "update_model", &update_model, NULL);
		g_object_set (G_OBJECT (iter), "update_model", FALSE, NULL);
		plist = ((GdaParameterList *) iter)->parameters;
		vlist = imodel->priv->cursor_values;
		while (plist && vlist) {
			gda_parameter_set_value (GDA_PARAMETER (plist->data), 
						 (GValue *) vlist->data);
			plist = g_slist_next (plist);
			vlist = g_slist_next (vlist);
		}
		if (plist || vlist) {
			if (plist) {
				add_error_too_few_values (imodel);
				while (plist) {
					gda_parameter_set_value (GDA_PARAMETER (plist->data), NULL);
					plist = g_slist_next (plist);
				}				
			}
			else 
				add_error_too_many_values (imodel);
		}
		
		if (gda_data_model_iter_is_valid (iter))
			imodel->priv->iter_row --;

		g_assert (imodel->priv->iter_row >= 0);
		g_object_set (G_OBJECT (iter), "current-row", imodel->priv->iter_row, 
			      "update_model", update_model, NULL);
		imodel->priv->extract.csv.pos = CSV_IN_DATA;
		return TRUE;
	}
	else {
		g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
		imodel->priv->extract.csv.pos = CSV_AT_START;
		return FALSE;
	}
}
