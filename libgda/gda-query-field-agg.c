/* gda-query-field-agg.c
 *
 * Copyright (C) 2005 Vivien Malerba
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
#include "gda-query-field-agg.h"
#include "gda-xml-storage.h"
#include "gda-entity-field.h"
#include "gda-entity.h"
#include "gda-renderer.h"
#include "gda-referer.h"
#include "gda-object-ref.h"
#include "gda-query.h"
#include "gda-dict.h"
#include "gda-dict-type.h"
#include "gda-dict-aggregate.h"
#include "gda-server-provider.h"

/* 
 * Main static functions 
 */
static void gda_query_field_agg_class_init (GdaQueryFieldAggClass * class);
static void gda_query_field_agg_init (GdaQueryFieldAgg *qf);
static void gda_query_field_agg_dispose (GObject *object);
static void gda_query_field_agg_finalize (GObject *object);

static void gda_query_field_agg_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_query_field_agg_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

/* XML storage interface */
static void         gda_query_field_agg_xml_storage_init (GdaXmlStorageIface *iface);
static xmlNodePtr   gda_query_field_agg_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean     gda_query_field_agg_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* Field interface */
static void         gda_query_field_agg_field_init      (GdaEntityFieldIface *iface);
static GdaEntity   *gda_query_field_agg_get_entity      (GdaEntityField *iface);
static GdaDictType *gda_query_field_agg_get_data_type   (GdaEntityField *iface);

/* Renderer interface */
static void         gda_query_field_agg_renderer_init   (GdaRendererIface *iface);
static gchar       *gda_query_field_agg_render_as_sql   (GdaRenderer *iface, GdaParameterList *context, guint options, GError **error);
static gchar       *gda_query_field_agg_render_as_str   (GdaRenderer *iface, GdaParameterList *context);

/* Referer interface */
static void         gda_query_field_agg_referer_init        (GdaRefererIface *iface);
static gboolean     gda_query_field_agg_activate            (GdaReferer *iface);
static void         gda_query_field_agg_deactivate          (GdaReferer *iface);
static gboolean     gda_query_field_agg_is_active           (GdaReferer *iface);
static GSList      *gda_query_field_agg_get_ref_objects     (GdaReferer *iface);
static void         gda_query_field_agg_replace_refs        (GdaReferer *iface, GHashTable *replacements);

/* virtual functions */
static GObject     *gda_query_field_agg_copy           (GdaQueryField *orig);
static gboolean     gda_query_field_agg_is_equal       (GdaQueryField *qfield1, GdaQueryField *qfield2);


#ifdef GDA_DEBUG
static void         gda_query_field_agg_dump           (GdaQueryFieldAgg *agg, guint offset);
#endif

/* When the GdaQuery is destroyed */
static void destroyed_object_cb (GObject *obj, GdaQueryFieldAgg *agg);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP_QUERY,
	PROP_AGG_OBJ,
	PROP_AGG_NAME,
	PROP_AGG_ID
};


/* private structure */
struct _GdaQueryFieldAggPrivate
{
	GdaQuery      *query;
	GdaObjectRef  *agg_ref;  /* references a GdaDictAggregate */
	GdaObjectRef  *arg;
};


/* module error */
GQuark gda_query_field_agg_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_query_field_agg_error");
	return quark;
}


GType
gda_query_field_agg_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryFieldAggClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_field_agg_class_init,
			NULL,
			NULL,
			sizeof (GdaQueryFieldAgg),
			0,
			(GInstanceInitFunc) gda_query_field_agg_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_query_field_agg_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo field_info = {
			(GInterfaceInitFunc) gda_query_field_agg_field_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo renderer_info = {
			(GInterfaceInitFunc) gda_query_field_agg_renderer_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo referer_info = {
			(GInterfaceInitFunc) gda_query_field_agg_referer_init,
			NULL,
			NULL
		};
		
		
		type = g_type_register_static (GDA_TYPE_QUERY_FIELD, "GdaQueryFieldAgg", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_ENTITY_FIELD, &field_info);
		g_type_add_interface_static (type, GDA_TYPE_RENDERER, &renderer_info);
		g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
	}
	return type;
}

