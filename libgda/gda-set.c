/* gda-set.c
 *
 * Copyright (C) 2003 - 2010 Vivien Malerba
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
#include <libgda/gda-custom-marshal.h>
#include <libgda/gda-types.h>

extern xmlDtdPtr gda_paramlist_dtd;
extern gchar *gda_lang_locale;

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
static GError *validate_change_holder_cb (GdaHolder *holder, const GValue *value, GdaSet *dataset);
static void source_changed_holder_cb (GdaHolder *holder, GdaSet *dataset);
static void att_holder_changed_cb (GdaHolder *holder, const gchar *att_name, const GValue *att_value, GdaSet *dataset);

static void compute_public_data (GdaSet *set);
static gboolean gda_set_real_add_holder (GdaSet *set, GdaHolder *holder);

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
	HOLDER_ATTR_CHANGED,
	VALIDATE_HOLDER_CHANGE,
	VALIDATE_SET,
	LAST_SIGNAL
};

static gint gda_set_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0 };


/* private structure */
struct _GdaSetPrivate
{
	gchar           *id;
	gchar           *name;
	gchar           *descr;
	GHashTable      *holders_hash; /* key = GdaHoler ID, value = GdaHolder */
	GArray          *holders_array;
	gboolean         read_only;
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
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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
		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaSet", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static gboolean
validate_accumulator (GSignalInvocationHint *ihint,
		      GValue *return_accu,
		      const GValue *handler_return,
		      gpointer data)
{
	GError *error;

	error = g_value_get_boxed (handler_return);
	g_value_set_boxed (return_accu, error);

	return error ? FALSE : TRUE; /* stop signal if an error has been set */
}

static GError *
m_validate_holder_change (GdaSet *set, GdaHolder *holder, const GValue *new_value)
{
	return NULL;
}

static GError *
m_validate_set (GdaSet *set)
{
	return NULL;
}

static void
gda_set_class_init (GdaSetClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_set_signals[HOLDER_CHANGED] =
		g_signal_new ("holder-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaSetClass, holder_changed),
			      NULL, NULL,
			      _gda_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
			      GDA_TYPE_HOLDER);

	/**
	 * GdaSet::validate-holder-change
	 * @set: the #GdaSet
	 * @holder: the #GdaHolder which is going to change
	 * @new_value: the proposed new value for @holder
	 * 
	 * Gets emitted when a #GdaHolder's in @set is going to change its value. One can connect to
	 * this signal to control which values @holder can have (for example to implement some business rules)
	 *
	 * Return value: NULL if @holder is allowed to change its value to @new_value, or a #GError
	 * otherwise.
	 */
	gda_set_signals[VALIDATE_HOLDER_CHANGE] =
		g_signal_new ("validate-holder-change",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaSetClass, validate_holder_change),
			      validate_accumulator, NULL,
			      _gda_marshal_ERROR__OBJECT_VALUE, GDA_TYPE_ERROR, 2,
			      GDA_TYPE_HOLDER, G_TYPE_VALUE);
	/**
	 * GdaSet::validate-set
	 * @set: the #GdaSet
	 * 
	 * Gets emitted when gda_set_is_valid() is called, use
	 * this signal to control which combination of values @set's holder can have (for example to implement some business rules)
	 *
	 * Return value: NULL if @set's contents has been validated, or a #GError
	 * otherwise.
	 */
	gda_set_signals[VALIDATE_SET] =
		g_signal_new ("validate-set",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaSetClass, validate_set),
			      validate_accumulator, NULL,
			      _gda_marshal_ERROR__VOID, GDA_TYPE_ERROR, 0);
	/**
	 * GdaSet::holder-attr-changed
	 * @set: the #GdaSet
	 * @holder: the GdaHolder for which an attribute changed
	 * @attr_name: attribute's name
	 * @attr_value: attribute's value
	 * 
	 * Gets emitted when an attribute for any of the #GdaHolder objects managed by @set has changed
	 */
	gda_set_signals[HOLDER_ATTR_CHANGED] =
		g_signal_new ("holder-attr-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaSetClass, holder_attr_changed),
			      NULL, NULL,
			      _gda_marshal_VOID__OBJECT_STRING_VALUE, G_TYPE_NONE, 3,
			      GDA_TYPE_HOLDER, G_TYPE_STRING, G_TYPE_VALUE);
	/**
	 * GdaSet::public-data-changed
	 * @set: the #GdaSet
	 * 
	 * Gets emitted when @set's public data (#GdaSetNode, #GdaSetGroup or #GdaSetSource values) have changed
	 */
	gda_set_signals[PUBLIC_DATA_CHANGED] =
		g_signal_new ("public-data-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaSetClass, public_data_changed),
			      NULL, NULL,
			      _gda_marshal_VOID__VOID, G_TYPE_NONE, 0);

	class->holder_changed = NULL;
	class->validate_holder_change = m_validate_holder_change;
	class->validate_set = m_validate_set;
	class->holder_attr_changed = NULL;
	class->public_data_changed = NULL;

	/* Properties */
	object_class->set_property = gda_set_set_property;
	object_class->get_property = gda_set_get_property;
	g_object_class_install_property (object_class, PROP_ID,
					 g_param_spec_string ("id", NULL, "Id", NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_NAME,
					 g_param_spec_string ("name", NULL, "Name", NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_DESCR,
					 g_param_spec_string ("description", NULL, "Description", NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_HOLDERS,
					 g_param_spec_pointer ("holders", "GSList of GdaHolders", 
							       "GdaHolder objects the set should contain", (
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
	set->priv->holders_array = NULL;
	set->priv->read_only = FALSE;
}


/**
 * gda_set_new:
 * @holders: (element-type GdaHolder) (transfer:none): a list of #GdaHolder objects
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

	obj = g_object_new (GDA_TYPE_SET, NULL);
	for (; holders; holders = holders->next) 
		gda_set_real_add_holder ((GdaSet*) obj, GDA_HOLDER (holders->data));
	compute_public_data ((GdaSet*) obj);

	return (GdaSet*) obj;
}

/*
 * _gda_set_new_read_only
 */
GdaSet *
_gda_set_new_read_only (GSList *holders)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_SET, NULL);
	((GdaSet*) obj)->priv->read_only = TRUE;
	for (; holders; holders = holders->next) 
		gda_set_real_add_holder ((GdaSet*) obj, GDA_HOLDER (holders->data));
	compute_public_data ((GdaSet*) obj);

	return (GdaSet*) obj;
}



/**
 * gda_set_copy:
 * @set: a #GdaSet object
 *
 * Creates a new #GdaSet object, copy of @set
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
	g_slist_free (holders);

	return copy;
}

/**
 * gda_set_new_inline:
 * @nb: the number of value holders which will be contained in the new #GdaSet
 * @...: a serie of a (const gchar*) id, (GType) type, and value
 *
 * Creates a new #GdaSet containing holders defined by each triplet in ...
 * For each triplet (id, Glib type and value), 
 * the value must be of the correct type (gchar * if type is G_STRING, ...)
 *
 * Note that this function is a utility function and that only a limited set of types are supported. Trying
 * to use an unsupported type will result in a warning, and the returned value holder holding a safe default
 * value.
 *
 * Returns: a new #GdaSet object
 */ 
GdaSet *
gda_set_new_inline (gint nb, ...)
{
	GdaSet *set = NULL;
	GSList *holders = NULL;
	va_list ap;
	gchar *id;
	gint i;
	gboolean allok = TRUE;

	/* build the list of holders */
	va_start (ap, nb);
	for (i = 0; i < nb; i++) {
		GType type;
		GdaHolder *holder;
		GValue *value;
		GError *lerror = NULL;

		id = va_arg (ap, char *);
		type = va_arg (ap, GType);
		holder = (GdaHolder *) g_object_new (GDA_TYPE_HOLDER, "g-type", type, "id", id, NULL);

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
		else if (type == G_TYPE_GTYPE)
			g_value_set_gtype (value, va_arg(ap, GType));
		else {
			g_warning (_("%s() does not handle values of type '%s'."),
				   __FUNCTION__, g_type_name (type));
			g_object_unref (holder);
			allok = FALSE;
			break;
		}

		if (!gda_holder_take_value (holder, value, &lerror)) {
			g_warning (_("Unable to set holder's value: %s"),
				   lerror && lerror->message ? lerror->message : _("No detail"));
			if (lerror)
				g_error_free (lerror);
			g_object_unref (holder);
			allok = FALSE;
			break;
		}
		holders = g_slist_append (holders, holder);
        }
	va_end (ap);

	/* create the set */
	if (allok) 
		set = gda_set_new (holders);
	if (holders) {
		g_slist_foreach (holders, (GFunc) g_object_unref, NULL);
		g_slist_free (holders);
	}
	return set;
}

/**
 * gda_set_set_holder_value:
 * @set: a #GdaSet object
 * @error: a place to store errors, or %NULL
 * @holder_id: the ID of the holder to set the value
 * @...: value, of the correct type, depending on the requested holder's type (not NULL)
 *
 * Set the value of the #GdaHolder which ID is @holder_id to a specified value
 *
 * Returns: TRUE if no error occurred and the value was set correctly
 */
gboolean
gda_set_set_holder_value (GdaSet *set, GError **error, const gchar *holder_id, ...)
{
	GdaHolder *holder;
	va_list ap;
	GValue *value;
	GType type;

	g_return_val_if_fail (GDA_IS_SET (set), FALSE);
	g_return_val_if_fail (set->priv, FALSE);

	holder = gda_set_get_holder (set, holder_id);
	if (!holder) {
		g_set_error (error, GDA_SET_ERROR, GDA_SET_HOLDER_NOT_FOUND_ERROR,
			     _("GdaHolder with ID '%s' not found in set"), holder_id);
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
	else if (type == G_TYPE_GTYPE)
		g_value_set_gtype (value, va_arg (ap, GType));	
	else {
		g_set_error (error, 0, 0,
			     _("%s() does not handle values of type '%s'."),
			     __FUNCTION__, g_type_name (type));
		va_end (ap);
		return FALSE;
	}

	va_end (ap);
	return gda_holder_take_value (holder, value, error);
}

/**
 * gda_set_get_holder_value:
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
 * gda_set_new_from_spec_string:
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
                                             "%s", "XML spec. does not conform to DTD");

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
 * gda_set_new_from_spec_node:
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
	const gchar *lang = gda_lang_locale;

	xmlNodePtr cur;
	gboolean allok = TRUE;
	gchar *str;

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
					g_set_error (error, 0, 0, "%s", err->message);
					g_object_unref (model);
					model = NULL;
					allok = FALSE;
				}
				else  {
					sources = g_slist_prepend (sources, model);
					str = (gchar*)xmlGetProp(cur, (xmlChar*) "name");
					if (str) 
						g_object_set_data_full (G_OBJECT (model), "name", str, xmlFree);
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
								     "g-type", 
								     gdatype ? gda_g_type_from_string ((gchar *) gdatype) : G_TYPE_STRING,
								     NULL));
				holders = g_slist_append (holders, holder);
			}
			if (gdatype)
				xmlFree (gdatype);
			
			/* set holder's attributes */
			if (! gda_utility_holder_load_attributes (holder, cur, sources, error))
				allok = FALSE;
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
 * gda_set_remove_holder:
 * @set: a #GdaSet object
 * @holder: the #GdaHolder to remove from @set
 *
 * Removes a #GdaHolder from the list of holders managed by @set
 */
void
gda_set_remove_holder (GdaSet *set, GdaHolder *holder)
{
	GdaSetNode *node;

	g_return_if_fail (GDA_IS_SET (set));
	g_return_if_fail (set->priv);
	g_return_if_fail (g_slist_find (set->holders, holder));

	g_signal_handlers_disconnect_by_func (G_OBJECT (holder),
					      G_CALLBACK (validate_change_holder_cb), set);
	if (! set->priv->read_only) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (holder),
						      G_CALLBACK (changed_holder_cb), set);
		g_signal_handlers_disconnect_by_func (G_OBJECT (holder),
						      G_CALLBACK (source_changed_holder_cb), set);
		g_signal_handlers_disconnect_by_func (G_OBJECT (holder),
						      G_CALLBACK (att_holder_changed_cb), set);
	}

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
	if (set->priv->holders_array) {
		g_array_free (set->priv->holders_array, TRUE);
		set->priv->holders_array = NULL;
	}
	g_object_unref (G_OBJECT (holder));
}

static void
source_changed_holder_cb (GdaHolder *holder, GdaSet *set)
{
	compute_public_data (set);
}

static void
att_holder_changed_cb (GdaHolder *holder, const gchar *att_name, const GValue *att_value, GdaSet *set)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'HOLDER_ATTR_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (set), gda_set_signals[HOLDER_ATTR_CHANGED], 0, holder, att_name, att_value);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'HOLDER_ATTR_CHANGED' from %s\n", __FUNCTION__);
#endif
}

static GError *
validate_change_holder_cb (GdaHolder *holder, const GValue *value, GdaSet *set)
{
	/* signal the holder validate-change */
	GError *error = NULL;
	if (set->priv->read_only)
		g_set_error (&error, GDA_SET_ERROR, GDA_SET_READ_ONLY_ERROR, _("Data set does not allow modifications"));
	else {
#ifdef GDA_DEBUG_signal
		g_print (">> 'VALIDATE_HOLDER_CHANGE' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (set), gda_set_signals[VALIDATE_HOLDER_CHANGE], 0, holder, value, &error);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'VALIDATE_HOLDER_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	return error;
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
							      G_CALLBACK (validate_change_holder_cb), set);
			if (! set->priv->read_only) {
				g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
								      G_CALLBACK (changed_holder_cb), set);
				g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
								      G_CALLBACK (source_changed_holder_cb), set);
				g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
								      G_CALLBACK (att_holder_changed_cb), set);
			}
			g_object_unref (list->data);
		}
		g_slist_free (set->holders);
	}
	if (set->priv->holders_hash) {
		g_hash_table_destroy (set->priv->holders_hash);
		set->priv->holders_hash = NULL;
	}
	if (set->priv->holders_array) {
		g_array_free (set->priv->holders_array, TRUE);
		set->priv->holders_array = NULL;
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
	GHashTable *groups = NULL;

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
		
		set->nodes_list = g_slist_prepend (set->nodes_list, node);
	}
	set->nodes_list = g_slist_reverse (set->nodes_list);

	/*
	 * Creation of the GdaSetSource and GdaSetGroup structures 
	 */
	for (list = set->nodes_list; list;list = list->next) {
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
		if (node->source_model && groups)
			group = g_hash_table_lookup (groups, node->source_model);
		if (group) 
			group->nodes = g_slist_append (group->nodes, node);
		else {
			group = g_new0 (GdaSetGroup, 1);
			group->nodes = g_slist_append (NULL, node);
			group->nodes_source = source;
			set->groups_list = g_slist_prepend (set->groups_list, group);
			if (node->source_model) {
				if (!groups)
					groups = g_hash_table_new (NULL, NULL); /* key = source model, 
										   value = GdaSetGroup */
				g_hash_table_insert (groups, node->source_model, group);
			}
		}		
	}
	set->groups_list = g_slist_reverse (set->groups_list);
	if (groups)
		g_hash_table_destroy (groups);

