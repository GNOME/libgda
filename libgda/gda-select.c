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

#include <glib/gdataset.h>
#include <glib/gscanner.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-select.h>
#include <libgda/gda-value.h>
#include <string.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ARRAY

typedef enum {
	GDA_SELECT_TOKEN_AND = G_TOKEN_LAST,
	GDA_SELECT_TOKEN_OR,
	GDA_SELECT_TOKEN_LAST
} GdaSelectTokenType;

typedef enum {
	GDA_SELECT_CONDITION_INVALID,
	GDA_SELECT_CONDITION_EQUAL,
	GDA_SELECT_CONDITION_LESS_THAN,
	GDA_SELECT_CONDITION_GREATER_THAN
} GdaSelectConditionType;

typedef struct {
	gchar *field_name;
	GdaSelectConditionType type;
	GdaValue *value;
} GdaSelectCondition;

struct _GdaSelectPrivate {
	GdaDataModel *source_model;
	gchar *expression;
	GScanner *scanner;
};

static void gda_select_class_init (GdaSelectClass *klass);
static void gda_select_init       (GdaSelect *sel, GdaSelectClass *klass);
static void gda_select_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;
static const struct {
	gchar *name;
	guint token;
} symbols[] = {
	{ "and", GDA_SELECT_TOKEN_AND },
	{ "or", GDA_SELECT_TOKEN_OR }
};

/*
 * Private functions
 */

static void
free_condition (gpointer data)
{
	GdaSelectCondition *cond = (GdaSelectCondition *) data;

	g_free (cond->field_name);
	gda_value_free (cond->value);
	g_free (cond);
}

