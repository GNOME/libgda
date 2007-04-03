/* gda-query-join.c
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-query-join.h"
#include "gda-dict-database.h"
#include "gda-query.h"
#include "gda-dict-table.h"
#include "gda-dict-constraint.h"
#include "gda-entity.h"
#include "gda-query-field.h"
#include "gda-query-field-field.h"
#include "gda-entity-field.h"
#include "gda-object-ref.h"
#include "gda-query-target.h"
#include "gda-xml-storage.h"
#include "gda-referer.h"
#include "gda-marshal.h"
#include "gda-query-condition.h"
#include "gda-query-parsing.h"

/* 
 * Main static functions 
 */
static void gda_query_join_class_init (GdaQueryJoinClass * class);
static void gda_query_join_init (GdaQueryJoin * srv);
static void gda_query_join_dispose (GObject   * object);
static void gda_query_join_finalize (GObject   * object);

static void gda_query_join_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_query_join_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);

/* XML storage interface */
static void        gda_query_join_xml_storage_init (GdaXmlStorageIface *iface);
static xmlNodePtr  gda_query_join_save_to_xml      (GdaXmlStorage *iface, GError **error);
static gboolean    gda_query_join_load_from_xml    (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* Referer interface */
static void        gda_query_join_referer_init        (GdaRefererIface *iface);
static gboolean    gda_query_join_activate            (GdaReferer *iface);
static void        gda_query_join_deactivate          (GdaReferer *iface);
static gboolean    gda_query_join_is_active           (GdaReferer *iface);
static GSList     *gda_query_join_get_ref_objects     (GdaReferer *iface);
static void        gda_query_join_replace_refs        (GdaReferer *iface, GHashTable *replacements);

/* When the Query or any of the refering GdaQueryTarget is destroyed */
static void        destroyed_object_cb (GObject *obj, GdaQueryJoin *join);
static void        target_ref_lost_cb (GdaObjectRef *ref, GdaQueryJoin *join);
static void        target_removed_cb (GdaQuery *query, GdaQueryTarget *target, GdaQueryJoin *join);

static void        gda_query_join_set_int_id (GdaQueryObject *join, guint id);
#ifdef GDA_DEBUG
static void        gda_query_join_dump (GdaQueryJoin *join, guint offset);
#endif

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	TYPE_CHANGED,
	CONDITION_CHANGED,
	LAST_SIGNAL
};

static gint gda_query_join_signals[LAST_SIGNAL] = { 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_QUERY,
	PROP_TARGET1_OBJ,
	PROP_TARGET1_ID,
	PROP_TARGET2_OBJ,
	PROP_TARGET2_ID,
};


/* private structure */
struct _GdaQueryJoinPrivate
{
	GdaQueryJoinType   join_type;
	GdaQuery          *query;
	GdaObjectRef      *target1;
	GdaObjectRef      *target2;
	GdaQueryCondition *cond;
};



/* module error */
GQuark gda_query_join_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_query_join_error");
	return quark;
}


GType
gda_query_join_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryJoinClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_join_class_init,
			NULL,
			NULL,
			sizeof (GdaQueryJoin),
			0,
			(GInstanceInitFunc) gda_query_join_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_query_join_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo referer_info = {
			(GInterfaceInitFunc) gda_query_join_referer_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GDA_TYPE_QUERY_OBJECT, "GdaQueryJoin", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
	}
	return type;
}

static void 
gda_query_join_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = NULL;
	iface->save_to_xml = gda_query_join_save_to_xml;
	iface->load_from_xml = gda_query_join_load_from_xml;
}

static void
gda_query_join_referer_init (GdaRefererIface *iface)
{
	iface->activate = gda_query_join_activate;
	iface->deactivate = gda_query_join_deactivate;
	iface->is_active = gda_query_join_is_active;
	iface->get_ref_objects = gda_query_join_get_ref_objects;
	iface->replace_refs = gda_query_join_replace_refs;
}


