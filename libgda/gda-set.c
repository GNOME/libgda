/* gda-set.c
 *
 * Copyright (C) 2003 - 2008 Vivien Malerba
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

#include <stdarg.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <glib/gi18n-lib.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include "gda-set.h"
#include "gda-marshal.h"
#include "gda-data-model.h"
#include "gda-data-model-import.h"
#include "gda-holder.h"
#include "gda-connection.h"
#include "gda-server-provider.h"
#include "gda-util.h"

extern xmlDtdPtr gda_paramlist_dtd;

/* 
 * Main static functions 
 */
static void gda_set_class_init (GdaSetClass *class);
static void gda_set_init (GdaSet *set);
static void gda_set_dispose (GObject *object);
static void gda_set_finalize (GObject *object);

static void set_remove_node (GdaSet *set, GdaSetNode *node);
static void set_remove_source (GdaSet *set, GdaSetSource *source);

static void changed_holder_cb (GdaHolder *holder, GdaSet *dataset);
static void source_changed_holder_cb (GdaHolder *holder, GdaSet *dataset);
static void notify_holder_cb (GdaHolder *holder, GParamSpec *pspec, GdaSet *dataset);

static void compute_public_data (GdaSet *set);
static void gda_set_real_add_holder (GdaSet *set, GdaHolder *holder);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_DESCR,
	PROP_HOLDERS
};

/* signals */
enum
{
	HOLDER_CHANGED,
	PUBLIC_DATA_CHANGED,
	HOLDER_PLUGIN_CHANGED,
	HOLDER_ATTR_CHANGED,
	LAST_SIGNAL
};

static gint gda_set_signals[LAST_SIGNAL] = { 0, 0, 0, 0 };


/* private structure */
struct _GdaSetPrivate
{
	gchar           *id;
	gchar           *name;
	gchar           *descr;
	GHashTable      *holders_hash; /* key = GdaHoler ID, value = GdaHolder */
};

