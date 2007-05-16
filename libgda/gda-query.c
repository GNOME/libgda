/* gda-query.c
 *
 * Copyright (C) 2003 - 2007 Vivien Malerba
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
#include "gda-query.h"
#include "gda-entity-field.h"
#include "gda-query-field.h"
#include "gda-query-field-field.h"
#include "gda-query-field-value.h"
#include "gda-query-field-all.h"
#include "gda-object-ref.h"
#include "gda-query-target.h"
#include "gda-entity.h"
#include "gda-dict-table.h"
#include "gda-dict-database.h"
#include "gda-dict-constraint.h"
#include "gda-xml-storage.h"
#include "gda-referer.h"
#include "gda-renderer.h"
#include "gda-parameter.h"
#include "gda-parameter-util.h"
#include "gda-marshal.h"
#include "gda-query-join.h"
#include "gda-parameter-list.h"
#include "gda-query-condition.h"
#include "gda-connection.h"
#include "gda-dict-function.h"
#include "gda-dict-aggregate.h"
#include "gda-data-handler.h"
#include "gda-query-field-func.h"
#include "gda-data-model-query.h"
#include "gda-data-model-private.h"

#include "gda-query-private.h"
#include "gda-query-parsing.h"

#include "gda-dict-reg-queries.h"

/* 
 * Main static functions 
 */
static void gda_query_class_init (GdaQueryClass * class);
static GObject *gda_query_constructor (GType type,
                                       guint n_construct_properties,
		                       GObjectConstructParam *construct_properties);
static void gda_query_init (GdaQuery * srv);
static void gda_query_dispose (GObject   * object);
static void gda_query_finalize (GObject   * object);

static void gda_query_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_query_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);
/* Entity interface */
static void        gda_query_entity_init         (GdaEntityIface *iface);
static gboolean    gda_query_has_field           (GdaEntity *iface, GdaEntityField *field);
static GSList     *gda_query_get_fields          (GdaEntity *iface);
static GdaEntityField    *gda_query_get_field_by_name   (GdaEntity *iface, const gchar *name);
static GdaEntityField    *gda_query_get_field_by_xml_id (GdaEntity *iface, const gchar *xml_id);
static GdaEntityField    *gda_query_get_field_by_index  (GdaEntity *iface, gint index);
static gint        gda_query_get_field_index     (GdaEntity *iface, GdaEntityField *field);
static void        gda_query_add_field           (GdaEntity *iface, GdaEntityField *field);
static void        gda_query_add_field_before    (GdaEntity *iface, GdaEntityField *field, GdaEntityField *field_before);
static void        gda_query_swap_fields         (GdaEntity *iface, GdaEntityField *field1, GdaEntityField *field2);
static void        gda_query_remove_field        (GdaEntity *iface, GdaEntityField *field);
static gboolean    gda_query_is_writable         (GdaEntity *iface);

/* XML storage interface */
static void        gda_query_xml_storage_init (GdaXmlStorageIface *iface);
static xmlNodePtr  gda_query_save_to_xml      (GdaXmlStorage *iface, GError **error);
static gboolean    gda_query_load_from_xml    (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* Referer interface */
static void        gda_query_referer_init        (GdaRefererIface *iface);
static gboolean    gda_query_activate            (GdaReferer *iface);
static void        gda_query_deactivate          (GdaReferer *iface);
static gboolean    gda_query_is_active           (GdaReferer *iface);
static GSList     *gda_query_get_ref_objects     (GdaReferer *iface);
static void        gda_query_replace_refs        (GdaReferer *iface, GHashTable *replacements);

/* Renderer interface */
static void        gda_query_renderer_init   (GdaRendererIface *iface);
static gchar      *gda_query_render_as_sql   (GdaRenderer *iface, GdaParameterList *context, 
					      GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar      *gda_query_render_as_str   (GdaRenderer *iface, GdaParameterList *context);
static gboolean    gda_query_is_valid        (GdaRenderer *iface, GdaParameterList *context, GError **error);

/* callbaks from sub objects */
static void destroyed_target_cb (GdaQueryTarget *target, GdaQuery *query);
static void destroyed_field_cb (GdaEntityField *field, GdaQuery *query);
static void destroyed_join_cb (GdaQueryJoin *join, GdaQuery *query);
static void destroyed_cond_cb (GdaQueryCondition *cond, GdaQuery *query);
static void destroyed_sub_query_cb (GdaQuery *sub_query, GdaQuery *query);
static void destroyed_parent_query (GdaQuery *parent_query, GdaQuery *query);

static void destroyed_param_source_cb (GdaDataModel *param_source, GdaQuery *query);

static void changed_target_cb (GdaQueryTarget *target, GdaQuery *query);
static void changed_field_cb (GdaEntityField *field, GdaQuery *query);
static void changed_join_cb (GdaQueryJoin *join, GdaQuery *query);
static void changed_sub_query_cb (GdaQuery *sub_query, GdaQuery *query);
static void changed_cond_cb (GdaQueryCondition *cond, GdaQuery *query);

static void change_parent_query (GdaQuery *query, GdaQuery *parent_query);

static void id_target_changed_cb (GdaQueryTarget *target, GdaQuery *query);
static void id_field_changed_cb (GdaQueryField *field, GdaQuery *query);
static void id_cond_changed_cb (GdaQueryCondition *cond, GdaQuery *query);

static gboolean query_sql_forget (GdaQuery *query, GError **error);
static void     query_clean_junk (GdaQuery *query);

static void gda_query_set_int_id (GdaQueryObject *query, guint id);
#ifdef GDA_DEBUG
static void gda_query_dump (GdaQuery *query, guint offset);
#endif

typedef struct
{
	GSList *targets;
	GSList *joins;
} JoinsPack;
#define JOINS_PACK(x) ((JoinsPack *)x)


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	TYPE_CHANGED,
	CONDITION_CHANGED,
	TARGET_ADDED,
	TARGET_REMOVED,
	TARGET_UPDATED,
	JOIN_ADDED,
	JOIN_REMOVED,
	JOIN_UPDATED,
	SUB_QUERY_ADDED,
	SUB_QUERY_REMOVED,
	SUB_QUERY_UPDATED,
	LAST_SIGNAL
};

static gint gda_query_signals[LAST_SIGNAL] = { 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_SERIAL_TARGET,
	PROP_SERIAL_FIELD,
	PROP_SERIAL_COND,
	PROP_REALLY_ALL_FIELDS,
	PROP_AUTO_CLEAN
};

/* module error */
GQuark gda_query_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_query_error");
	return quark;
}


GType
gda_query_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_class_init,
			NULL,
			NULL,
			sizeof (GdaQuery),
			0,
			(GInstanceInitFunc) gda_query_init
		};

		static const GInterfaceInfo entity_info = {
			(GInterfaceInitFunc) gda_query_entity_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_query_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo referer_info = {
			(GInterfaceInitFunc) gda_query_referer_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo renderer_info = {
			(GInterfaceInitFunc) gda_query_renderer_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GDA_TYPE_QUERY_OBJECT, "GdaQuery", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_ENTITY, &entity_info);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
		g_type_add_interface_static (type, GDA_TYPE_RENDERER, &renderer_info);
	}
	return type;
}

static void
gda_query_entity_init (GdaEntityIface *iface)
{
	iface->has_field = gda_query_has_field;
	iface->get_fields = gda_query_get_fields;
	iface->get_field_by_name = gda_query_get_field_by_name;
	iface->get_field_by_xml_id = gda_query_get_field_by_xml_id;
	iface->get_field_by_index = gda_query_get_field_by_index;
	iface->get_field_index = gda_query_get_field_index;
	iface->add_field = gda_query_add_field;
	iface->add_field_before = gda_query_add_field_before;
	iface->swap_fields = gda_query_swap_fields;
	iface->remove_field = gda_query_remove_field;
	iface->is_writable = gda_query_is_writable;
}

static void 
gda_query_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = NULL;
	iface->save_to_xml = gda_query_save_to_xml;
	iface->load_from_xml = gda_query_load_from_xml;
}

static void
gda_query_referer_init (GdaRefererIface *iface)
{
	iface->activate = gda_query_activate;
	iface->deactivate = gda_query_deactivate;
	iface->is_active = gda_query_is_active;
	iface->get_ref_objects = gda_query_get_ref_objects;
	iface->replace_refs = gda_query_replace_refs;
}

static void
gda_query_renderer_init (GdaRendererIface *iface)
{
	iface->render_as_sql = gda_query_render_as_sql;
	iface->render_as_str = gda_query_render_as_str;
	iface->is_valid = gda_query_is_valid;
}

static void m_changed_cb (GdaQuery *query);
static void
gda_query_class_init (GdaQueryClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	gda_query_signals[TYPE_CHANGED] =
		g_signal_new ("type_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, type_changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	gda_query_signals[CONDITION_CHANGED] =
		g_signal_new ("condition_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, condition_changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	gda_query_signals[TARGET_ADDED] =
		g_signal_new ("target_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, target_added),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
			      1, GDA_TYPE_QUERY_TARGET);
	gda_query_signals[TARGET_REMOVED] =
		g_signal_new ("target_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, target_removed),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
			      1, GDA_TYPE_QUERY_TARGET);
	gda_query_signals[TARGET_UPDATED] =
		g_signal_new ("target_updated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, target_updated),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
			      1, GDA_TYPE_QUERY_TARGET);

	gda_query_signals[JOIN_ADDED] =
		g_signal_new ("join_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, join_added),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
			      1, GDA_TYPE_QUERY_JOIN);
	gda_query_signals[JOIN_REMOVED] =
		g_signal_new ("join_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, join_removed),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
			      1, GDA_TYPE_QUERY_JOIN);
	gda_query_signals[JOIN_UPDATED] =
		g_signal_new ("join_updated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, join_updated),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
			      1, GDA_TYPE_QUERY_JOIN);

	gda_query_signals[SUB_QUERY_ADDED] =
		g_signal_new ("sub_query_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, sub_query_added),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
			      1, GDA_TYPE_QUERY);
	gda_query_signals[SUB_QUERY_REMOVED] =
		g_signal_new ("sub_query_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, sub_query_removed),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
			      1, GDA_TYPE_QUERY);
	gda_query_signals[SUB_QUERY_UPDATED] =
		g_signal_new ("sub_query_updated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaQueryClass, sub_query_updated),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
			      1, GDA_TYPE_QUERY);

	klass->type_changed = m_changed_cb;
	klass->condition_changed = m_changed_cb;
	klass->target_added = (void (*) (GdaQuery *, GdaQueryTarget *)) m_changed_cb;
	klass->target_removed = NULL;
	klass->target_updated = (void (*) (GdaQuery *, GdaQueryTarget *)) m_changed_cb;
	klass->join_added = (void (*) (GdaQuery *, GdaQueryJoin *)) m_changed_cb;
	klass->join_removed = NULL;
	klass->join_updated = (void (*) (GdaQuery *, GdaQueryJoin *)) m_changed_cb;
	klass->sub_query_added = NULL;
	klass->sub_query_removed = NULL;
	klass->sub_query_updated = NULL;

	object_class->constructor = gda_query_constructor;
	object_class->dispose = gda_query_dispose;
	object_class->finalize = gda_query_finalize;

	/* Properties */
	object_class->set_property = gda_query_set_property;
	object_class->get_property = gda_query_get_property;
	g_object_class_install_property (object_class, PROP_SERIAL_TARGET,
					 g_param_spec_uint ("target_serial", NULL, NULL, 
							    1, G_MAXUINT, 1, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_SERIAL_FIELD,
					 g_param_spec_uint ("field_serial", NULL, NULL, 
							    1, G_MAXUINT, 1, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_SERIAL_COND,
					 g_param_spec_uint ("cond_serial", NULL, NULL, 
							    1, G_MAXUINT, 1, G_PARAM_READABLE));

        /* Note that this is actually a GSList*, 
         * but there does not seem to be a way to define that in the properties system:
         */
	g_object_class_install_property (object_class, PROP_REALLY_ALL_FIELDS,
					 g_param_spec_pointer ("really_all_fields", NULL, NULL, G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_AUTO_CLEAN,
					 g_param_spec_boolean ("auto_clean", "Auto cleaning", 
							       "Determines if the query tries to clean unused objects", TRUE,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));

	/* virtual functions */
        GDA_QUERY_OBJECT_CLASS (klass)->set_int_id = (void (*)(GdaQueryObject *, guint)) gda_query_set_int_id;
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (klass)->dump = (void (*)(GdaObject *, guint)) gda_query_dump;
#endif

}

static void
m_changed_cb (GdaQuery *query)
{
	if (!query->priv->internal_transaction)
		gda_object_signal_emit_changed (GDA_OBJECT (query));
}

static GObject *
gda_query_constructor (GType type,
                       guint n_construct_properties,
		       GObjectConstructParam *construct_properties)
{
	GObject* obj;
	GdaDict* dict;
	guint id;
	GdaDictRegisterStruct *reg;

	GdaQueryClass* klass;
	klass = GDA_QUERY_CLASS(g_type_class_peek(GDA_TYPE_QUERY));

	obj = parent_class->constructor (type, n_construct_properties,
	                                 construct_properties);

	g_object_get(obj, "dict", &dict, NULL);
	reg = gda_dict_get_object_type_registration (ASSERT_DICT (dict), GDA_TYPE_QUERY);
	
	id = gda_queries_get_serial (reg);
	gda_query_object_set_int_id (GDA_QUERY_OBJECT (obj), id);
	
	gda_dict_declare_object_as (ASSERT_DICT (dict), GDA_OBJECT (obj), GDA_TYPE_QUERY);

	return obj;
}

static void
gda_query_init (GdaQuery * gda_query)
{
	gda_query->priv = g_new0 (GdaQueryPrivate, 1);
	gda_query->priv->query_type = GDA_QUERY_TYPE_SELECT;
	gda_query->priv->targets = NULL;
	gda_query->priv->joins_flat = NULL;
	gda_query->priv->joins_pack = NULL;
	gda_query->priv->fields = NULL;
	gda_query->priv->sub_queries = NULL;
	gda_query->priv->param_sources = NULL; /* list of GdaDataModel objects, owned here */
	gda_query->priv->cond = NULL;
	gda_query->priv->parent_query = NULL;
	gda_query->priv->sql = NULL;
	gda_query->priv->sql_exprs = NULL;
	gda_query->priv->fields_order_by = NULL;

	gda_query->priv->serial_target = 1;
	gda_query->priv->serial_field = 1;
	gda_query->priv->serial_cond = 1;
	gda_query->priv->internal_transaction = 0;

	gda_query->priv->all_conds = NULL;
	gda_query->priv->auto_clean = TRUE;
}

/**
 * gda_query_new
 * @dict: a #GdaDict object
 *
 * Creates a new #GdaQuery object
 *
 * Returns: the new object
 */
GdaQuery*
gda_query_new (GdaDict *dict)
{
	GObject *obj;
	GdaQuery *gda_query;

	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);
	obj = g_object_new (GDA_TYPE_QUERY, "dict", ASSERT_DICT (dict), NULL);
	gda_query = GDA_QUERY (obj);

	return gda_query;
}


/**
 * gda_query_new_copy
 * @orig: a #GdaQuery to make a copy of
 * @replacements: a hash table to store replacements, or %NULL
 * 
 * Copy constructor
 *
 * Returns: a the new copy of @orig
 */
GdaQuery *
gda_query_new_copy (GdaQuery *orig, GHashTable *replacements)
{
	GObject *obj;
	GdaQuery *gda_query;
	GdaDict *dict;
	GHashTable *repl;
	GSList *list;
	guint id;
	gint order_pos;
	GdaDictRegisterStruct *reg;

	g_return_val_if_fail (orig && GDA_IS_QUERY (orig), NULL);

	dict = gda_object_get_dict (GDA_OBJECT (orig));
	obj = g_object_new (GDA_TYPE_QUERY, "dict", dict, NULL);
	gda_query = GDA_QUERY (obj);

	gda_query->priv->internal_transaction ++;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_QUERY);
	id = gda_queries_get_serial (reg);
	gda_query_object_set_int_id (GDA_QUERY_OBJECT (obj), id);

	/* hash table for replacements */
	if (!replacements)
		repl = g_hash_table_new (NULL, NULL);
	else
		repl = replacements;

	g_hash_table_insert (repl, orig, gda_query);

	/* REM: parent query link is handled by the caller of this method */
	
	/* copy starts */
	gda_object_set_name (GDA_OBJECT (gda_query), gda_object_get_name (GDA_OBJECT (orig)));
	gda_object_set_description (GDA_OBJECT (gda_query), gda_object_get_description (GDA_OBJECT (orig)));
	gda_query->priv->query_type = orig->priv->query_type;
	if (orig->priv->sql)
		gda_query->priv->sql = g_strdup (orig->priv->sql);
	if (orig->priv->sql_exprs)
		gda_query->priv->sql_exprs = gda_delimiter_parse_copy_statement (orig->priv->sql_exprs, repl);
	gda_query->priv->global_distinct = orig->priv->global_distinct;

	/* copy sub queries */
	list = orig->priv->sub_queries;
	while (list) {
		GdaQuery *copy = gda_query_new_copy (GDA_QUERY (list->data), repl);
		gda_referer_replace_refs (GDA_REFERER (copy), repl);
		gda_query_add_sub_query (gda_query, copy);
		g_object_unref (G_OBJECT (copy));
		
		list = g_slist_next (list);
	}

	/* copy param sources */
	list = orig->priv->param_sources;
	while (list) {
		gda_query_add_param_source (gda_query, GDA_DATA_MODEL (list->data));
		list = g_slist_next (list);
	}

	/* copy targets */
	list = orig->priv->targets;
	while (list) {
		guint id;
		GdaQueryTarget *target = gda_query_target_new_copy (GDA_QUERY_TARGET (list->data));

		gda_referer_replace_refs (GDA_REFERER (target), repl);
		gda_query_add_target (gda_query, target, NULL);
		g_object_get (G_OBJECT (gda_query), "target_serial", &id, NULL);
		gda_query_object_set_int_id (GDA_QUERY_OBJECT (target), id);

		gda_query_target_set_alias (target, 
					    gda_query_target_get_alias ((GdaQueryTarget *) list->data));

		g_object_unref (G_OBJECT (target));
		g_hash_table_insert (repl, list->data, target);
		list = g_slist_next (list);
	}

	/* copy fields */
	list = orig->priv->fields;
	while (list) {
		guint id;
		GdaQueryField *qf = GDA_QUERY_FIELD (gda_query_field_new_copy (GDA_QUERY_FIELD (list->data)));
		gda_referer_replace_refs (GDA_REFERER (qf), repl);
		gda_query_add_field (GDA_ENTITY (gda_query), GDA_ENTITY_FIELD (qf));
		g_object_get (G_OBJECT (gda_query), "field_serial", &id, NULL);
		gda_query_object_set_int_id (GDA_QUERY_OBJECT (qf), id);

		gda_query_field_set_alias (qf,
					   gda_query_field_get_alias ((GdaQueryField *) list->data));

		g_object_unref (G_OBJECT (qf));
		g_hash_table_insert (repl, list->data, qf);

		if (gda_query->priv->sql_exprs) {
			gpointer data;

			data = g_object_get_data (G_OBJECT (list->data), "pspeclist");
			if (data) 
				g_object_set_data (G_OBJECT (g_hash_table_lookup (repl, list->data)),
						   "pspeclist",
						   g_hash_table_lookup (repl, data));
		}

		list = g_slist_next (list);
	}

	/* copy joins */
	list = orig->priv->joins_flat;
	while (list) {
		GdaQueryJoin *join = gda_query_join_new_copy (GDA_QUERY_JOIN (list->data), repl);
		gda_referer_replace_refs (GDA_REFERER (join), repl);
		gda_query_add_join (gda_query, join);
		g_object_unref (G_OBJECT (join));
		g_hash_table_insert (repl, list->data, join);
		list = g_slist_next (list);
	}

	/* copy condition */
	if (orig->priv->cond) {
		GdaQueryCondition *cond;
		guint id;

		cond = gda_query_condition_new_copy (orig->priv->cond, repl);
		g_object_get (G_OBJECT (gda_query), "cond_serial", &id, NULL);
		gda_referer_replace_refs (GDA_REFERER (cond), repl);
		gda_query_object_set_int_id (GDA_QUERY_OBJECT (cond), id);
		gda_query_set_condition (gda_query, cond);

		g_object_unref (G_OBJECT (cond));
		g_hash_table_insert (repl, orig->priv->cond, cond);
	}

	/* copy ORDER BY fields list */
	order_pos = 0;
	list = orig->priv->fields_order_by;
	while (list) {
		GdaQueryField *field;
		gboolean asc;

		asc = g_object_get_data (G_OBJECT (list->data), "order_by_asc") ? TRUE : FALSE;
		field = g_hash_table_lookup (repl, list->data);
		gda_query_set_order_by_field (gda_query, field, order_pos, asc);

		order_pos++;
		list = g_slist_next (list);
	}
		
	/* Another loop to make sure all the references have been replaced */
	gda_query_replace_refs (GDA_REFERER (gda_query), repl);

	if (!replacements)
		g_hash_table_destroy (repl);

	gda_query->priv->internal_transaction --;

#ifdef GDA_DEBUG_NO
	g_print ("Query %p is a copy of %p\n", gda_query, orig);
	gda_object_dump (GDA_OBJECT (orig), 2);
	gda_object_dump (GDA_OBJECT (gda_query), 2);
#endif

	return gda_query;	
}

/**
 * gda_query_new_from_sql
 * @dict: a #GdaDict object
 * @sql: an SQL statement
 * @error: location to store error, or %NULL
 *
 * Creates a new #GdaQuery object and fills its structure by parsing the @sql. If the parsing failed,
 * then the returned query is of type GDA_QUERY_TYPE_NON_PARSED_SQL.
 *
 * To be parsed successfully, the expected SQL must respect the SQL standard; some extensions have been
 * added to be able to define variables within the SQL statement. See the introduction to the #GdaQuery
 * for more information. 
 *
 * The @error is set only if the SQL statement parsing produced an error; there is always a new #GdaQuery
 * object which is returned.
 *
 * Returns: a new #GdaQuery
 */
GdaQuery *
gda_query_new_from_sql (GdaDict *dict, const gchar *sql, GError **error) 
{
	GdaQuery *query;

	query = gda_query_new (dict);
	gda_query_set_sql_text (query, sql, error);

	return query;
}


static void gda_query_clean (GdaQuery *gda_query);
static void
gda_query_dispose (GObject *object)
{
	GdaQuery *gda_query;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY (object));

	gda_query = GDA_QUERY (object);
	if (gda_query->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));
		
		gda_query_clean (gda_query);
	}

	/* parent class */
	parent_class->dispose (object);
}

static void cond_weak_ref_notify (GdaQuery *query, GdaQueryCondition *cond);

