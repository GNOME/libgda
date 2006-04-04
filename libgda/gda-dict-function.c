/* gda-dict-function.c
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

#include "gda-dict.h"
#include "gda-dict-type.h"
#include "gda-dict-function.h"
#include "gda-server-provider.h"
#include "gda-xml-storage.h"
#include <string.h>
#include <libgda/gda-util.h>
#include <glib/gi18n-lib.h>

/* 
 * Main static functions 
 */
static void gda_dict_function_class_init (GdaDictFunctionClass * class);
static void gda_dict_function_init (GdaDictFunction * srv);
static void gda_dict_function_dispose (GObject   * object);
static void gda_dict_function_finalize (GObject   * object);

static void gda_dict_function_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void gda_dict_function_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);

#ifdef GDA_DEBUG
static void gda_dict_function_dump (GdaDictFunction *func, guint offset);
#endif

static void        gnome_db_function_xml_storage_init (GdaXmlStorageIface *iface);
static gchar      *gnome_db_function_get_xml_id (GdaXmlStorage *iface);
static xmlNodePtr  gnome_db_function_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gnome_db_function_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;


/* properties */
enum
{
	PROP_0,
	PROP
};


/* private structure */
struct _GdaDictFunctionPrivate
{
	gchar                 *objectid;       /* unique id for the function */
	GdaDictType           *result_type;
	GSList                *arg_types;      /* list of GdaDictType pointers */
};


/* module error */
GQuark gda_dict_function_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_dict_function_error");
	return quark;
}


GType
gda_dict_function_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDictFunctionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_dict_function_class_init,
			NULL,
			NULL,
			sizeof (GdaDictFunction),
			0,
			(GInstanceInitFunc) gda_dict_function_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gnome_db_function_xml_storage_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDictFunction", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
	}
	return type;
}

static void 
gnome_db_function_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = gnome_db_function_get_xml_id;
	iface->save_to_xml = gnome_db_function_save_to_xml;
	iface->load_from_xml = gnome_db_function_load_from_xml;
}


