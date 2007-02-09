/* gda-dict-field.c
 *
 * Copyright (C) 2003 - 2007 Vivien Malerba
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

#include "gda-dict-field.h"
#include "gda-dict-table.h"
#include "gda-xml-storage.h"
#include "gda-entity-field.h"
#include "gda-entity.h"
#include "gda-renderer.h"
#include "gda-data-handler.h"
#include "gda-connection.h"
#include "gda-dict-type.h"
#include "gda-dict-constraint.h"
#include <string.h>
#include <libgda/gda-util.h>
#include <glib/gi18n-lib.h>

/* 
 * Main static functions 
 */
static void gda_dict_field_class_init (GdaDictFieldClass * class);
static void gda_dict_field_init (GdaDictField * srv);
static void gda_dict_field_dispose (GObject   * object);
static void gda_dict_field_finalize (GObject   * object);

static void gda_dict_field_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_dict_field_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);

/* XML storage interface */
static void        gda_dict_field_xml_storage_init (GdaXmlStorageIface *iface);
static gchar      *gda_dict_field_get_xml_id (GdaXmlStorage *iface);
static xmlNodePtr  gda_dict_field_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_dict_field_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* GdaEntityField interface */
static void           gda_dict_field_field_init       (GdaEntityFieldIface *iface);
static GdaEntity      *gda_dict_field_get_entity      (GdaEntityField *iface);
static GdaDictType    *gda_dict_field_get_data_type   (GdaEntityField *iface);

/* Renderer interface */
static void            gda_dict_field_renderer_init   (GdaRendererIface *iface);
static gchar          *gda_dict_field_render_as_sql   (GdaRenderer *iface, GdaParameterList *context, 
						       GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar          *gda_dict_field_render_as_str   (GdaRenderer *iface, GdaParameterList *context);

#ifdef GDA_DEBUG
static void            gda_dict_field_dump            (GdaDictField *field, guint offset);
#endif

/* When the DbTable or GdaDictType is destroyed */
static void destroyed_object_cb (GObject *obj, GdaDictField *field);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP_DB_TABLE,
	PROP_PLUGIN
};


/* private structure */
struct _GdaDictFieldPrivate
{
	GdaDictType     *data_type;
	GdaDictTable    *table;
	gint             length;     /* -1 if not applicable */
	gint             scale;      /* 0 if not applicable */
	GValue          *default_val;/* NULL if no default value */
	GdaDictFieldAttribute extra_attrs;/* OR'ed value of GdaDictFieldAttribute */
	gchar           *plugin;
};

/* module error */
GQuark gda_dict_field_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_dict_field_error");
	return quark;
}


GType
gda_dict_field_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDictFieldClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_dict_field_class_init,
			NULL,
			NULL,
			sizeof (GdaDictField),
			0,
			(GInstanceInitFunc) gda_dict_field_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_dict_field_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo field_info = {
			(GInterfaceInitFunc) gda_dict_field_field_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo renderer_info = {
			(GInterfaceInitFunc) gda_dict_field_renderer_init,
			NULL,
			NULL
		};
		
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDictField", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_ENTITY_FIELD, &field_info);
		g_type_add_interface_static (type, GDA_TYPE_RENDERER, &renderer_info);
	}
	return type;
}

static void 
gda_dict_field_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = gda_dict_field_get_xml_id;
	iface->save_to_xml = gda_dict_field_save_to_xml;
	iface->load_from_xml = gda_dict_field_load_from_xml;
}

static void
gda_dict_field_field_init (GdaEntityFieldIface *iface)
{
	iface->get_entity = gda_dict_field_get_entity;
	iface->get_data_type = gda_dict_field_get_data_type;
}

static void
gda_dict_field_renderer_init (GdaRendererIface *iface)
{
	iface->render_as_sql = gda_dict_field_render_as_sql;
	iface->render_as_str = gda_dict_field_render_as_str;
	iface->is_valid = NULL;
}