/* completely cleans the query's contents */
static void
gda_query_clean (GdaQuery *gda_query)
{
	/* order by list */
	if (gda_query->priv->fields_order_by) {
		g_slist_free (gda_query->priv->fields_order_by);
		gda_query->priv->fields_order_by = NULL;
	}
	
	/* parent query */
	if (gda_query->priv->parent_query)
		change_parent_query (gda_query, NULL);
	
	/* condition */
	if (gda_query->priv->cond)
		gda_object_destroy (GDA_OBJECT (gda_query->priv->cond));
	
	/* parameter sources */
	while (gda_query->priv->param_sources) 
		gda_object_destroy (GDA_OBJECT (gda_query->priv->param_sources->data));
	
	/* sub queries */
	while (gda_query->priv->sub_queries) 
		gda_object_destroy (GDA_OBJECT (gda_query->priv->sub_queries->data));
	
	/* joins */
	while (gda_query->priv->joins_flat) 
		gda_object_destroy (GDA_OBJECT (gda_query->priv->joins_flat->data));
	
	/* fields */
	while (gda_query->priv->fields) 
		gda_object_destroy (GDA_OBJECT (gda_query->priv->fields->data));
	
	/* targets */
	while (gda_query->priv->targets) 
		gda_object_destroy (GDA_OBJECT (gda_query->priv->targets->data));

	/* SQL statement */
	if (gda_query->priv->sql) {
		g_free (gda_query->priv->sql);
		gda_query->priv->sql = NULL;
	}
	if (gda_query->priv->sql_exprs) {
		gda_delimiter_destroy (gda_query->priv->sql_exprs);
		gda_query->priv->sql_exprs = NULL;
	}

	/* restore safe default values */
	gda_query->priv->query_type = GDA_QUERY_TYPE_SELECT;
	gda_query->priv->serial_target = 1;
	gda_query->priv->serial_field = 1;
	gda_query->priv->serial_cond = 1;

	/* clean priv->all_conds */
	if (gda_query->priv->all_conds) {
		while (gda_query->priv->all_conds)
			cond_weak_ref_notify (gda_query, 
					      GDA_QUERY_CONDITION (gda_query->priv->all_conds->data));
	}

	if (!gda_query->priv->internal_transaction)
		gda_object_signal_emit_changed (GDA_OBJECT (gda_query));
}

static void
gda_query_finalize (GObject *object)
{
	GdaQuery *gda_query;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY (object));

	gda_query = GDA_QUERY (object);
	if (gda_query->priv) {
		g_free (gda_query->priv);
		gda_query->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_query_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaQuery *gda_query;

	gda_query = GDA_QUERY (object);
	if (gda_query->priv) {
		switch (param_id) {
		case PROP_SERIAL_TARGET:
			break;
		case PROP_AUTO_CLEAN:
			gda_query->priv->auto_clean = g_value_get_boolean (value);
			break;
		}
	}
}

static void
gda_query_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdaQuery *gda_query;
	gda_query = GDA_QUERY (object);
	
	if (gda_query->priv) {
		switch (param_id) {
		case PROP_SERIAL_TARGET:
			g_value_set_uint (value, gda_query->priv->serial_target++);
			break;
		case PROP_SERIAL_FIELD:
			g_value_set_uint (value, gda_query->priv->serial_field++);
			break;
		case PROP_SERIAL_COND:
			g_value_set_uint (value, gda_query->priv->serial_cond++);
			break;
		case PROP_REALLY_ALL_FIELDS:
			g_value_set_pointer (value, gda_query->priv->fields);
			break;
		case PROP_AUTO_CLEAN:
			g_value_set_boolean (value, gda_query->priv->auto_clean);
			break;
		}	
	}
}

/**
 * gda_query_declare_condition
 * @query: a #GdaQuery object
 * @cond: a #GdaQueryCondition object
 *
 * Declares the existence of a new condition to @query. All the #GdaQueryCondition objects MUST
 * be declared to the corresponding #GdaQuery object for the library to work correctly.
 * Once @cond has been declared, @query does not hold any reference to @cond.
 *
 * This functions is called automatically from each gda_query_condition_new* function, and it should not be necessary 
 * to call it except for classes extending the #GdaQueryCondition class.
 */
void
gda_query_declare_condition (GdaQuery *query, GdaQueryCondition *cond)
{
	g_return_if_fail (query && GDA_IS_QUERY (query));
	g_return_if_fail (query->priv);
	g_return_if_fail (cond && GDA_IS_QUERY_CONDITION (cond));
	
	/* we don't take any reference on condition */
	if (g_slist_find (query->priv->all_conds, cond))
		return;	
	else {
		query->priv->all_conds = g_slist_append (query->priv->all_conds, cond);
		g_object_weak_ref (G_OBJECT (cond), (GWeakNotify) cond_weak_ref_notify, query);

		/* make sure the query->priv->serial_cond value is always 1 above this cond's id */
		id_cond_changed_cb (cond, query);
		g_signal_connect (G_OBJECT (cond), "numid_changed",
				  G_CALLBACK (id_cond_changed_cb), query);
	}
}

/**
 * gda_query_undeclare_condition
 * @query: a #GdaQuery object
 * @cond: a #GdaQueryCondition object
 *
 * Explicitely ask @query to forget about the existence of @cond. This function is used by the
 * #GdaQueryCondition implementation, and should not be called directly
 */
void
gda_query_undeclare_condition (GdaQuery *query, GdaQueryCondition *cond)
{
	g_return_if_fail (query && GDA_IS_QUERY (query));
	g_return_if_fail (query->priv);
	g_return_if_fail (cond && GDA_IS_QUERY_CONDITION (cond));

	cond_weak_ref_notify (query, cond);
	g_object_weak_unref (G_OBJECT (cond), (GWeakNotify) cond_weak_ref_notify, query);
}

static void
cond_weak_ref_notify (GdaQuery *query, GdaQueryCondition *cond)
{
	query->priv->all_conds = g_slist_remove (query->priv->all_conds, cond);
	g_signal_handlers_disconnect_by_func (G_OBJECT (cond),
					      G_CALLBACK (id_cond_changed_cb), query);
}

static void
id_cond_changed_cb (GdaQueryCondition *cond, GdaQuery *query)
{
	if (query->priv->serial_cond <= gda_query_object_get_int_id (GDA_QUERY_OBJECT (cond)))
		query->priv->serial_cond = gda_query_object_get_int_id (GDA_QUERY_OBJECT (cond)) + 1;
}

/*
 * This function is called whenever the structure of the query is changed (adding/removing targets, etc), and
 * make sure that the query is either in the following modes:
 * 1- the query structure is described using targets, fields, etc and its type is anything but GDA_QUERY_TYPE_NON_PARSED_SQL
 *    and query->priv->sql is NULL and query->priv->sql_exprs is NULL
 * or
 * 2- the query is described by a SQL text (in query->priv->sql), query->priv->sql_exprs may be NULL or not, and
 *    there might be some GdaQueryFieldValue query fields (to describe parameters, in which case query->priv->sql_exprs is not NULL).
 *    In this mode the query type is GDA_QUERY_TYPE_NON_PARSED_SQL.
 *
 * If the query is a GDA_QUERY_TYPE_NON_PARSED_SQL, then nothing can be done and
 * an error is returned
 */
static gboolean
query_sql_forget (GdaQuery *query, GError **error)
{
	/* return FALSE if we can't switch to non SQL mode */
	if (query->priv->query_type == GDA_QUERY_TYPE_NON_PARSED_SQL) {
		g_set_error (error, GDA_QUERY_ERROR, GDA_QUERY_STRUCTURE_ERROR, 
			     _("Can't modify the structure of a non-parsed SQL query"));
		return FALSE;
	}

	/* free SQL if any */
	if (query->priv->sql) {
		g_free (query->priv->sql);
		query->priv->sql = NULL;
	}
	if (query->priv->sql_exprs) {
		gda_delimiter_destroy (query->priv->sql_exprs);
		query->priv->sql_exprs = NULL;
	}

	return TRUE;
}

/*
 * Removes query fields which are useles: hidden fields not used by anything
 *
 * Note that this function doesn't don anything when @query's type
 * is GDA_QUERY_TYPE_NON_PARSED_SQL.
 */
static void
query_clean_junk (GdaQuery *query)
{
	GSList *list;
	
	/* while in an internal transaction, don't do anything */
	if (query->priv->internal_transaction || !query->priv->auto_clean)
		return;

	query->priv->internal_transaction++;

	if (query->priv->query_type == GDA_QUERY_TYPE_NON_PARSED_SQL)
		return;

	/* 
	 * finding which fields need to be removed
	 */
	list = query->priv->fields;
	while (list) {
		GdaQueryField *qfield = GDA_QUERY_FIELD (list->data);
		gboolean used = TRUE;

		if (! gda_query_field_is_visible (qfield)) {
			GSList *list2;

			used = FALSE;

			/* looking if field is used in WHERE clause */
			if (query->priv->cond) {
				list2 = gda_query_condition_get_ref_objects_all (query->priv->cond);
				if (g_slist_find (list2, qfield))
					used = TRUE;
				g_slist_free (list2);
			}

			/* looking if field is used in a join's condition */
			list2 = query->priv->joins_flat;
			while (list2 && !used) {
				GdaQueryCondition *cond = gda_query_join_get_condition (GDA_QUERY_JOIN (list2->data));
				if (cond) {
					GSList *list3 = gda_query_condition_get_ref_objects_all (cond);
					if (g_slist_find (list3, qfield))
						used = TRUE;
					g_slist_free (list3);
				}
				list2 = g_slist_next (list2);
			}

			/* looking for other fields using that field */
			if (!used && query->priv->sub_queries) {
				GSList *queries = query->priv->sub_queries;
				while (queries && !used) {
					list2 = gda_referer_get_ref_objects (GDA_REFERER (queries->data));
					if (g_slist_find (list2, qfield))
						used = TRUE;
					g_slist_free (list2);
					
					queries = g_slist_next (queries);
				}
			}

			list2 = query->priv->fields;
			while (list2 && !used) {
				if (list2->data != (gpointer) qfield) {
					GSList *list3 = gda_referer_get_ref_objects (GDA_REFERER (list2->data));
					if (g_slist_find (list3, qfield))
						used = TRUE;
					g_slist_free (list3);
				}
				list2 = g_slist_next (list2);
			}

			if (!used) {
#ifdef GDA_DEBUG_NO
				g_print ("Removed field %p:\n", qfield);
#endif
				gda_object_destroy (GDA_OBJECT (qfield));
				list = query->priv->fields;
			}
		}
		
		if (used)
			list = g_slist_next (list);
	}

	/* 
	 * finding which sub queries need to be removed: sub queries are used by:
	 * - GdaQueryTarget objects
	 * - GnomeDbQfSelect kind of field
	 */
	list = query->priv->sub_queries;
	while (list) {
		GSList *list2;
		gboolean used = FALSE;
		GdaQuery *subq = GDA_QUERY (list->data);
		
		/* looking if field is used in any target */
		list2 = query->priv->targets;
		while (list2 && !used) {
			GSList *list3 = gda_referer_get_ref_objects (GDA_REFERER (list2->data));
			if (g_slist_find (list3, subq))
				used = TRUE;
			g_slist_free (list3);
			list2 = g_slist_next (list2);
		}
		
		if (!used) {
			gda_object_destroy (GDA_OBJECT (subq));
			list = query->priv->sub_queries;
		}
		else
			list = g_slist_next (list);
	}

	query->priv->internal_transaction--;

	gda_object_signal_emit_changed (GDA_OBJECT (query));
}

static void
id_target_changed_cb (GdaQueryTarget *target, GdaQuery *query)
{
	if (query->priv->serial_target <= gda_query_object_get_int_id (GDA_QUERY_OBJECT (target)))
		query->priv->serial_target = gda_query_object_get_int_id (GDA_QUERY_OBJECT (target)) + 1;
}

static void
id_field_changed_cb (GdaQueryField *field, GdaQuery *query)
{
	if (query->priv->serial_field <= gda_query_object_get_int_id (GDA_QUERY_OBJECT (field))) 
		query->priv->serial_field = gda_query_object_get_int_id (GDA_QUERY_OBJECT (field)) + 1;
}

/**
 * gda_query_set_query_type
 * @query: a #GdaQuery object
 * @type: the new type of query
 *
 * Sets the type of @query
 */
void
gda_query_set_query_type (GdaQuery *query, GdaQueryType type)
{
	g_return_if_fail (query && GDA_IS_QUERY (query));
	g_return_if_fail (query->priv);

	if (query->priv->query_type != type) {
		query->priv->internal_transaction ++;
		gda_query_clean (query);
		query->priv->internal_transaction --;
		query->priv->query_type = type;
		    
#ifdef GDA_DEBUG_signal
		g_print (">> 'TYPE_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit_by_name (G_OBJECT (query), "type_changed");
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TYPE_CHANGED' from %s\n", __FUNCTION__);
#endif	
	}
}

/**
 * gda_query_get_query_type
 * @query: a #GdaQuery object
 *
 * Get the type of a query
 *
 * Returns: the type of @query
 */
GdaQueryType
gda_query_get_query_type (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), GDA_QUERY_TYPE_SELECT);
	g_return_val_if_fail (query->priv, GDA_QUERY_TYPE_SELECT);
	
	return query->priv->query_type;
}

/**
 * gda_query_get_query_type_string
 * @query: a #GdaQuery object
 *
 * Get the type of a query as a human readable string
 *
 * Returns: a string for the type of @query
 */
const gchar *
gda_query_get_query_type_string  (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);

	switch (query->priv->query_type) {
	case GDA_QUERY_TYPE_SELECT:
		return _("Select");
	case GDA_QUERY_TYPE_INSERT:
		return _("Insert");
	case GDA_QUERY_TYPE_UPDATE:
		return _("Update");
	case GDA_QUERY_TYPE_DELETE:
		return _("Delete");
	case GDA_QUERY_TYPE_UNION:
		return _("Select (union)");
	case GDA_QUERY_TYPE_INTERSECT:
		return _("Select (intersection)");
	case GDA_QUERY_TYPE_EXCEPT:
		return _("Select (exception)");
	case GDA_QUERY_TYPE_NON_PARSED_SQL:
		return _("SQL text");
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

/**
 * gda_query_is_select_query
 * @query: a # GdaQuery object
 *
 * Tells if @query is a SELECTION query (a simple SELECT, UNION, INTERSECT or EXCEPT);
 *
 * Returns: TRUE if @query is a selection query
 */
gboolean
gda_query_is_select_query (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), FALSE);
	g_return_val_if_fail (query->priv, FALSE);

	if ((query->priv->query_type == GDA_QUERY_TYPE_SELECT) ||
	    (query->priv->query_type == GDA_QUERY_TYPE_UNION) ||
	    (query->priv->query_type == GDA_QUERY_TYPE_INTERSECT) ||
	    (query->priv->query_type == GDA_QUERY_TYPE_EXCEPT))
		return TRUE;
	else {
		if ((query->priv->query_type == GDA_QUERY_TYPE_NON_PARSED_SQL) &&
		    ! g_ascii_strncasecmp (query->priv->sql, "select ", 7))
			return TRUE;
		else
			return FALSE;
	}
}

/**
 * gda_query_is_insert_query
 * @query: a # GdaQuery object
 *
 * Tells if @query is a INSERT query.
 *
 * Returns: TRUE if @query is an insertion query
 */
gboolean
gda_query_is_insert_query (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), FALSE);
	g_return_val_if_fail (query->priv, FALSE);

	if ((query->priv->query_type == GDA_QUERY_TYPE_INSERT))
		return TRUE;
	else {
		if ((query->priv->query_type == GDA_QUERY_TYPE_NON_PARSED_SQL) &&
		    ! g_ascii_strncasecmp (query->priv->sql, "insert into ", 12))
			return TRUE;
		else
			return FALSE;
	}
}

/**
 * gda_query_is_update_query
 * @query: a # GdaQuery object
 *
 * Tells if @query is a UPDATE query.
 *
 * Returns: TRUE if @query is an update query
 */
gboolean
gda_query_is_update_query (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), FALSE);
	g_return_val_if_fail (query->priv, FALSE);

	if ((query->priv->query_type == GDA_QUERY_TYPE_UPDATE))
		return TRUE;
	else {
		if ((query->priv->query_type == GDA_QUERY_TYPE_NON_PARSED_SQL) &&
		    ! g_ascii_strncasecmp (query->priv->sql, "update ", 7))
			return TRUE;
		else
			return FALSE;
	}
}

/**
 * gda_query_is_delete_query
 * @query: a # GdaQuery object
 *
 * Tells if @query is a DELETE query.
 *
 * Returns: TRUE if @query is an delete query
 */
gboolean
gda_query_is_delete_query (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), FALSE);
	g_return_val_if_fail (query->priv, FALSE);

	if ((query->priv->query_type == GDA_QUERY_TYPE_DELETE))
		return TRUE;
	else {
		if ((query->priv->query_type == GDA_QUERY_TYPE_NON_PARSED_SQL) &&
		    ! g_ascii_strncasecmp (query->priv->sql, "delete from ", 12))
			return TRUE;
		else
			return FALSE;
	}
}

/**
 * gda_query_is_modify_query
 * @query: a # GdaQuery object
 *
 * Tells if @query is a modification query (a simple UPDATE, DELETE, INSERT).; pure SQL
 * queries are not handled and will always return FALSE.
 *
 * Returns: TRUE if @query is a modification query
 */
gboolean
gda_query_is_modify_query (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), FALSE);
	g_return_val_if_fail (query->priv, FALSE);

	if (gda_query_is_delete_query (query) ||
	    gda_query_is_insert_query (query) ||
	    gda_query_is_update_query (query))
		return TRUE;
	else
		return FALSE;
}


/**
* gda_query_set_sql_text
* @query: a # GdaQuery object
* @sql: the SQL statement
* @error: location to store parsing error, or %NULL
*
* Defines @query's contents from an SQL statement. The SQL text is parsed and the internal query structured
* is built from that; the query type is also set. If the SQL text cannot be parsed, then the internal structure
* of the query is emptied and the query type is set to GDA_QUERY_TYPE_NON_PARSED_SQL.
*
* To be parsed successfully, the expected SQL must respect the SQL standard; some extensions have been
* added to be able to define variables within the SQL statement. See the introduction to the #GdaQuery
* for more information.
*/
void
gda_query_set_sql_text (GdaQuery *query, const gchar *sql, GError **error)
{
	sql_statement *result;
	gboolean err = FALSE;
	GError *local_error = NULL; /* we don't care about the error, but we don't want any output to stderr */

	g_return_if_fail (query && GDA_IS_QUERY (query));
	g_return_if_fail (query->priv);

	if (query->priv->sql) {
		g_free (query->priv->sql);
		query->priv->sql = NULL;
	}
	if (query->priv->sql_exprs) {
		gda_delimiter_destroy (query->priv->sql_exprs);
		query->priv->sql_exprs = NULL;
	}

	/* start internal transaction */
	query->priv->internal_transaction ++;

	/* try to parse the SQL */
	result = sql_parse_with_error (sql, error ? error : &local_error);
	if (!error && local_error) {
		g_error_free (local_error);
		local_error = NULL;
	}
	if (result) {
#ifdef GDA_DEBUG_NO
		sql_display (result);
#endif
		switch (result->type) {
		case SQL_select: 
			err = !parsed_create_select_query (query, (sql_select_statement *) result->statement, error);
			break;
		case SQL_insert:
			err = !parsed_create_insert_query (query, (sql_insert_statement *) result->statement, error);
			break;
		case SQL_delete:
			err = !parsed_create_delete_query (query, (sql_delete_statement *) result->statement, error);
			break;
		case SQL_update:
			err = !parsed_create_update_query (query, (sql_update_statement *) result->statement, error);
			break;
		}
		sql_destroy (result);
	}
	else
		err = TRUE;

	if (err) {
		GList *stm_list;
		GdaDelimiterStatement *stm;

		/* the query can't be parsed with libgda's parser */
		if (error && !(*error))
			g_set_error (error, GDA_QUERY_ERROR, GDA_QUERY_PARSE_ERROR,
				     _("Error during query parsing (%s)"), sql);
		gda_query_clean (query); /* force cleaning */
		gda_query_set_query_type (query, GDA_QUERY_TYPE_NON_PARSED_SQL);

		/* try the internal parser just to delimit the expressions and find the required parameters,
		* and check that we have everything we need to define a valid parameter.
		* If everything is OK, then create #GdaQueryFieldValue internal fields for each required
		* parameter.
		*/
		stm_list = gda_delimiter_parse_with_error (sql, &local_error);
		if (stm_list) {
			GList *params;
			GdaDict *dict = gda_object_get_dict (GDA_OBJECT (query));

			stm = gda_delimiter_concat_list (stm_list);
			gda_delimiter_free_list (stm_list);

			/*
			g_print ("DELIMITED: %s\n", sql);
			gda_delimiter_display (stm);
			*/
			params = stm->params_specs;
			while (params) {
				GdaDictType *dtype = NULL;
				GType gtype = G_TYPE_INVALID;
				GList *pspecs = (GList *)(params->data);
				
				while (pspecs && !dtype && (gtype == G_TYPE_INVALID)) {
					if (GDA_DELIMITER_PARAM_SPEC (pspecs->data)->type == GDA_DELIMITER_PARAM_TYPE) {
						dtype = gda_dict_get_dict_type_by_name (dict,
							      GDA_DELIMITER_PARAM_SPEC (pspecs->data)->content);
						if (!dtype) 
							gtype = gda_g_type_from_string (GDA_DELIMITER_PARAM_SPEC (pspecs->data)->content);
						else
							gtype = gda_dict_type_get_g_type (dtype);
					}
					pspecs = g_list_next (pspecs);
				}
				
				if (gtype == G_TYPE_INVALID) {
					if (error && !(*error))
						g_set_error (error, GDA_QUERY_ERROR, 
							     GDA_QUERY_PARAM_TYPE_ERROR,
							     _("No valid type specified for parameter, using gchararray"));
					gtype = G_TYPE_STRING;
				}

				GdaQueryField *field;
				
				/* create the #GdaQueryFieldValue fields */
				field = GDA_QUERY_FIELD (gda_query_field_value_new (query, gtype));
				if (dtype)
					gda_entity_field_set_dict_type (GDA_ENTITY_FIELD (field), dtype);
				gda_query_field_set_internal (field, TRUE);
				gda_query_field_set_visible (field, FALSE);
				gda_entity_add_field (GDA_ENTITY (query), GDA_ENTITY_FIELD (field));
				g_object_set_data (G_OBJECT (field), "pspeclist", params->data);
				g_object_unref (field);
				
				gda_query_field_value_set_is_parameter (GDA_QUERY_FIELD_VALUE (field), TRUE);
				
				pspecs = (GList *) (params->data);
				while (pspecs) {
					GdaDelimiterParamSpec *ps = GDA_DELIMITER_PARAM_SPEC (pspecs->data);
					switch (ps->type) {
					case GDA_DELIMITER_PARAM_NAME:
						gda_object_set_name (GDA_OBJECT (field), ps->content);
						break;
					case GDA_DELIMITER_PARAM_DESCR:
						gda_object_set_description (GDA_OBJECT (field), ps->content);
						break;
					case GDA_DELIMITER_PARAM_NULLOK:
						gda_query_field_value_set_not_null (GDA_QUERY_FIELD_VALUE (field),
										    (*(ps->content) == 'f') ||
										    (*(ps->content) == 'F') ? 
										    TRUE : FALSE);
						break;
					case GDA_DELIMITER_PARAM_ISPARAM:
						gda_query_field_value_set_is_parameter (GDA_QUERY_FIELD_VALUE (field), 
											(*(ps->content) == 'f') || 
											(*(ps->content) == 'F') ? 
											FALSE : TRUE);
						break;
					case GDA_DELIMITER_PARAM_TYPE:
						g_object_set (G_OBJECT (field), "string_type", ps->content, NULL);
						break;
					case GDA_DELIMITER_PARAM_DEFAULT: {
						GValue *defval = NULL;
						GdaDataHandler *dh;
						
						dh = gda_dict_get_handler (dict, gtype);
						if (dh) 
							defval = gda_data_handler_get_value_from_sql (dh,
												      ps->content, gtype);
						if (!defval) {
							dh = gda_dict_get_handler (dict, G_TYPE_STRING);
							defval = gda_data_handler_get_value_from_sql (dh,
												      ps->content, G_TYPE_STRING);
							if (!defval)
								defval = gda_data_handler_get_value_from_str (
													      dh, ps->content, G_TYPE_STRING);
						}
						gda_query_field_value_set_default_value (GDA_QUERY_FIELD_VALUE (field), 
											 defval);
						gda_query_field_value_set_value (GDA_QUERY_FIELD_VALUE (field), 
										 defval);
						gda_value_free (defval);
						break;
					}
					}
					pspecs = g_list_next (pspecs);
				}
				params = g_list_next (params);
			}

			query->priv->sql_exprs = stm;
		}
		else {
			/*g_warning ("NOT DELIMITED: %s (ERR: %s)", sql, 
			  local_error && local_error->message ? local_error->message : "unknown");*/
			if (local_error)
				g_error_free (local_error);
		}
	}

	/* copy SQL */
	query->priv->sql = g_strdup (sql);

	/* close internal transaction */	
	query->priv->internal_transaction --;
	
	gda_object_signal_emit_changed (GDA_OBJECT (query));
}

