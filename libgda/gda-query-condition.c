/* gda-query-condition.c
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-query-condition.h"
#include "gda-query.h"
#include "gda-query-join.h"
#include "gda-xml-storage.h"
#include "gda-renderer.h"
#include "gda-referer.h"
#include "gda-object-ref.h"
#include "gda-query-field.h"
#include "gda-query-field-field.h"
#include "gda-query-parsing.h"


/* 
 * Main static functions 
 */
static void gda_query_condition_class_init (GdaQueryConditionClass *class);
static void gda_query_condition_init (GdaQueryCondition *condition);
static void gda_query_condition_dispose (GObject *object);
static void gda_query_condition_finalize (GObject *object);

static void gda_query_condition_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_query_condition_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

static gboolean condition_type_is_node (GdaQueryConditionType type);

static void destroyed_object_cb (GObject *obj, GdaQueryCondition *cond);
static void destroyed_parent_cb (GdaQueryCondition *parent, GdaQueryCondition *cond);
static void destroyed_child_cb (GdaQueryCondition *child, GdaQueryCondition *cond);

static void child_cond_changed_cb (GdaQueryCondition *child, GdaQueryCondition *cond);
static void ops_ref_lost_cb (GdaObjectRef *refbase, GdaQueryCondition *cond);


/* XML storage interface */
static void        gda_query_condition_xml_storage_init (GdaXmlStorageIface *iface);
static xmlNodePtr  gda_query_condition_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_query_condition_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* Renderer interface */
static void            gda_query_condition_renderer_init      (GdaRendererIface *iface);
static gchar          *gda_query_condition_render_as_sql   (GdaRenderer *iface, GdaParameterList *context, 
							    GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar          *gda_query_condition_render_as_str   (GdaRenderer *iface, GdaParameterList *context);

/* Referer interface */
static void        gda_query_condition_referer_init        (GdaRefererIface *iface);
static gboolean    gda_query_condition_activate            (GdaReferer *iface);
static void        gda_query_condition_deactivate          (GdaReferer *iface);
static gboolean    gda_query_condition_is_active           (GdaReferer *iface);
static GSList     *gda_query_condition_get_ref_objects     (GdaReferer *iface);
static void        gda_query_condition_replace_refs        (GdaReferer *iface, GHashTable *replacements);

static void        gda_query_condition_set_int_id          (GdaQueryObject *condition, guint id);
#ifdef GDA_DEBUG
static void        gda_query_condition_dump                (GdaQueryCondition *condition, guint offset);
#endif

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP_QUERY,
	PROP_JOIN,
	PROP_COND_TYPE
};


struct _GdaQueryConditionPrivate
{
	GdaQuery              *query;
	GdaQueryJoin          *join; /* not NULL if cond is a join cond => more specific tests */
	GdaQueryConditionType  type;
	GdaQueryCondition     *cond_parent;
	GSList                *cond_children;
	GdaObjectRef          *ops[3]; /* references on GdaQueryField objects */
	gboolean               destroy_if_ref_lost; /* if TRUE, then commits suicide when one of the ops[] loses its ref to an object */

	gint                   internal_transaction; /* > 0 to revent emission of the "changed" signal, when
						      * several changes must occur before the condition 
						      * has a stable structure again */
};

/* module error */
GQuark gda_query_condition_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_query_condition_error");
	return quark;
}


GType
gda_query_condition_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryConditionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_condition_class_init,
			NULL,
			NULL,
			sizeof (GdaQueryCondition),
			0,
			(GInstanceInitFunc) gda_query_condition_init
		};
		
		static const GInterfaceInfo xml_storage_info = {
                        (GInterfaceInitFunc) gda_query_condition_xml_storage_init,
                        NULL,
                        NULL
                };

                static const GInterfaceInfo renderer_info = {
                        (GInterfaceInitFunc) gda_query_condition_renderer_init,
                        NULL,
                        NULL
                };

                static const GInterfaceInfo referer_info = {
                        (GInterfaceInitFunc) gda_query_condition_referer_init,
                        NULL,
                        NULL
                };

		type = g_type_register_static (GDA_TYPE_QUERY_OBJECT, "GdaQueryCondition", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
                g_type_add_interface_static (type, GDA_TYPE_RENDERER, &renderer_info);
                g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
	}
	return type;
}

static void
gda_query_condition_xml_storage_init (GdaXmlStorageIface *iface)
{
        iface->get_xml_id = NULL;
        iface->save_to_xml = gda_query_condition_save_to_xml;
        iface->load_from_xml = gda_query_condition_load_from_xml;
}

static void
gda_query_condition_renderer_init (GdaRendererIface *iface)
{
        iface->render_as_sql = gda_query_condition_render_as_sql;
        iface->render_as_str = gda_query_condition_render_as_str;
	iface->is_valid = NULL;
}

static void
gda_query_condition_referer_init (GdaRefererIface *iface)
{
        iface->activate = gda_query_condition_activate;
        iface->deactivate = gda_query_condition_deactivate;
        iface->is_active = gda_query_condition_is_active;
        iface->get_ref_objects = gda_query_condition_get_ref_objects;
        iface->replace_refs = gda_query_condition_replace_refs;
}

