/* gda-query-target.c
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
#include "gda-query-target.h"
#include "gda-entity.h"
#include "gda-object-ref.h"
#include "gda-xml-storage.h"
#include "gda-referer.h"
#include "gda-renderer.h"
#include "gda-query.h"
#include "gda-dict-table.h"
#include "gda-connection.h"
#include "gda-server-provider.h"

/* 
 * Main static functions 
 */
static void gda_query_target_class_init (GdaQueryTargetClass * class);
static void gda_query_target_init (GdaQueryTarget * srv);
static void gda_query_target_dispose (GObject   * object);
static void gda_query_target_finalize (GObject   * object);

static void gda_query_target_set_property (GObject *object,
					   guint param_id,
					   const GValue *value,
					   GParamSpec *pspec);
static void gda_query_target_get_property (GObject *object,
					   guint param_id,
					   GValue *value,
					   GParamSpec *pspec);

/* XML storage interface */
static void        gda_query_target_xml_storage_init (GdaXmlStorageIface *iface);
static xmlNodePtr  gda_query_target_save_to_xml      (GdaXmlStorage *iface, GError **error);
static gboolean    gda_query_target_load_from_xml    (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* Referer interface */
static void        gda_query_target_referer_init        (GdaRefererIface *iface);
static gboolean    gda_query_target_activate            (GdaReferer *iface);
static void        gda_query_target_deactivate          (GdaReferer *iface);
static gboolean    gda_query_target_is_active           (GdaReferer *iface);
static GSList     *gda_query_target_get_ref_objects     (GdaReferer *iface);
static void        gda_query_target_replace_refs        (GdaReferer *iface, GHashTable *replacements);

/* Renderer interface */
static void        gda_query_target_renderer_init   (GdaRendererIface *iface);
static gchar      *gda_query_target_render_as_sql   (GdaRenderer *iface, GdaParameterList *context, guint options, GError **error);
static gchar      *gda_query_target_render_as_str   (GdaRenderer *iface, GdaParameterList *context);

/* Alias interface */
/* static void        gda_query_target_alias_init        (GnomeDbAlias *iface); */


/* When the GdaQuery is destroyed */
static void        destroyed_object_cb (GObject *obj, GdaQueryTarget *target);
static void        gda_query_target_set_int_id (GdaQueryObject *target, guint id);

#ifdef GDA_DEBUG
static void        gda_query_target_dump (GdaQueryTarget *table, guint offset);
#endif

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;


/* properties */
enum
{
	PROP_0,
	PROP_QUERY,
	PROP_ENTITY_OBJ,
	PROP_ENTITY_NAME,
	PROP_ENTITY_ID
};


/* private structure */
struct _GdaQueryTargetPrivate
{
	GdaQuery      *query;
	GdaObjectRef  *entity_ref;
	gchar         *alias;
};


/* module error */
GQuark gda_query_target_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_query_target_error");
	return quark;
}


GType
gda_query_target_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryTargetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_target_class_init,
			NULL,
			NULL,
			sizeof (GdaQueryTarget),
			0,
			(GInstanceInitFunc) gda_query_target_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_query_target_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo referer_info = {
			(GInterfaceInitFunc) gda_query_target_referer_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo renderer_info = {
			(GInterfaceInitFunc) gda_query_target_renderer_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GDA_TYPE_QUERY_OBJECT, "GdaQueryTarget", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
		g_type_add_interface_static (type, GDA_TYPE_RENDERER, &renderer_info);
	}
	return type;
}

static void 
gda_query_target_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = NULL;
	iface->save_to_xml = gda_query_target_save_to_xml;
	iface->load_from_xml = gda_query_target_load_from_xml;
}

static void
gda_query_target_referer_init (GdaRefererIface *iface)
{
	iface->activate = gda_query_target_activate;
	iface->deactivate = gda_query_target_deactivate;
	iface->is_active = gda_query_target_is_active;
	iface->get_ref_objects = gda_query_target_get_ref_objects;
	iface->replace_refs = gda_query_target_replace_refs;
}