/**
 * gda_query_get_sql_text
 * @query: a #GdaQuery object
 *
 * Obtain a new string representing the SQL version of the query.
 *
 * WARNING: the returned SQL statement may contain some extensions which allow for the definition of
 * variables (see the introduction to the #GdaQuery for more information). As such the returned SQL cannot
 * be executed as it may provoque errors. To get an executable statement, use the #GdaRenderer interface's
 * methods.
 *
 * Returns: the new string
 */
const gchar *
gda_query_get_sql_text (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);

	return gda_query_render_as_sql (GDA_RENDERER (query), NULL, NULL, GDA_RENDERER_PARAMS_AS_DETAILED, NULL);
}





/**
 * gda_query_get_parent_query
 * @query: a #GdaQuery object
 *
 * Get the parent query of @query
 *
 * Returns: the parent query, or NULL if @query does not have any parent
 */
GdaQuery *
gda_query_get_parent_query (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);

	return query->priv->parent_query;
}

/**
 * gda_query_get_field_by_ref_field
 * @query: a #GdaQuery object
 * @target: a #GdaQueryTarget, or %NULL
 * @ref_field: a #GdaEntityField object
 * @field_state: tells about the status of the requested field, see #GdaQueryFieldState
 *
 * Finds the first #GdaQueryField object in @query which represents @ref_field.
 * The returned object will be a #GdaQueryFieldField object which represents @ref_field.
 *
 * If @target is specified, then the returned field will be linked to that #GdaQueryTarget object.
 *
 * Returns: a #GdaQueryFieldField object or %NULL
 */
GdaQueryField *
gda_query_get_field_by_ref_field (GdaQuery *query, GdaQueryTarget *target, GdaEntityField *ref_field, GdaQueryFieldState field_state)
{
	GdaQueryField *field = NULL;
	GSList *list;
	
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	if (target)
		g_return_val_if_fail (GDA_IS_QUERY_TARGET (target), NULL);

	list = query->priv->fields;
	while (list && !field) {
		if (GDA_IS_QUERY_FIELD_FIELD (list->data) &&
		    (gda_query_field_field_get_ref_field (GDA_QUERY_FIELD_FIELD (list->data)) == ref_field) &&
		    (!target || (gda_query_field_field_get_target (GDA_QUERY_FIELD_FIELD (list->data)) == target))) {
			if (((field_state & GDA_ENTITY_FIELD_ANY) == GDA_ENTITY_FIELD_ANY) ||
			    ((field_state & GDA_ENTITY_FIELD_VISIBLE) && gda_query_field_is_visible (list->data)) ||
			    ((field_state & GDA_ENTITY_FIELD_INVISIBLE) && !gda_query_field_is_visible (list->data)))
				field = GDA_QUERY_FIELD (list->data);
		}
		list = g_slist_next (list);
	}

	return field;
}

/**
 * gda_query_get_first_field_for_target
 * @query: a #GdaQuery object
 * @target:
 *
 * Finds the first occurence of a #GdaQueryFieldField object whose target is @target in @query
 *
 * Returns: the requested field, or %NULL
 */
GdaQueryField *
gda_query_get_first_field_for_target (GdaQuery *query, GdaQueryTarget *target)
{
	GdaQueryField *retval = NULL;
	GSList *list;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	g_return_val_if_fail (!target || g_slist_find (query->priv->targets, target), NULL);

	list = query->priv->fields;
	while (list && !retval) {
		GdaQueryField *field = GDA_QUERY_FIELD (list->data);
		if (gda_query_field_is_visible (field) &&
		    GDA_IS_QUERY_FIELD_FIELD (field) &&
		    (gda_query_field_field_get_target (GDA_QUERY_FIELD_FIELD (field)) == target))
			retval = field;
		list = g_slist_next (list);
	}

	return retval;
}

/**
 * gda_query_get_sub_queries
 * @query: a #GdaQuery object
 *
 * Get a list of all the sub-queries managed by @query
 *
 * Returns: a new list of the sub-queries
 */
GSList *
gda_query_get_sub_queries (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);

	if (query->priv->sub_queries)
		return g_slist_copy (query->priv->sub_queries);
	else
		return NULL;
}

/**
 * gda_query_add_param_source
 * @query: a #GdaQuery object
 * @param_source: a #GdaDataModel object
 *
 * Tells @query that @param_source is a query which potentially will constraint the possible values
 * of one or more of @query's parameters. This implies that @query keeps a reference on @param_source.
 */
void 
gda_query_add_param_source (GdaQuery *query, GdaDataModel *param_source)
{
	g_return_if_fail (GDA_IS_QUERY (query));
        g_return_if_fail (query->priv);
	g_return_if_fail (GDA_IS_DATA_MODEL (param_source));

	if (g_slist_find (query->priv->param_sources, param_source))
		return;

	query->priv->param_sources = g_slist_append (query->priv->param_sources, param_source);
	g_object_ref (G_OBJECT (param_source));

	gda_object_connect_destroy (param_source, 
				    G_CALLBACK (destroyed_param_source_cb), query);
}

static void
destroyed_param_source_cb (GdaDataModel *param_source, GdaQuery *query)
{
	g_assert (g_slist_find (query->priv->param_sources, param_source));

        query->priv->param_sources = g_slist_remove (query->priv->param_sources, param_source);
        g_signal_handlers_disconnect_by_func (G_OBJECT (param_source),
                                              G_CALLBACK (destroyed_param_source_cb), query);

        g_object_unref (param_source);
}

/**
 * gda_query_del_param_source
 * @query: a #GdaQuery object
 * @param_source: a #GdaDataModel object
 *
 * Tells @query that it should no longer take care of @param_source.
 * The parameters which depend on @param_source will still depend on it, though.
 */
void
gda_query_del_param_source (GdaQuery *query, GdaDataModel *param_source)
{
	g_return_if_fail (GDA_IS_QUERY (query));
        g_return_if_fail (query->priv);
	g_return_if_fail (GDA_IS_DATA_MODEL (param_source));
	g_return_if_fail (g_slist_find (query->priv->param_sources, param_source));

	destroyed_param_source_cb (param_source, query);
}

/**
 * gda_query_get_param_sources
 * @query: a #GdaQuery object
 *
 * Get a list of the parameter source queries that are references as such by @query.
 *
 * Returns: the list of #GdaQuery objects
 */
const GSList *
gda_query_get_param_sources (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
        g_return_val_if_fail (query->priv, NULL);

	return query->priv->param_sources;
}


/**
 * gda_query_add_sub_query
 * @query: a #GdaQuery object
 * @sub_query: a #GdaQuery object
 *
 * Add @sub_query to @query. Sub queries are managed by their parent query, and as such they
 * are destroyed when their parent query is destroyed.
 */
void
gda_query_add_sub_query (GdaQuery *query, GdaQuery *sub_query)
{
	g_return_if_fail (query && GDA_IS_QUERY (query));
        g_return_if_fail (query->priv);
	g_return_if_fail (sub_query && GDA_IS_QUERY (sub_query));
	g_return_if_fail (sub_query->priv);
        g_return_if_fail (!g_slist_find (query->priv->sub_queries, sub_query));

	query->priv->sub_queries = g_slist_append (query->priv->sub_queries, sub_query);
	change_parent_query (sub_query, query);
	g_object_ref (G_OBJECT (sub_query));

	gda_object_connect_destroy (sub_query, 
				 G_CALLBACK (destroyed_sub_query_cb), query);
        g_signal_connect (G_OBJECT (sub_query), "changed",
                          G_CALLBACK (changed_sub_query_cb), query);

#ifdef GDA_DEBUG_signal
        g_print (">> 'SUB_QUERY_ADDED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "sub_query_added", sub_query);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'SUB_QUERY_ADDED' from %s\n", __FUNCTION__);
#endif	
}

static void
destroyed_sub_query_cb (GdaQuery *sub_query, GdaQuery *query)
{
	g_assert (g_slist_find (query->priv->sub_queries, sub_query));

        query->priv->sub_queries = g_slist_remove (query->priv->sub_queries, sub_query);
        g_signal_handlers_disconnect_by_func (G_OBJECT (sub_query),
                                              G_CALLBACK (destroyed_sub_query_cb), query);
        g_signal_handlers_disconnect_by_func (G_OBJECT (sub_query),
                                              G_CALLBACK (changed_sub_query_cb), query);

	query->priv->internal_transaction ++;
#ifdef GDA_DEBUG_signal
        g_print (">> 'SUB_QUERY_REMOVED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "sub_query_removed", sub_query);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'SUB_QUERY_REMOVED' from %s\n", __FUNCTION__);
#endif
	query->priv->internal_transaction --;

        g_object_unref (sub_query);

	/* cleaning... */
	query_clean_junk (query);
}

static void
changed_sub_query_cb (GdaQuery *sub_query, GdaQuery *query)
{
#ifdef GDA_DEBUG_signal
        g_print (">> 'SUB_QUERY_UPDATED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "sub_query_updated", sub_query);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'SUB_QUERY_UPDATED' from %s\n", __FUNCTION__);
#endif
}

/**
 * gda_query_del_sub_query
 * @query: a #GdaQuery object
 * @sub_query: a #GdaQuery object
 *
 * Removes @sub_query from @query. @sub_query MUST be present within @query.
 */
void
gda_query_del_sub_query (GdaQuery *query, GdaQuery *sub_query)
{
	g_return_if_fail (query && GDA_IS_QUERY (query));
        g_return_if_fail (query->priv);
	g_return_if_fail (sub_query && GDA_IS_QUERY (sub_query));
	g_return_if_fail (g_slist_find (query->priv->sub_queries, sub_query));

	destroyed_sub_query_cb (sub_query, query);
}

static void
change_parent_query (GdaQuery *query, GdaQuery *parent_query)
{
	GdaDict *dict;
	g_return_if_fail (query && GDA_IS_QUERY (query));
	g_return_if_fail (query->priv);

	dict = gda_object_get_dict (GDA_OBJECT (query));

	/* if there was a parent query, then we break that link and give the query to the GdaDict */
	if (query->priv->parent_query) {
		/* REM: we don't remove 'query' from 'parent_query' since that already has been done */
		g_signal_handlers_disconnect_by_func (G_OBJECT (query->priv->parent_query),
						      G_CALLBACK (destroyed_parent_query), query);
		query->priv->parent_query = NULL;
	}
	
	if (parent_query) {
		g_return_if_fail (GDA_IS_QUERY (parent_query));
		query->priv->parent_query = parent_query;
		gda_object_connect_destroy (parent_query,
					 G_CALLBACK (destroyed_parent_query), query);
	}
}

static void
destroyed_parent_query (GdaQuery *parent_query, GdaQuery *query)
{
	gda_object_destroy (GDA_OBJECT (query));
}







/**
 * gda_query_get_targets
 * @query: a #GdaQuery object
 *
 * Get a list of all the targets used in @query
 *
 * Returns: a new list of the targets
 */
GSList *
gda_query_get_targets (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);

	if (query->priv->targets)
		return g_slist_copy (query->priv->targets);
	else
		return NULL;
}

static void gda_query_assign_targets_aliases (GdaQuery *query);

/**
 * gda_query_add_target
 * @query: a #GdaQuery object
 * @target: a #GdaQueryTarget to add to @query
 * @error: location to store error, or %NULL
 *
 * Adds a target to @query. A target represents a entity (it can actually be a table,
 * a view, or another query) which @query will use. 
 *
 * For a SELECT query, the targets appear
 * after the FROM clause. The targets can be joined two by two using #GdaQueryJoin objects
 *
 * For UPDATE, DELETE or INSERT queries, there can be only ONE #GdaQueryTarget object which is
 * the one where the data modifications are performed.
 *
 * For UNION and INTERSECT queries, there is no possible #GdaQueryTarget object.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_query_add_target (GdaQuery *query, GdaQueryTarget *target, GError **error)
{
	GdaEntity *ent;
	const gchar *str;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), FALSE);
        g_return_val_if_fail (query->priv, FALSE);
	g_return_val_if_fail (query_sql_forget (query, error), FALSE);
        g_return_val_if_fail (target && GDA_IS_QUERY_TARGET (target), FALSE);
        g_return_val_if_fail (!g_slist_find (query->priv->targets, target), FALSE);
	g_return_val_if_fail (gda_query_target_get_query (target) == query, FALSE);

	/* if target represents another GdaQuery, then make sure that other query is a sub query of @query */
	ent = gda_query_target_get_represented_entity (target);
	if (ent && GDA_IS_QUERY (ent)) {
		if ((gda_query_get_parent_query (GDA_QUERY (ent)) != query) ||
		    !g_slist_find (query->priv->sub_queries, ent)) {
			g_set_error (error,
				     GDA_QUERY_ERROR,
				     GDA_QUERY_TARGETS_ERROR,
				     _("The query represented by a target must be a sub-query of the current query"));
			return FALSE;
		}
	}

	/* specific tests */
	switch (query->priv->query_type) {
	case GDA_QUERY_TYPE_INSERT:
	case GDA_QUERY_TYPE_UPDATE:
	case GDA_QUERY_TYPE_DELETE:
		/* if there is already one target, then refuse to add another one */
		if (query->priv->targets) {
			g_set_error (error,
				     GDA_QUERY_ERROR,
				     GDA_QUERY_TARGETS_ERROR,
				     _("Queries which update data can only have one target"));
			return FALSE;
		}
		break;
	case GDA_QUERY_TYPE_UNION:
	case GDA_QUERY_TYPE_INTERSECT:
	case GDA_QUERY_TYPE_EXCEPT:
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_TARGETS_ERROR,
			     _("Aggregation queries may only have sub-queries, not targets."));
		return FALSE;
		break;
	default:
	case GDA_QUERY_TYPE_SELECT:
	case GDA_QUERY_TYPE_NON_PARSED_SQL:
		/* no specific test to be done */
		break;
	}

	query->priv->targets = g_slist_append (query->priv->targets, target);
	g_object_ref (G_OBJECT (target));
	gda_object_connect_destroy (target, 
				 G_CALLBACK (destroyed_target_cb), query);
        g_signal_connect (G_OBJECT (target), "changed",
                          G_CALLBACK (changed_target_cb), query);
        g_signal_connect (G_OBJECT (target), "numid_changed",
                          G_CALLBACK (id_target_changed_cb), query);

	gda_query_assign_targets_aliases (query);

	/* give the target a correct name */
	str = gda_object_get_name (GDA_OBJECT (target));
	if ((!str || !(*str)) && ent) {
		str = gda_object_get_name (GDA_OBJECT (ent));
		if (str && *str)
			gda_object_set_name (GDA_OBJECT (target), str);
	}

	if (query->priv->serial_target <= gda_query_object_get_int_id (GDA_QUERY_OBJECT (target)))
		query->priv->serial_target = gda_query_object_get_int_id (GDA_QUERY_OBJECT (target)) + 1;

#ifdef GDA_DEBUG_signal
        g_print (">> 'TARGET_ADDED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "target_added", target);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'TARGET_ADDED' from %s\n", __FUNCTION__);
#endif
	return TRUE;
}

static void
destroyed_target_cb (GdaQueryTarget *target, GdaQuery *query)
{
	/* REM: when a GdaQueryTarget object is destroyed, the GdaQueryJoin objects using it are 
	   also destroyed, so we don't need to take care of them here */
	g_assert (g_slist_find (query->priv->targets, target));

        query->priv->targets = g_slist_remove (query->priv->targets, target);
        g_signal_handlers_disconnect_by_func (G_OBJECT (target),
                                              G_CALLBACK (destroyed_target_cb), query);
        g_signal_handlers_disconnect_by_func (G_OBJECT (target),
                                              G_CALLBACK (changed_target_cb), query);
        g_signal_handlers_disconnect_by_func (G_OBJECT (target),
                                              G_CALLBACK (id_target_changed_cb), query);

	query->priv->internal_transaction ++;
#ifdef GDA_DEBUG_signal
        g_print (">> 'TARGET_REMOVED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "target_removed", target);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'TARGET_REMOVED' from %s\n", __FUNCTION__);
#endif
	query->priv->internal_transaction --;

        g_object_unref (target);
	gda_query_assign_targets_aliases (query);

	/* cleaning... */
	query_clean_junk (query);
}

static void
changed_target_cb (GdaQueryTarget *target, GdaQuery *query)
{
#ifdef GDA_DEBUG_signal
        g_print (">> 'TARGET_UPDATED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "target_updated", target);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'TARGET_UPDATED' from %s\n", __FUNCTION__);
#endif
}

static void
gda_query_assign_targets_aliases (GdaQuery *query)
{
	/* FIXME: add a targets assigning policy here; otherwise targets assign themselves the aliases as T<id>.
	 * The problem is with targets referencing other queries where there is a potential alias clash if the policy
	 * does not take care of the other queries
	 */
}

/**
 * gda_query_del_target
 * @query: a #GdaQuery object
 * @target: a #GdaQueryTarget object
 *
 * Removes @target from @query. @target MUST be present within @query. Warning:
 * All the joins and fields which depended on @target are also removed.
 */
void
gda_query_del_target (GdaQuery *query, GdaQueryTarget *target)
{
	g_return_if_fail (query && GDA_IS_QUERY (query));
        g_return_if_fail (query->priv);
	g_return_if_fail (query_sql_forget (query, NULL));
	g_return_if_fail (target && GDA_IS_QUERY_TARGET (target));
	g_return_if_fail (g_slist_find (query->priv->targets, target));

	destroyed_target_cb (target, query);
}


/**
 * gda_query_get_target_by_xml_id
 * @query: a #GdaQuery object
 * @xml_id: the XML Id of the requested #GdaQueryTarget object
 *
 * Get a pointer to a #GdaQueryTarget (which must be within @query) using
 * its XML Id
 *
 * Returns: the #GdaQueryTarget object, or NULL if not found
 */
GdaQueryTarget *
gda_query_get_target_by_xml_id (GdaQuery *query, const gchar *xml_id)
{
	GdaQueryTarget *target = NULL;
	GSList *list;
	gchar *str;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);

	list = query->priv->targets;
	while (list && !target) {
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
		if (!strcmp (str, xml_id))
			target = GDA_QUERY_TARGET (list->data);
		g_free (str);
		list = g_slist_next (list);
	}

	return target;
}

/**
 * gda_query_get_target_by_alias
 * @query: a #GdaQuery object
 * @alias_or_name: the alias or name
 *
 * Get a pointer to a #GdaQueryTarget (which must be within @query) using
 * its alias (if not found then @alias_or_name is interpreted as the target name)
 *
 * Returns: the #GdaQueryTarget object, or NULL if not found
 */
GdaQueryTarget *
gda_query_get_target_by_alias (GdaQuery *query, const gchar *alias_or_name)
{
	GdaQueryTarget *target = NULL;
	GSList *list;
	const gchar *str;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	g_return_val_if_fail (alias_or_name && *alias_or_name, NULL);

	list = query->priv->targets;
	while (list && !target) {
		str = gda_query_target_get_alias (GDA_QUERY_TARGET (list->data));
		if (str && !strcmp (str, alias_or_name))
			target = GDA_QUERY_TARGET (list->data);
		list = g_slist_next (list);
	}

	if (!target) {
		list = query->priv->targets;
		while (list && !target) {
			str = gda_object_get_name (GDA_OBJECT (list->data));
			if (str && !strcmp (str, alias_or_name))
				target = GDA_QUERY_TARGET (list->data);
			list = g_slist_next (list);
		}
	}

	return target;
}

/**
 * gda_query_get_target_pkfields
 * @query: a #GdaQuery object
 * @target: a #GdaQueryTarget object
 *
 * Makes a list of the #GdaQueryField objects which represent primary key fields of
 * the entity represented by @target.
 *
 * If the entity represented by @target does not have any primary key, or if the 
 * primary key's fields are not present in @query, then the returned value is %NULL.
 *
 * Returns: a new GSList, or %NULL.
 */
GSList *
gda_query_get_target_pkfields (GdaQuery *query, GdaQueryTarget *target)
{
	GdaEntity *entity;
	GSList *pk_fields = NULL;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	g_return_val_if_fail (target && GDA_IS_QUERY_TARGET (target), NULL);
	g_return_val_if_fail (g_slist_find (query->priv->targets, target), NULL);

	entity = gda_query_target_get_represented_entity (target);
	if (GDA_IS_DICT_TABLE (entity)) {
		GdaDictConstraint *pkcons;
		gboolean allthere = TRUE;
		GSList *cons_pk_fields, *flist;
		
		pkcons = gda_dict_table_get_pk_constraint (GDA_DICT_TABLE (entity));
		if (pkcons) {
			cons_pk_fields = gda_dict_constraint_pkey_get_fields (pkcons);
			flist = cons_pk_fields;
			while (flist && allthere) {
				GdaQueryField *field;
				
				field = gda_query_get_field_by_ref_field (query, target, flist->data, GDA_ENTITY_FIELD_VISIBLE);
				if (field)
					pk_fields = g_slist_append (pk_fields, field);
				else
					allthere = FALSE;
				flist = g_slist_next (flist);
			}
			g_slist_free (cons_pk_fields);

			if (!allthere) {
				g_slist_free (pk_fields);
				pk_fields = NULL;
			}
		}
	}
	else {
		/* not yet possible at the moment... */
		TO_IMPLEMENT;
	}

	return pk_fields;
}


/**
 * gda_query_get_joins
 * @query: a #GdaQuery object
 *
 * Get a list of all the joins used in @query
 *
 * Returns: a new list of the joins
 */
GSList *
gda_query_get_joins (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);

	if (query->priv->joins_flat)
		return g_slist_copy (query->priv->joins_flat);
	else
		return NULL;
}

/**
 * gda_query_get_join_by_targets
 * @query: a #GdaQuery object
 * @target1: a #GdaQueryTarget object
 * @target2: a #GdaQueryTarget object
 *
 * Find a join in @query which joins the @target1 and @target2 targets
 *
 * Returns: the #GdaQueryJoin object, or %NULL
 */
GdaQueryJoin *
gda_query_get_join_by_targets (GdaQuery *query, GdaQueryTarget *target1, GdaQueryTarget *target2)
{
	GdaQueryJoin *join = NULL;
	GSList *joins;
	GdaQueryTarget *lt1, *lt2;
	
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	g_return_val_if_fail (target1 && GDA_IS_QUERY_TARGET (target1), NULL);
	g_return_val_if_fail (gda_query_target_get_query (target1) == query, NULL);
	g_return_val_if_fail (target2 && GDA_IS_QUERY_TARGET (target2), NULL);
	g_return_val_if_fail (gda_query_target_get_query (target2) == query, NULL);

	joins = query->priv->joins_flat;
	while (joins && !join) {
		lt1 = gda_query_join_get_target_1 (GDA_QUERY_JOIN (joins->data));
		lt2 = gda_query_join_get_target_2 (GDA_QUERY_JOIN (joins->data));
		if (((lt1 == target1) && (lt2 == target2)) ||
		    ((lt1 == target2) && (lt2 == target1)))
			join = GDA_QUERY_JOIN (joins->data);

		joins = g_slist_next (joins);
	}

	return join;
}