static void
gda_query_condition_class_init (GdaQueryConditionClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_query_condition_dispose;
	object_class->finalize = gda_query_condition_finalize;

	/* Properties */
	object_class->set_property = gda_query_condition_set_property;
	object_class->get_property = gda_query_condition_get_property;
	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_object ("query", "Query to which the condition belongs", NULL, 
                                                               GDA_TYPE_QUERY,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE |
								G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_JOIN,
					 g_param_spec_object ("join", NULL, NULL, 
                                                               GDA_TYPE_QUERY_JOIN,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_COND_TYPE,
					 g_param_spec_int ("cond_type", "Type of condition", NULL,
							   GDA_QUERY_CONDITION_NODE_AND, GDA_QUERY_CONDITION_TYPE_UNKNOWN,
							   GDA_QUERY_CONDITION_TYPE_UNKNOWN,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	/* virtual functions */
	GDA_QUERY_OBJECT_CLASS (class)->set_int_id = (void (*)(GdaQueryObject *, guint)) gda_query_condition_set_int_id;
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_query_condition_dump;
#endif
}

static void
gda_query_condition_init (GdaQueryCondition *condition)
{
	gint i;
	condition->priv = g_new0 (GdaQueryConditionPrivate, 1);
	condition->priv->query = NULL;
	condition->priv->join = NULL;
	condition->priv->type = GDA_QUERY_CONDITION_TYPE_UNKNOWN;
	condition->priv->cond_parent = NULL;
	condition->priv->cond_children = NULL;
	for (i=0; i<3; i++)
		condition->priv->ops[i] = NULL;
	condition->priv->destroy_if_ref_lost = TRUE; /* TODO: create a property for this ? */
	condition->priv->internal_transaction = FALSE;
}

/**
 * gda_query_condition_new
 * @query: a #GdaQuery object
 * @type: the condition type
 *
 * Creates a new #GdaQueryCondition object
 *
 * Returns: the newly created object
 */
GdaQueryCondition *
gda_query_condition_new (GdaQuery *query, GdaQueryConditionType type)
{
	GObject *obj;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);

	obj = g_object_new (GDA_TYPE_QUERY_CONDITION, "dict", gda_object_get_dict (GDA_OBJECT (query)), 
			    "query", query, "cond_type", type, NULL);

	return (GdaQueryCondition*) obj;
}

static void
ops_ref_lost_cb (GdaObjectRef *refbase, GdaQueryCondition *cond)
{
	if (cond->priv->destroy_if_ref_lost &&
	    ! cond->priv->internal_transaction) {
		cond->priv->destroy_if_ref_lost = FALSE;
		/* commit suicide */
		gda_object_destroy (GDA_OBJECT (cond));
	}
}

/**
 * gda_query_condition_new_copy
 * @orig: a #GdaQueryCondition to copy
 * @replacements: a hash table to store replacements, or %NULL
 *
 * This is a copy constructor
 *
 * Returns: the new object
 */
GdaQueryCondition *
gda_query_condition_new_copy (GdaQueryCondition *orig, GHashTable *replacements)
{
	GObject *obj;
	GdaQuery *query;
	GdaQueryCondition *newcond;
	GdaDict *dict;
	GSList *list;
	gint i;

	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (orig), NULL);
	g_return_val_if_fail (orig->priv, NULL);
	g_object_get (G_OBJECT (orig), "query", &query, NULL);
	g_return_val_if_fail (query, NULL);

	dict = gda_object_get_dict (GDA_OBJECT (query));
	obj = g_object_new (GDA_TYPE_QUERY_CONDITION, "dict", dict, 
			    "query", query, 
			    "cond_type", orig->priv->type, NULL);

	newcond = GDA_QUERY_CONDITION (obj);
	if (replacements)
		g_hash_table_insert (replacements, orig, newcond);

	/* operators */
	for (i=0; i<3; i++) {
		g_object_unref (newcond->priv->ops[i]);
		newcond->priv->ops[i] = (GdaObjectRef *) gda_object_ref_new_copy (orig->priv->ops[i]);
		g_signal_connect (G_OBJECT (newcond->priv->ops[i]), "ref_lost",
				  G_CALLBACK (ops_ref_lost_cb), newcond);
	}

	/* children conditions */
	list = orig->priv->cond_children;
	while (list) {
		GObject *ccond;

		ccond = G_OBJECT (gda_query_condition_new_copy (GDA_QUERY_CONDITION (list->data), replacements));
		gda_query_condition_node_add_child (newcond, GDA_QUERY_CONDITION (ccond), NULL);
		g_object_unref (ccond);
		list = g_slist_next (list);
	}

	g_object_unref (query);
	return newcond;
}

/**
 * gda_query_condition_new_from_sql
 * @query: a #GdaQuery object
 * @sql_cond: a SQL statement representing a valid condition
 * @targets: location to store a list of targets used by the new condition (and its children), or %NULL
 * @error: location to store error, or %NULL
 *
 * Creates a new #GdaQueryCondition object, which references other objects of @query, 
 * from the @sql_cond statement.
 *
 * Returns: a new #GdaQueryCondition, or %NULL if there was an error in @sql_cond
 */
GdaQueryCondition *
gda_query_condition_new_from_sql (GdaQuery *query, const gchar *sql_cond, GSList **targets, GError **error)
{
	gchar *sql;
	sql_statement *result;
	GdaQueryCondition *newcond = NULL;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (sql_cond && *sql_cond, FALSE);
	
	sql = g_strdup_printf ("SELECT * FROM table WHERE %s", sql_cond);
	result = sql_parse_with_error (sql, error);
	if (result) {
		if (((sql_select_statement *)(result->statement))->where) {
			ParseData *pdata;
			
			pdata = parse_data_new (query);
			parse_data_compute_targets_hash (query, pdata);
			newcond = parsed_create_complex_condition (query, pdata, 
								   ((sql_select_statement *)(result->statement))->where,
								   TRUE, targets, error);
			parse_data_destroy (pdata);
		}
		else 
			g_set_error (error, GDA_QUERY_JOIN_ERROR, GDA_QUERY_JOIN_PARSE_ERROR,
				     _("No join condition found in '%s'"), sql_cond);
		sql_destroy (result);
	}
	else 
		if (error && !(*error))
			g_set_error (error, GDA_QUERY_JOIN_ERROR, GDA_QUERY_JOIN_PARSE_ERROR,
				     _("Error parsing '%s'"), sql_cond);
	g_free (sql);

	return newcond;
}

static void
gda_query_condition_dispose (GObject *object)
{
	GdaQueryCondition *condition;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_CONDITION (object));

	condition = GDA_QUERY_CONDITION (object);
	if (condition->priv) {
		gint i;

		condition->priv->internal_transaction ++;
		gda_object_destroy_check (GDA_OBJECT (object));

		if (condition->priv->cond_parent) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (condition->priv->cond_parent),
							      G_CALLBACK (destroyed_parent_cb), condition);
			condition->priv->cond_parent = NULL;
		}

		if (condition->priv->query) {
			gda_query_undeclare_condition (condition->priv->query, condition);
                        g_signal_handlers_disconnect_by_func (G_OBJECT (condition->priv->query),
                                                              G_CALLBACK (destroyed_object_cb), condition);
                        condition->priv->query = NULL;
                }

		if (condition->priv->join) {
                        g_signal_handlers_disconnect_by_func (G_OBJECT (condition->priv->join),
                                                              G_CALLBACK (destroyed_object_cb), condition);
                        condition->priv->join = NULL;
                }

		for (i=0; i<3; i++)
			if (condition->priv->ops[i]) {
				g_object_unref (condition->priv->ops[i]);
				condition->priv->ops[i] = NULL;
			}

		while (condition->priv->cond_children)
			destroyed_child_cb (GDA_QUERY_CONDITION (condition->priv->cond_children->data), condition);
		condition->priv->internal_transaction --;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
destroyed_object_cb (GObject *obj, GdaQueryCondition *cond)
{
	gda_object_destroy (GDA_OBJECT (cond));
}

static void
destroyed_parent_cb (GdaQueryCondition *parent, GdaQueryCondition *cond)
{
	g_assert (cond->priv->cond_parent == parent);
	g_signal_handlers_disconnect_by_func (G_OBJECT (parent),
					      G_CALLBACK (destroyed_parent_cb) , cond);
	cond->priv->cond_parent = NULL;
	gda_object_destroy (GDA_OBJECT (cond));
}

static void
destroyed_child_cb (GdaQueryCondition *child, GdaQueryCondition *cond)
{
	g_assert (g_slist_find (cond->priv->cond_children, child));

	/* child signals disconnect */
	g_signal_handlers_disconnect_by_func (G_OBJECT (cond),
					      G_CALLBACK (destroyed_parent_cb) , child);
	child->priv->cond_parent = NULL;

	/* parent signal disconnect */
	g_signal_handlers_disconnect_by_func (G_OBJECT (child),
					      G_CALLBACK (destroyed_child_cb) , cond);
	g_signal_handlers_disconnect_by_func (G_OBJECT (child),
					      G_CALLBACK (child_cond_changed_cb) , cond);
	g_object_unref (G_OBJECT (child));
	cond->priv->cond_children = g_slist_remove (cond->priv->cond_children, child);
	if (! cond->priv->internal_transaction)
		gda_object_signal_emit_changed (GDA_OBJECT (cond));
}


static void
gda_query_condition_finalize (GObject   * object)
{
	GdaQueryCondition *condition;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_CONDITION (object));

	condition = GDA_QUERY_CONDITION (object);
	if (condition->priv) {
		g_free (condition->priv);
		condition->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_query_condition_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	GdaQueryCondition *condition;
	guint id;
	gint i;
	GdaDict *dict;

	condition = GDA_QUERY_CONDITION (object);
	if (condition->priv) {
		switch (param_id) {
		case PROP_QUERY: {
			GdaQuery* ptr = g_value_get_object (value);
                        g_return_if_fail (ptr && GDA_IS_QUERY (ptr));

                        if (condition->priv->query) {
                                if (condition->priv->query == GDA_QUERY (ptr))
                                        return;
				
				gda_query_undeclare_condition (condition->priv->query, condition);
                                g_signal_handlers_disconnect_by_func (G_OBJECT (condition->priv->query),
                                                                      G_CALLBACK (destroyed_object_cb), 
								      condition);
                        }

                        condition->priv->query = GDA_QUERY (ptr);
			gda_object_connect_destroy (ptr, G_CALLBACK (destroyed_object_cb), condition);
			gda_query_declare_condition (condition->priv->query, condition);

			dict = gda_object_get_dict (GDA_OBJECT(ptr));
			for (i=0; i<3; i++) {
				condition->priv->ops[i] = GDA_OBJECT_REF (gda_object_ref_new (dict));
				g_signal_connect (G_OBJECT (condition->priv->ops[i]), "ref_lost",
						  G_CALLBACK (ops_ref_lost_cb), condition);
			}

			g_object_get (G_OBJECT (ptr), "cond_serial", &id, NULL);
			gda_query_object_set_int_id (GDA_QUERY_OBJECT (object), id);			
			break;
                }
		case PROP_JOIN: {
			GdaQueryJoin* ptr = GDA_QUERY_JOIN (g_value_get_object (value));
			if (ptr) {
				g_return_if_fail (GDA_IS_QUERY_JOIN (ptr));
				g_return_if_fail (gda_query_join_get_query (GDA_QUERY_JOIN (ptr)) == condition->priv->query);
			}

                        if (condition->priv->join) {
                                if (condition->priv->join == GDA_QUERY_JOIN (ptr))
                                        return;

                                g_signal_handlers_disconnect_by_func (G_OBJECT (condition->priv->join),
                                                                      G_CALLBACK (destroyed_object_cb), 
								      condition);
				condition->priv->join = NULL;
                        }

			if (ptr) {
				condition->priv->join = GDA_QUERY_JOIN (ptr);
				gda_object_connect_destroy (ptr, G_CALLBACK (destroyed_object_cb), condition);
			}
			break;
                }
		case PROP_COND_TYPE:
			if (condition->priv->type == g_value_get_int (value))
				return;

			condition->priv->type = g_value_get_int (value);
			gda_object_signal_emit_changed (GDA_OBJECT (condition));
			break;
		}
	}
}

