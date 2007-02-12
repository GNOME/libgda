/* gda-query-field-all.c
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

#include "gda-query-field-all.h"
#include "gda-xml-storage.h"
#include "gda-entity-field.h"
#include "gda-entity.h"
#include "gda-renderer.h"
#include "gda-referer.h"
#include "gda-object-ref.h"
#include "gda-query.h"
#include "gda-query-target.h"
#include <glib/gi18n-lib.h>
#include <string.h>

/* 
 * Main static functions 
 */
static void gda_query_field_all_class_init (GdaQueryFieldAllClass * class);
static void gda_query_field_all_init (GdaQueryFieldAll *qf);
static void gda_query_field_all_dispose (GObject *object);
static void gda_query_field_all_finalize (GObject *object);

static void gda_query_field_all_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_query_field_all_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);

/* XML storage interface */
static void        gda_query_field_all_xml_storage_init (GdaXmlStorageIface *iface);
static xmlNodePtr  gda_query_field_all_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_query_field_all_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* Field interface */
static void              gda_query_field_all_field_init      (GdaEntityFieldIface *iface);
static GdaEntity         *gda_query_field_all_get_entity      (GdaEntityField *iface);
static GdaDictType *gda_query_field_all_get_dict_type   (GdaEntityField *iface);

/* Renderer interface */
static void            gda_query_field_all_renderer_init      (GdaRendererIface *iface);
static gchar          *gda_query_field_all_render_as_sql   (GdaRenderer *iface, GdaParameterList *context, 
							    GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar          *gda_query_field_all_render_as_str   (GdaRenderer *iface, GdaParameterList *context);

/* Referer interface */
static void        gda_query_field_all_referer_init        (GdaRefererIface *iface);
static gboolean    gda_query_field_all_activate            (GdaReferer *iface);
static void        gda_query_field_all_deactivate          (GdaReferer *iface);
static gboolean    gda_query_field_all_is_active           (GdaReferer *iface);
static GSList     *gda_query_field_all_get_ref_objects     (GdaReferer *iface);
static void        gda_query_field_all_replace_refs        (GdaReferer *iface, GHashTable *replacements);

/* virtual functions */
static GObject    *gda_query_field_all_copy           (GdaQueryField *orig);
static gboolean    gda_query_field_all_is_equal       (GdaQueryField *qfield1, GdaQueryField *qfield2);


/* When the GdaQuery is destroyed */
static void destroyed_object_cb (GObject *obj, GdaQueryFieldAll *field);
static void target_removed_cb (GdaQuery *query, GdaQueryTarget *target, GdaQueryFieldAll *field);

#ifdef GDA_DEBUG
static void gda_query_field_all_dump (GdaQueryFieldAll *field, guint offset);
#endif

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP_QUERY,
	PROP_TARGET_OBJ,
	PROP_TARGET_NAME,
	PROP_TARGET_ID
};


/* private structure */
struct _GdaQueryFieldAllPrivate
{
	GdaQuery      *query;
	GdaObjectRef  *target_ref;
};


/* module error */
GQuark gda_query_field_all_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_query_field_all_error");
	return quark;
}


GType
gda_query_field_all_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryFieldAllClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_field_all_class_init,
			NULL,
			NULL,
			sizeof (GdaQueryFieldAll),
			0,
			(GInstanceInitFunc) gda_query_field_all_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_query_field_all_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo field_info = {
			(GInterfaceInitFunc) gda_query_field_all_field_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo renderer_info = {
			(GInterfaceInitFunc) gda_query_field_all_renderer_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo referer_info = {
			(GInterfaceInitFunc) gda_query_field_all_referer_init,
			NULL,
			NULL
		};
		
		
		type = g_type_register_static (GDA_TYPE_QUERY_FIELD, "GdaQueryFieldAll", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_ENTITY_FIELD, &field_info);
		g_type_add_interface_static (type, GDA_TYPE_RENDERER, &renderer_info);
		g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
	}
	return type;
}