static void
gda_dict_field_class_init (GdaDictFieldClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_dict_field_dispose;
	object_class->finalize = gda_dict_field_finalize;

	/* Properties */
	object_class->set_property = gda_dict_field_set_property;
	object_class->get_property = gda_dict_field_get_property;
	g_object_class_install_property (object_class, PROP_DB_TABLE,
					 g_param_spec_object ("db_table", NULL, NULL, 
                                                               GDA_TYPE_DICT_TABLE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_PLUGIN,
                                         g_param_spec_string ("entry_plugin", NULL, NULL, NULL,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_dict_field_dump;
#endif
}


static void
gda_dict_field_init (GdaDictField * gda_dict_field)
{
	gda_dict_field->priv = g_new0 (GdaDictFieldPrivate, 1);
	gda_dict_field->priv->table = NULL;
	gda_dict_field->priv->data_type = NULL;
	gda_dict_field->priv->length = -1;
	gda_dict_field->priv->scale = 0;
	gda_dict_field->priv->extra_attrs = 0;
	gda_dict_field->priv->plugin = NULL;
}


/**
 * gda_dict_field_new
 * @dict: a #GdaDict object
 * @type:  a #GdaDictType object (the field's type)
 *
 * Creates a new GdaDictField object
 *
 * Returns: the new object
 */
GObject*
gda_dict_field_new (GdaDict *dict, GdaDictType *type)
{
	GObject   *obj;
	GdaDictField *gda_dict_field;

	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);
	if (type)
		g_return_val_if_fail (GDA_IS_DICT_TYPE (type), NULL);

	obj = g_object_new (GDA_TYPE_DICT_FIELD, "dict", ASSERT_DICT (dict), NULL);
	gda_dict_field = GDA_DICT_FIELD (obj);

	if (type)
		gda_dict_field_set_dict_type (gda_dict_field, type);
	
	return obj;
}

static void 
destroyed_object_cb (GObject *obj, GdaDictField *field)
{
	gda_object_destroy (GDA_OBJECT (field));
}

static void
gda_dict_field_dispose (GObject *object)
{
	GdaDictField *gda_dict_field;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_FIELD (object));

	gda_dict_field = GDA_DICT_FIELD (object);
	if (gda_dict_field->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		if (gda_dict_field->priv->table) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (gda_dict_field->priv->table),
							      G_CALLBACK (destroyed_object_cb), gda_dict_field);
			gda_dict_field->priv->table = NULL;
		}
		if (gda_dict_field->priv->data_type) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (gda_dict_field->priv->data_type),
							      G_CALLBACK (destroyed_object_cb), gda_dict_field);
			g_object_unref (gda_dict_field->priv->data_type);
			gda_dict_field->priv->data_type = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_dict_field_finalize (GObject *object)
{
	GdaDictField *gda_dict_field;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_FIELD (object));

	gda_dict_field = GDA_DICT_FIELD (object);
	if (gda_dict_field->priv) {
		g_free (gda_dict_field->priv->plugin);
		
		g_free (gda_dict_field->priv);
		gda_dict_field->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_dict_field_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaDictField *gda_dict_field;

	gda_dict_field = GDA_DICT_FIELD (object);
	if (gda_dict_field->priv) {
		switch (param_id) {
		case PROP_DB_TABLE: {
			if (gda_dict_field->priv->table) {
				g_signal_handlers_disconnect_by_func (G_OBJECT (gda_dict_field->priv->table),
								      G_CALLBACK (destroyed_object_cb), gda_dict_field);
				gda_dict_field->priv->table = NULL; /* TODO: unref it? */
			}

			GdaDictTable* ptr = GDA_DICT_TABLE (g_value_get_object (value));
			if (ptr && GDA_IS_DICT_TABLE (ptr)) {
				gda_dict_field->priv->table = GDA_DICT_TABLE (ptr); /* TODO: ref it? */
				gda_object_connect_destroy (ptr,
							 G_CALLBACK (destroyed_object_cb), gda_dict_field);
			}
			break;
                }
		case PROP_PLUGIN:
			g_free (gda_dict_field->priv->plugin);
			if (g_value_get_string (value))
				gda_dict_field->priv->plugin = g_strdup (g_value_get_string (value));
			else
				gda_dict_field->priv->plugin = NULL;
			break;
		}
	}
}

static void
gda_dict_field_get_property (GObject *object,
			guint param_id,
			GValue *value,
			GParamSpec *pspec)
{
	GdaDictField *gda_dict_field;
	gda_dict_field = GDA_DICT_FIELD (object);
	
	if (gda_dict_field->priv) {
		switch (param_id) {
		case PROP_DB_TABLE:
			g_value_set_object (value, G_OBJECT (gda_dict_field->priv->table));
			break;
		case PROP_PLUGIN:
			g_value_set_string (value, gda_dict_field->priv->plugin);
			break;
		}	
	}
}