static void
gda_query_target_renderer_init (GdaRendererIface *iface)
{
	iface->render_as_sql = gda_query_target_render_as_sql;
	iface->render_as_str = gda_query_target_render_as_str;
	iface->is_valid = NULL;
}

static void
gda_query_target_class_init (GdaQueryTargetClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_query_target_dispose;
	object_class->finalize = gda_query_target_finalize;

	/* Properties */
	object_class->set_property = gda_query_target_set_property;
	object_class->get_property = gda_query_target_get_property;
	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_pointer ("query", "Query to which the target belongs", NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE |
								G_PARAM_CONSTRUCT_ONLY)));

	g_object_class_install_property (object_class, PROP_ENTITY_OBJ,
					 g_param_spec_pointer ("entity", "A pointer to a GdaEntity", NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_ENTITY_NAME,
					 g_param_spec_string ("entity_name", "Name an entity", NULL, NULL,
							      G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_ENTITY_ID,
					 g_param_spec_string ("entity_id", "XML ID of an entity", NULL, NULL,
							      G_PARAM_WRITABLE));
	/* virtual functions */
	GDA_QUERY_OBJECT_CLASS (class)->set_int_id = (void (*)(GdaQueryObject *, guint)) gda_query_target_set_int_id;
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_query_target_dump;
#endif

}

static void
gda_query_target_init (GdaQueryTarget *target)
{
	target->priv = g_new0 (GdaQueryTargetPrivate, 1);
	target->priv->query = NULL;
	target->priv->entity_ref = NULL;
	target->priv->alias = NULL;
}

/**
 * gda_query_target_new
 * @query: a #GdaQuery object
 * @table: the name of the table to reference
 * @as: optional alias
 *
 * Creates a new #GdaQueryTarget object, specifying the name of the table to reference.
 *
 * Returns: the new object
 */
GdaQueryTarget *
gda_query_target_new (GdaQuery *query, const gchar *table)
{
	GObject *obj;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (table && *table, NULL);

	obj = g_object_new (GDA_TYPE_QUERY_TARGET, 
			    "dict", gda_object_get_dict (GDA_OBJECT (query)), 
			    "query", query, "entity_name", table, NULL);
	return (GdaQueryTarget*) obj;
}


/**
 * gda_query_target_new_copy
 * @orig: a #GdaQueryTarget object to copy
 *
 * Makes a copy of an existing object (copy constructor)
 *
 * Returns: the new object
 */
GdaQueryTarget *
gda_query_target_new_copy (GdaQueryTarget *orig)
{
	GObject *obj;
	GdaQueryTarget *target;
	GdaDict *dict;
	GdaObject *ref;

	g_return_val_if_fail (GDA_IS_QUERY_TARGET (orig), NULL);

	dict = gda_object_get_dict (GDA_OBJECT (orig));
	obj = g_object_new (GDA_TYPE_QUERY_TARGET, "dict", dict, "query", orig->priv->query, NULL);
	target = GDA_QUERY_TARGET (obj);

	ref = gda_object_ref_get_ref_object (orig->priv->entity_ref);
	if (ref)
		gda_object_ref_set_ref_object (target->priv->entity_ref, ref);
	else {
		const gchar *ref_str;
		GdaObjectRefType ref_type;
		GType ref_gtype;

		ref_str = gda_object_ref_get_ref_object_name (orig->priv->entity_ref);
		if (ref_str)
			g_object_set (G_OBJECT (target->priv->entity_ref), "obj_name", ref_str, NULL);

		ref_str = gda_object_ref_get_ref_name (orig->priv->entity_ref, &ref_gtype, &ref_type);
		if (ref_str)
			gda_object_ref_set_ref_name (target->priv->entity_ref, ref_gtype, ref_type, ref_str);
	}

	return target;	
}


static void
destroyed_object_cb (GObject *obj, GdaQueryTarget *target)
{
	gda_object_destroy (GDA_OBJECT (target));
}


static void
gda_query_target_dispose (GObject *object)
{
	GdaQueryTarget *target;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_TARGET (object));

	target = GDA_QUERY_TARGET (object);
	if (target->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));
		
		if (target->priv->query) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (target->priv->query),
							      G_CALLBACK (destroyed_object_cb), target);
			target->priv->query = NULL;
		}
		if (target->priv->entity_ref) {
			g_object_unref (G_OBJECT (target->priv->entity_ref));
			target->priv->entity_ref = NULL;
		}

		if (target->priv->alias) {
			g_free (target->priv->alias);
			target->priv->alias = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_query_target_finalize (GObject   * object)
{
	GdaQueryTarget *target;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_TARGET (object));

	target = GDA_QUERY_TARGET (object);
	if (target->priv) {
		g_free (target->priv);
		target->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_query_target_set_property (GObject *object,
			       guint param_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	gpointer ptr;
	GdaQueryTarget *target;
	guint id;
	const gchar *str;
	GType target_ref;

	target = GDA_QUERY_TARGET (object);
	if (target->priv) {
		switch (param_id) {
		case PROP_QUERY:
			ptr = g_value_get_pointer (value);
			g_return_if_fail (GDA_IS_QUERY (ptr));
			g_return_if_fail (gda_object_get_dict (GDA_OBJECT (ptr)) == gda_object_get_dict (GDA_OBJECT (target)));
			
			if (target->priv->query) {
				if (target->priv->query == GDA_QUERY (ptr))
					return;
				
				g_signal_handlers_disconnect_by_func (G_OBJECT (target->priv->query),
								      G_CALLBACK (destroyed_object_cb), target);
			}
			
			target->priv->query = GDA_QUERY (ptr);
			gda_object_connect_destroy (ptr, 
						    G_CALLBACK (destroyed_object_cb), target);
			
			target->priv->entity_ref = GDA_OBJECT_REF (gda_object_ref_new (gda_object_get_dict (GDA_OBJECT (ptr))));
			
			g_object_get (G_OBJECT (ptr), "target_serial", &id, NULL);
			gda_query_object_set_int_id (GDA_QUERY_OBJECT (target), id);
			break;
		case PROP_ENTITY_OBJ:
			ptr = g_value_get_pointer (value);
			g_return_if_fail (GDA_IS_ENTITY (ptr));
			gda_object_ref_set_ref_object (target->priv->entity_ref, GDA_OBJECT (ptr));
			break;
		case PROP_ENTITY_NAME:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (target->priv->entity_ref, GDA_TYPE_DICT_TABLE, 
						     REFERENCE_BY_NAME, str);
			break;
		case PROP_ENTITY_ID:
			str = g_value_get_string (value);
			if (!str || *str == 'T') 
				target_ref = GDA_TYPE_DICT_TABLE;
			else
				target_ref = GDA_TYPE_QUERY;
			gda_object_ref_set_ref_name (target->priv->entity_ref, target_ref, 
						     REFERENCE_BY_XML_ID, str);
			break;
		}
	}
}

static void
gda_query_target_get_property (GObject *object,
			       guint param_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	GdaQueryTarget *target;

	target = GDA_QUERY_TARGET (object);
	
	if (target->priv) {
		switch (param_id) {
		case PROP_QUERY:
			g_value_set_pointer (value, target->priv->query);
			break;
		case PROP_ENTITY_OBJ:
			g_value_set_pointer (value, gda_object_ref_get_ref_object (target->priv->entity_ref));
			break;
		case PROP_ENTITY_NAME:
		case PROP_ENTITY_ID:
			/* NO READ */
			g_assert_not_reached ();
			break;
		}	
	}
}


/**
 * gda_query_target_get_query
 * @target: a #GdaQueryTarget object
 *
 * Get the #GdaQuery in which @target is
 *
 * Returns: the #GdaQuery object
 */
GdaQuery *
gda_query_target_get_query (GdaQueryTarget *target)
{
	g_return_val_if_fail (GDA_IS_QUERY_TARGET (target), NULL);
	g_return_val_if_fail (target->priv, NULL);

	return target->priv->query;
}

/**
 * gda_query_target_get_represented_entity
 * @target: a #GdaQueryTarget object
 *
 * Get the #GdaEntity object which is represented by @target
 *
 * Returns: the #GdaEntity object or NULL if @target is not active
 */
GdaEntity *
gda_query_target_get_represented_entity (GdaQueryTarget *target)
{
	GdaObject *ent;
	GdaEntity *entity = NULL;

	g_return_val_if_fail (GDA_IS_QUERY_TARGET (target), NULL);
	g_return_val_if_fail (target->priv, NULL);

	ent = gda_object_ref_get_ref_object (target->priv->entity_ref);
	if (ent)
		entity = GDA_ENTITY (ent);

	return entity;
}

/**
 * gda_query_target_get_represented_table_name
 * @target: a #GdaQueryTarget object
 *
 * Get the table name represented by @target
 *
 * Returns: the table name or NULL if @target does not represent a database table
 */
const gchar *
gda_query_target_get_represented_table_name (GdaQueryTarget *target)
{
	GdaObject *ent = NULL;

	g_return_val_if_fail (GDA_IS_QUERY_TARGET (target), NULL);
	g_return_val_if_fail (target->priv, NULL);

	ent = gda_object_ref_get_ref_object (target->priv->entity_ref);
	if (ent) {
		if (GDA_IS_DICT_TABLE (ent))
			return gda_object_get_name (GDA_OBJECT (ent));
		else
			return NULL;
	}
	else {
		if (gda_object_ref_get_ref_object_name (target->priv->entity_ref))
			return gda_object_ref_get_ref_object_name (target->priv->entity_ref);
		else
			return gda_object_ref_get_ref_name (target->priv->entity_ref, NULL, NULL);
	}
}

/**
 * gda_query_target_set_alias
 * @target: a #GdaQueryTarget object
 * @alias: the alias
 *
 * Sets @target's alias to @alias
 */
void
gda_query_target_set_alias (GdaQueryTarget *target, const gchar *alias)
{
	g_return_if_fail (GDA_IS_QUERY_TARGET (target));
	g_return_if_fail (target->priv);

	if (target->priv->alias) {
		g_free (target->priv->alias);
		target->priv->alias = NULL;
	}
	
	if (alias)
		target->priv->alias = g_strdup (alias);
}

/**
 * gda_query_target_get_alias
 * @target: a #GdaQueryTarget object
 *
 * Get @target's alias
 *
 * Returns: the alias
 */
const gchar *
gda_query_target_get_alias (GdaQueryTarget *target)
{
	g_return_val_if_fail (GDA_IS_QUERY_TARGET (target), NULL);
	g_return_val_if_fail (target->priv, NULL);

	
	if (!target->priv->alias)
		target->priv->alias = g_strdup_printf ("t%u", gda_query_object_get_int_id (GDA_QUERY_OBJECT (target)));

	return target->priv->alias;
}

/**
 * gda_query_target_get_complete_name
 * @target: a #GdaQueryTarget object
 *
 * Get a complete name for target in the form of "&lt;entity name&gt; AS &lt;target alias&gt;"
 *
 * Returns: a new string
 */
gchar *
gda_query_target_get_complete_name (GdaQueryTarget *target)
{
	const gchar *cstr, *cstr2;
	GdaEntity *ent;
	gchar *tmpstr = NULL;

	g_return_val_if_fail (GDA_IS_QUERY_TARGET (target), NULL);
	g_return_val_if_fail (target->priv, NULL);

	ent = gda_query_target_get_represented_entity (target);
	if (!GDA_IS_QUERY (ent)) {
		cstr = gda_object_get_name (GDA_OBJECT (target));
		if (!cstr || !(*cstr))
			cstr = gda_object_get_name (GDA_OBJECT (ent));
		if (cstr && *cstr)
			tmpstr = g_strdup (cstr);
		
		cstr2 = gda_query_target_get_alias (target);
		if (cstr2 && *cstr2) {
			if (tmpstr) {
				gchar *str2 = g_strdup_printf ("%s AS %s", tmpstr, cstr2);
			g_free (tmpstr);
			tmpstr = str2;
			}
			else 
				tmpstr = g_strdup (cstr2);
		}
		
		if (!tmpstr)
			tmpstr = g_strdup (_("No name"));
	}
	else {
		/* ent is a query => print only the alias */
		cstr2 = gda_query_target_get_alias (target);
		if (cstr2 && *cstr2) 
			tmpstr = g_strdup (cstr2);
		else
			tmpstr = g_strdup (_("No name"));
	}

	return tmpstr;
}

#ifdef GDA_DEBUG
static void
gda_query_target_dump (GdaQueryTarget *target, guint offset)
{
	gchar *str;
        guint i;
	
	g_return_if_fail (GDA_IS_QUERY_TARGET (target));

        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (target->priv) {
                g_print ("%s" D_COL_H1 "GdaQueryTarget" D_COL_NOR " %p (id=%s) ",
                         str, target, gda_object_get_id (GDA_OBJECT (target)));
		if (gda_query_target_is_active (GDA_REFERER (target)))
			g_print ("Active, references %p ", gda_object_ref_get_ref_object (target->priv->entity_ref));
		else
			g_print (D_COL_ERR "Non active" D_COL_NOR ", ");
		g_print ("requested name: %s\n", gda_object_ref_get_ref_name (target->priv->entity_ref, NULL, NULL));
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, target);
}
#endif

static void
gda_query_target_set_int_id (GdaQueryObject *target, guint id)
{
	gchar *str;

	str = g_strdup_printf ("%s:T%u", gda_object_get_id (GDA_OBJECT (GDA_QUERY_TARGET (target)->priv->query)), id);
	gda_object_set_id (GDA_OBJECT (target), str);
	g_free (str);
}


/* 
 * GdaReferer interface implementation
 */
static gboolean
gda_query_target_activate (GdaReferer *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_TARGET (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_TARGET (iface)->priv, FALSE);

	return gda_object_ref_activate (GDA_QUERY_TARGET (iface)->priv->entity_ref);
}

static void
gda_query_target_deactivate (GdaReferer *iface)
{
	g_return_if_fail (iface && GDA_IS_QUERY_TARGET (iface));
	g_return_if_fail (GDA_QUERY_TARGET (iface)->priv);

	gda_object_ref_deactivate (GDA_QUERY_TARGET (iface)->priv->entity_ref);
}

static gboolean
gda_query_target_is_active (GdaReferer *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_TARGET (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_TARGET (iface)->priv, FALSE);

	return gda_object_ref_is_active (GDA_QUERY_TARGET (iface)->priv->entity_ref);
}

static GSList *
gda_query_target_get_ref_objects (GdaReferer *iface)
{
	GSList *list = NULL;
	GdaObject *base;
	g_return_val_if_fail (iface && GDA_IS_QUERY_TARGET (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_TARGET (iface)->priv, NULL);

	base = gda_object_ref_get_ref_object (GDA_QUERY_TARGET (iface)->priv->entity_ref);
	if (base)
		list = g_slist_append (list, base);

	return list;
}

static void
gda_query_target_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	GdaQueryTarget *target;

	g_return_if_fail (iface && GDA_IS_QUERY_TARGET (iface));
	g_return_if_fail (GDA_QUERY_TARGET (iface)->priv);

	target = GDA_QUERY_TARGET (iface);
	if (target->priv->query) {
		GdaQuery *query = g_hash_table_lookup (replacements, target->priv->query);
		if (query) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (target->priv->query),
							      G_CALLBACK (destroyed_object_cb), target);
			target->priv->query = query;
			gda_object_connect_destroy (query,
						 G_CALLBACK (destroyed_object_cb), target);
		}
	}

	gda_object_ref_replace_ref_object (target->priv->entity_ref, replacements);
}