static void
gda_query_condition_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	GdaQueryCondition *condition;

	condition = GDA_QUERY_CONDITION (object);
        if (condition->priv) {
                switch (param_id) {
                case PROP_QUERY:
			g_value_set_object (value, G_OBJECT (condition->priv->query));
                        break;
		case PROP_JOIN:
			g_value_set_object (value, G_OBJECT (condition->priv->join));
                        break;
		case PROP_COND_TYPE:
			g_value_set_int (value, condition->priv->type);
			break;
                }
        }
}

static gboolean
condition_type_is_node (GdaQueryConditionType type)
{
	gboolean retval;

	switch (type) {
	case GDA_QUERY_CONDITION_NODE_AND:
	case GDA_QUERY_CONDITION_NODE_OR:
	case GDA_QUERY_CONDITION_NODE_NOT:
		retval = TRUE;
		break;
	default:
		retval = FALSE;
		break;
	}
	return retval;
}

/**
 * gda_query_condition_set_cond_type
 * @condition: a #GdaQueryCondition object
 * @type:
 *
 * Sets the kind of condition @condition represents. If @type implies a node condition and
 * @condition currently represents a leaf, or if @type implies a leaf condition and
 * @condition currently represents a node, then @condition is changed without any error.
 */
void
gda_query_condition_set_cond_type (GdaQueryCondition *condition, GdaQueryConditionType type)
{
	g_return_if_fail (GDA_IS_QUERY_CONDITION (condition));
	g_return_if_fail (condition->priv);
	if (condition->priv->type == type)
		return;

	if (condition_type_is_node (condition->priv->type) != condition_type_is_node (type)) {
		/* FIXME: remove all the children and all the operators */
		TO_IMPLEMENT;
	}

	condition->priv->type = type;
	if (! condition->priv->internal_transaction)
		gda_object_signal_emit_changed (GDA_OBJECT (condition));
}

/**
 * gda_query_condition_get_cond_type
 * @condition: a #GdaQueryCondition object
 *
 * Get the type of @condition
 *
 * Returns: the type
 */
GdaQueryConditionType
gda_query_condition_get_cond_type (GdaQueryCondition *condition)
{
	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (condition), GDA_QUERY_CONDITION_TYPE_UNKNOWN);
	g_return_val_if_fail (condition->priv, GDA_QUERY_CONDITION_TYPE_UNKNOWN);

	return condition->priv->type;
}

/**
 * gda_query_condition_get_children
 * @condition: a #GdaQueryCondition object
 *
 * Get a list of #GdaQueryCondition objects which are children of @condition
 *
 * Returns: a new list of #GdaQueryCondition objects
 */
GSList *
gda_query_condition_get_children (GdaQueryCondition *condition)
{
	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (condition), NULL);
	g_return_val_if_fail (condition->priv, NULL);

	if (condition->priv->cond_children)
		return g_slist_copy (condition->priv->cond_children);
	else
		return NULL;
}

/**
 * gda_query_condition_get_parent
 * @condition: a #GdaQueryCondition object
 *
 * Get the #GdaQueryCondition object which is parent of @condition
 *
 * Returns: the parent object, or %NULL
 */
GdaQueryCondition *
gda_query_condition_get_parent (GdaQueryCondition *condition)
{
	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (condition), NULL);
	g_return_val_if_fail (condition->priv, NULL);

	return condition->priv->cond_parent;
}


/**
 * gda_query_condition_get_child_by_xml_id
 * @condition: a #GdaQueryCondition object
 * @xml_id: the XML Id of the requested #GdaQueryCondition child
 *
 * Get a pointer to a #GdaQueryCondition child from its XML Id
 *
 * Returns: the #GdaQueryCondition object, or %NULL if not found
 */
GdaQueryCondition *
gda_query_condition_get_child_by_xml_id (GdaQueryCondition *condition, const gchar *xml_id)
{
	TO_IMPLEMENT;
	return NULL;
}

/**
 * gda_query_condition_is_ancestor
 * @condition: a #GdaQueryCondition object
 * @ancestor: a #GdaQueryCondition object
 *
 * Tests if @ancestor is an ancestor of @condition
 *
 * Returns: TRUE if @ancestor is an ancestor of @condition
 */
gboolean
gda_query_condition_is_ancestor (GdaQueryCondition *condition, GdaQueryCondition *ancestor)
{
	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (condition), FALSE);
	g_return_val_if_fail (condition->priv, FALSE);
	g_return_val_if_fail (ancestor && GDA_IS_QUERY_CONDITION (ancestor), FALSE);
	g_return_val_if_fail (ancestor->priv, FALSE);
	
	if (condition->priv->cond_parent == ancestor)
		return TRUE;
	if (condition->priv->cond_parent)
		return gda_query_condition_is_ancestor (condition->priv->cond_parent, ancestor);

	return FALSE;
}

/**
 * gda_query_condition_is_leaf
 * @condition: a #GdaQueryCondition object
 *
 * Tells if @condition is a leaf condition (not AND, OR, NOT, etc)
 *
 * Returns: TRUE if @condition is a leaf condition
 */
gboolean
gda_query_condition_is_leaf (GdaQueryCondition *condition)
{
	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (condition), FALSE);
	g_return_val_if_fail (condition->priv, FALSE);

	switch (condition->priv->type) {
	case GDA_QUERY_CONDITION_NODE_AND:
	case GDA_QUERY_CONDITION_NODE_OR:
	case GDA_QUERY_CONDITION_NODE_NOT:
		return FALSE;
	default:
		return TRUE;
	}
}

static gboolean gda_query_condition_node_add_child_pos (GdaQueryCondition *condition, GdaQueryCondition *child, gint pos, GError **error);


/**
 * gda_query_condition_node_add_child
 * @condition: a #GdaQueryCondition object
 * @child: a #GdaQueryCondition object
 * @error: location to store error, or %NULL
 *
 * Adds a child to @condition; this is possible only if @condition is a node type (AND, OR, etc)
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_query_condition_node_add_child (GdaQueryCondition *condition, GdaQueryCondition *child, GError **error)
{
	return gda_query_condition_node_add_child_pos (condition, child, -1, error);
}

static gboolean
gda_query_condition_node_add_child_pos (GdaQueryCondition *condition, GdaQueryCondition *child, gint pos, GError **error)
{
	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (condition), FALSE);
	g_return_val_if_fail (condition->priv, FALSE);
	g_return_val_if_fail (child && GDA_IS_QUERY_CONDITION (child), FALSE);
	g_return_val_if_fail (child->priv, FALSE);
	g_return_val_if_fail (!gda_query_condition_is_leaf (condition), FALSE);

	if (child->priv->cond_parent == condition)
		return TRUE;

	g_object_ref (G_OBJECT (child));

	if (child->priv->cond_parent) 
		gda_query_condition_node_del_child (child->priv->cond_parent, child);

	if (gda_query_condition_is_ancestor (condition, child)) {
		g_set_error (error,
                             GDA_QUERY_CONDITION_ERROR,
                             GDA_QUERY_CONDITION_PARENT_ERROR,
			     _("Conditions hierarchy error"));
		return FALSE;
	}

	/* a NOT node can only have one child */
	if (condition->priv->cond_children && (condition->priv->type == GDA_QUERY_CONDITION_NODE_NOT)) {
		g_set_error (error,
                             GDA_QUERY_CONDITION_ERROR,
                             GDA_QUERY_CONDITION_PARENT_ERROR,
			     _("A NOT node can only have one child"));
		return FALSE;
	}

	/* child part */
	child->priv->cond_parent = condition;
	gda_object_connect_destroy (condition, G_CALLBACK (destroyed_parent_cb), child);

	/* parent part */
	condition->priv->cond_children = g_slist_insert (condition->priv->cond_children, child, pos);
	gda_object_connect_destroy (child, G_CALLBACK (destroyed_child_cb), condition);
	g_signal_connect (G_OBJECT (child), "changed",
			  G_CALLBACK (child_cond_changed_cb), condition);

	if (! condition->priv->internal_transaction)
		gda_object_signal_emit_changed (GDA_OBJECT (condition));

	return TRUE;
}