/**
 * gda_dict_field_get_length
 * @field: a #GdaDictField  object
 *
 * Set the length of a field.
 *
 */
void
gda_dict_field_set_length (GdaDictField *field, gint length)
{
	gboolean changed = FALSE;

	g_return_if_fail (field && GDA_IS_DICT_FIELD (field));
	g_return_if_fail (field->priv);

	if (length <= 0) {
		changed = field->priv->length != -1;
		field->priv->length = -1;
	}
	else {
		changed = field->priv->length != length;
		field->priv->length = length;
	}

	/* signal the modification */
	if (changed)
		gda_object_signal_emit_changed (GDA_OBJECT (field));
}

/**
 * gda_dict_field_get_length
 * @field: a #GdaDictField  object
 *
 * Get the length of a field.
 *
 * Returns: the size of the corresponding data type has a fixed size, or -1
 */
gint
gda_dict_field_get_length (GdaDictField *field)
{
	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), -1);
	g_return_val_if_fail (field->priv, -1);

	return field->priv->length;
}

/**
 * gda_dict_field_get_scale
 * @field: a #GdaDictField  object
 *
 * Set the scale of a field.
 *
 */
void
gda_dict_field_set_scale (GdaDictField *field, gint scale)
{
	gboolean changed = FALSE;

	g_return_if_fail (field && GDA_IS_DICT_FIELD (field));
	g_return_if_fail (field->priv);
	if (scale <= 0) {
		changed = field->priv->scale != 0;
		field->priv->scale = 0;
	}
	else {
		changed = field->priv->scale != scale;
		field->priv->scale = scale;
	}

	/* signal the modification */
	if (changed)
		gda_object_signal_emit_changed (GDA_OBJECT (field));
}

/**
 * gda_dict_field_get_scale
 * @field: a #GdaDictField  object
 *
 * Get the scale of a field.
 *
 * Returns: the size of the corresponding data type has a fixed size, or -1
 */
gint
gda_dict_field_get_scale (GdaDictField *field)
{
	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), -1);
	g_return_val_if_fail (field->priv, -1);

	return field->priv->scale;
}

/**
 * gda_dict_field_get_constraints
 * @field: a #GdaDictField  object
 *
 * Get all the constraints which impact the given field. Constraints are of several type:
 * NOT NULL, primary key, foreign key, check constrains
 *
 * Returns: a new list of #GdaDictConstraint objects
 */
GSList *
gda_dict_field_get_constraints (GdaDictField *field)
{
	GSList *retval = NULL;
	GSList *list, *table_cons;

	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), NULL);
	g_return_val_if_fail (field->priv, NULL);
	g_return_val_if_fail (field->priv->table, NULL);

	table_cons = gda_dict_table_get_constraints (field->priv->table);
	list = table_cons;
	while (list) {
		if (gda_dict_constraint_uses_field (GDA_DICT_CONSTRAINT (list->data), field))
			retval = g_slist_append (retval, list->data);
		list = g_slist_next (list);
	}
	g_slist_free (table_cons);
	
	return retval;
}

/**
* gda_dict_field_set_dict_type
* @field: a #GdaDictField  object
* @type: a #GdaDictType object
*
* Sets the data type of the field
*/
void
gda_dict_field_set_dict_type (GdaDictField *field, GdaDictType *type)
{
	g_return_if_fail (field && GDA_IS_DICT_FIELD (field));
	g_return_if_fail (field->priv);
	g_return_if_fail (type && GDA_IS_DICT_TYPE (type));

	if (field->priv->data_type != type) {
		if (field->priv->data_type)
			g_signal_handlers_disconnect_by_func (G_OBJECT (field->priv->data_type),
							      G_CALLBACK (destroyed_object_cb), field);
		field->priv->data_type = type;
		g_object_ref (type);
		gda_object_connect_destroy (type, G_CALLBACK (destroyed_object_cb), field);
		
		/* signal the modification */
		gda_object_signal_emit_changed (GDA_OBJECT (field));
	}
}


