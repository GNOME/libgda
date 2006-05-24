/* gda-query-field-value.c
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
#include "gda-query-field-value.h"
#include "gda-xml-storage.h"
#include "gda-entity-field.h"
#include "gda-renderer.h"
#include "gda-referer.h"
#include "gda-entity.h"
#include "gda-query.h"
#include "gda-data-handler.h"
#include "gda-parameter.h"
#include "gda-object-ref.h"
#include "gda-parameter-list.h"
#include "gda-connection.h"
#include "gda-server-provider.h"
#include "gda-dict-type.h"

/* 
 * Main static functions 
 */
static void gda_query_field_value_class_init (GdaQueryFieldValueClass * class);
static void gda_query_field_value_init (GdaQueryFieldValue *qf);
static void gda_query_field_value_dispose (GObject *object);
static void gda_query_field_value_finalize (GObject *object);

static void gda_query_field_value_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_query_field_value_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);

/* XML storage interface */
static void        gda_query_field_value_xml_storage_init (GdaXmlStorageIface *iface);
static xmlNodePtr  gda_query_field_value_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_query_field_value_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* Field interface */
static void               gda_query_field_value_field_init      (GdaEntityFieldIface *iface);
static GdaEntity         *gda_query_field_value_get_entity      (GdaEntityField *iface);
static GType              gda_query_field_value_get_igda_type    (GdaEntityField *iface);
static GdaDictType       *gda_query_field_value_get_data_type   (GdaEntityField *iface);

/* Renderer interface */
static void            gda_query_field_value_renderer_init      (GdaRendererIface *iface);
static gchar          *gda_query_field_value_render_as_sql   (GdaRenderer *iface, GdaParameterList *context, guint options, GError **error);
static gchar          *gda_query_field_value_render_as_str   (GdaRenderer *iface, GdaParameterList *context);

/* Referer interface */
static void        gda_query_field_value_referer_init        (GdaRefererIface *iface);
static void        gda_query_field_value_replace_refs        (GdaReferer *iface, GHashTable *replacements);

/* virtual functions */
static GObject    *gda_query_field_value_copy           (GdaQueryField *orig);
static gboolean    gda_query_field_value_is_equal       (GdaQueryField *qfield1, GdaQueryField *qfield2);


/* When the GdaQuery or GdaQueryTarget is destroyed */
static void destroyed_object_cb (GdaObject *obj, GdaQueryFieldValue *field);
static void destroyed_type_cb (GdaObject *obj, GdaQueryFieldValue *field);
static void destroyed_restrict_cb (GdaObject *obj, GdaQueryFieldValue *field);

static GSList   *gda_query_field_value_get_params (GdaQueryField *qfield);

static gboolean gda_query_field_value_render_find_value (GdaQueryFieldValue *field, GdaParameterList *context,
					       const GValue **value_found, GdaParameter **param_source);

#ifdef GDA_DEBUG
static void gda_query_field_value_dump (GdaQueryFieldValue *field, guint offset);
#endif

/* get a pointer to the parents to be able to cvalue their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP_QUERY,
	PROP_GDA_TYPE,
	PROP_RESTRICT_MODEL,
        PROP_RESTRICT_COLUMN,
	PROP_ENTRY_PLUGIN
};


/* private structure */
struct _GdaQueryFieldValuePrivate
{
	GdaQuery              *query;
	GType                  gda_type;
	GdaDictType           *dict_type;

	GValue                *value;        /* MUST either be NULL, or of type GDA_VALUE_NULL or 'type' */
	GValue                *default_value;/* CAN either be NULL, or of any type */
	gboolean               is_parameter;
	gboolean               is_null_allowed;

	GdaDataModel          *restrict_model;
	gint                   restrict_col;

	gchar                 *plugin;       /* specific plugin to be used */
};


/* module error */
GQuark gda_query_field_value_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_query_field_value_error");
	return quark;
}


GType
gda_query_field_value_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryFieldValueClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_field_value_class_init,
			NULL,
			NULL,
			sizeof (GdaQueryFieldValue),
			0,
			(GInstanceInitFunc) gda_query_field_value_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_query_field_value_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo field_info = {
			(GInterfaceInitFunc) gda_query_field_value_field_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo renderer_info = {
			(GInterfaceInitFunc) gda_query_field_value_renderer_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo referer_info = {
			(GInterfaceInitFunc) gda_query_field_value_referer_init,
			NULL,
			NULL
		};
		
		
		type = g_type_register_static (GDA_TYPE_QUERY_FIELD, "GdaQueryFieldValue", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_ENTITY_FIELD, &field_info);
		g_type_add_interface_static (type, GDA_TYPE_RENDERER, &renderer_info);
		g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
	}
	return type;
}

static void 
gda_query_field_value_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = NULL;
	iface->save_to_xml = gda_query_field_value_save_to_xml;
	iface->load_from_xml = gda_query_field_value_load_from_xml;
}

static void
gda_query_field_value_field_init (GdaEntityFieldIface *iface)
{
	iface->get_entity = gda_query_field_value_get_entity;
	iface->get_gda_type = gda_query_field_value_get_igda_type;
	iface->get_data_type = gda_query_field_value_get_data_type;
}

static void
gda_query_field_value_renderer_init (GdaRendererIface *iface)
{
	iface->render_as_sql = gda_query_field_value_render_as_sql;
	iface->render_as_str = gda_query_field_value_render_as_str;
	iface->is_valid = NULL;
}

static void
gda_query_field_value_referer_init (GdaRefererIface *iface)
{
        iface->activate = NULL;
        iface->deactivate = NULL;
        iface->is_active = NULL;
        iface->get_ref_objects = NULL;
        iface->replace_refs = gda_query_field_value_replace_refs;
}

