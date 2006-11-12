/* gda-query-field-field.c
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
#include "gda-query-field-field.h"
#include "gda-xml-storage.h"
#include "gda-entity-field.h"
#include "gda-entity.h"
#include "gda-renderer.h"
#include "gda-referer.h"
#include "gda-object-ref.h"
#include "gda-query.h"
#include "gda-query-target.h"
#include "gda-dict.h"
#include "gda-connection.h"
#include "gda-server-provider.h"

/* 
 * Main static functions 
 */
static void gda_query_field_field_class_init (GdaQueryFieldFieldClass * class);
static void gda_query_field_field_init (GdaQueryFieldField *qf);
static void gda_query_field_field_dispose (GObject *object);
static void gda_query_field_field_finalize (GObject *object);

static void gda_query_field_field_set_property (GObject *object,
						guint param_id,
						const GValue *value,
						GParamSpec *pspec);
static void gda_query_field_field_get_property (GObject *object,
						guint param_id,
						GValue *value,
						GParamSpec *pspec);

/* XML storage interface */
static void         gda_query_field_field_xml_storage_init (GdaXmlStorageIface *iface);
static xmlNodePtr   gda_query_field_field_save_to_xml      (GdaXmlStorage *iface, GError **error);
static gboolean     gda_query_field_field_load_from_xml    (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* Field interface */
static void         gda_query_field_field_field_init      (GdaEntityFieldIface *iface);
static GdaEntity   *gda_query_field_field_get_entity      (GdaEntityField *iface);
static GdaDictType *gda_query_field_field_get_data_type   (GdaEntityField *iface);

/* Renderer interface */
static void         gda_query_field_field_renderer_init   (GdaRendererIface *iface);
static gchar       *gda_query_field_field_render_as_sql   (GdaRenderer *iface, GdaParameterList *context, guint options, GError **error);
static gchar       *gda_query_field_field_render_as_str   (GdaRenderer *iface, GdaParameterList *context);

/* Referer interface */
static void         gda_query_field_field_referer_init        (GdaRefererIface *iface);
static gboolean     gda_query_field_field_activate            (GdaReferer *iface);
static void         gda_query_field_field_deactivate          (GdaReferer *iface);
static gboolean     gda_query_field_field_is_active           (GdaReferer *iface);
static GSList      *gda_query_field_field_get_ref_objects     (GdaReferer *iface);
static void         gda_query_field_field_replace_refs        (GdaReferer *iface, GHashTable *replacements);

/* virtual functions */
static GObject     *gda_query_field_field_copy           (GdaQueryField *orig);
static gboolean     gda_query_field_field_is_equal       (GdaQueryField *qfield1, GdaQueryField *qfield2);

#ifdef GDA_DEBUG
static void         gda_query_field_field_dump           (GdaQueryFieldField *field, guint offset);
#endif


/* When the GdaQuery is destroyed */
static void destroyed_object_cb (GObject *obj, GdaQueryFieldField *field);
static void target_removed_cb (GdaQuery *query, GdaQueryTarget *target, GdaQueryFieldField *field);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP_QUERY,
	PROP_VALUE_PROVIDER_OBJECT,
	PROP_VALUE_PROVIDER_XML_ID,
	PROP_PLUGIN,
	PROP_TARGET_OBJ,
	PROP_TARGET_NAME,
	PROP_TARGET_ID,
	PROP_FIELD_OBJ,
	PROP_FIELD_NAME,
	PROP_FIELD_ID
};


/* private structure */
struct _GdaQueryFieldFieldPrivate
{
	GdaQuery      *query;
	GdaObjectRef  *target_ref; /* references a GdaQueryTarget */
	GdaObjectRef  *field_ref;  /* references a GdaEntityField in the entity behind the GdaQueryTarget */
	GdaObjectRef  *value_prov_ref;

	gchar         *plugin;       /* specific plugin to be used */
};


/* module error */
GQuark gda_query_field_field_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_query_field_field_error");
	return quark;
}


GType
gda_query_field_field_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryFieldFieldClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_field_field_class_init,
			NULL,
			NULL,
			sizeof (GdaQueryFieldField),
			0,
			(GInstanceInitFunc) gda_query_field_field_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_query_field_field_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo field_info = {
			(GInterfaceInitFunc) gda_query_field_field_field_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo renderer_info = {
			(GInterfaceInitFunc) gda_query_field_field_renderer_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo referer_info = {
			(GInterfaceInitFunc) gda_query_field_field_referer_init,
			NULL,
			NULL
		};
		
		
		type = g_type_register_static (GDA_TYPE_QUERY_FIELD, "GdaQueryFieldField", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_ENTITY_FIELD, &field_info);
		g_type_add_interface_static (type, GDA_TYPE_RENDERER, &renderer_info);
		g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
	}
	return type;
}

static void 
gda_query_field_field_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = NULL;
	iface->save_to_xml = gda_query_field_field_save_to_xml;
	iface->load_from_xml = gda_query_field_field_load_from_xml;
}

static void
gda_query_field_field_field_init (GdaEntityFieldIface *iface)
{
	iface->get_entity = gda_query_field_field_get_entity;
	iface->get_data_type = gda_query_field_field_get_data_type;
}

