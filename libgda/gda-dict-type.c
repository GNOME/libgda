/* gda-dict-type.c
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

#include "gda-dict-type.h"
#include "gda-xml-storage.h"
#include <string.h>
#include <libgda/gda-util.h>
#include <glib/gi18n-lib.h>

/* 
 * Main static functions 
 */
static void gda_dict_type_class_init (GdaDictTypeClass * class);
static void gda_dict_type_init (GdaDictType * type);
static void gda_dict_type_dispose (GObject   * object);
static void gda_dict_type_finalize (GObject   * object);

static void gda_dict_type_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_dict_type_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);

static void        dict_type_xml_storage_init (GdaXmlStorageIface *iface);
static gchar      *dict_type_get_xml_id (GdaXmlStorage *iface);
static xmlNodePtr  dict_type_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    dict_type_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP
};


/* private structure */
struct _GdaDictTypePrivate
{
	guint              numparams;
	GdaValueType       gda_type;
	GSList            *synonyms; /* list of gchar* */
};


/* module error */
GQuark gda_dict_type_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_dict_type_error");
	return quark;
}


GType
gda_dict_type_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDictTypeClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_dict_type_class_init,
			NULL,
			NULL,
			sizeof (GdaDictType),
			0,
			(GInstanceInitFunc) gda_dict_type_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) dict_type_xml_storage_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDictType", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
	}
	return type;
}

static void 
dict_type_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = dict_type_get_xml_id;
	iface->save_to_xml = dict_type_save_to_xml;
	iface->load_from_xml = dict_type_load_from_xml;
}