/**
 * gda_dict_field_set_default_value
 * @field: a #GdaDictField object
 * @value: a #GValue value or NULL
 *
 * Sets (or replace) the default value for the field. WARNING: the default value's data type can be
 * different from the field's data type (this is the case for example if the default value is a 
 * function like Postgres's default value for the SERIAL data type).
 */
void
gda_dict_field_set_default_value (GdaDictField *field, const GValue *value)
{
	g_return_if_fail (field && GDA_IS_DICT_FIELD (field));
	g_return_if_fail (field->priv);

	if (gda_value_compare_ext (field->priv->default_val, (GValue *) value)) {
		if (field->priv->default_val) {
			gda_value_free (field->priv->default_val);
			field->priv->default_val = NULL;
		}
		
		if (value)
			field->priv->default_val = gda_value_copy ((GValue *) value);
		
		/* signal the modification */
		gda_object_signal_emit_changed (GDA_OBJECT (field));
	}
}

/**
 * gda_dict_field_get_default_value
 * @field: a #GdaDictField object
 * 
 * Get the default value for the field if ne exists
 *
 * Returns: the default value
 */
const GValue *
gda_dict_field_get_default_value (GdaDictField *field)
{
	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), NULL);
	g_return_val_if_fail (field->priv, NULL);

	return field->priv->default_val;
}

/**
 * gda_dict_field_is_null_allowed
 * @field: a #GdaDictField object
 *
 * Test if @field can be %NULL or not
 *
 * Returns:
 */
gboolean
gda_dict_field_is_null_allowed (GdaDictField *field)
{
	gboolean retval = TRUE;
	GSList *list, *table_cons;

	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), FALSE);
	g_return_val_if_fail (field->priv, FALSE);
	g_return_val_if_fail (field->priv->table, FALSE);

	table_cons = gda_dict_table_get_constraints (field->priv->table);
	list = table_cons;
	while (list && retval) {
		if ((gda_dict_constraint_get_constraint_type (GDA_DICT_CONSTRAINT (list->data))==CONSTRAINT_NOT_NULL) &&
		    gda_dict_constraint_uses_field (GDA_DICT_CONSTRAINT (list->data), field))
			retval = FALSE;
		list = g_slist_next (list);
	}
	g_slist_free (table_cons);
	
	return retval;
}

/**
 * gda_dict_field_is_pkey_part
 * @field: a #GdaDictField object
 *
 * Test if @field is part of a primary key constraint
 *
 * Returns:
 */
gboolean
gda_dict_field_is_pkey_part (GdaDictField *field)
{
	gboolean retval = FALSE;
	GSList *constraints, *list;

	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), FALSE);
	g_return_val_if_fail (field->priv, FALSE);
	g_return_val_if_fail (field->priv->table, FALSE);

	constraints = gda_dict_table_get_constraints (field->priv->table);
	list = constraints;
	while (list && !retval) {
		if ((gda_dict_constraint_get_constraint_type (GDA_DICT_CONSTRAINT (list->data)) == CONSTRAINT_PRIMARY_KEY) &&
		    gda_dict_constraint_uses_field (GDA_DICT_CONSTRAINT (list->data), field))
			retval = TRUE;
		list = g_slist_next (list);
	}
	g_slist_free (constraints);

	return retval;
}

/**
 * gda_dict_field_is_pkey_alone
 * @field: a #GdaDictField object
 *
 * Test if @field is alone a primary key constraint
 *
 * Returns:
 */
gboolean
gda_dict_field_is_pkey_alone (GdaDictField *field)
{
	gboolean retval = FALSE;
	GSList *constraints, *list;

	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), FALSE);
	g_return_val_if_fail (field->priv, FALSE);
	g_return_val_if_fail (field->priv->table, FALSE);

	constraints = gda_dict_table_get_constraints (field->priv->table);
	list = constraints;
	while (list && !retval) {
		if ((gda_dict_constraint_get_constraint_type (GDA_DICT_CONSTRAINT (list->data)) == CONSTRAINT_PRIMARY_KEY) &&
		    gda_dict_constraint_uses_field (GDA_DICT_CONSTRAINT (list->data), field)) {
			GSList *fields = gda_dict_constraint_pkey_get_fields (GDA_DICT_CONSTRAINT (list->data));
			retval = g_slist_length (fields) == 1 ? TRUE : FALSE;
			g_slist_free (fields);
		}
		list = g_slist_next (list);
	}
	g_slist_free (constraints);

	return retval;
}