static void
gda_dict_function_class_init (GdaDictFunctionClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_dict_function_dispose;
	object_class->finalize = gda_dict_function_finalize;

	/* Properties */
	object_class->set_property = gda_dict_function_set_property;
	object_class->get_property = gda_dict_function_get_property;
	g_object_class_install_property (object_class, PROP,
					 g_param_spec_pointer ("prop", NULL, NULL, (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_dict_function_dump;
#endif

}

static void
gda_dict_function_init (GdaDictFunction * gda_dict_function)
{
	gda_dict_function->priv = g_new0 (GdaDictFunctionPrivate, 1);
	gda_dict_function->priv->objectid = NULL;
	gda_dict_function->priv->result_type = NULL;
	gda_dict_function->priv->arg_types = NULL;
}


/**
 * gda_dict_function_new
 * @dict: a #GdaDict object
 *
 * Creates a new GdaDictFunction object which rrpresents a function in the dictionary
 *
 * Returns: the new object
 */
GObject*
gda_dict_function_new (GdaDict *dict)
{
	GObject   *obj;
	GdaDictFunction *gda_dict_function;

	if (dict)
		g_return_val_if_fail (GDA_IS_DICT (dict), NULL);

	obj = g_object_new (GDA_TYPE_DICT_FUNCTION, "dict",
			    ASSERT_DICT (dict), NULL);
	gda_dict_function = GDA_DICT_FUNCTION (obj);

	return obj;
}


static void
gda_dict_function_dispose (GObject *object)
{
	GdaDictFunction *gda_dict_function;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_FUNCTION (object));

	gda_dict_function = GDA_DICT_FUNCTION (object);
	if (gda_dict_function->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		gda_dict_function_set_ret_type (gda_dict_function, NULL);
		gda_dict_function_set_arg_types (gda_dict_function, NULL);
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_dict_function_finalize (GObject   * object)
{
	GdaDictFunction *gda_dict_function;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_FUNCTION (object));

	gda_dict_function = GDA_DICT_FUNCTION (object);
	if (gda_dict_function->priv) {

		g_free (gda_dict_function->priv);
		gda_dict_function->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_dict_function_set_property (GObject *object,
			guint param_id,
			const GValue *value,
			GParamSpec *pspec)
{
	gpointer ptr;
	GdaDictFunction *gda_dict_function;

	gda_dict_function = GDA_DICT_FUNCTION (object);
	if (gda_dict_function->priv) {
		switch (param_id) {
		case PROP:
			/* FIXME */
			ptr = g_value_get_pointer (value);
			break;
		}
	}
}

static void
gda_dict_function_get_property (GObject *object,
			guint param_id,
			GValue *value,
			GParamSpec *pspec)
{
	GdaDictFunction *gda_dict_function;
	gda_dict_function = GDA_DICT_FUNCTION (object);
	
	if (gda_dict_function->priv) {
		switch (param_id) {
		case PROP:
			/* FIXME */
			g_value_set_pointer (value, NULL);
			break;
		}	
	}
}

#ifdef GDA_DEBUG
static void
gda_dict_function_dump (GdaDictFunction *func, guint offset)
{
	gchar *str;
	GString *string;
	GSList *list;
	gboolean first = TRUE;
	gint i;

	g_return_if_fail (func && GDA_IS_DICT_FUNCTION (func));
	g_return_if_fail (func->priv);
	
	/* string for the offset */
	str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

	/* return type */
	if (func->priv->result_type)
		g_print ("%sReturn data type: %s\n", str, gda_dict_type_get_sqlname (func->priv->result_type));
	else
		g_print ("%s" D_COL_ERR "No return type defined" D_COL_NOR "\n", str);
	
	/* arguments */
	string = g_string_new (" (");
	list = func->priv->arg_types;
	while (list) {
		if (first)
			first = FALSE;
		else
			g_string_append (string, ", ");
		g_string_append_printf (string, "%s", gda_dict_type_get_sqlname (GDA_DICT_TYPE (list->data)));
		list = g_slist_next (list);
	}
	g_string_append (string, ")");
	g_print ("%sArguments: %s\n", str, string->str);
	g_string_free (string, TRUE);

	g_free (str);
}
#endif



/* GdaXmlStorage interface implementation */
static gchar *
gnome_db_function_get_xml_id (GdaXmlStorage *iface)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_FUNCTION (iface), NULL);
	g_return_val_if_fail (GDA_DICT_FUNCTION (iface)->priv, NULL);

	return g_strconcat ("PR", GDA_DICT_FUNCTION (iface)->priv->objectid, NULL);
}

static xmlNodePtr
gnome_db_function_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL, subnode;
	GdaDictFunction *func;
	gchar *str;
	GSList *list;

	g_return_val_if_fail (iface && GDA_IS_DICT_FUNCTION (iface), NULL);
	g_return_val_if_fail (GDA_DICT_FUNCTION (iface)->priv, NULL);

	func = GDA_DICT_FUNCTION (iface);

	node = xmlNewNode (NULL, "gda_dict_function");

	str = gnome_db_function_get_xml_id (iface);
	xmlSetProp (node, "id", str);
	g_free (str);
	xmlSetProp (node, "name", gda_object_get_name (GDA_OBJECT (func)));
	xmlSetProp (node, "descr", gda_object_get_description (GDA_OBJECT (func)));
	xmlSetProp (node, "owner", gda_object_get_owner (GDA_OBJECT (func)));

	/* return type */
	if (func->priv->result_type) {
		subnode = xmlNewChild (node, NULL, "gda_func_param", NULL);
		
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (func->priv->result_type));
		xmlSetProp (subnode, "type", str);
		g_free (str);
		xmlSetProp (subnode, "way", "out");
	}

	/* argument types */
	list = func->priv->arg_types;
	while (list) {
		subnode = xmlNewChild (node, NULL, "gda_func_param", NULL);
		if (list->data) {
			str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
			xmlSetProp (subnode, "type", str);
		}
		xmlSetProp (subnode, "way", "in");
		list = g_slist_next (list);
	}

	return node;
}

static gboolean
gnome_db_function_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaDict *dict;
	GdaDictFunction *func;
	gchar *prop;
	gboolean pname = FALSE, pid = FALSE;
	xmlNodePtr subnode;
	GSList *argtypes = NULL;

	g_return_val_if_fail (iface && GDA_IS_DICT_FUNCTION (iface), FALSE);
	g_return_val_if_fail (GDA_DICT_FUNCTION (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	func = GDA_DICT_FUNCTION (iface);
	dict = gda_object_get_dict (GDA_OBJECT (func));
	if (strcmp (node->name, "gda_dict_function")) {
		g_set_error (error,
			     GDA_DICT_FUNCTION_ERROR,
			     GDA_DICT_FUNCTION_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_dict_function>"));
		return FALSE;
	}

	/* function's attributes */
	prop = xmlGetProp (node, "id");
	if (prop) {
		if ((*prop == 'P') && (*(prop+1)=='R')) {
			pid = TRUE;
			if (func->priv->objectid)
				g_free (func->priv->objectid);
			func->priv->objectid = g_strdup (prop+2);
		}
		g_free (prop);
	}

	prop = xmlGetProp (node, "name");
	if (prop) {
		pname = TRUE;
		gda_object_set_name (GDA_OBJECT (func), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "descr");
	if (prop) {
		gda_object_set_description (GDA_OBJECT (func), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "owner");
	if (prop) {
		gda_object_set_owner (GDA_OBJECT (func), prop);
		g_free (prop);
	}
	
	/* arguments and return type */
	subnode = node->children;
	while (subnode) {
		if (!strcmp (subnode->name, "gda_func_param")) {
			GdaDictType *dt = NULL;
			prop = xmlGetProp (subnode, "type");
			if (prop) {
				dt = gda_dict_get_data_type_by_xml_id (dict, prop);
				if (!dt) {
					/* Add a new custom data type (this can't happen at the moment because of the DTD) */
					TO_IMPLEMENT;
				}
				g_free (prop);
			}
			
			prop = xmlGetProp (subnode, "way");
			if (prop) {
				if (*prop == 'o') {
					if (func->priv->result_type) {
						g_set_error (error,
							     GDA_DICT_FUNCTION_ERROR,
							     GDA_DICT_FUNCTION_XML_LOAD_ERROR,
							     _("More than one return type for function '%s'"), 
							     gda_object_get_name (GDA_OBJECT (func)));
						return FALSE;
					}
					gda_dict_function_set_ret_type (func, dt);
				}
				else 
					argtypes = g_slist_prepend (argtypes, dt);
				g_free (prop);
			}
		}
		subnode = subnode->next;
	}

	argtypes = g_slist_reverse (argtypes);
	gda_dict_function_set_arg_types (func, argtypes);
	g_slist_free (argtypes);

	if (pname && pid)
		return TRUE;
	else {
		g_set_error (error,
			     GDA_DICT_FUNCTION_ERROR,
			     GDA_DICT_FUNCTION_XML_LOAD_ERROR,
			     _("Missing required attributes for <gda_dict_function>"));
		return FALSE;
	}
}




/**
 * gda_dict_function_set_dbms_id
 * @func: a #GdaDictFunction object
 * @id: the DBMS identifier
 *
 * Set the DBMS identifier of the function
 */
void
gda_dict_function_set_dbms_id (GdaDictFunction *func, const gchar *id)
{
	g_return_if_fail (func && GDA_IS_DICT_FUNCTION (func));
	g_return_if_fail (func->priv);
	g_return_if_fail (id && *id);

	if (func->priv->objectid)
		g_free (func->priv->objectid);

	func->priv->objectid = utility_build_encoded_id (NULL, id);
}


/**
 * gda_dict_function_get_dbms_id
 * @func: a #GdaDictFunction object
 *
 * Get the DBMS identifier of the function
 *
 * Returns: a new string with the function's id
 */
gchar *
gda_dict_function_get_dbms_id (GdaDictFunction *func)
{
	g_return_val_if_fail (func && GDA_IS_DICT_FUNCTION (func), NULL);
	g_return_val_if_fail (func->priv, NULL);

	return utility_build_decoded_id (NULL, func->priv->objectid);
}


/**
 * gda_dict_function_set_sqlname
 * @func: a #GdaDictFunction object
 * @sqlname: 
 *
 * Set the SQL name of the data type.
 */
void
gda_dict_function_set_sqlname (GdaDictFunction *func, const gchar *sqlname)
{
	g_return_if_fail (func && GDA_IS_DICT_FUNCTION (func));
	g_return_if_fail (func->priv);

	gda_object_set_name (GDA_OBJECT (func), sqlname);
}


/**
 * gda_dict_function_get_sqlname
 * @func: a #GdaDictFunction object
 *
 * Get the DBMS's name of a data type.
 *
 * Returns: the name of the data type
 */
const gchar *
gda_dict_function_get_sqlname (GdaDictFunction *func)
{
	g_return_val_if_fail (func && GDA_IS_DICT_FUNCTION (func), NULL);
	g_return_val_if_fail (func->priv, NULL);

	return gda_object_get_name (GDA_OBJECT (func));
}

static void destroyed_data_type_cb (GdaDictType *dt, GdaDictFunction *func);

/**
 * gda_dict_function_set_arg_types
 * @func: a #GdaDictFunction object
 * @arg_types: a list of #GdaDictType objects or #NULL values ordered to represent the data types
 * of the function's arguments .
 *
 * Set the arguments types of a function
 */
void 
gda_dict_function_set_arg_types (GdaDictFunction *func, const GSList *arg_types)
{
	GSList *list;

	g_return_if_fail (func && GDA_IS_DICT_FUNCTION (func));
	g_return_if_fail (func->priv);

	if (func->priv->arg_types) {
		list = func->priv->arg_types;
		while (list) {
			if (list->data) {
				g_signal_handlers_disconnect_by_func (G_OBJECT (list->data), 
								      G_CALLBACK (destroyed_data_type_cb),
								      func);
				g_object_unref (G_OBJECT (list->data));
			}
			list = g_slist_next (list);
		}
		g_slist_free (func->priv->arg_types);
	}

	func->priv->arg_types = g_slist_copy ((GSList *) arg_types);
	list = func->priv->arg_types;
	while (list) {
		if (list->data) {
			gda_object_connect_destroy (list->data,
						 G_CALLBACK (destroyed_data_type_cb), func);
			g_object_ref (G_OBJECT (list->data));
		}
		list = g_slist_next (list);
	}
}

/**
 * gda_dict_function_get_arg_types
 * @func: a #GdaDictFunction object
 * 
 * To consult the list of arguments types (and number) of a function.
 *
 * Returns: a list of #GdaDictType objects, the list MUST NOT be modified.
 */
const GSList *
gda_dict_function_get_arg_types (GdaDictFunction *func)
{
	g_return_val_if_fail (func && GDA_IS_DICT_FUNCTION (func), NULL);
	g_return_val_if_fail (func->priv, NULL);

	return func->priv->arg_types;
}

/**
 * gda_dict_function_set_ret_type
 * @func: a #GdaDictFunction object
 * @dt: a #GdaDictType object or #NULL
 *
 * Set the return type of a function
 */
void 
gda_dict_function_set_ret_type  (GdaDictFunction *func, GdaDictType *dt)
{
	g_return_if_fail (func && GDA_IS_DICT_FUNCTION (func));
	g_return_if_fail (func->priv);
	if (dt)
		g_return_if_fail (dt && GDA_IS_DICT_TYPE (dt));
	
	if (func->priv->result_type) { 
		g_signal_handlers_disconnect_by_func (G_OBJECT (func->priv->result_type), 
						      G_CALLBACK (destroyed_data_type_cb), func);
		g_object_unref (G_OBJECT (func->priv->result_type));
	}

	func->priv->result_type = dt;
	if (dt) {
		gda_object_connect_destroy (dt,
					 G_CALLBACK (destroyed_data_type_cb), func);
		g_object_ref (G_OBJECT (dt));
	}
}

static void
destroyed_data_type_cb (GdaDictType *dt, GdaDictFunction *func)
{
	gda_object_destroy (GDA_OBJECT (func));
}

/**
 * gda_dict_function_get_ret_type
 * @func: a #GdaDictFunction object
 * 
 * To consult the return type of a function.
 *
 * Returns: a #GdaDictType object.
 */
GdaDictType *
gda_dict_function_get_ret_type  (GdaDictFunction *func)
{
	g_return_val_if_fail (func && GDA_IS_DICT_FUNCTION (func), NULL);
	g_return_val_if_fail (func->priv, NULL);

	return func->priv->result_type;
}


/**
 * gda_dict_function_accepts_args
 * @func: a #GdaDictFunction object
 * @arg_types: a list of #GdaDictType objects or #NULL values, ordered
 *
 * Test if the proposed list of arguments (@arg_types) would be accepted by
 * the @func function.
 *
 * The non acceptance can be beause of data type incompatibilities or a wrong number
 * of data types.
 *
 * Returns: TRUE if accepted
 */
gboolean
gda_dict_function_accepts_args (GdaDictFunction *func, const GSList *arg_types)
{
	GSList *arg = (GSList *) arg_types, *list;
	gboolean args_ok = TRUE;
	GdaServerProviderInfo *sinfo = NULL;
	GdaDict *dict;
	GdaConnection *cnc;

	g_return_val_if_fail (func && GDA_IS_DICT_FUNCTION (func), FALSE);
	g_return_val_if_fail (func->priv, FALSE);

	dict = gda_object_get_dict (GDA_OBJECT (func));
	cnc = gda_dict_get_connection (dict);
	if (cnc) 
		sinfo = gda_connection_get_infos (cnc);
	list = (GSList *) gda_dict_function_get_arg_types (func);
	
	if (g_slist_length (arg) != g_slist_length (list))
		return FALSE;
	
	while (GDA_FUNC_AGG_TEST_PARAMS_DO_TEST && arg && list && args_ok) {
		if (!sinfo || !sinfo->implicit_data_types_casts) {
			/* Strict tests */
			if (arg->data && list->data &&
			    (arg->data != list->data))
				args_ok = FALSE;
		}
		else {
			/* GdaValueType compatibility test */
			if (arg->data && list->data &&
			    (gda_dict_type_get_gda_type (GDA_DICT_TYPE (arg->data)) !=
			     gda_dict_type_get_gda_type (GDA_DICT_TYPE (list->data))))
				args_ok = FALSE;
		}
		
		arg = g_slist_next (arg);
		list = g_slist_next (list);
	}

	return args_ok;
}
