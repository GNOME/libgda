/* gda-object-ref.c
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

#include <string.h>
#include "gda-object-ref.h"
#include "gda-marshal.h"
#include "gda-xml-storage.h"
#include "gda-dict-database.h"
#include "gda-dict-table.h"
#include "gda-dict-field.h"
#include "gda-query.h"
#include "gda-query-target.h"
#include "gda-query-condition.h"
#include "gda-entity.h"
#include "gda-entity-field.h"
#include "gda-query-field.h"
#include "gda-connection.h"
#include "gda-dict-function.h"
#include "gda-dict-aggregate.h"
#include "gda-dict-reg-aggregates.h" /* For gda_aggregates_get_by_name() */
#include "gda-dict-reg-functions.h" /* For gda_functions_get_by_name() */

#include "gda-query-field-all.h"
#include "gda-query-field-field.h"
#include "gda-query-field-value.h"
#include "gda-query-field-func.h"


/* 
 * Main static functions 
 */
static void gda_object_ref_class_init (GdaObjectRefClass * class);
static void gda_object_ref_init (GdaObjectRef * srv);
static void gda_object_ref_dispose (GObject   * object);
static void gda_object_ref_finalize (GObject   * object);

static void gda_object_ref_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gda_object_ref_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);
static GType handled_object_type (GType type);


static void destroyed_object_cb (GObject *obj, GdaObjectRef *ref);
static void helper_ref_destroyed_cb (GdaObject *obj, GdaObjectRef *ref);

#ifdef GDA_DEBUG
static void gda_object_ref_dump (GdaObjectRef *field, guint offset);
#endif

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	REF_FOUND,
	REF_LOST,
	LAST_SIGNAL
};

static gint gda_object_ref_signals[LAST_SIGNAL] = { 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_HELPER_REF,
	PROP_OBJ_NAME
};


/* private structure */
struct _GdaObjectRefPrivate
{
	gboolean               increase_ref_object;
	GdaObject             *ref_object;
	GType                  requested_type;
	GdaObjectRefType       ref_type;
	gchar                 *ref_name; /* reference as provided by gda_object_ref_set_ref_name() */
	gchar                 *obj_name; /* real name of the object as returned by gda_object_get_name() */
	gboolean               block_signals;
	GdaObject             *helper_ref; /* a GdaQuery (for target resolution) or a GdaObjectRef (for a field resolution) */
};


/* module error */
GQuark gda_object_ref_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_object_ref_error");
	return quark;
}


GType
gda_object_ref_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaObjectRefClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_object_ref_class_init,
			NULL,
			NULL,
			sizeof (GdaObjectRef),
			0,
			(GInstanceInitFunc) gda_object_ref_init
		};		
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaObjectRef", &info, 0);
	}
	return type;
}