static void
gda_query_field_value_class_init (GdaQueryFieldValueClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_query_field_value_dispose;
	object_class->finalize = gda_query_field_value_finalize;

	/* Properties */
	object_class->set_property = gda_query_field_value_set_property;
	object_class->get_property = gda_query_field_value_get_property;
	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_pointer ("query", "Query to which the field belongs", NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE |
								G_PARAM_CONSTRUCT_ONLY)));
        g_object_class_install_property (object_class, PROP_GDA_TYPE,
                                         g_param_spec_ulong ("gda_type", "Gda data type", NULL,
							   0, G_MAXULONG, GDA_TYPE_NULL,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_RESTRICT_MODEL,
                                         g_param_spec_pointer ("restrict_model", NULL, NULL,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (object_class, PROP_RESTRICT_COLUMN,
                                         g_param_spec_int ("restrict_column", NULL, NULL,
							   0, G_MAXINT, 0,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_ENTRY_PLUGIN,
                                         g_param_spec_string ("entry_plugin", NULL, NULL, NULL,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_query_field_value_dump;
#endif
	GDA_QUERY_FIELD_CLASS (class)->copy = gda_query_field_value_copy;
	GDA_QUERY_FIELD_CLASS (class)->is_equal = gda_query_field_value_is_equal;
	GDA_QUERY_FIELD_CLASS (class)->is_list = NULL;
	GDA_QUERY_FIELD_CLASS (class)->get_params = gda_query_field_value_get_params;
}

static void
gda_query_field_value_init (GdaQueryFieldValue *gda_query_field_value)
{
	gda_query_field_value->priv = g_new0 (GdaQueryFieldValuePrivate, 1);
	gda_query_field_value->priv->query = NULL;
	gda_query_field_value->priv->gda_type = G_TYPE_INVALID;
	gda_query_field_value->priv->dict_type = NULL;
	gda_query_field_value->priv->value = NULL;
	gda_query_field_value->priv->default_value = NULL;
	gda_query_field_value->priv->is_parameter = FALSE;
	gda_query_field_value->priv->is_null_allowed = FALSE;
	gda_query_field_value->priv->plugin = NULL;

	gda_query_field_value->priv->restrict_model = NULL;
	gda_query_field_value->priv->restrict_col = 0;
}


/**
 * gda_query_field_value_new
 * @query: a #GdaQuery in which the new object will be
 * @type: the GDA type for the value
 *
 * Creates a new GdaQueryFieldValue object which represents a value or a parameter.
 *
 * Returns: the new object
 */
GObject*
gda_query_field_value_new (GdaQuery *query, GType type)
{
	GObject *obj;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);

	obj = g_object_new (GDA_TYPE_QUERY_FIELD_VALUE, "dict", gda_object_get_dict (GDA_OBJECT (query)), 
			    "query", query, "gda_type", type, NULL);

	return obj;
}


static void 
destroyed_object_cb (GdaObject *obj, GdaQueryFieldValue *field)
{
	gda_object_destroy (GDA_OBJECT (field));
}

static void 
destroyed_type_cb (GdaObject *obj, GdaQueryFieldValue *field)
{
	g_assert ((GdaObject *) field->priv->dict_type == obj);
	g_signal_handlers_disconnect_by_func (G_OBJECT (field->priv->dict_type),
					      G_CALLBACK (destroyed_type_cb), field);
	field->priv->dict_type = NULL;
}

static void
gda_query_field_value_dispose (GObject *object)
{
	GdaQueryFieldValue *field;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD_VALUE (object));

	field = GDA_QUERY_FIELD_VALUE (object);
	if (field->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));
		if (field->priv->query) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (field->priv->query),
							      G_CALLBACK (destroyed_object_cb), field);
			field->priv->query = NULL;
		}
		if (field->priv->dict_type) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (field->priv->dict_type),
							      G_CALLBACK (destroyed_type_cb), field);
			field->priv->dict_type = NULL;
		}
		if (field->priv->value) {
			gda_value_free (field->priv->value);
			field->priv->value = NULL;
		}
		if (field->priv->default_value) {
			gda_value_free (field->priv->default_value);
			field->priv->default_value = NULL;
		}
		if (field->priv->restrict_model)
			destroyed_restrict_cb ((GdaObject*) field->priv->restrict_model, field);
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_query_field_value_finalize (GObject   * object)
{
	GdaQueryFieldValue *field;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD_VALUE (object));

	field = GDA_QUERY_FIELD_VALUE (object);
	if (field->priv) {
		if (field->priv->plugin)
			g_free (field->priv->plugin);

		g_free (field->priv);
		field->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_query_field_value_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdaQueryFieldValue *field;
	gpointer ptr;
	const gchar *str;
	guint id;

	field = GDA_QUERY_FIELD_VALUE (object);
	if (field->priv) {
		switch (param_id) {
		case PROP_QUERY:
			ptr = g_value_get_pointer (value);
			g_return_if_fail (ptr && GDA_IS_QUERY (ptr));

			if (field->priv->query) {
				if (field->priv->query == GDA_QUERY (ptr))
					return;

				g_signal_handlers_disconnect_by_func (G_OBJECT (field->priv->query),
								      G_CALLBACK (destroyed_object_cb), field);
			}

			field->priv->query = GDA_QUERY (ptr);
			gda_object_connect_destroy (ptr,
						    G_CALLBACK (destroyed_object_cb), field);
			g_object_get (G_OBJECT (ptr), "field_serial", &id, NULL);
			gda_query_object_set_int_id (GDA_QUERY_OBJECT (field), id);
			break;
		case PROP_GDA_TYPE:
			field->priv->gda_type = g_value_get_ulong (value);
			break;
		case PROP_RESTRICT_MODEL:
			ptr = g_value_get_pointer (value);
			g_return_if_fail (gda_query_field_value_restrict (field, 
									  (GdaDataModel *)ptr, -1,
									  NULL));
			break;
		case PROP_RESTRICT_COLUMN:
			field->priv->restrict_col = g_value_get_int (value);
			break;
		case PROP_ENTRY_PLUGIN:
			str =  g_value_get_string (value);
			if (field->priv->plugin) {
				g_free (field->priv->plugin);
				field->priv->plugin = NULL;
			}
			if (str)
				field->priv->plugin = g_strdup (str);
			break;
		}
	}
}