static void
gda_query_field_field_renderer_init (GdaRendererIface *iface)
{
	iface->render_as_sql = gda_query_field_field_render_as_sql;
	iface->render_as_str = gda_query_field_field_render_as_str;
	iface->is_valid = NULL;
}

static void
gda_query_field_field_referer_init (GdaRefererIface *iface)
{
        iface->activate = gda_query_field_field_activate;
        iface->deactivate = gda_query_field_field_deactivate;
        iface->is_active = gda_query_field_field_is_active;
        iface->get_ref_objects = gda_query_field_field_get_ref_objects;
        iface->replace_refs = gda_query_field_field_replace_refs;
}

static void
gda_query_field_field_class_init (GdaQueryFieldFieldClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_query_field_field_dispose;
	object_class->finalize = gda_query_field_field_finalize;

	/* Properties */
	object_class->set_property = gda_query_field_field_set_property;
	object_class->get_property = gda_query_field_field_get_property;
	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_pointer ("query", "Query to which the field belongs", NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE |
								G_PARAM_CONSTRUCT_ONLY)));

	g_object_class_install_property (object_class, PROP_VALUE_PROVIDER_OBJECT,
					 g_param_spec_pointer ("value_provider", NULL, NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_VALUE_PROVIDER_XML_ID,
					 g_param_spec_string ("value_provider_xml_id", NULL, NULL, NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_PLUGIN,
                                         g_param_spec_string ("entry_plugin", NULL, NULL, NULL,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property (object_class, PROP_TARGET_OBJ,
					 g_param_spec_pointer ("target", "A pointer to a GdaQueryTarget", NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_TARGET_NAME,
					 g_param_spec_string ("target_name", "Name or alias of a query target", NULL, NULL,
							      G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_TARGET_ID,
					 g_param_spec_string ("target_id", "XML ID of a query target", NULL, NULL,
							      G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_FIELD_OBJ,
					 g_param_spec_pointer ("field", "A pointer to a GdaEntityField", NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_FIELD_NAME,
					 g_param_spec_string ("field_name", "Name of an entity field", NULL, NULL,
							      G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_FIELD_ID,
					 g_param_spec_string ("field_id", "XML ID of an entity field", NULL, NULL,
							      G_PARAM_WRITABLE));
	
	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_query_field_field_dump;
#endif
	GDA_QUERY_FIELD_CLASS (class)->copy = gda_query_field_field_copy;
	GDA_QUERY_FIELD_CLASS (class)->is_equal = gda_query_field_field_is_equal;
	GDA_QUERY_FIELD_CLASS (class)->is_list = NULL;
	GDA_QUERY_FIELD_CLASS (class)->get_params = NULL;
}

static void
gda_query_field_field_init (GdaQueryFieldField *gda_query_field_field)
{
	gda_query_field_field->priv = g_new0 (GdaQueryFieldFieldPrivate, 1);
	gda_query_field_field->priv->query = NULL;
	gda_query_field_field->priv->target_ref = NULL;
	gda_query_field_field->priv->field_ref = NULL;
	gda_query_field_field->priv->value_prov_ref = NULL;
	gda_query_field_field->priv->plugin = NULL;
}

/**
 * gda_query_field_field_new
 * @query: a #GdaQuery in which the new object will be
 * @field: the name of the field to represent
 *
 * Creates a new #GdaQueryFieldField object which represents a given field.
 * @field can be among the following forms:
 * <itemizedlist>
 *   <listitem><para>field_name</para></listitem>
 *   <listitem><para>table_name.field_name</para></listitem>
 * </itemizedlist>
 *
 * Returns: the new object
 */
GdaQueryField *
gda_query_field_field_new (GdaQuery *query, const gchar *field)
{
	GObject *obj;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (field && *field, NULL);

	obj = g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
			    "dict", gda_object_get_dict (GDA_OBJECT (query)), 
			    "query", query, "field_name", field, NULL);
	return (GdaQueryField *) obj;
}

static void 
destroyed_object_cb (GObject *obj, GdaQueryFieldField *field)
{
	gda_object_destroy (GDA_OBJECT (field));
}

static void
target_removed_cb (GdaQuery *query, GdaQueryTarget *target, GdaQueryFieldField *field)
{
	GdaObject *base;

	base = gda_object_ref_get_ref_object (field->priv->target_ref);
	if (base && (GDA_QUERY_TARGET (base) == target)) 
		gda_object_destroy (GDA_OBJECT (field));
}

static void
gda_query_field_field_dispose (GObject *object)
{
	GdaQueryFieldField *ffield;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD_FIELD (object));

	ffield = GDA_QUERY_FIELD_FIELD (object);
	if (ffield->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		if (ffield->priv->value_prov_ref) {
			g_object_unref (G_OBJECT (ffield->priv->value_prov_ref));
			ffield->priv->value_prov_ref = NULL;
		}

		if (ffield->priv->query) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (ffield->priv->query),
							      G_CALLBACK (destroyed_object_cb), ffield);
			g_signal_handlers_disconnect_by_func (G_OBJECT (ffield->priv->query),
							      G_CALLBACK (target_removed_cb), ffield);
			ffield->priv->query = NULL;
		}
		if (ffield->priv->target_ref) {
			g_object_unref (G_OBJECT (ffield->priv->target_ref));
			ffield->priv->target_ref = NULL;
		}
		if (ffield->priv->field_ref) {
			g_object_unref (G_OBJECT (ffield->priv->field_ref));
			ffield->priv->field_ref = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_query_field_field_finalize (GObject   * object)
{
	GdaQueryFieldField *ffield;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD_FIELD (object));

	ffield = GDA_QUERY_FIELD_FIELD (object);
	if (ffield->priv) {
		if (ffield->priv->plugin)
			g_free (ffield->priv->plugin);

		g_free (ffield->priv);
		ffield->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_query_field_field_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdaQueryFieldField *ffield;
	gpointer ptr;
	const gchar *val;
	GdaDict *dict;
	guint id;
	const gchar *str;

	ffield = GDA_QUERY_FIELD_FIELD (object);
	if (ffield->priv) {
		switch (param_id) {
		case PROP_QUERY:
			ptr = g_value_get_pointer (value);
			g_return_if_fail (GDA_IS_QUERY (ptr));
			g_return_if_fail (gda_object_get_dict (GDA_OBJECT (ptr)) == gda_object_get_dict (GDA_OBJECT (ffield)));

			if (ffield->priv->query) {
				if (ffield->priv->query == GDA_QUERY (ptr))
					return;

				g_signal_handlers_disconnect_by_func (G_OBJECT (ffield->priv->query),
								      G_CALLBACK (destroyed_object_cb), ffield);
				g_signal_handlers_disconnect_by_func (G_OBJECT (ffield->priv->query),
								      G_CALLBACK (target_removed_cb), ffield);
			}

			ffield->priv->query = GDA_QUERY (ptr);
			gda_object_connect_destroy (ptr, 
						    G_CALLBACK (destroyed_object_cb), ffield);
			g_signal_connect (G_OBJECT (ptr), "target_removed",
					  G_CALLBACK (target_removed_cb), ffield);

			dict = gda_object_get_dict (GDA_OBJECT (ptr));
			ffield->priv->target_ref = GDA_OBJECT_REF (gda_object_ref_new (dict));
			g_object_set (G_OBJECT (ffield->priv->target_ref), "helper_ref", ptr, NULL);
			
			ffield->priv->field_ref = GDA_OBJECT_REF (gda_object_ref_new (dict));
			g_object_set (G_OBJECT (ffield->priv->field_ref), "helper_ref", ffield->priv->target_ref, NULL);

			g_object_get (G_OBJECT (ptr), "field_serial", &id, NULL);
			gda_query_object_set_int_id (GDA_QUERY_OBJECT (ffield), id);
			break;
		case PROP_VALUE_PROVIDER_OBJECT:
			ptr = g_value_get_pointer (value);
			if (ptr) {
				g_return_if_fail (GDA_IS_QUERY_FIELD (ptr));
				g_return_if_fail (gda_entity_field_get_entity (GDA_ENTITY_FIELD (ptr)) == 
						  GDA_ENTITY (ffield->priv->query));
				if (!ffield->priv->value_prov_ref)
					ffield->priv->value_prov_ref = GDA_OBJECT_REF (gda_object_ref_new (gda_object_get_dict (GDA_OBJECT (ffield))));
				gda_object_ref_set_ref_object_type (ffield->priv->value_prov_ref,
								 ptr, GDA_TYPE_ENTITY_FIELD);
			}
			else {
				if (ffield->priv->value_prov_ref) {
					g_object_unref (G_OBJECT (ffield->priv->value_prov_ref));
					ffield->priv->value_prov_ref = NULL;
				}
			}
			break;
		case PROP_VALUE_PROVIDER_XML_ID:
			val = g_value_get_string (value);
			if (val && *val) {
				gchar *qid, *str, *start, *tok;

				str = g_strdup (val);
				start = strtok_r (str, ":", &tok);
				qid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (ffield->priv->query));
				g_return_if_fail (!strcmp (start, qid));
				g_free (str);
				g_free (qid);

				if (!ffield->priv->value_prov_ref)
					ffield->priv->value_prov_ref = GDA_OBJECT_REF (gda_object_ref_new (gda_object_get_dict (GDA_OBJECT (ffield))));
				gda_object_ref_set_ref_name (ffield->priv->value_prov_ref,
							  GDA_TYPE_ENTITY_FIELD, REFERENCE_BY_XML_ID, val);
			}
			else {
				if (ffield->priv->value_prov_ref) {
					g_object_unref (G_OBJECT (ffield->priv->value_prov_ref));
					ffield->priv->value_prov_ref = NULL;
				}
			}
			break;
		case PROP_PLUGIN:
			val =  g_value_get_string (value);
			if (ffield->priv->plugin) {
				g_free (ffield->priv->plugin);
				ffield->priv->plugin = NULL;
			}
			if (val)
				ffield->priv->plugin = g_strdup (val);
			break;
		case PROP_TARGET_OBJ:
			ptr = g_value_get_pointer (value);
			g_return_if_fail (GDA_IS_QUERY_TARGET (ptr));
			gda_object_ref_set_ref_object (ffield->priv->target_ref, GDA_OBJECT (ptr));
			break;
		case PROP_TARGET_NAME:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (ffield->priv->target_ref, GDA_TYPE_QUERY_TARGET, 
						     REFERENCE_BY_NAME, str);
			break;
		case PROP_TARGET_ID:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (ffield->priv->target_ref, GDA_TYPE_QUERY_TARGET, 
						     REFERENCE_BY_XML_ID, str);
			break;
		case PROP_FIELD_OBJ:
			ptr = g_value_get_pointer (value);
			g_return_if_fail (GDA_IS_ENTITY_FIELD (ptr));
			gda_object_ref_set_ref_object (ffield->priv->field_ref, GDA_OBJECT (ptr));
			break;
		case PROP_FIELD_NAME:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (ffield->priv->field_ref, GDA_TYPE_ENTITY_FIELD, 
						     REFERENCE_BY_NAME, str);
			break;
		case PROP_FIELD_ID:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (ffield->priv->field_ref, GDA_TYPE_ENTITY_FIELD, 
						     REFERENCE_BY_XML_ID, str);
			break;
		}
	}
}

static void
gda_query_field_field_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	GdaQueryFieldField *ffield;
	ffield = GDA_QUERY_FIELD_FIELD (object);
	
	if (ffield->priv) {
		switch (param_id) {
		case PROP_QUERY:
			g_value_set_pointer (value, ffield->priv->query);
			break;
		case PROP_VALUE_PROVIDER_OBJECT:
			if (ffield->priv->value_prov_ref)
				g_value_set_pointer (value, 
						     gda_object_ref_get_ref_object (ffield->priv->value_prov_ref));
			else
				g_value_set_pointer (value, NULL);
			break;
		case PROP_VALUE_PROVIDER_XML_ID:
			if (ffield->priv->value_prov_ref)
				g_value_set_string (value, 
						    gda_object_ref_get_ref_name (ffield->priv->value_prov_ref,
									      NULL, NULL));
			else
				g_value_set_string (value, NULL);
			break;
		case PROP_PLUGIN:
			g_value_set_string (value, ffield->priv->plugin);
			if (! ffield->priv->plugin) {
				/* try to use the referenced field plugin */
				GdaObject *obj;
				obj = gda_object_ref_get_ref_object (ffield->priv->field_ref);
				if (obj) {
					gchar *plugin;
					g_object_get ((GObject*) obj, "entry_plugin", &plugin, NULL);
					if (plugin) 
						g_value_take_string (value, plugin);
				}
			}
			break;
		case PROP_TARGET_OBJ:
			g_value_set_pointer (value, gda_object_ref_get_ref_object (ffield->priv->target_ref));
			break;
		case PROP_FIELD_OBJ:
			g_value_set_pointer (value, gda_object_ref_get_ref_object (ffield->priv->field_ref));
			break;
		case PROP_TARGET_NAME:
		case PROP_TARGET_ID:
		case PROP_FIELD_NAME:
		case PROP_FIELD_ID:
			/* NO READ */
			g_assert_not_reached ();
			break;
		}	
	}
}

static GObject *
gda_query_field_field_copy (GdaQueryField *orig)
{
	GdaQueryFieldField *qf, *newqf;
	GObject *obj;
	GdaObject *ref;
	
	g_assert (GDA_IS_QUERY_FIELD_FIELD (orig));
	qf = GDA_QUERY_FIELD_FIELD (orig);

	obj = g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
			    "dict", gda_object_get_dict (GDA_OBJECT (qf)), 
			    "query", qf->priv->query, NULL);
	newqf = GDA_QUERY_FIELD_FIELD (obj);
	
	ref = gda_object_ref_get_ref_object (qf->priv->target_ref);
	if (ref)
		gda_object_ref_set_ref_object (newqf->priv->target_ref, ref);
	else {
		const gchar *ref_str;
		GdaObjectRefType ref_type;
		GType ref_gtype;

		ref_str = gda_object_ref_get_ref_object_name (qf->priv->target_ref);
		if (ref_str)
			g_object_set (G_OBJECT (newqf->priv->target_ref), "obj_name", ref_str, NULL);

		ref_str = gda_object_ref_get_ref_name (qf->priv->target_ref, &ref_gtype, &ref_type);
		if (ref_str)
			gda_object_ref_set_ref_name (newqf->priv->target_ref, ref_gtype, ref_type, ref_str);
	}

	ref = gda_object_ref_get_ref_object (qf->priv->field_ref);
	if (ref)
		gda_object_ref_set_ref_object (newqf->priv->field_ref, ref);
	else {
		const gchar *ref_str;
		GdaObjectRefType ref_type;
		GType ref_gtype;

		ref_str = gda_object_ref_get_ref_object_name (qf->priv->field_ref);
		if (ref_str)
			g_object_set (G_OBJECT (newqf->priv->field_ref), "obj_name", ref_str, NULL);

		ref_str = gda_object_ref_get_ref_name (qf->priv->field_ref, &ref_gtype, &ref_type);
		if (ref_str)
			gda_object_ref_set_ref_name (newqf->priv->field_ref, ref_gtype, ref_type, ref_str);
	}
	
	if (gda_object_get_name (GDA_OBJECT (orig)))
		gda_object_set_name (GDA_OBJECT (obj), gda_object_get_name (GDA_OBJECT (orig)));

	if (gda_object_get_description (GDA_OBJECT (orig)))
		gda_object_set_description (GDA_OBJECT (obj), gda_object_get_description (GDA_OBJECT (orig)));

	if (qf->priv->value_prov_ref) {
		GdaObject *ref = gda_object_ref_get_ref_object (qf->priv->value_prov_ref);
		if (ref)
			g_object_set (obj, "value_provider", ref, NULL);
		else
			g_object_set (obj, "value_provider_xml_id",
				      gda_object_ref_get_ref_name (qf->priv->value_prov_ref, NULL, NULL), NULL);
	}

	if (qf->priv->plugin)
		newqf->priv->plugin = g_strdup (qf->priv->plugin);

	return obj;
}

static gboolean
gda_query_field_field_is_equal (GdaQueryField *qfield1, GdaQueryField *qfield2)
{
	GdaObject *ref1, *ref2;
	gboolean retval;
	g_assert (GDA_IS_QUERY_FIELD_FIELD (qfield1));
	g_assert (GDA_IS_QUERY_FIELD_FIELD (qfield2));
	
	/* it is here assumed that qfield1 and qfield2 are of the same type and refer to the same
	   query */
	ref1 = gda_object_ref_get_ref_object (GDA_QUERY_FIELD_FIELD (qfield1)->priv->target_ref);
	ref2 = gda_object_ref_get_ref_object (GDA_QUERY_FIELD_FIELD (qfield2)->priv->target_ref);

	retval = (ref1 == ref2) ? TRUE : FALSE;
	if (retval) {
		ref1 = gda_object_ref_get_ref_object (GDA_QUERY_FIELD_FIELD (qfield1)->priv->field_ref);
		ref2 = gda_object_ref_get_ref_object (GDA_QUERY_FIELD_FIELD (qfield2)->priv->field_ref);
		retval = (ref1 == ref2) ? TRUE : FALSE;
	}

	return retval;
}

/**
 * gda_query_field_field_get_ref_field_name
 * @field: a #GdaQueryFieldField object
 *
 * Get the real name of the represented field. The returned name can be in either forms:
 * <itemizedlist>
 *   <listitem><para>field_name</para></listitem>
 *   <listitem><para>table_name.field_name</para></listitem>
 * </itemizedlist>
 *
 * Returns: represented field name (free the memory after usage)
 */
gchar *
gda_query_field_field_get_ref_field_name (GdaQueryFieldField *field)
{
	GdaObject *tobj, *fobj;
	const gchar *tname, *fname;
	g_return_val_if_fail (GDA_IS_QUERY_FIELD_FIELD (field), NULL);
	g_return_val_if_fail (field->priv, NULL);

	tobj = gda_object_ref_get_ref_object (field->priv->target_ref);
	if (tobj)
		tname = gda_query_target_get_alias (GDA_QUERY_TARGET (tobj));
	else
		tname = gda_object_ref_get_ref_name (field->priv->target_ref, NULL, NULL);

	fobj = gda_object_ref_get_ref_object (field->priv->field_ref);
	if (fobj)
		fname = gda_object_get_name (fobj);
	else
		fname = gda_object_ref_get_ref_name (field->priv->field_ref, NULL, NULL);

	if (tname && fname)
		return g_strdup_printf ("%s.%s", tname, fname);
	else {
		if (fname)
			return g_strdup (fname);
	}

	return NULL;
}

/**
 * gda_query_field_field_get_ref_field
 * @field: a #GdaQueryFieldField object
 *
 * Get the real #GdaEntityField object (well, the object which implements that interface)
 * referenced by @field
 *
 * Returns: the #GdaEntityField object, or NULL if @field is not active
 */
GdaEntityField *
gda_query_field_field_get_ref_field (GdaQueryFieldField *field)
{
	GdaObject *base;
	g_return_val_if_fail (GDA_IS_QUERY_FIELD_FIELD (field), NULL);
	g_return_val_if_fail (field->priv, NULL);

	base = gda_object_ref_get_ref_object (field->priv->field_ref);
	if (base)
		return GDA_ENTITY_FIELD (base);
	else
		return NULL;
}

/**
 * gda_query_field_field_get_target
 * @field: a #GdaQueryFieldField object
 *
 * Get the #GdaQueryTarget object @field 'belongs' to
 *
 * Returns: the #GdaQueryTarget object
 */
GdaQueryTarget *
gda_query_field_field_get_target (GdaQueryFieldField *field)
{
	GdaObject *base;
	g_return_val_if_fail (GDA_IS_QUERY_FIELD_FIELD (field), NULL);
	g_return_val_if_fail (field->priv, NULL);

	base = gda_object_ref_get_ref_object (field->priv->target_ref);
	if (base)
		return GDA_QUERY_TARGET (base);
	else
		return NULL;
}

#ifdef GDA_DEBUG
static void
gda_query_field_field_dump (GdaQueryFieldField *field, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (GDA_IS_QUERY_FIELD_FIELD (field));
	
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (field->priv) {
                g_print ("%s" D_COL_H1 "GdaQueryFieldField" D_COL_NOR " \"%s\" (%p, id=%s) ",
                         str, gda_object_get_name (GDA_OBJECT (field)), field, 
			 gda_object_get_id (GDA_OBJECT (field)));
		if (! gda_query_field_field_is_active (GDA_REFERER (field)))
			g_print (D_COL_ERR "Inactive" D_COL_NOR ", ");
		if (gda_query_field_is_visible (GDA_QUERY_FIELD (field)))
			g_print ("Visible, ");
		if (gda_query_field_is_internal (GDA_QUERY_FIELD (field)))
			g_print ("Internal, ");
		g_print ("references Target %s & Field %s", 
			 /* gda_object_ref_get_ref_object (field->priv->target_ref), */
			 gda_object_ref_get_ref_name (field->priv->target_ref, NULL, NULL),
			 /* gda_object_ref_get_ref_object (field->priv->field_ref), */
			 gda_object_ref_get_ref_name (field->priv->field_ref, NULL, NULL));

		if (field->priv->value_prov_ref) 
			g_print (" Value prov: %p (%s)\n",
				 gda_object_ref_get_ref_object (field->priv->value_prov_ref),
				 gda_object_ref_get_ref_name (field->priv->value_prov_ref, NULL, NULL));
		else
			g_print ("\n");
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, field);
}
#endif


/* 
 * GdaEntityField interface implementation
 */
static GdaEntity *
gda_query_field_field_get_entity (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv, NULL);

	return GDA_ENTITY (GDA_QUERY_FIELD_FIELD (iface)->priv->query);
}

static GdaDictType *
gda_query_field_field_get_data_type (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv, NULL);
	
	if (gda_query_field_field_activate (GDA_REFERER (iface))) {
		GdaEntityField *field;
		field = GDA_ENTITY_FIELD (gda_object_ref_get_ref_object (GDA_QUERY_FIELD_FIELD (iface)->priv->field_ref));
		return gda_entity_field_get_dict_type (field);
	}

	return NULL;
}

/* 
 * GdaXmlStorage interface implementation
 */
static xmlNodePtr
gda_query_field_field_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	GdaQueryFieldField *field;
	gchar *str;
	GdaObject *obj;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv, NULL);

	field = GDA_QUERY_FIELD_FIELD (iface);

	node = xmlNewNode (NULL, "gda_query_ffield");
	
	str = gda_xml_storage_get_xml_id (iface);
	xmlSetProp (node, "id", str);
	g_free (str);

	xmlSetProp (node, "name", gda_object_get_name (GDA_OBJECT (field)));
	if (gda_object_get_description (GDA_OBJECT (field)) && *gda_object_get_description (GDA_OBJECT (field)))
		xmlSetProp (node, "descr", gda_object_get_description (GDA_OBJECT (field)));
	
	obj = NULL;
	if (gda_object_ref_activate (field->priv->target_ref))
		obj = gda_object_ref_get_ref_object (field->priv->target_ref);

	if (obj) {
		gchar *xmlid;
		xmlid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (obj));
		xmlSetProp (node, "target", xmlid);
	}
	else {
		const gchar *cstr;

		cstr = gda_object_ref_get_ref_name (field->priv->target_ref, NULL, NULL);
		if (cstr)
			xmlSetProp (node, "target_name", cstr);
	}

	obj = NULL;
	if (gda_object_ref_activate (field->priv->field_ref))
		obj = gda_object_ref_get_ref_object (field->priv->field_ref);
	if (obj) {
		gchar *xmlid;
		xmlid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (obj));
		xmlSetProp (node, "object", xmlid);
	}
	else {
		const gchar *tmpstr;

		tmpstr = gda_object_ref_get_ref_name (field->priv->field_ref, NULL, NULL);
		if (tmpstr)
			xmlSetProp (node, "object_name", tmpstr);
	}

	if (! gda_query_field_is_visible (GDA_QUERY_FIELD (field)))
		xmlSetProp (node, "is_visible",  "f");
	if (gda_query_field_is_internal (GDA_QUERY_FIELD (field)))
		xmlSetProp (node, "is_internal", "t");

	if (field->priv->value_prov_ref)
		xmlSetProp (node, "value_prov", gda_object_ref_get_ref_name (field->priv->value_prov_ref, NULL, NULL));

	str = (gchar *) gda_query_field_get_alias (GDA_QUERY_FIELD (field));
	if (str && *str) 
		xmlSetProp (node, "alias", str);

	if (field->priv->plugin)
		xmlSetProp (node, "plugin", field->priv->plugin);

	return node;
}