static void 
gda_query_field_agg_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = NULL;
	iface->save_to_xml = gda_query_field_agg_save_to_xml;
	iface->load_from_xml = gda_query_field_agg_load_from_xml;
}

static void
gda_query_field_agg_field_init (GdaEntityFieldIface *iface)
{
	iface->get_entity = gda_query_field_agg_get_entity;
	iface->get_data_type = gda_query_field_agg_get_data_type;
}

static void
gda_query_field_agg_renderer_init (GdaRendererIface *iface)
{
	iface->render_as_sql = gda_query_field_agg_render_as_sql;
	iface->render_as_str = gda_query_field_agg_render_as_str;
	iface->is_valid = NULL;
}

static void
gda_query_field_agg_referer_init (GdaRefererIface *iface)
{
        iface->activate = gda_query_field_agg_activate;
        iface->deactivate = gda_query_field_agg_deactivate;
        iface->is_active = gda_query_field_agg_is_active;
        iface->get_ref_objects = gda_query_field_agg_get_ref_objects;
        iface->replace_refs = gda_query_field_agg_replace_refs;
}

static void
gda_query_field_agg_class_init (GdaQueryFieldAggClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_query_field_agg_dispose;
	object_class->finalize = gda_query_field_agg_finalize;

	/* Properties */
	object_class->set_property = gda_query_field_agg_set_property;
	object_class->get_property = gda_query_field_agg_get_property;
	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_pointer ("query", "Query to which the field belongs", NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE |
								G_PARAM_CONSTRUCT_ONLY)));

	g_object_class_install_property (object_class, PROP_AGG_OBJ,
					 g_param_spec_pointer ("aggregate", "A pointer to a GdaDictAggregate", NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_AGG_NAME,
					 g_param_spec_string ("aggregate_name", "Name of the aggregate to represent", NULL, NULL,
							      G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_AGG_ID,
					 g_param_spec_string ("aggregate_id", "XML ID of a aggregate", NULL, NULL,
							      G_PARAM_WRITABLE));

	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_query_field_agg_dump;
#endif
	GDA_QUERY_FIELD_CLASS (class)->copy = gda_query_field_agg_copy;
	GDA_QUERY_FIELD_CLASS (class)->is_equal = gda_query_field_agg_is_equal;
	GDA_QUERY_FIELD_CLASS (class)->is_list = NULL;
	GDA_QUERY_FIELD_CLASS (class)->get_params = NULL;
}

static void
gda_query_field_agg_init (GdaQueryFieldAgg *gda_query_field_agg)
{
	gda_query_field_agg->priv = g_new0 (GdaQueryFieldAggPrivate, 1);
	gda_query_field_agg->priv->query = NULL;
	gda_query_field_agg->priv->agg_ref = NULL;
	gda_query_field_agg->priv->arg = NULL;
}


/**
 * gda_query_field_agg_new
 * @query: a #GdaQuery in which the new object will be
 * @agg_name: the name of an aggregate to represent
 *
 * Creates a new GdaQueryFieldAgg object which represents the @agg aggregate
 *
 * Returns: the new object
 */
GObject*
gda_query_field_agg_new (GdaQuery *query, const gchar *agg_name)
{
	GObject *obj;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (agg_name && *agg_name, NULL);

	obj = g_object_new (GDA_TYPE_QUERY_FIELD_AGG, "dict", gda_object_get_dict (GDA_OBJECT (query)), 
			    "query", query, "aggregate_name", agg_name, NULL);

	return obj;
}

static void 
destroyed_object_cb (GObject *obj, GdaQueryFieldAgg *agg)
{
	gda_object_destroy (GDA_OBJECT (agg));
}

static void
gda_query_field_agg_dispose (GObject *object)
{
	GdaQueryFieldAgg *fagg;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD_AGG (object));

	fagg = GDA_QUERY_FIELD_AGG (object);
	if (fagg->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		if (fagg->priv->arg) {
			g_object_unref (G_OBJECT (fagg->priv->arg));
			fagg->priv->arg = NULL;
		}

		if (fagg->priv->query) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (fagg->priv->query),
							      G_CALLBACK (destroyed_object_cb), fagg);
			fagg->priv->query = NULL;
		}

		if (fagg->priv->agg_ref) {
			g_object_unref (G_OBJECT (fagg->priv->agg_ref));
			fagg->priv->agg_ref = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_query_field_agg_finalize (GObject   * object)
{
	GdaQueryFieldAgg *fagg;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD_AGG (object));

	fagg = GDA_QUERY_FIELD_AGG (object);
	if (fagg->priv) {
		g_free (fagg->priv);
		fagg->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_query_field_agg_set_property (GObject *object,
			guint param_id,
			const GValue *value,
			GParamSpec *pspec)
{
	GdaQueryFieldAgg *fagg;
	gpointer ptr;
	guint id;
	const gchar *str;

	fagg = GDA_QUERY_FIELD_AGG (object);
	if (fagg->priv) {
		switch (param_id) {
		case PROP_QUERY:
			ptr = g_value_get_pointer (value);
			g_return_if_fail (ptr && GDA_IS_QUERY (ptr));

			if (fagg->priv->query) {
				if (fagg->priv->query == GDA_QUERY (ptr))
					return;

				g_signal_handlers_disconnect_by_func (G_OBJECT (fagg->priv->query),
								      G_CALLBACK (destroyed_object_cb), fagg);
			}

			fagg->priv->query = GDA_QUERY (ptr);
			gda_object_connect_destroy (ptr,
						    G_CALLBACK (destroyed_object_cb), fagg);

			fagg->priv->agg_ref = GDA_OBJECT_REF (gda_object_ref_new (gda_object_get_dict (GDA_OBJECT (ptr))));
			
			g_object_get (G_OBJECT (ptr), "field_serial", &id, NULL);
			gda_query_object_set_int_id (GDA_QUERY_OBJECT (fagg), id);
			break;
		case PROP_AGG_OBJ:
			ptr = g_value_get_pointer (value);
			g_return_if_fail (GDA_IS_DICT_AGGREGATE (ptr));
			gda_object_ref_set_ref_object (fagg->priv->agg_ref, GDA_OBJECT (ptr));
			break;
		case PROP_AGG_NAME:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (fagg->priv->agg_ref, GDA_TYPE_DICT_AGGREGATE, 
						     REFERENCE_BY_NAME, str);
			break;
		case PROP_AGG_ID:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (fagg->priv->agg_ref, GDA_TYPE_DICT_AGGREGATE, 
						     REFERENCE_BY_XML_ID, str);
			break;
		}
	}
}

static void
gda_query_field_agg_get_property (GObject *object,
			guint param_id,
			GValue *value,
			GParamSpec *pspec)
{
	GdaQueryFieldAgg *fagg;
	fagg = GDA_QUERY_FIELD_AGG (object);
	
	if (fagg->priv) {
		switch (param_id) {
		case PROP_QUERY:
			g_value_set_pointer (value, fagg->priv->query);
			break;
		case PROP_AGG_OBJ:
			g_value_set_pointer (value, gda_object_ref_get_ref_object (fagg->priv->agg_ref));
			break;
		case PROP_AGG_NAME:
		case PROP_AGG_ID:
			/* NO READ */
			g_assert_not_reached ();
			break;
		}	
	}
}

static GObject *
gda_query_field_agg_copy (GdaQueryField *orig)
{
	GdaQueryFieldAgg *qf;
	GObject *obj;
	GdaDict *dict;
	GdaObject *ref;

	g_assert (GDA_IS_QUERY_FIELD_AGG (orig));
	qf = GDA_QUERY_FIELD_AGG (orig);

	obj = g_object_new (GDA_TYPE_QUERY_FIELD_AGG, 
			    "dict", gda_object_get_dict (GDA_OBJECT (qf)), 
			    "query", qf->priv->query, NULL);

	ref = gda_object_ref_get_ref_object (qf->priv->agg_ref);
	if (ref)
		gda_object_ref_set_ref_object (GDA_QUERY_FIELD_AGG (obj)->priv->agg_ref, ref);
	else {
		const gchar *ref_str;
		GdaObjectRefType ref_type;
		GType ref_gtype;

		ref_str = gda_object_ref_get_ref_name (qf->priv->agg_ref, &ref_gtype, &ref_type);
		if (ref_str)
			gda_object_ref_set_ref_name (GDA_QUERY_FIELD_AGG (obj)->priv->agg_ref, ref_gtype, ref_type, ref_str);
	}

	if (gda_object_get_name (GDA_OBJECT (orig)))
		gda_object_set_name (GDA_OBJECT (obj), gda_object_get_name (GDA_OBJECT (orig)));

	if (gda_object_get_description (GDA_OBJECT (orig)))
		gda_object_set_description (GDA_OBJECT (obj), gda_object_get_description (GDA_OBJECT (orig)));


	/* argument */
	dict = gda_object_get_dict (GDA_OBJECT (orig));
	if (qf->priv->arg) {
		GdaObjectRef *ref;
		const gchar *refname;
		GType type;

		refname = gda_object_ref_get_ref_name (qf->priv->arg, &type, NULL);
		ref = GDA_OBJECT_REF (gda_object_ref_new (dict));
		gda_object_ref_set_ref_name (ref, type, REFERENCE_BY_XML_ID, refname);
		GDA_QUERY_FIELD_AGG (obj)->priv->arg = ref;
	}

	return obj;
}

static gboolean
gda_query_field_agg_is_equal (GdaQueryField *qfield1, GdaQueryField *qfield2)
{
	const gchar *ref1, *ref2;
	gboolean retval;
	g_assert (GDA_IS_QUERY_FIELD_AGG (qfield1));
	g_assert (GDA_IS_QUERY_FIELD_AGG (qfield2));
	
	/* it is here assumed that qfield1 and qfield2 are of the same type and refer to the same
	   query */
	ref1 = gda_object_ref_get_ref_name (GDA_QUERY_FIELD_AGG (qfield1)->priv->agg_ref, NULL, NULL);
	ref2 = gda_object_ref_get_ref_name (GDA_QUERY_FIELD_AGG (qfield2)->priv->agg_ref, NULL, NULL);

	retval = !strcmp (ref1, ref2) ? TRUE : FALSE;
	if (retval) {
		TO_IMPLEMENT; /* arguments */
	}

	return retval;
}

/**
 * gda_query_field_agg_get_ref_agg
 * @agg: a #GdaQueryFieldAgg object
 *
 * Get the real #GdaDictAggregate object used by @agg
 *
 * Returns: the #GdaDictAggregate object, or NULL if @agg is not active
 */
GdaDictAggregate *
gda_query_field_agg_get_ref_agg (GdaQueryFieldAgg *agg)
{
	GdaObject *base;
	g_return_val_if_fail (agg && GDA_IS_QUERY_FIELD_AGG (agg), NULL);
	g_return_val_if_fail (agg->priv, NULL);

	base = gda_object_ref_get_ref_object (agg->priv->agg_ref);
	if (base)
		return GDA_DICT_AGGREGATE (base);
	else
		return NULL;
}

/**
 * gda_query_field_agg_set_arg
 * @agg: a #GdaQueryFieldAgg object
 * @arg: a #GdaQueryField object
 *
 * Sets the argument of @agg. If @arg is %NULL, then
 * the argument (if there was one) is removed.
 *
 * If @agg is not active, then no check on the provided arg
 * is performed.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_query_field_agg_set_arg (GdaQueryFieldAgg *agg, GdaQueryField *arg)
{
	gboolean arg_ok = TRUE;
	g_return_val_if_fail (agg && GDA_IS_QUERY_FIELD_AGG (agg), FALSE);
	g_return_val_if_fail (agg->priv, FALSE);

	if (arg && gda_object_ref_activate (agg->priv->agg_ref)) {
		/* test for the right argument */
		GdaServerProviderInfo *sinfo = NULL;
		GdaDict *dict;
		GdaConnection *cnc;
		GdaDictAggregate *sagg = GDA_DICT_AGGREGATE (gda_object_ref_get_ref_object (agg->priv->agg_ref));
		GdaDictType *argtype = gda_dict_aggregate_get_arg_type (sagg);

		dict = gda_object_get_dict (GDA_OBJECT (agg));
		cnc = gda_dict_get_connection (dict);
		if (cnc)
			sinfo = gda_connection_get_infos (cnc);

		if (GDA_FUNC_AGG_TEST_PARAMS_DO_TEST && arg && argtype) {
			if (!sinfo || !sinfo->implicit_data_types_casts) {
				/* Strict test */
				if (gda_query_field_get_dict_type (arg) != argtype)
					arg_ok = FALSE;
			}
			else {
				/* GdaValueType compatibility test */
				if (gda_dict_type_get_gda_type (gda_query_field_get_dict_type (GDA_QUERY_FIELD (arg))) !=
				    gda_dict_type_get_gda_type (argtype))
					arg_ok = FALSE;
			}
		}
	}

	if (!arg || arg_ok) {
		/* Delete any pre-existing argument */
		if (agg->priv->arg) {
			g_object_unref (G_OBJECT (agg->priv->arg));
			agg->priv->arg = NULL;
		}
	}

	if (arg && arg_ok) {
		/* add the new argument */
		GdaObjectRef *ref;
				
		ref = GDA_OBJECT_REF (gda_object_ref_new (gda_object_get_dict (GDA_OBJECT (agg))));
		gda_object_ref_set_ref_object (ref, GDA_OBJECT (arg));
		agg->priv->arg = ref;
	}

	if (arg_ok)
		gda_referer_activate (GDA_REFERER (agg));

	return arg_ok;
}