static void 
gda_query_field_all_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = NULL;
	iface->save_to_xml = gda_query_field_all_save_to_xml;
	iface->load_from_xml = gda_query_field_all_load_from_xml;
}

static void
gda_query_field_all_field_init (GdaEntityFieldIface *iface)
{
	iface->get_entity = gda_query_field_all_get_entity;
	iface->get_dict_type = gda_query_field_all_get_dict_type;
}

static void
gda_query_field_all_renderer_init (GdaRendererIface *iface)
{
	iface->render_as_sql = gda_query_field_all_render_as_sql;
	iface->render_as_str = gda_query_field_all_render_as_str;
	iface->is_valid = NULL;
}

static void
gda_query_field_all_referer_init (GdaRefererIface *iface)
{
        iface->activate = gda_query_field_all_activate;
        iface->deactivate = gda_query_field_all_deactivate;
        iface->is_active = gda_query_field_all_is_active;
        iface->get_ref_objects = gda_query_field_all_get_ref_objects;
        iface->replace_refs = gda_query_field_all_replace_refs;
}

static void
gda_query_field_all_class_init (GdaQueryFieldAllClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_query_field_all_dispose;
	object_class->finalize = gda_query_field_all_finalize;

	/* Properties */
	object_class->set_property = gda_query_field_all_set_property;
	object_class->get_property = gda_query_field_all_get_property;
	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_object ("query", "Query to which the field belongs", NULL, 
                                                               GDA_TYPE_QUERY,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE | 
								G_PARAM_CONSTRUCT_ONLY)));

	g_object_class_install_property (object_class, PROP_TARGET_OBJ,
					 g_param_spec_object ("target", "A pointer to a GdaQueryTarget", NULL,
                                                               GDA_TYPE_QUERY_TARGET, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_TARGET_NAME,
					 g_param_spec_string ("target_name", "Name or alias of a query target", NULL, NULL,
							      G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_TARGET_ID,
					 g_param_spec_string ("target_id", "XML ID of a query target", NULL, NULL,
							      G_PARAM_WRITABLE));

	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_query_field_all_dump;
#endif
	GDA_QUERY_FIELD_CLASS (class)->copy = gda_query_field_all_copy;
	GDA_QUERY_FIELD_CLASS (class)->is_equal = gda_query_field_all_is_equal;
	GDA_QUERY_FIELD_CLASS (class)->is_list = NULL;
	GDA_QUERY_FIELD_CLASS (class)->get_params = NULL;
}

static void
gda_query_field_all_init (GdaQueryFieldAll *gda_query_field_all)
{
	gda_query_field_all->priv = g_new0 (GdaQueryFieldAllPrivate, 1);
	gda_query_field_all->priv->query = NULL;
	gda_query_field_all->priv->target_ref = NULL;
}

/**
 * gda_query_field_all_new
 * @query: a #GdaQuery in which the new object will be
 * @target: the target's name or alias
 *
 * Creates a new #GdaQueryFieldAll object which represents all the fields of the entity represented
 * by a target.
 *
 * Returns: the new object
 */
GdaQueryField *
gda_query_field_all_new (GdaQuery *query, const gchar *target)
{
	GObject *obj;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (target && *target, NULL);

	obj = g_object_new (GDA_TYPE_QUERY_FIELD_ALL, "dict", gda_object_get_dict (GDA_OBJECT (query)), 
			    "query", query, 
			    "target_name", target, NULL);

	return (GdaQueryField *) obj;	
}

static void 
destroyed_object_cb (GObject *obj, GdaQueryFieldAll *field)
{
	gda_object_destroy (GDA_OBJECT (field));
}

static void
target_removed_cb (GdaQuery *query, GdaQueryTarget *target, GdaQueryFieldAll *field)
{
	GdaObject *base;

	base = gda_object_ref_get_ref_object (field->priv->target_ref);
	if (base && (GDA_QUERY_TARGET (base) == target)) 
		gda_object_destroy (GDA_OBJECT (field));	
}

static void
gda_query_field_all_dispose (GObject *object)
{
	GdaQueryFieldAll *fall;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD_ALL (object));

	fall = GDA_QUERY_FIELD_ALL (object);
	if (fall->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		if (fall->priv->query) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (fall->priv->query),
							      G_CALLBACK (destroyed_object_cb), fall);
			g_signal_handlers_disconnect_by_func (G_OBJECT (fall->priv->query),
							      G_CALLBACK (target_removed_cb), fall);
			fall->priv->query = NULL;
		}
		if (fall->priv->target_ref) {
			g_object_unref (G_OBJECT (fall->priv->target_ref));
			fall->priv->target_ref = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_query_field_all_finalize (GObject   * object)
{
	GdaQueryFieldAll *fall;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD_ALL (object));

	fall = GDA_QUERY_FIELD_ALL (object);
	if (fall->priv) {
		g_free (fall->priv);
		fall->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_query_field_all_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	GdaQueryFieldAll *fall;
	const gchar *str;
	guint id;

	fall = GDA_QUERY_FIELD_ALL (object);
	if (fall->priv) {
		switch (param_id) {
		case PROP_QUERY: {
			GdaQuery* ptr = GDA_QUERY (g_value_get_object (value));
			g_return_if_fail (GDA_IS_QUERY (ptr));
			g_return_if_fail (gda_object_get_dict (GDA_OBJECT (ptr)) == gda_object_get_dict (GDA_OBJECT (fall)));

			if (fall->priv->query) {
				if (fall->priv->query == GDA_QUERY (ptr))
					return;

				g_signal_handlers_disconnect_by_func (G_OBJECT (fall->priv->query),
								      G_CALLBACK (destroyed_object_cb), fall);

				g_signal_handlers_disconnect_by_func (G_OBJECT (fall->priv->query),
								      G_CALLBACK (target_removed_cb), fall);
			}

			fall->priv->query = GDA_QUERY (ptr);
			gda_object_connect_destroy (ptr, 
						    G_CALLBACK (destroyed_object_cb), fall);
			g_signal_connect (G_OBJECT (ptr), "target_removed",
					  G_CALLBACK (target_removed_cb), fall);

			fall->priv->target_ref = GDA_OBJECT_REF (gda_object_ref_new (gda_object_get_dict (GDA_OBJECT (ptr))));
			g_object_set (G_OBJECT (fall->priv->target_ref), "helper_ref", ptr, NULL);

			g_object_get (G_OBJECT (ptr), "field_serial", &id, NULL);
			gda_query_object_set_int_id (GDA_QUERY_OBJECT (fall), id);
			break;
                }
		case PROP_TARGET_OBJ: {
			GdaQueryTarget* ptr = GDA_QUERY_TARGET (g_value_get_object (value));
			g_return_if_fail (GDA_IS_QUERY_TARGET (ptr));
			gda_object_ref_set_ref_object (fall->priv->target_ref, GDA_OBJECT (ptr));
			break;
                }
		case PROP_TARGET_NAME:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (fall->priv->target_ref, GDA_TYPE_QUERY_TARGET, 
						     REFERENCE_BY_NAME, str);
			break;
		case PROP_TARGET_ID:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (fall->priv->target_ref, GDA_TYPE_QUERY_TARGET, 
						     REFERENCE_BY_XML_ID, str);
			break;
		}
	}
}

static void
gda_query_field_all_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	GdaQueryFieldAll *fall;
	fall = GDA_QUERY_FIELD_ALL (object);
	
	if (fall->priv) {
		switch (param_id) {
		case PROP_QUERY:
			g_value_set_object (value, G_OBJECT (fall->priv->query));
			break;
		case PROP_TARGET_OBJ:
			g_value_set_object (value, G_OBJECT (gda_object_ref_get_ref_object (fall->priv->target_ref)));
			break;
		case PROP_TARGET_NAME:
		case PROP_TARGET_ID:
			/* NO READ */
			g_assert_not_reached ();
			break;
		}	
	}
}