static void 
gda_set_set_property (GObject *object,
		      guint param_id,
		      const GValue *value,
		      GParamSpec *pspec)
{
	GdaSet* set;
	set = GDA_SET (object);

	switch (param_id) {
	case PROP_ID:
		g_free (set->priv->id);
		set->priv->id = g_value_dup_string (value);
		break;
	case PROP_NAME:
		g_free (set->priv->name);
		set->priv->name = g_value_dup_string (value);
		break;
	case PROP_DESCR:
		g_free (set->priv->descr);
		set->priv->descr = g_value_dup_string (value);
		break;
	case PROP_HOLDERS: {
		/* add the holders */
		GSList* holders;
		for (holders = (GSList*) g_value_get_pointer(value); holders; holders = holders->next) 
			gda_set_real_add_holder (set, GDA_HOLDER (holders->data));
		compute_public_data (set);	
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gda_set_get_property (GObject *object,
		      guint param_id,
		      GValue *value,
		      GParamSpec *pspec)
{
	GdaSet* set;
	set = GDA_SET (object);

	switch (param_id) {
	case PROP_ID:
		g_value_set_string (value, set->priv->id);
		break;
	case PROP_NAME:
		if (set->priv->name)
			g_value_set_string (value, set->priv->name);
		else
			g_value_set_string (value, set->priv->id);
		break;
	case PROP_DESCR:
		g_value_set_string (value, set->priv->descr);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/* module error */
GQuark gda_set_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_set_error");
	return quark;
}


GType
gda_set_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaSetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_set_class_init,
			NULL,
			NULL,
			sizeof (GdaSet),
			0,
			(GInstanceInitFunc) gda_set_init
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GdaSet", &info, 0);
	}

	return type;
}


static void
gda_set_class_init (GdaSetClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_set_signals[HOLDER_CHANGED] =
		g_signal_new ("holder_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaSetClass, holder_changed),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
			      GDA_TYPE_HOLDER);
	gda_set_signals[HOLDER_PLUGIN_CHANGED] =
		g_signal_new ("holder_plugin_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaSetClass, holder_plugin_changed),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
			      GDA_TYPE_HOLDER);
	gda_set_signals[HOLDER_ATTR_CHANGED] =
		g_signal_new ("holder_attr_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaSetClass, holder_attr_changed),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
			      GDA_TYPE_HOLDER);
	gda_set_signals[PUBLIC_DATA_CHANGED] =
		g_signal_new ("public_data_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaSetClass, public_data_changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE, 0);

	class->holder_changed = NULL;
	class->holder_plugin_changed = NULL;
	class->holder_attr_changed = NULL;
	class->public_data_changed = NULL;

	/* Properties */
	object_class->set_property = gda_set_set_property;
	object_class->get_property = gda_set_get_property;
	g_object_class_install_property (object_class, PROP_ID,
					 g_param_spec_string ("id", NULL, NULL, NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_NAME,
					 g_param_spec_string ("name", NULL, NULL, NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_DESCR,
					 g_param_spec_string ("description", NULL, NULL, NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_HOLDERS,
					 g_param_spec_pointer ("holders", "GSList of GdaHolders", 
							       "Holders the set should contain", (
								G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
  
	object_class->dispose = gda_set_dispose;
	object_class->finalize = gda_set_finalize;
}

static void
gda_set_init (GdaSet *set)
{
	set->priv = g_new0 (GdaSetPrivate, 1);
	set->holders = NULL;
	set->nodes_list = NULL;
	set->sources_list = NULL;
	set->groups_list = NULL;
	set->priv->holders_hash = g_hash_table_new (g_str_hash, g_str_equal);
}


/**
 * gda_set_new
 * @holders: a list of #GdaHolder objects
 *
 * Creates a new #GdaSet object, and populates it with the list given as argument.
 * The list can then be freed as it is copied. All the value holders in @holders are referenced counted
 * and modified, so they should not be used anymore afterwards.
 *
 * Returns: a new #GdaSet object
 */
GdaSet *
gda_set_new (GSList *holders)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_SET, "holders", holders, NULL);
	
	return GDA_SET (obj);
}

/**
 * gda_set_copy
 * @set: a #GdaSet object
 *
 * Creates a new #GdaSet object, opy of @set
 *
 * Returns: a new #GdaSet object
 */
GdaSet *
gda_set_copy (GdaSet *set)
{
	GdaSet *copy;
	GSList *list, *holders = NULL;
	g_return_val_if_fail (GDA_IS_SET (set), NULL);
	
	for (list = set->holders; list; list = list->next) 
		holders = g_slist_prepend (holders, gda_holder_copy (GDA_HOLDER (list->data)));
	holders = g_slist_reverse (holders);

	copy = g_object_new (GDA_TYPE_SET, "holders", holders, NULL);
	g_slist_foreach (holders, (GFunc) g_object_unref, NULL);

	return copy;
}

/**
 * gda_set_new_inline
 * @nb: the number of value holders which will be contained in the new #GdaSet
 * @...: a serie of a (const gchar*) id, (GType) type, and value
 *
 * Creates a new #GdaSet containing holders defined by each triplet in ...
 * For each triplet (id, Glib type and value), 
 * the value must be of the correct type (gchar * if type is G_STRING, ...)
 *
 * Note that this function is a utility function and that anly a limited set of types are supported. Trying
 * to use an unsupported type will result in a warning, and the returned value holder holding a safe default
 * value.
 *
 * Returns: a new #GdaSet object
 */ 
GdaSet *
gda_set_new_inline (gint nb, ...)
{
	GdaSet *set;
	GSList *holders = NULL;
	va_list ap;
	gchar *id;
	gint i;

	/* build the list of holders */
	va_start (ap, nb);
	for (i = 0; i < nb; i++) {
		GType type;
		GdaHolder *holder;
		GValue *value;

		id = va_arg (ap, char *);
		type = va_arg (ap, GType);
		holder = (GdaHolder *) g_object_new (GDA_TYPE_HOLDER, "g_type", type, "id", id, NULL);

		value = gda_value_new (type);
		if (type == G_TYPE_BOOLEAN) 
			g_value_set_boolean (value, va_arg (ap, int));
                else if (type == G_TYPE_STRING)
			g_value_set_string (value, va_arg (ap, gchar *));
                else if (type == G_TYPE_OBJECT)
			g_value_set_object (value, va_arg (ap, gpointer));
		else if (type == G_TYPE_INT)
			g_value_set_int (value, va_arg (ap, gint));
		else if (type == G_TYPE_UINT)
			g_value_set_uint (value, va_arg (ap, guint));
		else if (type == GDA_TYPE_BINARY)
			gda_value_set_binary (value, va_arg (ap, GdaBinary *));
		else if (type == G_TYPE_INT64)
			g_value_set_int64 (value, va_arg (ap, gint64));
		else if (type == G_TYPE_UINT64)
			g_value_set_uint64 (value, va_arg (ap, guint64));
		else if (type == GDA_TYPE_SHORT)
			gda_value_set_short (value, va_arg (ap, int));
		else if (type == GDA_TYPE_USHORT)
			gda_value_set_ushort (value, va_arg (ap, guint));
		else if (type == G_TYPE_CHAR)
			g_value_set_char (value, va_arg (ap, int));
		else if (type == G_TYPE_UCHAR)
			g_value_set_uchar (value, va_arg (ap, guint));
		else if (type == G_TYPE_FLOAT)
			g_value_set_float (value, va_arg (ap, double));
		else if (type == G_TYPE_DOUBLE)
			g_value_set_double (value, va_arg (ap, gdouble));
		else if (type == GDA_TYPE_NUMERIC)
			gda_value_set_numeric (value, va_arg (ap, GdaNumeric *));
		else if (type == G_TYPE_DATE)
			g_value_set_boxed (value, va_arg (ap, GDate *));
		else if (type == G_TYPE_LONG)
			g_value_set_long (value, va_arg (ap, glong));
		else if (type == G_TYPE_ULONG)
			g_value_set_ulong (value, va_arg (ap, gulong));
		else
			g_warning (_("%s() does not handle values of type %s, value not assigned."),
				   __FUNCTION__, g_type_name (type));

		if (!gda_holder_take_value (holder, value))
			g_warning (_("Could not set GdaHolder's value, value not assigned"));
		holders = g_slist_append (holders, holder);
        }
	va_end (ap);

	/* create the set */
	set = gda_set_new (holders);
	if (holders) {
		g_slist_foreach (holders, (GFunc) g_object_unref, NULL);
		g_slist_free (holders);
	}

	return set;
}

/**
 * gda_set_set_holder_value
 * @set: a #GdaSet object
 * @holder_id: the ID of the holder to set the value
 * @...: value, of the correct type, depending on the requested holder's type
 *
 * Set the value of the #GdaHolder which ID is @holder_id to a specified value
 *
 * Returns: TRUE if no error occurred and the value was set correctly
 */
gboolean
gda_set_set_holder_value (GdaSet *set, const gchar *holder_id, ...)
{
	GdaHolder *holder;
	va_list ap;
	GValue *value;
	GType type;

	g_return_val_if_fail (GDA_IS_SET (set), FALSE);
	g_return_val_if_fail (set->priv, FALSE);

	holder = gda_set_get_holder (set, holder_id);
	if (!holder) {
		g_warning (_("GdaHolder with ID '%s' not found in set"), holder_id);
		return FALSE;
	}
	type = gda_holder_get_g_type (holder);
	va_start (ap, holder_id);
	value = gda_value_new (type);
	if (type == G_TYPE_BOOLEAN) 
		g_value_set_boolean (value, va_arg (ap, int));
	else if (type == G_TYPE_STRING)
		g_value_set_string (value, va_arg (ap, gchar *));
	else if (type == G_TYPE_OBJECT)
		g_value_set_object (value, va_arg (ap, gpointer));
	else if (type == G_TYPE_INT)
		g_value_set_int (value, va_arg (ap, gint));
	else if (type == G_TYPE_UINT)
		g_value_set_uint (value, va_arg (ap, guint));
	else if (type == GDA_TYPE_BINARY)
		gda_value_set_binary (value, va_arg (ap, GdaBinary *));
	else if (type == G_TYPE_INT64)
		g_value_set_int64 (value, va_arg (ap, gint64));
	else if (type == G_TYPE_UINT64)
		g_value_set_uint64 (value, va_arg (ap, guint64));
	else if (type == GDA_TYPE_SHORT)
		gda_value_set_short (value, va_arg (ap, int));
	else if (type == GDA_TYPE_USHORT)
		gda_value_set_ushort (value, va_arg (ap, guint));
	else if (type == G_TYPE_CHAR)
		g_value_set_char (value, va_arg (ap, int));
	else if (type == G_TYPE_UCHAR)
		g_value_set_uchar (value, va_arg (ap, guint));
	else if (type == G_TYPE_FLOAT)
		g_value_set_float (value, va_arg (ap, double));
	else if (type == G_TYPE_DOUBLE)
		g_value_set_double (value, va_arg (ap, gdouble));
	else if (type == GDA_TYPE_NUMERIC)
		gda_value_set_numeric (value, va_arg (ap, GdaNumeric *));
	else if (type == G_TYPE_DATE)
		g_value_set_boxed (value, va_arg (ap, GDate *));
	else if (type == G_TYPE_LONG)
		g_value_set_long (value, va_arg (ap, glong));
	else if (type == G_TYPE_ULONG)
		g_value_set_ulong (value, va_arg (ap, gulong));
	else
		g_warning (_("%s() does not handle values of type %s, value not assigned."),
			   __FUNCTION__, g_type_name (type));
	
	va_end (ap);
	if (!gda_holder_take_value (holder, value)) {
		g_warning (_("Could not set GdaHolder's value, value not assigned"));
		return FALSE;
	}
	return TRUE;
}

/**
 * gda_set_get_holder_value
 * @set: a #GdaSet object
 * @holder_id: the ID of the holder to set the value
 *
 * Get the value of the #GdaHolder which ID is @holder_id
 *
 * Returns: the requested GValue, or %NULL (see gda_holder_get_value())
 */
const GValue *
gda_set_get_holder_value (GdaSet *set, const gchar *holder_id)
{
	GdaHolder *holder;

	g_return_val_if_fail (GDA_IS_SET (set), FALSE);
	g_return_val_if_fail (set->priv, FALSE);

	holder = gda_set_get_holder (set, holder_id);
	if (holder) 
		return gda_holder_get_value (holder);
	else
		return NULL;
}

static void
xml_validity_error_func (void *ctx, const char *msg, ...)
{
        va_list args;
        gchar *str, *str2, *newerr;

        va_start (args, msg);
        str = g_strdup_vprintf (msg, args);
        va_end (args);

	str2 = *((gchar **) ctx);

        if (str2) {
                newerr = g_strdup_printf ("%s\n%s", str2, str);
                g_free (str2);
        }
        else
                newerr = g_strdup (str);
        g_free (str);

	*((gchar **) ctx) = newerr;
}

/**
 * gda_set_new_from_spec_string
 * @xml_spec: a string
 * @error: a place to store the error, or %NULL
 *
 * Creates a new #GdaSet object from the @xml_spec
 * specifications
 *
 * Returns: a new object, or %NULL if an error occurred
 */
GdaSet *
gda_set_new_from_spec_string (const gchar *xml_spec, GError **error)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	GdaSet *set;

	/* string parsing */
	doc = xmlParseMemory (xml_spec, strlen (xml_spec));
	if (!doc)
		return NULL;

	{
                /* doc validation */
                xmlValidCtxtPtr validc;
                int xmlcheck;
		gchar *err_str = NULL;
		xmlDtdPtr old_dtd = NULL;

                validc = g_new0 (xmlValidCtxt, 1);
                validc->userData = &err_str;
                validc->error = xml_validity_error_func;
                validc->warning = NULL;

                xmlcheck = xmlDoValidityCheckingDefaultValue;
                xmlDoValidityCheckingDefaultValue = 1;

                /* replace the DTD with ours */
		if (gda_paramlist_dtd) {
			old_dtd = doc->intSubset;
			doc->intSubset = gda_paramlist_dtd;
		}

#ifndef G_OS_WIN32
                if (doc->intSubset && !xmlValidateDocument (validc, doc)) {
			if (gda_paramlist_dtd)
				doc->intSubset = old_dtd;
                        xmlFreeDoc (doc);
                        g_free (validc);
			
                        if (err_str) {
                                g_set_error (error,
                                             GDA_SET_ERROR,
                                             GDA_SET_XML_SPEC_ERROR,
                                             "XML spec. does not conform to DTD:\n%s", err_str);
                                g_free (err_str);
                        }
                        else
                                g_set_error (error,
                                             GDA_SET_ERROR,
                                             GDA_SET_XML_SPEC_ERROR,
                                             "XML spec. does not conform to DTD");

                        xmlDoValidityCheckingDefaultValue = xmlcheck;
                        return NULL;
                }
#endif
		if (gda_paramlist_dtd)
			doc->intSubset = old_dtd;
                xmlDoValidityCheckingDefaultValue = xmlcheck;
                g_free (validc);
        }

	/* doc is now non NULL */
	root = xmlDocGetRootElement (doc);
	if (strcmp ((gchar*)root->name, "data-set-spec") != 0){
		g_set_error (error, GDA_SET_ERROR, GDA_SET_XML_SPEC_ERROR,
			     _("Spec's root node != 'data-set-spec': '%s'"), root->name);
		return NULL;
	}

	/* creating holders */
	root = root->xmlChildrenNode;
	while (xmlNodeIsText (root)) 
		root = root->next; 

	set = gda_set_new_from_spec_node (root, error);
	xmlFreeDoc(doc);
	return set;
}


/**
 * gda_set_new_from_spec_node
 * @xml_spec: a #xmlNodePtr for a &lt;holders&gt; tag
 * @error: a place to store the error, or %NULL
 *
 * Creates a new #GdaSet object from the @xml_spec
 * specifications
 *
 * Returns: a new object, or %NULL if an error occurred
 */
GdaSet *
gda_set_new_from_spec_node (xmlNodePtr xml_spec, GError **error)
{
	GdaSet *set = NULL;
	GSList *holders = NULL, *sources = NULL;
	GSList *list;
	const gchar *lang;

	xmlNodePtr cur;
	gboolean allok = TRUE;
	gchar *str;

#ifdef HAVE_LC_MESSAGES
	lang = setlocale (LC_MESSAGES, NULL);
#else
	lang = setlocale (LC_CTYPE, NULL);
#endif

	if (strcmp ((gchar*)xml_spec->name, "parameters") != 0){
		g_set_error (error, GDA_SET_ERROR, GDA_SET_XML_SPEC_ERROR,
			     _("Missing node <parameters>: '%s'"), xml_spec->name);
		return NULL;
	}

	/* Holders' sources, not mandatory: makes the @sources list */
	cur = xml_spec->next;
	while (cur && (xmlNodeIsText (cur) || strcmp ((gchar*)cur->name, "sources"))) 
		cur = cur->next; 
	if (allok && cur && !strcmp ((gchar*)cur->name, "sources")){
		for (cur = cur->xmlChildrenNode; (cur != NULL) && allok; cur = cur->next) {
			if (xmlNodeIsText (cur)) 
				continue;

			if (!strcmp ((gchar*)cur->name, "gda_array")) {
				GdaDataModel *model;
				GSList *errors;

				model = gda_data_model_import_new_xml_node (cur);
				errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (model));
				if (errors) {
					GError *err = (GError *) errors->data;
					g_set_error (error, 0, 0, err->message);
					g_object_unref (model);
					model = NULL;
					allok = FALSE;
				}
				else  {
					sources = g_slist_prepend (sources, model);
					str = (gchar*)xmlGetProp(cur, (xmlChar*)"name");
					if (str) 
						g_object_set_data_full (G_OBJECT (model), "name", str, g_free);
				}
			}
		}
	}	

	/* holders */
	for (cur = xml_spec->xmlChildrenNode; cur && allok; cur = cur->next) {
		if (xmlNodeIsText (cur)) 
			continue;

		if (!strcmp ((gchar*)cur->name, "parameter")) {
			GdaHolder *holder = NULL;
			gchar *str, *id;
			xmlChar *this_lang;
			xmlChar *gdatype;

			/* don't care about entries for the wrong locale */
			this_lang = xmlGetProp(cur, (xmlChar*)"lang");
			if (this_lang && strncmp ((gchar*)this_lang, lang, strlen ((gchar*)this_lang))) {
				g_free (this_lang);
				continue;
			}

			/* find if there is already a holder with the same ID */
			id = (gchar*)xmlGetProp(cur, (xmlChar*)"id");
			for (list = holders; list && !holder; list = list->next) {
				str = (gchar *) gda_holder_get_id ((GdaHolder *) list->data);
				if (str && id && !strcmp (str, id))
					holder = (GdaHolder *) list->data;
			}
			if (id) 
				xmlFree (id);

			if (holder && !this_lang) {
				xmlFree (this_lang);
				continue;
			}
			g_free (this_lang);
			

			/* find data type and create GdaHolder */
			gdatype = xmlGetProp (cur, BAD_CAST "gdatype");

			if (!holder) {
				holder = (GdaHolder*) (g_object_new (GDA_TYPE_HOLDER,
								     "g_type", 
								     gdatype ? gda_g_type_from_string ((gchar *) gdatype) : G_TYPE_STRING,
								     NULL));
				holders = g_slist_append (holders, holder);
			}
			if (gdatype) xmlFree (gdatype);
			
			/* set holder's attributes */
			gda_utility_holder_load_attributes (holder, cur, sources);
		}
	}

	/* setting prepared new names from sources (models) */
	for (list = sources; list; list = list->next) {
		str = g_object_get_data (G_OBJECT (list->data), "newname");
		if (str) {
			g_object_set_data_full (G_OBJECT (list->data), "name", g_strdup (str), g_free);
			g_object_set_data (G_OBJECT (list->data), "newname", NULL);
		}
		str = g_object_get_data (G_OBJECT (list->data), "newdescr");
		if (str) {
			g_object_set_data_full (G_OBJECT (list->data), "descr", g_strdup (str), g_free);
			g_object_set_data (G_OBJECT (list->data), "newdescr", NULL);
		}
	}

	/* holders' values, constraints: TODO */
	
	/* GdaSet creation */
	if (allok) {
		xmlChar *prop;;
		set = gda_set_new (holders);

		prop = xmlGetProp(xml_spec, (xmlChar*)"id");
		if (prop) {
			set->priv->id = g_strdup ((gchar*)prop);
			xmlFree (prop);
		}
		prop = xmlGetProp(xml_spec, (xmlChar*)"name");
		if (prop) {
			set->priv->name = g_strdup ((gchar*)prop);
			xmlFree (prop);
		}
		prop = xmlGetProp(xml_spec, (xmlChar*)"descr");
		if (prop) {
			set->priv->descr = g_strdup ((gchar*)prop);
			xmlFree (prop);
		}
	}

	g_slist_foreach (holders, (GFunc) g_object_unref, NULL);
	g_slist_free (holders);
	g_slist_foreach (sources, (GFunc) g_object_unref, NULL);
	g_slist_free (sources);

	return set;
}


/**
 * gda_set_get_spec
 * @set: a #GdaSet object
 *
 * Get the specification as an XML string. See the gda_set_new_from_spec_string()
 * form more information about the XML specification string format.
 *
 * Returns: a new string
 */
gchar *
gda_set_get_spec (GdaSet *set)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlChar *xmlbuff;
	int buffersize;
	GSList *list;

	g_return_val_if_fail (GDA_IS_SET (set), NULL);

	doc = xmlNewDoc ((xmlChar*)"1.0");
	g_return_val_if_fail (doc, NULL);
	root = xmlNewDocNode (doc, NULL, (xmlChar*)"data-set-spec", NULL);
	xmlDocSetRootElement (doc, root);

	/* holders list */
	for (list = set->holders; list; list = list->next) {
		xmlNodePtr node;
		GdaHolder *holder = GDA_HOLDER (list->data);
		gchar *str;
		const gchar *cstr;

		node = xmlNewTextChild (root, NULL, (xmlChar*)"parameter", NULL);
		g_object_get (G_OBJECT (holder), "id", &str, NULL);
		if (str) 
			xmlSetProp(node, (xmlChar*)"id", (xmlChar*)str);
		g_free (str);

		g_object_get (G_OBJECT (holder), "name", &str, NULL);
		if (str)
			xmlSetProp(node, (xmlChar*)"name", (xmlChar*)cstr);
		g_free (str);

		g_object_get (G_OBJECT (holder), "description", &str, NULL);
		if (str)
			xmlSetProp(node, (xmlChar*)"descr", (xmlChar*)str);
		g_free (str);

		xmlSetProp(node, (xmlChar*)"gdatype", (xmlChar*)gda_g_type_to_string (gda_holder_get_g_type (holder)));

		xmlSetProp(node, (xmlChar*)"nullok", (xmlChar*)(gda_holder_get_not_null (holder) ? "FALSE" : "TRUE"));
		str = g_object_get_data (G_OBJECT (holder), "__gda_entry_plugin");
		if (str) 
			xmlSetProp(node, (xmlChar*)"plugin", (xmlChar*)str);
	}

	/* holders' values, sources, constraints: TODO */

	xmlDocDumpFormatMemory (doc, &xmlbuff, &buffersize, 1);
	
	xmlFreeDoc(doc);
	return (gchar *) xmlbuff;
}

/**
 * gda_set_remove_holder
 * @set:
 * @holder:
 */
void
gda_set_remove_holder (GdaSet *set, GdaHolder *holder)
{
	GdaSetNode *node;

	g_return_if_fail (GDA_IS_SET (set));
	g_return_if_fail (set->priv);
	g_return_if_fail (g_slist_find (set->holders, holder));

	g_signal_handlers_disconnect_by_func (G_OBJECT (holder),
					      G_CALLBACK (changed_holder_cb), set);
	g_signal_handlers_disconnect_by_func (G_OBJECT (holder),
					      G_CALLBACK (source_changed_holder_cb), set);
	g_signal_handlers_disconnect_by_func (G_OBJECT (holder),
					      G_CALLBACK (notify_holder_cb), set);

	/* now destroy the GdaSetNode and the GdaSetSource if necessary */
	node = gda_set_get_node (set, holder);
	g_assert (node);
	if (node->source_model) {
		GdaSetSource *source;

		source = gda_set_get_source_for_model (set, node->source_model);
		g_assert (source);
		g_assert (source->nodes);
		if (! source->nodes->next)
			set_remove_source (set, source);
	}
	set_remove_node (set, node);

	set->holders = g_slist_remove (set->holders, holder);
	g_hash_table_remove (set->priv->holders_hash, gda_holder_get_id (holder));
	g_object_unref (G_OBJECT (holder));
}

static void
source_changed_holder_cb (GdaHolder *holder, GdaSet *set)
{
	compute_public_data (set);
}

static void
notify_holder_cb (GdaHolder *holder, GParamSpec *pspec, GdaSet *set)
{
	if (!strcmp (pspec->name, "plugin")) {
#ifdef GDA_DEBUG_signal
		g_print (">> 'HOLDER_PLUGIN_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (set), gda_set_signals[HOLDER_PLUGIN_CHANGED], 0, holder);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'HOLDER_PLUGIN_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	if (!strcmp (pspec->name, "use-default-value")) {
#ifdef GDA_DEBUG_signal
		g_print (">> 'HOLDER_ATTR_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (set), gda_set_signals[HOLDER_ATTR_CHANGED], 0, holder);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'HOLDER_ATTR_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
}

static void
changed_holder_cb (GdaHolder *holder, GdaSet *set)
{
	/* signal the holder change */
#ifdef GDA_DEBUG_signal
	g_print (">> 'HOLDER_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (set), gda_set_signals[HOLDER_CHANGED], 0, holder);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'HOLDER_CHANGED' from %s\n", __FUNCTION__);
#endif
}

static void
group_free (GdaSetGroup *group, gpointer data)
{
	g_slist_free (group->nodes);
	g_free (group);
}

static void
gda_set_dispose (GObject *object)
{
	GdaSet *set;
	GSList *list;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_SET (object));

	set = GDA_SET (object);
	/* free the holders list */
	if (set->holders) {
		for (list = set->holders; list; list = list->next) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
				G_CALLBACK (changed_holder_cb), set);
			g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
				G_CALLBACK (source_changed_holder_cb), set);
			g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
				G_CALLBACK (notify_holder_cb), set);
			g_object_unref (G_OBJECT (list->data));
		}
		g_slist_free (set->holders);
	}
	if (set->priv->holders_hash) {
		g_hash_table_destroy (set->priv->holders_hash);
		set->priv->holders_hash = NULL;
	}

	/* free the nodes if there are some */
	while (set->nodes_list)
		set_remove_node (set, GDA_SET_NODE (set->nodes_list->data));
	while (set->sources_list)
		set_remove_source (set, GDA_SET_SOURCE (set->sources_list->data));

	g_slist_foreach (set->groups_list, (GFunc) group_free, NULL);
	g_slist_free (set->groups_list);
	set->groups_list = NULL;

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_set_finalize (GObject *object)
{
	GdaSet *set;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_SET (object));

	set = GDA_SET (object);
	if (set->priv) {
		g_free (set->priv->id);
		g_free (set->priv->name);
		g_free (set->priv->descr);
		g_free (set->priv);
		set->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void compute_shown_columns_index (GdaSetSource *source);
static void compute_ref_columns_index (GdaSetSource *source);

/*
 * Resets and computes set->nodes, and if some nodes already exist, they are previously discarded
 */
static void 
compute_public_data (GdaSet *set)
{
	GSList *list;
	GdaSetNode *node;
	GdaSetSource *source;
	GdaSetGroup *group;
	GHashTable *groups = g_hash_table_new (NULL, NULL); /* key = source model, 
							       value = GdaSetGroup */

	/*
	 * Get rid of all the previous structures
	 */
	while (set->nodes_list)
		set_remove_node (set, GDA_SET_NODE (set->nodes_list->data));
	while (set->sources_list)
		set_remove_source (set, GDA_SET_SOURCE (set->sources_list->data));

	g_slist_foreach (set->groups_list, (GFunc) group_free, NULL);
	g_slist_free (set->groups_list);
	set->groups_list = NULL;

	/*
	 * Creation of the GdaSetNode structures
	 */
	for (list = set->holders; list; list = list->next) {
		node = g_new0 (GdaSetNode, 1);
		node->holder = GDA_HOLDER (list->data);
		node->source_model = gda_holder_get_source_model (node->holder,
								  &(node->source_column));
		if (node->source_model)
			g_object_ref (node->source_model);
		
		set->nodes_list = g_slist_append (set->nodes_list, node);
	}

	/*
	 * Creation of the GdaSetSource and GdaSetGroup structures 
	 */
	list = set->nodes_list;
	while (list) {
		node = GDA_SET_NODE (list->data);
		
		/* source */
		source = NULL;
		if (node->source_model) {
			source = gda_set_get_source_for_model (set, node->source_model);
			if (source) 
				source->nodes = g_slist_prepend (source->nodes, node);
			else {
				source = g_new0 (GdaSetSource, 1);
				source->data_model = node->source_model;
				source->nodes = g_slist_prepend (NULL, node);
				set->sources_list = g_slist_prepend (set->sources_list, source);
			}
		}

		/* group */
		group = NULL;
		if (node->source_model)
			group = g_hash_table_lookup (groups, node->source_model);
		if (group) 
			group->nodes = g_slist_append (group->nodes, node);
		else {
			group = g_new0 (GdaSetGroup, 1);
			group->nodes = g_slist_append (NULL, node);
			group->nodes_source = source;
			set->groups_list = g_slist_append (set->groups_list, group);
			if (node->source_model)
				g_hash_table_insert (groups, node->source_model, group);
		}		

		list = g_slist_next (list);
	}
	
	/*
	 * Filling information in all the GdaSetSource structures
	 */
	list = set->sources_list;
	while (list) {
		compute_shown_columns_index (GDA_SET_SOURCE (list->data));
		compute_ref_columns_index (GDA_SET_SOURCE (list->data));
		list = g_slist_next (list);
	}

	g_hash_table_destroy (groups);

#ifdef GDA_DEBUG_signal
        g_print (">> 'PUBLIC_DATA_CHANGED' from %p\n", set);
#endif
	g_signal_emit (set, gda_set_signals[PUBLIC_DATA_CHANGED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'PUBLIC_DATA_CHANGED' from %p\n", set);
#endif
}

static void
compute_shown_columns_index (GdaSetSource *source)
{
        gint ncols, nholders;
        gint *mask = NULL, masksize = 0;

        nholders = g_slist_length (source->nodes);
        g_return_if_fail (nholders > 0);
        ncols = gda_data_model_get_n_columns (GDA_DATA_MODEL (source->data_model));
        g_return_if_fail (ncols > 0);

        if (ncols > nholders) {
                /* we only want columns which are not holders */
                gint i, current = 0;

                masksize = ncols - nholders;
                mask = g_new0 (gint, masksize);
                for (i = 0; i < ncols ; i++) {
                        GSList *list = source->nodes;
                        gboolean found = FALSE;
                        while (list && !found) {
                                if (GDA_SET_NODE (list->data)->source_column == i)
                                        found = TRUE;
                                else
                                        list = g_slist_next (list);
                        }
                        if (!found) {
                                mask[current] = i;
                                current ++;
                        }
                }
                masksize = current;
        }
        else {
                /* we want all the columns */
                gint i;

                masksize = ncols;
                mask = g_new0 (gint, masksize);
                for (i=0; i<ncols; i++) {
                        mask[i] = i;
                }
        }

        source->shown_n_cols = masksize;
        source->shown_cols_index = mask;
}

static void
compute_ref_columns_index (GdaSetSource *source)
{
        gint ncols, nholders;
        gint *mask = NULL, masksize = 0;

        nholders = g_slist_length (source->nodes);
        g_return_if_fail (nholders > 0);
        ncols = gda_data_model_get_n_columns (GDA_DATA_MODEL (source->data_model));
        g_return_if_fail (ncols > 0);

        if (ncols > nholders) {
                /* we only want columns which are holders */
                gint i, current = 0;

                masksize = ncols - nholders;
                mask = g_new0 (gint, masksize);
                for (i=0; i<ncols ; i++) {
                        GSList *list = source->nodes;
                        gboolean found = FALSE;
                        while (list && !found) {
                                if (GDA_SET_NODE (list->data)->source_column == i)
                                        found = TRUE;
                                else
                                        list = g_slist_next (list);
                        }
                        if (found) {
                                mask[current] = i;
                                current ++;
                        }
                }
                masksize = current;
        }
        else {
                /* we want all the columns */
                gint i;

                masksize = ncols;
                mask = g_new0 (gint, masksize);
                for (i=0; i<ncols; i++) {
                        mask[i] = i;
                }
        }

        source->ref_n_cols = masksize;
        source->ref_cols_index = mask;
}


/**
 * gda_set_add_holder
 * @set: a #GdaSet object
 * @holder: a #GdaHolder object
 *
 * Adds @holder to the list of holders managed within @set.
 *
 * NOTE: if @set already has a #GdaHolder with the same ID as @holder, then @holder
 * will not be added to the set, even if @holder's type or value is not the same as the
 * on already in @set.
 */
void
gda_set_add_holder (GdaSet *set, GdaHolder *holder)
{
	g_return_if_fail (GDA_IS_SET (set));
	g_return_if_fail (GDA_IS_HOLDER (holder));

	gda_set_real_add_holder (set, holder);
	compute_public_data (set);
}

static void
gda_set_real_add_holder (GdaSet *set, GdaHolder *holder)
{
	GdaHolder *similar;

	if (g_slist_find (set->holders, holder))
		return;

	/* 
	 * try to find a similar holder in the set->holders:
	 * a holder B is similar to a holder A if it has the same ID
	 */
	similar = (GdaHolder*) g_hash_table_lookup (set->priv->holders_hash, gda_holder_get_id (holder));
	if (!similar) {
		/* really add @holder to the set */
		set->holders = g_slist_append (set->holders, holder);
		g_hash_table_insert (set->priv->holders_hash, (gchar*) gda_holder_get_id (holder), holder);
		g_object_ref (holder);
		g_signal_connect (G_OBJECT (holder), "changed",
				  G_CALLBACK (changed_holder_cb), set);
		g_signal_connect (G_OBJECT (holder), "source_changed",
				  G_CALLBACK (source_changed_holder_cb), set);
		g_signal_connect (G_OBJECT (holder), "notify",
				  G_CALLBACK (notify_holder_cb), set);
	}
#ifdef GDA_DEBUG_NO
	else 
		g_print ("In Set %p, Holder %p and %p are similar, keeping %p only\n", set, similar, holder, similar);
#endif
}


/**
 * gda_set_merge_with_set
 * @set: a #GdaSet object
 * @set_to_merge: a #GdaSet object
 *
 * Add to @set all the holders of @set_to_merge. 
 * Note1: only the #GdaHolder of @set_to_merge for which no holder in @set has the same ID are merged
 * Note2: all the #GdaHolder merged in @set are still used by @set_to_merge.
 */
void
gda_set_merge_with_set (GdaSet *set, GdaSet *set_to_merge)
{
	GSList *holders;
	g_return_if_fail (GDA_IS_SET (set));
	g_return_if_fail (set_to_merge && GDA_IS_SET (set_to_merge));

	for (holders = set_to_merge->holders; holders; holders = holders->next)
		gda_set_real_add_holder (set, GDA_HOLDER (holders->data));
	compute_public_data (set);
}

static void
set_remove_node (GdaSet *set, GdaSetNode *node)
{
	g_return_if_fail (g_slist_find (set->nodes_list, node));

	if (node->source_model)
		g_object_unref (G_OBJECT (node->source_model));

	set->nodes_list = g_slist_remove (set->nodes_list, node);
	g_free (node);
}

static void
set_remove_source (GdaSet *set, GdaSetSource *source)
{
	g_return_if_fail (g_slist_find (set->sources_list, source));

	if (source->nodes)
		g_slist_free (source->nodes);
	g_free (source->shown_cols_index);
	g_free (source->ref_cols_index);

	set->sources_list = g_slist_remove (set->sources_list, source);
	g_free (source);
}

/**
 * gda_set_is_valid
 * @set: a #GdaSet object
 *
 * Tells if all the set's holders have valid data
 *
 * Returns: TRUE if the set is valid
 */
gboolean
gda_set_is_valid (GdaSet *set)
{
	GSList *holders;
	gboolean retval = TRUE;

	g_return_val_if_fail (GDA_IS_SET (set), FALSE);
	g_return_val_if_fail (set->priv, FALSE);

	for (holders = set->holders; holders && retval; holders = g_slist_next (holders)) {
		if (!gda_holder_is_valid ((GdaHolder*) holders->data)) 
			retval = FALSE;
		
#ifdef GDA_DEBUG_NO
		g_print ("== HOLDER %p: valid= %d, value=%s\n", holders->data, gda_holder_is_valid (GDA_HOLDER (holders->data)),
			 gda_holder_get_value (GDA_HOLDER (holders->data)) ?
			 gda_value_stringify (gda_holder_get_value (GDA_HOLDER (holders->data))) : "Null");
#endif
	}

	return retval;
}

/**
 * gda_set_get_holder
 * @set: a #GdaSet object
 * @holder_id: the ID of the requested value holder
 *
 * Finds a #GdaHolder using its ID
 *
 * Returns: a #GdaHolder or %NULL
 */
GdaHolder *
gda_set_get_holder (GdaSet *set, const gchar *holder_id)
{
	g_return_val_if_fail (GDA_IS_SET (set), NULL);
	g_return_val_if_fail (set->priv, NULL);

	return (GdaHolder *) g_hash_table_lookup (set->priv->holders_hash, holder_id);
}


/**
 * gda_set_get_node
 * @set: a #GdaSet object
 * @holder: a #GdaHolder object
 *
 * Finds a #GdaSetNode holding information for @holder, don't modify the returned structure
 *
 * Returns: a #GdaSetNode or %NULL
 */
GdaSetNode *
gda_set_get_node (GdaSet *set, GdaHolder *holder)
{
	GdaSetNode *retval = NULL;
	GSList *list;

	g_return_val_if_fail (GDA_IS_SET (set), NULL);
	g_return_val_if_fail (set->priv, NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	g_return_val_if_fail (g_slist_find (set->holders, holder), NULL);

	for (list = set->nodes_list; list && !retval; list = list->next) {
		if (GDA_SET_NODE (list->data)->holder == holder)
			retval = GDA_SET_NODE (list->data);
	}

	return retval;
}

/**
 * gda_set_get_source
 * @set: a #GdaSet object
 * @holder: a #GdaHolder object
 *
 * Finds a #GdaSetSource which contains the #GdaDataModel restricting the possible values of
 * @holder, don't modify the returned structure.
 *
 * Returns: a #GdaSetSource or %NULL
 */
GdaSetSource *
gda_set_get_source (GdaSet *set, GdaHolder *holder)
{
	GdaSetNode *node;
	
	node = gda_set_get_node (set, holder);
	if (node && node->source_model)
		return gda_set_get_source_for_model (set, node->source_model);
	else
		return NULL;
}

/**
 * gda_set_get_group
 * @set: a #GdaSet object
 * @holder: a #GdaHolder object
 *
 * Finds a #GdaSetGroup which lists a  #GdaSetNode containing @holder,
 * don't modify the returned structure.
 *
 * Returns: a #GdaSetGroup or %NULL
 */
GdaSetGroup *
gda_set_get_group (GdaSet *set, GdaHolder *holder)
{
	GdaSetGroup *retval = NULL;
	GSList *list, *sublist;

	g_return_val_if_fail (GDA_IS_SET (set), NULL);
	g_return_val_if_fail (set->priv, NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	g_return_val_if_fail (g_slist_find (set->holders, holder), NULL);

	for (list = set->groups_list; list && !retval; list = list->next) {
		sublist = GDA_SET_GROUP (list->data)->nodes;
		while (sublist && !retval) {
			if (GDA_SET_NODE (sublist->data)->holder == holder)
				retval = GDA_SET_GROUP (list->data);
			else
				sublist = g_slist_next (sublist);	
		}
	}

	return retval;
}


/**
 * gda_set_get_source_for_model
 * @set: a #GdaSet object
 * @model: a #GdaDataModel object
 *
 * Finds the #GdaSetSource structure used in @set for which @model is a
 * the data model, don't modify the returned structure
 *
 * Returns: a #GdaSetSource pointer or %NULL.
 */
GdaSetSource *
gda_set_get_source_for_model (GdaSet *set, GdaDataModel *model)
{
	GdaSetSource *retval = NULL;
	GSList *list;

	g_return_val_if_fail (GDA_IS_SET (set), NULL);
	g_return_val_if_fail (set->priv, NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	list = set->sources_list;
	while (list && !retval) {
		if (GDA_SET_SOURCE (list->data)->data_model == model)
			retval = GDA_SET_SOURCE (list->data);

		list = g_slist_next (list);
	}

	return retval;
}