static gboolean gda_query_are_joins_active (GdaQuery *query);
#ifdef GDA_DEBUG_0
static void joins_pack_dump (GdaQuery *query);
#endif
static gboolean joins_pack_add_join (GdaQuery *query, GdaQueryJoin *join);
static void joins_pack_del_join (GdaQuery *query, GdaQueryJoin *join);

/**
 * gda_query_add_join
 * @query: a #GdaQuery object
 * @join : a #GdaQueryJoin object
 *
 * Add a join to @query. A join is defined by the two #GdaQueryTarget objects it joins and by
 * a join condition which MUST ONLY make use of fields of the two entities represented by the
 * targets.
 *
 * For any given couple of #GdaQueryTarget objects, there can exist ONLY ONE #GdaQueryJoin which joins the
 * two.
 *
 * Returns: TRUE on success, and FALSE otherwise
 */
gboolean
gda_query_add_join (GdaQuery *query, GdaQueryJoin *join)
{
	GSList *joins;
	GdaQueryTarget *t1, *t2, *lt1, *lt2;
	gboolean already_exists = FALSE;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), FALSE);
        g_return_val_if_fail (query->priv, FALSE);
	g_return_val_if_fail (query_sql_forget (query, NULL), FALSE);
        g_return_val_if_fail (join && GDA_IS_QUERY_JOIN (join), FALSE);
        g_return_val_if_fail (!g_slist_find (query->priv->joins_flat, join), FALSE);
	g_return_val_if_fail (gda_query_join_get_query (join) == query, FALSE);
	g_return_val_if_fail (gda_referer_is_active (GDA_REFERER (join)), FALSE);
	g_return_val_if_fail (gda_query_are_joins_active (query), FALSE);

	/* make sure there is not yet another join for the couple of #GdaQueryTarget objects
	   used by 'join' */
	t1 = gda_query_join_get_target_1 (join);
	t2 = gda_query_join_get_target_2 (join);

	joins = query->priv->joins_flat;
	while (joins && !already_exists) {
		lt1 = gda_query_join_get_target_1 (GDA_QUERY_JOIN (joins->data));
		lt2 = gda_query_join_get_target_2 (GDA_QUERY_JOIN (joins->data));
		if (((lt1 == t1) && (lt2 == t2)) ||
		    ((lt1 == t2) && (lt2 == t1)))
			already_exists = TRUE;

		joins = g_slist_next (joins);
	}
	g_return_val_if_fail (!already_exists, FALSE);

	g_return_val_if_fail (joins_pack_add_join (query, join), FALSE);
	query->priv->joins_flat = g_slist_append (query->priv->joins_flat, join);
	g_object_ref (G_OBJECT (join));
	gda_object_connect_destroy (join,
				 G_CALLBACK (destroyed_join_cb), query);
        g_signal_connect (G_OBJECT (join), "changed",
                          G_CALLBACK (changed_join_cb), query);

#ifdef GDA_DEBUG_signal
        g_print (">> 'JOIN_ADDED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "join_added", join);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'JOIN_ADDED' from %s\n", __FUNCTION__);
#endif	
	return TRUE;
}

static void
destroyed_join_cb (GdaQueryJoin *join, GdaQuery *query)
{
	g_assert (g_slist_find (query->priv->joins_flat, join));

        query->priv->joins_flat = g_slist_remove (query->priv->joins_flat, join);
	joins_pack_del_join (query, join);

        g_signal_handlers_disconnect_by_func (G_OBJECT (join),
                                              G_CALLBACK (destroyed_join_cb), query);
        g_signal_handlers_disconnect_by_func (G_OBJECT (join),
                                              G_CALLBACK (changed_join_cb), query);

	query->priv->internal_transaction ++;
#ifdef GDA_DEBUG_signal
        g_print (">> 'JOIN_REMOVED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "join_removed", join);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'JOIN_REMOVED' from %s\n", __FUNCTION__);
#endif
	
	query->priv->internal_transaction --;
        g_object_unref (join);

	/* cleaning... */
	query_clean_junk (query);
}

static void
changed_join_cb (GdaQueryJoin *join, GdaQuery *query)
{
#ifdef GDA_DEBUG_signal
        g_print (">> 'JOIN_UPDATED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "join_updated", join);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'JOIN_UPDATED' from %s\n", __FUNCTION__);
#endif
}

/**
 * gda_query_del_join
 * @query: a #GdaQuery object
 * @join: a #GdaQueryJoin object
 *
 * Removes @join from @query. @join MUST be present within @query.
 */
void
gda_query_del_join (GdaQuery *query, GdaQueryJoin *join)
{
	g_return_if_fail (query && GDA_IS_QUERY (query));
        g_return_if_fail (query->priv);
	g_return_if_fail (query_sql_forget (query, NULL));
	g_return_if_fail (join && GDA_IS_QUERY_JOIN (join));
	g_return_if_fail (g_slist_find (query->priv->joins_flat, join));

	destroyed_join_cb (join, query);
}

#ifdef GDA_DEBUG_0
static void
joins_pack_dump (GdaQuery *query)
{
	GSList *packs, *list;
	gchar *xml;

	packs = query->priv->joins_pack;
	while (packs) {
		g_print ("=== PACK ===\n");
		list = JOINS_PACK (packs->data)->targets;
		g_print ("TARGETS: ");
		while (list) {
			xml = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
			g_print ("%s ", xml);
			g_free (xml);
			list = g_slist_next (list);
		}
		g_print ("\nJOINS: ");
		list = JOINS_PACK (packs->data)->joins;
		while (list) {
			g_print ("%p ", list->data);
			list = g_slist_next (list);
		}
		g_print ("\n");
		packs = g_slist_next (packs);
	}
}
#endif

/*
 * Adds a join in the joins pack structure, which is another way of storing joins
 * within the query.
 * It is assumed that the join itself is not yet present in the list of joins
 *
 * Returns: TRUE if no error
 */
static gboolean
joins_pack_add_join (GdaQuery *query, GdaQueryJoin *join)
{
	GSList *pack_list;
	JoinsPack *pack;
	JoinsPack *pack1 = NULL, *pack2 = NULL;
	GdaQueryTarget *t1, *t2;

	/* we want an active join only */
	g_return_val_if_fail (gda_referer_activate (GDA_REFERER (join)), FALSE);
	t1 = gda_query_join_get_target_1 (join);
	t2 = gda_query_join_get_target_2 (join);

	/* try fo identify existing joins packings in which the new join can go */
	pack_list = query->priv->joins_pack;
	while (pack_list && !pack1 && !pack2) {
		pack = JOINS_PACK (pack_list->data);
		if (!pack1) {
			if (g_slist_find (pack->targets, t2)) {
				GdaQueryTarget *tmp;
				gda_query_join_swap_targets (join);
				tmp = t1;
				t1 = t2;
				t2 = tmp;
			}

			if (g_slist_find (pack->targets, t1)) 
				pack1 = pack;
		}
		else 
			if (g_slist_find (pack->targets, t2)) 
				pack2 = pack;
		
		pack_list = g_slist_next (pack_list);
	}

	/* updating the packings */
	if (!pack1) {
		/* a new JoinsPack is necessary */
		pack = g_new0 (JoinsPack, 1);
		pack->targets = g_slist_append (NULL, t1);
		pack->targets = g_slist_append (pack->targets, t2);
		pack->joins = g_slist_append (NULL, join);

		query->priv->joins_pack = g_slist_append (query->priv->joins_pack, pack);
	}
	else {
		/* append join to the identified pack1 */
		pack1->joins = g_slist_append (pack1->joins, join);
		pack1->targets = g_slist_append (pack1->targets, t2);

		if (pack2 && (pack2 != pack1)) {
			/* merge pack2 into pack1 */
			GSList *joins;
			GSList *targets;
			GdaQueryJoin *cjoin;
			
			/* reodering the joins to start with t2 */
			targets = g_slist_append (NULL, t2);
			while (pack2->joins) {
				joins = pack2->joins;
				cjoin = NULL;

				while (joins && !cjoin) {
					t1 = gda_query_join_get_target_1 (GDA_QUERY_JOIN (joins->data));
					t2 = gda_query_join_get_target_2 (GDA_QUERY_JOIN (joins->data));

					if (g_slist_find (targets, t1)) {
						cjoin = GDA_QUERY_JOIN (joins->data);
						targets = g_slist_append (targets, t2);
						pack1->targets = g_slist_append (pack1->targets, t2);
					}
					else {
						if (g_slist_find (targets, t2)) {
							cjoin = GDA_QUERY_JOIN (joins->data);
							gda_query_join_swap_targets (cjoin);
							targets = g_slist_append (targets, t1);
							pack1->targets = g_slist_append (pack1->targets, t1);
						}
					}
				}

				g_assert (cjoin);
				pack2->joins = g_slist_remove (pack2->joins, cjoin);
				pack1->joins = g_slist_append (pack1->joins, cjoin);
			}
			
			g_slist_free (targets);
			/* getting rid of pack2 */
			query->priv->joins_pack = g_slist_remove (query->priv->joins_pack, pack2);
			g_slist_free (pack2->targets);
			g_free (pack2);
		}
	}

	return TRUE;
}

/* 
 * Removes a join from the joins packing structures.
 * It is assumed that the join itself IS present in the list of joins
 */
static void
joins_pack_del_join (GdaQuery *query, GdaQueryJoin *join)
{
	JoinsPack *joinpack = NULL, *pack;
	GSList *pack_list, *list;

	/* identifying the pack in which join is; if the join is not activatable, then it
	   may already have been removed */
	pack_list = query->priv->joins_pack;
	while (pack_list && !joinpack) {
		pack = JOINS_PACK (pack_list->data);
		if (g_slist_find (pack->joins, join)) 
			joinpack = pack;
		pack_list = g_slist_next (pack_list);
	}
	
	if (g_slist_find (query->priv->joins_flat, join))
		g_assert (joinpack);

	/* removing the pack and adding again all the joins within that pack */
	if (joinpack) {
		query->priv->joins_pack = g_slist_remove (query->priv->joins_pack, joinpack);

		list = joinpack->joins;
		while (list) {
			if ((GDA_QUERY_JOIN (list->data) != join) && 
			    gda_referer_activate (GDA_REFERER (list->data)))
				joins_pack_add_join (query, GDA_QUERY_JOIN (list->data));
			
			list = g_slist_next (list);
		}
		g_slist_free (joinpack->targets);
		g_slist_free (joinpack->joins);
		g_free (joinpack);
	}
}






/**
 * gda_query_get_condition
 * @query: a #GdaQuery object
 *
 * Get the query's associated condition
 *
 * Returns: the #GdaQueryCondition object
 */
GdaQueryCondition *
gda_query_get_condition (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	
	return query->priv->cond;
}

/**
 * gda_query_set_condition
 * @query: a #GdaQuery object
 * @cond: a #GdaQueryCondition object, or %NULL to remove condition
 *
 * Sets the query's associated condition; if there was already a query condition,
 * then the old one is trashed first.
 *
 * Pass %NULL as the @cond argument to remove any query condition
 */
void
gda_query_set_condition (GdaQuery *query, GdaQueryCondition *cond)
{
	g_return_if_fail (query && GDA_IS_QUERY (query));
	g_return_if_fail (query->priv);
	g_return_if_fail (query_sql_forget (query, NULL));
	if (cond)
		g_return_if_fail (GDA_IS_QUERY_CONDITION (cond));

	if (query->priv->cond == cond)
		return;
	
	query->priv->internal_transaction ++;

	if (query->priv->cond) 
		destroyed_cond_cb (query->priv->cond, query);

	if (cond) {
		g_object_ref (G_OBJECT (cond));
		query->priv->cond = cond;
		g_signal_connect (G_OBJECT (cond), "changed",
				  G_CALLBACK (changed_cond_cb), query);

		gda_object_connect_destroy (cond,
					       G_CALLBACK (destroyed_cond_cb), query);
	}

	query->priv->internal_transaction --;

	/* cleaning... */
	query_clean_junk (query);
}

static void
changed_cond_cb (GdaQueryCondition *cond, GdaQuery *query)
{
	if (query->priv->auto_clean) {
		/* if query->priv->cond is limited to a single empty node, then remove it */
		if (query->priv->cond &&
		    ! gda_query_condition_is_leaf (query->priv->cond)) {
			GSList *list = gda_query_condition_get_children (query->priv->cond);
			if (!list) {
				query->priv->internal_transaction ++;
				destroyed_cond_cb (query->priv->cond, query);
				query->priv->internal_transaction --;
			}
			g_slist_free (list);
		}
	}
	    
	if (! query->priv->internal_transaction)
		gda_object_signal_emit_changed (GDA_OBJECT (query));
}

static void
destroyed_cond_cb (GdaQueryCondition *cond, GdaQuery *query)
{
	/* real destroy code */
	g_assert (query->priv->cond == cond);
	g_signal_handlers_disconnect_by_func (G_OBJECT (cond),
					      G_CALLBACK (destroyed_cond_cb), query);
	g_signal_handlers_disconnect_by_func (G_OBJECT (cond),
					      G_CALLBACK (changed_cond_cb), query);
	query->priv->cond = NULL;
	g_object_unref (G_OBJECT (cond));
	
	/* cleaning... */
	query_clean_junk (query);
}

/**
 * gda_query_get_parameters
 * @query: a #GdaQuery object
 *
 * Get a list of parameters which the query accepts.
 *
 * Returns: a list of #GdaParameter objects (the list and objects must be freed by the caller)
 */
GSList *
gda_query_get_parameters (GdaQuery *query)
{
	GSList *list, *tmplist, *retval = NULL;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	
	/* parameters for value fields */
	list = query->priv->fields;
	while (list) {
		tmplist = gda_query_field_get_parameters (GDA_QUERY_FIELD (list->data));
		if (tmplist)
			retval = g_slist_concat (retval, tmplist);
		list = g_slist_next (list);
	}

	/* parameters for sub queries */
	list = query->priv->sub_queries;
	while (list) {
		tmplist = gda_query_get_parameters (GDA_QUERY (list->data));
		if (tmplist)
			retval = g_slist_concat (retval, tmplist);
		list = g_slist_next (list);
	}

	return retval;
}

/**
 * gda_query_get_parameter_list
 * @query: a #GdaQuery object
 *
 * Like the gda_query_get_parameters() method, get a list of parameters which the query accepts,
 * except that the parameters are stored within a #GdaParameterList object, and can be used as an argument
 * to the gda_query_execute() method.
 *
 * Returns: a new #GdaParameterList object, or %NULL if @query does not accept any parameter.
 */
GdaParameterList *
gda_query_get_parameter_list (GdaQuery *query)
{
	GdaParameterList *dataset = NULL;
	GSList *list, *params;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);	

	params = gda_query_get_parameters (query);

	if (params) {
		dataset = GDA_PARAMETER_LIST (gda_parameter_list_new (params));
 
		/* get rid of the params list since we don't use them anymore */
		list = params;
		while (list) {
			g_object_unref (G_OBJECT (list->data));
			list = g_slist_next (list);
		}
		g_slist_free (params);
	}

	return dataset;
}

/**
 * gda_query_execute
 * @query: the #GdaQuery to execute
 * @params: a #GdaParameterList object obtained using gda_query_get_parameter_list()
 * @iter_model_only_requested: set to TRUE if the returned data model will only be accessed using an iterator
 * @error: a place to store errors, or %NULL
 *
 * Executes @query and returns #GdaDataModel if @query's execution yields to a data set, or a
 * #GdaParameterList object otherwise, or %NULL if an error occurred. You can test the return value
 * using GObject's introscpection features such as GDA_IS_DATA_MODEL() or GDA_IS_PARAMETER_LIST().
 *
 * For more information about the returned value, see gda_server_provider_execute_command().
 *
 * Returns: a #GdaDataModel, a #GdaParameterList or %NULL.
 */
GdaObject *
gda_query_execute (GdaQuery *query, GdaParameterList *params, gboolean iter_model_only_requested, GError **error)
{
	gchar *str;
	GdaCommand *cmde;
	GdaConnection *cnc;
	GdaDict *dict;
	GList *list;
	GdaParameterList *options = NULL;
	GdaObject *retval = NULL;
	GdaServerProvider *prov;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (!params || GDA_IS_PARAMETER_LIST (params), NULL);

	dict = gda_object_get_dict (GDA_OBJECT (query));
	cnc = gda_dict_get_connection (dict);
	if (!cnc) {
		g_set_error (error, GDA_QUERY_ERROR, GDA_QUERY_NO_CNC_ERROR,
			     _("No connection associated to query's dictionary"));
		return NULL;
	}
	if (!gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_QUERY_ERROR, GDA_QUERY_CNC_CLOSED_ERROR,
			     _("Connection associated to query's dictionary is closed"));
		return NULL;
	}

	/* try to use the execute_query() virtual method of the provider if implemented */
	prov = gda_connection_get_provider_obj (cnc);
	g_assert (prov);
	if (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (prov))->execute_query) {
		GList *list;
		GList *errors_before = NULL;
		const GList *errors_after;

		if (error) {
			list = (GList *) gda_connection_get_events (cnc);
			if (list)
				errors_before = gda_connection_event_list_copy (list);
		}
		retval = gda_server_provider_execute_query (prov, cnc, query, params);
		if (error) {
			errors_after = gda_connection_get_events (cnc);
			for (list = g_list_last ((GList *) errors_after); list && !(*error); list = list->prev) {
				if (!g_list_find (errors_before, list->data)) 
					g_set_error (error, GDA_QUERY_ERROR, GDA_QUERY_EXEC_ERROR,
						     gda_connection_event_get_description (GDA_CONNECTION_EVENT (list->data)));
			}
			if (errors_before)
				gda_connection_event_list_free (errors_before);
		}
		return retval;
	}

	/* use SQL and execute_command() virtual method of the provider */
	str = gda_renderer_render_as_sql (GDA_RENDERER (query), params, NULL, 0, error);
	if (!str)
		return NULL;

#ifdef GDA_DEBUG_no
	g_print ("GdaQueryExecute:\nSQL= %s\n", str);
#endif

	cmde = gda_command_new (str, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (iter_model_only_requested) {
		options = (GdaParameterList *) g_object_new (GDA_TYPE_PARAMETER_LIST, "dict", dict, NULL);
		gda_parameter_list_add_param_from_string (options, "ITER_MODEL_ONLY", 
							  G_TYPE_BOOLEAN, "TRUE");
	}
	list = gda_connection_execute_command (cnc, cmde, options, error);
	if (list) {
		GList *plist;

		retval = (GdaObject*) g_list_last (list)->data;
		if (retval)
			g_object_ref (retval);
		plist = list;
		for (plist = list; plist; plist = plist->next)
			if (plist->data)
				g_object_unref (plist->data);
		g_list_free (list);
	}

	if (options)
		g_object_unref (options);
	gda_command_free (cmde);
	g_free (str);

	return retval;
}


/**
 * gda_query_add_field_from_sql
 * @query: a #GdaQuery object
 * @field: a SQL expression
 * @error: place to store the error, or %NULL
 *
 * Parses @field and if it represents a valid SQL expression for a
 * field, then add it to @query.
 *
 * Returns: a new #GdaQueryField object, or %NULL
 */
GdaQueryField *
gda_query_add_field_from_sql (GdaQuery *query, const gchar *field, GError **error)
{
	GdaQueryField *qfield = NULL;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);

	qfield = gda_query_field_new_from_sql (query, field, error);
	if (qfield) {
		gda_entity_add_field (GDA_ENTITY (query), GDA_ENTITY_FIELD (qfield));
		g_object_unref (G_OBJECT (qfield));
	}

	return qfield;
}


#ifdef GDA_DEBUG
static void
gda_query_dump (GdaQuery *query, guint offset)
{
	gchar *str;
        guint i;
	GSList *list;
	GError *error = NULL;
	
	g_return_if_fail (query && GDA_IS_QUERY (query));

        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (query->priv) {
		gchar *sql;

                g_print ("%s" D_COL_H1 "GdaQuery" D_COL_NOR " %p (type=%d, QU%d) %s",
                         str, query, query->priv->query_type, gda_query_object_get_int_id (GDA_QUERY_OBJECT (query)),
			 gda_object_get_name (GDA_OBJECT (query)));
		if (gda_query_is_active (GDA_REFERER (query)))
			g_print (": Active\n");
		else
			g_print (D_COL_ERR ": Non active\n" D_COL_NOR);
		
		/* targets */
		if (query->priv->targets)
			g_print ("%sTargets:\n", str);
		else
			g_print ("%sNo target defined.\n", str);
		list = query->priv->targets;
		while (list) {
			gda_object_dump (GDA_OBJECT (list->data), offset+5);
			list = g_slist_next (list);
		}

		/* fields */
		if (query->priv->fields)
			g_print ("%sFields:\n", str);
		else
			g_print ("%sNo field defined.\n", str);
		list = query->priv->fields;
		while (list) {
			gda_object_dump (GDA_OBJECT (list->data), offset+5);
			list = g_slist_next (list);
		}

		/* joins */
		if (query->priv->joins_flat)
			g_print ("%sJoins:\n", str);
		list = query->priv->joins_flat;
		while (list) {
			gda_object_dump (GDA_OBJECT (list->data), offset+5);
			list = g_slist_next (list);
		}
		/*joins_pack_dump (query);*/ /* RAW joins pack output */

		/* condition */
		if (query->priv->cond) {
			g_print ("%sCondition:\n", str);
			gda_object_dump (GDA_OBJECT (query->priv->cond), offset+5);
		}

		if (0 && query->priv->all_conds) {
			g_print ("%sReferenced conditions:\n", str);
			list = query->priv->all_conds;
			while (list) {
				gda_object_dump (list->data, offset + 5);
				list = g_slist_next (list);
			}
		}


		/* sub queries */
		if (query->priv->sub_queries)
			g_print ("%sSub-queries:\n", str);
		list = query->priv->sub_queries;
		while (list) {
			gda_object_dump (GDA_OBJECT (list->data), offset+5);
			list = g_slist_next (list);
		}

		/* parameters sources */
		if (query->priv->param_sources)
			g_print ("%sParameters sources:\n", str);
		list = query->priv->param_sources;
		while (list) {
			g_print ("%s     GdaDataModel %p\n", str, list->data);
			/* gda_object_dump (GDA_OBJECT (list->data), offset+5); */
			list = g_slist_next (list);
		}

		/* Rendered version of the query */
		sql = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, NULL, GDA_RENDERER_PARAMS_AS_DETAILED, &error);
		if (sql) {
			g_print ("%sSQL=%s\n", str, sql);
			g_free (sql);
		}
		else {
			g_print ("%sError occurred:\n%s%s\n", str, str, 
				 error && error->message ? error->message : _("Unknown error"));
			if (error)
				g_error_free (error);
		}
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, query);
	g_free (str);
}
#endif

static void
gda_query_set_int_id (GdaQueryObject *query, guint id)
{
	gchar *str;
	GdaDictRegisterStruct *reg;

	str = g_strdup_printf ("QU%u", id);
	gda_object_set_id (GDA_OBJECT (query), str);
	g_free (str);

	reg = gda_dict_get_object_type_registration (gda_object_get_dict ((GdaObject *)query), GDA_TYPE_QUERY);
	gda_queries_declare_serial (reg, id);
}

