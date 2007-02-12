/* gda-query-field-func.c
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
#include "gda-query-field-func.h"
#include "gda-xml-storage.h"
#include "gda-entity-field.h"
#include "gda-entity.h"
#include "gda-renderer.h"
#include "gda-referer.h"
#include "gda-object-ref.h"
#include "gda-query.h"
#include "gda-connection.h"
#include "gda-dict-type.h"
#include "gda-dict-function.h"
#include "gda-server-provider.h"

/* 
 * Main static functions 
 */
static void gda_query_field_func_class_init (GdaQueryFieldFuncClass * class);
static void gda_query_field_func_init (GdaQueryFieldFunc *qf);
static void gda_query_field_func_dispose (GObject *object);
static void gda_query_field_func_finalize (GObject *object);

static void gda_query_field_func_set_property (GObject *object,
				      guint param_id,
				      const GValue *value,
				      GParamSpec *pspec);
static void gda_query_field_func_get_property (GObject *object,
				      guint param_id,
				      GValue *value,
				      GParamSpec *pspec);

/* XML storage interface */
static void        gda_query_field_func_xml_storage_init (GdaXmlStorageIface *iface);
static xmlNodePtr  gda_query_field_func_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_query_field_func_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* Field interface */
static void              gda_query_field_func_field_init      (GdaEntityFieldIface *iface);
static GdaEntity         *gda_query_field_func_get_entity      (GdaEntityField *iface);
static GdaDictType *gda_query_field_func_get_dict_type   (GdaEntityField *iface);

/* Renderer interface */
static void            gda_query_field_func_renderer_init   (GdaRendererIface *iface);
static gchar          *gda_query_field_func_render_as_sql   (GdaRenderer *iface, GdaParameterList *context, 
							     GSList **out_params_used, GdaRendererOptions options, GError **error);
static gchar          *gda_query_field_func_render_as_str   (GdaRenderer *iface, GdaParameterList *context);

/* Referer interface */
static void        gda_query_field_func_referer_init        (GdaRefererIface *iface);
static gboolean    gda_query_field_func_activate            (GdaReferer *iface);
static void        gda_query_field_func_deactivate          (GdaReferer *iface);
static gboolean    gda_query_field_func_is_active           (GdaReferer *iface);
static GSList     *gda_query_field_func_get_ref_objects     (GdaReferer *iface);
static void        gda_query_field_func_replace_refs        (GdaReferer *iface, GHashTable *replacements);

/* virtual functions */
static GObject    *gda_query_field_func_copy           (GdaQueryField *orig);
static gboolean    gda_query_field_func_is_equal       (GdaQueryField *qfield1, GdaQueryField *qfield2);


#ifdef GDA_DEBUG
static void        gda_query_field_func_dump           (GdaQueryFieldFunc *func, guint offset);
#endif

/* When the GdaQuery or GdaQueryTarget is destroyed */
static void destroyed_object_cb (GObject *obj, GdaQueryFieldFunc *func);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;


/* properties */
enum
{
	PROP_0,
	PROP_QUERY,
	PROP_FUNC_OBJ,
	PROP_FUNC_NAME,
	PROP_FUNC_ID
};


/* private structure */
struct _GdaQueryFieldFuncPrivate
{
	GdaQuery      *query;
	GdaObjectRef  *func_ref;  /* references a GdaDictFunction */
	GSList        *args;      /* list of GdaObjectRef objects */
};


/* module error */
GQuark gda_query_field_func_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_query_field_func_error");
	return quark;
}


GType
gda_query_field_func_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryFieldFuncClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_field_func_class_init,
			NULL,
			NULL,
			sizeof (GdaQueryFieldFunc),
			0,
			(GInstanceInitFunc) gda_query_field_func_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_query_field_func_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo field_info = {
			(GInterfaceInitFunc) gda_query_field_func_field_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo renderer_info = {
			(GInterfaceInitFunc) gda_query_field_func_renderer_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo referer_info = {
			(GInterfaceInitFunc) gda_query_field_func_referer_init,
			NULL,
			NULL
		};
		
		
		type = g_type_register_static (GDA_TYPE_QUERY_FIELD, "GdaQueryFieldFunc", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_ENTITY_FIELD, &field_info);
		g_type_add_interface_static (type, GDA_TYPE_RENDERER, &renderer_info);
		g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
	}
	return type;
}

static void 
gda_query_field_func_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = NULL;
	iface->save_to_xml = gda_query_field_func_save_to_xml;
	iface->load_from_xml = gda_query_field_func_load_from_xml;
}