static void
gda_query_join_class_init (GdaQueryJoinClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_query_join_signals[TYPE_CHANGED] =
		g_signal_new ("type_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryJoinClass, type_changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	gda_query_join_signals[CONDITION_CHANGED] =
		g_signal_new ("condition_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryJoinClass, condition_changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	class->type_changed = NULL;
	class->condition_changed = NULL;

	object_class->dispose = gda_query_join_dispose;
	object_class->finalize = gda_query_join_finalize;

	/* Properties */
	object_class->set_property = gda_query_join_set_property;
	object_class->get_property = gda_query_join_get_property;

	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_object ("query", "Query to which the field belongs", NULL, 
                                                               GDA_TYPE_QUERY,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE |
								G_PARAM_CONSTRUCT_ONLY)));

	g_object_class_install_property (object_class, PROP_TARGET1_OBJ,
					 g_param_spec_object ("target1", "A pointer to a GdaQueryTarget", NULL, 
                                                               GDA_TYPE_QUERY_TARGET,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_TARGET1_ID,
					 g_param_spec_string ("target1_id", "XML ID of a target", NULL, NULL,
							      G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_TARGET2_OBJ,
					 g_param_spec_object ("target2", "A pointer to a GdaQueryTarget", NULL, 
                                                               GDA_TYPE_QUERY_TARGET,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_TARGET2_ID,
					 g_param_spec_string ("target2_id", "XML ID of a target", NULL, NULL,
							      G_PARAM_WRITABLE));

	/* virtual functions */
	GDA_QUERY_OBJECT_CLASS (class)->set_int_id = (void (*)(GdaQueryObject *, guint)) gda_query_join_set_int_id;	
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_query_join_dump;
#endif

}

static void
gda_query_join_init (GdaQueryJoin * join)
{
	join->priv = g_new0 (GdaQueryJoinPrivate, 1);
	join->priv->join_type = GDA_QUERY_JOIN_TYPE_INNER;
	join->priv->query = NULL;
	join->priv->target1 = NULL;
	join->priv->target2 = NULL;
	join->priv->cond = NULL;
}

/**
 * gda_query_join_new_with_targets
 * @query: a #GdaQuery object in which the join will occur
 * @target_1: the 1st #GdaQueryTarget object participating in the join
 * @target_2: the 2nd #GdaQueryTarget object participating in the join
 *
 * Creates a new GdaQueryJoin object. Note: the #GdaQueryTarget ranks (1st and 2nd) does not matter, but
 * is necessary since the join may not be symetrical (LEFT or RIGHT join). Also, the #GdaQueryJoin object
 * may decide to swap the two if necessary.
 *
 * Returns: the new object
 */
GdaQueryJoin*
gda_query_join_new_with_targets (GdaQuery *query, GdaQueryTarget *target_1, GdaQueryTarget *target_2)
{
	GObject *obj;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (target_1 && GDA_IS_QUERY_TARGET (target_1), NULL);
	g_return_val_if_fail (target_2 && GDA_IS_QUERY_TARGET (target_2), NULL);
	g_return_val_if_fail (gda_query_target_get_query (target_1) == query, NULL);
	g_return_val_if_fail (gda_query_target_get_query (target_2) == query, NULL);
	g_return_val_if_fail (target_1 != target_2, NULL);

	obj = g_object_new (GDA_TYPE_QUERY_JOIN,
			    "dict", gda_object_get_dict (GDA_OBJECT (query)), 
			    "query", query,
			    "target1", target_1,
			    "target2", target_2, NULL);
	
	return (GdaQueryJoin*) obj;
}

/**
 * gda_query_join_new_with_xml_ids
 * @query: a #GdaQuery object in which the join will occur
 * @target_1_xml_id: the 1st #GdaQueryTarget object's XML id participating in the join
 * @target_2_xml_id: the 2nd #GdaQueryTarget object's XML id participating in the join
 *
 * Creates a new GdaQueryJoin object. Note: the #GdaQueryTarget ranks (1st and 2nd) does not matter, but
 * is necessary since the join may not be symetrical (LEFT or RIGHT join). Also, the #GdaQueryJoin object
 * may decide to swap the two if necessary.
 *
 * Returns: the new object
 */
GdaQueryJoin *
gda_query_join_new_with_xml_ids (GdaQuery *query, const gchar *target_1_xml_id, const gchar *target_2_xml_id)
{
	GObject *obj;
	gchar *qid, *ptr, *tok;
	gchar *tid;
	
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (target_1_xml_id && *target_1_xml_id, NULL);
	g_return_val_if_fail (target_2_xml_id && *target_2_xml_id, NULL);
	g_return_val_if_fail (strcmp (target_1_xml_id, target_2_xml_id), NULL);

	/* check that the XML Ids start with the query's XML Id */
	qid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (query));
	tid = g_strdup (target_1_xml_id);
	ptr = strtok_r (tid, ":", &tok);
	g_return_val_if_fail (!strcmp (ptr, qid), NULL);
	g_free (tid);
	tid = g_strdup (target_2_xml_id);
	ptr = strtok_r (tid, ":", &tok);
	g_return_val_if_fail (!strcmp (ptr, qid), NULL);
	g_free (tid);
	g_free (qid);

	obj = g_object_new (GDA_TYPE_QUERY_JOIN,
			    "dict", gda_object_get_dict (GDA_OBJECT (query)), 
			    "query", query,
			    "target1_id", target_1_xml_id,
			    "target2_id", target_2_xml_id, NULL);
	return (GdaQueryJoin*) obj;
}

/**
 * gda_query_join_new_copy
 * @orig: a #GdaQueryJoin to make a copy of
 * @replacements: a hash table to store replacements, or %NULL
 * 
 * Copy constructor
 *
 * Returns: a the new copy of @orig
 */
GdaQueryJoin *
gda_query_join_new_copy (GdaQueryJoin *orig, GHashTable *replacements)
{
	GObject *obj;
	GdaQueryJoin *join;
	GdaDict *dict;
	GdaQuery *query;
	GdaObject *ref;

	g_return_val_if_fail (orig && GDA_IS_QUERY_JOIN (orig), NULL);

	g_object_get (G_OBJECT (orig), "query", &query, NULL);
	g_return_val_if_fail (query, NULL);

	dict = gda_object_get_dict (GDA_OBJECT (orig));
	obj = g_object_new (GDA_TYPE_QUERY_JOIN, "dict", dict, 
			    "query", query, NULL);
	g_object_unref (query);
	join = GDA_QUERY_JOIN (obj);
	if (replacements)
		g_hash_table_insert (replacements, orig, join);

	ref = gda_object_ref_get_ref_object (orig->priv->target1);
	if (ref)
		gda_object_ref_set_ref_object (join->priv->target1, ref);
	else {
		const gchar *ref_str;
		GdaObjectRefType ref_type;
		GType ref_gtype;

		ref_str = gda_object_ref_get_ref_name (orig->priv->target1, &ref_gtype, &ref_type);
		if (ref_str)
			gda_object_ref_set_ref_name (join->priv->target1, ref_gtype, ref_type, ref_str);
	}

	ref = gda_object_ref_get_ref_object (orig->priv->target2);
	if (ref)
		gda_object_ref_set_ref_object (join->priv->target2, ref);
	else {
		const gchar *ref_str;
		GdaObjectRefType ref_type;
		GType ref_gtype;

		ref_str = gda_object_ref_get_ref_name (orig->priv->target2, &ref_gtype, &ref_type);
		if (ref_str)
			gda_object_ref_set_ref_name (join->priv->target2, ref_gtype, ref_type, ref_str);
	}

	join->priv->join_type = orig->priv->join_type;

	/* join condition */
	if (orig->priv->cond) {
		GdaQueryCondition *cond = gda_query_condition_new_copy (orig->priv->cond, replacements);
		gda_query_join_set_condition (join, cond);
		g_object_unref (G_OBJECT (cond));
		if (replacements)
			g_hash_table_insert (replacements, orig->priv->cond, cond);
	}

	return join;	
}

static void
destroyed_object_cb (GObject *obj, GdaQueryJoin *join)
{
	gda_object_destroy (GDA_OBJECT (join));
}

static void
target_ref_lost_cb (GdaObjectRef *ref, GdaQueryJoin *join)
{
	gda_object_destroy (GDA_OBJECT (join));
}

static void
target_removed_cb (GdaQuery *query, GdaQueryTarget *target, GdaQueryJoin *join)
{
	GdaObject *t;
	gboolean to_destroy = FALSE;

	t = gda_object_ref_get_ref_object (join->priv->target1);
	if (t == (GdaObject *) target)
		to_destroy = TRUE;
	if (! to_destroy) {
		t = gda_object_ref_get_ref_object (join->priv->target2);
		if (t == (GdaObject *) target)
			to_destroy = TRUE;
	}

	if (to_destroy)
		gda_object_destroy (GDA_OBJECT (join));
}


static void
cond_changed_cb (GdaQueryCondition *cond, GdaQueryJoin *join)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'CONDITION_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (join), "condition_changed");
#ifdef GDA_DEBUG_signal
	g_print ("<< 'CONDITION_CHANGED' from %s\n", __FUNCTION__);
#endif
	gda_object_signal_emit_changed (GDA_OBJECT (join));
}

static void
destroyed_cond_cb (GdaQueryCondition *cond, GdaQueryJoin *join) 
{
	g_assert (cond == join->priv->cond);
	g_signal_handlers_disconnect_by_func (G_OBJECT (join->priv->cond),
					      G_CALLBACK (destroyed_cond_cb), join);
	g_signal_handlers_disconnect_by_func (G_OBJECT (join->priv->cond),
					      G_CALLBACK (cond_changed_cb), join);
	g_object_set (G_OBJECT (cond), "join", NULL, NULL);
	g_object_unref (join->priv->cond);
	join->priv->cond = NULL;
}

static void
gda_query_join_dispose (GObject *object)
{
	GdaQueryJoin *join;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_JOIN (object));

	join = GDA_QUERY_JOIN (object);
	if (join->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));
		
		if (join->priv->query) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (join->priv->query),
							      G_CALLBACK (destroyed_object_cb), join);
			g_signal_handlers_disconnect_by_func (G_OBJECT (join->priv->query),
							      G_CALLBACK (target_removed_cb), join);
			join->priv->query = NULL;
		}
		if (join->priv->target1) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (join->priv->target1),
							      G_CALLBACK (target_ref_lost_cb), join);
			g_object_unref (G_OBJECT (join->priv->target1));
			join->priv->target1 = NULL;
		}
		if (join->priv->target2) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (join->priv->target2),
							      G_CALLBACK (target_ref_lost_cb), join);
			g_object_unref (G_OBJECT (join->priv->target2));
			join->priv->target2 = NULL;
		}
		if (join->priv->cond)
			destroyed_cond_cb (join->priv->cond, join);
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_query_join_finalize (GObject   * object)
{
	GdaQueryJoin *join;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_JOIN (object));

	join = GDA_QUERY_JOIN (object);
	if (join->priv) {
		g_free (join->priv);
		join->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_query_join_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaQueryJoin *join;
	GdaDict *dict;
	const gchar *str;
	
	join = GDA_QUERY_JOIN (object);
	if (join->priv) {
		switch (param_id) {
		case PROP_QUERY: {
			GdaQuery* ptr = GDA_QUERY (g_value_get_object (value));
			g_return_if_fail (GDA_IS_QUERY (ptr));
			dict = gda_object_get_dict (GDA_OBJECT (join));
			g_return_if_fail (gda_object_get_dict (GDA_OBJECT (ptr)) == dict);

			if (join->priv->query) {
				if (join->priv->query == GDA_QUERY (ptr))
					return;

				g_signal_handlers_disconnect_by_func (G_OBJECT (join->priv->query),
								      G_CALLBACK (destroyed_object_cb), join);
				g_signal_handlers_disconnect_by_func (G_OBJECT (join->priv->query),
								      G_CALLBACK (target_removed_cb), join);
			}

			join->priv->query = GDA_QUERY (ptr);
			gda_object_connect_destroy (ptr, 
						    G_CALLBACK (destroyed_object_cb), join);
			g_signal_connect (G_OBJECT (ptr), "target_removed",
					  G_CALLBACK (target_removed_cb), join);

			/* NOTE: if the references to the any target is lost then we want to destroy the join */
			join->priv->target1 = GDA_OBJECT_REF (gda_object_ref_new (dict));
			g_object_set (G_OBJECT (join->priv->target1), "helper_ref", ptr, NULL);
			g_signal_connect (G_OBJECT (join->priv->target1), "ref_lost",
					  G_CALLBACK (target_ref_lost_cb), join);

			join->priv->target2 = GDA_OBJECT_REF (gda_object_ref_new (dict));
			g_object_set (G_OBJECT (join->priv->target2), "helper_ref", ptr, NULL);
			g_signal_connect (G_OBJECT (join->priv->target2), "ref_lost",
					  G_CALLBACK (target_ref_lost_cb), join);

			break;
                }
		case PROP_TARGET1_OBJ: {
			GdaQueryTarget* ptr = GDA_QUERY_TARGET (g_value_get_object (value));
			g_return_if_fail (GDA_IS_QUERY_TARGET (ptr));
			gda_object_ref_set_ref_object (join->priv->target1, GDA_OBJECT (ptr));
			break;
                }
		case PROP_TARGET1_ID:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (join->priv->target1, GDA_TYPE_QUERY_TARGET, 
						     REFERENCE_BY_XML_ID, str);
			break;
		case PROP_TARGET2_OBJ: {
			GdaQueryTarget* ptr = GDA_QUERY_TARGET (g_value_get_object (value));
			g_return_if_fail (GDA_IS_QUERY_TARGET (ptr));
			gda_object_ref_set_ref_object (join->priv->target2, GDA_OBJECT (ptr));
			break;
                }
		case PROP_TARGET2_ID:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (join->priv->target2, GDA_TYPE_QUERY_TARGET, 
						     REFERENCE_BY_XML_ID, str);
			break;
		}
	}
}