/* Forwards the "changed" signal from children to parent condition */
static void
child_cond_changed_cb (GdaQueryCondition *child, GdaQueryCondition *cond)
{
	if (! cond->priv->internal_transaction)
		gda_object_signal_emit_changed (GDA_OBJECT (cond));
}

/**
 * gda_query_condition_node_del_child
 * @condition: a #GdaQueryCondition object
 * @child: a #GdaQueryCondition object
 *
 * Removes a child from @condition; this is possible only if @condition is a node type (AND, OR, etc)
 */
void
gda_query_condition_node_del_child (GdaQueryCondition *condition, GdaQueryCondition *child)
{
	g_return_if_fail (GDA_IS_QUERY_CONDITION (condition));
	g_return_if_fail (condition->priv);
	g_return_if_fail (child && GDA_IS_QUERY_CONDITION (child));
	g_return_if_fail (child->priv);
	g_return_if_fail (child->priv->cond_parent == condition);
	g_return_if_fail (!gda_query_condition_is_leaf (condition));

	destroyed_child_cb (child, condition);
}

/**
 * gda_query_condition_leaf_set_left_op
 * @condition: a #GdaQueryCondition object
 * @op: which oparetor is concerned
 * @field: a # GdaQueryField object
 *
 * Sets one of @condition's operators
 */
void
gda_query_condition_leaf_set_operator (GdaQueryCondition *condition, GdaQueryConditionOperator op, GdaQueryField *field)
{
	GdaQuery *query1, *query2;

	g_return_if_fail (GDA_IS_QUERY_CONDITION (condition));
	g_return_if_fail (condition->priv);
	g_return_if_fail (field && GDA_IS_QUERY_FIELD (field));
	g_return_if_fail (gda_query_condition_is_leaf (condition));

	g_object_get (G_OBJECT (condition), "query", &query1, NULL);
	g_object_get (G_OBJECT (field), "query", &query2, NULL);
	g_return_if_fail (query1);
	g_return_if_fail (query1 == query2);
	g_object_unref (query1);
	g_object_unref (query2);

	gda_object_ref_set_ref_object_type (condition->priv->ops[op], GDA_OBJECT (field), GDA_TYPE_QUERY_FIELD);
}

/**
 * gda_query_condition_leaf_get_operator
 * @condition: a #GdaQueryCondition object
 * @op: which oparetor is concerned
 *
 * Get one of @condition's operators.
 *
 * Returns: the requested #GdaQueryField object
 */
GdaQueryField *
gda_query_condition_leaf_get_operator  (GdaQueryCondition *condition, GdaQueryConditionOperator op)
{
	GdaObject *base;
	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (condition), NULL);
	g_return_val_if_fail (condition->priv, NULL);
	g_return_val_if_fail (gda_query_condition_is_leaf (condition), NULL);

	gda_object_ref_activate (condition->priv->ops[op]);
	base = gda_object_ref_get_ref_object (condition->priv->ops[op]);
	if (base)
		return GDA_QUERY_FIELD (base);
	else
		return NULL;
}

/*
 * Analyses @field and the #GdaQueryTargets object used
 * if @target is not NULL, and if there is only one target used, then set it to that target
 *
 * Returns: 0 if no target is used
 *          1 if one target is used
 *          2 if more than one target is used
 */
static gint
qfield_uses_nb_target (GdaQueryField *field, GdaQueryTarget **target)
{
	gint retval = 0;
	GdaQueryTarget *t1 = NULL;

	if (!field) 
		retval = 0;
	else {
		if (GDA_IS_QUERY_FIELD_FIELD (field)) {
			t1 = gda_query_field_field_get_target (GDA_QUERY_FIELD_FIELD (field));
			retval = 1;
		}
		else {
			GSList *list = gda_referer_get_ref_objects (GDA_REFERER (field));
			GSList *tmp;
			
			tmp = list;
			while (tmp) {
				if (GDA_IS_QUERY_FIELD_FIELD (tmp->data)) {
					if (!t1) {
						t1 = gda_query_field_field_get_target (GDA_QUERY_FIELD_FIELD (tmp->data));
						retval = 1;
					}
					else
						if (t1 != gda_query_field_field_get_target (GDA_QUERY_FIELD_FIELD (tmp->data)))
							retval = 2;
				}
				tmp = g_slist_next (tmp);
			}
			g_slist_free (list);
		}
	}

	if (retval && target)
		*target = t1;
	
	return retval;
}

static gboolean
gda_query_condition_represents_join_real (GdaQueryCondition *condition,
				   GdaQueryTarget **target1, GdaQueryTarget **target2, 
				   gboolean *is_equi_join, 
				   gboolean force_2_targets, gboolean force_fields)
{
	GdaQueryTarget *t1 = NULL, *t2 = NULL;
	gboolean retval = TRUE;
	gboolean is_equi;
	gint nb;

	if (gda_query_condition_is_leaf (condition)) {
		GdaQueryField *field;

		field = gda_query_condition_leaf_get_operator (condition, GDA_QUERY_CONDITION_OP_LEFT);
		nb = qfield_uses_nb_target (field, &t1);
		if (nb == 2)
			retval = FALSE;
		if (force_fields && (nb == 0))
			retval = FALSE;

		if (retval) {
			field = gda_query_condition_leaf_get_operator (condition, GDA_QUERY_CONDITION_OP_RIGHT);
			nb = qfield_uses_nb_target (field, &t2);
			if (nb == 2)
				retval = FALSE;
			if (force_fields && (nb == 0))
				retval = FALSE;
		}
		
		if (retval && (condition->priv->type == GDA_QUERY_CONDITION_LEAF_BETWEEN)) {
			if (force_fields)
				retval = FALSE;
			else {
				GdaQueryTarget *t2bis = NULL;
				field = gda_query_condition_leaf_get_operator (condition, GDA_QUERY_CONDITION_OP_RIGHT2);
				if (qfield_uses_nb_target (field, &t2bis) == 2)
					retval = FALSE;
				if (t2bis && (t2bis != t2))
					retval = FALSE;
			}
		}

		is_equi = (condition->priv->type == GDA_QUERY_CONDITION_LEAF_EQUAL);
		if (force_fields && !is_equi)
			retval = FALSE;
	}
	else {
		gpointer tref[2] = {NULL, NULL};
		GSList *list = condition->priv->cond_children;
		is_equi = TRUE;

		while (list && retval) {
			GdaQueryTarget *tmp1 = NULL, *tmp2 = NULL;
			gboolean eqj = FALSE;

			retval = gda_query_condition_represents_join_real (GDA_QUERY_CONDITION (list->data), 
								    &tmp1, &tmp2, &eqj, FALSE, force_fields);
			if (retval) {
				if (tmp1) {
					gboolean pushed = FALSE;
					gint i = 0;
					while ((i<2) && !pushed) {
						if (! tref[i]) {
							tref[i] = tmp1;
							pushed = TRUE;
						}
						else 
							if (tref[i] == tmp1) pushed = TRUE;
						i++;
					}
					if (!pushed)
						retval = FALSE;
				}
				if (tmp2) {
					gboolean pushed = FALSE;
					gint i = 0;
					while ((i<2) && !pushed) {
						if (! tref[i]) {
							tref[i] = tmp2;
							pushed = TRUE;
						}
						else 
							if (tref[i] == tmp2) pushed = TRUE;
						i++;
					}
					if (!pushed)
						retval = FALSE;
				}

				is_equi = is_equi && eqj;
			}
			list = g_slist_next (list);
		}
		if (retval) {
			t1 = tref[0];
			t2 = tref[1];
		}

		is_equi = is_equi && (condition->priv->type == GDA_QUERY_CONDITION_NODE_AND);
	}

	if (retval) {
		if (force_2_targets && (!t1 || !t2))
			retval = FALSE;
		else {
			if (force_fields && (t1 == t2))
				retval = FALSE;
			else {
				if (target1) *target1 = t1;
				if (target1) *target2 = t2;
				if (is_equi_join) *is_equi_join = is_equi;
			}
		}
	}
	
	return retval;
}