static void
gda_query_field_func_field_init (GdaEntityFieldIface *iface)
{
	iface->get_entity = gda_query_field_func_get_entity;
	iface->get_dict_type = gda_query_field_func_get_dict_type;
}

static void
gda_query_field_func_renderer_init (GdaRendererIface *iface)
{
	iface->render_as_sql = gda_query_field_func_render_as_sql;
	iface->render_as_str = gda_query_field_func_render_as_str;
	iface->is_valid = NULL;
}

static void
gda_query_field_func_referer_init (GdaRefererIface *iface)
{
        iface->activate = gda_query_field_func_activate;
        iface->deactivate = gda_query_field_func_deactivate;
        iface->is_active = gda_query_field_func_is_active;
        iface->get_ref_objects = gda_query_field_func_get_ref_objects;
        iface->replace_refs = gda_query_field_func_replace_refs;
}

static void
gda_query_field_func_class_init (GdaQueryFieldFuncClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_query_field_func_dispose;
	object_class->finalize = gda_query_field_func_finalize;

	/* Properties */
	object_class->set_property = gda_query_field_func_set_property;
	object_class->get_property = gda_query_field_func_get_property;
	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_object ("query", "Query to which the field belongs", NULL, 
                                                               GDA_TYPE_QUERY,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE |
								G_PARAM_CONSTRUCT_ONLY)));

	g_object_class_install_property (object_class, PROP_FUNC_OBJ,
					 g_param_spec_object ("function", "A pointer to a GdaDictFunction", NULL,
                                                               GDA_TYPE_DICT_FUNCTION, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_FUNC_NAME,
					 g_param_spec_string ("function_name", "Name of the function to represent", NULL, NULL,
							      G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_FUNC_ID,
					 g_param_spec_string ("function_id", "XML ID of a function", NULL, NULL,
							      G_PARAM_WRITABLE));
	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_query_field_func_dump;
#endif
	GDA_QUERY_FIELD_CLASS (class)->copy = gda_query_field_func_copy;
	GDA_QUERY_FIELD_CLASS (class)->is_equal = gda_query_field_func_is_equal;
	GDA_QUERY_FIELD_CLASS (class)->is_list = NULL;
	GDA_QUERY_FIELD_CLASS (class)->get_params = NULL;
}

static void
gda_query_field_func_init (GdaQueryFieldFunc *gda_query_field_func)
{
	gda_query_field_func->priv = g_new0 (GdaQueryFieldFuncPrivate, 1);
	gda_query_field_func->priv->query = NULL;
	gda_query_field_func->priv->func_ref = NULL;
	gda_query_field_func->priv->args = NULL;
}


/**
 * gda_query_field_func_new
 * @query: a #GdaQuery in which the new object will be
 * @func_name: the name of the function to use
 *
 * Creates a new GdaQueryFieldFunc object which represents the @func_name function
 *
 * Returns: the new object
 */
GdaQueryField*
gda_query_field_func_new (GdaQuery *query, const gchar *func_name)
{
	GObject *obj;

	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);
	g_return_val_if_fail (func_name && *func_name, NULL);

	obj = g_object_new (GDA_TYPE_QUERY_FIELD_FUNC, "dict", gda_object_get_dict (GDA_OBJECT (query)), 
			    "query", query, "function_name", func_name, NULL);

	return (GdaQueryField *) obj;
}

static void 
destroyed_object_cb (GObject *obj, GdaQueryFieldFunc *func)
{
	gda_object_destroy (GDA_OBJECT (func));
}