/**
 * gda_query_field_agg_get_arg
 * @agg: a #GdaQueryFieldAgg object
 *
 * Get a list of the other #GdaQueryField objects which are arguments of @agg. If some
 * of them are missing, then a %NULL is inserted where it should have been.
 *
 * Returns: the #GnomeDbQField argument
 */
GdaQueryField *
gda_query_field_agg_get_args (GdaQueryFieldAgg *agg)
{
	g_return_val_if_fail (agg && GDA_IS_QUERY_FIELD_AGG (agg), NULL);
	g_return_val_if_fail (agg->priv, NULL);

	if (agg->priv->arg)
		return (GdaQueryField *) gda_object_ref_get_ref_object (agg->priv->arg);
	else
		return NULL;
}

#ifdef GDA_DEBUG
static void
gda_query_field_agg_dump (GdaQueryFieldAgg *agg, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (agg && GDA_IS_QUERY_FIELD_AGG (agg));
	
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (agg->priv) {
                g_print ("%s" D_COL_H1 "GdaQueryFieldAgg" D_COL_NOR " \"%s\" (%p, id=%s) ",
                         str, gda_object_get_name (GDA_OBJECT (agg)), agg, 
			 gda_object_get_id (GDA_OBJECT (agg)));
		if (gda_query_field_agg_is_active (GDA_REFERER (agg)))
			g_print ("Active");
		else
			g_print (D_COL_ERR "Inactive" D_COL_NOR);
		if (gda_query_field_is_visible (GDA_QUERY_FIELD (agg)))
			g_print (", Visible\n");
		g_print ("\n");
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, agg);
}
#endif