/**
 * gda_query_condition_represents_join
 * @condition: a #GdaQueryCondition object
 * @target1: place to store one of the targets, or %NULL
 * @target2: place to store the other target, or %NULL
 * @is_equi_join: place to store if the join is an equi join
 *
 * Tells if @condition represents a join condition: it is a condition (within a #GdaQuery object)
 * for which the only #GdaQueryFieldField fields taking part in the condition are from two distincts
 * #GdaQueryTarget objects. Such conditions can be assigned to a #GdaQueryJoin object using the 
 * gda_query_join_set_condition() or gda_query_join_set_condition_from_fkcons() methods.
 *
 * Additionnaly, if @condition is a join condition, and if @target1 and @target2 are not %NULL
 * then they are set to point to the two #GdaQueryTarget objects taking part in the condition. In this
 * case @target1 and @target2 wil hold non %NULL values.
 *
 * In a similar way, if @is_equi_join is not %NULL, then it will be set to TRUE if the join
 * condition is an equi join (that is the only comparison operator is the equal sign and there are
 * only AND operators in the condition).
 *
 * If @condition is not a join condition, then @target1, @target2 and @is_equi_join are left
 * untouched.
 * 
 * Returns: TRUE if @condition is a join condition
 */
gboolean
gda_query_condition_represents_join (GdaQueryCondition *condition,
			      GdaQueryTarget **target1, GdaQueryTarget **target2, gboolean *is_equi_join)
{
	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (condition), FALSE);
	g_return_val_if_fail (condition->priv, FALSE);
	
	return gda_query_condition_represents_join_real (condition, target1, target2, is_equi_join, TRUE, FALSE);
}

/**
 * gda_query_condition_represents_join_strict
 * @condition: a #GdaQueryCondition object
 * @target1: place to store one of the targets, or %NULL
 * @target2: place to store the other target, or %NULL
 *
 * Tells if @condition represents a strict join condition: it is a join condition as defined for the 
 * gda_query_condition_represents_join() method, but where the condition is either "target1.field1=target2.field2"
 * or a list of such conditions conjuncted by the AND operator.
 *
 * If @condition is not a join condition, then @target1 and @target2 are left
 * untouched.
 * 
 * Returns: TRUE if @condition is a strict join condition
 */
gboolean
gda_query_condition_represents_join_strict (GdaQueryCondition *condition,
				     GdaQueryTarget **target1, GdaQueryTarget **target2)
{
	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (condition), FALSE);
	g_return_val_if_fail (condition->priv, FALSE);
	
	return gda_query_condition_represents_join_real (condition, target1, target2, NULL, TRUE, TRUE);
}

static GSList*
cond_get_main_sub_conditions (GdaQueryCondition *cond)
{
	GSList *retval = NULL;

	if (cond->priv->type == GDA_QUERY_CONDITION_NODE_AND) {
		GSList *list;
		
		list = cond->priv->cond_children;
		while (list) {
			GSList *tmp = cond_get_main_sub_conditions (GDA_QUERY_CONDITION (list->data));
			if (tmp)
				retval = g_slist_concat (retval, tmp);
			list = g_slist_next (list);
		}
	}
	else
		retval = g_slist_append (retval, cond);
	
	return retval;
}

/**
 * gda_query_condition_get_main_conditions
 * @condition: a #GdaQueryCondition object
 *
 * Makes a list of all the conditions which
 * are always verified by @condition when it returns TRUE when evaluated.
 * Basically the returned list lists the atomic conditions which are AND'ed
 * together to form the complex @condition.
 *
 * Examples: if @condition is:
 * <itemizedlist>
 *   <listitem><para> "A and B" then the list will contains {A, B}</para></listitem>
 *   <listitem><para> "A and (B or C)" it will contain {A, B or C}</para></listitem>
 *   <listitem><para> "A and (B and not C)", it will contain {A, B, not C}</para></listitem>
 * </itemizedlist>
 *
 * Returns: a new list of #GdaQueryCondition objects
 */
GSList *
gda_query_condition_get_main_conditions (GdaQueryCondition *condition)
{
	g_return_val_if_fail (GDA_IS_QUERY_CONDITION (condition), FALSE);
	g_return_val_if_fail (condition->priv, FALSE);

	return cond_get_main_sub_conditions (condition);
}


static const gchar *condition_type_to_str (GdaQueryConditionType type);

#ifdef GDA_DEBUG
static void
gda_query_condition_dump (GdaQueryCondition *condition, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (GDA_IS_QUERY_CONDITION (condition));
	
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (condition->priv) {
		GSList *children;
		if (gda_object_get_name (GDA_OBJECT (condition)))
			g_print ("%s" D_COL_H1 "GdaQueryCondition" D_COL_NOR " %s \"%s\" (%p, id=%s, query=%p) ",
				 str, condition_type_to_str (condition->priv->type), 
				 gda_object_get_name (GDA_OBJECT (condition)), 
				 condition, gda_object_get_id (GDA_OBJECT (condition)), condition->priv->query);
		else
			g_print ("%s" D_COL_H1 "GdaQueryCondition" D_COL_NOR " %s (%p, id=%s, query=%p) ",
				 str, condition_type_to_str (condition->priv->type), 
				 condition, gda_object_get_id (GDA_OBJECT (condition)), condition->priv->query);
		if (gda_query_condition_is_active (GDA_REFERER (condition)))
			g_print ("Active");
		else
			g_print (D_COL_ERR "Non active" D_COL_NOR);

		for (i=0; i<3; i++) {
			if (i == 0)
				g_print (" (");
			else
				g_print (", ");
			if (condition->priv->ops[i]) {
				GdaObject *ref;

				ref = gda_object_ref_get_ref_object (condition->priv->ops[i]);
				if (ref) {
					gchar *id = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (ref));
					g_print ("'%s'=%p", id, ref);
					g_free (id);
				}
				else
					g_print ("_null");	
			}
			else
				g_print ("NULL");
		}
		g_print (")\n");

		children = condition->priv->cond_children;
		while (children) {
			gda_query_condition_dump (GDA_QUERY_CONDITION (children->data), offset+5);
			children = g_slist_next (children);
		}
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, condition);
}
#endif

static void
gda_query_condition_set_int_id (GdaQueryObject *condition, guint id)
{
	gchar *str;

	str = g_strdup_printf ("%s:C%u", gda_object_get_id (GDA_OBJECT (GDA_QUERY_CONDITION (condition)->priv->query)), id);
	gda_object_set_id (GDA_OBJECT (condition), str);
	g_free (str);
}

/* 
 * GdaXmlStorage interface implementation
 */

static const gchar *
condition_type_to_str (GdaQueryConditionType type)
{
	switch (type) {
	case GDA_QUERY_CONDITION_NODE_AND:
		return "AND";
	case GDA_QUERY_CONDITION_NODE_OR:
		return "OR";
        case GDA_QUERY_CONDITION_NODE_NOT:
		return "NOT";
        case GDA_QUERY_CONDITION_LEAF_EQUAL:
		return "EQ";
        case GDA_QUERY_CONDITION_LEAF_DIFF:
		return "NE";
        case GDA_QUERY_CONDITION_LEAF_SUP:
		return "SUP";
        case GDA_QUERY_CONDITION_LEAF_SUPEQUAL:
		return "ESUP";
        case GDA_QUERY_CONDITION_LEAF_INF:
		return "INF";
        case GDA_QUERY_CONDITION_LEAF_INFEQUAL:
		return "EINF";
        case GDA_QUERY_CONDITION_LEAF_LIKE:
		return "LIKE";
	case GDA_QUERY_CONDITION_LEAF_SIMILAR:
		return "SIMI";
        case GDA_QUERY_CONDITION_LEAF_REGEX:
		return "REG";
        case GDA_QUERY_CONDITION_LEAF_REGEX_NOCASE:
		return "CREG";
        case GDA_QUERY_CONDITION_LEAF_NOT_REGEX:
		return "NREG";
        case GDA_QUERY_CONDITION_LEAF_NOT_REGEX_NOCASE:
		return "CNREG";
        case GDA_QUERY_CONDITION_LEAF_IN:
		return "IN";
        case GDA_QUERY_CONDITION_LEAF_BETWEEN:
		return "BTW";
	default:
		return "???";
	}
}