static void
gda_object_ref_class_init (GdaObjectRefClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_object_ref_signals[REF_FOUND] =
		g_signal_new ("ref_found",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaObjectRefClass, ref_found),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	gda_object_ref_signals[REF_LOST] =
		g_signal_new ("ref_lost",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaObjectRefClass, ref_lost),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	class->ref_found = NULL;
	class->ref_lost = NULL;

	object_class->dispose = gda_object_ref_dispose;
	object_class->finalize = gda_object_ref_finalize;

	/* Properties */
	object_class->set_property = gda_object_ref_set_property;
	object_class->get_property = gda_object_ref_get_property;
	g_object_class_install_property (object_class, PROP_HELPER_REF,
					 g_param_spec_object ("helper_ref", "Pointer to a Query object for target "
							       "resolution, or a GdaObjectRef referencing a target"
							       "for a field resolution",
							       NULL, 
                                                               GDA_TYPE_OBJECT, /* Either a GdaQuery or a GdaObjectRef */
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_OBJ_NAME,
					 g_param_spec_string ("obj_name", "Name of the object as when it was last found",
							      NULL, NULL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_object_ref_dump;
#endif
}

static void
gda_object_ref_init (GdaObjectRef * ref)
{
	ref->priv = g_new0 (GdaObjectRefPrivate, 1);
	ref->priv->increase_ref_object = TRUE; /* the default is to use g_object_ref() and g_object_unref() on object */
	ref->priv->ref_object = NULL;
	ref->priv->requested_type = 0;
	ref->priv->ref_type = REFERENCE_BY_XML_ID;
	ref->priv->ref_name = NULL;
	ref->priv->obj_name = NULL;
	ref->priv->block_signals = FALSE;
	ref->priv->helper_ref = NULL;
}


/**
 * gda_object_ref_new
 * @dict: a #GdaDict object
 *
 * Creates a new GdaObjectRef object. This #GdaObjectRef object does itself increase the
 * reference count of the referenced object, so if all the reference count holders call g_object_unref()
 * on there referenced object, then that object will not be destroyed because this #GdaObjectRef still
 * has a reference on it. Use gda_object_ref_new_no_ref_count() if you don't want to increase the reference
 * count of the referenced object.
 *
 * Returns: the new object
 */
GObject*
gda_object_ref_new (GdaDict *dict)
{
	GObject   *obj;
	GdaObjectRef *ref;

	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);

	obj = g_object_new (GDA_TYPE_OBJECT_REF, "dict", ASSERT_DICT (dict), NULL);
	ref = GDA_OBJECT_REF (obj);

	return obj;
}

/**
 * gda_object_ref_new_no_ref_count
 * @dict: a #GdaDict object
 *
 * Creates a new GdaObjectRef object. This #GdaObjectRef object does not
 * itself increase the reference count of the object it keeps a reference to,
 * which means that if all the reference count holders call g_object_unref()
 * on there referenced object, then that object will actually be destroyed
 * and a "ref_lost" signal will be emitted. Use gda_object_ref_new() if you want to
 * increase the count of the referenced object.
 *
 * Returns: the new object
 */
GObject *
gda_object_ref_new_no_ref_count (GdaDict *dict)
{
	GObject *obj;

	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);
	obj = gda_object_ref_new (ASSERT_DICT (dict));
	GDA_OBJECT_REF (obj)->priv->increase_ref_object = FALSE;

	return obj;
}

/**
 * gda_object_ref_new_copy
 * @orig: a #GdaObjectRef object
 *
 * Creates a new GdaObjectRef object which is a copy of @orig. This is a copy constructor.
 *
 * Returns: the new object
 */
GObject *
gda_object_ref_new_copy (GdaObjectRef *orig)
{
	GObject   *obj;
	GdaObjectRef *ref;

	g_return_val_if_fail (orig && GDA_IS_OBJECT_REF (orig), NULL);

	obj = g_object_new (GDA_TYPE_OBJECT_REF, "dict", gda_object_get_dict (GDA_OBJECT (orig)), NULL);
	ref = GDA_OBJECT_REF (obj);

	ref->priv->increase_ref_object = orig->priv->increase_ref_object;
	ref->priv->requested_type = orig->priv->requested_type;
	ref->priv->ref_type = orig->priv->ref_type;
	if (orig->priv->ref_name) 
		ref->priv->ref_name = g_strdup (orig->priv->ref_name);
	if (orig->priv->obj_name)
		ref->priv->obj_name = g_strdup (orig->priv->obj_name);

	if (orig->priv->ref_object) {
		GObject *obj = G_OBJECT (orig->priv->ref_object);

		if (orig->priv->increase_ref_object)
			g_object_ref (obj);

		gda_object_connect_destroy (obj, G_CALLBACK (destroyed_object_cb), ref);
		ref->priv->ref_object = GDA_OBJECT (obj);
		if (! ref->priv->block_signals) {
#ifdef GDA_DEBUG_signal
			g_print (">> 'REF_FOUND' from %s\n", __FUNCTION__);
#endif
			g_signal_emit (G_OBJECT (ref), gda_object_ref_signals[REF_FOUND], 0);
#ifdef GDA_DEBUG_signal
			g_print ("<< 'REF_FOUND' from %s\n", __FUNCTION__);
#endif
		}
	}

	return obj;	
}

static void 
destroyed_object_cb (GObject *obj, GdaObjectRef *ref)
{
	g_return_if_fail (ref->priv->ref_object && (G_OBJECT (ref->priv->ref_object) == obj));

	g_signal_handlers_disconnect_by_func (G_OBJECT (ref->priv->ref_object),
					      G_CALLBACK (destroyed_object_cb), ref);
	if (ref->priv->increase_ref_object)
		g_object_unref (ref->priv->ref_object);

	ref->priv->ref_object = NULL;
#ifdef GDA_DEBUG_signal
	g_print (">> 'REF_LOST' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (ref), gda_object_ref_signals[REF_LOST], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'REF_LOST' from %s\n", __FUNCTION__);
#endif
}

static void
gda_object_ref_dispose (GObject *object)
{
	GdaObjectRef *ref;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_OBJECT_REF (object));

	ref = GDA_OBJECT_REF (object);
	if (ref->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		if (ref->priv->ref_object)
			destroyed_object_cb (G_OBJECT (ref->priv->ref_object), ref);
		if (ref->priv->ref_name) {
			g_free (ref->priv->ref_name);
			ref->priv->ref_name = NULL;
		}
		if (ref->priv->obj_name) {
			g_free (ref->priv->obj_name);
			ref->priv->obj_name = NULL;
		}
		if (ref->priv->helper_ref)
			helper_ref_destroyed_cb (ref->priv->helper_ref, ref);
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_object_ref_finalize (GObject   * object)
{
	GdaObjectRef *ref;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_OBJECT_REF (object));

	ref = GDA_OBJECT_REF (object);
	if (ref->priv) {

		g_free (ref->priv);
		ref->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void
helper_ref_destroyed_cb (GdaObject *obj, GdaObjectRef *ref)
{
	g_assert (ref->priv->helper_ref == obj);
	ref->priv->helper_ref = NULL;
	g_signal_handlers_disconnect_by_func (obj,
					      G_CALLBACK (helper_ref_destroyed_cb), ref);  
}

static void 
gda_object_ref_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaObjectRef* ref = GDA_OBJECT_REF (object);

	if (ref->priv) {
		switch (param_id) {
		case PROP_HELPER_REF: {
			GdaObject* ptr = g_value_get_object (value);
			if (ref->priv->helper_ref != ptr) {
				if (ref->priv->helper_ref) 
					helper_ref_destroyed_cb (ref->priv->helper_ref, ref);
				
				if (ptr) {
					ref->priv->helper_ref = ptr;
					gda_object_connect_destroy (ptr,
								    G_CALLBACK (helper_ref_destroyed_cb), ref);
				}
			}
			break;
                }
		case PROP_OBJ_NAME:
			g_free (ref->priv->obj_name);
			ref->priv->obj_name = NULL;
			if (g_value_get_string (value))
				ref->priv->obj_name = g_strdup (g_value_get_string (value));
			break;
		}
	}
}

static void
gda_object_ref_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdaObjectRef *ref;
	ref = GDA_OBJECT_REF (object);
	
	if (ref->priv) {
		switch (param_id) {
		case PROP_HELPER_REF:
			g_value_set_object (value, G_OBJECT (ref->priv->helper_ref));
			break;
		case PROP_OBJ_NAME:
			g_value_set_string (value, ref->priv->obj_name);
			break;
		}	
	}
}

/*
 * Returns 0 if @type is not an handled object type, and the real handled object type
 * if it is handled by the GdaObjectRef object
 */
static GType
handled_object_type (GType type)
{
	GType retval = 0;

	/* types accepted AS IS */
	if ((type == GDA_TYPE_DICT_TABLE) ||
	    (type == GDA_TYPE_DICT_FIELD) ||
	    (type == GDA_TYPE_QUERY) ||
	    (type == GDA_TYPE_QUERY_TARGET) ||
	    (type == GDA_TYPE_ENTITY_FIELD) ||
	    (type == GDA_TYPE_DICT_FUNCTION) ||
	    (type == GDA_TYPE_DICT_AGGREGATE) ||
	    (type == GDA_TYPE_QUERY_FIELD))
		retval = type;

	/* type conversion */
	if ((type == GDA_TYPE_QUERY_FIELD_ALL) ||
	    (type == GDA_TYPE_QUERY_FIELD_FIELD) ||
	    (type == GDA_TYPE_QUERY_FIELD_VALUE) ||
	    (type == GDA_TYPE_QUERY_FIELD_FUNC))
		retval = GDA_TYPE_QUERY_FIELD;

	return retval;
}

/**
 * gda_object_ref_set_ref_name
 * @ref: a #GdaObjectRef object
 * @ref_type: the requested referenced object's data type, or 0 if not specified and @type = REFERENCE_BY_XML_ID
 * @type: how to interpret the @name argument
 * @name: the name of the requested object
 *
 * Sets the type and XML Id of the object we want to reference. If any other object was already
 * referenced @ref is first reinitialized
 *
 * Rem: the name format is dependant on the type of object which is requested
 */
void
gda_object_ref_set_ref_name (GdaObjectRef *ref, GType ref_type, GdaObjectRefType type, const gchar *name)
{
	g_return_if_fail (GDA_IS_OBJECT_REF (ref));
	g_return_if_fail (ref->priv);
	g_return_if_fail (name && *name);

	/* make sure we know how to retreive the requested object */
	if (!ref_type && (type == REFERENCE_BY_XML_ID) && (strlen (name) > 2)) {
		gchar *str = g_strdup (name), *ptr, *tok;
		gboolean has_sep = FALSE; /* TRUE if there is a ":" in the string */

		ptr = strtok_r (str, ":", &tok);
		ptr = strtok_r (NULL, ":", &tok);
		if (!ptr)
			ptr = str;
		else
			has_sep = TRUE;
		if ((strlen (ptr) > 2) || has_sep) {
			if ((*ptr == 'T') && (*(ptr+1) == 'V'))
				ref_type = GDA_TYPE_DICT_TABLE;
			if ((*ptr == 'F') && (*(ptr+1) == 'I'))
				ref_type = GDA_TYPE_DICT_FIELD;
			if ((*ptr == 'D') && (*(ptr+1) == 'T'))
				ref_type = GDA_TYPE_DICT_TYPE;
			if ((*ptr == 'P') && (*(ptr+1) == 'R'))
				ref_type = GDA_TYPE_DICT_FUNCTION;
			if ((*ptr == 'A') && (*(ptr+1) == 'G'))
				ref_type = GDA_TYPE_DICT_AGGREGATE;
			if ((*ptr == 'Q') && (*(ptr+1) == 'U'))
				ref_type = GDA_TYPE_QUERY;
			if ((*ptr == 'Q') && (*(ptr+1) == 'F'))
				ref_type = GDA_TYPE_QUERY_FIELD;
			if (has_sep && (*ptr == 'T'))
				ref_type = GDA_TYPE_QUERY_TARGET;
			if (has_sep && (*ptr == 'C'))
				ref_type = GDA_TYPE_QUERY_CONDITION;
		}
		g_free (str);
	}
	ref_type = handled_object_type (ref_type);
	g_return_if_fail (ref_type);

	/* Is there anything to change ? */
	if (ref->priv->ref_name && name && !strcmp (ref->priv->ref_name, name) &&
	    (ref_type == ref->priv->requested_type) && (ref->priv->ref_type == type)) {
		gda_object_ref_activate (ref);
		return;
	}
	
	gda_object_ref_deactivate (ref);
	
	ref->priv->ref_type = type;
	if (ref->priv->ref_name) {
		g_free (ref->priv->ref_name);
		ref->priv->ref_name = NULL;
	}
	if (name)
		ref->priv->ref_name = g_strdup (name);
	ref->priv->requested_type = ref_type;

	gda_object_ref_activate (ref);
}

/**
 * gda_object_ref_set_ref_object_type
 * @ref: a #GdaObjectRef object
 * @object: the object to keep a reference to
 * @type: the type of object requested: it must be a type in the class hierarchy of @object
 *
 * Rather than to set the XML Id of the object @ref has to reference, this function allows
 * to directly give the object, and specify the requested type, in case the object is known.
 */
void
gda_object_ref_set_ref_object_type (GdaObjectRef *ref, GdaObject *object, GType type)
{
	g_return_if_fail (GDA_IS_OBJECT_REF (ref));
	g_return_if_fail (ref->priv);
	g_return_if_fail (object && GDA_IS_OBJECT (object));

	/* make sure we know how to retreive the requested object */
	type = handled_object_type (type);
	g_return_if_fail (type);

	gda_object_ref_deactivate (ref);

	ref->priv->ref_type =  REFERENCE_BY_XML_ID;
	if (ref->priv->ref_name) {
		g_free (ref->priv->ref_name);
		ref->priv->ref_name = NULL;
	}
	
	ref->priv->ref_name = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (object));
	ref->priv->requested_type = type;

	/* Object treatment */
	if (ref->priv->increase_ref_object)
		g_object_ref (object);
	gda_object_connect_destroy (object, G_CALLBACK (destroyed_object_cb), ref);
	ref->priv->ref_object = object;

	g_free (ref->priv->obj_name);
	ref->priv->obj_name = g_strdup (gda_object_get_name (object));

	if (! ref->priv->block_signals) {
#ifdef GDA_DEBUG_signal
		g_print (">> 'REF_FOUND' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (ref), gda_object_ref_signals[REF_FOUND], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'REF_FOUND' from %s\n", __FUNCTION__);
#endif
	}
}