#ifdef GDA_DEBUG_signal
        g_print (">> 'PUBLIC_DATA_CHANGED' from %p\n", set);
#endif
	g_signal_emit (set, gda_set_signals[PUBLIC_DATA_CHANGED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'PUBLIC_DATA_CHANGED' from %p\n", set);
#endif
}

/**
 * gda_set_add_holder:
 * @set: a #GdaSet object
 * @holder: a #GdaHolder object
 *
 * Adds @holder to the list of holders managed within @set.
 *
 * NOTE: if @set already has a #GdaHolder with the same ID as @holder, then @holder
 * will not be added to the set (even if @holder's type or value is not the same as the
 * one already in @set).
 *
 * Returns: TRUE if @holder has been added to @set (and FALSE if it has not been added because there is another #GdaHolder
 * with the same ID)
 */
gboolean
gda_set_add_holder (GdaSet *set, GdaHolder *holder)
{
	gboolean added;
	g_return_val_if_fail (GDA_IS_SET (set), FALSE);
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);

	added = gda_set_real_add_holder (set, holder);
	if (added)
		compute_public_data (set);
	return added;
}

static gboolean
gda_set_real_add_holder (GdaSet *set, GdaHolder *holder)
{
	GdaHolder *similar;
	const gchar *hid;

	/* 
	 * try to find a similar holder in the set->holders:
	 * a holder B is similar to a holder A if it has the same ID
	 */
	hid = gda_holder_get_id (holder);
	if (!hid) {
		g_warning (_("GdaHolder needs to have an ID"));
		return FALSE;
	}

	similar = (GdaHolder*) g_hash_table_lookup (set->priv->holders_hash, hid);
	if (!similar) {
		/* really add @holder to the set */
		set->holders = g_slist_append (set->holders, holder);
		g_hash_table_insert (set->priv->holders_hash, (gchar*) hid, holder);
		if (set->priv->holders_array) {
			g_array_free (set->priv->holders_array, TRUE);
			set->priv->holders_array = NULL;
		}
		g_object_ref (holder);
		g_signal_connect (G_OBJECT (holder), "validate-change",
				  G_CALLBACK (validate_change_holder_cb), set);
		if (! set->priv->read_only) {
			g_signal_connect (G_OBJECT (holder), "changed",
					  G_CALLBACK (changed_holder_cb), set);
			g_signal_connect (G_OBJECT (holder), "source-changed",
					  G_CALLBACK (source_changed_holder_cb), set);
			g_signal_connect (G_OBJECT (holder), "attribute-changed",
					  G_CALLBACK (att_holder_changed_cb), set);
		}
		return TRUE;
	}
	else if (similar == holder)
		return FALSE;
	else {
#ifdef GDA_DEBUG_NO
		g_print ("In Set %p, Holder %p and %p are similar, keeping %p only\n", set, similar, holder, similar);
#endif
		return FALSE;
	}
}