/**
 * gda_query_set_order_by_field
 * @query: a #GdaQuery
 * @field: a #GdaQueryField which is in @query
 * @order: the order in the list of ORDER BY fields (starts at 0), or -1
 * @ascendant: TRUE to sort ascending
 *
 * Sets @field to be used in the ORDER BY clause (using the @order and @ascendant attributes) if
 * @order >= 0. If @order < 0, then @field will not be used in the ORDER BY clause.
 */
void
gda_query_set_order_by_field (GdaQuery *query, GdaQueryField *field, gint order, gboolean ascendant)
{
	g_return_if_fail (query && GDA_IS_QUERY (query));
	g_return_if_fail (query->priv);
	g_return_if_fail (field && GDA_IS_QUERY_FIELD (field));
	g_return_if_fail (g_slist_find (query->priv->fields, field));

	if (! (GDA_IS_QUERY_FIELD_VALUE (field) && 
	       (query->priv->query_type == GDA_QUERY_TYPE_NON_PARSED_SQL)))
		g_return_if_fail (query_sql_forget (query, NULL));
	
	if ((query->priv->query_type == GDA_QUERY_TYPE_INSERT) ||
	    (query->priv->query_type == GDA_QUERY_TYPE_DELETE) ||
	    (query->priv->query_type == GDA_QUERY_TYPE_UPDATE))
		return;
	
	if (g_slist_find (query->priv->fields_order_by, field))
		query->priv->fields_order_by = g_slist_remove (query->priv->fields_order_by, field);
	
	if (order < 0)
		g_object_set_data (G_OBJECT (field), "order_by_asc", NULL);
	else { /* add to the ORDER BY */
		g_object_set_data (G_OBJECT (field), "order_by_asc", GINT_TO_POINTER (ascendant));
		query->priv->fields_order_by = g_slist_insert (query->priv->fields_order_by, field, order);
	}

	if (!query->priv->internal_transaction)
		gda_object_signal_emit_changed (GDA_OBJECT (query));
}

/**
 * gda_query_get_order_by_field
 * @query: a #GdaQuery
 * @field: a #GdaQueryField which is in @query
 * @ascendant: if not %NULL, will be set TRUE if ascendant sorting and FALSE otherwise
 *
 * Tells if @field (which MUST be in @query) is part of the ORDER BY clause.
 *
 * Returns: -1 if no, and the order where it appears in the ORDER BY list otherwise
 */
gint
gda_query_get_order_by_field (GdaQuery *query, GdaQueryField *field, gboolean *ascendant)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), -1);
	g_return_val_if_fail (query->priv, -1);
	g_return_val_if_fail (field && GDA_IS_QUERY_FIELD (field), -1);
	g_return_val_if_fail (g_slist_find (query->priv->fields, field), -1);

	if (ascendant)
		*ascendant = g_object_get_data (G_OBJECT (field), "order_by_asc") ? TRUE : FALSE;
	return g_slist_index (query->priv->fields_order_by, field);
}


/* NOTE: A query's field has several status:
   -> internal: such a field is added by libgnomedb for its own needs, should NEVER
      get out of the query
   -> visible: such fields make the external representation of the equivalent entity
   -> non visible: fields that take part in making the query (condition, values, function params, etc
*/


/**
 * gda_query_get_all_fields
 * @query: a #GdaQuery object
 *
 * Fetch a list of all the fields of @query: the ones which are visible, and
 * the ones which are not visible and are not internal query fields.
 *
 * Returns: a new list of fields
 */
GSList *
gda_query_get_all_fields (GdaQuery *query)
{
	GSList *list, *fields = NULL;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (GDA_QUERY (query)->priv, NULL);
	
	list = query->priv->fields;
	while (list) {
		if (gda_query_field_is_visible (GDA_QUERY_FIELD (list->data)) ||
		    !gda_query_field_is_internal (GDA_QUERY_FIELD (list->data)))
			fields = g_slist_append (fields, list->data);
		list = g_slist_next (list);
	}

	return fields;
}

/**
 * gda_query_get_field_by_sql_naming
 * @query: a #GdaQuery object
 * @sql_name: the SQL naming for the requested field
 *
 * Returns:
 */
GdaQueryField *
gda_query_get_field_by_sql_naming (GdaQuery *query, const gchar *sql_name) {
	
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	
	return gda_query_get_field_by_sql_naming_fields (query, sql_name, query->priv->fields);
}

/**
 * @query: a #GdaQuery object
 * @param_name: a parameter name
 *
 * Get a pointer to the #GdaQueryFieldValue object which is a parameter named @param_name
 *
 * Returns: a pointer to the requested query field, or %NULL if it was not found
 */
GdaQueryField *
gda_query_get_field_by_param_name (GdaQuery *query, const gchar *param_name)
{
	GdaQueryField *qf;
	GSList *allfields;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	g_return_val_if_fail (param_name, NULL);

	for (qf = NULL, allfields = query->priv->fields; allfields && !qf; allfields = allfields->next) {
		qf = GDA_QUERY_FIELD (allfields->data);
		if (!GDA_IS_QUERY_FIELD_VALUE (qf) || !gda_query_field_value_get_is_parameter ((GdaQueryFieldValue*) qf))
			qf = NULL;
		else {
			if (strcmp (gda_object_get_name (GDA_OBJECT (qf)), param_name))
				qf = NULL;
		}
	}

	return qf;
}

/**
 * gda_query_get_field_by_sql_naming_fields
 * @query: a #GdaQuery object
 * @sql_name: the SQL naming for the requested field
 * @fields_list: an explicit list of fields to search into
 *
 * Returns:
 */
GdaQueryField *
gda_query_get_field_by_sql_naming_fields (GdaQuery *query, const gchar *sql_name, GSList *fields_list)
{
	GdaQueryField *field = NULL;
	gboolean err = FALSE;
	GSList *list;

	g_return_val_if_fail (sql_name && *sql_name, NULL);

	list = fields_list;
	while (list && !err) {
		if (GDA_IS_QUERY_FIELD_FIELD (list->data)) {
			/* split the sql_name into a list of strings, by the '.' separator */
			gchar **split = g_strsplit (sql_name, ".", 0);
			gint nb_tokens;
			gchar *ref_name = gda_query_field_field_get_ref_field_name (GDA_QUERY_FIELD_FIELD (list->data));
			gchar *str;

			for (nb_tokens = 0; split[nb_tokens] != NULL; nb_tokens++);
			if (nb_tokens == 1) {
				/* first try to find the table corresponding to a lower-case version of @sql_name, 
				   and if not found, try with the unchanged @sql_name */
				str = g_utf8_strdown (sql_name, -1);
				if (!strcmp (ref_name, str) ||
				    !strcmp (ref_name, sql_name)) {
					if (field)
						err = TRUE;
					else
						field = GDA_QUERY_FIELD (list->data);
				}
				g_free (str);
			}
			else {
				/* compare with target_alias.ref_field */
				GdaQueryTarget *target;

				target = gda_query_field_field_get_target (GDA_QUERY_FIELD_FIELD (list->data));
				if (target) {
					str = g_utf8_strdown (split[1], -1);
					if (!strcmp (gda_query_target_get_alias (target), split[0]) &&
					    (!strcmp (ref_name, str) ||
					     !strcmp (ref_name, split[1]))) {
						if (field)
							err = TRUE;
						else
							field = GDA_QUERY_FIELD (list->data);
					}
					
					/* compare with target_ref_entity.ref_field */
					if (!field) {
						gchar *str2 = g_utf8_strdown (split[0], -1);
						GdaEntity *tmpent = gda_query_target_get_represented_entity (target);
						const gchar *entstr;
						
						if (tmpent)
							entstr = gda_object_get_name (GDA_OBJECT (tmpent));
						else
							entstr = gda_query_target_get_represented_table_name (target);
						
						if (!err && !field &&
						    (!strcmp (entstr, str2) ||
						     !strcmp (entstr, split[0])) &&
						    (!strcmp (ref_name, str) ||
						     !strcmp (ref_name, split[1]) )) {
							if (field)
								err = TRUE;
							else
								field = GDA_QUERY_FIELD (list->data);
						}
						g_free (str2);
					}
					g_free (str);
				}
			}
			g_strfreev (split);
		}

		if (GDA_IS_QUERY_FIELD_ALL (list->data)) {
			/* split the sql_name into a list of strings, by the '.' separator */
			gchar **split = g_strsplit (sql_name, ".", 0);
			gint nb_tokens;
			
			for (nb_tokens = 0; split[nb_tokens] != NULL; nb_tokens++);
			if (nb_tokens == 1) {
				if (!strcmp ("*", sql_name)) {
					if (field)
						err = TRUE;
					else
						field = GDA_QUERY_FIELD (list->data);
				}
			}
			else {
				/* compare with target_alias.* */
				GdaQueryTarget *target = gda_query_field_all_get_target (GDA_QUERY_FIELD_ALL (list->data));
				if (!strcmp (gda_query_target_get_alias (target), split[0]) &&
				    !strcmp ("*", split[1])) {
					if (field)
						err = TRUE;
					else
						field = GDA_QUERY_FIELD (list->data);
				}
				
				/* compare with target_ref_entity.ref_field */
				if (!err && !field &&
				    gda_query_target_get_represented_table_name (target) &&
				    !strcmp (gda_query_target_get_represented_table_name (target), split[0]) &&
				    !strcmp ("*", split[1])) {
					if (field)
						err = TRUE;
					else
						field = GDA_QUERY_FIELD (list->data);
				}
			}
			g_strfreev (split);
		}

		if (GDA_IS_QUERY_FIELD_FUNC (list->data)) {
			TO_IMPLEMENT;
		}
		if (GDA_IS_QUERY_FIELD_VALUE (list->data)) {
			/* do nothing */
		}
		
		list = g_slist_next (list);
	}
	if (err)
		return NULL;
	return field;
}

/*
 * GdaEntity interface implementation
 */
static gboolean
gda_query_has_field (GdaEntity *iface, GdaEntityField *field)
{
	g_return_val_if_fail (GDA_IS_QUERY (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, FALSE);
	g_return_val_if_fail (field && GDA_IS_QUERY_FIELD (field), FALSE);

	if (gda_query_field_is_visible (GDA_QUERY_FIELD (field)) &&
	    g_slist_find (GDA_QUERY (iface)->priv->fields, field))
		return TRUE;
	else
		return FALSE;
}

static GSList *
gda_query_get_fields (GdaEntity *iface)
{
	GdaQuery *query;
	GSList *list, *fields = NULL;

	g_return_val_if_fail (GDA_IS_QUERY (iface), NULL);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, NULL);
	query = GDA_QUERY (iface);

	list = query->priv->fields;
	while (list) {
		if (gda_query_field_is_visible (GDA_QUERY_FIELD (list->data)))
			fields = g_slist_append (fields, list->data);
		list = g_slist_next (list);
	}

	return fields;
}


static GdaEntityField *
gda_query_get_field_by_name (GdaEntity *iface, const gchar *name)
{
	GdaQuery *query;
	GSList *list;
	GdaEntityField *field = NULL;
	gboolean err = FALSE;
	
	g_return_val_if_fail (GDA_IS_QUERY (iface), NULL);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, NULL);
	query = GDA_QUERY (iface);
	
	/* first pass: using gda_entity_field_get_name () */
	list = query->priv->fields;
	while (list && !err) {
		if (!strcmp (gda_entity_field_get_name (GDA_ENTITY_FIELD (list->data)), name)) {
			if (field) 
				err = TRUE;
			else
				field = GDA_ENTITY_FIELD (list->data);
		}
		list = g_slist_next (list);
	}
	if (err)
		return NULL;
	if (field)
		return field;
	
	/* second pass: using SQL naming */
	return (GdaEntityField *) gda_query_get_field_by_sql_naming (query, name);
}

static GdaEntityField *
gda_query_get_field_by_xml_id (GdaEntity *iface, const gchar *xml_id)
{
	GdaQuery *query;
	GSList *list;
	GdaEntityField *field = NULL;
	gchar *str;

	g_return_val_if_fail (GDA_IS_QUERY (iface), NULL);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, NULL);
	query = GDA_QUERY (iface);
	
	list = query->priv->fields;
	while (list && !field) {
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
		if (!strcmp (str, xml_id))
			field = GDA_ENTITY_FIELD (list->data);
		list = g_slist_next (list);
	}

	return field;
}

static GdaEntityField *
gda_query_get_field_by_index (GdaEntity *iface, gint index)
{
	GdaQuery *query;
	GSList *list;
	GdaEntityField *field = NULL;
	gint i = -1;

	g_return_val_if_fail (GDA_IS_QUERY (iface), NULL);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, NULL);
	query = GDA_QUERY (iface);
	
	list = query->priv->fields;
	while (list && !field) {
		if (gda_query_field_is_visible (GDA_QUERY_FIELD (list->data))) {
			i++;
			if (i == index)
				field = GDA_ENTITY_FIELD (list->data);
		}

		list = g_slist_next (list);
	}

	return field;
}

static gint
gda_query_get_field_index (GdaEntity *iface, GdaEntityField *field)
{
	GdaQuery *query;
	GSList *list;
	gint current, pos = -1;

	g_return_val_if_fail (GDA_IS_QUERY (iface), -1);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, -1);
	g_return_val_if_fail (field && GDA_IS_QUERY_FIELD (field), -1);
	query = GDA_QUERY (iface);

	if (!g_slist_find (query->priv->fields, field))
		return -1;

	if (!gda_query_field_is_visible (GDA_QUERY_FIELD (field)))
		return -1;

	current = 0;
	list = query->priv->fields;
	while (list && (pos==-1)) {
		if (list->data == (gpointer) field)
			pos = current;
		if (gda_query_field_is_visible (GDA_QUERY_FIELD (list->data)))
			current++;
		list = g_slist_next (list);
	}

	return pos;
}

/**
 * gda_query_get_fields_by_target
 * @query: a #GdaQuery object
 * @target: a #GdaQueryTarget object representing a target in @query
 *
 * Get a list of all the #GdaQueryField objects in @query which depent on the existance of 
 * @target.
 *
 * Returns: a new list of #GdaQueryField objects
 */
GSList *
gda_query_get_fields_by_target (GdaQuery *query, GdaQueryTarget *target, gboolean visible_fields_only)
{
	GSList *retval = NULL;
	GSList *tmplist, *ptr;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	g_return_val_if_fail (target && GDA_IS_QUERY_TARGET (target), NULL);
	g_return_val_if_fail (g_slist_find (query->priv->targets, target), NULL);

	if (visible_fields_only)
		tmplist = gda_query_get_fields (GDA_ENTITY (query));
	else
		tmplist = gda_query_get_all_fields (query);

	ptr = tmplist;
	while (ptr) {
		if ((GDA_IS_QUERY_FIELD_FIELD (ptr->data) && (gda_query_field_field_get_target (GDA_QUERY_FIELD_FIELD (ptr->data)) == target)) ||
		    (GDA_IS_QUERY_FIELD_ALL (ptr->data) && (gda_query_field_all_get_target (GDA_QUERY_FIELD_ALL (ptr->data)) == target)))
			retval = g_slist_prepend (retval, ptr->data);
			
		ptr = g_slist_next (ptr);
	}
	g_slist_free (tmplist);
	retval = g_slist_reverse (retval);

	return retval;
}

static void
gda_query_add_field (GdaEntity *iface, GdaEntityField *field)
{
	gda_query_add_field_before (iface, field, NULL);
}

static void
gda_query_add_field_before (GdaEntity *iface, GdaEntityField *field, GdaEntityField *field_before)
{
	GdaQuery *query;
	gint pos = -1;
	GdaConnection *cnc;

	g_return_if_fail (GDA_IS_QUERY (iface));
	g_return_if_fail (GDA_QUERY (iface)->priv);
	query = GDA_QUERY (iface);

	g_return_if_fail (field && GDA_IS_QUERY_FIELD (field));
        g_return_if_fail (!g_slist_find (query->priv->fields, field));
	g_return_if_fail (gda_entity_field_get_entity (field) == GDA_ENTITY (query));

	cnc = gda_dict_get_connection (gda_object_get_dict (GDA_OBJECT (query)));
	if (cnc) {
		/* test that the GType of @field is handled by the provider used in cnc */
		GdaServerProvider *prov;

		prov = gda_connection_get_provider_obj (cnc);
		if (prov) {
			GType type;
			GdaDataHandler *handler;
			
			type = gda_entity_field_get_g_type (field);
			if (type == GDA_TYPE_BLOB) {
				if (! gda_server_provider_supports_feature (prov, cnc, GDA_CONNECTION_FEATURE_BLOBS))
					g_warning (_("While adding to a GdaQuery: Blobs are not supported by the "
						     "connection's provider and may be rendered incorrectly"));
			}
			if ((type != G_TYPE_INVALID) && (type != GDA_TYPE_BLOB)){
				handler = gda_server_provider_get_data_handler_gtype (prov, cnc, type);
				if (!handler) 
					g_warning (_("While adding to a GdaQuery: field type '%s' is not supported by the "
						     "connection's provider and may be rendered incorrectly"), g_type_name (type));
			}
		}
	}
	
	if (! (GDA_IS_QUERY_FIELD_VALUE (field) && 
	       (query->priv->query_type == GDA_QUERY_TYPE_NON_PARSED_SQL)))
		g_return_if_fail (query_sql_forget (query, NULL));

	if (field_before) {
		g_return_if_fail (field_before && GDA_IS_QUERY_FIELD (field_before));
		g_return_if_fail (g_slist_find (query->priv->fields, field_before));
		g_return_if_fail (gda_entity_field_get_entity (field_before) == GDA_ENTITY (query));
		pos = g_slist_index (query->priv->fields, field_before);
	}

	query->priv->fields = g_slist_insert (query->priv->fields, field, pos);
	g_object_ref (G_OBJECT (field));

	if (query->priv->serial_field <= gda_query_object_get_int_id (GDA_QUERY_OBJECT (field)))
		query->priv->serial_field = gda_query_object_get_int_id (GDA_QUERY_OBJECT (field)) + 1;
	
	gda_object_connect_destroy (field, G_CALLBACK (destroyed_field_cb), query);
        g_signal_connect (G_OBJECT (field), "changed",
                          G_CALLBACK (changed_field_cb), query);
        g_signal_connect (G_OBJECT (field), "numid_changed",
                          G_CALLBACK (id_field_changed_cb), query);

#ifdef GDA_DEBUG_signal
        g_print (">> 'FIELD_ADDED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "field_added", field);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'FIELD_ADDED' from %s\n", __FUNCTION__);
#endif	
	m_changed_cb (query);
}

static void
destroyed_field_cb (GdaEntityField *field, GdaQuery *query)
{
	g_assert (g_slist_find (query->priv->fields, field));

	if (! (GDA_IS_QUERY_FIELD_VALUE (field) && 
	       (query->priv->query_type == GDA_QUERY_TYPE_NON_PARSED_SQL)))
		g_return_if_fail (query_sql_forget (query, NULL));

	gda_query_set_order_by_field (query, GDA_QUERY_FIELD (field), -1, FALSE);
        query->priv->fields = g_slist_remove (query->priv->fields, field);
        g_signal_handlers_disconnect_by_func (G_OBJECT (field),
                                              G_CALLBACK (destroyed_field_cb), query);
        g_signal_handlers_disconnect_by_func (G_OBJECT (field),
                                              G_CALLBACK (changed_field_cb), query);
        g_signal_handlers_disconnect_by_func (G_OBJECT (field),
                                              G_CALLBACK (id_field_changed_cb), query);

#ifdef GDA_DEBUG_signal
        g_print (">> 'FIELD_REMOVED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "field_removed", field);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'FIELD_REMOVED' from %s\n", __FUNCTION__);
#endif

        g_object_unref (field);

	/* cleaning... */
	query_clean_junk (query);
}

static void
changed_field_cb (GdaEntityField *field, GdaQuery *query)
{
#ifdef GDA_DEBUG_signal
        g_print (">> 'FIELD_UPDATED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "field_updated", field);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'FIELD_UPDATED' from %s\n", __FUNCTION__);
#endif
	gda_object_signal_emit_changed (GDA_OBJECT (query));
}