/**
 * gda_object_ref_set_ref_object
 * @ref: a #GdaObjectRef object
 * @object: the object to keep a reference to
 *
 * Rather than to set the XML Id of the object @ref has to reference, this function allows
 * to directly give the object, in case the object is known.
 */
void
gda_object_ref_set_ref_object (GdaObjectRef *ref, GdaObject *object)
{
	GType ref_type;

	g_return_if_fail (object && GDA_IS_OBJECT (object));
	ref_type = G_OBJECT_TYPE (object);
	
	gda_object_ref_set_ref_object_type (ref, object, ref_type);
}

/**
 * gda_object_ref_replace_ref_object
 * @ref: a #GdaObjectRef object
 * @replacements: a #GHashTable
 *
 * Changes the referenced object with a new one: it looks into @replacements and if the
 * currently referenced object appears there as a key, then the reference is replaced with
 * the corresponding value.
 *
 * Nothing happens if @ref is not active, or if the referenced object is not found in @replacaments.
 */
void
gda_object_ref_replace_ref_object (GdaObjectRef *ref, GHashTable *replacements)
{
	g_return_if_fail (GDA_IS_OBJECT_REF (ref));
	g_return_if_fail (ref->priv);

	if (!replacements)
		return;
	
	if (ref->priv->ref_object) {
		GdaObject *repl;
		repl = g_hash_table_lookup (replacements, ref->priv->ref_object);
		if (repl) {
			/* we don't want to send a "ref_dropped" signal here since we are just
			   changing the referenced object without really losing the reference */
			ref->priv->block_signals = TRUE;
			gda_object_ref_set_ref_object_type (ref, repl, ref->priv->requested_type);		
			ref->priv->block_signals = FALSE;
		}
	}
}

