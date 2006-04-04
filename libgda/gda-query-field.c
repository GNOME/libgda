/* gda-query-field.c
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-query-field.h"
#include "gda-query.h"
#include "gda-entity-field.h"
#include "gda-query-field-all.h"
#include "gda-query-field-field.h"
#include "gda-query-field-value.h"
#include "gda-query-field-func.h"
#include "gda-query-field-agg.h"
#include "gda-xml-storage.h"
#include "gda-object-ref.h"
#include "gda-connection.h"

#include "gda-query-parsing.h"


/* 
 * Main static functions 
 */
static void gda_query_field_class_init (GdaQueryFieldClass *class);
static void gda_query_field_init (GdaQueryField *qfield);
static void gda_query_field_dispose (GObject *object);
static void gda_query_field_finalize (GObject *object);

static void gda_query_field_set_int_id (GdaQueryObject *qfield, guint id);
/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _GdaQueryFieldPrivate
{
	gchar     *alias;
	gboolean   visible;
	gboolean   internal;
};

/* module error */
GQuark gda_query_field_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_query_field_error");
	return quark;
}


GType
gda_query_field_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryFieldClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_field_class_init,
			NULL,
			NULL,
			sizeof (GdaQueryField),
			0,
			(GInstanceInitFunc) gda_query_field_init
		};
		
		type = g_type_register_static (GDA_TYPE_QUERY_OBJECT, "GdaQueryField", &info, 0);
	}
	return type;
}

static void
gda_query_field_class_init (GdaQueryFieldClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	/* virtual functions */
	class->copy = NULL;
	class->is_equal = NULL;

	GDA_QUERY_OBJECT_CLASS (class)->set_int_id = (void (*)(GdaQueryObject *, guint)) gda_query_field_set_int_id;

	object_class->dispose = gda_query_field_dispose;
	object_class->finalize = gda_query_field_finalize;
}

static void
gda_query_field_init (GdaQueryField *qfield)
{
	qfield->priv = g_new0 (GdaQueryFieldPrivate, 1);
	qfield->priv->alias = NULL;
	qfield->priv->visible = TRUE;
	qfield->priv->internal = FALSE;
}

static void
gda_query_field_set_int_id (GdaQueryObject *qfield, guint id)
{
	gchar *str;
	GdaEntity *ent;

	ent = gda_entity_field_get_entity (GDA_ENTITY_FIELD (qfield));
	str = g_strdup_printf ("%s:QF%u", gda_object_get_id (GDA_OBJECT (ent)), id);
	gda_object_set_id (GDA_OBJECT (qfield), str);
	g_free (str);
}

/**
 * gda_query_field_new_from_xml
 * @query: a #GdaQuery object
 * @node: an XML node corresponding to a GDA_QUERY_FIELD tag
 * @error: location to store error, or %NULL
 *
 * This is an object factory which does create instances of class inheritants of the #GnomeDbDfield class.
 * Ths #GdaQueryField object MUST then be attached to @query
 * 
 * Returns: the newly created object
 */
