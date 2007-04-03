/* gda-dict-type.c
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
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

#if 0 /* This object does not have any properties. */
static void gda_dict_type_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_dict_type_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);
#endif

static void        dict_type_xml_storage_init (GdaXmlStorageIface *iface);
static gchar      *dict_type_get_xml_id (GdaXmlStorage *iface);
static xmlNodePtr  dict_type_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    dict_type_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

#if 0 /* This object does not have any properties. */
/* properties */
enum
{
	PROP_0,
	PROP
};
#endif


/* private structure */
struct _GdaDictTypePrivate
{
	guint              numparams;
	GType       g_type;
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

        #if 0 /* This object does not have any properties. */
	/* Properties */
	object_class->set_property = gda_dict_type_set_property;
	object_class->get_property = gda_dict_type_get_property;

        /* TODO: What kind of object is this meant to be?
           When we know, we should use g_param_spec_object() instead of g_param_spec_pointer().
           murrayc.
         */
	g_object_class_install_property (object_class, PROP,
					 g_param_spec_pointer ("prop", NULL, NULL, (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        #endif
}

static void
gda_dict_type_init (GdaDictType * gda_dict_type)
{
	gda_dict_type->priv = g_new0 (GdaDictTypePrivate, 1);
	gda_dict_type->priv->numparams = -1;
	gda_dict_type->priv->g_type = 0;
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

#if 0 /* This object does not have any properties. */
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
#endif

/* GdaXmlStorage interface implementation */
static gchar *
dict_type_get_xml_id (GdaXmlStorage *iface)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_TYPE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_TYPE (iface)->priv, NULL);

	return gda_utility_build_encoded_id ("DT", gda_object_get_name (GDA_OBJECT (iface)));
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

	node = xmlNewNode (NULL, (xmlChar*)"gda_dict_type");
	
	str = dict_type_get_xml_id (iface);
	xmlSetProp(node, (xmlChar*)"id", (xmlChar*)str);
	g_free (str);
	xmlSetProp(node, (xmlChar*)"name", (xmlChar*)gda_object_get_name (GDA_OBJECT (dt)));
	xmlSetProp(node, (xmlChar*)"owner", (xmlChar*)gda_object_get_owner (GDA_OBJECT (dt)));
	xmlSetProp(node, (xmlChar*)"descr", (xmlChar*)gda_object_get_description (GDA_OBJECT (dt)));
	str = g_strdup_printf ("%d", dt->priv->numparams);
	xmlSetProp(node, (xmlChar*)"nparam", (xmlChar*)str);
	g_free (str);
	xmlSetProp(node, (xmlChar*)"gdatype", (xmlChar*)gda_g_type_to_string (dt->priv->g_type));

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
		xmlSetProp(node, (xmlChar*)"synonyms", (xmlChar*)string->str);
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
	if (strcmp ((gchar*)node->name, "gda_dict_type")) {
		g_set_error (error,
			     GDA_DICT_TYPE_ERROR,
			     GDA_DICT_TYPE_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_dict_type>"));
		return FALSE;
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"name");
	if (prop) {
		pname = TRUE;
		gda_object_set_name (GDA_OBJECT (dt), prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"descr");
	if (prop) {
		gda_object_set_description (GDA_OBJECT (dt), prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"owner");
	if (prop) {
		gda_object_set_owner (GDA_OBJECT (dt), prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"nparam");
	if (prop) {
		pnparam = TRUE;
		dt->priv->numparams = atoi (prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"gdatype");
	if (prop) {
		dt->priv->g_type = gda_g_type_from_string (prop);
		if (dt->priv->g_type == G_TYPE_INVALID)
			g_set_error (error,
				     GDA_DICT_TYPE_ERROR,
				     GDA_DICT_TYPE_XML_LOAD_ERROR,
				     _("Unknown GType '%s'"), prop);
		else
			pgdatype = TRUE;
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"synonyms");
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
		if (error && !*error)
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
 * gda_dict_type_set_g_type
 * @dt: a #GdaDictType object
 * @g_type: 
 *
 * Set the gda type for a data type
 */
void
gda_dict_type_set_g_type (GdaDictType *dt, GType g_type)
{
	g_return_if_fail (dt && GDA_IS_DICT_TYPE (dt));
	g_return_if_fail (dt->priv);

	dt->priv->g_type = g_type;
}

/**
 * gda_dict_type_get_g_type
 * @dt: a #GdaDictType object
 *
 * Get the gda type of a data type
 *
 * Returns: the gda type
 */
GType
gda_dict_type_get_g_type (GdaDictType *dt)
{
	g_return_val_if_fail (dt && GDA_IS_DICT_TYPE (dt), G_TYPE_INVALID);
	g_return_val_if_fail (dt->priv, G_TYPE_INVALID);

	return dt->priv->g_type;
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