static gboolean
gda_query_field_field_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaQueryFieldField *field;
	gchar *prop;
	gboolean target = FALSE;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	field = GDA_QUERY_FIELD_FIELD (iface);
	if (strcmp (node->name, "gda_query_ffield")) {
		g_set_error (error,
			     GDA_QUERY_FIELD_FIELD_ERROR,
			     GDA_QUERY_FIELD_FIELD_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_query_ffield>"));
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

	prop = xmlGetProp (node, "target");
	if (prop) {
		target = TRUE;
		gda_object_ref_set_ref_name (field->priv->target_ref, GDA_TYPE_QUERY_TARGET, REFERENCE_BY_XML_ID, prop);
		g_free (prop);
	}
	else {
		prop = xmlGetProp (node, "target_name");
		if (prop) {
			target = TRUE;
			gda_object_ref_set_ref_name (field->priv->target_ref, GDA_TYPE_QUERY_TARGET, REFERENCE_BY_NAME, prop);
			g_free (prop);
		}
	}

	prop = xmlGetProp (node, "object");
	if (prop) {
		target = TRUE;
		gda_object_ref_set_ref_name (field->priv->field_ref, GDA_TYPE_ENTITY_FIELD, REFERENCE_BY_XML_ID, prop);
		g_free (prop);
	}
	else {
		prop = xmlGetProp (node, "object_name");
		if (prop) {
			target = TRUE;
			gda_object_ref_set_ref_name (field->priv->field_ref, GDA_TYPE_ENTITY_FIELD, REFERENCE_BY_NAME, prop);
			g_free (prop);
		}
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

	prop = xmlGetProp (node, "value_prov");
	if (prop) {
		g_object_set (G_OBJECT (iface), "value_provider_xml_id", prop, NULL);
		g_free (prop);
	}

	prop = xmlGetProp (node, "alias");
	if (prop) {
		gda_query_field_set_alias (GDA_QUERY_FIELD (field), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "plugin");
	if (prop) 
		field->priv->plugin = prop;

	if (target)
		return TRUE;
	else {
		g_set_error (error,
			     GDA_QUERY_FIELD_FIELD_ERROR,
			     GDA_QUERY_FIELD_FIELD_XML_LOAD_ERROR,
			     _("Missing required attributes for <gda_query_ffield>"));
		return FALSE;
	}
}


/*
 * GdaRenderer interface implementation
 */

static gchar *
gda_query_field_field_render_as_sql (GdaRenderer *iface, GdaParameterList *context, guint options, GError **error)
{
	GdaQueryFieldField *field;
	GdaObject *fobj;
	const gchar *tname = NULL, *fname = NULL;
	gchar *retval = NULL;
	GdaServerProviderInfo *sinfo = NULL;
	GdaDict *dict;
	GdaConnection *cnc;
	gchar *tmp = NULL;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv, NULL);
	field = GDA_QUERY_FIELD_FIELD (iface);
	g_return_val_if_fail (field->priv, NULL);

	dict = gda_object_get_dict (GDA_OBJECT (iface));
	cnc = gda_dict_get_connection (dict);
	if (cnc)
		sinfo = gda_connection_get_infos (cnc);

	if (!sinfo || sinfo->supports_prefixed_fields) {
		if (! (options & GDA_RENDERER_FIELDS_NO_TARGET_ALIAS)) {
			GdaObject *tobj;
			
			tobj = gda_object_ref_get_ref_object (field->priv->target_ref);
			if (tobj)
				tname = gda_query_target_get_alias (GDA_QUERY_TARGET (tobj));
			else
				tname = gda_object_ref_get_ref_name (field->priv->target_ref, NULL, NULL);
		}
	}
		
	fobj = gda_object_ref_get_ref_object (field->priv->field_ref);
	if (fobj)
		fname = gda_object_get_name (fobj);
	else {
		fname = gda_object_ref_get_ref_object_name (field->priv->field_ref);
		if (!fname)
			fname = gda_object_ref_get_ref_name (field->priv->field_ref, NULL, NULL);
	}
	
	if (fname && (!sinfo || sinfo->quote_non_lc_identifiers)) {
		/* see if we need quotes around returned string */
		tmp = g_utf8_strdown (fname, -1);
		if (((*tmp <= '9') && (*tmp >= '0')) || strcmp (tmp, fname)) {
			g_free (tmp);
			fname = g_strdup_printf ("\"%s\"", fname);
			tmp = (gchar*) fname;
		}
	}

	if (tname && fname)
		retval = g_strdup_printf ("%s.%s", tname, fname);
	else {
		if (fname)
			retval = g_strdup (fname);
	}
	g_free (tmp);

	return retval;
}

static gchar *
gda_query_field_field_render_as_str (GdaRenderer *iface, GdaParameterList *context)
{
	GdaObject *tobj, *fobj;
	gchar *tname;
	const gchar *fname;
	GdaQueryFieldField *field;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv, NULL);
	field = GDA_QUERY_FIELD_FIELD (iface);

	tobj = gda_object_ref_get_ref_object (field->priv->target_ref);
	if (tobj) {
		GdaEntity *ent = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (tobj));
		if (ent)
			tname = g_strdup_printf ("%s(%s)", 
						 gda_object_get_name (GDA_OBJECT (ent)), 
						 gda_query_target_get_alias (GDA_QUERY_TARGET (tobj)));
		else
			tname = g_strdup (gda_query_target_get_alias (GDA_QUERY_TARGET (tobj)));
	}
	else {
		if (gda_object_ref_get_ref_name (field->priv->target_ref, NULL, NULL)) 
			tname = g_strdup (gda_object_ref_get_ref_name (field->priv->target_ref, NULL, NULL));
	}

	fobj = gda_object_ref_get_ref_object (field->priv->field_ref);
	if (fobj)
		fname = gda_object_get_name (fobj);
	else
		fname = gda_object_ref_get_ref_name (field->priv->field_ref, NULL, NULL);

	if (tname) {
		gchar *str;
		str = g_strdup_printf ("%s.%s", tname, fname);
		g_free (tname);
		return str;
	}
	else {
		if (fname)
			return g_strdup (fname);
		else
			return NULL;
	}
}