/* 
 * GdaEntity interface implementation
 */
static GdaEntity *
gda_query_field_agg_get_entity (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv, NULL);

	return GDA_ENTITY (GDA_QUERY_FIELD_AGG (iface)->priv->query);
}

static GdaDictType *
gda_query_field_agg_get_data_type (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv, NULL);
	
	if (gda_query_field_agg_activate (GDA_REFERER (iface))) {
		GdaDictAggregate *agg;
		agg = GDA_DICT_AGGREGATE (gda_object_ref_get_ref_object (GDA_QUERY_FIELD_AGG (iface)->priv->agg_ref));
		return gda_dict_aggregate_get_ret_type (agg);
	}

	return NULL;
}

/* 
 * GdaXmlStorage interface implementation
 */
static xmlNodePtr
gda_query_field_agg_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	GdaQueryFieldAgg *agg;
	gchar *str;
	GdaObject *obj;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv, NULL);

	agg = GDA_QUERY_FIELD_AGG (iface);

	node = xmlNewNode (NULL, "gda_query_fagg");
	
	str = gda_xml_storage_get_xml_id (iface);
	xmlSetProp (node, "id", str);
	g_free (str);

	xmlSetProp (node, "name", gda_object_get_name (GDA_OBJECT (agg)));
	if (gda_object_get_description (GDA_OBJECT (agg)) && *gda_object_get_description (GDA_OBJECT (agg)))
		xmlSetProp (node, "descr", gda_object_get_description (GDA_OBJECT (agg)));

	obj = NULL;
	if (gda_object_ref_activate (agg->priv->agg_ref))
		obj = gda_object_ref_get_ref_object (agg->priv->agg_ref);

	if (obj) {
		gchar *xmlid;
		xmlid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (obj));
		xmlSetProp (node, "agg", xmlid);
	}
	else {
		const gchar *cstr;

		cstr = gda_object_ref_get_ref_name (agg->priv->agg_ref, NULL, NULL);
		if (cstr)
			xmlSetProp (node, "agg_name", cstr);
	}

	if (! gda_query_field_is_visible (GDA_QUERY_FIELD (agg)))
		xmlSetProp (node, "is_visible",  "f");
	if (gda_query_field_is_internal (GDA_QUERY_FIELD (agg)))
		xmlSetProp (node, "is_internal", "t");

	str = (gchar *) gda_query_field_get_alias (GDA_QUERY_FIELD (agg));
	if (str && *str) 
		xmlSetProp (node, "alias", str);

	/* aggregate's argument */
	if (agg->priv->arg) {
		xmlNodePtr argnode;
		
		argnode = xmlNewChild (node, NULL, "gda_query_field_ref", NULL);
		xmlSetProp (argnode, "object", gda_object_ref_get_ref_name (agg->priv->arg, NULL, NULL));
	}

	return node;
}