static void
gda_query_swap_fields (GdaEntity *iface, GdaEntityField *field1, GdaEntityField *field2)
{
	GdaQuery *query;
	GSList *ptr1, *ptr2;

	g_return_if_fail (GDA_IS_QUERY (iface));
	g_return_if_fail (GDA_QUERY (iface)->priv);
	query = GDA_QUERY (iface);
	g_return_if_fail (query_sql_forget (query, NULL));

	g_return_if_fail (field1 && GDA_IS_QUERY_FIELD (field1));
	ptr1 = g_slist_find (query->priv->fields, field1);
	g_return_if_fail (ptr1);

	g_return_if_fail (field2 && GDA_IS_QUERY_FIELD (field2));
	ptr2 = g_slist_find (query->priv->fields, field2);
	g_return_if_fail (ptr2);

	ptr1->data = field2;
	ptr2->data = field1;

#ifdef GDA_DEBUG_signal
        g_print (">> 'FIELDS_ORDER_CHANGED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (query), "fields_order_changed");
#ifdef GDA_DEBUG_signal
        g_print ("<< 'FIELDS_ORDER_CHANGED' from %s\n", __FUNCTION__);
#endif

	gda_object_signal_emit_changed (GDA_OBJECT (query));
}

static void
gda_query_remove_field (GdaEntity *iface, GdaEntityField *field)
{
	GdaQuery *query;

	g_return_if_fail (GDA_IS_QUERY (iface));
	g_return_if_fail (GDA_QUERY (iface)->priv);
	query = GDA_QUERY (iface);
	g_return_if_fail (field && GDA_IS_QUERY_FIELD (field));
	g_return_if_fail (g_slist_find (query->priv->fields, field));

	gda_object_destroy (GDA_OBJECT (field));
}

static gboolean
gda_query_is_writable (GdaEntity *iface)
{
	g_return_val_if_fail (GDA_IS_QUERY (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, FALSE);
	
	return FALSE;
}

/**
 * gda_query_get_main_conditions
 * @query: a #GdaQuery object
 *
 * Makes a list of all the conditions (part of the WHERE clause) which
 * are always verified by @query when it is executed.
 *
 * Examples: if the WHERE clause is:
 * --> "A and B" then the list will contains {A, B}
 * --> "A and (B or C)" it will contain {A, B or C}
 * --> "A and (B and not C)", it will contain {A, B, not C}
 *
 * Returns: a new list of #GdaQueryCondition objects
 */
GSList *
gda_query_get_main_conditions (GdaQuery *query)
{
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);

	if (query->priv->cond)
		return gda_query_condition_get_main_conditions (query->priv->cond);
	else
		return NULL;
}

/**
 * gda_query_append_condition
 * @query: a #GdaQuery object
 * @cond: a #GdaQueryCondition object
 * @append_as_and: mode of append if there is already a query condition
 *
 * Appends the @cond object to @query's condition. If @query does not yet
 * have any condition, then the result is the same as gda_query_set_condition();
 * otherwise, @cond is added to @query's condition, using the AND operator
 * if @append_as_and is TRUE, and an OR operator if @append_as_and is FALSE.
 */
void
gda_query_append_condition (GdaQuery *query, GdaQueryCondition *cond, gboolean append_as_and)
{
	g_return_if_fail (query && GDA_IS_QUERY (query));
	g_return_if_fail (query->priv);
	g_return_if_fail (cond && GDA_IS_QUERY_CONDITION (cond));

	if (!query->priv->cond)
		gda_query_set_condition (query, cond);
	else {
		if ((append_as_and && 
		     (gda_query_condition_get_cond_type (query->priv->cond) == GDA_QUERY_CONDITION_NODE_AND)) ||
		    (!append_as_and && 
		     (gda_query_condition_get_cond_type (query->priv->cond) == GDA_QUERY_CONDITION_NODE_OR))) {
			/* add cond to query->priv->cond */
			g_assert (gda_query_condition_node_add_child (query->priv->cond, cond, NULL));
		}
		else {
			GdaQueryCondition *nodecond, *oldcond;

			oldcond = query->priv->cond;
			nodecond = gda_query_condition_new (query, append_as_and ? 
							    GDA_QUERY_CONDITION_NODE_AND : GDA_QUERY_CONDITION_NODE_OR);
			g_object_ref (G_OBJECT (oldcond));
			query->priv->internal_transaction ++;
			gda_query_set_condition (query, nodecond);
			query->priv->internal_transaction --;
			g_assert (gda_query_condition_node_add_child (nodecond, oldcond, NULL));
			g_object_unref (G_OBJECT (oldcond));
			g_object_unref (G_OBJECT (nodecond));

			g_assert (gda_query_condition_node_add_child (query->priv->cond, cond, NULL));
		}
	}
}

/* 
 * GdaReferer interface implementation
 */

static gboolean
gda_query_activate (GdaReferer *iface)
{
	gboolean retval = TRUE;
	GdaQuery *query;
	GSList *list;

	g_return_val_if_fail (GDA_IS_QUERY (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, FALSE);
	query = GDA_QUERY (iface);

	list = query->priv->sub_queries;
	while (list && retval) {
		retval = gda_referer_activate (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	list = query->priv->targets;
	while (list && retval) {
		retval = gda_referer_activate (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	list = query->priv->fields;
	while (list && retval) {
		retval = gda_referer_activate (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	list = query->priv->joins_flat;
	while (list && retval) {
		retval = gda_referer_activate (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	if (retval && query->priv->cond)
		retval = gda_referer_activate (GDA_REFERER (query->priv->cond));

	return retval;
}

static void
gda_query_deactivate (GdaReferer *iface)
{
	GdaQuery *query;
	GSList *list;

	g_return_if_fail (GDA_IS_QUERY (iface));
	g_return_if_fail (GDA_QUERY (iface)->priv);
	query = GDA_QUERY (iface);

	list = query->priv->sub_queries;
	while (list) {
		gda_referer_deactivate (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	list = query->priv->targets;
	while (list) {
		gda_referer_deactivate (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	list = query->priv->fields;
	while (list) {
		gda_referer_deactivate (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	list = query->priv->joins_flat;
	while (list) {
		gda_referer_deactivate (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	if (query->priv->cond)
		gda_referer_deactivate (GDA_REFERER (query->priv->cond));
}

static gboolean
gda_query_are_joins_active (GdaQuery *query)
{
	gboolean retval = TRUE;
	GSList *list;
	list = query->priv->joins_flat;
	while (list && retval) {
		retval = gda_referer_is_active (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	return retval;
}

static gboolean
gda_query_is_active (GdaReferer *iface)
{
	gboolean retval = TRUE;
	GdaQuery *query;
	GSList *list;

	g_return_val_if_fail (GDA_IS_QUERY (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, FALSE);
	query = GDA_QUERY (iface);

	list = query->priv->sub_queries;
	while (list && retval) {
		retval = gda_referer_is_active (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	list = query->priv->targets;
	while (list && retval) {
		retval = gda_referer_is_active (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	if (retval)
		retval = gda_query_are_joins_active (query);

	list = query->priv->fields;
	while (list && retval) {
		retval = gda_referer_is_active (GDA_REFERER (list->data));
		list = g_slist_next (list);
	}

	if (retval && query->priv->cond)
		retval = gda_referer_is_active (GDA_REFERER (query->priv->cond));

	return retval;
}

static GSList *
gda_query_get_ref_objects (GdaReferer *iface)
{
	GSList *list = NULL, *sub, *retval = NULL;
	GdaQuery *query;

	/* FIXME: do not take care of the objects which belong to the query itself */

	g_return_val_if_fail (GDA_IS_QUERY (iface), NULL);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, NULL);
	query = GDA_QUERY (iface);

	/* list = query->priv->param_sources; */
/* 	while (list && retval) { */
/* 		sub = gda_referer_get_ref_objects (GDA_REFERER (list->data)); */
/* 		retval = g_slist_concat (retval, sub); */
/* 		list = g_slist_next (list); */
/* 	} */

	list = query->priv->sub_queries;
	while (list && retval) {
		sub = gda_referer_get_ref_objects (GDA_REFERER (list->data));
		retval = g_slist_concat (retval, sub);
		list = g_slist_next (list);
	}

	list = query->priv->targets;
	while (list && retval) {
		sub = gda_referer_get_ref_objects (GDA_REFERER (list->data));
		retval = g_slist_concat (retval, sub);
		list = g_slist_next (list);
	}

	list = query->priv->fields;
	while (list && retval) {
		sub = gda_referer_get_ref_objects (GDA_REFERER (list->data));
		retval = g_slist_concat (retval, sub);
		list = g_slist_next (list);
	}

	list = query->priv->joins_flat;
	while (list && retval) {
		sub = gda_referer_get_ref_objects (GDA_REFERER (list->data));
		retval = g_slist_concat (retval, sub);
		list = g_slist_next (list);
	}

	if (query->priv->cond) {
		sub = gda_referer_get_ref_objects (GDA_REFERER (query->priv->cond));
		retval = g_slist_concat (retval, sub);
	}

	return retval;
}

static void
gda_query_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	GSList *list;
	GdaQuery *query;

	g_return_if_fail (GDA_IS_QUERY (iface));
	g_return_if_fail (GDA_QUERY (iface)->priv);
	query = GDA_QUERY (iface);

	/* list = query->priv->param_sources; */
/* 	while (list) { */
/* 		gda_referer_replace_refs (GDA_REFERER (list->data), replacements); */
/* 		list = g_slist_next (list); */
/* 	} */

	list = query->priv->sub_queries;
	while (list) {
		gda_referer_replace_refs (GDA_REFERER (list->data), replacements);
		list = g_slist_next (list);
	}

	list = query->priv->targets;
	while (list) {
		gda_referer_replace_refs (GDA_REFERER (list->data), replacements);
		list = g_slist_next (list);
	}

	list = query->priv->fields;
	while (list) {
		gda_referer_replace_refs (GDA_REFERER (list->data), replacements);
		list = g_slist_next (list);
	}

	list = query->priv->joins_flat;
	while (list) {
		gda_referer_replace_refs (GDA_REFERER (list->data), replacements);
		list = g_slist_next (list);
	}

	if (query->priv->cond)
		gda_referer_replace_refs (GDA_REFERER (query->priv->cond), replacements);
}

/* 
 * GdaXmlStorage interface implementation
 */

static const gchar *convert_query_type_to_str (GdaQueryType type);
static GdaQueryType  convert_str_to_query_type (const gchar *str);

static xmlNodePtr
gda_query_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL, psources = NULL;
	GdaQuery *query;
	gchar *str;
	const gchar *type;
	GSList *list;

	g_return_val_if_fail (GDA_IS_QUERY (iface), NULL);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, NULL);
	query = GDA_QUERY (iface);

	node = xmlNewNode (NULL, (xmlChar*)"gda_query");
	str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (query));
	xmlSetProp(node, (xmlChar*)"id", (xmlChar*)str);
	g_free (str);
	xmlSetProp(node, (xmlChar*)"name", (xmlChar*)gda_object_get_name (GDA_OBJECT (query)));
        xmlSetProp(node, (xmlChar*)"descr", (xmlChar*)gda_object_get_description (GDA_OBJECT (query)));

	/* save as SQL text if query is not active */
	if (! gda_referer_activate (GDA_REFERER (query))) {
		str = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, NULL, 0, error);
		if (!str)
			return NULL;
		xmlNewChild (node, NULL, (xmlChar*)"gda_query_text", BAD_CAST str);
		g_free (str);
		xmlSetProp(node, (xmlChar*)"query_type", (xmlChar*)"TXT");

		return node;
	}

	type = convert_query_type_to_str (query->priv->query_type);
	xmlSetProp(node, (xmlChar*)"query_type", (xmlChar*)type);

	/* param sources */
	list = query->priv->param_sources;
	if (list) 
		psources = xmlNewChild (node, NULL, (xmlChar*)"gda_param_sources", NULL);
	
	while (list) {
		xmlNodePtr sub = NULL;
		if (GDA_IS_DATA_MODEL_QUERY (list->data)) {
			GdaQuery *query;
			xmlNodePtr qnode;

			sub = xmlNewNode (NULL, (xmlChar*)"gda_model_query");

			g_object_get (G_OBJECT (list->data), "query", &query, NULL);
			g_assert (query);
			qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (query), error);
			g_object_unref (query);
			xmlAddChild (sub, qnode);

			g_object_get (G_OBJECT (list->data), "insert_query", &query, NULL);
			if (query) {
				qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (query), error);
				xmlAddChild (sub, qnode);
				g_object_unref (query);
			}
			g_object_get (G_OBJECT (list->data), "update_query", &query, NULL);
			if (query) {
				qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (query), error);
				xmlAddChild (sub, qnode);
				g_object_unref (query);
			}
			g_object_get (G_OBJECT (list->data), "delete_query", &query, NULL);
			if (query) {
				qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (query), error);
				xmlAddChild (sub, qnode);
				g_object_unref (query);
			}
		}
		else 
			/* simple raw data model */
			sub = gda_data_model_to_xml_node (GDA_DATA_MODEL (list->data), NULL, 0, NULL, 0,
							  gda_object_get_name (GDA_OBJECT (list->data)));

		if (sub)
			xmlAddChild (psources, sub);
		else {
			xmlFreeNode (node);
			return NULL;
		}
		list = g_slist_next (list);
	}

	if (query->priv->query_type != GDA_QUERY_TYPE_NON_PARSED_SQL) {
		if (query->priv->global_distinct)
			xmlSetProp(node, (xmlChar*)"distinct", (xmlChar*)"t");
		
		/* targets */
		list = query->priv->targets;
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
		
		/* fields */
		list = query->priv->fields;
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
		
		/* joins */
		list = query->priv->joins_flat;
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

		/* condition */
		if (query->priv->cond) {
			xmlNodePtr sub = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (query->priv->cond), error);
			if (sub)
				xmlAddChild (node, sub);
			else {
				xmlFreeNode (node);
				return NULL;
			}
		}
		
		/* fields ORDER BY */
		if (query->priv->fields_order_by) {
			xmlNodePtr sub = xmlNewChild (node, NULL, (xmlChar*)"gda_query_fields_order", NULL);
			
			list = query->priv->fields_order_by;
			while (list) {
				xmlNodePtr order = xmlNewChild (sub, NULL, (xmlChar*)"gda_query_field_ref", NULL);
				
				str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
				xmlSetProp(order, (xmlChar*)"object", (xmlChar*)str);
				g_free (str);
				
				xmlSetProp(order, (xmlChar*)"order", 
					    (xmlChar*)(g_object_get_data (G_OBJECT (list->data), "order_by_asc") ? "ASC" : "DES"));
				list = g_slist_next (list);
			}
		}

		/* sub queries */
		list = query->priv->sub_queries;
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
	}
	else {
		/* Text if SQL query */
		xmlNewChild (node, NULL, (xmlChar*)"gda_query_text", (xmlChar*)query->priv->sql);
	}

	return node;
}


static gboolean
gda_query_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaQuery *query;
	gchar *prop;
	gboolean id = FALSE;
	xmlNodePtr children;

	g_return_val_if_fail (GDA_IS_QUERY (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);
	query = GDA_QUERY (iface);
	gda_query_clean (query);
	g_return_val_if_fail (query_sql_forget (query, error), FALSE);

	if (strcmp ((gchar*)node->name, "gda_query")) {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_query>"));
		return FALSE;
	}

	/* query itself */
	prop = (gchar*)xmlGetProp(node, (xmlChar*)"id");
	if (prop) {
		if ((*prop == 'Q') && (*(prop+1) == 'U')) {
			gda_query_object_set_int_id (GDA_QUERY_OBJECT (query), atoi (prop+2));
			g_free (prop);
			id = TRUE;
		}
		else {
			g_set_error (error,
				     GDA_QUERY_ERROR,
				     GDA_QUERY_XML_LOAD_ERROR,
				     _("XML ID for a query should be QUxxx where xxx is a number"));
			return FALSE;
		}
	}	

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"name");
        if (prop) {
                gda_object_set_name (GDA_OBJECT (query), prop);
                g_free (prop);
        }

        prop = (gchar*)xmlGetProp(node, (xmlChar*)"descr");
        if (prop) {
                gda_object_set_description (GDA_OBJECT (query), prop);
                g_free (prop);
        }

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"query_type");
	if (prop) {
		query->priv->query_type = convert_str_to_query_type (prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"distinct");
	if (prop) {
		query->priv->global_distinct = *prop == 't' ? TRUE : FALSE;
		g_free (prop);
	}

	/* children nodes */
	children = node->children;
	while (children) {
		gboolean done = FALSE;

		/* parameters sources */
		if (!done && !strcmp ((gchar*)children->name, "gda_param_sources")) {
			GdaDict *dict = gda_object_get_dict (GDA_OBJECT (query));
			xmlNodePtr sparams = children->children;
			while (sparams) {
				if (!strcmp ((gchar*)sparams->name, "gda_model_query")) {
					GdaDataModel *model = NULL;
					xmlNodePtr qnode;

					qnode = sparams->children;
					while (qnode) {
						if (!strcmp ((gchar*)qnode->name, "gda_query")) {
							GdaQuery *squery;
							squery = gda_query_new (dict);
							if (gda_xml_storage_load_from_xml (GDA_XML_STORAGE (squery), 
											   qnode, error)) {
								if (!model) {
									if (!gda_query_is_select_query (squery)) {
										g_set_error (error,
											     GDA_QUERY_ERROR,
											     GDA_QUERY_XML_LOAD_ERROR,
											     _("First query of <gda_model_query> "
											       "must be a SELECT query"));
										return FALSE;
									}
									model = gda_data_model_query_new (squery);
									gda_query_add_param_source (query, model);
									g_object_unref (G_OBJECT (model));
								}
								else {
									if (gda_query_is_insert_query (squery))
										g_object_set (model, "insert_query", squery, NULL);
									else if (gda_query_is_update_query (squery))
										g_object_set (model, "update_query", squery, NULL);
									else if (gda_query_is_delete_query (squery))
										g_object_set (model, "delete_query", squery, NULL);
								}
								g_object_unref (G_OBJECT (squery));
							}
							else {
								g_object_unref (G_OBJECT (squery));
								return FALSE;
							}
						}
						qnode = qnode->next;
					}
				}
				else {
					if (!strcmp ((gchar*)sparams->name, "gda_array")) {
						GdaDataModel *model;
						GSList *errors;

						model = gda_data_model_import_new_xml_node (sparams);
						errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (model));
						if (errors) {
							GError *err = (GError *) errors->data;
							g_set_error (error, 0, 0, err->message);
							g_object_unref (model);
							model = NULL;
							return FALSE;
						}
						else {
							gda_query_add_param_source (query, model);
							g_object_unref (G_OBJECT (model));
						}
					}
				}
				sparams = sparams->next;
			}
			done = TRUE;
                }

		/* targets */
		if (!done && !strcmp ((gchar*)children->name, "gda_query_target")) {
                        GdaQueryTarget *target;
			gchar *str;

			str = (gchar*)xmlGetProp(children, (xmlChar*)"entity_ref");
			if (str) {
				target = g_object_new (GDA_TYPE_QUERY_TARGET, 
						       "dict", gda_object_get_dict (GDA_OBJECT (query)), 
						       "query", query, "entity_id", str, NULL);
				g_free (str);
				if (gda_xml_storage_load_from_xml (GDA_XML_STORAGE (target), children, error)) {
					if (!gda_query_add_target (query, target, error)) {
						g_object_unref (G_OBJECT (target));
						return FALSE;
					}
					g_object_unref (G_OBJECT (target));
				}
				else 
					return FALSE;
			}
			else {
				str = (gchar*)xmlGetProp(children, (xmlChar*)"table_name");
				if (str) {
					target = g_object_new (GDA_TYPE_QUERY_TARGET, 
							       "dict", gda_object_get_dict (GDA_OBJECT (query)), 
							       "query", query, "entity_name", str, NULL);
					g_free (str);
					if (gda_xml_storage_load_from_xml (GDA_XML_STORAGE (target), children, error)) {
						if (!gda_query_add_target (query, target, error)) {
							g_object_unref (G_OBJECT (target));
							return FALSE;
						}
						g_object_unref (G_OBJECT (target));
					}
					else 
						return FALSE;
				}
				else {
					g_set_error (error, GDA_QUERY_ERROR,
						     GDA_QUERY_XML_LOAD_ERROR,
						     _("Missing both attributes 'entity_ref' and 'table_name' from <gda_query_target>"));
					return FALSE;
				}
			}
			done = TRUE;
                }

		/* fields ORDER BY */
		if (!done && !strcmp ((gchar*)children->name, "gda_query_fields_order")) {
			xmlNodePtr order = children->children;
			gint pos = 0;

			while (order) {
				if (!strcmp ((gchar*)order->name, "gda_query_field_ref")) {
					GdaEntityField *field = NULL;
					gboolean asc = TRUE;

					prop = (gchar*)xmlGetProp(order, (xmlChar*)"object");
					if (prop) {
						field = gda_entity_get_field_by_xml_id (GDA_ENTITY (query), prop);
						if (!field)
							g_set_error (error,
								     GDA_QUERY_ERROR,
								     GDA_QUERY_XML_LOAD_ERROR,
								     _("Can't find field '%s'"), prop);
						g_free (prop);
						pos ++;
					}
					
					prop = (gchar*)xmlGetProp(order, (xmlChar*)"order");
					if (prop) {
						asc = (*prop == 'A');
						g_free (prop);
					}
					if (field) 
						gda_query_set_order_by_field (query, GDA_QUERY_FIELD (field), pos, asc);
					else 
						return FALSE;
				}
				order = order->next;
			}

			done = TRUE;
                }

		/* fields */
		if (!done && g_str_has_prefix ((gchar*)children->name, "gda_query_f")) {
			GdaQueryField *obj;

			obj = gda_query_field_new_from_xml (query, children, error);
			if (obj) {
				gda_query_add_field (GDA_ENTITY (query), GDA_ENTITY_FIELD (obj));
				g_object_unref (obj);
			}
			else
				return FALSE;
			done = TRUE;
                }

		/* joins */
		if (!done && !strcmp ((gchar*)children->name, "gda_query_join")) {
                        GdaQueryJoin *join;
			gchar *t1, *t2;

			t1 = (gchar*)xmlGetProp(children, (xmlChar*)"target1");
			t2 = (gchar*)xmlGetProp(children, (xmlChar*)"target2");

			if (t1 && t2) {
				join = gda_query_join_new_with_xml_ids (query, t1, t2);
				g_free (t1);
				g_free (t2);
				if (gda_xml_storage_load_from_xml (GDA_XML_STORAGE (join), children, error)) {
					gda_query_add_join (query, join);
					g_object_unref (G_OBJECT (join));
				}
				else
					return FALSE;
			}
			else
				return FALSE;
			done = TRUE;
                }

		/* condition */
		if (!done && !strcmp ((gchar*)children->name, "gda_query_cond")) {
			GdaQueryCondition *cond;

			cond = gda_query_condition_new (query, GDA_QUERY_CONDITION_NODE_AND);
			if (gda_xml_storage_load_from_xml (GDA_XML_STORAGE (cond), children, error)) {
				gda_query_set_condition (query, cond);
				g_object_unref (G_OBJECT (cond));
			}
			else
				return FALSE;
			done = TRUE;
                }

		/* textual query */
		if (!done && !strcmp ((gchar*)children->name, "gda_query_text")) {
			gchar *contents;
			GError *error = NULL;

			contents = (gchar*)xmlNodeGetContent (children);
			gda_query_set_sql_text (query, contents, &error);
			if (error)
				g_error_free (error);
			g_free (contents);
			done = TRUE;
                }

		/* sub queries */
		if (!done && !strcmp ((gchar*)children->name, "gda_query")) {
			GdaQuery *squery;
			GdaDict *dict = gda_object_get_dict (GDA_OBJECT (query));

			squery = gda_query_new (dict);
			if (gda_xml_storage_load_from_xml (GDA_XML_STORAGE (squery), children, error)) {
				gda_query_add_sub_query (query, squery);
				g_object_unref (G_OBJECT (squery));
			}
			else
				return FALSE;
			done = TRUE;
                }
		
		children = children->next;
	}

	if (0) {
		GSList *list;
		list = query->priv->fields;
		while (list) {
			g_print ("Query %p, field %p: refcount=%d\n", query, list->data, G_OBJECT (list->data)->ref_count);
			list = g_slist_next (list);
		}
	}

	if (id)
		return TRUE;
	else {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_XML_LOAD_ERROR,
			     _("Problem loading <gda_query>"));
		return FALSE;
	}
}

static const gchar *
convert_query_type_to_str (GdaQueryType type)
{
	switch (type) {
	default:
	case GDA_QUERY_TYPE_SELECT:
		return "SEL";
	case GDA_QUERY_TYPE_INSERT:
		return "INS";
	case GDA_QUERY_TYPE_UPDATE:
		return "UPD";
	case GDA_QUERY_TYPE_DELETE:
		return "DEL";
	case GDA_QUERY_TYPE_UNION:
		return "NION";
	case GDA_QUERY_TYPE_INTERSECT:
		return "ECT";
	case GDA_QUERY_TYPE_EXCEPT:
		return "XPT";
	case GDA_QUERY_TYPE_NON_PARSED_SQL:
		return "TXT";
	}
}

static GdaQueryType
convert_str_to_query_type (const gchar *str)
{
	switch (*str) {
	case 'S':
	default:
		return GDA_QUERY_TYPE_SELECT;
	case 'I':
		return GDA_QUERY_TYPE_INSERT;
	case 'U':
		return GDA_QUERY_TYPE_UPDATE;
	case 'D':
		return GDA_QUERY_TYPE_DELETE;
	case 'N':
		return GDA_QUERY_TYPE_UNION;
	case 'E':
		return GDA_QUERY_TYPE_INTERSECT;
	case 'T':
		return GDA_QUERY_TYPE_NON_PARSED_SQL;
	case 'X':
		return GDA_QUERY_TYPE_EXCEPT;
	}
}





/*
 * GdaRenderer interface implementation
 */

static gchar *render_sql_select (GdaQuery *query, GdaParameterList *context, 
				 GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar *render_sql_insert (GdaQuery *query, GdaParameterList *context, 
				 GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar *render_sql_update (GdaQuery *query, GdaParameterList *context, 
				 GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar *render_sql_delete (GdaQuery *query, GdaParameterList *context, 
				 GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar *render_sql_union (GdaQuery *query, GdaParameterList *context, 
				GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar *render_sql_intersect (GdaQuery *query, GdaParameterList *context, 
				    GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar *render_sql_except (GdaQuery *query, GdaParameterList *context, 
				 GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar *render_sql_non_parsed_with_params (GdaQuery *query, GdaParameterList *context, 
						 GSList **out_params_used, GdaRendererOptions options, GError **error);

static gboolean assert_coherence_all_params_present (GdaQuery *query, GdaParameterList *context, GError **error);
static gboolean assert_coherence_entities_same_fields (GdaEntity *ent1, GdaEntity *ent2);
static gboolean assert_coherence_sub_query_select (GdaQuery *query, GdaParameterList *context, GError **error);
static gboolean assert_coherence_data_select_query (GdaQuery *query, GdaParameterList *context, GError **error);
static gboolean assert_coherence_data_modify_query (GdaQuery *query, GdaParameterList *context, GError **error);
static gboolean assert_coherence_aggregate_query (GdaQuery *query, GdaParameterList *context, GError **error);

/**
 * gda_query_is_well_formed
 * @query: a #GdaQuery object
 * @context: a #GdaParameterList obtained using gda_query_get_parameter_list(), or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Tells if @query is well formed, and if @context is not %NULL, also tells if rendering to
 * SQL can be done without error
 *
 * Returns: FALSE if @query is not well formed
 */
gboolean
gda_query_is_well_formed (GdaQuery *query, GdaParameterList *context, GError **error)
{
	g_return_val_if_fail (GDA_IS_QUERY (query), FALSE);
	g_return_val_if_fail (query->priv, FALSE);

	if (context) {
		g_return_val_if_fail (GDA_IS_PARAMETER_LIST (context), FALSE);
		if (!assert_coherence_all_params_present (query, context, error))
			return FALSE;
	}

	switch (query->priv->query_type) {
	case GDA_QUERY_TYPE_SELECT:
		return assert_coherence_data_select_query (query, context, error);
	case GDA_QUERY_TYPE_INSERT:
	case GDA_QUERY_TYPE_UPDATE:
	case GDA_QUERY_TYPE_DELETE:
		return assert_coherence_data_modify_query (query, context, error);
	case GDA_QUERY_TYPE_UNION:
	case GDA_QUERY_TYPE_INTERSECT:
	case GDA_QUERY_TYPE_EXCEPT:
		return assert_coherence_aggregate_query (query, context, error);
	case GDA_QUERY_TYPE_NON_PARSED_SQL:
		return TRUE;
	default:
		g_assert_not_reached ();
	}	
	return FALSE;
}

static gchar *
gda_query_render_as_sql (GdaRenderer *iface, GdaParameterList *context, 
			 GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	GdaQuery *query;
	gchar *sql = NULL;
	
	g_return_val_if_fail (GDA_IS_QUERY (iface), NULL);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, NULL);
	query = GDA_QUERY (iface);

	/* make sure all the required parameters are in @context */
	if (!assert_coherence_all_params_present (query, context, error))
		return NULL;

	switch (query->priv->query_type) {
	case GDA_QUERY_TYPE_SELECT:
		if (assert_coherence_data_select_query (query, context, error))
			sql = render_sql_select (query, context, out_params_used, options, error);
		break;
	case GDA_QUERY_TYPE_INSERT:
		if (assert_coherence_data_modify_query (query, context, error))
			sql = render_sql_insert (query, context, out_params_used, options, error);
		break;
	case GDA_QUERY_TYPE_UPDATE:
		if (assert_coherence_data_modify_query (query, context, error))
			sql = render_sql_update (query, context, out_params_used, options, error);
		break;
	case GDA_QUERY_TYPE_DELETE:
		if (assert_coherence_data_modify_query (query, context, error))
			sql = render_sql_delete (query, context, out_params_used, options, error);
		break;
	case GDA_QUERY_TYPE_UNION:
		if (assert_coherence_aggregate_query (query, context, error))
			sql = render_sql_union (query, context, out_params_used, options, error);
		break;
	case GDA_QUERY_TYPE_INTERSECT:
		if (assert_coherence_aggregate_query (query, context, error))
			sql = render_sql_intersect (query, context, out_params_used, options, error);
		break;
	case GDA_QUERY_TYPE_EXCEPT:
		if (assert_coherence_aggregate_query (query, context, error)) {
			if (g_slist_length (query->priv->sub_queries) != 2)
				g_set_error (error,
					     GDA_QUERY_ERROR,
					     GDA_QUERY_RENDER_ERROR,
					     _("More than two sub-queries for an EXCEPT query"));
			else
				sql = render_sql_except (query, context, out_params_used, options, error);
		}
		break;
	case GDA_QUERY_TYPE_NON_PARSED_SQL:
		if (query->priv->sql) {
			if (query->priv->sql_exprs && query->priv->sql_exprs->params_specs &&
			    assert_coherence_all_params_present (query, context, NULL))
				sql = render_sql_non_parsed_with_params (query, context, out_params_used, options, error);
			else
				sql = g_strdup (query->priv->sql);
		}
		else
			g_set_error (error,
				     GDA_QUERY_ERROR,
				     GDA_QUERY_RENDER_ERROR,
				     _("Empty query"));
		break;
	default:
		g_assert_not_reached ();
	}	

	return sql;
}

/* make sure the context provides all the required values for the parameters */
static gboolean
assert_coherence_all_params_present (GdaQuery *query, GdaParameterList *context, GError **error)
{
	gboolean allok = TRUE;
	GSList *params, *plist;

	params = gda_query_get_parameters (query);

	plist = params;
	while (plist && allok) {
		GSList *for_fields = gda_parameter_get_param_users (GDA_PARAMETER (plist->data));

		while (allok && for_fields) {
			if (gda_entity_field_get_entity (GDA_ENTITY_FIELD (for_fields->data)) == GDA_ENTITY (query)) {
				gboolean found = FALSE;
				GSList *clist = NULL;
				GdaQueryField *for_field = GDA_QUERY_FIELD (for_fields->data);
				GdaParameter *invalid_param = NULL;

				if (context)
					clist = context->parameters;
				
				/* if the parameter has a value, then OK */
				if (GDA_IS_QUERY_FIELD_VALUE (for_field) && gda_query_field_value_get_value (GDA_QUERY_FIELD_VALUE (for_field)))
					found = TRUE;
				
				/* try to find a value within the context */
				while (clist && !found && !invalid_param) {
					if (g_slist_find (gda_parameter_get_param_users (GDA_PARAMETER (clist->data)), 
							  for_field)) {
						if (gda_parameter_is_valid (GDA_PARAMETER (clist->data)))
							found = TRUE;
						else
							invalid_param = GDA_PARAMETER (clist->data);
					}
					clist = g_slist_next (clist);
				}
				
				if (!found) {
					if (context) {
						allok = FALSE;
						if (invalid_param)
							g_set_error (error,
								     GDA_QUERY_ERROR,
								     GDA_QUERY_RENDER_ERROR,
								     _("Invalid parameter '%s'"),
								     gda_object_get_name (GDA_OBJECT (invalid_param)));
						else {
							g_set_error (error,
								     GDA_QUERY_ERROR,
								     GDA_QUERY_RENDER_ERROR,
								     _("Missing parameters"));
#ifdef GDA_DEBUG
							g_print ("QUERY MISSING PARAM: QU%d %s\n", 
								 gda_query_object_get_int_id (GDA_QUERY_OBJECT (query)),
								 gda_object_get_name (GDA_OBJECT (query)));
#endif
						}	
					}
				}
			}
			for_fields = g_slist_next (for_fields);
		}
		
		plist = g_slist_next (plist);
	}

	/* free all the parameters which won't be used again */
	plist = params;
	while (plist) {
		g_object_unref (G_OBJECT (plist->data));
		plist = g_slist_next (plist);
	}

	g_slist_free (params);

	return allok;
}

/* makes sure the number of fields of the two entities is the same.
 * IMPROVE: test for the field types compatibilities, but we know nothing about
 * possible (implicit or not) conversions between data types
 */
static gboolean
assert_coherence_entities_same_fields (GdaEntity *ent1, GdaEntity *ent2)
{
	gboolean retval;
	GSList *list1, *list2;

	list1 = gda_entity_get_fields (ent1);
	list2 = gda_entity_get_fields (ent2);

	retval = g_slist_length (list1) == g_slist_length (list2) ? TRUE : FALSE;

	g_slist_free (list1);
	g_slist_free (list2);

	return retval;
}

/* makes sure that all the sub queries of @query are SELECT queries, and are valid.
 * It is assumed that @query is active.
 *
 * Fills error if provided
 */
static gboolean 
assert_coherence_sub_query_select (GdaQuery *query, GdaParameterList *context, GError **error)
{
	GSList *list;
	gboolean retval = TRUE;
	GdaQuery *sub;

	list = query->priv->sub_queries;
	while (list && retval) {
		sub = GDA_QUERY (list->data);
		if ((sub->priv->query_type != GDA_QUERY_TYPE_SELECT) &&
		    (sub->priv->query_type != GDA_QUERY_TYPE_UNION) && 
		    (sub->priv->query_type != GDA_QUERY_TYPE_INTERSECT) &&
		    (sub->priv->query_type != GDA_QUERY_TYPE_EXCEPT)){
			gchar *str = gda_query_render_as_str (GDA_RENDERER (sub), context);
			
			retval = FALSE;
			g_set_error (error,
				     GDA_QUERY_ERROR,
				     GDA_QUERY_RENDER_ERROR,
				     _("Query %s is not a selection query"), str);
			g_free (str);
		}
		else 
			retval = assert_coherence_sub_query_select (sub, context, error);
		list = g_slist_next (list);
	}
	
	return retval;
}


static gboolean
assert_coherence_data_select_query (GdaQuery *query, GdaParameterList *context, GError **error)
{
	gboolean retval;

	/* make sure all the sub queries are of type SELECT (recursively) and are also valid */
	retval = assert_coherence_sub_query_select (query, context, error);
	return retval;
}

static gboolean
assert_coherence_data_modify_query (GdaQuery *query, GdaParameterList *context, GError **error)
{
	gboolean retval = TRUE;

	/* make sure there is only 1 target and the represented entity can be modified */
	if (retval && (g_slist_length (query->priv->targets) == 0)) {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_RENDER_ERROR,
			     _("No target defined to apply modifications"));
		retval = FALSE;
	}

	if (retval && (g_slist_length (query->priv->targets) > 1)) {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_RENDER_ERROR,
			     _("More than one target defined to apply modifications"));
		retval = FALSE;
	}
	
	/* make sure entity is writable */
	if (retval) {
		GdaEntity *entity = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (query->priv->targets->data));
		if (entity && !gda_entity_is_writable (entity)) {
			g_set_error (error,
				     GDA_QUERY_ERROR,
				     GDA_QUERY_RENDER_ERROR,
				     _("Entity %s is not writable"), gda_object_get_name (GDA_OBJECT (entity)));
			retval = FALSE;	
		}
	}

	/* make sure all the sub queries are of type SELECT (recursively) and are also valid */
	if (retval)
		retval = assert_coherence_sub_query_select (query, context, error);

	/* make sure all visible fields are of type GDA_QUERY_FIELD_FIELD */
	if (retval) {
		GSList *list;
		list = query->priv->fields;
		while (list && retval) {
			if (gda_query_field_is_visible (GDA_QUERY_FIELD (list->data))) {
				if (!GDA_IS_QUERY_FIELD_FIELD (list->data)) {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_RENDER_ERROR,
						     _("Modification query field has incompatible type"));
					retval = FALSE;
				}
			}
			list = g_slist_next (list);
		}
	}

	/* INSERT specific tests */
	if (retval && (query->priv->query_type == GDA_QUERY_TYPE_INSERT)) {
		/* I there is a sub query, make sure that:
		   - there is only ONE sub query of type SELECT
		   - that sub query has the same number and the same types of visible fields
		*/
		if (query->priv->sub_queries) {
			if (g_slist_length (query->priv->sub_queries) > 1) {
				g_set_error (error,
					     GDA_QUERY_ERROR,
					     GDA_QUERY_RENDER_ERROR,
					     _("An insertion query can only have one sub-query"));
				retval = FALSE;
			}
			if (retval && !assert_coherence_entities_same_fields (GDA_ENTITY (query),
									      GDA_ENTITY (query->priv->sub_queries->data))) {
				g_set_error (error,
					     GDA_QUERY_ERROR,
					     GDA_QUERY_RENDER_ERROR,
					     _("Insertion query fields incompatible with sub-query's fields"));
				retval = FALSE;
			}
		}
		else {
			/* If there is no sub query, then make sure all the fields are of values */
			GSList *list;
			list = query->priv->fields;
			while (list && retval) {
				if (gda_query_field_is_visible (GDA_QUERY_FIELD (list->data))) {
					GdaObject *value_prov;
					g_object_get (G_OBJECT (list->data), "value_provider", &value_prov, NULL);
					if (value_prov && 
					    (GDA_IS_QUERY_FIELD_FIELD (value_prov) ||
					    (GDA_IS_QUERY_FIELD_ALL (value_prov)))) {
						g_set_error (error,
							     GDA_QUERY_ERROR,
							     GDA_QUERY_RENDER_ERROR,
							     _("Insertion query field has incompatible value assignment"));
						retval = FALSE;
					}
					if (value_prov)
						g_object_unref (value_prov);
				}
				list = g_slist_next (list);
			}
		}
		
		/* make sure there is no condition */
		if (retval && query->priv->cond) {
			g_set_error (error,
				     GDA_QUERY_ERROR,
				     GDA_QUERY_RENDER_ERROR,
				     _("Insertion query can't have any conditions"));
			retval = FALSE;
		}
	}


	/* DELETE specific tests */
	if (retval && (query->priv->query_type == GDA_QUERY_TYPE_DELETE)) {
		GSList *list;
		list = query->priv->fields;
		while (list && retval) {
			if (gda_query_field_is_visible (GDA_QUERY_FIELD (list->data))) {
				g_set_error (error,
					     GDA_QUERY_ERROR,
					     GDA_QUERY_RENDER_ERROR,
					     _("Deletion query can't have any visible field"));
				retval = FALSE;
			}
			list = g_slist_next (list);
		}
	}

	/* UPDATE specific tests */
	if (retval && (query->priv->query_type == GDA_QUERY_TYPE_UPDATE)) {
		GSList *list;
		list = query->priv->fields;
		while (list && retval) {
			if (gda_query_field_is_visible (GDA_QUERY_FIELD (list->data))) {
				GdaObject *value_prov;
				g_object_get (G_OBJECT (list->data), "value_provider", &value_prov, NULL);
				if (value_prov && 
				    (GDA_IS_QUERY_FIELD_ALL (value_prov))) {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_RENDER_ERROR,
						     _("Update query field has incompatible value assignment"));
					retval = FALSE;
				}
				if (value_prov)
					g_object_unref (value_prov);
			}
			list = g_slist_next (list);
		}
	}

	return retval;
}

static gboolean
assert_coherence_aggregate_query (GdaQuery *query, GdaParameterList *context, GError **error)
{
	gboolean retval;

	/* FIXME: 
	   - make sure all the fields in each sub query are of the same type (and same number of them)
	*/

	/* make sure all the sub queries are of type SELECT (recursively) and are also valid */
	retval = assert_coherence_sub_query_select (query, context, error);

	if (retval && (g_slist_length (query->priv->targets) != 0)) {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_RENDER_ERROR,
			     _("An aggregate type (UNION, etc) of query can't have any targets"));
		retval = FALSE;
	}

	if (retval && query->priv->cond) {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_RENDER_ERROR,
			     _("An aggregate type (UNION, etc) of query can't have any conditions"));
		retval = FALSE;
	}

	return retval;
}

/* Structure used while rendering a JoinsPack: each time a join is analysed, such a structure is added to a list.
 * --> 'target' is always not NULL and represents the GdaQueryTarget the node brings.
 * --> 'first_join', if present, is the join which will be used to get the join type (INNER, etc)
 * --> 'other_joins', if present is the list of other joins participating in 'cond'
 * --> 'cond' is the string representing the join condition.
 */
typedef struct {
	GdaQueryTarget *target;
	GdaQueryJoin   *first_join;
	GSList   *other_joins;
	GString  *cond;
} JoinRenderNode;
#define JOIN_RENDER_NODE(x) ((JoinRenderNode *) x)

static gchar *render_join_condition (GdaQueryJoin *join, GdaParameterList *context, 
				     GSList **out_params_used, GdaRendererOptions options, GError **error, GSList *fk_constraints);
static gchar *
render_sql_select (GdaQuery *query, GdaParameterList *context, GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	GString *sql;
	gchar *retval, *str;
	GSList *list;
	gboolean first;
	gboolean err = FALSE;
	GdaDict *dict = gda_object_get_dict (GDA_OBJECT (query));
	GSList *fk_constraints = NULL;
	gboolean pprint = options & GDA_RENDERER_EXTRA_PRETTY_SQL;

	fk_constraints = gda_dict_database_get_all_constraints (gda_dict_get_database (dict));

	/* query is supposed to be active and of a good constitution here */
	sql = g_string_new ("SELECT ");

	/* DISTINCT */
	if (query->priv->global_distinct) 
		g_string_append (sql, "DISTINCT ");
	else {
		/* FIXME */
	}

	/* fields */
	first = TRUE;
	list = query->priv->fields;
	while (list && !err) {
		if (gda_query_field_is_visible (GDA_QUERY_FIELD (list->data))) {
			if (first)
				first = FALSE;
			else {
				if (pprint)
					g_string_append (sql, ",\n\t");
				else
					g_string_append (sql, ", ");
			}

			str = gda_renderer_render_as_sql (GDA_RENDERER (list->data), context, out_params_used, 
							  options, error);
			if (str) {
				g_string_append (sql, str);
				g_free (str);

				str = (gchar *) gda_query_field_get_alias (GDA_QUERY_FIELD (list->data));
				if (str && *str) {
					if (strstr (str, " "))
						g_string_append_printf (sql, " AS \"%s\"", str);
					else
						g_string_append_printf (sql, " AS %s", str);
				}
			}
			else {
				if (error && *error)
					err = TRUE;
				else
					g_string_append (sql, "NULL");
			}
		}
		list = g_slist_next (list);
	}

	
	/* targets and joins */
	if (query->priv->targets && !err) {
		GSList *all_joined = NULL; /* all the joined targets */
		GSList *packs, *list;
		JoinsPack *pack;
		GdaQueryJoin *join;

		first = TRUE;
		if (pprint)
			g_string_append (sql, "\nFROM ");
		else
			g_string_append (sql, " FROM ");
		packs = query->priv->joins_pack;
		while (packs && !err) {
			/* preparing the list of JoinRenderNodes */
			GSList *join_nodes = NULL;

			if (first) 
				first = FALSE;
			else {
				if (pprint)
					g_string_append (sql, ",\n\t");
				else
					g_string_append (sql, ", ");
			}

			pack = JOINS_PACK (packs->data);
			list = pack->joins;
			while (list && !err) {
				GSList *jnode;
				gint targets_found = 0;
				GdaQueryTarget *l_target, *r_target;
				JoinRenderNode *node = NULL;
				gchar *str;

				join = GDA_QUERY_JOIN (list->data);
				l_target = gda_query_join_get_target_1 (join);
				r_target = gda_query_join_get_target_2 (join);

				/* Find a JoinRenderNode for the current join (in 'node')*/
				jnode = join_nodes;
				while (jnode && (targets_found < 2)) {
					if (JOIN_RENDER_NODE (jnode->data)->target == l_target)
						targets_found ++;
					if (JOIN_RENDER_NODE (jnode->data)->target == r_target)
						targets_found ++;
					if (targets_found == 2)
						node = JOIN_RENDER_NODE (jnode->data);
					jnode = g_slist_next (jnode);
				}
				g_assert (targets_found <= 2);
				switch (targets_found) {
				case 0:
					node = g_new0 (JoinRenderNode, 1);
					node->target = l_target;
					join_nodes = g_slist_append (join_nodes, node);
				case 1:
					node = g_new0 (JoinRenderNode, 1);
					node->target = r_target;
					node->first_join = join;
					join_nodes = g_slist_append (join_nodes, node);
					break;
				default:
					node->other_joins = g_slist_append (node->other_joins, join);
					break;
				}

				/* render the join condition in 'node->cond' */
				str = render_join_condition (join, context, out_params_used, 
							     options, error, fk_constraints);
				if (str) {
					if (!node->cond)
						node->cond = g_string_new ("");
					if (node->other_joins)
						g_string_append (node->cond, " AND ");
					g_string_append (node->cond, str);
				}
				else 
					err = TRUE;

				list = g_slist_next (list);
			}
			
			/* actual rendering of the joins in the JoinsPack */
			list = join_nodes;
			while (list) {
				if (list != join_nodes) {
					if (pprint)
						g_string_append (sql, "\n\t");
					else
						g_string_append (sql, " ");
				}
				
				/* join type if applicable */
				if (JOIN_RENDER_NODE (list->data)->first_join) {
					g_string_append (sql, gda_query_join_render_type (JOIN_RENDER_NODE (list->data)->first_join));
					g_string_append (sql, " ");
				}

				/* target */
				str = gda_renderer_render_as_sql (GDA_RENDERER (JOIN_RENDER_NODE (list->data)->target), 
								  context, out_params_used, options, error);
				
				if (str) {
					g_string_append (sql, str);
					g_free (str);
				}
				else
					err = TRUE;

				/* condition */
				if (JOIN_RENDER_NODE (list->data)->cond) {
					if (JOIN_RENDER_NODE (list->data)->cond->str &&
					    *JOIN_RENDER_NODE (list->data)->cond->str)
						g_string_append_printf (sql, " ON (%s)", 
									JOIN_RENDER_NODE (list->data)->cond->str);
				}
				list = g_slist_next (list);
			}

			/* free the list of JoinRenderNodes */
			list = join_nodes;
			while (list) {
				if (JOIN_RENDER_NODE (list->data)->other_joins)
					g_slist_free (JOIN_RENDER_NODE (list->data)->other_joins);
				if (JOIN_RENDER_NODE (list->data)->cond)
					g_string_free (JOIN_RENDER_NODE (list->data)->cond, TRUE);
				g_free (list->data);
				list = g_slist_next (list);
			}
			if (join_nodes)
				g_slist_free (join_nodes);

			/* update the all_joined list */
			all_joined = g_slist_concat (all_joined, g_slist_copy (pack->targets));

			packs = g_slist_next (packs);
		}

		/* Adding targets in no join at all */
		list = query->priv->targets;
		while (list && !err) {
			if (!g_slist_find (all_joined, list->data)) {
				if (first) 
					first = FALSE;
				else
					g_string_append (sql, ", ");
				str = gda_renderer_render_as_sql (GDA_RENDERER (list->data), context, 
								  out_params_used, options, error);
				if (str) {
					g_string_append (sql, str);
					g_free (str);
				}
				else
					err = TRUE;
			}
			list = g_slist_next (list);
		}
		g_string_append (sql, " ");
		g_slist_free (all_joined);
	}

	/* GROUP BY */
	/* FIXME */	

	/* condition */
	if (query->priv->cond) {
		if (pprint)
			g_string_append (sql, "\nWHERE ");
		else
			g_string_append (sql, "WHERE ");
		str = gda_renderer_render_as_sql (GDA_RENDERER (query->priv->cond), context, 
						  out_params_used, options, error);
		if (str) {
			g_string_append (sql, str);
			g_string_append (sql, " ");
			g_free (str);
		}
		else
			err = TRUE;
	}
	
	/* ORDER BY */
	list = query->priv->fields_order_by;
	while (list) {
		if (0) { /* render using field numbers */
			gint pos = gda_query_get_field_index (GDA_ENTITY (query), GDA_ENTITY_FIELD (list->data));
			if (pos > -1) {
				if (list == query->priv->fields_order_by) { 
					if (pprint)
						g_string_append (sql, "\nORDER BY ");
					else
						g_string_append (sql, "ORDER BY ");
				}
				else
					g_string_append (sql, ", ");
				
				g_string_append_printf (sql, "%d", pos + 1);
				if (! g_object_get_data (G_OBJECT (list->data), "order_by_asc"))
					g_string_append (sql, " DESC");
			}
		}
		else { /* render using field names */
			str = gda_renderer_render_as_sql (GDA_RENDERER (list->data), context, 
							  out_params_used, options, error);
			if (str) {
				if (list == query->priv->fields_order_by){ 
					if (pprint)
						g_string_append (sql, "\nORDER BY ");
					else
						g_string_append (sql, "ORDER BY ");
				} 
				else
					g_string_append (sql, ", ");	
				g_string_append (sql, str);
				if (! g_object_get_data (G_OBJECT (list->data), "order_by_asc"))
					g_string_append (sql, " DESC");
			}
		}
			
			
		list = g_slist_next (list);
	}

	if (!err) 
		retval = sql->str;
	else 
		retval = NULL;
	g_string_free (sql, err);

	if (fk_constraints)
		g_slist_free (fk_constraints);

	return retval;
}

static gchar *
render_join_condition (GdaQueryJoin *join, GdaParameterList *context, GSList **out_params_used, 
		       GdaRendererOptions options, GError **error, GSList *fk_constraints)
{
	GString *string;
	gchar *retval = NULL;
	gboolean joincond = FALSE;
	GdaQueryCondition *cond;
	gboolean err = FALSE;
	
	string = g_string_new ("");
	
	if ((cond = gda_query_join_get_condition (join))) {
		gchar *str = gda_renderer_render_as_sql (GDA_RENDERER (cond), context, out_params_used, options, error);
		joincond = TRUE;

		if (!str)
			err = TRUE;
		else {
			g_string_append (string, str);
			g_free (str);
		}
	}
	else {
		GdaEntity *ent1, *ent2;
		ent1 = gda_query_target_get_represented_entity (gda_query_join_get_target_1 (join));
		ent2 = gda_query_target_get_represented_entity (gda_query_join_get_target_2 (join));

		if (GDA_IS_DICT_TABLE (ent1) && GDA_IS_DICT_TABLE (ent2)) {
			/* find a FK in the database constraints for the two targets */
			GSList *fklist;
			GdaDictConstraint *fkcons = NULL, *fkptr;
			gboolean same_order = TRUE;

			fklist = fk_constraints;
			while (fklist && !fkcons) {
				fkptr = GDA_DICT_CONSTRAINT (fklist->data);
				if ((gda_dict_constraint_get_constraint_type (fkptr) == CONSTRAINT_FOREIGN_KEY) &&
				    (((gda_dict_constraint_get_table (fkptr) == GDA_DICT_TABLE (ent1)) &&
				      (gda_dict_constraint_fkey_get_ref_table (fkptr) == GDA_DICT_TABLE (ent2))) ||
				     ((gda_dict_constraint_get_table (fkptr) == GDA_DICT_TABLE (ent2)) &&
				      (gda_dict_constraint_fkey_get_ref_table (fkptr) == GDA_DICT_TABLE (ent1))))) {
					fkcons = fkptr;

					if ((gda_dict_constraint_get_table (fkptr) == GDA_DICT_TABLE (ent2)) &&
					    (gda_dict_constraint_fkey_get_ref_table (fkptr) == GDA_DICT_TABLE (ent1)))
						same_order = FALSE;
				}
				fklist = g_slist_next (fklist);
			}

			if (fkcons) {
				GSList *fkpairs;
				GdaDictConstraintFkeyPair *pair;
				gboolean fkfirst = TRUE;

				fkpairs = gda_dict_constraint_fkey_get_fields (fkcons);
				fklist = fkpairs;
				while (fklist) {
					pair = GDA_DICT_CONSTRAINT_FK_PAIR (fklist->data);
					if (fkfirst)
						fkfirst = FALSE;
					else
						g_string_append (string, " AND ");
					if (same_order)
						g_string_append (string, gda_query_target_get_alias (gda_query_join_get_target_1 (join)));
					else
						g_string_append (string, gda_query_target_get_alias (gda_query_join_get_target_2 (join)));
					g_string_append (string, ".");
					g_string_append (string, gda_entity_field_get_name (GDA_ENTITY_FIELD (pair->fkey)));
					g_string_append (string, "=");
					if (same_order)
						g_string_append (string, gda_query_target_get_alias (gda_query_join_get_target_2 (join)));
					else
						g_string_append (string, gda_query_target_get_alias (gda_query_join_get_target_1 (join)));
					g_string_append (string, ".");
					g_string_append (string, gda_entity_field_get_name (GDA_ENTITY_FIELD (pair->ref_pkey)));
					g_free (fklist->data);
					fklist = g_slist_next (fklist);
				}
				g_slist_free (fkpairs);
				joincond = TRUE;
			}
		}
	}

	if (err) {
		retval = NULL;
		g_string_free (string, TRUE);
	}
	else {
		retval = string->str;
		g_string_free (string, FALSE);
	}

	return retval;
}


static gchar *
render_sql_insert (GdaQuery *query, GdaParameterList *context, GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	GString *sql;
	gchar *retval;
	GdaEntity *ent;
	GSList *list;
	gboolean first, err = FALSE;
	gboolean pprint = options & GDA_RENDERER_EXTRA_PRETTY_SQL;
	GSList *printed_fields = NULL;
	GSList *printed_values = NULL;

	/* compute the list of fields apprearing after the INSERT: only the fields with non DEFAULT values */
	if (query->priv->sub_queries)
		printed_fields = gda_entity_get_fields (GDA_ENTITY (query));
	else {
		GSList *vfields;
		GError *my_error = NULL;

		vfields = gda_entity_get_fields (GDA_ENTITY (query));
		list = vfields;
		while (list && !err) {
			GdaEntityField *value_prov;
			gchar *str;
			gboolean add_field = TRUE;

			g_object_get (G_OBJECT (list->data), "value_provider", &value_prov, NULL);
			g_assert (value_prov);
			str = gda_renderer_render_as_sql (GDA_RENDERER (value_prov), context, out_params_used,
							  options | GDA_RENDERER_ERROR_IF_DEFAULT, &my_error);
			if (!str) {
				if (my_error) {
					if (my_error->code == GDA_QUERY_FIELD_VALUE_DEFAULT_PARAM_ERROR) {
						/* this value is a DEFAULT value, don't print it */
						add_field = FALSE;
					}
					else {
						/* make sure @error is set */
						gda_renderer_render_as_sql (GDA_RENDERER (value_prov), context, 
									    out_params_used, options, error);
						err = TRUE;
					}
					g_error_free (my_error);
				}
				else
					str = g_strdup ("NULL");
			}
			g_object_unref (value_prov);
			
			if (add_field) {
				gchar *tmpstr;
				tmpstr = gda_renderer_render_as_sql (GDA_RENDERER (list->data), context, out_params_used,
								     options | GDA_RENDERER_FIELDS_NO_TARGET_ALIAS, NULL);
				if (tmpstr) {
					printed_fields = g_slist_append (printed_fields, list->data);
					g_free (tmpstr);
				}
				printed_values = g_slist_append (printed_values, str);
			}
			else
				g_free (str);

			list = g_slist_next (list);
		}
		g_slist_free (vfields);
	}

	if (err) {
		if (printed_fields)
			g_slist_free (printed_fields);
		if (printed_values) {
			g_slist_foreach (printed_values, (GFunc) g_free, NULL);
			g_slist_free (printed_values);
		}

		return NULL;
	}

	/* query is supposed to be active and of a good constitution here */
	sql = g_string_new ("INSERT INTO ");
	ent = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (query->priv->targets->data));
	if (ent)
		g_string_append (sql, gda_object_get_name (GDA_OBJECT (ent)));
	else
		g_string_append (sql, gda_query_target_get_represented_table_name (GDA_QUERY_TARGET (query->priv->targets->data)));
	if (pprint) g_string_append (sql, "\n\t");

	/* list of fields */
	if (printed_fields) {
		if (g_slist_length (printed_fields) != g_slist_length (printed_values)) {
			g_set_error (error, GDA_QUERY_ERROR, GDA_QUERY_RENDER_ERROR,
				     _("Can't correctly print the list of columns to use."));
			if (printed_values) {
				g_slist_foreach (printed_values, (GFunc) g_free, NULL);
				g_slist_free (printed_values);
			}
			
			return NULL;
		}

		g_string_append (sql, " (");
		list = printed_fields;
		first = TRUE;
		while (list) {
			gchar *tmpstr;

			if (first) 
				first = FALSE;
			else {
				if (pprint)
					g_string_append (sql, ",\n\t");
				else
					g_string_append (sql, ", ");
			}
			tmpstr = gda_renderer_render_as_sql (GDA_RENDERER (list->data), context, out_params_used,
							     options | GDA_RENDERER_FIELDS_NO_TARGET_ALIAS, NULL);
			g_string_append (sql, tmpstr);
			g_free (tmpstr);
			
			list = g_slist_next (list);
		}
		g_string_append (sql, ") ");
		g_slist_free (printed_fields);
		if (pprint) g_string_append (sql, "\n");
	}
	else
		g_string_append_c (sql, ' ');

	/* list of values */
	if (query->priv->sub_queries) {
		gchar *str = gda_query_render_as_sql (GDA_RENDERER (query->priv->sub_queries->data), context, 
						      out_params_used, options, error);
		if (str) {
			g_string_append (sql, str);
			g_free (str);
		}
		else
			err = TRUE;
	}
	else {
		g_string_append (sql, "VALUES (");
		if (pprint) g_string_append (sql, "\n\t");
		
		first = TRUE;
		list = printed_values;
		while (list) {
			if (first) 
				first = FALSE;
			else {
				if (pprint)
					g_string_append (sql, ",\n\t");
				else
					g_string_append (sql, ", ");
			}
			
			g_string_append (sql, (gchar *) list->data);

			list = g_slist_next (list);
		}

		g_slist_foreach (printed_values, (GFunc) g_free, NULL);
		g_slist_free (printed_values);
		g_string_append (sql, ")");
	}

	if (!err)
		retval = sql->str;
	else
		retval = NULL;

	g_string_free (sql, err);
	return retval;
}

static gchar *
render_sql_update (GdaQuery *query, GdaParameterList *context, GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	GString *sql;
	gchar *retval;
	GdaEntity *ent;
	GSList *list;
	gboolean first, err = FALSE;
	gboolean pprint = options & GDA_RENDERER_EXTRA_PRETTY_SQL;

	/* query is supposed to be active and of a good constitution here */
	sql = g_string_new ("UPDATE ");
	ent = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (query->priv->targets->data));
	if (ent)
		g_string_append (sql, gda_object_get_name (GDA_OBJECT (ent)));
	else
		g_string_append (sql, gda_query_target_get_represented_table_name (GDA_QUERY_TARGET (query->priv->targets->data)));
	if (pprint)
		g_string_append (sql, "\nSET ");
	else
		g_string_append (sql, " SET ");
	list = query->priv->fields;
	first = TRUE;
	while (list && !err) {
		if (gda_query_field_is_visible (GDA_QUERY_FIELD (list->data))) {
			GdaEntityField *value_prov;
			gchar *str;
			if (first) 
				first = FALSE;
			else {
				if (pprint)
					g_string_append (sql, ",\n\t");
				else
					g_string_append (sql, ", ");
			}
			str = gda_renderer_render_as_sql (GDA_RENDERER (list->data), context, out_params_used,
							  options | GDA_RENDERER_FIELDS_NO_TARGET_ALIAS, error);
			if (str) {
				g_string_append (sql, str);
				g_free (str);
			}
			else
				err = TRUE;
			g_string_append (sql, "=");

			g_object_get (G_OBJECT (list->data), "value_provider", &value_prov, NULL);
			if (value_prov) {
				gchar *str;
				str = gda_renderer_render_as_sql (GDA_RENDERER (value_prov), context, 
								  out_params_used, options, error);
				if (str) {
					g_string_append (sql, str);
					g_free (str);
				}
				else {
					if (error && *error)
						err = TRUE;
					else
						g_string_append (sql, "NULL");
				}
				g_object_unref (value_prov);
			}
			else {
				/* signal an error */
				g_set_error (error,
					     GDA_QUERY_ERROR,
					     GDA_QUERY_RENDER_ERROR,
					     _("Missing values"));
				err = TRUE;
			}
		}
		list = g_slist_next (list);
	}
	g_string_append (sql, " ");


	if (!err && query->priv->cond) {
		gchar *str;
		if (pprint) 
			g_string_append (sql, "\nWHERE ");
		else
			g_string_append (sql, "WHERE ");
		str = gda_renderer_render_as_sql (GDA_RENDERER (query->priv->cond), context, out_params_used, 
						  options | GDA_RENDERER_FIELDS_NO_TARGET_ALIAS, error);
		if (str) {
			g_string_append (sql, str);
			g_free (str);
		}
		else
			err = TRUE;
	}

	if (!err)
		retval = sql->str;
	else
		retval = NULL;

	g_string_free (sql, err);
	return retval;
}

static gchar *
render_sql_delete (GdaQuery *query, GdaParameterList *context, GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	GString *sql;
	gchar *retval;
	GdaEntity *ent;
	gboolean err = FALSE;
	gboolean pprint = options & GDA_RENDERER_EXTRA_PRETTY_SQL;

	/* query is supposed to be active and of a good constitution here */
	sql = g_string_new ("DELETE FROM ");
	ent = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (query->priv->targets->data));
	if (ent)
		g_string_append (sql, gda_object_get_name (GDA_OBJECT (ent)));
	else
		g_string_append (sql, gda_query_target_get_represented_table_name (GDA_QUERY_TARGET (query->priv->targets->data)));

	if (pprint) g_string_append (sql, "\n");
	if (query->priv->cond) {
		gchar *str;
		g_string_append (sql, " WHERE ");
		str = gda_renderer_render_as_sql (GDA_RENDERER (query->priv->cond), context, out_params_used,
						  options | GDA_RENDERER_FIELDS_NO_TARGET_ALIAS, error);
		if (str) {
			g_string_append (sql, str);
			g_free (str);
		}
		else
			err = TRUE;
	}

	if (!err)
		retval = sql->str;
	else
		retval = NULL;

	g_string_free (sql, FALSE);
	return retval;
}

static gchar *
render_sql_union (GdaQuery *query, GdaParameterList *context, GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	GString *sql;
	gchar *retval, *str;
	GSList *list;
	gboolean first = TRUE;
	gboolean err = FALSE;

	/* query is supposed to be active here, and of good constitution */
	sql = g_string_new ("");
	list = query->priv->sub_queries;
	while (list && !err) {
		if (first)
			first = FALSE;
		else
			g_string_append (sql, " UNION ");
		str = gda_query_render_as_sql (GDA_RENDERER (list->data), context, out_params_used,
					       options, error);
		if (str) {
			g_string_append_printf (sql, "(%s)", str);
			g_free (str);
		}
		else err = TRUE;

		list = g_slist_next (list);
	}

	if (!err)
		retval = sql->str;
	else
		retval = NULL;

	g_string_free (sql, err);
	return retval;
}

static gchar *
render_sql_intersect (GdaQuery *query, GdaParameterList *context, GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	GString *sql;
	gchar *retval, *str;
	GSList *list;
	gboolean first = TRUE;
	gboolean err = FALSE;

	/* query is supposed to be active here, and of good constitution */
	sql = g_string_new ("");
	list = query->priv->sub_queries;
	while (list && !err) {
		if (first)
			first = FALSE;
		else
			g_string_append (sql, " INTERSECT ");
		str = gda_query_render_as_sql (GDA_RENDERER (list->data), context, 
					       out_params_used, options, error);
		if (str) {
			g_string_append_printf (sql, "(%s)", str);
			g_free (str);
		}
		else err = TRUE;

		list = g_slist_next (list);
	}

	if (!err)
		retval = sql->str;
	else
		retval = NULL;

	g_string_free (sql, err);
	return retval;
}

static gchar *
render_sql_except (GdaQuery *query, GdaParameterList *context, GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	GString *sql;
	gchar *retval, *str;
	GSList *list;
	gboolean first = TRUE;
	gboolean err = FALSE;

	/* query is supposed to be active here, and of good constitution */
	sql = g_string_new ("");
	list = query->priv->sub_queries;
	while (list && !err) {
		if (first)
			first = FALSE;
		else
			g_string_append (sql, " EXCEPT ");
		str = gda_query_render_as_sql (GDA_RENDERER (list->data), context, 
					       out_params_used, options, error);
		if (str) {
			g_string_append_printf (sql, "(%s)", str);
			g_free (str);
		}
		else err = TRUE;

		list = g_slist_next (list);
	}

	if (!err)
		retval = sql->str;
	else
		retval = NULL;

	g_string_free (sql, err);
	return retval;
}

static gchar *
render_sql_non_parsed_with_params (GdaQuery *query, GdaParameterList *context, GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	GString *string;
	gchar *retval;
	GList *list;
	
	g_return_val_if_fail (query->priv->sql_exprs, NULL);
	g_return_val_if_fail (query->priv->sql_exprs->params_specs, NULL);
	
	/* if no context, then return as we don't have any default value for the parameters */
	string = g_string_new("");
	list = query->priv->sql_exprs->expr_list;
	while (list) {
		GdaDelimiterExpr *expr = GDA_DELIMITER_SQL_EXPR (list->data);

		/*if (list != query->priv->sql_exprs->expr_list)
		  g_string_append (string, " ");*/

		if (expr->pspec_list) {
			GdaQueryFieldValue *found = NULL;
			GSList *fields;
			gchar *str = NULL;
			
			fields = query->priv->fields;
			while (fields && !found) {
				if (g_object_get_data (G_OBJECT (fields->data), "pspeclist") == expr->pspec_list)
					found = GDA_QUERY_FIELD_VALUE (fields->data);
				fields = g_slist_next (fields);
			}
			
			if (found) 
				str = gda_renderer_render_as_sql (GDA_RENDERER (found), context, out_params_used,
								  options, error);
			
			if (!str) {
				g_string_free (string, TRUE);
				return NULL;
			}
			if (list != query->priv->sql_exprs->expr_list)
				g_string_append_c (string, ' ');
			g_string_append (string, str);
			g_free (str);
		}
		else
			if (expr->sql_text) 
				g_string_append (string, expr->sql_text);

		list = g_list_next (list);
	}

	retval = string->str;
	g_string_free (string, FALSE);
	return retval;
}

static gchar *
gda_query_render_as_str (GdaRenderer *iface, GdaParameterList *context)
{
	GdaQuery *query;
	gchar *str;
	const gchar *cstr;

	g_return_val_if_fail (GDA_IS_QUERY (iface), NULL);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, NULL);
	query = GDA_QUERY (iface);

	cstr = gda_object_get_name (GDA_OBJECT (query));
	if (cstr && *cstr)
		str = g_strdup_printf (_("Query '%s'"), cstr);
	else
		str = g_strdup (_("Unnamed Query"));
	       
	return str;
}

static gboolean
gda_query_is_valid (GdaRenderer *iface, GdaParameterList *context, GError **error)
{
	g_return_val_if_fail (GDA_IS_QUERY (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY (iface)->priv, FALSE);

	return assert_coherence_all_params_present (GDA_QUERY (iface), context, error);
}

/**
 * gda_query_expand_all_field
 * @query: a #GdaQuery object
 * @target: a #GdaQueryTarget, or %NULL
 *
 * Converts each visible "target.*" (#GdaQueryFieldAll) field into its list of fields. For example "t1.*" becomes "t1.a, t1.b"
 * if table t1 is composed of fields "a" and "b". The original GdaQueryFieldAll field is not removed, but
 * simply rendered non visible.
 *
 * The returned list must be free'd by the caller using g_slist_free().
 *
 * Returns: a new list of the #GdaQueryField objects which have been created
 */
GSList *
gda_query_expand_all_field (GdaQuery *query, GdaQueryTarget *target)
{
	GSList *list, *retlist = NULL;
	
	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (query->priv, NULL);
	g_return_val_if_fail (!target || (GDA_IS_QUERY_TARGET (target) && (gda_query_target_get_query (target) == query)), NULL);

	list = query->priv->fields;
	while (list) {
		if (GDA_IS_QUERY_FIELD_ALL (list->data) && gda_query_field_is_visible (GDA_QUERY_FIELD (list->data))) {
			GdaQueryTarget *t;
			
			t = gda_query_field_all_get_target (GDA_QUERY_FIELD_ALL (list->data));
			if (!target || (t == target)) {
				GSList *entfields = NULL, *list2;
				GdaEntityField *newfield;
				GdaEntity *repr_entity;

				repr_entity = gda_query_target_get_represented_entity (t);
				if (repr_entity)
					entfields = gda_entity_get_fields (repr_entity);
				else 
					g_warning (_("Could expand '%s.*' into a list of fields"), 
						   gda_query_target_get_represented_table_name (t));
				list2 = entfields;
				while (list2) {
					newfield = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
									    "dict", gda_object_get_dict (GDA_OBJECT (query)), 
									    "query", query, NULL);
					g_object_set (G_OBJECT (newfield), "target", t, "field", list2->data, NULL);
					g_object_set_data (G_OBJECT (newfield), "star_field", list->data);
					retlist = g_slist_append (retlist, newfield);
					gda_entity_add_field_before (GDA_ENTITY (query), newfield,
								    GDA_ENTITY_FIELD (list->data));
					gda_object_set_name (GDA_OBJECT (newfield), 
								gda_object_get_name (GDA_OBJECT (list2->data)));
					gda_object_set_description (GDA_OBJECT (newfield), 
								 gda_object_get_description (GDA_OBJECT (list2->data)));
					g_object_unref (G_OBJECT (newfield));
					list2 = g_slist_next (list2);
				}
				g_slist_free (entfields);
				gda_query_field_set_visible (GDA_QUERY_FIELD (list->data), FALSE);
			}
		}
		list = g_slist_next (list);
	}

	return retlist;
}


/**
 * gda_query_order_fields_using_join_conds
 * @query: a #GdaQuery object
 *
 * Re-orders the fields in @query using the joins' conditions: for each join condition,
 * the used query fields are grouped together near the 1st visible field.
 */
void
gda_query_order_fields_using_join_conds (GdaQuery *query)
{
	GSList *list;
	gboolean reordered = FALSE;

	g_return_if_fail (query && GDA_IS_QUERY (query));
	g_return_if_fail (query->priv);

	list = query->priv->joins_flat;
	while (list) {
		GdaQueryCondition *cond = gda_query_join_get_condition (GDA_QUERY_JOIN (list->data));
		if (cond) {
			GSList *refs, *ptr;
			gint minpos = G_MAXINT;

			refs = gda_query_condition_get_ref_objects_all (cond);
			ptr = refs;
			while (ptr) {
				if (GDA_IS_QUERY_FIELD_FIELD (ptr->data) && 
				    gda_query_field_is_visible (GDA_QUERY_FIELD (ptr->data)) &&
				    g_slist_find (query->priv->fields, ptr->data)) {
					gint pos = g_slist_index (query->priv->fields, ptr->data);
					if (pos < minpos)
						minpos = pos;
				}
				ptr = g_slist_next (ptr);
			}

			if (minpos != G_MAXINT) {
				ptr = refs;
				while (ptr) {
					if (GDA_IS_QUERY_FIELD_FIELD (ptr->data) && 
					    g_slist_find (query->priv->fields, ptr->data)) {
						if (g_slist_index (query->priv->fields, ptr->data) > minpos) {
							query->priv->fields = g_slist_remove (query->priv->fields, 
											      ptr->data);
							query->priv->fields = g_slist_insert (query->priv->fields, 
											      ptr->data, ++minpos);
							reordered = TRUE;
						}
					}
					ptr = g_slist_next (ptr);
				}
			}
			
			g_slist_free (refs);
		}
		list = g_slist_next (list);
	}

	if (reordered) {
#ifdef GDA_DEBUG_signal
		g_print (">> 'FIELDS_ORDER_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit_by_name (G_OBJECT (query), "fields_order_changed");
#ifdef GDA_DEBUG_signal
		g_print ("<< 'FIELDS_ORDER_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
}