static void
gda_query_field_value_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	GdaQueryFieldValue *field = GDA_QUERY_FIELD_VALUE (object);
	
	if (field->priv) {
		switch (param_id) {
		case PROP_QUERY:
			g_value_set_pointer (value, field->priv->query);
			break;
		case PROP_GDA_TYPE:
			g_value_set_ulong (value, field->priv->gda_type);
			break;
		case PROP_RESTRICT_MODEL:
			g_value_set_pointer (value, field->priv->restrict_model);
			break;
		case PROP_RESTRICT_COLUMN:
			g_value_set_int (value, field->priv->restrict_col);
			break;
		case PROP_ENTRY_PLUGIN:
			g_value_set_string (value, field->priv->plugin);
			break;
		}	
	}
}

static GObject *
gda_query_field_value_copy (GdaQueryField *orig)
{
	GdaQueryFieldValue *qf, *nqf;
	GObject *obj;
	g_assert (GDA_IS_QUERY_FIELD_VALUE (orig));
	qf = GDA_QUERY_FIELD_VALUE (orig);

	obj = gda_query_field_value_new (qf->priv->query, qf->priv->gda_type);
	nqf = GDA_QUERY_FIELD_VALUE (obj);
	if (qf->priv->dict_type)
		gda_query_field_value_set_dict_type (nqf, qf->priv->dict_type);

	if (qf->priv->value)
		nqf->priv->value = gda_value_copy (qf->priv->value);
	if (qf->priv->default_value)
		nqf->priv->default_value = gda_value_copy (qf->priv->default_value);
	nqf->priv->is_parameter = qf->priv->is_parameter;
	nqf->priv->is_null_allowed = qf->priv->is_null_allowed;
	
	gda_query_field_value_restrict (nqf, qf->priv->restrict_model, qf->priv->restrict_col, NULL);

	if (gda_object_get_name (GDA_OBJECT (orig)))
		gda_object_set_name (GDA_OBJECT (obj), gda_object_get_name (GDA_OBJECT (orig)));

	if (gda_object_get_description (GDA_OBJECT (orig)))
		gda_object_set_description (GDA_OBJECT (obj), gda_object_get_description (GDA_OBJECT (orig)));

	if (qf->priv->plugin)
		nqf->priv->plugin = g_strdup (qf->priv->plugin);


	return obj;
}

static gboolean
gda_query_field_value_is_equal (GdaQueryField *qfield1, GdaQueryField *qfield2)
{
	gboolean retval;
	GdaQueryFieldValue *qf1, *qf2;
	GValue *val1, *val2;
	GType t1 = GDA_TYPE_NULL, t2 = GDA_TYPE_NULL;

	/* it is here assumed that qfield1 and qfield2 are of the same type and refer to the same
	   query */
	g_assert (GDA_IS_QUERY_FIELD_VALUE (qfield1));
	g_assert (GDA_IS_QUERY_FIELD_VALUE (qfield2));
	qf1 = GDA_QUERY_FIELD_VALUE (qfield1);
	qf2 = GDA_QUERY_FIELD_VALUE (qfield2);

	/* comparing values */
	val1 = qf1->priv->value;
	val2 = qf2->priv->value;
	if (val1)
		t1 = G_VALUE_TYPE (val1);
	if (val2)
		t2 = G_VALUE_TYPE (val2);

	retval = qf1->priv->dict_type == qf2->priv->dict_type ? TRUE : FALSE;

	if (retval) 
		retval = (t1 == t2) ? TRUE : FALSE;

	if (retval && (t1 != GDA_TYPE_NULL)) 
		retval = gda_value_compare (val1, val2) ? FALSE : TRUE;

	return retval;
}

/**
 * gda_query_field_value_set_value
 * @field: a #GdaQueryFieldValue object
 * @val: the value to be set, or %NULL
 *
 * Sets the value of @field, or removes it (if @val is %NULL)
 */
void
gda_query_field_value_set_value (GdaQueryFieldValue *field, const GValue *val)
{
	g_return_if_fail (GDA_IS_QUERY_FIELD_VALUE (field));
	g_return_if_fail (field->priv);	

	if (val)
		g_return_if_fail (G_VALUE_TYPE ((GValue *)val) == field->priv->gda_type);

	if (field->priv->value) {
		gda_value_free (field->priv->value);
		field->priv->value = NULL;
	}

	if (val)
		field->priv->value = gda_value_copy ((GValue *)val);
	gda_object_changed (GDA_OBJECT (field));
}

/**
 * gda_query_field_value_get_value
 * @field: a #GdaQueryFieldValue object
 *
 * Get the value stored by @field. If there is no value, but a default value exists, then the
 * default value is returned.n it's up to the caller to test if there is a default value for @field.
 * The default value can be of a different type than the one expected by @field.
 *
 * Returns: the value or NULL
 */
const GValue *
gda_query_field_value_get_value (GdaQueryFieldValue *field)
{
	g_return_val_if_fail (GDA_IS_QUERY_FIELD_VALUE (field), NULL);
	g_return_val_if_fail (field->priv, NULL);

	return field->priv->value;

	return NULL;
}