static void
gda_query_field_func_dispose (GObject *object)
{
	GdaQueryFieldFunc *ffunc;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD_FUNC (object));

	ffunc = GDA_QUERY_FIELD_FUNC (object);
	if (ffunc->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		if (ffunc->priv->args) {
			GSList *list = ffunc->priv->args;
			while (list) {
				g_object_unref (G_OBJECT (list->data));
				list = g_slist_next (list);
			}
			g_slist_free (ffunc->priv->args);
			ffunc->priv->args = NULL;
		}

		if (ffunc->priv->query) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (ffunc->priv->query),
							      G_CALLBACK (destroyed_object_cb), ffunc);
			ffunc->priv->query = NULL;
		}

		if (ffunc->priv->func_ref) {
			g_object_unref (G_OBJECT (ffunc->priv->func_ref));
			ffunc->priv->func_ref = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_query_field_func_finalize (GObject   * object)
{
	GdaQueryFieldFunc *ffunc;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_QUERY_FIELD_FUNC (object));

	ffunc = GDA_QUERY_FIELD_FUNC (object);
	if (ffunc->priv) {
		g_free (ffunc->priv);
		ffunc->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_query_field_func_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	GdaQueryFieldFunc *ffunc;
	guint id;
	const gchar *str;

	ffunc = GDA_QUERY_FIELD_FUNC (object);
	if (ffunc->priv) {
		switch (param_id) {
		case PROP_QUERY: {
			GdaQuery* ptr = GDA_QUERY (g_value_get_object (value));
			g_return_if_fail (GDA_IS_QUERY (ptr));

			if (ffunc->priv->query) {
				if (ffunc->priv->query == GDA_QUERY (ptr))
					return;

				g_signal_handlers_disconnect_by_func (G_OBJECT (ffunc->priv->query),
								      G_CALLBACK (destroyed_object_cb), ffunc);
			}

			ffunc->priv->query = GDA_QUERY (ptr);
			gda_object_connect_destroy (ptr,
						    G_CALLBACK (destroyed_object_cb), ffunc);

			ffunc->priv->func_ref = GDA_OBJECT_REF (gda_object_ref_new (gda_object_get_dict (GDA_OBJECT (ptr))));
			
			g_object_get (G_OBJECT (ptr), "field_serial", &id, NULL);
			gda_query_object_set_int_id (GDA_QUERY_OBJECT (ffunc), id);
			break;
                }
		case PROP_FUNC_OBJ: {
			GdaDictFunction* ptr = GDA_DICT_FUNCTION (g_value_get_object (value));
			g_return_if_fail (GDA_IS_DICT_FUNCTION (ptr));
			gda_object_ref_set_ref_object (ffunc->priv->func_ref, GDA_OBJECT (ptr));
			break;
                }
		case PROP_FUNC_NAME:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (ffunc->priv->func_ref, GDA_TYPE_DICT_FUNCTION, 
						     REFERENCE_BY_NAME, str);
			break;
		case PROP_FUNC_ID:
			str = g_value_get_string (value);
			gda_object_ref_set_ref_name (ffunc->priv->func_ref, GDA_TYPE_DICT_FUNCTION, 
						     REFERENCE_BY_XML_ID, str);
			break;
		}
	}
}

static void
gda_query_field_func_get_property (GObject *object,
			guint param_id,
			GValue *value,
			GParamSpec *pspec)
{
	GdaQueryFieldFunc *ffunc;
	ffunc = GDA_QUERY_FIELD_FUNC (object);
	
	if (ffunc->priv) {
		switch (param_id) {
		case PROP_QUERY:
			g_value_set_object (value, G_OBJECT (ffunc->priv->query));
			break;
		case PROP_FUNC_OBJ:
			g_value_set_object (value, G_OBJECT (gda_object_ref_get_ref_object (ffunc->priv->func_ref)));
			break;
		case PROP_FUNC_NAME:
		case PROP_FUNC_ID:
			/* NO READ */
			g_assert_not_reached ();
			break;
		}	
	}
}

static GObject *
gda_query_field_func_copy (GdaQueryField *orig)
{
	GdaQueryFieldFunc *qf;
	GObject *obj;
	GSList *list;
	GdaDict *dict;
	GdaObject *ref;

	g_assert (GDA_IS_QUERY_FIELD_FUNC (orig));
	qf = GDA_QUERY_FIELD_FUNC (orig);

	obj = g_object_new (GDA_TYPE_QUERY_FIELD_FUNC, 
			    "dict", gda_object_get_dict (GDA_OBJECT (qf)), 
			    "query", qf->priv->query, NULL);

	ref = gda_object_ref_get_ref_object (qf->priv->func_ref);
	if (ref)
		gda_object_ref_set_ref_object (GDA_QUERY_FIELD_FUNC (obj)->priv->func_ref, ref);
	else {
		const gchar *ref_str;
		GdaObjectRefType ref_type;
		GType ref_gtype;

		ref_str = gda_object_ref_get_ref_object_name (qf->priv->func_ref);
		if (ref_str)
			g_object_set (G_OBJECT (GDA_QUERY_FIELD_FUNC (obj)->priv->func_ref), "obj_name", ref_str, NULL);

		ref_str = gda_object_ref_get_ref_name (qf->priv->func_ref, &ref_gtype, &ref_type);
		if (ref_str)
			gda_object_ref_set_ref_name (GDA_QUERY_FIELD_FUNC (obj)->priv->func_ref, ref_gtype, ref_type, ref_str);
	}

	if (gda_object_get_name (GDA_OBJECT (orig)))
		gda_object_set_name (GDA_OBJECT (obj), gda_object_get_name (GDA_OBJECT (orig)));

	if (gda_object_get_description (GDA_OBJECT (orig)))
		gda_object_set_description (GDA_OBJECT (obj), gda_object_get_description (GDA_OBJECT (orig)));


	/* arguments */
	dict = gda_object_get_dict (GDA_OBJECT (orig));
	list = qf->priv->args;
	while (list) {
		GdaObjectRef *ref;
		const gchar *refname;
		GType type;

		refname = gda_object_ref_get_ref_name (GDA_OBJECT_REF (list->data), &type, NULL);
		ref = GDA_OBJECT_REF (gda_object_ref_new (dict));
		gda_object_ref_set_ref_name (ref, type, REFERENCE_BY_XML_ID, refname);
		GDA_QUERY_FIELD_FUNC (obj)->priv->args = g_slist_append (GDA_QUERY_FIELD_FUNC (obj)->priv->args, ref);
		list = g_slist_next (list);
	}

	return obj;
}

static gboolean
gda_query_field_func_is_equal (GdaQueryField *qfield1, GdaQueryField *qfield2)
{
	const gchar *ref1, *ref2;
	gboolean retval;
	g_assert (GDA_IS_QUERY_FIELD_FUNC (qfield1));
	g_assert (GDA_IS_QUERY_FIELD_FUNC (qfield2));
	
	/* it is here assumed that qfield1 and qfield2 are of the same type and refer to the same
	   query */
	ref1 = gda_object_ref_get_ref_name (GDA_QUERY_FIELD_FUNC (qfield1)->priv->func_ref, NULL, NULL);
	ref2 = gda_object_ref_get_ref_name (GDA_QUERY_FIELD_FUNC (qfield2)->priv->func_ref, NULL, NULL);

	retval = !strcmp (ref1, ref2) ? TRUE : FALSE;
	if (retval) {
		TO_IMPLEMENT; /* arguments */
	}

	return retval;
}

/**
 * gda_query_field_func_get_ref_func
 * @func: a #GdaQueryFieldFunc object
 *
 * Get the real #GdaDictFunction object used by @func
 *
 * Returns: the #GdaDictFunction object, or NULL if @func is not active
 */
GdaDictFunction *
gda_query_field_func_get_ref_func (GdaQueryFieldFunc *func)
{
	GdaObject *base;
	g_return_val_if_fail (func && GDA_IS_QUERY_FIELD_FUNC (func), NULL);
	g_return_val_if_fail (func->priv, NULL);

	base = gda_object_ref_get_ref_object (func->priv->func_ref);
	if (base)
		return GDA_DICT_FUNCTION (base);
	else
		return NULL;
}

/**
 * gda_query_field_func_get_ref_func
 * @func: a #GdaQueryFieldFunc object
 *
 * Get the name of the function which @func represents
 *
 * Returns: the function name
 */
const gchar *
gda_query_field_func_get_ref_func_name (GdaQueryFieldFunc *func)
{
	GdaObject *base;
	g_return_val_if_fail (func && GDA_IS_QUERY_FIELD_FUNC (func), NULL);
	g_return_val_if_fail (func->priv, NULL);

	base = gda_object_ref_get_ref_object (func->priv->func_ref);
	if (base)
		return gda_object_get_name (base);
	else
		return gda_object_ref_get_ref_name (func->priv->func_ref, NULL, NULL);
}

/**
 * gda_query_field_func_set_args
 * @func: a #GdaQueryFieldFunc object
 * @args: a list of #GdaQueryField objects
 *
 * Sets the argument(s) of @func. If @args is %NULL, then
 * all the arguments (if there was any) are removed.
 *
 * If @func is not active, then no check on the provided args
 * is performed.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_query_field_func_set_args (GdaQueryFieldFunc *func, GSList *args)
{
	gboolean args_ok = TRUE;
	g_return_val_if_fail (func && GDA_IS_QUERY_FIELD_FUNC (func), FALSE);
	g_return_val_if_fail (func->priv, FALSE);

	if (args && gda_object_ref_activate (func->priv->func_ref)) {
		/* test for the right arguments */
		GdaDictFunction *sfunc = GDA_DICT_FUNCTION (gda_object_ref_get_ref_object (func->priv->func_ref));
		GSList *arg = args, *list = (GSList *) gda_dict_function_get_arg_dict_types (sfunc);
		GdaServerProviderInfo *sinfo = NULL;
		GdaDict *dict;
		GdaConnection *cnc;

		dict = gda_object_get_dict (GDA_OBJECT (func));
		cnc = gda_dict_get_connection (dict);
		if (cnc)
			sinfo = gda_connection_get_infos (cnc);

		if (g_slist_length (arg) != g_slist_length (list))
			args_ok = FALSE;

		while (GDA_FUNC_AGG_TEST_PARAMS_DO_TEST && arg && list && args_ok) {
			if (!sinfo || !sinfo->implicit_data_types_casts) {
				/* Strict tests */
				if (arg->data && list->data &&
				    list->data &&
				    (gda_query_field_get_dict_type (GDA_QUERY_FIELD (arg->data)) != GDA_DICT_TYPE (list->data)))
					args_ok = FALSE;
			}
			else {
				/* GType compatibility test */
				if (arg->data && list->data &&
				    list->data &&
				    (gda_dict_type_get_g_type (gda_query_field_get_dict_type (GDA_QUERY_FIELD (arg->data))) !=
				     gda_dict_type_get_g_type (GDA_DICT_TYPE (list->data))))
					args_ok = FALSE;
			}

			arg = g_slist_next (arg);
			list = g_slist_next (list);
		}
	}

	if (!args || args_ok) {
		/* Delete any pre-existing argument */
		if (func->priv->args) {
			GSList *list = func->priv->args;
			while (list) {
				g_object_unref (G_OBJECT (list->data));
				list = g_slist_next (list);
			}
			g_slist_free (func->priv->args);
			func->priv->args = NULL;
		}
	}

	if (args && args_ok) {
		/* add the new arguments */
		GSList *list = args;
		while (list) {
			GdaObjectRef *ref;
				
			ref = GDA_OBJECT_REF (gda_object_ref_new (gda_object_get_dict (GDA_OBJECT (func))));
			if (list->data)
				gda_object_ref_set_ref_object (ref, GDA_OBJECT (list->data));
			func->priv->args = g_slist_append (func->priv->args, ref);
			list = g_slist_next (list);
		}
	}

	if (args_ok)
		gda_referer_activate (GDA_REFERER (func));

	return args_ok;
}