static GdaQueryConditionType
condition_str_to_type (const gchar *type)
{
	switch (*type) {
	case 'A':
		return GDA_QUERY_CONDITION_NODE_AND;
	case 'O':
		return GDA_QUERY_CONDITION_NODE_OR;
	case 'N':
		if (!strcmp (type, "NOT"))
			return GDA_QUERY_CONDITION_NODE_NOT;
		else {
			if (!strcmp (type, "NE"))
				return GDA_QUERY_CONDITION_LEAF_DIFF;
			else
				return GDA_QUERY_CONDITION_LEAF_NOT_REGEX;
		}
	case 'E':
		if (!strcmp (type, "EQ"))
			return GDA_QUERY_CONDITION_LEAF_EQUAL;
		else if (!strcmp (type, "ESUP"))
			return GDA_QUERY_CONDITION_LEAF_SUPEQUAL;
		else
			return GDA_QUERY_CONDITION_LEAF_INFEQUAL;
	case 'S':
		if (type[1] == 'I')
			return GDA_QUERY_CONDITION_LEAF_SIMILAR;
		else
			return GDA_QUERY_CONDITION_LEAF_SUP;

	case 'I':
		if (!strcmp (type, "INF"))
			return GDA_QUERY_CONDITION_LEAF_INF;
		else
			return GDA_QUERY_CONDITION_LEAF_IN;
	case 'L':
		return GDA_QUERY_CONDITION_LEAF_LIKE;
	case 'R':
		return GDA_QUERY_CONDITION_LEAF_REGEX;
	case 'C':
		switch (type[1]) {
		case 'R':
			return GDA_QUERY_CONDITION_LEAF_REGEX_NOCASE;
		case 'N':
			return GDA_QUERY_CONDITION_LEAF_NOT_REGEX_NOCASE;
		default:
			return GDA_QUERY_CONDITION_TYPE_UNKNOWN;
		}
	case 'B':
		return GDA_QUERY_CONDITION_LEAF_BETWEEN;
	default:
		return GDA_QUERY_CONDITION_TYPE_UNKNOWN;
	}
}

static xmlNodePtr
gda_query_condition_save_to_xml (GdaXmlStorage *iface, GError **error)
{
        xmlNodePtr node = NULL;
	GdaQueryCondition *cond;
        gchar *str;
	GdaObject *base;
	GSList *list;

        g_return_val_if_fail (iface && GDA_IS_QUERY_CONDITION (iface), NULL);
        g_return_val_if_fail (GDA_QUERY_CONDITION (iface)->priv, NULL);

        cond = GDA_QUERY_CONDITION (iface);

        node = xmlNewNode (NULL, (xmlChar*)"gda_query_cond");

        str = gda_xml_storage_get_xml_id (iface);
        xmlSetProp(node, (xmlChar*)"id", (xmlChar*)str);
        g_free (str);

        xmlSetProp(node, (xmlChar*)"type", (xmlChar*)condition_type_to_str (cond->priv->type));

	base = gda_object_ref_get_ref_object (cond->priv->ops[GDA_QUERY_CONDITION_OP_LEFT]);
	if (base) {
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (base));
		xmlSetProp(node, (xmlChar*)"l_op", (xmlChar*)str);
		g_free (str);
	}

	base = gda_object_ref_get_ref_object (cond->priv->ops[GDA_QUERY_CONDITION_OP_RIGHT]);
	if (base) {
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (base));
		xmlSetProp(node, (xmlChar*)"r_op", (xmlChar*)str);
		g_free (str);
	}

	base = gda_object_ref_get_ref_object (cond->priv->ops[GDA_QUERY_CONDITION_OP_RIGHT2]);
	if (base) {
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (base));
		xmlSetProp(node, (xmlChar*)"r_op2", (xmlChar*)str);
		g_free (str);
	}

	/* sub conditions */
	list = cond->priv->cond_children;
	while (list) {
		xmlNodePtr sub = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
		if (sub)
                        xmlAddChild (node, sub);
                else {
                        xmlFreeNode (node);
                        return NULL;
                }
		list = g_slist_next (list);
	}

        return node;
}

static gboolean
gda_query_condition_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaQueryCondition *cond;
        gchar *prop;
        gboolean id = FALSE;
	xmlNodePtr children;

        g_return_val_if_fail (iface && GDA_IS_QUERY_CONDITION (iface), FALSE);
        g_return_val_if_fail (GDA_QUERY_CONDITION (iface)->priv, FALSE);
        g_return_val_if_fail (node, FALSE);

	cond = GDA_QUERY_CONDITION (iface);

	if (strcmp ((gchar*)node->name, "gda_query_cond")) {
                g_set_error (error,
                             GDA_QUERY_CONDITION_ERROR,
                             GDA_QUERY_CONDITION_XML_LOAD_ERROR,
                             _("XML Tag is not <gda_query_cond>"));
                return FALSE;
        }

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"id");
        if (prop) {
                gchar *ptr, *tok;
                ptr = strtok_r (prop, ":", &tok);
                ptr = strtok_r (NULL, ":", &tok);
                if (strlen (ptr) < 2) {
                        g_set_error (error,
                                     GDA_QUERY_CONDITION_ERROR,
                                     GDA_QUERY_CONDITION_XML_LOAD_ERROR,
                                     _("Wrong 'id' attribute in <gda_query_cond>"));
                        return FALSE;
                }
                gda_query_object_set_int_id (GDA_QUERY_OBJECT (cond), atoi (ptr+1));
		id = TRUE;
                g_free (prop);
        }

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"type");
        if (prop) {
		cond->priv->type = condition_str_to_type (prop);
		if (cond->priv->type == GDA_QUERY_CONDITION_TYPE_UNKNOWN) {
			g_set_error (error,
                                     GDA_QUERY_CONDITION_ERROR,
                                     GDA_QUERY_CONDITION_XML_LOAD_ERROR,
                                     _("Wrong 'type' attribute in <gda_query_cond>"));
                        return FALSE;
		}
                g_free (prop);
        }

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"l_op");
	if (prop) {
		gda_object_ref_set_ref_name (cond->priv->ops[GDA_QUERY_CONDITION_OP_LEFT], GDA_TYPE_QUERY_FIELD,
					  REFERENCE_BY_XML_ID, prop);
		g_free (prop);
	}


	prop = (gchar*)xmlGetProp(node, (xmlChar*)"r_op");
	if (prop) {
		gda_object_ref_set_ref_name (cond->priv->ops[GDA_QUERY_CONDITION_OP_RIGHT], GDA_TYPE_QUERY_FIELD,
					  REFERENCE_BY_XML_ID, prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"r_op2");
	if (prop) {
		gda_object_ref_set_ref_name (cond->priv->ops[GDA_QUERY_CONDITION_OP_RIGHT2], GDA_TYPE_QUERY_FIELD,
					  REFERENCE_BY_XML_ID, prop);
		g_free (prop);
	}

	/* children nodes */
	children = node->children;
	while (children) {
		if (!strcmp ((gchar*)children->name, "gda_query_cond")) {
			GdaQueryCondition *scond;

			scond = gda_query_condition_new (cond->priv->query, GDA_QUERY_CONDITION_NODE_AND);
			if (gda_xml_storage_load_from_xml (GDA_XML_STORAGE (scond), children, error)) {
				gda_query_condition_node_add_child (cond, scond, NULL);
				g_object_unref (G_OBJECT (scond));
			}
			else
				return FALSE;
                }

		children = children->next;
	}

	if (!id) {
		g_set_error (error,
			     GDA_QUERY_CONDITION_ERROR,
			     GDA_QUERY_CONDITION_XML_LOAD_ERROR,
			     _("Missing ID attribute in <gda_query_cond>"));
		return FALSE;
        }

        return TRUE;
}