static GObject *
gda_query_field_all_copy (GdaQueryField *orig)
{
	GdaQueryFieldAll *qf;
	GObject *obj;
	GdaObject *ref;

	g_assert (GDA_IS_QUERY_FIELD_ALL (orig));
	qf = GDA_QUERY_FIELD_ALL (orig);

	obj = g_object_new (GDA_TYPE_QUERY_FIELD_ALL, 
			    "dict", gda_object_get_dict (GDA_OBJECT (qf)), 
			    "query", qf->priv->query, NULL);

	ref = gda_object_ref_get_ref_object (qf->priv->target_ref);
	if (ref)
		gda_object_ref_set_ref_object (GDA_QUERY_FIELD_ALL (obj)->priv->target_ref, ref);
	else {
		const gchar *ref_str;
		GdaObjectRefType ref_type;
		GType ref_gtype;

		ref_str = gda_object_ref_get_ref_name (qf->priv->target_ref, &ref_gtype, &ref_type);
		if (ref_str)
			gda_object_ref_set_ref_name (GDA_QUERY_FIELD_ALL (obj)->priv->target_ref, ref_gtype, ref_type, ref_str);
	}

	if (gda_object_get_name (GDA_OBJECT (orig)))
		gda_object_set_name (GDA_OBJECT (obj), gda_object_get_name (GDA_OBJECT (orig)));

	if (gda_object_get_description (GDA_OBJECT (orig)))
		gda_object_set_description (GDA_OBJECT (obj), gda_object_get_description (GDA_OBJECT (orig)));

	return obj;
}