/**
 * gda_dict_field_is_fkey_part
 * @field: a #GdaDictField object
 *
 * Test if @field is part of a foreign key constraint
 *
 * Returns:
 */
gboolean
gda_dict_field_is_fkey_part (GdaDictField *field)
{
	gboolean retval = FALSE;
	GSList *constraints, *list;

	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), FALSE);
	g_return_val_if_fail (field->priv, FALSE);
	g_return_val_if_fail (field->priv->table, FALSE);

	constraints = gda_dict_table_get_constraints (field->priv->table);
	list = constraints;
	while (list && !retval) {
		if ((gda_dict_constraint_get_constraint_type (GDA_DICT_CONSTRAINT (list->data)) == CONSTRAINT_FOREIGN_KEY) &&
		    gda_dict_constraint_uses_field (GDA_DICT_CONSTRAINT (list->data), field))
			retval = TRUE;
		list = g_slist_next (list);
	}
	g_slist_free (constraints);

	return retval;
}

/**
 * gda_dict_field_is_fkey_alone
 * @field: a #GdaDictField object
 *
 * Test if @field is alone a foreign key constraint
 *
 * Returns:
 */
gboolean
gda_dict_field_is_fkey_alone (GdaDictField *field)
{
	gboolean retval = FALSE;
	GSList *constraints, *list;

	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), FALSE);
	g_return_val_if_fail (field->priv, FALSE);
	g_return_val_if_fail (field->priv->table, FALSE);

	constraints = gda_dict_table_get_constraints (field->priv->table);
	list = constraints;
	while (list && !retval) {
		if ((gda_dict_constraint_get_constraint_type (GDA_DICT_CONSTRAINT (list->data)) == CONSTRAINT_FOREIGN_KEY) &&
		    gda_dict_constraint_uses_field (GDA_DICT_CONSTRAINT (list->data), field)) {
			GSList *fields = gda_dict_constraint_fkey_get_fields (GDA_DICT_CONSTRAINT (list->data));
			GSList *list2;
			retval = g_slist_length (fields) == 1 ? TRUE : FALSE;

			list2 = fields;
			while (list2) {
				g_free (list2->data);
				list2 = g_slist_next (list2);
			}
			g_slist_free (fields);
		}
		list = g_slist_next (list);
	}
	g_slist_free (constraints);

	return retval;	
}

/**
 * gda_dict_field_is_fkey_alone
 * @field: a #GdaDictField object
 * @attributes: the new attributes value
 *
 * Sets @field's extra attributes. The @attributes is an OR'ed value of all the possible
 * values in #GdaDictFieldAttribute.
 */
void
gda_dict_field_set_attributes (GdaDictField *field, GdaDictFieldAttribute attributes)
{
	g_return_if_fail (field && GDA_IS_DICT_FIELD (field));
	g_return_if_fail (field->priv);

	field->priv->extra_attrs = attributes;
}

/**
 * gda_dict_field_is_fkey_alone
 * @field: a #GdaDictField object
 *
 * Get @field's extra attributes. The @attributes is an OR'ed value of all the possible
 * values in #GdaDictFieldAttribute.
 *
 * Returns: the new attributes value
 */
guint
gda_dict_field_get_attributes (GdaDictField *field)
{
	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), 0);
	g_return_val_if_fail (field->priv, 0);

	return field->priv->extra_attrs;
}


#ifdef GDA_DEBUG
static void
gda_dict_field_dump (GdaDictField *field, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (field && GDA_IS_DICT_FIELD (field));
	
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (field->priv)
                g_print ("%s" D_COL_H1 "GdaDictField" D_COL_NOR " %s (%p)\n",
                         str, gda_object_get_name (GDA_OBJECT (field)), field);
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, field);
}
#endif


/* 
 * GdaEntityField interface implementation
 */
static GdaEntity *
gda_dict_field_get_entity (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_DICT_FIELD (iface)->priv, NULL);

	return GDA_ENTITY (GDA_DICT_FIELD (iface)->priv->table);
}

static GdaDictType *
gda_dict_field_get_data_type (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_DICT_FIELD (iface)->priv, NULL);

	return GDA_DICT_FIELD (iface)->priv->data_type;
}