/**
 * gda_query_field_value_set_default_value
 * @field: a #GdaQueryFieldValue object
 * @default_val: the default value to be set, or %NULL
 *
 * Sets the default value of @field, or removes it (if @default_val is %NULL)
 */
void
gda_query_field_value_set_default_value (GdaQueryFieldValue *field, const GValue *default_val)
{
	g_return_if_fail (GDA_IS_QUERY_FIELD_VALUE (field));
	g_return_if_fail (field->priv);
	
	if (field->priv->default_value) {
		gda_value_free (field->priv->default_value);
		field->priv->default_value = NULL;
	}

	if (default_val) 
		field->priv->default_value = gda_value_copy ((GValue *)default_val);
}

/**
 * gda_query_field_value_get_default_value
 * @field: a #GdaQueryFieldValue object
 *
 * Get the default value stored by @field.
 *
 * Returns: the value or NULL
 */
const GValue *
gda_query_field_value_get_default_value (GdaQueryFieldValue *field)
{
	g_return_val_if_fail (GDA_IS_QUERY_FIELD_VALUE (field), NULL);
	g_return_val_if_fail (field->priv, NULL);

	return field->priv->default_value;
}

/**
 * gda_query_field_value_get_gda_type
 * @field: a #GdaQueryFieldValue object
 *
 * Get the GDA type of value stored within @field
 *
 * Returns: the type
 */
GType
gda_query_field_value_get_gda_type (GdaQueryFieldValue *field)
{
	g_return_val_if_fail (GDA_IS_QUERY_FIELD_VALUE (field), G_TYPE_INVALID);
	g_return_val_if_fail (field->priv, G_TYPE_INVALID);

	return field->priv->gda_type;
}

/**
 * gda_query_field_value_set_is_parameter
 * @field: a #GdaQueryFieldValue object
 * @is_param:
 *
 * Sets wether @field can be considered as a parameter
 */
void
gda_query_field_value_set_is_parameter (GdaQueryFieldValue *field, gboolean is_param)
{
	g_return_if_fail (GDA_IS_QUERY_FIELD_VALUE (field));
	g_return_if_fail (field->priv);

	field->priv->is_parameter = is_param;
}


/**
 * gda_query_field_value_is_parameter
 * @field: a #GdaQueryFieldValue object
 *
 * Tells if @field can be considered as a parameter
 *
 * Returns: TRUE if @field can be considered as a parameter
 */
gboolean
gda_query_field_value_is_parameter (GdaQueryFieldValue *field)
{
	g_return_val_if_fail (GDA_IS_QUERY_FIELD_VALUE (field), FALSE);
	g_return_val_if_fail (field->priv, FALSE);

	return field->priv->is_parameter;
}

static GSList *
gda_query_field_value_get_params (GdaQueryField *qfield)
{
	GSList *list = NULL;
	GdaQueryFieldValue *field = GDA_QUERY_FIELD_VALUE (qfield);
	
	if (field->priv->is_parameter) {
		GdaParameter *param;

		param = GDA_PARAMETER (g_object_new (GDA_TYPE_PARAMETER, 
						     "dict", gda_object_get_dict (GDA_OBJECT (qfield)),
						     "gda_type", field->priv->gda_type, NULL));
		gda_parameter_declare_param_user (param, GDA_OBJECT (qfield));
		
		/* parameter's attributes */
		gda_object_set_name (GDA_OBJECT (param), gda_object_get_name (GDA_OBJECT (field)));
		gda_object_set_description (GDA_OBJECT (param), gda_object_get_description (GDA_OBJECT (field)));
		gda_parameter_set_value (param, field->priv->value);

		if (field->priv->default_value)
			gda_parameter_set_default_value (param, field->priv->default_value);
		gda_parameter_set_not_null (param, !field->priv->is_null_allowed);

		/* specified plugin */
		if (field->priv->plugin)
			g_object_set (G_OBJECT (param), "entry_plugin", field->priv->plugin, NULL);

		/* possible values in a sub query */
		if (field->priv->restrict_model && (field->priv->restrict_col >= 0)) {
			gda_parameter_restrict_values (param, field->priv->restrict_model,
						       field->priv->restrict_col, NULL);
		}

		list = g_slist_append (list, param);
	}
	
	return list;
}

/**
 * gda_query_field_value_set_not_null
 * @field: a #GdaQueryFieldValue object
 * @not_null:
 *
 * Sets if a NULL value is acceptable for @field. If @not_null is TRUE, then @field
 * can't have a NULL value.
 */
void
gda_query_field_value_set_not_null (GdaQueryFieldValue *field, gboolean not_null)
{
	g_return_if_fail (GDA_IS_QUERY_FIELD_VALUE (field));
	g_return_if_fail (field->priv);

	field->priv->is_null_allowed = !not_null;
}

/**
 * gda_query_field_value_get_not_null
 * @field: a #GdaQueryFieldValue object
 *
 * Tells if @field can receive a NULL value.
 *
 * Returns: TRUE if @field can't have a NULL value
 */
gboolean
gda_query_field_value_get_not_null (GdaQueryFieldValue *field)
{
	g_return_val_if_fail (GDA_IS_QUERY_FIELD_VALUE (field), FALSE);
	g_return_val_if_fail (field->priv, FALSE);

	return !field->priv->is_null_allowed;
}


/**
 * gda_query_field_value_is_value_null
 * @field: a #GdaQueryFieldValue object
 * @context: a #GdaParameterList object
 *
 * Tells if @field represents a NULL value.
 *
 * Returns:
 */