/*
 * GdaReferer interface implementation
 */
static gboolean
gda_query_field_field_activate (GdaReferer *iface)
{
	gboolean act1, act2;
	gboolean active = FALSE;
	GdaQueryFieldField *field;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv, FALSE);
	field = GDA_QUERY_FIELD_FIELD (iface);

	act1 = gda_object_ref_activate (field->priv->target_ref);
	act2 = gda_object_ref_activate (field->priv->field_ref);
	if (act1 && act2) {
		/* coherence test */
		GdaQueryTarget *target;
		GdaEntityField *rfield;

		target = GDA_QUERY_TARGET (gda_object_ref_get_ref_object (field->priv->target_ref));
		rfield = GDA_ENTITY_FIELD (gda_object_ref_get_ref_object (field->priv->field_ref));
		if (gda_query_target_get_represented_entity (target) != gda_entity_field_get_entity (rfield))
			gda_object_ref_deactivate (field->priv->field_ref);
		else
			active = TRUE;
	}

	if (active && field->priv->value_prov_ref)
			active = gda_object_ref_activate (field->priv->value_prov_ref);

	return active;
}

static void
gda_query_field_field_deactivate (GdaReferer *iface)
{
	GdaQueryFieldField *field;
	g_return_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface));
	g_return_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv);
	field = GDA_QUERY_FIELD_FIELD (iface);

	gda_object_ref_deactivate (field->priv->target_ref);
	gda_object_ref_deactivate (field->priv->field_ref);
	if (field->priv->value_prov_ref)
		gda_object_ref_deactivate (field->priv->value_prov_ref);
}