static gboolean
gda_query_field_agg_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaQueryFieldAgg *agg;
	gchar *prop;
	gboolean aggref = FALSE;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	agg = GDA_QUERY_FIELD_AGG (iface);
	if (strcmp (node->name, "gda_query_fagg")) {
		g_set_error (error,
			     GDA_QUERY_FIELD_AGG_ERROR,
			     GDA_QUERY_FIELD_AGG_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_query_fagg>"));
		return FALSE;
	}

	prop = xmlGetProp (node, "id");
	if (prop) {
		gchar *ptr, *tok;
		ptr = strtok_r (prop, ":", &tok);
		ptr = strtok_r (NULL, ":", &tok);
		if (strlen (ptr) < 3) {
			g_set_error (error,
				     GDA_QUERY_FIELD_AGG_ERROR,
				     GDA_QUERY_FIELD_AGG_XML_LOAD_ERROR,
				     _("XML ID for a query field should be QUxxx:QFyyy where xxx and yyy are numbers"));
			return FALSE;
		}
		gda_query_object_set_int_id (GDA_QUERY_OBJECT (agg), atoi (ptr+2));
		g_free (prop);
	}

	prop = xmlGetProp (node, "name");
	if (prop) {
		gda_object_set_name (GDA_OBJECT (agg), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "descr");
	if (prop) {
		gda_object_set_description (GDA_OBJECT (agg), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "agg");
	if (prop) {
		aggref = TRUE;
		gda_object_ref_set_ref_name (agg->priv->agg_ref, GDA_TYPE_DICT_AGGREGATE, REFERENCE_BY_XML_ID, prop);
		g_free (prop);
	}
	else {
		prop = xmlGetProp (node, "agg_name");
		if (prop) {
			aggref = TRUE;
			gda_object_ref_set_ref_name (agg->priv->agg_ref, GDA_TYPE_DICT_AGGREGATE, REFERENCE_BY_NAME, prop);
			g_free (prop);
		}
	}

	prop = xmlGetProp (node, "is_visible");
	if (prop) {
		gda_query_field_set_visible (GDA_QUERY_FIELD (agg), (*prop == 't') ? TRUE : FALSE);
		g_free (prop);
	}

	prop = xmlGetProp (node, "is_internal");
	if (prop) {
		gda_query_field_set_internal (GDA_QUERY_FIELD (agg), (*prop == 't') ? TRUE : FALSE);
		g_free (prop);
	}

	prop = xmlGetProp (node, "alias");
	if (prop) {
		gda_query_field_set_alias (GDA_QUERY_FIELD (agg), prop);
		g_free (prop);
	}
	
	/* aggregate's argument */
	if (node->children) {
		GdaDict *dict;
		xmlNodePtr argnode = node->children;
		dict = gda_object_get_dict (GDA_OBJECT (agg));
		while (argnode) {
			if (!strcmp (argnode->name, "gda_query_field_ref")) {
				if (! agg->priv->arg) {
					prop = xmlGetProp (argnode, "object");
					if (prop) {
						GdaObjectRef *ref;
						
						ref = GDA_OBJECT_REF (gda_object_ref_new (dict));
						gda_object_ref_set_ref_name (ref, GDA_TYPE_ENTITY_FIELD, REFERENCE_BY_XML_ID, prop);
						g_free (prop);
						agg->priv->arg = ref;
					}
				}
				else {
					GdaObject *ref;
		
					ref = gda_object_ref_get_ref_object (agg->priv->agg_ref);
					g_set_error (error,
						     GDA_QUERY_FIELD_AGG_ERROR,
						     GDA_QUERY_FIELD_AGG_XML_LOAD_ERROR,
						     _("More than one argument for aggregate %s"), 
						     gda_object_get_name (ref));
					return FALSE;
				}
			}
			argnode = argnode->next;
		}
	}

	if (! agg->priv->arg || !aggref) {
		g_set_error (error,
			     GDA_QUERY_FIELD_AGG_ERROR,
			     GDA_QUERY_FIELD_AGG_XML_LOAD_ERROR,
			     _("Missing required attributes for <gda_query_fagg>"));
		return FALSE;
	}
	else
		return TRUE;
}


/*
 * GdaRenderer interface implementation
 */

static gchar *
gda_query_field_agg_render_as_sql (GdaRenderer *iface, GdaParameterList *context, guint options, GError **error)
{
	gchar *str = NULL;
	GdaObject *base;
	GdaQueryFieldAgg *agg;
	gboolean err = FALSE;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv, NULL);
	agg = GDA_QUERY_FIELD_AGG (iface);

	base = gda_object_ref_get_ref_object (agg->priv->agg_ref);

	if (base) {
		GString *string;

		string = g_string_new (gda_object_get_name (base));
		g_string_append (string, " (");
		if (agg->priv->arg) {
			gchar *argstr;
			GdaObject *argbase;

			argbase = gda_object_ref_get_ref_object (agg->priv->arg);
			if (argbase) {
				argstr = gda_renderer_render_as_sql (GDA_RENDERER (argbase), context, 
									  options, error);
				if (argstr) {
					g_string_append (string, argstr);
					g_free (argstr);
				}
				else
					err = TRUE;
			}
			else {
				const gchar *tmpstr;
				tmpstr = gda_object_ref_get_ref_name (agg->priv->arg, NULL, NULL);
				g_set_error (error,
					     GDA_QUERY_FIELD_AGG_ERROR,
					     GDA_QUERY_FIELD_AGG_RENDER_ERROR,
					     _("Can't find referenced field '%s'"), tmpstr);
				err = TRUE;
			}
		}
		else {
			g_set_error (error,
				     GDA_QUERY_FIELD_AGG_ERROR,
				     GDA_QUERY_FIELD_AGG_RENDER_ERROR,
				     _("Aggregate '%s' has no argument"), gda_object_get_name (base));
			err = TRUE;
		}
		
		g_string_append (string, ")");
		str = string->str;
		g_string_free (string, FALSE);
	}
	else {
		g_set_error (error,
			     GDA_QUERY_FIELD_AGG_ERROR,
			     GDA_QUERY_FIELD_AGG_RENDER_ERROR,
			     _("Can't find aggregate '%s'"), gda_object_ref_get_ref_name (agg->priv->agg_ref,
											     NULL, NULL));
		err = TRUE;
	}

	if (err) {
		if (str)
			g_free (str);
		return NULL;
	}

	return str;
}

static gchar *
gda_query_field_agg_render_as_str (GdaRenderer *iface, GdaParameterList *context)
{
	gchar *str = NULL;
	GdaObject *base;
	GdaQueryFieldAgg *agg;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv, NULL);
	agg = GDA_QUERY_FIELD_AGG (iface);

	base = gda_object_ref_get_ref_object (agg->priv->agg_ref);

	if (base) {
		GString *string;
		GSList *list;

		string = g_string_new (gda_object_get_name (base));
		g_string_append (string, " (");
		if (agg->priv->arg) {
			gchar *argstr;
			GdaObject *argbase;
			
			argbase = gda_object_ref_get_ref_object (agg->priv->arg);
			if (argbase) {
				argstr = gda_renderer_render_as_str (GDA_RENDERER (argbase), context);
				g_assert (argstr);
				g_string_append (string, argstr);
				g_free (argstr);
			}
			else {
				const gchar *tmpstr;
				tmpstr = gda_object_ref_get_ref_name (agg->priv->arg, NULL, NULL);
				g_string_append (string, tmpstr);
			}

			list = g_slist_next (list);
		}
		g_string_append (string, ")");

		str = string->str;
		g_string_free (string, FALSE);

	}
	else 
		str = g_strdup (_("Non-activated aggregate"));
	
	return str;
}