static void
gda_query_join_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdaQueryJoin *join;
	join = GDA_QUERY_JOIN (object);
	
	if (join->priv) {
		switch (param_id) {
			case PROP_QUERY:
			g_value_set_object (value, G_OBJECT (join->priv->query));
			break;
		case PROP_TARGET1_OBJ:
			g_value_set_object (value, G_OBJECT (gda_object_ref_get_ref_object (join->priv->target1)));
			break;
		case PROP_TARGET2_OBJ:
			g_value_set_object (value, G_OBJECT (gda_object_ref_get_ref_object (join->priv->target2)));
			break;
		default:
			g_assert_not_reached ();
			break;
		}	
	}
}

/**
 * gda_query_join_set_join_type
 * @join: a #GdaQueryJoin object
 * @type: the new type of join
 *
 * Sets the type of @join
 */
void
gda_query_join_set_join_type (GdaQueryJoin *join, GdaQueryJoinType type)
{
	g_return_if_fail (GDA_IS_QUERY_JOIN (join));
	g_return_if_fail (join->priv);
	
	if (join->priv->join_type != type) {
		join->priv->join_type = type;
#ifdef GDA_DEBUG_signal
		g_print (">> 'TYPE_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit_by_name (G_OBJECT (join), "type_changed");
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TYPE_CHANGED' from %s\n", __FUNCTION__);
#endif
		gda_object_signal_emit_changed (GDA_OBJECT (join));
	}
}

/**
 * gda_query_join_get_join_type
 * @join: a #GdaQueryJoin object
 *
 * Get the type of a join
 *
 * Returns: the type of @join
 */
GdaQueryJoinType
gda_query_join_get_join_type (GdaQueryJoin *join)
{
	g_return_val_if_fail (GDA_IS_QUERY_JOIN (join), GDA_QUERY_JOIN_TYPE_CROSS);
	g_return_val_if_fail (join->priv, GDA_QUERY_JOIN_TYPE_CROSS);
	
	return join->priv->join_type;
}


/**
 * gda_query_join_get_query
 * @join: a #GdaQueryJoin object
 * 
 * Get the #GdaQuery to which @join is attached to
 *
 * Returns: the #GdaQuery
 */
GdaQuery *
gda_query_join_get_query (GdaQueryJoin *join)
{
	g_return_val_if_fail (GDA_IS_QUERY_JOIN (join), NULL);
	g_return_val_if_fail (join->priv, NULL);
	
	return join->priv->query;
}

/**
 * gda_query_join_get_target_1
 * @join: a #GdaQueryJoin object
 *
 * Get the 1st #GdaQueryTarget participating in the join
 *
 * Returns: the #GdaQueryTarget
 */
GdaQueryTarget *
gda_query_join_get_target_1 (GdaQueryJoin *join)
{
	GdaObject *base;

	g_return_val_if_fail (GDA_IS_QUERY_JOIN (join), NULL);
	g_return_val_if_fail (join->priv, NULL);
	
	base = gda_object_ref_get_ref_object (join->priv->target1);
	if (base)
		return GDA_QUERY_TARGET (base);
	else
		return NULL;
}

/**
 * gda_query_join_get_target_2
 * @join: a #GdaQueryJoin object
 *
 * Get the 2nd #GdaQueryTarget participating in the join
 *
 * Returns: the #GdaQueryTarget
 */
GdaQueryTarget *
gda_query_join_get_target_2 (GdaQueryJoin *join)
{
	GdaObject *base;

	g_return_val_if_fail (GDA_IS_QUERY_JOIN (join), NULL);
	g_return_val_if_fail (join->priv, NULL);
	
	base = gda_object_ref_get_ref_object (join->priv->target2);
	if (base)
		return GDA_QUERY_TARGET (base);
	else
		return NULL;
}


/**
 * gda_query_join_swap_targets
 * @join: a #GdaQueryJoin object
 *
 * Changes the relative roles of the two #GdaQueryTarget objects. It does not
 * change the join condition itself, and is usefull only for the internals
 * of the #GdaQuery object
 */
void
gda_query_join_swap_targets (GdaQueryJoin *join)
{
	GdaObjectRef *ref;
	g_return_if_fail (GDA_IS_QUERY_JOIN (join));
	g_return_if_fail (join->priv);

	ref = join->priv->target1;
	join->priv->target1 = join->priv->target2;
	join->priv->target2 = ref;

	switch (join->priv->join_type) {
	case GDA_QUERY_JOIN_TYPE_LEFT_OUTER:
		join->priv->join_type = GDA_QUERY_JOIN_TYPE_RIGHT_OUTER;
		break;
	case GDA_QUERY_JOIN_TYPE_RIGHT_OUTER:
		join->priv->join_type = GDA_QUERY_JOIN_TYPE_LEFT_OUTER;
		break;
	default:
		break;
	}
}

/**
 * gda_query_join_set_condition_from_fkcons
 * @join: a #GdaQueryJoin object
 *
 * Creates a #GdaQueryCondition for @join using the foreign key constraints
 * present in the database if the two targets @join joins are database tables
 * (#GdaDictTable objects).
 *
 * If there is more than one FK constraint between the database tables, then
 * no join is created, and the call returns FALSE.
 *
 * Returns: TRUE if suitable foreign keys were found and a join condition
 * has been created
 */
gboolean
gda_query_join_set_condition_from_fkcons (GdaQueryJoin *join)
{
	GdaQueryCondition *jcond = NULL;
	GSList *fklist, *list;
	GdaDict *dict;
	GdaDictTable *tab1, *tab2;
	GdaQueryTarget *target1, *target2;
	GdaDictConstraint *cons = NULL;
	gboolean tab1_is_pkey = FALSE;

	g_return_val_if_fail (GDA_IS_QUERY_JOIN (join), FALSE);
	g_return_val_if_fail (join->priv, FALSE);
	dict = gda_object_get_dict (GDA_OBJECT (join));

	/* checks */
	target1 = gda_query_join_get_target_1 (join);
	tab1 = (GdaDictTable *) gda_query_target_get_represented_entity (target1);
	if (!tab1 || !GDA_IS_DICT_TABLE (tab1))
		return FALSE;

	target2 = gda_query_join_get_target_2 (join);
	tab2 = (GdaDictTable *) gda_query_target_get_represented_entity (target2);
	if (!tab2 || !GDA_IS_DICT_TABLE (tab2))
		return FALSE;

	list = gda_dict_database_get_tables_fk_constraints (gda_dict_get_database (dict), tab1, tab2, FALSE);
	if (g_slist_length (list) != 1) {
		g_slist_free (list);
		return FALSE;
	}
	else {
		cons = GDA_DICT_CONSTRAINT (list->data);
		g_slist_free (list);
		if (gda_dict_constraint_get_table (cons) == tab1)
			tab1_is_pkey = TRUE;
	}

	/* remove any pre-existing join condition */
	if (join->priv->cond)
		destroyed_cond_cb (join->priv->cond, join);
	
	/* condition creation */
	fklist = gda_dict_constraint_fkey_get_fields (cons);
	if (g_slist_length (fklist) > 1)
		jcond = gda_query_condition_new (join->priv->query, GDA_QUERY_CONDITION_NODE_AND);
	list = fklist;
	while (list) {
		GdaDictConstraintFkeyPair *pair = GDA_DICT_CONSTRAINT_FK_PAIR (list->data);
		GdaQueryCondition *cond;
		GdaQueryField *qfield;
		GdaDictField *dbfield;

		if (!jcond) {
			jcond = gda_query_condition_new (join->priv->query, GDA_QUERY_CONDITION_LEAF_EQUAL);
			cond = jcond;
		}
		else {
			cond = gda_query_condition_new (join->priv->query, GDA_QUERY_CONDITION_LEAF_EQUAL);
			g_assert (gda_query_condition_node_add_child (jcond, cond, NULL));
			g_object_unref (cond);
		}
				
		/* left operator */
		if (tab1_is_pkey) 
			dbfield = pair->fkey;
		else
			dbfield = pair->ref_pkey;
		qfield = gda_query_get_field_by_ref_field (join->priv->query,
							  target1, GDA_ENTITY_FIELD (dbfield), GDA_ENTITY_FIELD_ANY);
		if (!qfield) {
			qfield = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
								 "dict", gda_object_get_dict (GDA_OBJECT (join->priv->query)), 
								 "query", join->priv->query, NULL);
			g_object_set (G_OBJECT (qfield), "target", target1, "field", dbfield, NULL);
			gda_query_field_set_visible (qfield, FALSE);
			gda_entity_add_field (GDA_ENTITY (join->priv->query), GDA_ENTITY_FIELD (qfield));
			g_object_unref (qfield);
		}
		gda_query_condition_leaf_set_operator (cond, GDA_QUERY_CONDITION_OP_LEFT, qfield);

		/* right operator */
		if (! tab1_is_pkey) 
			dbfield = pair->fkey;
		else
			dbfield = pair->ref_pkey;
		qfield = gda_query_get_field_by_ref_field (join->priv->query,
							   target2, GDA_ENTITY_FIELD (dbfield), GDA_ENTITY_FIELD_ANY);
		if (!qfield) {
			qfield = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
								 "dict", gda_object_get_dict (GDA_OBJECT (join->priv->query)), 
								 "query", join->priv->query, NULL);
			g_object_set (G_OBJECT (qfield), "target", target2, "field", dbfield, NULL);
			gda_query_field_set_visible (qfield, FALSE);
			gda_entity_add_field (GDA_ENTITY (join->priv->query), GDA_ENTITY_FIELD (qfield));
			g_object_unref (qfield);
		}
		gda_query_condition_leaf_set_operator (cond, GDA_QUERY_CONDITION_OP_RIGHT, qfield);
		
		g_free (pair);
		list = g_slist_next (list);
	}
	g_slist_free (fklist);

	gda_query_join_set_condition (join, jcond);
	g_object_unref (jcond);

	return TRUE;
}