static void
gda_dict_type_class_init (GdaDictTypeClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_dict_type_dispose;
	object_class->finalize = gda_dict_type_finalize;

	/* Properties */
	object_class->set_property = gda_dict_type_set_property;
	object_class->get_property = gda_dict_type_get_property;
	g_object_class_install_property (object_class, PROP,
					 g_param_spec_pointer ("prop", NULL, NULL, (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
gda_dict_type_init (GdaDictType * gda_dict_type)
{
	gda_dict_type->priv = g_new0 (GdaDictTypePrivate, 1);
	gda_dict_type->priv->numparams = -1;
	gda_dict_type->priv->gda_type = 0;
	gda_dict_type->priv->synonyms = NULL;
}

/**
 * gda_dict_type_new
 * @dict: a #GdaDict object, or %NULL
 *
 * Creates a new GdaDictType object which represent a data type defined in a data dictionary
 *
 * Returns: the new object
 */
GdaDictType*
gda_dict_type_new (GdaDict *dict)
{
	GObject   *obj;
	GdaDictType *gda_dict_type;

	if (dict)
		g_return_val_if_fail (GDA_IS_DICT (dict), NULL);

	obj = g_object_new (GDA_TYPE_DICT_TYPE, "dict",
			    ASSERT_DICT (dict), NULL);
	gda_dict_type = GDA_DICT_TYPE (obj);
	
	return gda_dict_type;
}


static void
gda_dict_type_dispose (GObject *object)
{
	GdaDictType *gda_dict_type;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_TYPE (object));

	gda_dict_type = GDA_DICT_TYPE (object);
	gda_object_destroy_check (GDA_OBJECT (object));

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_dict_type_finalize (GObject   * object)
{
	GdaDictType *gda_dict_type;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_TYPE (object));

	gda_dict_type = GDA_DICT_TYPE (object);
	if (gda_dict_type->priv) {
		if (gda_dict_type->priv->synonyms) {
			g_slist_foreach (gda_dict_type->priv->synonyms, (GFunc) g_free, NULL);
			g_slist_free (gda_dict_type->priv->synonyms);
		}
		g_free (gda_dict_type->priv);
		gda_dict_type->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_dict_type_set_property (GObject *object,
			guint param_id,
			const GValue *value,
			GParamSpec *pspec)
{
	gpointer ptr;
	GdaDictType *gda_dict_type;

	gda_dict_type = GDA_DICT_TYPE (object);
	if (gda_dict_type->priv) {
		switch (param_id) {
		case PROP:
			/* FIXME */
			ptr = g_value_get_pointer (value);
			break;
		}
	}
}

static void
gda_dict_type_get_property (GObject *object,
			guint param_id,
			GValue *value,
			GParamSpec *pspec)
{
	GdaDictType *gda_dict_type;
	gda_dict_type = GDA_DICT_TYPE (object);
	
	if (gda_dict_type->priv) {
		switch (param_id) {
		case PROP:
			/* FIXME */
			g_value_set_pointer (value, NULL);
			break;
		}	
	}
}

/* GdaXmlStorage interface implementation */
static gchar *
dict_type_get_xml_id (GdaXmlStorage *iface)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_TYPE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_TYPE (iface)->priv, NULL);

	return utility_build_encoded_id ("DT", gda_object_get_name (GDA_OBJECT (iface)));
}

static xmlNodePtr
dict_type_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	GdaDictType *dt;
	gchar *str;

	g_return_val_if_fail (iface && GDA_IS_DICT_TYPE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_TYPE (iface)->priv, NULL);
	dt = GDA_DICT_TYPE (iface);

	node = xmlNewNode (NULL, "gda_dict_type");
	
	str = dict_type_get_xml_id (iface);
	xmlSetProp (node, "id", str);
	g_free (str);
	xmlSetProp (node, "name", gda_object_get_name (GDA_OBJECT (dt)));
	xmlSetProp (node, "owner", gda_object_get_owner (GDA_OBJECT (dt)));
	xmlSetProp (node, "descr", gda_object_get_description (GDA_OBJECT (dt)));
	str = g_strdup_printf ("%d", dt->priv->numparams);
	xmlSetProp (node, "nparam", str);
	g_free (str);
	xmlSetProp (node, "gdatype", gda_type_to_string (dt->priv->gda_type));

	if (dt->priv->synonyms) {
		GSList *list = dt->priv->synonyms;
		GString *string;

		string = g_string_new ((gchar *) dt->priv->synonyms->data);
		list = g_slist_next (list);
		while (list) {
			g_string_append_c (string, ',');
			g_string_append (string, (gchar *) list->data);
			list = g_slist_next (list);			
		}
		xmlSetProp (node, "synonyms", string->str);
		g_string_free (string, TRUE);
	}

	return node;
}

static gboolean
dict_type_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaDictType *dt;
	gchar *prop;
	gboolean pname = FALSE, pnparam = FALSE, pgdatype = FALSE;

	g_return_val_if_fail (iface && GDA_IS_DICT_TYPE (iface), FALSE);
	g_return_val_if_fail (GDA_DICT_TYPE (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	dt = GDA_DICT_TYPE (iface);
	if (strcmp (node->name, "gda_dict_type")) {
		g_set_error (error,
			     GDA_DICT_TYPE_ERROR,
			     GDA_DICT_TYPE_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_dict_type>"));
		return FALSE;
	}

	prop = xmlGetProp (node, "name");
	if (prop) {
		pname = TRUE;
		gda_object_set_name (GDA_OBJECT (dt), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "descr");
	if (prop) {
		gda_object_set_description (GDA_OBJECT (dt), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "owner");
	if (prop) {
		gda_object_set_owner (GDA_OBJECT (dt), prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "nparam");
	if (prop) {
		pnparam = TRUE;
		dt->priv->numparams = atoi (prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "gdatype");
	if (prop) {
		pgdatype = TRUE;
		dt->priv->gda_type = gda_type_from_string (prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "synonyms");
	if (prop) {
		gchar *tok;
		gchar *buf;
		GSList *list = NULL;

		tok = strtok_r (prop, ",", &buf);
		if (tok) {
			list = g_slist_append (list, g_strdup (tok));
			tok = strtok_r (NULL, ",", &buf);
			while (tok) {
				list = g_slist_append (list, g_strdup (tok));
				tok = strtok_r (NULL, ",", &buf);				
			}
		}
		g_free (prop);
		dt->priv->synonyms = list;
	}

	if (pname && pnparam && pgdatype)
		return TRUE;
	else {
		g_set_error (error,
			     GDA_DICT_TYPE_ERROR,
			     GDA_DICT_TYPE_XML_LOAD_ERROR,
			     _("Missing required attributes for <gda_dict_type>"));
		return FALSE;
	}
}

/**
 * gda_dict_type_set_sqlname
 * @dt: a #GdaDictType object
 * @sqlname: 
 *
 * Set the SQL name of the data type.
 */
void
gda_dict_type_set_sqlname (GdaDictType *dt, const gchar *sqlname)
{
	g_return_if_fail (dt && GDA_IS_DICT_TYPE (dt));
	g_return_if_fail (dt->priv);

	gda_object_set_name (GDA_OBJECT (dt), sqlname);
}


/**
 * gda_dict_type_get_sqlname
 * @dt: a #GdaDictType object
 *
 * Get the DBMS's name of a data type.
 *
 * Returns: the name of the data type
 */
const gchar *
gda_dict_type_get_sqlname (GdaDictType *dt)
{
	g_return_val_if_fail (dt && GDA_IS_DICT_TYPE (dt), NULL);
	g_return_val_if_fail (dt->priv, NULL);

	return gda_object_get_name (GDA_OBJECT (dt));
}

/**
 * gda_dict_type_set_gda_type
 * @dt: a #GdaDictType object
 * @gda_type: 
 *
 * Set the gda type for a data type
 */
void
gda_dict_type_set_gda_type (GdaDictType *dt, GdaValueType gda_type)
{
	g_return_if_fail (dt && GDA_IS_DICT_TYPE (dt));
	g_return_if_fail (dt->priv);

	dt->priv->gda_type = gda_type;
}

/**
 * gda_dict_type_get_gda_type
 * @dt: a #GdaDictType object
 *
 * Get the gda type of a data type
 *
 * Returns: the gda type
 */
GdaValueType
gda_dict_type_get_gda_type (GdaDictType *dt)
{
	g_return_val_if_fail (dt && GDA_IS_DICT_TYPE (dt), GDA_VALUE_TYPE_UNKNOWN);
	g_return_val_if_fail (dt->priv, GDA_VALUE_TYPE_UNKNOWN);

	return dt->priv->gda_type;
}

/**
 * gda_dict_type_add_synonym
 * @dt: a #GdaDictType object
 * @synonym:
 *
 * Sets a new synonym to the @dt data type.
 */
void
gda_dict_type_add_synonym (GdaDictType *dt, const gchar *synonym)
{
	gboolean found = FALSE;
	GSList *list;

	g_return_if_fail (dt && GDA_IS_DICT_TYPE (dt));
	g_return_if_fail (dt->priv);
	g_return_if_fail (synonym && *synonym);

	list = dt->priv->synonyms;
	while (list && !found) {
		if (!strcmp (synonym, (gchar *) list->data))
			found = TRUE;
		list = g_slist_next (list);
	}
	if (!found)
		dt->priv->synonyms = g_slist_prepend (dt->priv->synonyms, g_strdup (synonym));
}

/**
 * gda_dict_type_get_synonyms
 * @dt: a #GdaDictType object
 *
 * Get a list of @dt's synonyms
 *
 * Returns: a list of strings which must not be modified
 */
const GSList *
gda_dict_type_get_synonyms (GdaDictType *dt)
{
	g_return_val_if_fail (dt && GDA_IS_DICT_TYPE (dt), NULL);
	g_return_val_if_fail (dt->priv, NULL);

	return dt->priv->synonyms;
}

/**
 * gda_dict_type_clear_synonyms
 * @dt: a #GdaDictType object
 *
 * Removes any synonym attached to @dt
 */
void
gda_dict_type_clear_synonyms (GdaDictType *dt)
{
	g_return_if_fail (dt && GDA_IS_DICT_TYPE (dt));
	g_return_if_fail (dt->priv);

	if (dt->priv->synonyms) {
		g_slist_foreach (dt->priv->synonyms, (GFunc) g_free, NULL);
		g_slist_free (dt->priv->synonyms);
		dt->priv->synonyms = NULL;
	}
}
