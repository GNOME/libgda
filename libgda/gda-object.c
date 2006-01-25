/* gda-object.c
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
#include <libgda/gda-object.h>
#include "gda-dict.h"
#include "gda-marshal.h"
#include <glib/gi18n-lib.h>

extern GdaDict *default_dict;

/* 
 * Main static functions 
 */
static void gda_object_class_init (GdaObjectClass * class);
static void gda_object_init (GdaObject * srv);
static void gda_object_dispose (GObject   * object);
static void gda_object_finalize (GObject   * object);

static void gda_object_set_property    (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_object_get_property    (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	CHANGED,
	ID_CHANGED,
	NAME_CHANGED,
	DESCR_CHANGED,
	OWNER_CHANGED,
	TO_BE_DESTROYED,
	DESTROYED,
	LAST_SIGNAL
};

static gint gda_object_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_DICT,
	PROP_BLOCK_CHANGED,
	PROP_ID_STRING
};


struct _GdaObjectPrivate
{
	GdaDict           *dict;     /* property: NOT NULL to use GdaObject object */

	gchar             *name;
	gchar             *descr;
	gchar             *owner;
	gchar             *id_string;
	
	gboolean           destroyed;/* TRUE if the object has been "destroyed" */
	gboolean           changed_locked;
};

GType
gda_object_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaObjectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_object_class_init,
			NULL,
			NULL,
			sizeof (GdaObject),
			0,
			(GInstanceInitFunc) gda_object_init
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GdaObject", &info, 0);
	}
	return type;
}