GdaQueryField *
gda_query_field_new_from_xml (GdaQuery *query, xmlNodePtr node, GError **error)
{
	GdaQueryField *obj = NULL;
	gboolean done = FALSE;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (node, NULL);

	if (!strcmp (node->name, "gda_query_fall")) {
		gchar *target;
		
		done = TRUE;
		target = xmlGetProp (node, "target");
		if (target) {
			obj = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_ALL, 
							      "dict", gda_object_get_dict (GDA_OBJECT (query)), 
							      "query", query, NULL);
			g_object_set (G_OBJECT (obj), "target_id", target, NULL);
			g_free (target);
		}
		else {
			g_set_error (error,
				     GDA_QUERY_FIELD_ALL_ERROR,
				     GDA_QUERY_FIELD_ALL_XML_LOAD_ERROR,
				     _("Missing 'target' attribute in <gda_query_fall>"));
			return NULL;
		}
	}

	if (!done && !strcmp (node->name, "gda_query_ffield")) {
		done = TRUE;
		obj = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
						      "dict", gda_object_get_dict (GDA_OBJECT (query)), 
						      "query", query, NULL);
	}

	if (!done && !strcmp (node->name, "gda_query_fagg")) {
		done = TRUE;
		obj = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_AGG, 
						      "dict", gda_object_get_dict (GDA_OBJECT (query)), 
						      "query", query, NULL);
	}

	if (!done && !strcmp (node->name, "gda_query_ffunc")) {
		done = TRUE;
		obj = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_FUNC, 
						      "dict", gda_object_get_dict (GDA_OBJECT (query)), 
						      "query", query, NULL);
	}

	if (!done && !strcmp (node->name, "gda_query_fval")) {
		done = TRUE;
		obj = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_VALUE, 
						      "dict", gda_object_get_dict (GDA_OBJECT (query)), 
						      "query", query, NULL);
	}

	g_assert (done);
	if (obj) {
		if (!gda_xml_storage_load_from_xml (GDA_XML_STORAGE (obj), node, error)) {
			g_object_unref (obj);
			return NULL;
		}
	}
	else 
		g_set_error (error,
			     GDA_QUERY_FIELD_ALL_ERROR,
			     GDA_QUERY_FIELD_ALL_XML_LOAD_ERROR,
			     _("Missing Implementation in loading <gda_query_f*>"));

	return obj;
}


/**
 * gda_query_field_new_copy
 * @orig: a #GdaQueryField to copy
 *
 * This is a copy constructor
 *
 * Returns: the new object
 */
GdaQueryField *
gda_query_field_new_copy (GdaQueryField *orig)
{
	GdaQueryFieldClass *class;
	GObject *obj;
	GdaQuery *query;
	GdaQueryField *newfield;

	g_return_val_if_fail (orig && GDA_IS_QUERY_FIELD (orig), NULL);
	g_return_val_if_fail (orig->priv, NULL);
	g_object_get (G_OBJECT (orig), "query", &query, NULL);
	g_return_val_if_fail (query, NULL);

	class = GDA_QUERY_FIELD_CLASS (G_OBJECT_GET_CLASS (orig));
	g_return_val_if_fail (class->copy, NULL);

	obj = (class->copy) (orig);
	newfield = GDA_QUERY_FIELD (obj);
	newfield->priv->visible = orig->priv->visible;
	newfield->priv->internal = orig->priv->internal;

	return newfield;
}

/**
 * gda_query_field_new_from_sql
 * @query: a #GdaQuery object
 * @sqlfield: a SQL statement representing a query field
 * @error: location to store error, or %NULL
 *
 * Creates a new #GdaQueryField from its SQL representation
 *
 * Returns: a new #GdaQueryField object, or %NULL if an error occured
 */
GdaQueryField *
gda_query_field_new_from_sql (GdaQuery *query, const gchar *sqlfield, GError **error)
{
	gchar *sql;
	sql_statement *result;
	GdaQueryField *qfield = NULL;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (sqlfield && *sqlfield, NULL);

	sql = g_strconcat ("SELECT ", sqlfield, NULL);
	result = sql_parse_with_error (sql, error);
	if (result) {
		GList *list = ((sql_select_statement *) result->statement)->fields;
		if (list) {
			if (g_slist_next (list))
				g_set_error (error, GDA_QUERY_ERROR, GDA_QUERY_PARSE_ERROR,
					     _("More than one field to parse in '%s'"), sqlfield);
			else {
				ParseData *pdata;
				
				pdata = parse_data_new (query);
				parse_data_compute_targets_hash (query, pdata);
				qfield = (GdaQueryField *) parsed_create_global_query_field (query, FALSE, pdata, 
											(sql_field *) list->data,
											FALSE, NULL, NULL, error);
				parse_data_destroy (pdata);
			}
		}
		else 
			g_set_error (error, GDA_QUERY_ERROR, GDA_QUERY_PARSE_ERROR,
				     _("No field to parse in '%s'"), sqlfield);
		sql_destroy (result);
	}
	else {
		if (error && !(*error))
			g_set_error (error, GDA_QUERY_ERROR, GDA_QUERY_PARSE_ERROR,
				     _("Error parsing '%s'"), sqlfield);
	}
	g_free (sql);
	
	return qfield;
}