/**
 * gda_query_join_set_condition_from_sql
 * @join: a #GdaQueryJoin object
 * @cond: a SQL expression
 * @error: place to store the error, or %NULL
 *
 * Parses @cond and if it represents a valid SQL expression to be @join's
 * condition, then set it to be @join's condition.
 *
 * Returns: a TRUE on success
 */
gboolean
gda_query_join_set_condition_from_sql (GdaQueryJoin *join, const gchar *cond, GError **error)
{
	GdaQueryCondition *newcond;
	gboolean has_error = FALSE;
	GSList *targets = NULL;

	g_return_val_if_fail (GDA_IS_QUERY_JOIN (join), FALSE);
	g_return_val_if_fail (join->priv, FALSE);
	g_return_val_if_fail (cond && *cond, FALSE);

	newcond = gda_query_condition_new_from_sql (join->priv->query, cond, &targets, error);
	if (newcond) {
		/* test nb of targets */
		if (g_slist_length (targets) != 2) {
			g_set_error (error,
				     GDA_QUERY_JOIN_ERROR,
				     GDA_QUERY_JOIN_SQL_ANALYSE_ERROR,
				     _("Join condition must be between two entities"));
			has_error = TRUE;
		}
		else {
			GdaQueryTarget *t1, *t2;
			GdaQueryTarget *nt1, *nt2;
			nt1 = (GdaQueryTarget *) targets->data;
			nt2 = (GdaQueryTarget *) targets->next->data;
			t1 = gda_query_join_get_target_1 (join);
			t2 = gda_query_join_get_target_2 (join);
			
			if (((t1 == nt1) && (t2 == nt2)) ||
			    ((t1 == nt2) && (t2 == nt1))) {
				gda_query_join_set_condition (join, newcond);
			}
			else {
				g_set_error (error, GDA_QUERY_JOIN_ERROR, GDA_QUERY_JOIN_SQL_ANALYSE_ERROR,
					     _("Condition '%s' does not involve the same "
					       "entities as the join"), cond);
				has_error = TRUE;
			}
		}
		
		if (targets)
			g_slist_free (targets);
		g_object_unref (G_OBJECT (newcond));
	}
	else 
		has_error = TRUE;

	return !has_error;
}