/* 
 * GdaXmlStorage interface implementation
 */
static xmlNodePtr
gda_query_target_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	GdaQueryTarget *target;
	gchar *str;

	g_return_val_if_fail (iface && GDA_IS_QUERY_TARGET (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_TARGET (iface)->priv, NULL);

	target = GDA_QUERY_TARGET (iface);

	node = xmlNewNode (NULL, "gda_query_target");
	
	str = gda_xml_storage_get_xml_id (iface);
	xmlSetProp (node, "id", str);
	g_free (str);
	
	if (target->priv->entity_ref) {
		str = NULL;
		GdaObject *base = gda_object_ref_get_ref_object (target->priv->entity_ref);

		if (base) {
			str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (base));
			if (str) {
				xmlSetProp (node, "entity_ref", str);
				g_free (str);
			}
			else {
				g_set_error (error, GDA_QUERY_TARGET_ERROR,
					     GDA_QUERY_TARGET_XML_SAVE_ERROR,
					     _("Can't get XML ID of target's referenced entity"));
				xmlFreeNode (node);
				return NULL;
			}
		}
		else {
			str = (gchar *) gda_object_ref_get_ref_name (target->priv->entity_ref, NULL, NULL);
			if (str) 
				xmlSetProp (node, "table_name", str);
			else {
				g_set_error (error, GDA_QUERY_TARGET_ERROR,
					     GDA_QUERY_TARGET_XML_SAVE_ERROR,
					     _("Can't get the name of target's referenced table"));
				xmlFreeNode (node);
				return NULL;
			}
		}
		
	}

	return node;
}

