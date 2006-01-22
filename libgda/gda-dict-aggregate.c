/* gda-dict-aggregate.c
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
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
#include "gda-dict-aggregate.h"
#include "gda-server-provider.h"
#include "gda-xml-storage.h"
#include "gda-dict.h"
#include "gda-dict-type.h"
#include <libgda/gda-util.h>

/* 
 * Main static functions 
 */
static void gda_dict_aggregate_class_init (GdaDictAggregateClass * class);
static void gda_dict_aggregate_init (GdaDictAggregate * agg);
static void gda_dict_aggregate_dispose (GObject   * object);
static void gda_dict_aggregate_finalize (GObject   * object);

static void gda_dict_aggregate_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void gda_dict_aggregate_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);

#ifdef GDA_DEBUG
static void gda_dict_aggregate_dump (GdaDictAggregate *agg, guint offset);
#endif

static void        gnome_db_aggregate_xml_storage_init (GdaXmlStorageIface *iface);
static gchar      *gnome_db_aggregate_get_xml_id (GdaXmlStorage *iface);
static xmlNodePtr  gnome_db_aggregate_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gnome_db_aggregate_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP
};


/* private structure */
struct _GdaDictAggregatePrivate
{
	gchar                 *objectid;       /* unique id for the aggregate */
	GdaDictType           *result_type;
	GdaDictType           *arg_type;
};


/* module error */
GQuark gda_dict_aggregate_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_dict_aggregate_error");
	return quark;
}


GType
gda_dict_aggregate_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDictAggregateClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_dict_aggregate_class_init,
			NULL,
			NULL,
			sizeof (GdaDictAggregate),
			0,
			(GInstanceInitFunc) gda_dict_aggregate_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gnome_db_aggregate_xml_storage_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDictAggregate", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
	}
	return type;
}

static void 
gnome_db_aggregate_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = gnome_db_aggregate_get_xml_id;
	iface->save_to_xml = gnome_db_aggregate_save_to_xml;
	iface->load_from_xml = gnome_db_aggregate_load_from_xml;
}


