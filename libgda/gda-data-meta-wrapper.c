/* GDA library
 * Copyright (C) 2009 The GNOME Foundation.
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

#undef GDA_DISABLE_DEPRECATED
#include "gda-data-meta-wrapper.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-data-model.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>

/* we don't want to duplicate the symbols in <libgda/sqlite/keywords_hash.h>, so simply
 * declare them as external
 */
extern const unsigned char UpperToLower[];
#define charMap(X) UpperToLower[(unsigned char)(X)]
extern int casecmp(const char *zLeft, const char *zRight, int N);
#include "keywords_hash.c" /* this one is dynamically generated */

/*
 * Each value in @values may be %NULL, a valid pointer, or 0x1
 */
typedef struct {
	gint row; /* row number it is for */
	gint size; /* size of the @values array */
	GValue **values;
} CompRow;

#define NON_COMP_VALUE ((GValue*) 0x1)

static void
comp_row_free (CompRow *row)
{
	if (row->values) {
		gint i;
		for (i = 0; i < row->size; i++) {
			if (row->values [i] && (row->values [i] != NON_COMP_VALUE))
				gda_value_free (row->values [i]);
		}
		g_free (row->values);
	}
	g_free (row);
}

struct _GdaDataMetaWrapperPrivate {
	GdaDataModel *model;
	gint nb_cols;

	gint *cols_to_wrap;
	gint cols_to_wrap_size;
	GdaSqlIdentifierStyle mode;
	GdaSqlReservedKeywordsFunc reserved_keyword_func;

	GHashTable *computed_rows; /* key = row number, value = a CompRow pointer
				    * may be %NULL if we don't keep computed values */
	CompRow *buffer; /* may be %NULL if we keep computed values */

	/* note: either @computed_rows or @buffer is NULL, not both and one is not NULL */
};

/* properties */
enum
{
        PROP_0,
	PROP_MODEL,
};

static void gda_data_meta_wrapper_class_init (GdaDataMetaWrapperClass *klass);
static void gda_data_meta_wrapper_init       (GdaDataMetaWrapper *model,
					      GdaDataMetaWrapperClass *klass);
static void gda_data_meta_wrapper_dispose    (GObject *object);
static void gda_data_meta_wrapper_finalize   (GObject *object);

static void gda_data_meta_wrapper_set_property (GObject *object,
						    guint param_id,
						    const GValue *value,
						    GParamSpec *pspec);
static void gda_data_meta_wrapper_get_property (GObject *object,
						    guint param_id,
						    GValue *value,
						    GParamSpec *pspec);

/* GdaDataModel interface */
static void                 gda_data_meta_wrapper_data_model_init (GdaDataModelIface *iface);
static gint                 gda_data_meta_wrapper_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_meta_wrapper_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_meta_wrapper_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_meta_wrapper_get_access_flags(GdaDataModel *model);
static const GValue        *gda_data_meta_wrapper_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);

static GObjectClass *parent_class = NULL;

/**
 * gda_data_meta_wrapper_get_type
 *
 * Returns: the #GType of GdaDataMetaWrapper.
 */