/**
 * gda_query_join_set_condition
 * @join: a #GdaQueryJoin object
 * @cond: a  #GdaQueryCondition object, or %NULL to remove the join's condition
 * 
 * Sets @cond to be @join's condition. This is possible only if @cond uses
 * query fields which are either of type GdaQueryFieldField and reference one of the two
 * targets which @join uses, or any other query field type.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_query_join_set_condition (GdaQueryJoin *join, GdaQueryCondition *cond)
{
	GdaQueryTarget *t1, *t2;
	g_return_val_if_fail (GDA_IS_QUERY_JOIN (join), FALSE);
	g_return_val_if_fail (join->priv, FALSE);

	/* test if the condition is OK */
	if (! gda_query_condition_represents_join (cond, &t1, &t2, NULL)) 
		return FALSE;

	if (! (((gda_object_ref_get_ref_object (join->priv->target1) == (GdaObject*) t1) && 
		(gda_object_ref_get_ref_object (join->priv->target2) == (GdaObject*) t2)) ||
	       ((gda_object_ref_get_ref_object (join->priv->target1) == (GdaObject*) t2) && 
		(gda_object_ref_get_ref_object (join->priv->target2) == (GdaObject*) t1))))
		return FALSE;

	/* actual work */
	if (join->priv->cond && (join->priv->cond != cond)) 
		destroyed_cond_cb (join->priv->cond, join);
	
	if (cond) {
		g_object_ref (G_OBJECT (cond));
		gda_object_connect_destroy (cond, G_CALLBACK (destroyed_cond_cb), join);
		g_signal_connect (G_OBJECT (cond), "changed",
				  G_CALLBACK (cond_changed_cb), join);
		join->priv->cond = cond;
		g_object_set (G_OBJECT (cond), "join", join, NULL);
#ifdef GDA_DEBUG_signal
		g_print (">> 'CONDITION_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit_by_name (G_OBJECT (join), "condition_changed");
#ifdef GDA_DEBUG_signal
		g_print ("<< 'CONDITION_CHANGED' from %s\n", __FUNCTION__);
#endif
		gda_object_signal_emit_changed (GDA_OBJECT (join));
	}

	return TRUE;
}