/**
 * gda_object_ref_get_ref_name
 * @ref: a #GdaObjectRef object
 * @ref_type: where to store the requested referenced object's data type, or NULL
 * @type: where to store how to interpret the returned name, or NULL
 *
 * Get the caracteristics of the requested object
 *
 * Returns: the name of the object (to be interpreted with @type)
 */
const gchar *
gda_object_ref_get_ref_name (GdaObjectRef *ref, GType *ref_type, GdaObjectRefType *type)
{
	g_return_val_if_fail (GDA_IS_OBJECT_REF (ref), NULL);
	g_return_val_if_fail (ref->priv, NULL);

	if (ref_type)
		*ref_type = ref->priv->requested_type;
	if (type)
		*type = ref->priv->ref_type;

	return ref->priv->ref_name;
}

/**
 * gda_object_ref_get_ref_object_name
 * @ref: a #GdaObjectRef object
 *
 * Get the name (as returned by gda_object_get_name()) of the last object referenced
 *
 * Returns: the name of the object (to be interpreted with @type)
 */
const gchar *
gda_object_ref_get_ref_object_name (GdaObjectRef *ref)
{
	g_return_val_if_fail (GDA_IS_OBJECT_REF (ref), NULL);
	g_return_val_if_fail (ref->priv, NULL);

	return ref->priv->obj_name;
}