static void
gda_dict_aggregate_class_init (GdaDictAggregateClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_dict_aggregate_dispose;
	object_class->finalize = gda_dict_aggregate_finalize;

	/* Properties */
	object_class->set_property = gda_dict_aggregate_set_property;
	object_class->get_property = gda_dict_aggregate_get_property;
	g_object_class_install_property (object_class, PROP,
					 g_param_spec_pointer ("prop", NULL, NULL, (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_dict_aggregate_dump;
#endif

}

static void
gda_dict_aggregate_init (GdaDictAggregate * gda_dict_aggregate)
{
	gda_dict_aggregate->priv = g_new0 (GdaDictAggregatePrivate, 1);
	gda_dict_aggregate->priv->objectid = NULL;
	gda_dict_aggregate->priv->result_type = NULL;
	gda_dict_aggregate->priv->arg_type = NULL;
}


/**
 * gda_dict_aggregate_new
 * @dict: a #GdaDict object
 *
 * Creates a new GdaDictAggregate object which represents an aggregate in the dictionary
 *
 * Returns: the new object
 */
GObject*
gda_dict_aggregate_new (GdaDict *dict)
{
	GObject   *obj;
	GdaDictAggregate *gda_dict_aggregate;

	if (dict)
		g_return_val_if_fail (GDA_IS_DICT (dict), NULL);

	obj = g_object_new (GDA_TYPE_DICT_AGGREGATE, "dict",
			    ASSERT_DICT (dict), NULL);
	gda_dict_aggregate = GDA_DICT_AGGREGATE (obj);

	return obj;
}


static void
gda_dict_aggregate_dispose (GObject *object)
{
	GdaDictAggregate *gda_dict_aggregate;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_AGGREGATE (object));

	gda_dict_aggregate = GDA_DICT_AGGREGATE (object);
	if (gda_dict_aggregate->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		gda_dict_aggregate_set_ret_type (gda_dict_aggregate, NULL);
		gda_dict_aggregate_set_arg_type (gda_dict_aggregate, NULL);
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_dict_aggregate_finalize (GObject   * object)
{
	GdaDictAggregate *gda_dict_aggregate;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_AGGREGATE (object));

	gda_dict_aggregate = GDA_DICT_AGGREGATE (object);
	if (gda_dict_aggregate->priv) {

		g_free (gda_dict_aggregate->priv);
		gda_dict_aggregate->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_dict_aggregate_set_property (GObject *object,
			guint param_id,
			const GValue *value,
			GParamSpec *pspec)
{
	gpointer ptr;
	GdaDictAggregate *gda_dict_aggregate;

	gda_dict_aggregate = GDA_DICT_AGGREGATE (object);
	if (gda_dict_aggregate->priv) {
		switch (param_id) {
		case PROP:
			/* FIXME */
			ptr = g_value_get_pointer (value);
			break;
		}
	}
}

static void
gda_dict_aggregate_get_property (GObject *object,
			guint param_id,
			GValue *value,
			GParamSpec *pspec)
{
	GdaDictAggregate *gda_dict_aggregate;
	gda_dict_aggregate = GDA_DICT_AGGREGATE (object);
	
	if (gda_dict_aggregate->priv) {
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
gda_dict_aggregate_dump (GdaDictAggregate *agg, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (agg && GDA_IS_DICT_AGGREGATE (agg));
	g_return_if_fail (agg->priv);
	
	/* string for the offset */
	str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

	/* return type */
	g_print ("%s" D_COL_H1 "GdaDictAggregate" D_COL_NOR " %s (%p)\n",
		 str, gda_object_get_name (GDA_OBJECT (agg)), agg);
	if (agg->priv->result_type)
		g_print ("%sReturn data type: %s\n", str, gda_dict_type_get_sqlname (agg->priv->result_type));
	else
		g_print ("%s" D_COL_ERR "No return type defined" D_COL_NOR "\n", str);

	if (agg->priv->arg_type)
		g_print ("%sParameter data type: %s\n", str, gda_dict_type_get_sqlname (agg->priv->arg_type));
	else
		g_print ("%sAny argument type allowed\n", str);

	g_free (str);
}
#endif



/* GdaXmlStorage interface implementation */
static gchar *
gnome_db_aggregate_get_xml_id (GdaXmlStorage *iface)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_AGGREGATE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_AGGREGATE (iface)->priv, NULL);

	return g_strconcat ("AG", GDA_DICT_AGGREGATE (iface)->priv->objectid, NULL);
}

static xmlNodePtr
gnome_db_aggregate_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL, subnode;
	GdaDictAggregate *agg;
	gchar *str;

	g_return_val_if_fail (iface && GDA_IS_DICT_AGGREGATE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_AGGREGATE (iface)->priv, NULL);

	agg = GDA_DICT_AGGREGATE (iface);

	node = xmlNewNode (NULL, "gda_dict_aggregate");

	str = gnome_db_aggregate_get_xml_id (iface);
	xmlSetProp (node, "id", str);
	g_free (str);
	xmlSetProp (node, "name", gda_object_get_name (GDA_OBJECT (agg)));
	xmlSetProp (node, "descr", gda_object_get_description (GDA_OBJECT (agg)));
	xmlSetProp (node, "owner", gda_object_get_owner (GDA_OBJECT (agg)));

	/* return type */
	if (agg->priv->result_type) {
		subnode = xmlNewChild (node, NULL, "gda_func_param", NULL);
		
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (agg->priv->result_type));
		xmlSetProp (subnode, "type", str);
		g_free (str);
		xmlSetProp (subnode, "way", "out");
	}

	/* argument type */
	if (agg->priv->arg_type) {
		subnode = xmlNewChild (node, NULL, "gda_func_param", NULL);
		
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (agg->priv->arg_type));
		xmlSetProp (subnode, "type", str);
		g_free (str);
		xmlSetProp (subnode, "way", "in");
	}

	return node;
}

static gboolean
gnome_db_aggregate_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaDict *dict;
	GdaDictAggregate *agg;
	gchar *prop;
	gboolean pname = FALSE, pid = FALSE;
	xmlNodePtr subnode;

	g_return_val_if_fail (iface && GDA_IS_DICT_AGGREGATE (iface), FALSE);
	g_return_val_if_fail (GDA_DICT_AGGREGATE (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	agg = GDA_DICT_AGGREGATE (iface);
	dict = gda_object_get_dict (GDA_OBJECT (agg));
	if (strcmp (node->name, "gda_dict_aggregate")) {
		g_set_error (error,
			     GDA_DICT_AGGREGATE_ERROR,
			     GDA_DICT_AGGREGATE_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_dict_aggregate>"));
		return FALSE;
	}

	/* aggregate's attributes */
	prop = xmlGetProp (node, "id");
	if (prop) {
		if ((*prop == 'A') && (*(prop+1)=='G')) {
			pid = TRUE;
			if (agg->priv->objectid)
				g_free (agg->priv->objectid);
			agg->priv->objectid = g_strdup (prop+2);
		}
		g_free (prop);
	}

	prop = xmlGetProp (node, "name");
	if (prop) {
		pname = TRUE;
		gda_object_set_name (GDA_OBJECT (agg), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "descr");
	if (prop) {
		gda_object_set_description (GDA_OBJECT (agg), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "owner");
	if (prop) {
		gda_object_set_owner (GDA_OBJECT (agg), prop);
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
				g_free (prop);
			}

			if (!dt) {
				g_set_error (error,
					     GDA_DICT_AGGREGATE_ERROR,
					     GDA_DICT_AGGREGATE_XML_LOAD_ERROR,
					     _("Can't find data type for aggregate '%s'"), 
					     gda_object_get_name (GDA_OBJECT (agg)));
				return FALSE;
			}
			
			prop = xmlGetProp (subnode, "way");
			if (prop) {
				if (*prop == 'o') {
					if (agg->priv->result_type) {
						g_set_error (error,
							     GDA_DICT_AGGREGATE_ERROR,
							     GDA_DICT_AGGREGATE_XML_LOAD_ERROR,
							     _("More than one return type for aggregate '%s'"), 
							     gda_object_get_name (GDA_OBJECT (agg)));
						return FALSE;
					}
					gda_dict_aggregate_set_ret_type (agg, dt);
				}
				else {
					if (agg->priv->arg_type) {
						g_set_error (error,
							     GDA_DICT_AGGREGATE_ERROR,
							     GDA_DICT_AGGREGATE_XML_LOAD_ERROR,
							     _("More than one argument type for aggregate '%s'"), 
							     gda_object_get_name (GDA_OBJECT (agg)));
						return FALSE;
					}
					gda_dict_aggregate_set_arg_type (agg, dt);
				}
				g_free (prop);
			}
		}
		subnode = subnode->next;
	}

	if (pname && pid)
		return TRUE;
	else {
		g_set_error (error,
			     GDA_DICT_AGGREGATE_ERROR,
			     GDA_DICT_AGGREGATE_XML_LOAD_ERROR,
			     _("Missing required attributes for <gda_dict_aggregate>"));
		return FALSE;
	}
}




/**
 * gda_dict_aggregate_set_dbms_id
 * @agg: a #GdaDictAggregate object
 * @id: the DBMS identifier
 *
 * Set the DBMS identifier of the aggregate
 */
void
gda_dict_aggregate_set_dbms_id (GdaDictAggregate *agg, const gchar *id)
{
	g_return_if_fail (agg && GDA_IS_DICT_AGGREGATE (agg));
	g_return_if_fail (agg->priv);
	g_return_if_fail (id && *id);

	if (agg->priv->objectid)
		g_free (agg->priv->objectid);

	agg->priv->objectid = utility_build_encoded_id (NULL, id);
}


/**
 * gda_dict_aggregate_get_dbms_id
 * @agg: a #GdaDictAggregate object
 *
 * Get the DBMS identifier of the aggregate
 *
 * Returns: the aggregate's id
 */
gchar *
gda_dict_aggregate_get_dbms_id (GdaDictAggregate *agg)
{
	g_return_val_if_fail (agg && GDA_IS_DICT_AGGREGATE (agg), NULL);
	g_return_val_if_fail (agg->priv, NULL);

	return utility_build_decoded_id (NULL, agg->priv->objectid);
}


/**
 * gda_dict_aggregate_set_sqlname
 * @agg: a #GdaDictAggregate object
 * @sqlname: 
 *
 * Set the SQL name of the data type.
 */
void
gda_dict_aggregate_set_sqlname (GdaDictAggregate *agg, const gchar *sqlname)
{
	g_return_if_fail (agg && GDA_IS_DICT_AGGREGATE (agg));
	g_return_if_fail (agg->priv);

	gda_object_set_name (GDA_OBJECT (agg), sqlname);
}


/**
 * gda_dict_aggregate_get_sqlname
 * @agg: a #GdaDictAggregate object
 *
 * Get the DBMS's name of a data type.
 *
 * Returns: the name of the data type
 */
const gchar *
gda_dict_aggregate_get_sqlname (GdaDictAggregate *agg)
{
	g_return_val_if_fail (agg && GDA_IS_DICT_AGGREGATE (agg), NULL);
	g_return_val_if_fail (agg->priv, NULL);

	return gda_object_get_name (GDA_OBJECT (agg));
}

static void destroyed_data_type_cb (GdaDictType *dt, GdaDictAggregate *agg);

/**
 * gda_dict_aggregate_set_arg_type
 * @agg: a #GdaDictAggregate object
 * @dt: a #GdaDictType objects or #NULL value to represent the data type
 * of the aggregate's unique argument .
 *
 * Set the argument type of a aggregate
 */
void 
gda_dict_aggregate_set_arg_type (GdaDictAggregate *agg, GdaDictType *dt)
{
	g_return_if_fail (agg && GDA_IS_DICT_AGGREGATE (agg));
	g_return_if_fail (agg->priv);
	if (dt)
		g_return_if_fail (dt && GDA_IS_DICT_TYPE (dt));
	
	if (agg->priv->arg_type) { 
		g_signal_handlers_disconnect_by_func (G_OBJECT (agg->priv->arg_type), 
						      G_CALLBACK (destroyed_data_type_cb), agg);
		g_object_unref (G_OBJECT (agg->priv->arg_type));
	}

	agg->priv->arg_type = dt;
	if (dt) {
		gda_object_connect_destroy (dt, G_CALLBACK (destroyed_data_type_cb), agg);
		g_object_ref (G_OBJECT (dt));
	}
}

/**
 * gda_dict_aggregate_get_arg_type
 * @agg: a #GdaDictAggregate object
 * 
 * To consult the list of arguments types (and number) of a aggregate.
 *
 * Returns: a list of #GdaDictType objects, the list MUST NOT be modified.
 */
GdaDictType *
gda_dict_aggregate_get_arg_type (GdaDictAggregate *agg)
{
	g_return_val_if_fail (agg && GDA_IS_DICT_AGGREGATE (agg), NULL);
	g_return_val_if_fail (agg->priv, NULL);

	return agg->priv->arg_type;
}

/**
 * gda_dict_aggregate_set_ret_type
 * @agg: a #GdaDictAggregate object
 * @dt: a #GdaDictType object or #NULL
 *
 * Set the return type of a aggregate
 */
void 
gda_dict_aggregate_set_ret_type  (GdaDictAggregate *agg, GdaDictType *dt)
{
	g_return_if_fail (agg && GDA_IS_DICT_AGGREGATE (agg));
	g_return_if_fail (agg->priv);
	if (dt)
		g_return_if_fail (dt && GDA_IS_DICT_TYPE (dt));
	
	if (agg->priv->result_type) { 
		g_signal_handlers_disconnect_by_func (G_OBJECT (agg->priv->result_type), 
						      G_CALLBACK (destroyed_data_type_cb), agg);
		g_object_unref (G_OBJECT (agg->priv->result_type));
	}

	agg->priv->result_type = dt;
	if (dt) {
		gda_object_connect_destroy (dt, G_CALLBACK (destroyed_data_type_cb), agg);
		g_object_ref (G_OBJECT (dt));
	}
}

static void
destroyed_data_type_cb (GdaDictType *dt, GdaDictAggregate *agg)
{
	gda_object_destroy (GDA_OBJECT (agg));
}

/**
 * gda_dict_aggregate_get_ret_type
 * @agg: a #GdaDictAggregate object
 * 
 * To consult the return type of a aggregate.
 *
 * Returns: a #GdaDictType object.
 */
GdaDictType *
gda_dict_aggregate_get_ret_type  (GdaDictAggregate *agg)
{
	g_return_val_if_fail (agg && GDA_IS_DICT_AGGREGATE (agg), NULL);
	g_return_val_if_fail (agg->priv, NULL);

	return agg->priv->result_type;
}