gboolean
gda_query_field_value_is_value_null (GdaQueryFieldValue *field, GdaParameterList *context)
{
	gboolean value_found;
	const GValue *value;

	g_return_val_if_fail (GDA_IS_QUERY_FIELD_VALUE (field), FALSE);
	g_return_val_if_fail (field->priv, FALSE);

	value_found = gda_query_field_value_render_find_value (field, context, &value, NULL);
	if (!value_found) 
		value = field->priv->value;

	if (!value || gda_value_is_null ((GValue *)value))
		return TRUE;
	else
		return FALSE;
}


/**
 * gda_query_field_value_restrict
 * @field: a #GdaQueryFieldValue object
 * @model: a #GdaDataModel object
 * @col: a valid column in @model
 * @error: a place to store errors, or %NULL
 *
 * Restricts the possible values which @field can have among the calues stored in
 * @model at column @col.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_query_field_value_restrict (GdaQueryFieldValue *field, GdaDataModel *model, gint col, GError **error)
{
	g_return_val_if_fail (GDA_IS_QUERY_FIELD_VALUE (field), FALSE);
	g_return_val_if_fail (field->priv, FALSE);

	/* No check is done on the validity of @col or even its existance */
	/* Note: for internal implementation if @col<0, then it's ignored */

	if (field->priv->restrict_model == model) {
		if (col >= 0)
			field->priv->restrict_col = col;
		return TRUE;
	}
	
	if (field->priv->restrict_model)
		destroyed_restrict_cb (GDA_OBJECT (field->priv->restrict_model), field);

	if (col >= 0)
		field->priv->restrict_col = col;

	if (model) {
		field->priv->restrict_model = model;
		g_object_ref (model);
		gda_object_connect_destroy (model, G_CALLBACK (destroyed_restrict_cb), field);
	}

	return TRUE;
}

static void
destroyed_restrict_cb (GdaObject *obj, GdaQueryFieldValue *field)
{
	g_assert (field->priv->restrict_model == (GdaDataModel *)obj);
	g_object_unref (obj);
	field->priv->restrict_model = NULL;
	g_signal_handlers_disconnect_by_func (obj,
					      G_CALLBACK (destroyed_restrict_cb), field);
}


/**
 * gda_query_field_value_set_dict_type
 * @field: a #GdaQueryFieldValue object
 * @type: a #GdaDictType object, or %NULL
 *
 * Set the #GdaDictType type of @field
 */
void
gda_query_field_value_set_dict_type (GdaQueryFieldValue *field, GdaDictType *type)
{
	g_return_if_fail (GDA_IS_QUERY_FIELD_VALUE (field));
	g_return_if_fail (field->priv);
	if (type)
		g_return_if_fail (GDA_IS_DICT_TYPE (type));

	if (type == field->priv->dict_type)
		return;

	/* get rid of the old data type */
	if (field->priv->dict_type) {
		g_signal_handlers_disconnect_by_func (field->priv->dict_type,
						      G_CALLBACK (destroyed_type_cb), field);
		field->priv->dict_type = NULL;
	}
	
	if (type) {
		/* setting the new data type */
		field->priv->dict_type = type;
		gda_object_connect_destroy (type,
					    G_CALLBACK (destroyed_type_cb), field);
		
		if (field->priv->gda_type != gda_dict_type_get_gda_type (type)) {
			g_warning ("GdaQueryFieldValue: setting to GDA type incompatible dict type");
			field->priv->gda_type = gda_dict_type_get_gda_type (type);
		}
	}

	/* signal a change */
	gda_object_changed (GDA_OBJECT (field));
}

/**
 * gda_query_field_value_get_dict_type
 * @field: a #GdaQueryFieldValue object
 *
 * Get the #GdaDictType type of @field
 *
 * Returns: the #GdaDictType type
 */
GdaDictType *
gda_query_field_value_get_dict_type (GdaQueryFieldValue *field)
{
	g_return_val_if_fail (GDA_IS_QUERY_FIELD_VALUE (field), NULL);
	g_return_val_if_fail (field->priv, NULL);

	return field->priv->dict_type;
}

#ifdef GDA_DEBUG
static void
gda_query_field_value_dump (GdaQueryFieldValue *field, guint offset)
{
	gchar *str;
	gint i;
	GdaDataHandler *dh;
	GdaDict *dict;

	g_return_if_fail (GDA_IS_QUERY_FIELD_VALUE (field));
	dict = gda_object_get_dict (GDA_OBJECT (field));
	
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (field->priv) {
		gchar *val;
		dh = gda_dict_get_default_handler (dict, field->priv->gda_type);
                g_print ("%s" D_COL_H1 "GdaQueryFieldValue" D_COL_NOR " \"%s\" (%p, id=%s) ",
                         str, gda_object_get_name (GDA_OBJECT (field)), field, 
			 gda_object_get_id (GDA_OBJECT (field)));
		if (field->priv->is_parameter) 
			g_print ("is param, ");

		if (gda_referer_is_active (GDA_REFERER (field)))
			g_print ("Active, ");
		else
			g_print (D_COL_ERR "Inactive" D_COL_NOR ", ");

		if (gda_query_field_is_visible (GDA_QUERY_FIELD (field)))
			g_print ("Visible, ");
		if (field->priv->value) {
			val = gda_data_handler_get_sql_from_value (dh, field->priv->value);
			g_print ("Value: %s ", val);
			g_free (val);
		}

		if (field->priv->default_value) {
			GdaDataHandler *dhd;
			dhd = gda_dict_get_default_handler (dict, G_VALUE_TYPE (field->priv->default_value));
			val = gda_data_handler_get_sql_from_value (dhd, field->priv->default_value);
			g_print ("Default: %s ", val);
			g_free (val);
		}
		g_print ("\n");

		if (field->priv->restrict_model) {
			g_print ("%sValues restrictions: COLUMN %d\n", str, field->priv->restrict_col);
			gda_object_dump (GDA_OBJECT (field->priv->restrict_model), offset+5);
		}
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, field);
}
#endif