GType
_gda_data_meta_wrapper_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaDataMetaWrapperClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_meta_wrapper_class_init,
			NULL,
			NULL,
			sizeof (GdaDataMetaWrapper),
			0,
			(GInstanceInitFunc) gda_data_meta_wrapper_init
		};

		static const GInterfaceInfo data_model_info = {
			(GInterfaceInitFunc) gda_data_meta_wrapper_data_model_init,
			NULL,
			NULL
		};

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaDataMetaWrapper", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_data_meta_wrapper_class_init (GdaDataMetaWrapperClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* properties */
	object_class->set_property = gda_data_meta_wrapper_set_property;
        object_class->get_property = gda_data_meta_wrapper_get_property;
	g_object_class_install_property (object_class, PROP_MODEL,
                                         g_param_spec_object ("model", NULL, "Data model being wrapped",
                                                              GDA_TYPE_DATA_MODEL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	/* virtual functions */
	object_class->dispose = gda_data_meta_wrapper_dispose;
	object_class->finalize = gda_data_meta_wrapper_finalize;

#ifdef GDA_DEBUG
	test_keywords ();
#endif
}

static void
gda_data_meta_wrapper_data_model_init (GdaDataModelIface *iface)
{
	iface->i_get_n_rows = gda_data_meta_wrapper_get_n_rows;
	iface->i_get_n_columns = gda_data_meta_wrapper_get_n_columns;
	iface->i_describe_column = gda_data_meta_wrapper_describe_column;
        iface->i_get_access_flags = gda_data_meta_wrapper_get_access_flags;
	iface->i_get_value_at = gda_data_meta_wrapper_get_value_at;
	iface->i_get_attributes_at = NULL;

	iface->i_create_iter = NULL;
	iface->i_iter_at_row = NULL;
	iface->i_iter_next = NULL;
	iface->i_iter_prev = NULL;

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
gda_data_meta_wrapper_init (GdaDataMetaWrapper *model, GdaDataMetaWrapperClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_META_WRAPPER (model));
	model->priv = g_new0 (GdaDataMetaWrapperPrivate, 1);
	model->priv->model = NULL;
	model->priv->cols_to_wrap = NULL;
	model->priv->cols_to_wrap_size = 0;
	model->priv->mode = GDA_DATA_META_WRAPPER_MODE_LC;
	model->priv->reserved_keyword_func = NULL;
	model->priv->computed_rows = NULL;
	model->priv->buffer = NULL;
}

static void
gda_data_meta_wrapper_dispose (GObject *object)
{
	GdaDataMetaWrapper *model = (GdaDataMetaWrapper *) object;

	g_return_if_fail (GDA_IS_DATA_META_WRAPPER (model));

	/* free memory */
	if (model->priv) {
		/* random meta model free */
		if (model->priv->model) {
			g_object_unref (model->priv->model);
			model->priv->model = NULL;
		}

		if (model->priv->computed_rows) {
			g_hash_table_destroy (model->priv->computed_rows);
			model->priv->computed_rows = NULL;
		}

		if (model->priv->buffer) {
			comp_row_free (model->priv->buffer);
			model->priv->buffer = NULL;
		}

		if (model->priv->cols_to_wrap) {
			g_free (model->priv->cols_to_wrap);
			model->priv->cols_to_wrap = NULL;
			model->priv->cols_to_wrap_size = 0;
		}
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_data_meta_wrapper_finalize (GObject *object)
{
	GdaDataMetaWrapper *model = (GdaDataMetaWrapper *) object;

	g_return_if_fail (GDA_IS_DATA_META_WRAPPER (model));

	/* free memory */
	if (model->priv) {
		g_free (model->priv);
		model->priv = NULL;
	}

	/* chain to parent class */
	parent_class->finalize (object);
}

static void
gda_data_meta_wrapper_set_property (GObject *object,
				      guint param_id,
				      const GValue *value,
				      GParamSpec *pspec)
{
	GdaDataMetaWrapper *model;

	model = GDA_DATA_META_WRAPPER (object);
	if (model->priv) {
		switch (param_id) {
		case PROP_MODEL: {
			GdaDataModel *mod = g_value_get_object(value);
			if (mod) {
				g_return_if_fail (GDA_IS_DATA_MODEL (mod));
				if (! (gda_data_model_get_access_flags (mod) & GDA_DATA_MODEL_ACCESS_RANDOM)) {
					g_warning ("Internal implementation error: data model does not support random access");
					return;
				}
  
                                if (model->priv->model)
					g_object_unref (model->priv->model);

				model->priv->model = mod;
				g_object_ref (mod);
				model->priv->nb_cols = gda_data_model_get_n_columns (mod);
			}
			break;
		}
		default:
			g_assert_not_reached ();
			break;
		}
	}
}

static void
gda_data_meta_wrapper_get_property (GObject *object,
					guint param_id,
					GValue *value,
					GParamSpec *pspec)
{
	GdaDataMetaWrapper *model;

	model = GDA_DATA_META_WRAPPER (object);
	if (model->priv) {
		switch (param_id) {
		case PROP_MODEL:
			g_value_set_object (value, G_OBJECT (model->priv->model));
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
}

/**
 * gda_data_meta_wrapper_new
 * @model: a #GdaDataModel
 *
 * Creates a new #GdaDataModel object which buffers the rows of @model. This object is usefull
 * only if @model can only be metaed using cursor based method.
 *
 * Returns: a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
_gda_data_meta_wrapper_new (GdaDataModel *model, gboolean reuseable, gint *cols, gint size, GdaSqlIdentifierStyle mode,
			    GdaSqlReservedKeywordsFunc reserved_keyword_func)
{
	GdaDataMetaWrapper *retmodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	
	retmodel = g_object_new (GDA_TYPE_DATA_META_WRAPPER,
				 "model", model, NULL);
			      
	retmodel->priv->cols_to_wrap = g_new0 (gint, size);
	memcpy (retmodel->priv->cols_to_wrap, cols, sizeof (gint) * size);
	retmodel->priv->cols_to_wrap_size = size;
	retmodel->priv->mode = mode;
	retmodel->priv->reserved_keyword_func = reserved_keyword_func;
	
	if (reuseable)
		retmodel->priv->computed_rows = g_hash_table_new_full (g_int_hash, g_int_equal,
								       NULL, (GDestroyNotify) comp_row_free);
	else {
		retmodel->priv->buffer = g_new0 (CompRow, 1);
		retmodel->priv->buffer->size = size;
		retmodel->priv->buffer->values = g_new0 (GValue *, size);
	}

	return GDA_DATA_MODEL (retmodel);
}

/*
 * GdaDataModel interface implementation
 */
static gint
gda_data_meta_wrapper_get_n_rows (GdaDataModel *model)
{
	GdaDataMetaWrapper *imodel;
	g_return_val_if_fail (GDA_IS_DATA_META_WRAPPER (model), 0);
	imodel = GDA_DATA_META_WRAPPER (model);
	g_return_val_if_fail (imodel->priv, 0);

	return gda_data_model_get_n_rows (imodel->priv->model);
}

static gint
gda_data_meta_wrapper_get_n_columns (GdaDataModel *model)
{
	GdaDataMetaWrapper *imodel;
	g_return_val_if_fail (GDA_IS_DATA_META_WRAPPER (model), 0);
	imodel = GDA_DATA_META_WRAPPER (model);
	g_return_val_if_fail (imodel->priv, 0);
	
	if (imodel->priv->model)
		return imodel->priv->nb_cols;
	else
		return 0;
}

static GdaColumn *
gda_data_meta_wrapper_describe_column (GdaDataModel *model, gint col)
{
	GdaDataMetaWrapper *imodel;
	g_return_val_if_fail (GDA_IS_DATA_META_WRAPPER (model), NULL);
	imodel = GDA_DATA_META_WRAPPER (model);
	g_return_val_if_fail (imodel->priv, NULL);

	if (imodel->priv->model)
		return gda_data_model_describe_column (imodel->priv->model, col);
	else
		return NULL;
}

static GdaDataModelAccessFlags
gda_data_meta_wrapper_get_access_flags (GdaDataModel *model)
{
	GdaDataMetaWrapper *imodel;

	g_return_val_if_fail (GDA_IS_DATA_META_WRAPPER (model), 0);
	imodel = GDA_DATA_META_WRAPPER (model);
	g_return_val_if_fail (imodel->priv, 0);
	
	return GDA_DATA_MODEL_ACCESS_RANDOM;
}

static gboolean
identifier_needs_quotes (const gchar *str, GdaSqlIdentifierStyle mode)
{
	const gchar *ptr;
	for (ptr = str; *ptr; ptr++) {
		/* quote if 1st char is a number */
		if ((*ptr <= '9') && (*ptr >= '0')) {
			if (ptr == str)
				return TRUE;
			continue;
		}
		if ((*ptr >= 'A') && (*ptr <= 'Z')) {
			if (mode == GDA_SQL_IDENTIFIERS_LOWER_CASE)
				return TRUE;
			continue;
		}
		if ((*ptr >= 'a') && (*ptr <= 'z')) {
			if (mode == GDA_SQL_IDENTIFIERS_UPPER_CASE)
				return TRUE;
			continue;
		}
		if ((*ptr != '$') && (*ptr != '_') && (*ptr != '#'))
			return TRUE;
	}
	return FALSE;
}

static gboolean
identifier_is_all_lower (const gchar *str)
{
	const gchar *ptr;
	for (ptr = str; *ptr; ptr++) {
		if (*ptr != g_ascii_tolower (*ptr))
			return FALSE;
	}
	return TRUE;
}

static gchar *
to_lower (gchar *str)
{
	gchar *ptr;
	for (ptr = str; *ptr; ptr++)
		*ptr = g_ascii_tolower (*ptr);
	return str;
}

/*
 * Returns:
 *  - NULL if no changes are necessary from the current value
 *  - a new GValue if changes were necessary
 */
static GValue *
compute_value (const GValue *value, GdaSqlIdentifierStyle mode, GdaSqlReservedKeywordsFunc reserved_keyword_func)
{
	GValue *retval = NULL;
	const gchar *str;

	if (G_VALUE_TYPE (value) != G_TYPE_STRING)
		return NULL;
	str = g_value_get_string (value);
	if (!str)
		return NULL;
	if ((*str == '"') && (str[strlen(str) - 1] == '"'))
		return NULL; /* already quoted */
	gchar **sa = g_strsplit (str, ".", 0);
	if (sa[1]) {
		gint i;
		gboolean onechanged = FALSE;
		for (i = 0; sa[i]; i++) {
			if (identifier_needs_quotes (sa[i], mode)) {
				gchar *tmp = gda_sql_identifier_add_quotes (sa[i]);
				g_free (sa[i]);
				sa[i] = tmp;
				onechanged = TRUE;
			}
			else {
				if (! identifier_is_all_lower (sa[i])) {
					to_lower (sa[i]);
					onechanged = TRUE;
				}

				if ((reserved_keyword_func && reserved_keyword_func (sa[i])) ||
				    (! reserved_keyword_func && is_keyword (sa[i]))) {
					gchar *tmp = gda_sql_identifier_add_quotes (sa[i]);
					g_free (sa[i]);
					sa[i] = tmp;
					onechanged = TRUE;
				}
			}
		}
		if (onechanged) {
			retval = gda_value_new (G_TYPE_STRING);
			g_value_take_string (retval, g_strjoinv (".", sa));
		}
	}
	else {
		if (identifier_needs_quotes (str, mode)) {
			retval = gda_value_new (G_TYPE_STRING);
			g_value_take_string (retval, gda_sql_identifier_add_quotes (str));
		}
		else {
			gchar *tmp = NULL;
			if (! identifier_is_all_lower (str))
				tmp = to_lower (g_strdup (str));

			if ((reserved_keyword_func && reserved_keyword_func (tmp ? tmp : str)) ||
			    (! reserved_keyword_func && is_keyword (tmp ? tmp : str))) {
				gchar *tmp2 = gda_sql_identifier_add_quotes (tmp ? tmp : str);
				if (tmp)
					g_free (tmp);
				tmp = tmp2;
			}
			if (tmp) {
				retval = gda_value_new (G_TYPE_STRING);
				g_value_take_string (retval, tmp);
			}
		}
	}
	g_strfreev (sa);

	return retval;
}

/*
 * Returns: the index in @imodel->priv->cols_to_wrap for column @col, or -1 if not found
 */
static gint
get_index_col (GdaDataMetaWrapper *imodel, gint col)
{
	gint i;
	for (i = 0; i < imodel->priv->cols_to_wrap_size; i++) {
		if (imodel->priv->cols_to_wrap [i] == col)
			return i;
		else if (imodel->priv->cols_to_wrap [i] > col)
			return -1;
	}
	return -1;
}

static const GValue *
gda_data_meta_wrapper_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaDataMetaWrapper *imodel;

	g_return_val_if_fail (GDA_IS_DATA_META_WRAPPER (model), NULL);
	imodel = GDA_DATA_META_WRAPPER (model);
	g_return_val_if_fail (imodel->priv, NULL);
	g_return_val_if_fail (imodel->priv->model, NULL);
	g_return_val_if_fail (row >= 0, NULL);

	if (col >= imodel->priv->nb_cols) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
			     _("Column %d out of range (0-%d)"), col, imodel->priv->nb_cols - 1);
		return NULL;
	}

	gint indexcol = get_index_col (imodel, col);
	if (indexcol == -1)
		return gda_data_model_get_value_at (imodel->priv->model, col, row, error);

	/* data may need to be changed */
	if (imodel->priv->computed_rows) {
		CompRow *crow = NULL;
		crow = g_hash_table_lookup (imodel->priv->computed_rows, &row);
		if (!crow || !(crow->values[indexcol]) || (crow->values[indexcol] == NON_COMP_VALUE)) {
			const GValue *cvalue;
			cvalue = gda_data_model_get_value_at (imodel->priv->model, col, row, error);
			if (!cvalue)
				return NULL;
			
			GValue *retval;
			retval = compute_value (cvalue, imodel->priv->mode, imodel->priv->reserved_keyword_func);
			if (!retval)
				return cvalue;
			
			if (!crow) {
				crow = g_new0 (CompRow, 1);
				crow->row = row;
				crow->size = imodel->priv->cols_to_wrap_size;
				crow->values = g_new (GValue *, crow->size);
				gint i;
				for (i = 0; i < crow->size; i++)
					crow->values [i] = NON_COMP_VALUE;
				g_hash_table_insert (imodel->priv->computed_rows, &(crow->row), crow);
			}
			crow->values[indexcol] = retval;
			return retval;
		}
		else
			return crow->values[indexcol];
	}
	else {
		const GValue *cvalue;
		cvalue = gda_data_model_get_value_at (imodel->priv->model, col, row, error);
		if (!cvalue)
			return NULL;

		GValue *retval;
		retval = compute_value (cvalue, imodel->priv->mode, imodel->priv->reserved_keyword_func);
		if (!retval)
			return cvalue;
		if (imodel->priv->buffer->values [indexcol])
			gda_value_free (imodel->priv->buffer->values [indexcol]);
		imodel->priv->buffer->values [indexcol] = retval;
		return retval;
	}
	return NULL;
}