static gboolean
fill_data (GdaSelect *sel)
{
	gboolean done;
	gboolean res;
	GList *conditions;
	GdaSelectCondition *cond;

	g_return_val_if_fail (GDA_IS_SELECT (sel), FALSE);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (sel->priv->source_model), FALSE);

	g_scanner_input_text (sel->priv->scanner,
			      sel->priv->expression,
			      strlen (sel->priv->expression));

	/* prepare the list of conditions */
	done = FALSE;
	conditions = NULL;
	cond = NULL;
	res = FALSE;

	while (!done) {
		int token;

		token = g_scanner_get_next_token (sel->priv->scanner);
		switch (token) {

		case G_TOKEN_INT :
			if (cond && !cond->value) {
				gint val = g_scanner_cur_value (sel->priv->scanner).v_int;
				cond->value = gda_value_new_integer (val);

				conditions = g_list_append (conditions, cond);
				cond = NULL;
			} else {
				gda_log_error (_("Unexpected integer value"));
				done = TRUE;
			}
			break;

		case G_TOKEN_FLOAT :
			if (cond && !cond->value) {
				gfloat val = g_scanner_cur_value (sel->priv->scanner).v_float;
				cond->value = gda_value_new_single (val);
				conditions = g_list_append (conditions, cond);
				cond = NULL;
			} else {
				gda_log_error (_("Unexpected float value"));
				done = TRUE;
			}
			break;

		case G_TOKEN_CHAR :
			if (cond && !cond->value) {
				cond->value = gda_value_new_tinyint (g_scanner_cur_value(sel->priv->scanner).v_char);
				conditions = g_list_append (conditions, cond);
				cond = NULL;
			} else {
				gda_log_error (_("Unexpected integer value"));
				done = TRUE;
			}
			break;

		case G_TOKEN_STRING :
			if (cond && !cond->value) {
				cond->value = gda_value_new_string (
					g_scanner_cur_value (sel->priv->scanner).v_string);
				conditions = g_list_append (conditions, cond);
				cond = NULL;
			} else {
				gda_log_error (_("Unexpected string value"));
				done = TRUE;
			}
			break;

		case '=' :
			if (cond && !cond->value
			    && cond->type == GDA_SELECT_CONDITION_INVALID) {
				cond->type = GDA_SELECT_CONDITION_EQUAL;
			}
			else {
				gda_log_error (_("Unexpected symbol '='"));
				done = TRUE;
			}
			break;

		case '<' :
			if (cond && !cond->value
			    && cond->type == GDA_SELECT_CONDITION_INVALID) {
				cond->type = GDA_SELECT_CONDITION_LESS_THAN;
			}
			else {
				gda_log_error (_("Unexpected symbol '<'"));
				done = TRUE;
			}
			break;

		case '>' :
			if (cond && !cond->value
			    && cond->type == GDA_SELECT_CONDITION_INVALID) {
				cond->type = GDA_SELECT_CONDITION_GREATER_THAN;
			}
			else {
				gda_log_error (_("Unexpected symbol '>'"));
				done = TRUE;
			}
			break;

		case '-' :
			token = g_scanner_get_next_token (sel->priv->scanner);
			if (token == G_TOKEN_INT) {
				if (cond && !cond->value) {
					gint val = g_scanner_cur_value (sel->priv->scanner).v_int;
					cond->value = gda_value_new_integer (-val);
					conditions = g_list_append (conditions, cond);
					cond = NULL;
				} else {
					gda_log_error (_("Unexpected integer value"));
					done = TRUE;
				}
			} else if (token == G_TOKEN_FLOAT) {
				if (cond && !cond->value) {
					gfloat val = g_scanner_cur_value (sel->priv->scanner).v_float;
					cond->value = gda_value_new_single (-val);
					conditions = g_list_append (conditions, cond);
					cond = NULL;
				} else {
					gda_log_error (_("Unexpected float value"));
					done = TRUE;
				}
			} else {
				gda_log_error (_("Found a non-numeric value after a '-' sign"));
				done = TRUE;
			}
			break;

		case G_TOKEN_SYMBOL :
			break;

		case G_TOKEN_IDENTIFIER :
			if (!cond) {
				gchar *ident = g_scanner_cur_value (sel->priv->scanner).v_identifier;

				cond = g_new0 (GdaSelectCondition, 1);
				cond->field_name = g_strdup (ident);
				cond->type = GDA_SELECT_CONDITION_INVALID;
			} else {
				gda_log_error (_("Unexpected identifier"));
				done = TRUE;
			}
			break;

		case G_TOKEN_EOF :
			done = TRUE;
			res = TRUE;
			break;
		}
	}

	if (res) {
		gint rows, r;
		const GdaRow *fields;

		rows = gda_data_model_get_n_rows (sel->priv->source_model);
		for (r = 0; r < rows; r++) {
			fields = gda_data_model_get_row (sel->priv->source_model, r);
		}
	}

	/* free memory */
	g_list_foreach (conditions, (GFunc) free_condition, NULL);
	g_list_free (conditions);

	return res;
}

/*
 * GdaSelect class implementation
 */

static GdaFieldAttributes *
gda_select_describe_column (GdaDataModel *model, gint col)
{
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (sel->priv->source_model), NULL);

	return gda_data_model_describe_column (sel->priv->source_model, col);
}

static const GdaRow *
gda_select_get_row (GdaDataModel *model, gint row)
{
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), NULL);

	/* FIXME: identify this row, so that it can be updated and changes
	   proxied to the source_model */

	return GDA_DATA_MODEL_CLASS (parent_class)->get_row (model, row);
}

static gboolean
gda_select_is_editable (GdaDataModel *model)
{
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), FALSE);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (sel->priv->source_model), FALSE);

	return gda_data_model_is_editable (sel->priv->source_model);
}

static const GdaRow *
gda_select_append_row (GdaDataModel *model, const GList *values)
{
	const GdaRow *row;
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (sel->priv->source_model), NULL);

	/* add the row to the source data model */
	row = gda_data_model_append_row (sel->priv->source_model, values);
	if (!row)
		return NULL;

	/* re-fill the select object from the source data model */
	fill_data (sel);
	
	return row;
}

static void
gda_select_class_init (GdaSelectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_select_finalize;
	// we use the get_n_rows and get_n_columns of the base class
	model_class->describe_column = gda_select_describe_column;
	model_class->get_row = gda_select_get_row;
	// we use the get_value_at of the base class
	model_class->is_editable = gda_select_is_editable;
	model_class->append_row = gda_select_append_row;
}