static void
gda_object_class_init (GdaObjectClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_object_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaObjectClass, changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	gda_object_signals[NAME_CHANGED] =
		g_signal_new ("name_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaObjectClass, name_changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	gda_object_signals[ID_CHANGED] =
		g_signal_new ("id_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaObjectClass, id_changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	gda_object_signals[DESCR_CHANGED] =
		g_signal_new ("descr_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaObjectClass, descr_changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	gda_object_signals[OWNER_CHANGED] =
		g_signal_new ("owner_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaObjectClass, owner_changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	gda_object_signals[DESTROYED] =
		g_signal_new ("destroyed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaObjectClass, destroyed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	gda_object_signals[TO_BE_DESTROYED] =
		g_signal_new ("to_be_destroyed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaObjectClass, to_be_destroyed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	class->changed = NULL;
	class->name_changed = gda_object_changed;
	class->id_changed = gda_object_changed;
	class->descr_changed = gda_object_changed;
	class->owner_changed = gda_object_changed;
	class->destroyed = NULL;
	class->to_be_destroyed = NULL;

	/* virtual functions */
	class->signal_changed = NULL;
#ifdef GDA_DEBUG
	class->dump = NULL;
#endif

	object_class->dispose = gda_object_dispose;
	object_class->finalize = gda_object_finalize;

	/* class attributes */
	class->id_unique_enforced = TRUE;

	/* Properties */
	object_class->set_property = gda_object_set_property;
	object_class->get_property = gda_object_get_property;
	g_object_class_install_property (object_class, PROP_DICT,
					 g_param_spec_pointer ("dict", "GdaDict", 
							       "Dictionary to which the object is related", 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE | 
								G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_BLOCK_CHANGED,
                                         g_param_spec_boolean ("changed_blocked", "Block changed signal", NULL, FALSE,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_ID_STRING,
                                         g_param_spec_string ("string_id", "Identifier string", NULL, NULL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gda_object_init (GdaObject *gdaobj)
{
	gdaobj->priv = g_new0 (GdaObjectPrivate, 1);
	gdaobj->priv->dict = NULL;
	gdaobj->priv->destroyed = FALSE;

	gdaobj->priv->name = NULL;
	gdaobj->priv->descr = NULL;
	gdaobj->priv->owner = NULL;
	gdaobj->priv->id_string = NULL;

	gdaobj->priv->changed_locked = FALSE;
}

static void
gda_object_dispose (GObject *object)
{
	GdaObject *gdaobj;

	g_return_if_fail (GDA_IS_OBJECT (object));

	gdaobj = GDA_OBJECT (object);
	if (gdaobj->priv) {
		if (! gdaobj->priv->destroyed)
			gda_object_destroy (gdaobj);

		if (gdaobj->priv->dict) {
			GdaObjectClass *real_class;

			real_class = (GdaObjectClass *) G_OBJECT_GET_CLASS (gdaobj);
			if (real_class->id_unique_enforced && gdaobj->priv->id_string) {
				gchar *oldid = gdaobj->priv->id_string;

				gdaobj->priv->id_string = NULL;
				gda_dict_declare_object_string_id_change (gdaobj->priv->dict, gdaobj, oldid);
				g_free (oldid);
			}
			g_object_remove_weak_pointer (G_OBJECT (gdaobj->priv->dict),
						      (gpointer) & (gdaobj->priv->dict));
			gdaobj->priv->dict = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_object_finalize (GObject   * object)
{
	GdaObject *gdaobj;

	g_return_if_fail (GDA_IS_OBJECT (object));

	gdaobj = GDA_OBJECT (object);
	if (gdaobj->priv) {
		if (! gdaobj->priv->destroyed) {
			g_warning ("GdaObject::finalize(%p) not destroyed!\n", gdaobj);
		}

		if (gdaobj->priv->name)
			g_free (gdaobj->priv->name);
		if (gdaobj->priv->descr)
			g_free (gdaobj->priv->descr);
		if (gdaobj->priv->owner)
			g_free (gdaobj->priv->owner);
		if (gdaobj->priv->id_string)
			g_free (gdaobj->priv->id_string);

		g_free (gdaobj->priv);
		gdaobj->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_object_set_property (GObject *object,
			 guint param_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	gpointer ptr;
	GdaObject *gdaobj;

	gdaobj = GDA_OBJECT (object);
	if (gdaobj->priv) {
		switch (param_id) {
		case PROP_DICT:
			ptr = g_value_get_pointer (value);
			gdaobj->priv->dict = ASSERT_DICT (ptr);
			if (!gdaobj->priv->dict)
				g_error (_("LibGda must be initialized before any usage."));

			g_object_add_weak_pointer (G_OBJECT (gdaobj->priv->dict),
						   (gpointer) & (gdaobj->priv->dict));
			break;
		case PROP_BLOCK_CHANGED:
			if (g_value_get_boolean (value))
				gda_object_block_changed (gdaobj);
			else
				gda_object_unblock_changed (gdaobj);
			break;
		case PROP_ID_STRING:
			gda_object_set_id (gdaobj, g_value_get_string (value));
			break;
		}
	}
}

static void
gda_object_get_property (GObject *object,
			 guint param_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	GdaObject *gdaobj;

	gdaobj = GDA_OBJECT (object);
	if (gdaobj->priv) {
		switch (param_id) {
		case PROP_DICT:
			g_value_set_pointer (value, gdaobj->priv->dict);
			break;
		case PROP_BLOCK_CHANGED:
			g_value_set_boolean (value, gdaobj->priv->changed_locked);
			break;
		case PROP_ID_STRING:
			g_value_set_string (value, gdaobj->priv->id_string);
			break;
		}
	}
}

/**
 * gda_object_get_dict
 * @gdaobj: a #GdaObject object
 *
 * Fetch the corresponding #GdaDict object.
 *
 * Returns: the #GdaDict object to which @gdaobj is attached to
 */ 
GdaDict *
gda_object_get_dict (GdaObject *gdaobj)
{
	g_return_val_if_fail (GDA_IS_OBJECT (gdaobj), NULL);
	g_return_val_if_fail (gdaobj->priv, NULL);

	return gdaobj->priv->dict;
}


/**
 * gda_object_set_id
 * @gdaobj: a #GdaObject object
 * @strid: the string Identifier
 *
 * Sets the string ID of the @gdaobj object.
 *
 * The string ID must be unique for all the objects related to a given #GdaDict object.
 */
void
gda_object_set_id (GdaObject *gdaobj, const gchar *strid)
{
	GdaObjectClass *real_class;
	gchar *oldid = NULL;

	g_return_if_fail (GDA_IS_OBJECT (gdaobj));
	g_return_if_fail (gdaobj->priv);

	if (!gdaobj->priv->id_string && !strid)
		return;
	if (gdaobj->priv->id_string && strid &&
	    !strcmp (gdaobj->priv->id_string, strid))
		return;

	if (gdaobj->priv->id_string) {
		oldid = gdaobj->priv->id_string;
		gdaobj->priv->id_string = NULL;
	}
	if (strid && *strid)
		gdaobj->priv->id_string = g_strdup (strid);

	real_class = (GdaObjectClass *) G_OBJECT_GET_CLASS (gdaobj);
	if (real_class->id_unique_enforced)
		gda_dict_declare_object_string_id_change (gdaobj->priv->dict, gdaobj, oldid);
	
#ifdef GDA_DEBUG_signal
	g_print (">> 'ID_CHANGED' from %s %p\n", G_OBJECT_TYPE_NAME (gdaobj), gdaobj);
#endif
	g_signal_emit (G_OBJECT (gdaobj), gda_object_signals[ID_CHANGED], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'ID_CHANGED' from %s %p\n", G_OBJECT_TYPE_NAME (gdaobj), gdaobj);
#endif
	g_free (oldid);
}

/**
 * gda_object_set_name
 * @gdaobj: a #GdaObject object
 * @name: 
 *
 * Sets the name of the GdaObject object. If the name is changed, then the 
 * "name_changed" signal is emitted.
 * 
 */
void
gda_object_set_name (GdaObject *gdaobj, const gchar *name)
{
	gboolean changed = FALSE;

	g_return_if_fail (GDA_IS_OBJECT (gdaobj));
	g_return_if_fail (gdaobj->priv);

	if (name) {
		if (gdaobj->priv->name) {
			changed = strcmp (gdaobj->priv->name, name) ? TRUE : FALSE;
			g_free (gdaobj->priv->name);
		}
		else
			changed = TRUE;
		gdaobj->priv->name = g_strdup (name);
	}

	if (changed) {
#ifdef GDA_DEBUG_signal
		g_print (">> 'NAME_CHANGED' from %s %p\n", G_OBJECT_TYPE_NAME (gdaobj), gdaobj);
#endif
		g_signal_emit (G_OBJECT (gdaobj), gda_object_signals[NAME_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'NAME_CHANGED' from %s %p\n", G_OBJECT_TYPE_NAME (gdaobj), gdaobj);
#endif
	}
}


/**
 * gda_object_set_description
 * @gdaobj: a #GdaObject object
 * @descr: 
 *
 * Sets the description of the GdaObject object. If the description is changed, then the 
 * "descr_changed" signal is emitted.
 * 
 */
void
gda_object_set_description (GdaObject *gdaobj, const gchar *descr)
{
	gboolean changed = FALSE;

	g_return_if_fail (GDA_IS_OBJECT (gdaobj));
	g_return_if_fail (gdaobj->priv);

	if (descr) {
		if (gdaobj->priv->descr) {
			changed = strcmp (gdaobj->priv->descr, descr) ? TRUE : FALSE;
			g_free (gdaobj->priv->descr);
		}
		else
			changed = TRUE;
		gdaobj->priv->descr = g_strdup (descr);
	}

	if (changed) {
#ifdef GDA_DEBUG_signal
		g_print (">> 'DESCR_CHANGED' from gda_object_set_descr (%s)\n", gdaobj->priv->descr);
#endif
		g_signal_emit (G_OBJECT (gdaobj), gda_object_signals[DESCR_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'DESCR_CHANGED' from gda_object_set_descr (%s)\n", gdaobj->priv->descr);
#endif
	}
}


/**
 * gda_object_set_owner
 * @gdaobj: a #GdaObject object
 * @owner: 
 *
 * Sets the owner of the GdaObject object. If the owner is changed, then the 
 * "owner_changed" signal is emitted.
 * 
 */
void
gda_object_set_owner (GdaObject *gdaobj, const gchar *owner)
{
	gboolean changed = FALSE;

	g_return_if_fail (GDA_IS_OBJECT (gdaobj));
	g_return_if_fail (gdaobj->priv);

	if (owner) {
		if (gdaobj->priv->owner) {
			changed = strcmp (gdaobj->priv->owner, owner) ? TRUE : FALSE;
			g_free (gdaobj->priv->owner);
		}
		else
			changed = TRUE;
		gdaobj->priv->owner = g_strdup (owner);
	}

	if (changed) {
#ifdef GDA_DEBUG_signal
		g_print (">> 'OWNER_CHANGED' from gda_object_set_descr (%s)\n", gdaobj->priv->descr);
#endif
		g_signal_emit (G_OBJECT (gdaobj), gda_object_signals[OWNER_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'OWNER_CHANGED' from gda_object_set_descr (%s)\n", gdaobj->priv->descr);
#endif
	}
}

/**
 * gda_object_get_id
 * @gdaobj: a #GdaObject object
 * 
 * Fetch the string ID of the GdaObject object.
 * 
 * Returns: the id.
 */
const gchar *
gda_object_get_id (GdaObject *gdaobj)
{
	g_return_val_if_fail (GDA_IS_OBJECT (gdaobj), 0);
	g_return_val_if_fail (gdaobj->priv, 0);
		
	return gdaobj->priv->id_string;
}

/**
 * gda_object_get_name
 * @gdaobj: a #GdaObject object
 * 
 * Fetch the name of the GdaObject object.
 * 
 * Returns: the object's name.
 */
const gchar *
gda_object_get_name (GdaObject *gdaobj)
{
	g_return_val_if_fail (GDA_IS_OBJECT (gdaobj), NULL);
	g_return_val_if_fail (gdaobj->priv, NULL);

	return gdaobj->priv->name;
}


/**
 * gda_object_get_description
 * @gdaobj: a #GdaObject object
 * 
 * Fetch the description of the GdaObject object.
 * 
 * Returns: the object's description.
 */
const gchar *
gda_object_get_description (GdaObject *gdaobj)
{
	g_return_val_if_fail (GDA_IS_OBJECT (gdaobj), NULL);
	g_return_val_if_fail (gdaobj->priv, NULL);

	return gdaobj->priv->descr;
}

/**
 * gda_object_get_owner
 * @gdaobj: a #GdaObject object
 * 
 * Fetch the owner of the GdaObject object.
 * 
 * Returns: the object's owner.
 */
const gchar *
gda_object_get_owner (GdaObject *gdaobj)
{
	g_return_val_if_fail (GDA_IS_OBJECT (gdaobj), NULL);
	g_return_val_if_fail (gdaobj->priv, NULL);

	return gdaobj->priv->owner;
}



/**
 * gda_object_destroy
 * @gdaobj: a #GdaObject object
 *
 * Force the @gdaobj object to be destroyed, even if we don't have a reference on it
 * (we can't call g_object_unref() then) and even if the object is referenced
 * multiple times by other objects.
 *
 * The "destroyed" signal is then emitted to tell the other reference holders that the object
 * must be destroyed and that they should give back their reference (using g_object_unref()).
 * However if a reference is still present, the object will not actually be destroyed and will
 * still be alive, but its state may not be deterministic.
 *
 * This is usefull because sometimes objects need to disappear even if they are referenced by other
 * objects. This usage is the same as with the gtk_widget_destroy() function on widgets.
 *
 * When this method returns, @gdaobj should have been destroyed and should not be used anymore.
 */
void
gda_object_destroy (GdaObject *gdaobj)
{
	g_return_if_fail (GDA_IS_OBJECT (gdaobj));
	g_return_if_fail (gdaobj->priv);

	if (!gdaobj->priv->destroyed) {
		GdaObjectClass *class;
		class = GDA_OBJECT_CLASS (G_OBJECT_GET_CLASS (gdaobj));

		/* add a ref count on the object to make sure it's not destroyed before we emit all the
		 * signals */
		g_object_ref (gdaobj);

#ifdef GDA_DEBUG_signal
		g_print (">> 'TO_BE_DESTROYED' from %s %p\n", G_OBJECT_TYPE_NAME (gdaobj), gdaobj);
#endif
		g_signal_emit (G_OBJECT (gdaobj), gda_object_signals[TO_BE_DESTROYED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TO_BE_DESTROYED' from %p\n", gdaobj);
#endif
		gdaobj->priv->destroyed = TRUE;
#ifdef GDA_DEBUG_signal
		g_print (">> 'DESTROYED' from %s %p\n", G_OBJECT_TYPE_NAME (gdaobj), gdaobj);
#endif
		g_signal_emit (G_OBJECT (gdaobj), gda_object_signals[DESTROYED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'DESTROYED' from %p\n", gdaobj);
#endif

		/* the object may now be actually destroyed */
		g_object_unref (gdaobj);
	}
	else 
		g_warning ("GdaObject::destroy called on already destroyed object %p, "
			   "of type %s (refcount=%d)\n", gdaobj, G_OBJECT_TYPE_NAME (gdaobj), G_OBJECT (gdaobj)->ref_count);
}

/**
 * gda_object_destroy_check
 * @gdaobj: a #GdaObject object
 *
 * Checks that the object has been destroyed, and if not, then calls gda_object_destroy() on it.
 * This is usefull for objects inheriting the #GdaObject object to be called first line in their
 * dispose() method.
 */
void
gda_object_destroy_check (GdaObject *gdaobj)
{
	g_return_if_fail (GDA_IS_OBJECT (gdaobj));

	if (gdaobj->priv && !gdaobj->priv->destroyed)
		gda_object_destroy (gdaobj);
}

/**
 * gda_object_connect_destroy
 * @gdaobj: a #GdaObject object
 * 
 * Connects the "destoy" signal of the @gdaobj object but first cheks that @gdaobj
 * exists and has not yet been destroyed.
 *
 * Returns: the handler id of the signal
 */
gulong
gda_object_connect_destroy (gpointer gdaobj, GCallback callback, gpointer data)
{
	g_return_val_if_fail (GDA_IS_OBJECT (gdaobj), 0);
	g_return_val_if_fail (((GdaObject*)gdaobj)->priv, 0);
	g_return_val_if_fail (! ((GdaObject*)gdaobj)->priv->destroyed, 0);

	return g_signal_connect (gdaobj, "destroyed", callback, data);
}

/**
 * gda_object_changed
 * @gdaobj: a #GdaObject object
 *
 * Force emission of the "changed" signal, except if gda_object_block_changed() has
 * been called.
 */
void
gda_object_changed (GdaObject *gdaobj)
{
	g_return_if_fail (GDA_IS_OBJECT (gdaobj));
	g_return_if_fail (gdaobj->priv);

	if (gdaobj->priv->changed_locked)
		return;

#ifdef GDA_DEBUG_signal
	g_print (">> 'CHANGED' from %s %p\n", G_OBJECT_TYPE_NAME (gdaobj), gdaobj);
#endif
	g_signal_emit (G_OBJECT (gdaobj), gda_object_signals[CHANGED], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'CHANGED' from %s %p\n", G_OBJECT_TYPE_NAME (gdaobj), gdaobj);
#endif
}

/**
 * gda_object_block_changed
 * @gdaobj: a #GdaObject object
 *
 * No "changed" signal will be emitted.
 */
void
gda_object_block_changed (GdaObject *gdaobj)
{
	GdaObjectClass *class;

	g_return_if_fail (GDA_IS_OBJECT (gdaobj));
	g_return_if_fail (gdaobj->priv);

	gdaobj->priv->changed_locked = TRUE;

	class = GDA_OBJECT_CLASS (G_OBJECT_GET_CLASS (gdaobj));
	if (class->signal_changed)
		(*class->signal_changed) (gdaobj, TRUE);
}

/**
 * gda_object_unblock_changed
 * @gdaobj: a #GdaObject object
 *
 * The "changed" signal will again be emitted.
 */
void
gda_object_unblock_changed (GdaObject *gdaobj)
{
	GdaObjectClass *class;

	g_return_if_fail (GDA_IS_OBJECT (gdaobj));
	g_return_if_fail (gdaobj->priv);

	gdaobj->priv->changed_locked = FALSE;

	class = GDA_OBJECT_CLASS (G_OBJECT_GET_CLASS (gdaobj));
	if (class->signal_changed)
		(*class->signal_changed) (gdaobj, FALSE);
}

/**
 * gda_object_dump
 * @gdaobj: a #GdaObject object
 * @offset: the offset (in caracters) at which the dump will start
 *
 * Writes a textual description of the object to STDOUT. This function only
 * exists if libergeant is compiled with the "--enable-debug" option. This is
 * a virtual function.
 */
void
gda_object_dump (GdaObject *gdaobj, guint offset)
{
#ifdef GDA_DEBUG
	gchar *str;
	guint i;

	g_return_if_fail (gdaobj);
	g_return_if_fail (GDA_IS_OBJECT (gdaobj));

	/* string for the offset */
	str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;
	
	/* dump */
	if (gdaobj->priv) {
		GdaObjectClass *class;
		class = GDA_OBJECT_CLASS (G_OBJECT_GET_CLASS (gdaobj));
		if (class->dump)
			(class->dump) (gdaobj, offset);
		else 
			g_print ("%s" D_COL_H1 "%s %p (name=%s, id=%s)\n" D_COL_NOR, 
				 str, G_OBJECT_TYPE_NAME (gdaobj), gdaobj, gdaobj->priv->name,
				 gdaobj->priv->id_string);
	}
	else
		g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, gdaobj);

	g_free (str);
#endif
}