static gboolean
gda_query_field_field_is_active (GdaReferer *iface)
{
	gboolean active;
	GdaQueryFieldField *field;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv, FALSE);
	field = GDA_QUERY_FIELD_FIELD (iface);

	active =  gda_object_ref_is_active (field->priv->target_ref) &&
		gda_object_ref_is_active (field->priv->field_ref);

	if (active && field->priv->value_prov_ref)
		active = gda_object_ref_is_active (field->priv->value_prov_ref);

	return active;
}

static GSList *
gda_query_field_field_get_ref_objects (GdaReferer *iface)
{
	GSList *list = NULL;
        GdaObject *base;
	GdaQueryFieldField *field;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv, NULL);
	field = GDA_QUERY_FIELD_FIELD (iface);

        base = gda_object_ref_get_ref_object (field->priv->target_ref);
        if (base)
                list = g_slist_append (list, base);

        base = gda_object_ref_get_ref_object (field->priv->field_ref);
        if (base)
                list = g_slist_append (list, base);

	if (field->priv->value_prov_ref) {
		base = gda_object_ref_get_ref_object (field->priv->value_prov_ref);
		if (base)
			list = g_slist_append (list, base);
	}

        return list;
}

static void
gda_query_field_field_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	GdaQueryFieldField *field;

        g_return_if_fail (iface && GDA_IS_QUERY_FIELD_FIELD (iface));
        g_return_if_fail (GDA_QUERY_FIELD_FIELD (iface)->priv);

        field = GDA_QUERY_FIELD_FIELD (iface);
        if (field->priv->query) {
                GdaQuery *query = g_hash_table_lookup (replacements, field->priv->query);
                if (query) {
                        g_signal_handlers_disconnect_by_func (G_OBJECT (field->priv->query),
                                                              G_CALLBACK (destroyed_object_cb), field);
                        g_signal_handlers_disconnect_by_func (G_OBJECT (field->priv->query),
                                                              G_CALLBACK (target_removed_cb), field);
                        field->priv->query = query;
			gda_object_connect_destroy (query, 
						 G_CALLBACK (destroyed_object_cb), field);
                        g_signal_connect (G_OBJECT (query), "target_removed",
                                          G_CALLBACK (target_removed_cb), field);
                }
        }

        gda_object_ref_replace_ref_object (field->priv->target_ref, replacements);
        gda_object_ref_replace_ref_object (field->priv->field_ref, replacements);
	if (field->priv->value_prov_ref)
		gda_object_ref_replace_ref_object (field->priv->value_prov_ref, replacements);
}