/* 
 * GdaEntityField interface implementation
 */
static GdaEntity *
gda_query_field_value_get_entity (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_VALUE (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_VALUE (iface)->priv, NULL);

	return GDA_ENTITY (GDA_QUERY_FIELD_VALUE (iface)->priv->query);
}

static GType
gda_query_field_value_get_igda_type (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_VALUE (iface), G_TYPE_INVALID);
	g_return_val_if_fail (GDA_QUERY_FIELD_VALUE (iface)->priv, G_TYPE_INVALID);

	return GDA_QUERY_FIELD_VALUE (iface)->priv->gda_type;
}

static GdaDictType *
gda_query_field_value_get_data_type (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_VALUE (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_VALUE (iface)->priv, NULL);

	return GDA_QUERY_FIELD_VALUE (iface)->priv->dict_type;
}

/* 
 * GdaXmlStorage interface implementation
 */
static xmlNodePtr
gda_query_field_value_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	GdaQueryFieldValue *field;
	gchar *str;
	const gchar *cstr;
	GdaDataHandler *dh;
	GdaDict *dict;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_VALUE (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_VALUE (iface)->priv, NULL);

	field = GDA_QUERY_FIELD_VALUE (iface);
	dict = gda_object_get_dict (GDA_OBJECT (field));

	node = xmlNewNode (NULL, "gda_query_fval");
	
	str = gda_xml_storage_get_xml_id (iface);
	xmlSetProp (node, "id", str);
	g_free (str);

	xmlSetProp (node, "name", gda_object_get_name (GDA_OBJECT (field)));
	if (gda_object_get_description (GDA_OBJECT (field)) && *gda_object_get_description (GDA_OBJECT (field)))
		xmlSetProp (node, "descr", gda_object_get_description (GDA_OBJECT (field)));
	if (! gda_query_field_is_visible (GDA_QUERY_FIELD (field)))
		xmlSetProp (node, "is_visible",  "f");
	if (gda_query_field_is_internal (GDA_QUERY_FIELD (field)))
		xmlSetProp (node, "is_internal", "t");

	xmlSetProp (node, "is_param", field->priv->is_parameter ? "t" : "f");
	xmlSetProp (node, "gda_type",  gda_type_to_string (field->priv->gda_type));
	if (field->priv->dict_type) {
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (field->priv->dict_type));
		xmlSetProp (node, "dict_type",  str);
		g_free (str);
	}

	dh = gda_dict_get_default_handler (dict, field->priv->gda_type);
	if (field->priv->value && (field->priv->gda_type != GDA_TYPE_NULL)) {
		str = gda_data_handler_get_str_from_value (dh, field->priv->value);
		xmlSetProp (node, "value", str);
		g_free (str);
	}
	if (field->priv->default_value) {
		GdaDataHandler *dhd = gda_dict_get_default_handler (dict, G_VALUE_TYPE (field->priv->default_value));
		GType vtype;
		
		str = gda_data_handler_get_str_from_value (dhd, field->priv->default_value);
		xmlSetProp (node, "default", str);
		g_free (str);
		vtype = G_VALUE_TYPE (field->priv->default_value);
		xmlSetProp (node, "default_gda_type", gda_type_to_string (vtype));
	}

	xmlSetProp (node, "null_ok", field->priv->is_null_allowed ? "t" : "f");
	if (field->priv->restrict_model) {
		str = g_strdup_printf ("DA%s:%d", 
				       gda_object_get_name (GDA_OBJECT (field->priv->restrict_model)),
				       field->priv->restrict_col);
		xmlSetProp (node, "restrict", str);
		g_free (str);
	}

	cstr = gda_query_field_get_alias (GDA_QUERY_FIELD (field));
	if (cstr && *cstr) 
		xmlSetProp (node, "alias", cstr);
	
	if (field->priv->plugin)
		xmlSetProp (node, "plugin", field->priv->plugin);

	return node;
}