/**
 * gda_query_field_func_get_args
 * @func: a #GdaQueryFieldFunc object
 *
 * Get a list of the other #GdaQueryField objects which are arguments of @func. If some
 * of them are missing, then a %NULL is inserted where it should have been.
 *
 * Returns: a new list of arguments
 */
GSList *
gda_query_field_func_get_args (GdaQueryFieldFunc *func)
{
	GSList *retval = NULL, *list;

	g_return_val_if_fail (func && GDA_IS_QUERY_FIELD_FUNC (func), NULL);
	g_return_val_if_fail (func->priv, NULL);
	
	list = func->priv->args;
	while (list) {
		GdaObject *base = NULL;

		if (list->data)
			base = gda_object_ref_get_ref_object (GDA_OBJECT_REF (list->data));

		retval = g_slist_append (retval, base);
		list = g_slist_next (list);
	}

	return retval;
}

#ifdef GDA_DEBUG
static void
gda_query_field_func_dump (GdaQueryFieldFunc *func, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (func && GDA_IS_QUERY_FIELD_FUNC (func));
	
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (func->priv) {
                g_print ("%s" D_COL_H1 "GdaQueryFieldFunc" D_COL_NOR " \"%s\" (%p, id=%s) ",
                         str, gda_object_get_name (GDA_OBJECT (func)), func,
			 gda_object_get_id (GDA_OBJECT (func)));
		if (gda_query_field_func_is_active (GDA_REFERER (func)))
			g_print ("Active");
		else
			g_print (D_COL_ERR "Inactive" D_COL_NOR);
		if (gda_query_field_is_visible (GDA_QUERY_FIELD (func)))
			g_print (", Visible\n");
		g_print ("\n");
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, func);
}
#endif