/* 
 * GdaXmlStorage interface implementation
 */
static gchar *
gda_dict_field_get_xml_id (GdaXmlStorage *iface)
{
	gchar *t_xml_id, *tmp, *xml_id;

	g_return_val_if_fail (iface && GDA_IS_DICT_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_DICT_FIELD (iface)->priv, NULL);

	t_xml_id = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (GDA_DICT_FIELD (iface)->priv->table));
	tmp = gda_utility_build_encoded_id ("FI", gda_object_get_name (GDA_OBJECT (iface)));
	xml_id = g_strconcat (t_xml_id, ":", tmp, NULL);
	g_free (t_xml_id);
	g_free (tmp);
	
	return xml_id;
}

static xmlNodePtr
gda_dict_field_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	GdaDictField *field;
	gchar *str;

	g_return_val_if_fail (iface && GDA_IS_DICT_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_DICT_FIELD (iface)->priv, NULL);

	field = GDA_DICT_FIELD (iface);

	node = xmlNewNode (NULL, (xmlChar*)"gda_dict_field");
	
	str = gda_dict_field_get_xml_id (iface);
	xmlSetProp(node, (xmlChar*)"id", (xmlChar*)str);
	g_free (str);
	xmlSetProp(node, (xmlChar*)"name", (xmlChar*)gda_object_get_name (GDA_OBJECT (field)));
	if (gda_object_get_owner (GDA_OBJECT (field)))
		xmlSetProp(node, (xmlChar*)"owner", (xmlChar*)gda_object_get_owner (GDA_OBJECT (field)));
	xmlSetProp(node, (xmlChar*)"descr", (xmlChar*)gda_object_get_description (GDA_OBJECT (field)));
	str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (field->priv->data_type));
	xmlSetProp(node, (xmlChar*)"type", (xmlChar*)str);
	g_free (str);
	str = g_strdup_printf ("%d", field->priv->length);
	xmlSetProp(node, (xmlChar*)"length", (xmlChar*)str);
	g_free (str);
	str = g_strdup_printf ("%d", field->priv->scale);
	xmlSetProp(node, (xmlChar*)"scale", (xmlChar*)str);
	g_free (str);

	if (field->priv->default_val) {
		GdaDataHandler *dh;
		GType vtype;
		
		vtype = G_VALUE_TYPE (field->priv->default_val);
		xmlSetProp(node, (xmlChar*)"default_g_type", (xmlChar*)gda_g_type_to_string (vtype));

		dh = gda_dict_get_default_handler (gda_object_get_dict (GDA_OBJECT (field)), vtype);
		str = gda_data_handler_get_str_from_value (dh, field->priv->default_val);
		xmlSetProp(node, (xmlChar*)"default", (xmlChar*)str);
		g_free (str);
	}

	str = gda_utility_table_field_attrs_stringify (field->priv->extra_attrs);
	if (str) {
		xmlSetProp(node, (xmlChar*)"extra_attr", (xmlChar*)str);
		g_free (str);
	}

	if (field->priv->plugin && *(field->priv->plugin)) 
		xmlSetProp(node, (xmlChar*)"plugin", (xmlChar*)field->priv->plugin);

	return node;
}