static void
gda_select_init (GdaSelect *sel, GdaSelectClass *klass)
{
	gint i;

	g_return_if_fail (GDA_IS_SELECT (sel));

	/* allocate internal structure */
	sel->priv = g_new0 (GdaSelectPrivate, 1);
	sel->priv->source_model = NULL;
	sel->priv->expression = NULL;

	/* initialize the scanner */
	sel->priv->scanner = g_scanner_new (NULL);
	for (i = 0; i < G_N_ELEMENTS (symbols); i++) {
		g_scanner_scope_add_symbol (sel->priv->scanner, 0, symbols[i].name,
					    GINT_TO_POINTER (symbols[i].token));
	}
}

static void
gda_select_finalize (GObject *object)
{
	GdaSelect *sel = (GdaSelect *) object;

	g_return_if_fail (GDA_IS_SELECT (sel));

	/* free memory */
	if (sel->priv->source_model) {
		g_object_unref (G_OBJECT (sel->priv->source_model));
		sel->priv->source_model = NULL;
	}

	if (sel->priv->expression) {
		g_free (sel->priv->expression);
		sel->priv->expression = NULL;
	}

	g_scanner_destroy (sel->priv->scanner);
	sel->priv->scanner = NULL;

	g_free (sel->priv);
	sel->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_select_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaSelectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_select_class_init,
			NULL,
			NULL,
			sizeof (GdaSelect),
			0,
			(GInstanceInitFunc) gda_select_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaSelect", &info, 0);
	}
	return type;
}

/**
 * gda_select_new
 *
 * Create a new #GdaSelect object, which allows programs to filter
 * #GdaDataModel's based on a given expression.
 *
 * A #GdaSelect is just another #GdaDataModel-based class, so it
 * can be used in the same way any other data model class is.
 *
 * Returns: the newly created object.
 */
GdaDataModel *
gda_select_new (void)
{
	GdaDataModel *model;

	model = g_object_new (GDA_TYPE_SELECT, NULL);
	return model;
}

/**
 * gda_select_set_source
 * @sel: a #GdaSelect object.
 * @source: a #GdaDataModel to use as source.
 *
 * Associate a data model with the given #GdaSelect object to
 * be used as source of data when executing the selection.
 */
void
gda_select_set_source (GdaSelect *sel, GdaDataModel *source)
{
	g_return_if_fail (GDA_IS_SELECT (sel));

	if (GDA_IS_DATA_MODEL (sel->priv->source_model)) {
		g_object_unref (G_OBJECT (sel->priv->source_model));
		sel->priv->source_model = NULL;
	}

	if (GDA_IS_DATA_MODEL (source)) {
		g_object_ref (G_OBJECT (source));
		sel->priv->source_model = source;
	}
}

/**
 * gda_select_set_expression
 * @sel: a #GdaSelect object.
 * @expression: the expression to be used for filtering rows.
 *
 * Set the expression to be used on the given #GdaSelect object
 * for filtering rows from the source data model (which is
 * set with #gda_select_set_source).
 */
void
gda_select_set_expression (GdaSelect *sel, const gchar *expression)
{
	g_return_if_fail (GDA_IS_SELECT (sel));

	if (sel->priv->expression)
		g_free (sel->priv->expression);
	sel->priv->expression = g_strdup (expression);
}

/**
 * gda_select_run
 * @sel: a #GdaSelect object.
 *
 * Run the query and fill in the #GdaSelect object with the
 * rows that matched the expression (which can be set with
 * #gda_select_set_expression) associated with this #GdaSelect
 * object.
 *
 * After calling this function, if everything is successful,
 * the #GdaSelect object will contain the matched rows, which
 * can then be accessed like a normal #GdaDataModel.
 *
 * Returns: TRUE if successful, FALSE if there was an error.
 */
gboolean
gda_select_run (GdaSelect *sel)
{
	g_return_val_if_fail (GDA_IS_SELECT (sel), FALSE);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (sel->priv->source_model), FALSE);

	gda_data_model_array_clear (GDA_DATA_MODEL_ARRAY (sel));
	return fill_data (sel);
}