/**
 * gda_query_join_get_condition
 * @join: a #GdaQueryJoin object
 *
 * Get the join's associated condition
 *
 * Returns: the #GdaQueryCondition object
 */
GdaQueryCondition *
gda_query_join_get_condition (GdaQueryJoin *join)
{
	g_return_val_if_fail (GDA_IS_QUERY_JOIN (join), NULL);
	g_return_val_if_fail (join->priv, NULL);
	
	return join->priv->cond;
}


/**
 * gda_query_join_render_type
 * @join: a #GdaQueryJoin object
 *
 * Get the SQL version of the join type ("INNER JOIN", "LEFT JOIN", etc)
 *
 * Returns: the type as a const string
 */
const gchar *
gda_query_join_render_type (GdaQueryJoin *join)
{
	g_return_val_if_fail (GDA_IS_QUERY_JOIN (join), NULL);
	g_return_val_if_fail (join->priv, NULL);

	switch (join->priv->join_type) {
	case GDA_QUERY_JOIN_TYPE_INNER:
		return "INNER JOIN";
	case GDA_QUERY_JOIN_TYPE_LEFT_OUTER:
		return "LEFT JOIN";
	case GDA_QUERY_JOIN_TYPE_RIGHT_OUTER:
		return "RIGHT JOIN";
	case GDA_QUERY_JOIN_TYPE_FULL_OUTER:
		return "FULL JOIN";
        case GDA_QUERY_JOIN_TYPE_CROSS:
		return "CROSS JOIN";
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

#ifdef GDA_DEBUG
static void
gda_query_join_dump (GdaQueryJoin *join, guint offset)
{
	gchar *str;
        guint i;
	
	g_return_if_fail (GDA_IS_QUERY_JOIN (join));

        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (join->priv) {
                g_print ("%s" D_COL_H1 "GdaQueryJoin" D_COL_NOR " %p ", str, join);
		if (gda_query_join_is_active (GDA_REFERER (join)))
			g_print ("Active, Targets: %p -> %p ", gda_object_ref_get_ref_object (join->priv->target1),
				 gda_object_ref_get_ref_object (join->priv->target2));
		else
			g_print (D_COL_ERR "Non active" D_COL_NOR ", ");
		g_print ("requested targets: %s & %s", gda_object_ref_get_ref_name (join->priv->target1, NULL, NULL),
			 gda_object_ref_get_ref_name (join->priv->target2, NULL, NULL));
		if (join->priv->cond) {
			g_print (", Condition:\n");
			gda_object_dump (GDA_OBJECT (join->priv->cond), offset+5);
		}
		else
			g_print ("\n");
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, join);
}
#endif

static void
gda_query_join_set_int_id (GdaQueryObject *join, guint id)
{

	/* REM: join IDs are not used because they are never found by their ID */
	g_assert_not_reached ();
	/*
	gchar *str;
	str = g_strdup_printf ("%s:J%u", gda_object_get_id (GDA_OBJECT (GDA_QUERY_JOIN (join)->priv->query)), id);
	gda_object_set_id (GDA_OBJECT (join), str);
	g_free (str);
	*/
}


/* 
 * GdaReferer interface implementation
 */
static gboolean
gda_query_join_activate (GdaReferer *iface)
{
	gboolean retval;
	g_return_val_if_fail (iface && GDA_IS_QUERY_JOIN (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_JOIN (iface)->priv, FALSE);

	retval = gda_object_ref_activate (GDA_QUERY_JOIN (iface)->priv->target1);
	retval = retval && gda_object_ref_activate (GDA_QUERY_JOIN (iface)->priv->target2) && retval;
	if (GDA_QUERY_JOIN (iface)->priv->cond)
		retval = retval && gda_referer_activate (GDA_REFERER (GDA_QUERY_JOIN (iface)->priv->cond));

	return retval;
}

static void
gda_query_join_deactivate (GdaReferer *iface)
{
	g_return_if_fail (iface && GDA_IS_QUERY_JOIN (iface));
	g_return_if_fail (GDA_QUERY_JOIN (iface)->priv);

	gda_object_ref_deactivate (GDA_QUERY_JOIN (iface)->priv->target1);
	gda_object_ref_deactivate (GDA_QUERY_JOIN (iface)->priv->target2);
	if (GDA_QUERY_JOIN (iface)->priv->cond)
		gda_referer_deactivate (GDA_REFERER (GDA_QUERY_JOIN (iface)->priv->cond));
}

static gboolean
gda_query_join_is_active (GdaReferer *iface)
{
	gboolean retval;

	g_return_val_if_fail (iface && GDA_IS_QUERY_JOIN (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_JOIN (iface)->priv, FALSE);

	retval = gda_object_ref_is_active (GDA_QUERY_JOIN (iface)->priv->target1);
	retval = retval && gda_object_ref_is_active (GDA_QUERY_JOIN (iface)->priv->target2);
	if (GDA_QUERY_JOIN (iface)->priv->cond) 
		retval = retval && gda_referer_is_active (GDA_REFERER (GDA_QUERY_JOIN (iface)->priv->cond));
	
	return retval;
}

static GSList *
gda_query_join_get_ref_objects (GdaReferer *iface)
{
	GSList *list = NULL;
	GdaObject *base;
	GdaQueryJoin *join;
	g_return_val_if_fail (iface && GDA_IS_QUERY_JOIN (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_JOIN (iface)->priv, NULL);

	join = GDA_QUERY_JOIN (iface);
	base = gda_object_ref_get_ref_object (join->priv->target1);
	if (base)
		list = g_slist_append (list, base);
	base = gda_object_ref_get_ref_object (join->priv->target2);
	if (base)
		list = g_slist_append (list, base);
	if (join->priv->cond) {
		GSList *list2 = gda_query_condition_get_ref_objects_all (join->priv->cond);
		if (list2)
			list = g_slist_concat (list, list2);
	}

	return list;
}

static void
gda_query_join_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	GdaQueryJoin *join;
	g_return_if_fail (iface && GDA_IS_QUERY_JOIN (iface));
	g_return_if_fail (GDA_QUERY_JOIN (iface)->priv);

	join = GDA_QUERY_JOIN (iface);
	if (join->priv->query) {
		GdaQuery *query = g_hash_table_lookup (replacements, join->priv->query);
		if (query) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (join->priv->query),
							      G_CALLBACK (destroyed_object_cb), join);
			g_signal_handlers_disconnect_by_func (G_OBJECT (join->priv->query),
							      G_CALLBACK (target_removed_cb), join);
			join->priv->query = query;
			gda_object_connect_destroy (query, 
						 G_CALLBACK (destroyed_object_cb), join);
			g_signal_connect (G_OBJECT (query), "target_removed",
					  G_CALLBACK (target_removed_cb), join);
		}
	}

	gda_object_ref_replace_ref_object (join->priv->target1, replacements);
	gda_object_ref_replace_ref_object (join->priv->target2, replacements);
	if (join->priv->cond) 
		gda_referer_replace_refs (GDA_REFERER (join->priv->cond), replacements);
}

/* 
 * GdaXmlStorage interface implementation
 */

static const gchar *convert_join_type_to_str (GdaQueryJoinType type);
static GdaQueryJoinType convert_str_to_join_type (const gchar *str);

static xmlNodePtr
gda_query_join_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	GdaQueryJoin *join;
	gchar *str;
	const gchar *type;

	g_return_val_if_fail (iface && GDA_IS_QUERY_JOIN (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_JOIN (iface)->priv, NULL);

	join = GDA_QUERY_JOIN (iface);

	node = xmlNewNode (NULL, (xmlChar*)"gda_query_join");
	
	/* targets */
	if (join->priv->target1) {
		str = NULL;
		if (gda_object_ref_is_active (join->priv->target1)) {
			GdaObject *base = gda_object_ref_get_ref_object (join->priv->target1);
			g_assert (base);
			str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (base));
		}
		else 
			str = g_strdup (gda_object_ref_get_ref_name (join->priv->target1, NULL, NULL));
		
		if (str) {
			xmlSetProp(node, (xmlChar*)"target1", (xmlChar*)str);
			g_free (str);
		}
	}

	if (join->priv->target2) {
		str = NULL;
		if (gda_object_ref_is_active (join->priv->target2)) {
			GdaObject *base = gda_object_ref_get_ref_object (join->priv->target2);
			g_assert (base);
			str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (base));
		}
		else 
			str = g_strdup (gda_object_ref_get_ref_name (join->priv->target2, NULL, NULL));
		
		if (str) {
			xmlSetProp(node, (xmlChar*)"target2", (xmlChar*)str);
			g_free (str);
		}
	}

	/* join type */
	type = convert_join_type_to_str (join->priv->join_type);
	xmlSetProp(node, (xmlChar*)"join_type", (xmlChar*)type);

	/* join condition */
	if (join->priv->cond) {
		xmlNodePtr sub = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (join->priv->cond), error);
		if (sub)
                        xmlAddChild (node, sub);
                else {
                        xmlFreeNode (node);
                        return NULL;
                }
	}

	return node;
}