/**
 * gda_set_merge_with_set:
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

	set->sources_list = g_slist_remove (set->sources_list, source);
	g_free (source);
}

/**
 * gda_set_is_valid:
 * @set: a #GdaSet object
 * @error: a place to store validation errors, or %NULL
 *
 * This method tells if all @set's #GdaHolder objects are valid, and if
 * they represent a valid combination of values, as defined by rules
 * external to Libgda: the "validate-set" signal is emitted and if none of the signal handlers return an
 * error, then the returned value is TRUE, otherwise the return value is FALSE as soon as a signal handler
 * returns an error.
 *
 * Returns: TRUE if the set is valid
 */
gboolean
gda_set_is_valid (GdaSet *set, GError **error)
{
	GSList *holders;

	g_return_val_if_fail (GDA_IS_SET (set), FALSE);
	g_return_val_if_fail (set->priv, FALSE);

	for (holders = set->holders; holders; holders = holders->next) {
		if (!gda_holder_is_valid ((GdaHolder*) holders->data)) {
			g_set_error (error, GDA_SET_ERROR, GDA_SET_INVALID_ERROR,
				     "%s", _("One or more values are invalid"));
			return FALSE;
		}
	}

	return _gda_set_validate (set, error);
}

gboolean
_gda_set_validate (GdaSet *set, GError **error)
{
	/* signal the holder validate-set */
	GError *lerror = NULL;
#ifdef GDA_DEBUG_signal
	g_print (">> 'VALIDATE_SET' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (set), gda_set_signals[VALIDATE_SET], 0, &lerror);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'VALIDATE_SET' from %s\n", __FUNCTION__);
#endif
	if (lerror) {
		g_propagate_error (error, lerror);
		return FALSE;
	}
	return TRUE;
}