/*
 * GdaReferer interface implementation
 */
static gboolean
gda_query_field_agg_activate (GdaReferer *iface)
{
	gboolean active = FALSE;
	GdaQueryFieldAgg *agg;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv, FALSE);
	agg = GDA_QUERY_FIELD_AGG (iface);

	active = gda_object_ref_activate (agg->priv->agg_ref);
	active = active && agg->priv->arg && gda_object_ref_activate (agg->priv->arg);

	return active;
}

static void
gda_query_field_agg_deactivate (GdaReferer *iface)
{
	GdaQueryFieldAgg *agg;

	g_return_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface));
	g_return_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv);
	agg = GDA_QUERY_FIELD_AGG (iface);

	gda_object_ref_deactivate (agg->priv->agg_ref);
	if (agg->priv->arg)
		gda_object_ref_deactivate (agg->priv->arg);
}

static gboolean
gda_query_field_agg_is_active (GdaReferer *iface)
{
	gboolean active;
	GdaQueryFieldAgg *agg;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv, FALSE);
	agg = GDA_QUERY_FIELD_AGG (iface);

	active = gda_object_ref_is_active (agg->priv->agg_ref);
	active = active && agg->priv->arg && gda_object_ref_is_active (agg->priv->arg);

	return active;
}

static GSList *
gda_query_field_agg_get_ref_objects (GdaReferer *iface)
{
	GSList *list = NULL;
        GdaObject *base;
	GdaQueryFieldAgg *agg;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv, NULL);
	agg = GDA_QUERY_FIELD_AGG (iface);

        base = gda_object_ref_get_ref_object (agg->priv->agg_ref);
        if (base)
                list = g_slist_append (list, base);
	if (agg->priv->arg) {
		base = gda_object_ref_get_ref_object (agg->priv->arg);
		if (base)
			list = g_slist_append (list, base);
	}

        return list;
}

static void
gda_query_field_agg_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	GdaQueryFieldAgg *agg;

        g_return_if_fail (iface && GDA_IS_QUERY_FIELD_AGG (iface));
        g_return_if_fail (GDA_QUERY_FIELD_AGG (iface)->priv);

        agg = GDA_QUERY_FIELD_AGG (iface);
        if (agg->priv->query) {
                GdaQuery *query = g_hash_table_lookup (replacements, agg->priv->query);
                if (query) {
                        g_signal_handlers_disconnect_by_func (G_OBJECT (agg->priv->query),
                                                              G_CALLBACK (destroyed_object_cb), agg);
                        agg->priv->query = query;
			gda_object_connect_destroy (query, 
						 G_CALLBACK (destroyed_object_cb), agg);
                }
        }

        gda_object_ref_replace_ref_object (agg->priv->agg_ref, replacements);
	if (agg->priv->arg)
		gda_object_ref_replace_ref_object (agg->priv->arg, replacements);
}