/* 
 * GdaEntity interface implementation
 */
static GdaEntity *
gda_query_field_func_get_entity (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv, NULL);

	return GDA_ENTITY (GDA_QUERY_FIELD_FUNC (iface)->priv->query);
}

static GdaDictType *
gda_query_field_func_get_dict_type (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv, NULL);
	
	if (gda_query_field_func_activate (GDA_REFERER (iface))) {
		GdaDictFunction *func;
		func = GDA_DICT_FUNCTION (gda_object_ref_get_ref_object (GDA_QUERY_FIELD_FUNC (iface)->priv->func_ref));
		return gda_dict_function_get_ret_dict_type (func);
	}

	return NULL;
}

/* 
 * GdaXmlStorage interface implementation
 */
static xmlNodePtr
gda_query_field_func_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	GdaQueryFieldFunc *func;
	gchar *str;
	GSList *list;
	GdaObject *obj;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv, NULL);

	func = GDA_QUERY_FIELD_FUNC (iface);

	node = xmlNewNode (NULL, (xmlChar*)"gda_query_ffunc");
	
	str = gda_xml_storage_get_xml_id (iface);
	xmlSetProp(node, (xmlChar*)"id", (xmlChar*)str);
	g_free (str);

	xmlSetProp(node, (xmlChar*)"name", (xmlChar*)gda_object_get_name (GDA_OBJECT (func)));
	if (gda_object_get_description (GDA_OBJECT (func)) && *gda_object_get_description (GDA_OBJECT (func)))
		xmlSetProp(node, (xmlChar*)"descr", (xmlChar*)gda_object_get_description (GDA_OBJECT (func)));

	obj = NULL;
	if (gda_object_ref_activate (func->priv->func_ref))
		obj = gda_object_ref_get_ref_object (func->priv->func_ref);

	if (obj) {
		gchar *xmlid;
		xmlid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (obj));
		xmlSetProp(node, (xmlChar*)"func", (xmlChar*)xmlid);
	}
	else {
		const gchar *cstr;

		cstr = gda_object_ref_get_ref_name (func->priv->func_ref, NULL, NULL);
		if (cstr)
			xmlSetProp(node, (xmlChar*)"func_name", (xmlChar*)cstr);
	}

	if (! gda_query_field_is_visible (GDA_QUERY_FIELD (func)))
		xmlSetProp(node, (xmlChar*)"is_visible",  (xmlChar*)"f");
	if (gda_query_field_is_internal (GDA_QUERY_FIELD (func)))
		xmlSetProp(node, (xmlChar*)"is_internal", (xmlChar*)"t");

	str = (gchar *) gda_query_field_get_alias (GDA_QUERY_FIELD (func));
	if (str && *str) 
		xmlSetProp(node, (xmlChar*)"alias", (xmlChar*)str);

	/* function's arguments */
	list = func->priv->args;
	while (list) {
		xmlNodePtr argnode;

		argnode = xmlNewChild (node, NULL, (xmlChar*)"gda_query_field_ref", NULL);
		xmlSetProp(argnode, (xmlChar*)"object", (xmlChar*)gda_object_ref_get_ref_name (GDA_OBJECT_REF (list->data), NULL, NULL));
		list = g_slist_next (list);
	}

	return node;
}