static gboolean
gda_query_field_value_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaQueryFieldValue *field;
	gchar *prop;
	GdaDataHandler *dh = NULL;
	gboolean err = FALSE;
	GdaDict *dict;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_VALUE (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_VALUE (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	field = GDA_QUERY_FIELD_VALUE (iface);
	dict = gda_object_get_dict (GDA_OBJECT (field));
	if (strcmp (node->name, "gda_query_fval")) {
		g_set_error (error,
			     GDA_QUERY_FIELD_VALUE_ERROR,
			     GDA_QUERY_FIELD_VALUE_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_query_fval>"));
		return FALSE;
	}

	prop = xmlGetProp (node, "id");
	if (prop) {
		gchar *ptr, *tok;
		ptr = strtok_r (prop, ":", &tok);
		ptr = strtok_r (NULL, ":", &tok);
		if (strlen (ptr) < 3) {
			g_set_error (error,
				     GDA_QUERY_FIELD_FIELD_ERROR,
				     GDA_QUERY_FIELD_FIELD_XML_LOAD_ERROR,
				     _("XML ID for a query field should be QUxxx:QFyyy where xxx and yyy are numbers"));
			return FALSE;
		}
		gda_query_object_set_int_id (GDA_QUERY_OBJECT (field), atoi (ptr+2));
		g_free (prop);
	}

	prop = xmlGetProp (node, "name");
	if (prop) {
		gda_object_set_name (GDA_OBJECT (field), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "descr");
	if (prop) {
		gda_object_set_description (GDA_OBJECT (field), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "is_visible");
	if (prop) {
		gda_query_field_set_visible (GDA_QUERY_FIELD (field), (*prop == 't') ? TRUE : FALSE);
		g_free (prop);
	}

	prop = xmlGetProp (node, "is_internal");
	if (prop) {
		gda_query_field_set_internal (GDA_QUERY_FIELD (field), (*prop == 't') ? TRUE : FALSE);
		g_free (prop);
	}

	prop = xmlGetProp (node, "gda_type");
	if (prop) {
		field->priv->gda_type = gda_type_from_string (prop);
		dh = gda_dict_get_default_handler (dict, field->priv->gda_type);
		g_free (prop);

		if (field->priv->gda_type == GDA_TYPE_NULL)
			field->priv->value = gda_value_new_null ();
	}

	prop = xmlGetProp (node, "dict_type");
	if (prop) {
		GdaDictType *dict_type;
		dict_type = gda_dict_get_dict_type_by_xml_id (dict, prop);
		if (dict_type) {
			gda_query_field_value_set_dict_type (field, dict_type);
			dh = gda_dict_get_default_handler (dict, gda_dict_type_get_gda_type (field->priv->dict_type));
		}
		g_free (prop);
	}

	prop = xmlGetProp (node, "value");
	if (prop) {
		if (dh) {
			field->priv->value = gda_data_handler_get_value_from_str (dh, prop, field->priv->gda_type);
			if (!field->priv->value) {
				g_set_error (error,
					     GDA_QUERY_FIELD_VALUE_ERROR,
					     GDA_QUERY_FIELD_VALUE_XML_LOAD_ERROR,
					     _("Can't interpret '%s' as a value"), prop);
				g_free (prop);
				return FALSE;
			}
		}

		g_free (prop);
	}

	prop = xmlGetProp (node, "default");
	if (prop) {
		gchar *str2;

		str2 = xmlGetProp (node, "default_gda_type");
		if (str2) {
			GdaDataHandler *dh2;
			GType vtype;
			GValue *value;
			
			vtype = gda_type_from_string (str2);			
			dh2 = gda_dict_get_default_handler (dict, vtype);
			value = gda_data_handler_get_value_from_str (dh2, prop, vtype);
			field->priv->default_value = value;

			g_free (str2);
		}
		g_free (prop);
	}

	prop = xmlGetProp (node, "is_param");
	if (prop) {
		field->priv->is_parameter = (*prop == 't') ? TRUE : FALSE;
		g_free (prop);
	}

	prop = xmlGetProp (node, "null_ok");
	if (prop) {
		field->priv->is_null_allowed = (*prop == 't') ? TRUE : FALSE;
		g_free (prop);
	}

	prop = xmlGetProp (node, "plugin");
	if (prop) 
		field->priv->plugin = prop;

	prop = xmlGetProp (node, "restrict");
	if (prop) {
		GdaDataModel *model;
		gint col;
		TO_IMPLEMENT; /* find data model from name */
		if (!gda_query_field_value_restrict (field, model, col, error))
			err = TRUE;
		g_free (prop);
	}

	prop = xmlGetProp (node, "alias");
	if (prop) {
		gda_query_field_set_alias (GDA_QUERY_FIELD (field), prop);
		g_free (prop);
	}

	if (!dh && (field->priv->gda_type != GDA_TYPE_NULL)) {
		err = TRUE;
		g_set_error (error,
			     GDA_QUERY_FIELD_VALUE_ERROR,
			     GDA_QUERY_FIELD_VALUE_XML_LOAD_ERROR,
			     _("Missing required gda_type for <gda_query_fval>"));
	}

	if (!err) {
		if (!field->priv->is_parameter) {
			if (!field->priv->value) {
				err = TRUE;
				g_set_error (error,
					     GDA_QUERY_FIELD_VALUE_ERROR,
					     GDA_QUERY_FIELD_VALUE_XML_LOAD_ERROR,
					     _("Value field '%s' does not have a value!"),
					     gda_object_get_name (GDA_OBJECT (field)));
			}
		}
	}

	return !err;
}


/*
 * GdaRenderer interface implementation
 */

static gboolean
gda_query_field_value_render_find_value (GdaQueryFieldValue *field, GdaParameterList *context, 
			       const GValue **value_found, GdaParameter **param_source)
{
	const GValue *cvalue = NULL;
	gboolean found = FALSE;

	if (param_source)
		*param_source = NULL;
	if (value_found)
		*value_found = NULL;

	/* looking for a value into the context first */
	if (context) {
		GSList *list = context->parameters;
		GSList *for_fields;
		
		while (list && !found) {
			for_fields = gda_parameter_get_param_users (GDA_PARAMETER (list->data));
			if (g_slist_find (for_fields, field)) {
				if (param_source)
					*param_source = GDA_PARAMETER (list->data);
				found = TRUE;
				cvalue = gda_parameter_get_value (GDA_PARAMETER (list->data));
			}
			list = g_slist_next (list);
		}
	}
	
	/* using the field's value, if available */
	if (!cvalue && field->priv->value) {
		found = TRUE;
		cvalue = field->priv->value;
	}

	if (value_found)
		*value_found = cvalue;

	return found;
}

static gchar *
gda_query_field_value_render_as_sql (GdaRenderer *iface, GdaParameterList *context, guint options, GError **error)
{
	gchar *str = NULL;
	GdaQueryFieldValue *field;
	const GValue *value = NULL;
	gboolean sqlext = options & GDA_RENDERER_EXTRA_VAL_ATTRS;
	GdaDict *dict;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_VALUE (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_VALUE (iface)->priv, NULL);
	field = GDA_QUERY_FIELD_VALUE (iface);

	dict = gda_object_get_dict (GDA_OBJECT (field));

	if (field->priv->is_parameter) {
		gboolean value_found;
		GdaParameter *param_source = NULL;

		value_found = gda_query_field_value_render_find_value (field, context, &value, &param_source);
	
		/* actual rendering */
		if (value_found) {
			if (param_source && ! gda_parameter_is_valid (param_source)) {
				gchar *str, *str2;
				
				str2 = value ? gda_value_stringify ((GValue *)value) : g_strdup ("NULL");
				str = g_strdup_printf (_("Invalid parameter '%s' (value: %s)"),
						       gda_object_get_name (GDA_OBJECT (param_source)), str2);
				g_free (str2);
				
				g_set_error (error,
					     GDA_QUERY_FIELD_VALUE_ERROR,
					     GDA_QUERY_FIELD_VALUE_RENDER_ERROR,
					     str);
				g_free (str);
				
				/*g_print ("Param %p (%s) is invalid!\n", param_source, 
				  gda_object_get_name (GDA_OBJECT (param_source)));*/
				return NULL;
			}
		
			str = NULL;
			if (param_source) {
				gboolean use_default;
				g_object_get (G_OBJECT (param_source), "use_default_value", &use_default, NULL);
				if (use_default) {
					if (options & GDA_RENDERER_ERROR_IF_DEFAULT) {
						g_set_error (error,
							     GDA_QUERY_FIELD_VALUE_ERROR,
							     GDA_QUERY_FIELD_VALUE_DEFAULT_PARAM_ERROR,
							     "Default value requested");
						return NULL;
					}
					else
						str = g_strdup ("DEFAULT");
				}
			}
			if (!str) {
				if (value && (G_VALUE_TYPE ((GValue *)value) != GDA_TYPE_NULL)) {
					GdaDataHandler *dh;
					
					dh = gda_dict_get_handler (dict, field->priv->gda_type);
					if (dh)
						str = gda_data_handler_get_sql_from_value (dh, value);
				}
				if (!str)
					str = g_strdup ("NULL");	
			}
		}
		else {
			if (field->priv->is_null_allowed)
				str = g_strdup ("##");
			else {
				if (context) {
					g_set_error (error,
						     GDA_QUERY_FIELD_VALUE_ERROR,
						     GDA_QUERY_FIELD_VALUE_RENDER_ERROR,
						     _("No specified value"));
				}
				else
					str = g_strdup ("##");
			}
		}
		
	}
	else {
		value = field->priv->value;
		if (value && (G_VALUE_TYPE ((GValue *)value) != GDA_TYPE_NULL)) {
			GdaDataHandler *dh;
			
			dh = gda_dict_get_handler (dict, field->priv->gda_type);
			if (dh)
				str = gda_data_handler_get_sql_from_value (dh, value);
			else
				str = g_strdup ("NULL");
		}
		else
			str = g_strdup ("NULL");	
	}

	if (sqlext && field->priv->is_parameter) {
		GString *extra = g_string_new ("");
		const gchar *tmpstr;
		gchar *str2;
		gboolean isfirst = TRUE;

		/* add extra information about the value, as an extension of SQL */
		if (! field->priv->is_parameter) {
			g_string_append (extra, ":isparam=\"FALSE\"");
			isfirst = FALSE;
		}

		if (isfirst)
			isfirst = FALSE;
		else
			g_string_append (extra, " ");
		if (field->priv->dict_type)
			g_string_append_printf (extra, ":type=\"%s\"", 
						gda_object_get_name (GDA_OBJECT (field->priv->dict_type)));
		else
			/* print gda_type instead */
			g_string_append_printf (extra, ":type=\"%s\"", gda_type_to_string (field->priv->gda_type));

		tmpstr = gda_object_get_name (GDA_OBJECT (field));
		if (tmpstr && *tmpstr) {
			if (isfirst)
				isfirst = FALSE;
			else
				g_string_append (extra, " ");

			g_string_append_printf (extra, ":name=\"%s\"", tmpstr);
		}


		tmpstr = gda_object_get_description (GDA_OBJECT (field));
		if (tmpstr && *tmpstr) {
			if (isfirst)
				isfirst = FALSE;
			else
				g_string_append (extra, " ");
			g_string_append_printf (extra, ":descr=\"%s\"", tmpstr);
		}

		if (field->priv->is_null_allowed) {
			if (isfirst)
				isfirst = FALSE;
			else
				g_string_append (extra, " ");
			g_string_append (extra, ":nullok=\"TRUE\"");
		}

		str2 = g_strdup_printf ("%s [%s]", str, extra->str);
		g_free (str);
		str = str2;

		g_string_free (extra, TRUE);
	}

	return str;
}

static gchar *
gda_query_field_value_render_as_str (GdaRenderer *iface, GdaParameterList *context)
{
	gchar *str = NULL;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_VALUE (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_VALUE (iface)->priv, NULL);
	
	str = gda_query_field_value_render_as_sql (iface, context, 0, NULL);
	if (!str)
		str = g_strdup ("???");
	return str;
}


/*
 * GdaReferer interface implementation
 */
static void
gda_query_field_value_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	GdaQueryFieldValue *field;

        g_return_if_fail (iface && GDA_IS_QUERY_FIELD_VALUE (iface));
        g_return_if_fail (GDA_QUERY_FIELD_VALUE (iface)->priv);

        field = GDA_QUERY_FIELD_VALUE (iface);	
        if (field->priv->query) {
                GdaQuery *query = g_hash_table_lookup (replacements, field->priv->query);
                if (query) {
                        g_signal_handlers_disconnect_by_func (G_OBJECT (field->priv->query),
                                                              G_CALLBACK (destroyed_object_cb), field);
                        field->priv->query = query;
			gda_object_connect_destroy (query,
						    G_CALLBACK (destroyed_object_cb), field);
                }
        }
}