static gboolean
gda_query_join_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaQueryJoin *join;
	gchar *prop;
	gboolean t1 = FALSE, t2 = FALSE;
	xmlNodePtr children;

	g_return_val_if_fail (iface && GDA_IS_QUERY_JOIN (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_JOIN (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	join = GDA_QUERY_JOIN (iface);
	if (strcmp ((gchar*)node->name, "gda_query_join")) {
		g_set_error (error,
			     GDA_QUERY_JOIN_ERROR,
			     GDA_QUERY_JOIN_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_query_join>"));
		return FALSE;
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"target1");
	if (prop) {
		if (join->priv->target1) {
			gda_object_ref_set_ref_name (join->priv->target1, GDA_TYPE_QUERY_TARGET, 
						  REFERENCE_BY_XML_ID, prop);
			t1 = TRUE;
		}
		g_free (prop);
	}
	prop = (gchar*)xmlGetProp(node, (xmlChar*)"target2");
	if (prop) {
		if (join->priv->target2) {
			gda_object_ref_set_ref_name (join->priv->target2, GDA_TYPE_QUERY_TARGET, 
						  REFERENCE_BY_XML_ID, prop);
			t2 = TRUE;
		}
		g_free (prop);
	}
	prop = (gchar*)xmlGetProp(node, (xmlChar*)"join_type");
	if (prop) {
		join->priv->join_type = convert_str_to_join_type (prop);
		g_free (prop);
	}

	/* children nodes */
	children = node->children;
	while (children) {
		if (!strcmp ((gchar*)children->name, "gda_query_cond")) {
			GdaQueryCondition *cond;

			cond = gda_query_condition_new (join->priv->query, GDA_QUERY_CONDITION_NODE_AND);
			if (gda_xml_storage_load_from_xml (GDA_XML_STORAGE (cond), children, error)) {
				gda_query_join_set_condition (join, cond);
				g_object_unref (G_OBJECT (cond));
			}
			else
				return FALSE;
                }

		children = children->next;
	}
	if (t1 && t2)
		return TRUE;
	else {
		g_set_error (error,
			     GDA_QUERY_JOIN_ERROR,
			     GDA_QUERY_JOIN_XML_LOAD_ERROR,
			     _("Problem loading <gda_query_join>"));
		return FALSE;
	}
}

static const gchar *
convert_join_type_to_str (GdaQueryJoinType type)
{
	switch (type) {
	default:
	case GDA_QUERY_JOIN_TYPE_INNER:
		return "INNER";
	case GDA_QUERY_JOIN_TYPE_LEFT_OUTER:
		return "LEFT";
	case GDA_QUERY_JOIN_TYPE_RIGHT_OUTER:
		return "RIGHT";
	case GDA_QUERY_JOIN_TYPE_FULL_OUTER:
		return "FULL";
        case GDA_QUERY_JOIN_TYPE_CROSS:
		return "CROSS";
	}
}

static GdaQueryJoinType
convert_str_to_join_type (const gchar *str)
{
	switch (*str) {
	case 'I':
	default:
		return GDA_QUERY_JOIN_TYPE_INNER;
	case 'L':
		return GDA_QUERY_JOIN_TYPE_LEFT_OUTER;
	case 'R':
		return GDA_QUERY_JOIN_TYPE_RIGHT_OUTER;
	case 'F':
		return GDA_QUERY_JOIN_TYPE_FULL_OUTER;
	case 'C':
		return GDA_QUERY_JOIN_TYPE_CROSS;
	}
}