static gboolean
gda_query_field_func_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaQueryFieldFunc *func;
	gchar *prop;
	gboolean funcref = FALSE;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	func = GDA_QUERY_FIELD_FUNC (iface);
	if (strcmp ((gchar*)node->name, "gda_query_ffunc")) {
		g_set_error (error,
			     GDA_QUERY_FIELD_FUNC_ERROR,
			     GDA_QUERY_FIELD_FUNC_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_query_ffunc>"));
		return FALSE;
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"id");
	if (prop) {
		gchar *ptr, *tok;
		ptr = strtok_r (prop, ":", &tok);
		ptr = strtok_r (NULL, ":", &tok);
		if (strlen (ptr) < 3) {
			g_set_error (error,
				     GDA_QUERY_FIELD_FUNC_ERROR,
				     GDA_QUERY_FIELD_FUNC_XML_LOAD_ERROR,
				     _("XML ID for a query field should be QUxxx:QFyyy where xxx and yyy are numbers"));
			return FALSE;
		}
		gda_query_object_set_int_id (GDA_QUERY_OBJECT (func), atoi (ptr+2));
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"name");
	if (prop) {
		gda_object_set_name (GDA_OBJECT (func), prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"descr");
	if (prop) {
		gda_object_set_description (GDA_OBJECT (func), prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"func");
	if (prop) {
		funcref = TRUE;
		gda_object_ref_set_ref_name (func->priv->func_ref, GDA_TYPE_DICT_FUNCTION, REFERENCE_BY_XML_ID, prop);
		g_free (prop);
	}
	else {
		prop = (gchar*)xmlGetProp(node, (xmlChar*)"func_name");
		if (prop) {
			funcref = TRUE;
			gda_object_ref_set_ref_name (func->priv->func_ref, GDA_TYPE_DICT_FUNCTION, REFERENCE_BY_NAME, prop);
			g_free (prop);
		}
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"is_visible");
	if (prop) {
		gda_query_field_set_visible (GDA_QUERY_FIELD (func), (*prop == 't') ? TRUE : FALSE);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"is_internal");
	if (prop) {
		gda_query_field_set_internal (GDA_QUERY_FIELD (func), (*prop == 't') ? TRUE : FALSE);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"alias");
	if (prop) {
		gda_query_field_set_alias (GDA_QUERY_FIELD (func), prop);
		g_free (prop);
	}
	
	/* function's arguments */
	if (node->children) {
		GdaDict *dict;
		xmlNodePtr argnode = node->children;
		dict = gda_object_get_dict (GDA_OBJECT (func));
		while (argnode) {
			if (!strcmp ((gchar*)argnode->name, "gda_query_field_ref")) {
				prop = (gchar*)xmlGetProp(argnode, (xmlChar*)"object");
				if (prop) {
					GdaObjectRef *ref;

					ref = GDA_OBJECT_REF (gda_object_ref_new (dict));
					gda_object_ref_set_ref_name (ref, GDA_TYPE_ENTITY_FIELD, REFERENCE_BY_XML_ID, prop);
					g_free (prop);
					func->priv->args = g_slist_append (func->priv->args, ref);
				}
			}
			argnode = argnode->next;
		}
	}
	
	if (funcref) {
		/* test the right number of arguments */
		GdaObject *ref;
		
		ref = gda_object_ref_get_ref_object (func->priv->func_ref);
		if (ref && 
		    (g_slist_length (func->priv->args) != 
		     g_slist_length ((GSList *) gda_dict_function_get_arg_dict_types (GDA_DICT_FUNCTION (ref))))) {
			g_set_error (error,
				     GDA_QUERY_FIELD_FUNC_ERROR,
				     GDA_QUERY_FIELD_FUNC_XML_LOAD_ERROR,
				     _("Wrong number of arguments for function %s"), gda_object_get_name (ref));
			return FALSE;
		}
		return TRUE;
	}
	else {
		g_set_error (error,
			     GDA_QUERY_FIELD_FUNC_ERROR,
			     GDA_QUERY_FIELD_FUNC_XML_LOAD_ERROR,
			     _("Missing required attributes for <gda_query_ffunc>"));
		return FALSE;
	}
}


/*
 * GdaRenderer interface implementation
 */

static gchar *
gda_query_field_func_render_as_sql (GdaRenderer *iface, GdaParameterList *context, 
				    GSList **out_params_used, GdaRendererOptions options, GError **error)
{
	gchar *str = NULL;
	GdaObject *base;
	GdaQueryFieldFunc *func;
	gboolean err = FALSE;
	GString *string;
	GSList *list;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv, NULL);
	func = GDA_QUERY_FIELD_FUNC (iface);

	base = gda_object_ref_get_ref_object (func->priv->func_ref);

	if (base) 
		string = g_string_new (gda_object_get_name (base));
	else {
		const gchar *cstr;
		cstr = gda_query_field_func_get_ref_func_name (func);
		if (cstr)
			string = g_string_new (cstr);
		else {
			g_set_error (error, 0, 0,
                                     _("Don't know how to render function"));
                        return NULL;
		}
	}

	g_string_append (string, " (");
	list = func->priv->args;
	while (list && !err) {
		gchar *argstr;
		GdaObject *argbase;
		
		if (list != func->priv->args)
			g_string_append (string, ", ");
		
		argbase = gda_object_ref_get_ref_object (GDA_OBJECT_REF (list->data));
		if (argbase) {
			argstr = gda_renderer_render_as_sql (GDA_RENDERER (argbase), context, 
							     out_params_used, options, error);
			if (argstr) {
				g_string_append (string, argstr);
				g_free (argstr);
			}
			else
				err = TRUE;
		}
		else {
			const gchar *tmpstr;
			tmpstr = gda_object_ref_get_ref_name (GDA_OBJECT_REF (list->data), NULL, NULL);
			g_set_error (error,
				     GDA_QUERY_FIELD_FUNC_ERROR,
				     GDA_QUERY_FIELD_FUNC_RENDER_ERROR,
				     _("Can't find referenced field '%s'"), tmpstr);
			err = TRUE;
		}
		
		list = g_slist_next (list);
	}
	g_string_append (string, ")");
	str = string->str;
	g_string_free (string, FALSE);

	if (err) {
		if (str)
			g_free (str);
		return NULL;
	}

	return str;
}

static gchar *
gda_query_field_func_render_as_str (GdaRenderer *iface, GdaParameterList *context)
{
	gchar *str = NULL;
	GdaObject *base;
	GdaQueryFieldFunc *func;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv, NULL);
	func = GDA_QUERY_FIELD_FUNC (iface);

	base = gda_object_ref_get_ref_object (func->priv->func_ref);

	if (base) {
		GString *string;
		GSList *list;

		string = g_string_new (gda_object_get_name (base));
		g_string_append (string, " (");
		list = func->priv->args;
		while (list) {
			gchar *argstr;
			GdaObject *argbase;

			if (list != func->priv->args)
				g_string_append (string, ", ");

			argbase = gda_object_ref_get_ref_object (GDA_OBJECT_REF (list->data));
			if (argbase) {
				argstr = gda_renderer_render_as_str (GDA_RENDERER (argbase), context);
				g_assert (argstr);
				g_string_append (string, argstr);
				g_free (argstr);
			}
			else {
				const gchar *tmpstr;
				tmpstr = gda_object_ref_get_ref_name (GDA_OBJECT_REF (list->data), NULL, NULL);
				g_string_append (string, tmpstr);
			}

			list = g_slist_next (list);
		}
		g_string_append (string, ")");

		str = string->str;
		g_string_free (string, FALSE);

	}
	else 
		str = g_strdup (_("Non-activated function"));
	
	return str;
}