/**
 * gda_set_get_holder:
 * @set: a #GdaSet object
 * @holder_id: the ID of the requested value holder
 *
 * Finds a #GdaHolder using its ID
 *
 * Returns: (transfer none): the requested #GdaHolder or %NULL
 */
GdaHolder *
gda_set_get_holder (GdaSet *set, const gchar *holder_id)
{
	g_return_val_if_fail (GDA_IS_SET (set), NULL);
	g_return_val_if_fail (holder_id, NULL);

	return (GdaHolder *) g_hash_table_lookup (set->priv->holders_hash, holder_id);
}

/**
 * gda_set_get_nth_holder:
 * @set: a #GdaSet object
 * @pos: the position of the requested #GdaHolder, starting at %0
 *
 * Finds a #GdaHolder using its position
 *
 * Returns: (transfer none): the requested #GdaHolder or %NULL
 *
 * Since: 4.2
 */
GdaHolder *
gda_set_get_nth_holder (GdaSet *set, gint pos)
{
	g_return_val_if_fail (GDA_IS_SET (set), NULL);
	if (! set->priv->holders_array) {
		GSList *list;
		set->priv->holders_array = g_array_sized_new (FALSE, FALSE, sizeof (GdaHolder*),
							      g_slist_length (set->holders));
		for (list = set->holders; list; list = list->next)
			g_array_append_val (set->priv->holders_array, list->data);
	}
	if ((pos < 0) || (pos > set->priv->holders_array->len))
		return NULL;
	else
		return g_array_index (set->priv->holders_array, GdaHolder*, pos);
}

/**
 * gda_set_get_node:
 * @set: a #GdaSet object
 * @holder: a #GdaHolder object
 *
 * Finds a #GdaSetNode holding information for @holder, don't modify the returned structure
 *
 * Returns: (transfer none): the requested #GdaSetNode or %NULL
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
 * gda_set_get_source:
 * @set: a #GdaSet object
 * @holder: a #GdaHolder object
 *
 * Finds a #GdaSetSource which contains the #GdaDataModel restricting the possible values of
 * @holder, don't modify the returned structure.
 *
 * Returns: (transfer none): the requested #GdaSetSource or %NULL
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
 * gda_set_get_group:
 * @set: a #GdaSet object
 * @holder: a #GdaHolder object
 *
 * Finds a #GdaSetGroup which lists a  #GdaSetNode containing @holder,
 * don't modify the returned structure.
 *
 * Returns: (transfer none): the requested #GdaSetGroup or %NULL
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
 * gda_set_get_source_for_model:
 * @set: a #GdaSet object
 * @model: a #GdaDataModel object
 *
 * Finds the #GdaSetSource structure used in @set for which @model is a
 * the data model (the returned structure should not be modified).
 *
 * Returns: (transfer none): the requested #GdaSetSource pointer or %NULL.
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