/*
 * GdaRenderer interface implementation
 */

static gchar *
gda_query_condition_render_as_sql (GdaRenderer *iface, GdaParameterList *context, 
				   GSList **out_params_used, GdaRendererOptions options, GError **error)
{
        gchar *retval = NULL, *str;
	GString *string;
        GdaQueryCondition *cond;
	gboolean is_node = FALSE;
	gchar *link = NULL;
	GdaObject *ops[3];
	gint i;
	gboolean pprint = options & GDA_RENDERER_EXTRA_PRETTY_SQL;

        g_return_val_if_fail (iface && GDA_IS_QUERY_CONDITION (iface), NULL);
        g_return_val_if_fail (GDA_QUERY_CONDITION (iface)->priv, NULL);
        cond = GDA_QUERY_CONDITION (iface);
	if (!gda_query_condition_activate (GDA_REFERER (cond))) {
		g_set_error (error,
			     GDA_QUERY_CONDITION_ERROR,
			     GDA_QUERY_CONDITION_RENDERER_ERROR,
			     _("Condition is not active"));
		return NULL;
	}

	for (i=0; i<3; i++)
		ops[i] = gda_object_ref_get_ref_object (cond->priv->ops[i]);

	/* testing for completeness */
	switch (cond->priv->type) {
	case GDA_QUERY_CONDITION_NODE_NOT:
		if (g_slist_length (cond->priv->cond_children) != 1) {
			g_set_error (error,
				     GDA_QUERY_CONDITION_ERROR,
				     GDA_QUERY_CONDITION_RENDERER_ERROR,
				     _("Condition operator 'NOT' must have one argument"));
			return NULL;
		}
		break;
	case GDA_QUERY_CONDITION_NODE_AND:
	case GDA_QUERY_CONDITION_NODE_OR:
		if (g_slist_length (cond->priv->cond_children) == 0) {
			g_set_error (error,
				     GDA_QUERY_CONDITION_ERROR,
				     GDA_QUERY_CONDITION_RENDERER_ERROR,
				     _("Condition must have at least one argument"));
			return NULL;
		}
		break;
	case GDA_QUERY_CONDITION_LEAF_EQUAL:
	case GDA_QUERY_CONDITION_LEAF_DIFF:
	case GDA_QUERY_CONDITION_LEAF_SUP:
        case GDA_QUERY_CONDITION_LEAF_SUPEQUAL:
        case GDA_QUERY_CONDITION_LEAF_INF:
        case GDA_QUERY_CONDITION_LEAF_INFEQUAL:
        case GDA_QUERY_CONDITION_LEAF_LIKE:
	case GDA_QUERY_CONDITION_LEAF_SIMILAR:
        case GDA_QUERY_CONDITION_LEAF_REGEX:
        case GDA_QUERY_CONDITION_LEAF_REGEX_NOCASE:
        case GDA_QUERY_CONDITION_LEAF_NOT_REGEX:
        case GDA_QUERY_CONDITION_LEAF_NOT_REGEX_NOCASE:
	case GDA_QUERY_CONDITION_LEAF_IN:
		if (!ops[GDA_QUERY_CONDITION_OP_LEFT] || !ops[GDA_QUERY_CONDITION_OP_RIGHT]) {
			g_set_error (error,
				     GDA_QUERY_CONDITION_ERROR,
				     GDA_QUERY_CONDITION_RENDERER_ERROR,
				     _("Condition must have two arguments"));
			return NULL;
		}
		break;
        case GDA_QUERY_CONDITION_LEAF_BETWEEN:
		if (!ops[GDA_QUERY_CONDITION_OP_LEFT] || !ops[GDA_QUERY_CONDITION_OP_RIGHT] ||
		    !ops[GDA_QUERY_CONDITION_OP_RIGHT2]) {
			g_set_error (error,
				     GDA_QUERY_CONDITION_ERROR,
				     GDA_QUERY_CONDITION_RENDERER_ERROR,
				     _("Condition 'BETWEEN' must have three arguments"));
			return NULL;
		}
	default:
		break;
	}

	/* actual rendering */
	string = g_string_new ("");
	switch (cond->priv->type) {
	case GDA_QUERY_CONDITION_NODE_AND:
		is_node = TRUE;
		link = " AND ";
		break;
	case GDA_QUERY_CONDITION_NODE_OR:
		is_node = TRUE;
		link = " OR ";
		break;
	case GDA_QUERY_CONDITION_NODE_NOT:
		is_node = TRUE;
		link = "NOT ";
		break;
	case GDA_QUERY_CONDITION_LEAF_EQUAL:
		link = "=";
		if (GDA_IS_QUERY_FIELD_VALUE (ops[GDA_QUERY_CONDITION_OP_RIGHT]) &&
		    context && 
		    gda_query_field_value_is_value_null (GDA_QUERY_FIELD_VALUE (ops[GDA_QUERY_CONDITION_OP_RIGHT]), context))
				link = " IS ";
		break;
	case GDA_QUERY_CONDITION_LEAF_DIFF:
		link = "!=";
		if (GDA_IS_QUERY_FIELD_VALUE (ops[GDA_QUERY_CONDITION_OP_RIGHT]) &&
		    context && 
		    gda_query_field_value_is_value_null (GDA_QUERY_FIELD_VALUE (ops[GDA_QUERY_CONDITION_OP_RIGHT]), context))
			link = " IS NOT ";
		break;
	case GDA_QUERY_CONDITION_LEAF_SUP:
		link = " > ";
		break;
        case GDA_QUERY_CONDITION_LEAF_SUPEQUAL:
		link = " >= ";
		break;
        case GDA_QUERY_CONDITION_LEAF_INF:
		link = " < ";
		break;
        case GDA_QUERY_CONDITION_LEAF_INFEQUAL:
		link = " <= ";
		break;
        case GDA_QUERY_CONDITION_LEAF_LIKE:
		link = " LIKE ";
		break;
        case GDA_QUERY_CONDITION_LEAF_SIMILAR:
		link = " SIMILAR TO ";
		break;
        case GDA_QUERY_CONDITION_LEAF_REGEX:
		link = " ~ ";
		break;
        case GDA_QUERY_CONDITION_LEAF_REGEX_NOCASE:
		link = " ~* ";
		break;
        case GDA_QUERY_CONDITION_LEAF_NOT_REGEX:
		link = " !~ ";
		break;
        case GDA_QUERY_CONDITION_LEAF_NOT_REGEX_NOCASE:
		link = " !~* ";
		break;
	case GDA_QUERY_CONDITION_LEAF_IN:
		link = " IN ";
		break;
        case GDA_QUERY_CONDITION_LEAF_BETWEEN:
		link = " BETWEEN ";
		break;
	default:
		break;
	}

	if (link) {
		if (is_node) {
			if (cond->priv->type != GDA_QUERY_CONDITION_NODE_NOT) { /* type = AND or OR */
				gboolean first = TRUE;
				GSList *list;

				list = cond->priv->cond_children;
				while (list) {
					if (first) 
						first = FALSE;
					else {
						if (pprint && ! cond->priv->join)
							g_string_append (string, "\n\t");
						g_string_append_printf (string, "%s", link);
					}

					if ((cond->priv->type == GDA_QUERY_CONDITION_NODE_AND) &&
					    (GDA_QUERY_CONDITION (list->data)->priv->type == GDA_QUERY_CONDITION_NODE_OR))
						g_string_append (string, "(");

					str = gda_renderer_render_as_sql (GDA_RENDERER (list->data), context, 
									  out_params_used, options, error);
					if (!str) {
						g_string_free (string, TRUE);
						return NULL;
					}
					g_string_append (string, str);
					g_free (str);

					if ((cond->priv->type == GDA_QUERY_CONDITION_NODE_AND) &&
					    (GDA_QUERY_CONDITION (list->data)->priv->type == GDA_QUERY_CONDITION_NODE_OR))
						g_string_append (string, ")");
					list = g_slist_next (list);
				}
			}
			else { /* type = NOT */
				g_string_append_printf (string, "%s", link);
				str = gda_renderer_render_as_sql (GDA_RENDERER (cond->priv->cond_children->data), 
								  context, out_params_used, options, error);
				if (!str) {
					g_string_free (string, TRUE);
					return NULL;
				}
				g_string_append (string, "(");
				g_string_append (string, str);
				g_string_append (string, ")");
				g_free (str);
			}
		}
		else {
			str = gda_renderer_render_as_sql (GDA_RENDERER (ops[GDA_QUERY_CONDITION_OP_LEFT]), context, 
							  out_params_used, options, error);
			if (!str) {
				g_string_free (string, TRUE);
				return NULL;
			}
			g_string_append (string, str);
			g_free (str);
			g_string_append_printf (string, "%s", link);
			str = gda_renderer_render_as_sql (GDA_RENDERER (ops[GDA_QUERY_CONDITION_OP_RIGHT]), context, 
							  out_params_used, options, error);
			if (!str) {
				g_string_free (string, TRUE);
				return NULL;
			}
			g_string_append (string, str);
			g_free (str);
		}
	}

	if (cond->priv->type == GDA_QUERY_CONDITION_LEAF_BETWEEN) {
		str = gda_renderer_render_as_sql (GDA_RENDERER (ops[GDA_QUERY_CONDITION_OP_RIGHT2]), context, 
						  out_params_used, options, error);
		if (!str) {
			g_string_free (string, TRUE);
			return NULL;
		}
		g_string_append_printf (string, " AND %s", str);
		g_free (str);
	}

	retval = string->str;
	g_string_free (string, FALSE);

        return retval;
}