static gboolean
gda_query_field_all_is_equal (GdaQueryField *qfield1, GdaQueryField *qfield2)
{
	const gchar *ref1, *ref2;
	g_assert (GDA_IS_QUERY_FIELD_ALL (qfield1));
	g_assert (GDA_IS_QUERY_FIELD_ALL (qfield2));
	
	/* it is here assumed that qfield1 and qfield2 are of the same type and refer to the same
	   query */
	ref1 = gda_object_ref_get_ref_name (GDA_QUERY_FIELD_ALL (qfield1)->priv->target_ref, NULL, NULL);
	ref2 = gda_object_ref_get_ref_name (GDA_QUERY_FIELD_ALL (qfield2)->priv->target_ref, NULL, NULL);

	return !strcmp (ref1, ref2) ? TRUE : FALSE;
}

/**
 * gda_query_field_all_get_target
 * @field: a #GdaQueryFieldAll object
 *
 * Get the #GdaQueryTarget object @field 'belongs' to
 *
 * Returns: the #GdaQueryTarget object
 */
GdaQueryTarget *
gda_query_field_all_get_target (GdaQueryFieldAll *field)
{
	GdaObject *base;
	g_return_val_if_fail (field && GDA_IS_QUERY_FIELD_ALL (field), NULL);
	g_return_val_if_fail (field->priv, NULL);

	base = gda_object_ref_get_ref_object (field->priv->target_ref);
	if (base)
		return GDA_QUERY_TARGET (base);
	else
		return NULL;
}

#ifdef GDA_DEBUG
static void
gda_query_field_all_dump (GdaQueryFieldAll *field, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (field && GDA_IS_QUERY_FIELD_ALL (field));
	
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (field->priv) {
                g_print ("%s" D_COL_H1 "GdaQueryFieldAll" D_COL_NOR " \"%s\" (%p, id=%s) ",
                         str, gda_object_get_name (GDA_OBJECT (field)), field, 
			 gda_object_get_id (GDA_OBJECT (field)));
		if (gda_query_field_all_is_active (GDA_REFERER (field)))
			g_print ("Active, ");
		else
			g_print (D_COL_ERR "Inactive" D_COL_NOR ", ");
		g_print ("references %p (%s)\n", 
			 gda_object_ref_get_ref_object (field->priv->target_ref),
			 gda_object_ref_get_ref_name (field->priv->target_ref, NULL, NULL));
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, field);
}
#endif


