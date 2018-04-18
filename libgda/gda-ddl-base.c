/* gda-ddl-base.c
 *
 * Copyright © 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "gda-ddl-base.h"
#include <glib/gi18n.h>


G_DEFINE_QUARK (gda_ddl_base_error,gda_ddl_base_error)

typedef struct
{
	gchar *m_catalog;
	gchar *m_schema;
	gchar *m_name;
	gchar *m_fullname;
} GdaDdlBasePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaDdlBase, gda_ddl_base, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_CATALOG,
	PROP_SCHEMA,
	PROP_NAME,
	/*<prvate>*/
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

GdaDdlBase *
gda_ddl_base_new (void)
{
	return g_object_new (GDA_TYPE_DDL_BASE, NULL);
}

static void
gda_ddl_base_finalize (GObject *object)
{
	GdaDdlBase *self = (GdaDdlBase *)object;
	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	g_free (priv->m_catalog);
	g_free (priv->m_schema);
	g_free (priv->m_name);
	g_free (priv->m_fullname);

	G_OBJECT_CLASS (gda_ddl_base_parent_class)->finalize (object);
}

static void
gda_ddl_base_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
	GdaDdlBase *self = GDA_DDL_BASE (object);
	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	switch (prop_id) {
		case PROP_CATALOG:
			g_value_set_string (value,priv->m_catalog);
			break;
		case PROP_SCHEMA:
			g_value_set_string (value,priv->m_schema);
			break;
		case PROP_NAME:
			g_value_set_string (value,priv->m_name);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gda_ddl_base_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
	GdaDdlBase *self = GDA_DDL_BASE (object);
	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	switch (prop_id) {
		case PROP_CATALOG:
			g_free (priv->m_catalog);
			priv->m_catalog = g_value_dup_string (value);
			break;
		case PROP_SCHEMA:
			g_free (priv->m_schema);
			priv->m_schema = g_value_dup_string (value);
			break;
		case PROP_NAME:
			g_free (priv->m_name);
			priv->m_name = g_value_dup_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gda_ddl_base_class_init (GdaDdlBaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_ddl_base_finalize;
	object_class->get_property = gda_ddl_base_get_property;
	object_class->set_property = gda_ddl_base_set_property;
}

static void
gda_ddl_base_init (GdaDdlBase *self)
{
	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	priv->m_catalog = NULL;
	priv->m_schema  = NULL;
	priv->m_name    = NULL;
	priv->m_fullname = NULL;
}

/**
 * gda_ddl_base_set_names:
 * @self: a #GdaDdlBase object
 * @catalog: a catalog associated with table
 * @schema: a schema associated with table
 * @name: a table name associated with table
 *
 * Allow to set table name. @catalog and @schema can be %NULL but
 * @name always should be a valid, not %NULL string. The @name must be alsways
 * set. If @catalog is %NULL @schema may not be %NULL but if @schema is
 * %NULL @catalog also should be %NULL.
 *
 * Since: 6.0
 * */
gboolean
gda_ddl_base_set_names (GdaDdlBase *self,
			const gchar* catalog,
			const gchar* schema,
			const gchar* name,
			GError **error)
{
	g_return_val_if_fail (self,FALSE);

	if (!name) {
		g_set_error (error,GDA_DDL_BASE_ERROR,
			     GDA_DDL_BASE_UNVALID_NAME,
			     _("Provided table name can't be null\n"));
		return FALSE;
	}

	/* Check if catalog NULL but schema is not NULL */
	if (catalog && !schema) {
		g_set_error (error,GDA_DDL_BASE_ERROR,
			     GDA_DDL_BASE_NAME_MISSMATCH,
			     _("Schema name is not set while catalog name is set\n"));
		return FALSE;
	}

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	g_free (priv->m_name);
	priv->m_name = g_strdup (name);

	g_free (priv->m_schema);
	g_free (priv->m_catalog);

	if (schema)
		priv->m_schema = g_strdup (schema);
	else
		priv->m_catalog = NULL;

	if (catalog)
		priv->m_catalog = g_strdup (catalog);
	else
		priv->m_catalog = NULL;

	g_free (priv->m_fullname);

	GString *fullnamestr = NULL;

	fullnamestr = g_string_new (NULL);

	if (priv->m_catalog && priv->m_schema) {
		g_string_printf (fullnamestr,"%s.%s.%s",priv->m_catalog,priv->m_schema,priv->m_name);
	} else {
	/* In this block  catalog is NULL */
		if (priv->m_schema)
			g_string_printf (fullnamestr,"%s.%s",priv->m_schema,priv->m_name);
		else
			g_string_printf (fullnamestr,"%s",priv->m_name);
	}

	priv->m_fullname = g_strdup (fullnamestr->str);
	g_string_free (fullnamestr, TRUE);
	return TRUE;
}

/**
 * gda_ddl_base_get_full_name:
 *
 * If NULL is returned, database
 * object name hasn't been set yet.
 *
 * Returns: Full name of the database object. This string should not be freed.
 */
const gchar*
gda_ddl_base_get_full_name (GdaDdlBase *self,
			    GError **error)
{
	g_return_val_if_fail (self,NULL);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	/* if (!priv->m_fullname) { */
	/* 	g_set_error (error,GDA_DDL_BASE_ERROR, */
	/* 		     GDA_DDL_BASE_NAME_IS_NULL, */
	/* 		     _("Name is NULL. It should be set before use.\n")); */
	/* 	return NULL; */
	/* } */

	GString *fullnamestr = NULL;

	fullnamestr = g_string_new (NULL);

	if (priv->m_catalog && priv->m_schema && priv->m_name)
		g_string_printf (fullnamestr,"%s.%s.%s",priv->m_catalog,priv->m_schema,priv->m_name);
	else if (priv->m_schema && priv->m_name)
		g_string_printf (fullnamestr,"%s.%s",priv->m_schema,priv->m_name);
	else if (priv->m_name)
		g_string_printf (fullnamestr,"%s",priv->m_name);
	else {
		g_set_error (error,GDA_DDL_BASE_ERROR,
			     GDA_DDL_BASE_NAME_IS_NULL,
			     _("Name is NULL. It should be set before use.\n"));
		return NULL;
	}

	/* In this block  catalog is NULL */
	priv->m_fullname = g_strdup (fullnamestr->str);
	g_string_free (fullnamestr, TRUE);

	return priv->m_fullname;
}

/**
 * gda_ddl_base_get_catalog:
 *
 * @self: GdaDdlBase object
 * @error: an error holder
 *
 * Returns current catalog name. The returned string should not be freed.
 * In case of error, the @error is set appropriatly and %NULL is returned.
 *
 * Returns: Current catalog or NULL
 */
const gchar*
gda_ddl_base_get_catalog (GdaDdlBase  *self,
			  GError     **error)
{
	g_return_val_if_fail (self,NULL);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	if (!priv->m_catalog) {
		g_set_error (error,GDA_DDL_BASE_ERROR,
					 GDA_DDL_BASE_NAME_IS_NULL,
					 _("Catalog is NULL. It should be set before use.\n"));
		return NULL;
	}

	return priv->m_catalog;
}

/**
 * gda_ddl_base_get_schema:
 *
 * @self: GdaDdlBase object
 * @error: an error holder
 *
 * Returns current schema name. The returned string should not be freed.
 * In case of error, the @error is set appropriatly and %NULL is returned.
 *
 * Returns: Current catalog or NULL
 */
const gchar*
gda_ddl_base_get_schema (GdaDdlBase  *self,
			 GError     **error)
{
	g_return_val_if_fail (self,NULL);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	if (!priv->m_schema) {
		g_set_error (error,GDA_DDL_BASE_ERROR,
			     GDA_DDL_BASE_NAME_IS_NULL,
			     _("Schema is NULL. It should be set before use.\n"));
		return NULL;
	}

	return priv->m_schema;
}

/**
 * gda_ddl_base_get_name:
 *
 * @self: GdaDdlBase object
 * @error: an error holder
 *
 * Returns current object name. The returned string should not be freed.
 * In case of error, the @error is set appropriatly and %NULL is returned.
 *
 * Returns: Current catalog or NULL
 */
const gchar*
gda_ddl_base_get_name (GdaDdlBase  *self,
		       GError     **error)
{
	g_return_val_if_fail (self,NULL);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	if (!priv->m_name) {
		g_set_error (error,GDA_DDL_BASE_ERROR,
					 GDA_DDL_BASE_NAME_IS_NULL,
					 _("Name is NULL. It should be set before use.\n"));
		return NULL;
	}

	return priv->m_name;
}


void
gda_ddl_base_free (GdaDdlBase *self)
{
	g_clear_object (&self);
}

void
gda_ddl_base_set_catalog (GdaDdlBase  *self,
			  const gchar *catalog)
{
	g_return_if_fail (self);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	g_free (priv->m_catalog);
	priv->m_catalog = g_strdup (catalog);
}

void
gda_ddl_base_set_schema (GdaDdlBase  *self,
			 const gchar *schema)
{
	g_return_if_fail (self);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	g_free (priv->m_schema);
	priv->m_schema = g_strdup (schema);
}

void
gda_ddl_base_set_name (GdaDdlBase  *self,
		       const gchar *name)
{
	g_return_if_fail (self);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	g_free (priv->m_name);
	priv->m_name = g_strdup (name);
}

