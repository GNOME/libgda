/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#define G_LOG_DOMAIN "GDA-repetitive-statement"

#include <gda-repetitive-statement.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-connection.h>

typedef struct
{
	GdaStatement* statement;
	GSList* values_sets; /* list of GdaSet pointers, objects referenced here */
} GdaRepetitiveStatementPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GdaRepetitiveStatement, gda_repetitive_statement, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_STATEMENT
};

static void
gda_repetitive_statement_init (GdaRepetitiveStatement *object)
{
	GdaRepetitiveStatementPrivate *priv = gda_repetitive_statement_get_instance_private (object);
	
	priv->statement = NULL;
	priv->values_sets = NULL;
}

static void
gda_repetitive_statement_finalize (GObject *object)
{
	GdaRepetitiveStatementPrivate *priv = gda_repetitive_statement_get_instance_private (GDA_REPETITIVE_STATEMENT (object));
	
	g_object_unref (priv->statement);
	g_slist_free_full (priv->values_sets, (GDestroyNotify) g_object_unref);

	G_OBJECT_CLASS (gda_repetitive_statement_parent_class)->finalize (object);
}

static void
gda_repetitive_statement_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	g_return_if_fail (GDA_IS_REPETITIVE_STATEMENT (object));
	
	GdaRepetitiveStatementPrivate *priv = gda_repetitive_statement_get_instance_private (GDA_REPETITIVE_STATEMENT (object));

	switch (prop_id) {
	case PROP_STATEMENT:
		priv->statement = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gda_repetitive_statement_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	g_return_if_fail (GDA_IS_REPETITIVE_STATEMENT (object));

	GdaRepetitiveStatementPrivate *priv = gda_repetitive_statement_get_instance_private (GDA_REPETITIVE_STATEMENT (object));

	switch (prop_id) {
	case PROP_STATEMENT:
		g_value_set_object (value, priv->statement);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gda_repetitive_statement_class_init (GdaRepetitiveStatementClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_repetitive_statement_finalize;
	object_class->set_property = gda_repetitive_statement_set_property;
	object_class->get_property = gda_repetitive_statement_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_STATEMENT,
	                                 g_param_spec_object ("statement",
	                                                      "stmt",
	                                                      "Statement to Execute",
	                                                      GDA_TYPE_STATEMENT,
	                                                      G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_CONSTRUCT_ONLY));
}

/**
 * gda_repetitive_statement_new:
 * @stmt: a #GdaStatement object
 *
 * Creates a new #GdaRepetitiveStatement object which, when executed, will execute @stmt once for all
 * the values set which will have been defined using gda_repetitive_statement_append_set().
 * Use gda_connection_repetitive_statement_execute() to actually execute it.
 *
 * Returns: a new #GdaRepetitiveStatement object
 *
 * Since: 4.2
 */
GdaRepetitiveStatement*
gda_repetitive_statement_new (GdaStatement *stmt)
{
	GdaRepetitiveStatement *rstmt;
	
	rstmt = GDA_REPETITIVE_STATEMENT (g_object_new (GDA_TYPE_REPETITIVE_STATEMENT,
							"statement", stmt, NULL));
	
	return rstmt;
}

/**
 * gda_repetitive_statement_get_template_set:
 * @rstmt: a #GdaRepetitiveStatement object
 * @set: a place to store the returned template set
 * @error: (nullable): a place to store error, or %NULL
 *
 * Gets a new #GdaSet object with the parameters used by the template statement in the
 * @rstmt object. 
 *
 * Use this object with gda_repetitive_statement_append_set().
 *
 * Returns: %TRUE on success, %FALSE on error
 *
 * Since: 4.2
 */
gboolean
gda_repetitive_statement_get_template_set (GdaRepetitiveStatement *rstmt, GdaSet **set, GError **error)
{
	GdaRepetitiveStatementPrivate *priv = gda_repetitive_statement_get_instance_private (rstmt);
	
	return gda_statement_get_parameters (priv->statement, set, error);
}

/**
 * gda_repetitive_statement_append_set:
 * @rstmt: a #GdaRepetitiveStatement object
 * @values: a #GdaSet object with the values to be used
 * @make_copy: %TRUE if @values is copied, and %FALSE if @values is only ref'ed
 *
 * Specifies that @rstmt be executed one time with the values contained in @values. 
 *
 * A new #GdaSet to be used as the @values argument can be obtained using
 * gda_repetitive_statement_get_template_set().
 *
 * Returns: a new #GdaRepetitiveStatement object
 *
 * Since: 4.2
 */
gboolean
gda_repetitive_statement_append_set (GdaRepetitiveStatement *rstmt, GdaSet *values, gboolean make_copy)
{
	GdaSet *set;

	g_return_val_if_fail (GDA_IS_REPETITIVE_STATEMENT(rstmt), FALSE);
	g_return_val_if_fail (GDA_IS_SET (values), FALSE);

	GdaRepetitiveStatementPrivate *priv = gda_repetitive_statement_get_instance_private (rstmt);
	
	if (make_copy)
		set = gda_set_copy (values);
	else
		set = g_object_ref (values);
	priv->values_sets = g_slist_prepend (priv->values_sets, set);
	
	return TRUE;
}

/**
 * gda_repetitive_statement_get_all_sets:
 * @rstmt: a #GdaRepetitiveStatement object
 *
 * Get all the values sets which will have been added using gda_repetitive_statement_append_set().
 *
 * Returns: (transfer container) (element-type GdaSet): a new #GSList of #GdaSet objects (free with g_slist_free()).
 *
 * Since: 4.2
 */
GSList*
gda_repetitive_statement_get_all_sets (GdaRepetitiveStatement *rstmt)
{
	GdaRepetitiveStatementPrivate *priv = gda_repetitive_statement_get_instance_private (rstmt);
	
	return g_slist_copy (g_slist_reverse (priv->values_sets));
}