/*
 * GdaReferer interface implementation
 */
static gboolean
gda_query_field_func_activate (GdaReferer *iface)
{
	gboolean active = FALSE;
	GdaQueryFieldFunc *func;
	GSList *list;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv, FALSE);
	func = GDA_QUERY_FIELD_FUNC (iface);

	active = gda_object_ref_activate (func->priv->func_ref);
	list = func->priv->args;
	while (list) {
		active = active && gda_object_ref_activate (GDA_OBJECT_REF (list->data));
		list = g_slist_next (list);
	}

	return active;
}

static void
gda_query_field_func_deactivate (GdaReferer *iface)
{
	GdaQueryFieldFunc *func;
	GSList *list;

	g_return_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface));
	g_return_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv);
	func = GDA_QUERY_FIELD_FUNC (iface);

	gda_object_ref_deactivate (func->priv->func_ref);
	list = func->priv->args;
	while (list) {
		gda_object_ref_deactivate (GDA_OBJECT_REF (list->data));
		list = g_slist_next (list);
	}
}

static gboolean
gda_query_field_func_is_active (GdaReferer *iface)
{
	gboolean active;
	GdaQueryFieldFunc *func;
	GSList *list;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface), FALSE);
	g_return_val_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv, FALSE);
	func = GDA_QUERY_FIELD_FUNC (iface);

	active = gda_object_ref_is_active (func->priv->func_ref);
	list = func->priv->args;
	while (list && active) {
		active = gda_object_ref_is_active (GDA_OBJECT_REF (list->data)) && active;
		list = g_slist_next (list);
	}

	return active;
}