static gboolean
gda_dict_field_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaDict *dict;
	GdaDictField *field;
	gchar *prop;
	gboolean name = FALSE, type = FALSE;

	g_return_val_if_fail (iface && GDA_IS_DICT_FIELD (iface), FALSE);
	g_return_val_if_fail (GDA_DICT_FIELD (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	field = GDA_DICT_FIELD (iface);
	dict = gda_object_get_dict (GDA_OBJECT (field));
	if (strcmp ((gchar*)node->name, "gda_dict_field")) {
		g_set_error (error,
			     GDA_DICT_FIELD_ERROR,
			     GDA_DICT_FIELD_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_dict_field>"));
		return FALSE;
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"name");
	if (prop) {
		name = TRUE;
		gda_object_set_name (GDA_OBJECT (field), prop);
		xmlFree (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"descr");
	if (prop) {
		gda_object_set_description (GDA_OBJECT (field), prop);
		xmlFree (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"owner");
	if (prop) {
		gda_object_set_owner (GDA_OBJECT (field), prop);
		xmlFree (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"type");
	if (prop) {
		if ((*prop == 'D') && prop+1 && (*(prop+1) == 'T')) {
			GdaDictType *dt = gda_dict_get_dict_type_by_xml_id (dict, prop);
			if (dt) 
				gda_dict_field_set_dict_type (field, dt);
			else {
				/* create a new custom data type */
				gchar *tmp, *tmp2;
				xmlNodePtr tnode;
				
				dt = GDA_DICT_TYPE (gda_dict_type_new (dict));
				tmp = gda_utility_build_decoded_id (NULL, prop + 2);
				gda_dict_type_set_sqlname (dt, tmp);
				g_free (tmp);
				gda_dict_type_set_g_type (dt, GDA_TYPE_BLOB);

				tnode = node->parent;
				g_assert (tnode && !strcmp ((gchar*)tnode->name, "gda_dict_table"));
				tmp = (gchar*)xmlGetProp(tnode, (xmlChar*)"name");
				g_assert (tmp);
				tmp2 = (gchar*)g_strdup_printf (_("Custom data type, declared for the %s.%s field"),
							tmp, gda_object_get_name (GDA_OBJECT (field)));
				xmlFree (tmp);
				gda_object_set_description (GDA_OBJECT (dt), tmp2);
				g_free (tmp2);
				gda_dict_declare_object (dict, GDA_OBJECT(dt));
				gda_dict_field_set_dict_type (field, dt);
				g_object_unref (dt);
			}
			type = TRUE;
		}
		xmlFree (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"length");
	if (prop) {
		field->priv->length = atoi (prop);
		xmlFree (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"scale");
	if (prop) {
		field->priv->scale = atoi (prop);
		xmlFree (prop);
	}
	
	prop = (gchar*)xmlGetProp(node, (xmlChar*)"default");
	if (prop) {
		gchar *str2;

		str2 = (gchar*)xmlGetProp(node, (xmlChar*)"default_g_type");
		if (str2) {
			GType vtype;
			GdaDataHandler *dh;
			GValue *value;
			
			vtype = gda_g_type_from_string (str2);
			if (vtype == G_TYPE_INVALID) {
				g_set_error (error,
					     GDA_DICT_FIELD_ERROR,
					     GDA_DICT_FIELD_XML_LOAD_ERROR,
					     _("Unknown GDA data type '%s'"), str2);
				xmlFree (str2);
				return FALSE;				
			}
			dh = gda_dict_get_default_handler (dict, vtype);
			if (dh) {
				value = gda_data_handler_get_value_from_str (dh, prop, vtype);
				gda_dict_field_set_default_value (field, value);
				gda_value_free (value);
			}
			else {
				g_set_error (error,
					     GDA_DICT_FIELD_ERROR,
					     GDA_DICT_FIELD_XML_LOAD_ERROR,
					     _("Could not find a data handler for data type '%s'"), str2);
				xmlFree (str2);
				return FALSE;
			}
			xmlFree (str2);
		}
		xmlFree (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"extra_attr");
	if (prop) {
		gda_dict_field_set_attributes (field, gda_utility_table_field_attrs_parse (prop));
		xmlFree (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"plugin");
	if (prop) {
		g_object_set (G_OBJECT (field), "entry_plugin", prop, NULL);
		xmlFree (prop);
	}

	if (name && type)
		return TRUE;
	else {
		g_set_error (error,
			     GDA_DICT_FIELD_ERROR,
			     GDA_DICT_FIELD_XML_LOAD_ERROR,
			     _("Missing required attributes for <gda_dict_field>"));
		return FALSE;
	}
}


/*
 * GdaRenderer interface implementation
 */

static gchar *
gda_dict_field_render_as_sql (GdaRenderer *iface, GdaParameterList *context, 
			      GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	gchar *str = NULL;

	g_return_val_if_fail (iface && GDA_IS_DICT_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_DICT_FIELD (iface)->priv, NULL);
	
        /* TODO: */
	TO_IMPLEMENT;
	return str;
}

static gchar *
gda_dict_field_render_as_str (GdaRenderer *iface, GdaParameterList *context)
{
	gchar *str = NULL;

	g_return_val_if_fail (iface && GDA_IS_DICT_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_DICT_FIELD (iface)->priv, NULL);
	
	TO_IMPLEMENT;
	return str;
}