/* 
 * GdaEntityField interface implementation
 */
static GdaEntity *
gda_query_field_all_get_entity (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv, NULL);

	return GDA_ENTITY (GDA_QUERY_FIELD_ALL (iface)->priv->query);
}

static GdaDictType *
gda_query_field_all_get_dict_type (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv, NULL);

	return NULL;
}

/* 
 * GdaXmlStorage interface implementation
 */
static xmlNodePtr
gda_query_field_all_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	GdaQueryFieldAll *field;
	gchar *str;
	GdaObject *obj;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv, NULL);

	field = GDA_QUERY_FIELD_ALL (iface);

	node = xmlNewNode (NULL, (xmlChar*)"gda_query_fall");
	
	str = gda_xml_storage_get_xml_id (iface);
	xmlSetProp(node, (xmlChar*)"id", (xmlChar*)str);
	g_free (str);

	xmlSetProp(node, (xmlChar*)"name", (xmlChar*)gda_object_get_name (GDA_OBJECT (field)));

	obj = NULL;
	if (gda_object_ref_activate (field->priv->target_ref))
		obj = gda_object_ref_get_ref_object (field->priv->target_ref);

	if (obj) {
		gchar *xmlid;
		xmlid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (obj));
		xmlSetProp(node, (xmlChar*)"target", (xmlChar*)xmlid);
	}
	else {
		const gchar *cstr;

		cstr = gda_object_ref_get_ref_name (field->priv->target_ref, NULL, NULL);
		if (cstr)
			xmlSetProp(node, (xmlChar*)"target_name", (xmlChar*)cstr);
	}

	if (! gda_query_field_is_visible (GDA_QUERY_FIELD (field)))
		xmlSetProp(node, (xmlChar*)"is_visible",  (xmlChar*)"f");
	if (gda_query_field_is_internal (GDA_QUERY_FIELD (field)))
		xmlSetProp(node, (xmlChar*)"is_internal", (xmlChar*)"t");

	return node;
}