static GSList *
gda_query_field_func_get_ref_objects (GdaReferer *iface)
{
	GSList *list = NULL;
        GdaObject *base;
	GdaQueryFieldFunc *func;
	GSList *args;

	g_return_val_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface), NULL);
	g_return_val_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv, NULL);
	func = GDA_QUERY_FIELD_FUNC (iface);

        base = gda_object_ref_get_ref_object (func->priv->func_ref);
        if (base)
                list = g_slist_append (list, base);

	args = func->priv->args;
	while (args) {
		base = gda_object_ref_get_ref_object (GDA_OBJECT_REF (args->data));
		if (base)
			list = g_slist_append (list, base);
		args = g_slist_next (args);
	}

        return list;
}

static void
gda_query_field_func_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	GdaQueryFieldFunc *func;
	GSList *list;

        g_return_if_fail (iface && GDA_IS_QUERY_FIELD_FUNC (iface));
        g_return_if_fail (GDA_QUERY_FIELD_FUNC (iface)->priv);

        func = GDA_QUERY_FIELD_FUNC (iface);
        if (func->priv->query) {
                GdaQuery *query = g_hash_table_lookup (replacements, func->priv->query);
                if (query) {
                        g_signal_handlers_disconnect_by_func (G_OBJECT (func->priv->query),
                                                              G_CALLBACK (destroyed_object_cb), func);
                        func->priv->query = query;
			gda_object_connect_destroy (query, 
						 G_CALLBACK (destroyed_object_cb), func);
                }
        }

        gda_object_ref_replace_ref_object (func->priv->func_ref, replacements);
	list = func->priv->args;
	while (list) {
		gda_object_ref_replace_ref_object (GDA_OBJECT_REF (list->data), replacements);
		list = g_slist_next (list);
	}
}