/**
 * gda_object_ref_get_ref_type
 * @ref: a #GdaObjectRef object
 *
 * Get the type of the referenced object by @ref (or the requested type if @ref is not active)
 *
 * Returns: the type
 */
GType
gda_object_ref_get_ref_type (GdaObjectRef *ref)
{
	g_return_val_if_fail (GDA_IS_OBJECT_REF (ref), 0);
	g_return_val_if_fail (ref->priv, 0);

	return ref->priv->requested_type;
}


/**
 * gda_object_ref_get_ref_object
 * @ref: a #GdaObjectRef object
 *
 * Get the referenced object by @ref
 *
 * Returns: a pointer to the object, or NULL if the reference is not active
 */
GdaObject *
gda_object_ref_get_ref_object (GdaObjectRef *ref)
{
	g_return_val_if_fail (GDA_IS_OBJECT_REF (ref), NULL);
	g_return_val_if_fail (ref->priv, NULL);

	if (!ref->priv->ref_object)
		gda_object_ref_activate (ref);

	return ref->priv->ref_object;
}

/**
 * gda_object_ref_activate
 * @ref: a #GdaObjectRef object
 *
 * Tries to "activate" a reference (to find the referenced object). Nothing happens if
 * the object is already activated
 *
 * Returns: TRUE on success
 */