static gboolean
gda_query_field_all_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaQueryFieldAll *field;
	gchar *prop;
	gboolean target = FALSE;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	field = GDA_QUERY_FIELD_ALL (iface);
	if (strcmp ((gchar*)node->name, "gda_query_fall")) {
		g_set_error (error,
			     GDA_QUERY_FIELD_ALL_ERROR,
			     GDA_QUERY_FIELD_ALL_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_query_fall>"));
		return FALSE;
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"id");
	if (prop) {
		gchar *ptr, *tok;
		ptr = strtok_r (prop, ":", &tok);
		ptr = strtok_r (NULL, ":", &tok);
		if (strlen (ptr) < 3) {
			g_set_error (error,
				     GDA_QUERY_FIELD_ALL_ERROR,
				     GDA_QUERY_FIELD_ALL_XML_LOAD_ERROR,
				     _("XML ID for a query field should be QUxxx:QFyyy where xxx and yyy are numbers"));
			return FALSE;
		}
		gda_query_object_set_int_id (GDA_QUERY_OBJECT (field), atoi (ptr+2));
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"name");
	if (prop) {
		gda_object_set_name (GDA_OBJECT (field), prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"target");
	if (prop) {
		target = TRUE;
		gda_object_ref_set_ref_name (field->priv->target_ref, GDA_TYPE_QUERY_TARGET, REFERENCE_BY_XML_ID, prop);
		g_free (prop);
	}
	else {
		prop = (gchar*)xmlGetProp(node, (xmlChar*)"target_name");
		if (prop) {
			target = TRUE;
			gda_object_ref_set_ref_name (field->priv->target_ref, GDA_TYPE_QUERY_TARGET, REFERENCE_BY_NAME, prop);
			g_free (prop);
		}
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"is_visible");
	if (prop) {
		gda_query_field_set_visible (GDA_QUERY_FIELD (field), (*prop == 't') ? TRUE : FALSE);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"is_internal");
	if (prop) {
		gda_query_field_set_internal (GDA_QUERY_FIELD (field), (*prop == 't') ? TRUE : FALSE);
		g_free (prop);
	}

	if (target)
		return TRUE;
	else {
		g_set_error (error,
			     GDA_QUERY_FIELD_ALL_ERROR,
			     GDA_QUERY_FIELD_ALL_XML_LOAD_ERROR,
			     _("Missing required attributes for <gda_query_fall>"));

		return FALSE;
	}
}


/*
 * GdaRenderer interface implementation
 */

static gchar *
gda_query_field_all_render_as_sql (GdaRenderer *iface, GdaParameterList *context, 
				   GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	gchar *str = NULL;
	GdaObject *base;
	GdaQueryFieldAll *field;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv, NULL);
	field = GDA_QUERY_FIELD_ALL (iface);

	base = gda_object_ref_get_ref_object (field->priv->target_ref);
	if (base) 
		str = g_strdup_printf ("%s.*", gda_query_target_get_alias (GDA_QUERY_TARGET (base)));
	else
		g_set_error (error,
			     GDA_QUERY_FIELD_ALL_ERROR,
			     GDA_QUERY_FIELD_ALL_RENDER_ERROR,
			     _("Can't find target '%s'"), gda_object_ref_get_ref_name (field->priv->target_ref,
										    NULL, NULL));
	
	return str;
}

static gchar *
gda_query_field_all_render_as_str (GdaRenderer *iface, GdaParameterList *context)
{
	gchar *str = NULL;
	GdaObject *base;
	GdaQueryFieldAll *field;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv, NULL);
	field = GDA_QUERY_FIELD_ALL (iface);

	base = gda_object_ref_get_ref_object (field->priv->target_ref);
	if (base) {
		GdaEntity *ent = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (base));
		str = g_strdup_printf ("%s(%s).*", gda_object_get_name (GDA_OBJECT (ent)), 
				       gda_query_target_get_alias (GDA_QUERY_TARGET (base)));
	}
	else
		str = g_strdup (_("Non-activated field"));
	return str;
}


/*
 * GdaReferer interface implementation
 */
static gboolean
gda_query_field_all_activate (GdaReferer *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv, FALSE);

	return gda_object_ref_activate (GDA_QUERY_FIELD_ALL (iface)->priv->target_ref);
}

static void
gda_query_field_all_deactivate (GdaReferer *iface)
{
	g_return_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface));
	g_return_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv);

	gda_object_ref_deactivate (GDA_QUERY_FIELD_ALL (iface)->priv->target_ref);
}

static gboolean
gda_query_field_all_is_active (GdaReferer *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv, FALSE);

	return gda_object_ref_is_active (GDA_QUERY_FIELD_ALL (iface)->priv->target_ref);
}

static GSList *
gda_query_field_all_get_ref_objects (GdaReferer *iface)
{
	GSList *list = NULL;
        GdaObject *base;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv, NULL);

        base = gda_object_ref_get_ref_object (GDA_QUERY_FIELD_ALL (iface)->priv->target_ref);
        if (base)
                list = g_slist_append (list, base);

        return list;
}

static void
gda_query_field_all_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	GdaQueryFieldAll *field;

        g_return_if_fail (iface && GDA_IS_QUERY_FIELD_ALL (iface));
        g_return_if_fail (GDA_QUERY_FIELD_ALL (iface)->priv);

        field = GDA_QUERY_FIELD_ALL (iface);
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
}