static void
gda_query_field_dispose (GObject *object)
{
	GdaQueryField *qfield;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD (object));

	qfield = GDA_QUERY_FIELD (object);
	if (qfield->priv) {
		if (qfield->priv->alias) {
			g_free (qfield->priv->alias);
			qfield->priv->alias = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_query_field_finalize (GObject   * object)
{
	GdaQueryField *qfield;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD (object));

	qfield = GDA_QUERY_FIELD (object);
	if (qfield->priv) {
		g_free (qfield->priv);
		qfield->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

/**
 * gda_query_field_set_alias
 * @qfield: a #GdaQueryField object
 * @alias: the alias to set @qfield to
 *
 * Sets @qfield's alias
 */
void
gda_query_field_set_alias (GdaQueryField *qfield, const gchar *alias)
{
	g_return_if_fail (qfield && GDA_IS_QUERY_FIELD (qfield));
	g_return_if_fail (qfield->priv);

	if (qfield->priv->alias) {
		g_free (qfield->priv->alias);
		qfield->priv->alias = NULL;
	}
	
	if (alias)
		qfield->priv->alias = g_strdup (alias);
	gda_object_changed (GDA_OBJECT (qfield));
}

/**
 * gda_query_field_get_alias
 * @qfield: a #GdaQueryField object
 *
 * Get @qfield's alias
 *
 * Returns: the alias
 */
const gchar *
gda_query_field_get_alias (GdaQueryField *qfield)
{
	g_return_val_if_fail (qfield && GDA_IS_QUERY_FIELD (qfield), NULL);
	g_return_val_if_fail (qfield->priv, NULL);

	return qfield->priv->alias;
}


/**
 * gda_query_field_set_visible
 * @qfield: a #GdaQueryField object
 * @visible:
 *
 * Sets the visibility of @qfield. A visible field will appear in the query's 
 * corresponding (virtual) entity, whereas a non visible one will be hidden (and
 * possibly not taking part in the query).
 */
void
gda_query_field_set_visible (GdaQueryField *qfield, gboolean visible)
{
	GdaQuery *query;

	g_return_if_fail (qfield && GDA_IS_QUERY_FIELD (qfield));
	g_return_if_fail (qfield->priv);
	g_object_get (G_OBJECT (qfield), "query", &query, NULL);
	g_return_if_fail (query);

	if (qfield->priv->visible != visible) {
		qfield->priv->visible = visible;
		if (visible)
			g_signal_emit_by_name (G_OBJECT (query), "field_added", GDA_ENTITY_FIELD (qfield));
		else
			g_signal_emit_by_name (G_OBJECT (query), "field_removed", GDA_ENTITY_FIELD (qfield));
		gda_object_changed (GDA_OBJECT (query));
	}
}


/**
 * gda_query_field_is_visible
 * @qfield: a #GdaQueryField object
 *
 * Returns: TRUE if @field is visible
 */
gboolean
gda_query_field_is_visible (GdaQueryField *qfield)
{
	g_return_val_if_fail (qfield && GDA_IS_QUERY_FIELD (qfield), FALSE);
	g_return_val_if_fail (qfield->priv, FALSE);

	return qfield->priv->visible;
}

/**
 * gda_query_field_set_internal
 * @qfield: a #GdaQueryField object
 * @internal:
 *
 * Sets weather @qfield is internal or not. Internal fields in a query are fields added
 * or changed by libgnomedb itself, such fields may or may not be visible.
 */
void
gda_query_field_set_internal (GdaQueryField *qfield, gboolean internal)
{
	g_return_if_fail (qfield && GDA_IS_QUERY_FIELD (qfield));
	g_return_if_fail (qfield->priv);

	qfield->priv->internal = internal;
}


/**
 * gda_query_field_is_internal
 * @qfield: a #GdaQueryField object
 *
 * Returns: TRUE if @field is internal
 */
gboolean
gda_query_field_is_internal (GdaQueryField *qfield)
{
	g_return_val_if_fail (qfield && GDA_IS_QUERY_FIELD (qfield), FALSE);
	g_return_val_if_fail (qfield->priv, FALSE);

	return qfield->priv->internal;
}

/**
 * gda_query_field_get_dict_type
 * @qfield: a #GdaQueryField object
 *
 * Get the #GdaDictType represented by the @qfield object: for a function it returns
 * the return type, for a value, it returns its type, etc.
 *
 * Returns: the data type, or %NULL if @qfield does not have a data type.
 */
GdaDictType *
gda_query_field_get_dict_type (GdaQueryField *qfield)
{
	g_return_val_if_fail (qfield && GDA_IS_QUERY_FIELD (qfield), NULL);
	g_return_val_if_fail (qfield->priv, NULL);

	/* it is assumed that qfield really implements the GdaEntityField interface */
	return gda_entity_field_get_data_type (GDA_ENTITY_FIELD (qfield));
}

/**
 * gda_query_field_get_parameters
 * @qfield: a #GdaQueryField object
 *
 * Get a list of all the parameters needed to @qfield to be
 * rendered as a valid statement
 *
 * Returns: a new list of parameters for @qfield
 */
GSList *
gda_query_field_get_parameters (GdaQueryField *qfield)
{
	GdaQueryFieldClass *class;

	g_return_val_if_fail (qfield && GDA_IS_QUERY_FIELD (qfield), NULL);
	g_return_val_if_fail (qfield->priv, NULL);
	class = GDA_QUERY_FIELD_CLASS (G_OBJECT_GET_CLASS (qfield));

	if (class->get_params)
		return (class->get_params) (qfield);
	else
		return NULL;
}

/**
 * gda_query_field_is_equal
 * @qfield1: a #GdaQueryField object
 * @qfield2: a #GdaQueryField object
 *
 * Compares the @qfield1 and @qfield2. The name and aliases of the two fields are
 * not compared, only the contents of the fields are.
 *
 * Returns: TRUE if they are equal and FALSE otherwise
 */
gboolean
gda_query_field_is_equal (GdaQueryField *qfield1, GdaQueryField *qfield2)
{
	GdaQueryFieldClass *class1, *class2;
	GdaQuery *q1, *q2;

	g_return_val_if_fail (qfield1 && GDA_IS_QUERY_FIELD (qfield1), FALSE);
	g_return_val_if_fail (qfield2 && GDA_IS_QUERY_FIELD (qfield2), FALSE);
	g_return_val_if_fail (qfield1->priv, FALSE);
	g_return_val_if_fail (qfield2->priv, FALSE);
	
	if (qfield1 == qfield2)
		return TRUE;

	g_object_get (G_OBJECT (qfield1), "query", &q1, NULL);
	g_object_get (G_OBJECT (qfield2), "query", &q2, NULL);
	g_return_val_if_fail (q1, FALSE);
	g_return_val_if_fail (q2, FALSE);

	if (q1 != q2)
		return FALSE;

	class1 = GDA_QUERY_FIELD_CLASS (G_OBJECT_GET_CLASS (qfield1));
	class2 = GDA_QUERY_FIELD_CLASS (G_OBJECT_GET_CLASS (qfield2));
	if (class1 != class2)
		return FALSE;

	g_return_val_if_fail (class1->is_equal, FALSE);

	return (class1->is_equal) (qfield1, qfield2);
}


/**
 * gda_query_field_is_list
 * @qfield: a #GdaQueryField object
 *
 * Tells if @qfield can potentially represent a list of values.
 *
 * Returns: TRUE if @field can be a list of values
 */
gboolean
gda_query_field_is_list (GdaQueryField *qfield)
{
	GdaQueryFieldClass *class;

	g_return_val_if_fail (qfield && GDA_IS_QUERY_FIELD (qfield), FALSE);
	g_return_val_if_fail (qfield->priv, FALSE);

	class = GDA_QUERY_FIELD_CLASS (G_OBJECT_GET_CLASS (qfield));
	if (class->is_list)
		return (class->is_list) (qfield);
	else
		return FALSE;
}