static gchar *
gda_query_condition_render_as_str (GdaRenderer *iface, GdaParameterList *context)
{
        gchar *str = NULL;

        g_return_val_if_fail (iface && GDA_IS_QUERY_CONDITION (iface), NULL);
        g_return_val_if_fail (GDA_QUERY_CONDITION (iface)->priv, NULL);

        str = gda_query_condition_render_as_sql (iface, context, NULL, 0, NULL);
        if (!str)
                str = g_strdup ("???");
        return str;
}


/*
 * GdaReferer interface implementation
 */
static gboolean
gda_query_condition_activate (GdaReferer *iface)
{
	gboolean activated = TRUE;
	gint i;
	GSList *list;
	GdaQueryCondition *cond;

        g_return_val_if_fail (iface && GDA_IS_QUERY_CONDITION (iface), FALSE);
        g_return_val_if_fail (GDA_QUERY_CONDITION (iface)->priv, FALSE);
	cond = GDA_QUERY_CONDITION (iface);

	for (i=0; i<3; i++) {
		if (!gda_object_ref_activate (cond->priv->ops[i]))
			activated = FALSE;
	}
	list = cond->priv->cond_children;
	while (list) {
		if (!gda_query_condition_activate (GDA_REFERER (list->data)))
			activated = FALSE;

		list = g_slist_next (list);
	}

	return activated;
}

static void
gda_query_condition_deactivate (GdaReferer *iface)
{
	gint i;
	GSList *list;
	GdaQueryCondition *cond;

        g_return_if_fail (iface && GDA_IS_QUERY_CONDITION (iface));
        g_return_if_fail (GDA_QUERY_CONDITION (iface)->priv);
	cond = GDA_QUERY_CONDITION (iface);

	for (i=0; i<3; i++)
		gda_object_ref_deactivate (cond->priv->ops[i]);
	list = cond->priv->cond_children;
	while (list) {
		gda_query_condition_deactivate (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}
}

static gboolean
gda_query_condition_is_active (GdaReferer *iface)
{
	gboolean activated = TRUE;
	gint i;
	GSList *list;
	GdaQueryCondition *cond;

        g_return_val_if_fail (iface && GDA_IS_QUERY_CONDITION (iface), FALSE);
        g_return_val_if_fail (GDA_QUERY_CONDITION (iface)->priv, FALSE);
	cond = GDA_QUERY_CONDITION (iface);

	for (i=0; i<3; i++) {
		if (!gda_object_ref_is_active (cond->priv->ops[i]))
			activated = FALSE;
	}
	list = cond->priv->cond_children;
	while (list && activated) {
		activated = gda_query_condition_is_active (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	return activated;
}

static GSList *
gda_query_condition_get_ref_objects (GdaReferer *iface)
{
        GSList *list = NULL;
	gint i;
	
        g_return_val_if_fail (iface && GDA_IS_QUERY_CONDITION (iface), NULL);
        g_return_val_if_fail (GDA_QUERY_CONDITION (iface)->priv, NULL);

	for (i=0; i<3; i++) {
                GdaObject *base = gda_object_ref_get_ref_object (GDA_QUERY_CONDITION (iface)->priv->ops[i]);
                if (base)
                        list = g_slist_append (list, base);
        }

        return list;
}

/**
 * gda_query_condition_get_ref_objects_all
 * @cond: a #GdaQueryCondition object
 *
 * Get a complete list of the objects referenced by @cond, 
 * including its descendants (unlike the gda_referer_get_ref_objects()
 * function applied to @cond).
 *
 * Returns: a new list of referenced objects
 */
GSList *
gda_query_condition_get_ref_objects_all (GdaQueryCondition *cond)
{
        GSList *list = NULL, *children;
	gint i;
	
        g_return_val_if_fail (cond && GDA_IS_QUERY_CONDITION (cond), NULL);
        g_return_val_if_fail (cond->priv, NULL);

	for (i=0; i<3; i++) {
		if (cond->priv->ops[i]) {
			GdaObject *base = gda_object_ref_get_ref_object (cond->priv->ops[i]);
			if (base)
				list = g_slist_append (list, base);
		}
        }

	children = cond->priv->cond_children;
	while (children) {
		GSList *clist = gda_query_condition_get_ref_objects_all (GDA_QUERY_CONDITION (children->data));
		if (clist)
			list = g_slist_concat (list, clist);
		children = g_slist_next (children);
	}

        return list;
}

static void
gda_query_condition_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	gint i;
	GdaQueryCondition *cond;
	GSList *list;

        g_return_if_fail (iface && GDA_IS_QUERY_CONDITION (iface));
        g_return_if_fail (GDA_QUERY_CONDITION (iface)->priv);

        cond = GDA_QUERY_CONDITION (iface);
        if (cond->priv->query) {
                GdaQuery *query = g_hash_table_lookup (replacements, cond->priv->query);
                if (query) {
			gda_query_undeclare_condition (cond->priv->query, cond);
                        g_signal_handlers_disconnect_by_func (G_OBJECT (cond->priv->query),
                                                              G_CALLBACK (destroyed_object_cb), cond);
                        cond->priv->query = query;
			gda_object_connect_destroy (query, G_CALLBACK (destroyed_object_cb), cond);
			gda_query_declare_condition (query, cond);
                }
        }

	if (cond->priv->join) {
                GdaQueryJoin *join = g_hash_table_lookup (replacements, cond->priv->join);
                if (join) {
                        g_signal_handlers_disconnect_by_func (G_OBJECT (cond->priv->join),
                                                              G_CALLBACK (destroyed_object_cb), cond);
                        cond->priv->join = join;
			gda_object_connect_destroy (join, G_CALLBACK (destroyed_object_cb), cond);
                }
        }

	/* references in operators */
	for (i=0; i<3; i++)
                gda_object_ref_replace_ref_object (cond->priv->ops[i], replacements);

	/* references in children's list */
	list = cond->priv->cond_children;
	while (list) {
		GdaQueryCondition *sub = g_hash_table_lookup (replacements, list->data);
		if (sub) {
			i = g_slist_position (cond->priv->cond_children, list);
			cond->priv->internal_transaction ++;
			gda_query_condition_node_del_child (cond, GDA_QUERY_CONDITION (list->data));
			cond->priv->internal_transaction --;
			gda_query_condition_node_add_child_pos (cond, sub, i, NULL);
		}
		else
			list = g_slist_next (list);
	}

	/* references in children themselves */
	list = cond->priv->cond_children;
	while (list) {
		gda_query_condition_replace_refs (GDA_REFERER (list->data), replacements);
		list = g_slist_next (list);
	}
}