static gboolean
gda_query_target_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaQueryTarget *target;
	gboolean has_name=FALSE, id=FALSE;
	gchar *str;

	g_return_val_if_fail (iface && GDA_IS_QUERY_TARGET (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_TARGET (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	target = GDA_QUERY_TARGET (iface);
	if (strcmp (node->name, "gda_query_target")) {
		g_set_error (error,
			     GDA_QUERY_TARGET_ERROR,
			     GDA_QUERY_TARGET_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_query_target>"));
		return FALSE;
	}

	str = xmlGetProp (node, "id");
	if (str) {
		gchar *tok, *ptr;

		ptr = strtok_r (str, ":", &tok);
		ptr = strtok_r (NULL, ":", &tok);
		if (*ptr && (*ptr == 'T')) {
			guint bid;
			
			bid = atoi (ptr + 1);
			gda_query_object_set_int_id (GDA_QUERY_OBJECT (target), bid);
			id = TRUE;
		}
		else {
			g_set_error (error,
				     GDA_QUERY_TARGET_ERROR,
				     GDA_QUERY_TARGET_XML_LOAD_ERROR,
				     _("XML ID for a query target should be QUxxx:Tyyy where xxx and yyy are numbers"));
			return FALSE;
		}
		g_free (str);
	}

	str = xmlGetProp (node, "entity_ref");
	if (str) {
		GType target_ref;
		const gchar *name;
		GdaObject *base;
		
		g_assert (target->priv->entity_ref);

		if (*str == 'T') 
			target_ref = GDA_TYPE_DICT_TABLE;
		else
			target_ref = GDA_TYPE_QUERY;
		gda_object_ref_set_ref_name (target->priv->entity_ref, target_ref, 
					     REFERENCE_BY_XML_ID, str);
		
		base = gda_object_ref_get_ref_object (target->priv->entity_ref);
		if (base) {
			name = gda_object_get_name (base);
			if (name && *name)
				gda_object_set_name (GDA_OBJECT (target), name);
		}
		
		has_name = TRUE;
		g_free (str);
	}
	else {
		str = xmlGetProp (node, "table_name");
		if (str) {
			g_assert (target->priv->entity_ref);
			gda_object_ref_set_ref_name (target->priv->entity_ref, GDA_TYPE_DICT_TABLE, 
						     REFERENCE_BY_NAME, str);
			gda_object_set_name (GDA_OBJECT (target), str);

			has_name = TRUE;
			g_free (str);
		}
	}

	if (has_name)
		return TRUE;
	else {
		g_set_error (error,
			     GDA_QUERY_TARGET_ERROR,
			     GDA_QUERY_TARGET_XML_LOAD_ERROR,
			     _("Error loading data from <gda_query_target> node"));
		return FALSE;
	}
}

/* 
 * GdaRenderer interface implementation
 */

static gchar *
gda_query_target_render_as_sql (GdaRenderer *iface, GdaParameterList *context, guint options, GError **error)
{
	gchar *str;
	GString *string = NULL;
	GdaQueryTarget *target;
	GdaEntity *entity;
	gboolean err = FALSE;
	GdaServerProviderInfo *sinfo = NULL;
	GdaDict *dict;
	GdaConnection *cnc;
	
	dict = gda_object_get_dict (GDA_OBJECT (iface));
	cnc = gda_dict_get_connection (dict);
	if (cnc)
		sinfo = gda_connection_get_infos (cnc);

	g_return_val_if_fail (GDA_IS_QUERY_TARGET (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_TARGET (iface)->priv, NULL);
	target = GDA_QUERY_TARGET (iface);

	entity = gda_query_target_get_represented_entity (target);
	if (entity) {
		if (GDA_IS_DICT_TABLE (entity)) {
			gchar *tmp = NULL, *tname;

			tname = (gchar *) gda_object_get_name (GDA_OBJECT (entity));
			if (!sinfo || sinfo->quote_non_lc_identifiers) {
				tmp = g_utf8_strdown (tname, -1);
				if (((*tmp <= '9') && (*tmp >= '0')) || strcmp (tmp, tname)) {
					g_free (tmp);
					tname = g_strdup_printf ("\"%s\"", tname);
					tmp = tname;
				}
			}
			string = g_string_new (tname);			
			g_free (tmp);
		}
		
		if (GDA_IS_QUERY (entity)) {
			string = g_string_new ("(");
			str = gda_renderer_render_as_sql (GDA_RENDERER (entity), context, options, error);
			if (str) {
				g_string_append (string, str);
				g_free (str);
			}
			else
				err = TRUE;
			
			g_string_append (string, ")");
		}
	}
	else {
		const gchar *cstr;
		cstr = gda_query_target_get_represented_table_name (target);
		if (cstr) 
			string = g_string_new (cstr);	
		else {
			g_set_error (error, 0, 0,
				     _("Don't know how to render target"));
			return NULL;
		}
	}

	if (!err) {
		/* adding alias */	
		if (!sinfo || sinfo->supports_alias) {
			if (!sinfo || sinfo->alias_needs_as_keyword)
				g_string_append (string, " AS ");
			else
				g_string_append_c (string, ' ');
			g_string_append (string, gda_query_target_get_alias (target));
		}
		
		str = string->str;
	}
	else
		str = NULL;
	g_string_free (string, err);

	return str;
}

static gchar *
gda_query_target_render_as_str (GdaRenderer *iface, GdaParameterList *context)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_TARGET (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_TARGET (iface)->priv, NULL);

	TO_IMPLEMENT;
	return NULL;
}