gboolean
gda_object_ref_activate (GdaObjectRef *ref)
{
	GdaObject *obj = NULL;
	gboolean done = FALSE;

	g_return_val_if_fail (GDA_IS_OBJECT_REF (ref), FALSE);
	g_return_val_if_fail (ref->priv, FALSE);

	if (!ref->priv->ref_name)
		/* no object reference set, so we consider ourselve active */
		return TRUE;

	if (ref->priv->ref_object)
		return TRUE;

	/* Find the object */
	/* TABLE */
	if (!done && (ref->priv->requested_type == GDA_TYPE_DICT_TABLE)) {
		GdaDictDatabase *db;
		GdaDictTable *table;

		done = TRUE;
		db = gda_dict_get_database (gda_object_get_dict (GDA_OBJECT (ref)));
		if (ref->priv->ref_type == REFERENCE_BY_XML_ID)
			table = gda_dict_database_get_table_by_xml_id (db, ref->priv->ref_name);
		else
			table = gda_dict_database_get_table_by_name (db, ref->priv->ref_name);

		if (table)
			obj = GDA_OBJECT (table);
	}

	/* TABLE's FIELD */
	if (!done && (ref->priv->requested_type == GDA_TYPE_DICT_FIELD)) {
		GdaDictDatabase *db;
		GdaDictField *field;

		done = TRUE;
		db = gda_dict_get_database (gda_object_get_dict (GDA_OBJECT (ref)));
		if (ref->priv->ref_type == REFERENCE_BY_XML_ID)
			field = gda_dict_database_get_field_by_xml_id (db, ref->priv->ref_name);
		else
			field = gda_dict_database_get_field_by_name (db, ref->priv->ref_name);

		if (field)
			obj = GDA_OBJECT (field);
	}

	/* QUERY */
	if (!done && (ref->priv->requested_type == GDA_TYPE_QUERY)) {
		GdaQuery *query = NULL;

		done = TRUE;
		if (ref->priv->ref_type == REFERENCE_BY_XML_ID)
			query = GDA_QUERY (gda_dict_get_object_by_xml_id (gda_object_get_dict (GDA_OBJECT (ref)), 
							       GDA_TYPE_QUERY,
							       ref->priv->ref_name));
		else
			TO_IMPLEMENT; /* not really needed, anyway */
		if (query)
			obj = GDA_OBJECT (query);
	}

	/* QUERY's FIELD */
	if (!done && (ref->priv->requested_type == GDA_TYPE_QUERY_FIELD)) {
		GdaQuery *query;
		gchar *str, *ptr, *tok;

		str = g_strdup (ref->priv->ref_name);
		ptr = strtok_r (str, ":", &tok);
		
		done = TRUE;
		query = GDA_QUERY (gda_dict_get_object_by_xml_id (gda_object_get_dict (GDA_OBJECT (ref)), 
						       GDA_TYPE_QUERY, ptr));
		
		if (query) {
			GdaEntityField *field;
			
			field = gda_entity_get_field_by_xml_id (GDA_ENTITY (query), ref->priv->ref_name);
			if (field)
				obj = GDA_OBJECT (field);
		}
	}


	/* TARGET */
	if (!done && (ref->priv->requested_type == GDA_TYPE_QUERY_TARGET)) {
		done = TRUE;

		if (ref->priv->ref_type == REFERENCE_BY_XML_ID) {
			gchar *str, *ptr, *tok;
			GdaQuery *query;
			
			str = g_strdup (ref->priv->ref_name);
			ptr = strtok_r (str, ":", &tok);
			query = GDA_QUERY (gda_dict_get_object_by_xml_id (gda_object_get_dict (GDA_OBJECT (ref)), 
							       GDA_TYPE_QUERY, ptr));
			g_free (str);
			if (query) {
				GdaQueryTarget *target;
				
				target = gda_query_get_target_by_xml_id (query, ref->priv->ref_name);
				if (target)
					obj = GDA_OBJECT (target);
			}
		}
		else {
			done = TRUE;
			if (ref->priv->helper_ref) {
				GdaQueryTarget *target = NULL;
				if (! GDA_IS_QUERY (ref->priv->helper_ref))
					g_warning ("GdaObjectRef helper object must be a GdaQuery for target resolution");
				else
					target = (GdaQueryTarget *) gda_query_get_target_by_alias (GDA_QUERY (ref->priv->helper_ref), 
												   ref->priv->ref_name);

				if (target)
					obj = GDA_OBJECT (target);
			}
			else
				g_warning ("Can't find a target reference, specify a query usign the \"helper_ref\" property");
		}
	}
	
	/* Generic FIELD (GdaEntityField interface)*/
	if (!done && (ref->priv->requested_type == GDA_TYPE_ENTITY_FIELD)) {
		if (ref->priv->ref_type == REFERENCE_BY_XML_ID) {
			gchar *str, *ptr, *tok;
			str = g_strdup (ref->priv->ref_name);
			ptr = strtok_r (str, ":", &tok);
			if ((*ptr == 'T') && (*(ptr+1) == 'V')) {
				/* we are really looking for a table's field */
				GdaDictDatabase *db;
				GdaDictField *field;
				
				done = TRUE;
				
				db = gda_dict_get_database (gda_object_get_dict (GDA_OBJECT (ref)));
				field = gda_dict_database_get_field_by_xml_id (db, ref->priv->ref_name);
				
				if (field)
					obj = GDA_OBJECT (field);
			}
			
			if (!done && (*ptr == 'Q') && (*(ptr+1) == 'U')) {
				/* we are really looking for a query's field */
				GdaQuery *query;
				
				done = TRUE;
				query = GDA_QUERY (gda_dict_get_object_by_xml_id (gda_object_get_dict (GDA_OBJECT (ref)), 
								       GDA_TYPE_QUERY, ptr));
				
				if (query) {
					GdaEntityField *field;
					
					field = gda_entity_get_field_by_xml_id (GDA_ENTITY (query), ref->priv->ref_name);
					if (field)
						obj = GDA_OBJECT (field);
				}
			}
			g_free (str);		
		}
		else {
			if (ref->priv->helper_ref) {
				GdaQueryTarget *target = NULL;
				if (! GDA_IS_OBJECT_REF (ref->priv->helper_ref))
					g_warning ("GdaObjectRef helper object must be a GdaObjectRef representing a "
						   "target for field resolution");
				else
					target = (GdaQueryTarget*) gda_object_ref_get_ref_object (GDA_OBJECT_REF (ref->priv->helper_ref));

				if (target) {
					GdaEntityField *tmp = NULL;
					GdaEntity *ent;

					ent = gda_query_target_get_represented_entity (target);
					if (ent)
						tmp = gda_entity_get_field_by_name (ent, ref->priv->ref_name);
					if (tmp)
						obj = GDA_OBJECT (tmp);
				}
			}
			else
				g_warning ("Can't find a field reference, specify a query usign the \"helper_ref\" property");
			
			done = TRUE;
		}
	}

	/* Dict function */
	if (!done && (ref->priv->requested_type == GDA_TYPE_DICT_FUNCTION)) {
		GdaDictFunction *func = NULL;

		done = TRUE;
		if (ref->priv->ref_type == REFERENCE_BY_XML_ID)
			func = GDA_DICT_FUNCTION (gda_dict_get_function_by_xml_id (gda_object_get_dict (GDA_OBJECT (ref)), 
								ref->priv->ref_name));
		else {
			/* Find the list of functions by its name, and if there is more than one, 
			 * then we don't make a choice, and say that we did not find it because
			 * we don't have the arguments' types information */
			GSList *list;

			list = gda_dict_get_functions_by_name (gda_object_get_dict (GDA_OBJECT (ref)), 
								ref->priv->ref_name);
			if (g_slist_length (list) ==  1) 
				func = GDA_DICT_FUNCTION (list->data);
			g_slist_free (list);
		}
		if (func)
			obj = GDA_OBJECT (func);
	}

	/* Dict aggregate */
	if (!done && (ref->priv->requested_type == GDA_TYPE_DICT_AGGREGATE)) {
		GdaDictAggregate *agg = NULL;

		done = TRUE;
		if (ref->priv->ref_type == REFERENCE_BY_XML_ID)
			agg = GDA_DICT_AGGREGATE (gda_dict_get_aggregate_by_xml_id (gda_object_get_dict (GDA_OBJECT (ref)), 
								ref->priv->ref_name));
		else {
			/* Find the list of functions by its name, and if there is more than one, 
			 * then we don't make a choice, and say that we did not find it because
			 * we don't have the arguments' types information */
			GSList *list;

			list = gda_dict_get_aggregates_by_name (gda_object_get_dict (GDA_OBJECT (ref)), 
								ref->priv->ref_name);
			if (g_slist_length (list) ==  1) 
				agg = GDA_DICT_AGGREGATE (list->data);
			g_slist_free (list);
		}
		if (agg)
			obj = GDA_OBJECT (agg);
	}

	/* Object treatment */
	if (obj) {
		if (ref->priv->increase_ref_object)
			g_object_ref (obj);

		gda_object_connect_destroy (obj, G_CALLBACK (destroyed_object_cb), ref);
		ref->priv->ref_object = obj;

		g_free (ref->priv->obj_name);
		ref->priv->obj_name = g_strdup (gda_object_get_name (obj));

#ifdef GDA_DEBUG_signal
		g_print (">> 'REF_FOUND' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (ref), gda_object_ref_signals[REF_FOUND], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'REF_FOUND' from %s\n", __FUNCTION__);
#endif
	}

	return ref->priv->ref_object ? TRUE : FALSE;
}

/**
 * gda_object_ref_deactivate
 * @ref: a #GdaObjectRef object
 *
 * Desctivates the object (loses the reference to the object)
 */
void
gda_object_ref_deactivate (GdaObjectRef *ref)
{
	g_return_if_fail (GDA_IS_OBJECT_REF (ref));
	g_return_if_fail (ref->priv);

	if (!ref->priv->ref_name)
		return;
	
	if (! ref->priv->ref_object)
		return;

	g_signal_handlers_disconnect_by_func (G_OBJECT (ref->priv->ref_object),
					      G_CALLBACK (destroyed_object_cb), ref);
	if (ref->priv->increase_ref_object)
		g_object_unref (ref->priv->ref_object);

	ref->priv->ref_object = NULL;

	if (! ref->priv->block_signals) {
#ifdef GDA_DEBUG_signal
		g_print (">> 'REF_LOST' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (ref), gda_object_ref_signals[REF_LOST], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'REF_LOST' from %s\n", __FUNCTION__);
#endif
	}
}

/**
 * gda_object_ref_is_active
 * @ref: a #GdaObjectRef object
 *
 * Find wether @ref is active
 *
 * Returns: TRUE if @ref is active
 */
gboolean
gda_object_ref_is_active (GdaObjectRef *ref)
{
	g_return_val_if_fail (GDA_IS_OBJECT_REF (ref), FALSE);
	g_return_val_if_fail (ref->priv, FALSE);
	
	if (!ref->priv->ref_name)
		/* no object reference set, so we consider ourselve active */
		return TRUE;

	return ref->priv->ref_object ? TRUE : FALSE;
}


#ifdef GDA_DEBUG
static void
gda_object_ref_dump (GdaObjectRef *ref, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (GDA_IS_OBJECT_REF (ref));
	g_return_if_fail (ref->priv);

	/* string for the offset */
	str = g_new0 (gchar, offset+1);
	for (i=0; i<offset; i++)
		str[i] = ' ';
	str[offset] = 0;

	if (ref->priv->ref_object) {
		g_print ("%s" D_COL_H1 "GdaObjectRef" D_COL_NOR " Active, points to id='%s': %p\n", str, 
			 ref->priv->ref_name, ref->priv->ref_object);
		/*gda_object_dump (GDA_OBJECT (ref->priv->ref_object), offset);*/
	}
	else 
		g_print ("%s" D_COL_ERR "BaseRef to id '%s' not active\n" D_COL_NOR, str, ref->priv->ref_name);
}
#endif

