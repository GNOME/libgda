/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2012 Daniel Espinosa <despinosa@src.gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <gda-attributes-manager.h>
#include <gda-value.h>

/*
 * Structure for the attribute names
 */
typedef struct {
	GdaAttributesManager *mgr;
	gchar                *att_name;
	GDestroyNotify        att_name_destroy;
} AttName;
static guint attname_hash (gconstpointer key);
static gboolean attname_equal (gconstpointer key1, gconstpointer key2);
static void attname_free (AttName *key);

/*
 * Structure which contains a GHashTable for each object/pointer the attributes apply to
 */
typedef struct {
	GdaAttributesManager *mgr;
	GSList               *objects; /* list of GPointers/Gobjects to which attributes apply */
	GHashTable           *values_hash; /* key = a AttName ptr, value = a GValue */
} ObjAttrs;
static void objattrs_unref (ObjAttrs *attrs);


struct _GdaAttributesManager {
	GRecMutex                   mutex;
	gboolean                    for_objects; /* TRUE if key->data are GObjects */
	GdaAttributesManagerSignal  signal_func;
	gpointer                    signal_data;
	GHashTable                 *obj_hash; /* key = a gpointer to which attributes apply, value = a ObjAttrs ptr */
};

static guint
attname_hash (gconstpointer key)
{
	return g_str_hash (((AttName*) key)->att_name);
}

static gboolean
attname_equal (gconstpointer key1, gconstpointer key2)
{
	if (!strcmp (((AttName*) key1)->att_name, ((AttName*) key2)->att_name))
		return TRUE;
	else
		return FALSE;
}

static void
attname_free (AttName *key)
{
	if (key->att_name_destroy)
		key->att_name_destroy (key->att_name);

	g_free (key);
}

static void
objattrs_unref (ObjAttrs *attrs)
{
	if (!attrs->objects) {
		g_hash_table_destroy (attrs->values_hash);
		g_free (attrs);
	}
}

/**
 * gda_attributes_manager_new:
 * @for_objects: set to TRUE if attributes will be set on objects.
 * @signal_func: (allow-none) (scope call): a function to be called whenever an attribute changes on an object (if @for_objects is TRUE), or %NULL
 * @signal_data: user data passed as last argument of @signal_func when it is called
 *
 * Creates a new #GdaAttributesManager, which can store (name, value) attributes for pointers or GObject objects
 * (in the latter case, the attributes are destroyed when objects are also destroyed).
 *
 * Free-function: gda_attributes_manager_free
 * Returns: the new #GdaAttributesManager
 */
GdaAttributesManager *
gda_attributes_manager_new (gboolean for_objects, GdaAttributesManagerSignal signal_func, gpointer signal_data)
{
	GdaAttributesManager *mgr;

	mgr = g_new0 (GdaAttributesManager, 1);
	g_rec_mutex_init (& (mgr->mutex));
	mgr->obj_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
					       (GDestroyNotify) objattrs_unref);
	mgr->for_objects = for_objects;
	mgr->signal_func = signal_func;
	mgr->signal_data = signal_data;

	return mgr;
}

static void
obj_destroyed_cb (ObjAttrs *attrs, GObject *where_the_object_was)
{
	GRecMutex *mutex;

	/* rem: we need to keep a pointer to mutex because attrs may be destroyed
	 * in g_hash_table_remove */
	mutex = & (attrs->mgr->mutex);

	g_rec_mutex_lock (mutex);
	attrs->objects = g_slist_remove (attrs->objects, where_the_object_was);
	g_hash_table_remove (attrs->mgr->obj_hash, where_the_object_was);
	g_rec_mutex_unlock (mutex);
}

static void
foreach_destroy_func (gpointer ptr, ObjAttrs *attrs, GdaAttributesManager *mgr)
{
	if (mgr->for_objects)
		g_object_weak_unref (G_OBJECT (ptr), 
				     (GWeakNotify) obj_destroyed_cb, attrs);
	attrs->objects = g_slist_remove (attrs->objects, ptr);
}

/**
 * gda_attributes_manager_free:
 * @mgr: a #GdaAttributesManager
 *
 * Frees all the resssources managed by @mgr
 */
void
gda_attributes_manager_free (GdaAttributesManager *mgr)
{
	GRecMutex *mutex;
	mutex = &(mgr->mutex);
	g_rec_mutex_lock (mutex);
	g_hash_table_foreach (mgr->obj_hash, (GHFunc) foreach_destroy_func, mgr);
	g_hash_table_destroy (mgr->obj_hash);
	g_free (mgr);
	g_rec_mutex_unlock (mutex);
	g_rec_mutex_clear (mutex);
}

typedef struct {
	GdaAttributesManager *to_mgr;
	gpointer              ptr;
	
} CopyData;
static void foreach_copy_func (AttName *attname, const GValue *value, CopyData *cdata);

static void
manager_real_set (GdaAttributesManager *mgr, gpointer ptr, 
		  const gchar *att_name, GDestroyNotify destroy, 
		  const GValue *value, gboolean steal_value)
{
	ObjAttrs *objattrs;

	g_return_if_fail (att_name);
	if (mgr->for_objects) 
		g_return_if_fail (G_IS_OBJECT (ptr));
	
	g_rec_mutex_lock (& (mgr->mutex));

	/* pick up the correct ObjAttrs */
	objattrs = g_hash_table_lookup (mgr->obj_hash, ptr);
	if (!objattrs) {
		objattrs = g_new0 (ObjAttrs, 1);
		objattrs->mgr = mgr;
		objattrs->objects = g_slist_prepend (NULL, ptr);
		objattrs->values_hash = g_hash_table_new_full (attname_hash, attname_equal, 
							       (GDestroyNotify) attname_free,
							       (GDestroyNotify) gda_value_free);
		g_hash_table_insert (mgr->obj_hash, ptr, objattrs);
		if (mgr->for_objects) 
			g_object_weak_ref (G_OBJECT (ptr), (GWeakNotify) obj_destroyed_cb, objattrs);
	}

	if (objattrs->objects->next) {
		/* create another ObjAttrs specifically for @ptr */
		ObjAttrs *objattrs2;
		objattrs2 = g_new0 (ObjAttrs, 1);
		objattrs2->mgr = mgr;
		objattrs2->objects = g_slist_prepend (NULL, ptr);
		objattrs2->values_hash = g_hash_table_new_full (attname_hash, attname_equal, 
							       (GDestroyNotify) attname_free, 
								(GDestroyNotify) gda_value_free);

		objattrs->objects = g_slist_remove (objattrs->objects, ptr);
		g_hash_table_remove (mgr->obj_hash, ptr);
		g_hash_table_insert (mgr->obj_hash, ptr, objattrs2);

		if (mgr->for_objects) {
			g_object_weak_unref (G_OBJECT (ptr), (GWeakNotify) obj_destroyed_cb, objattrs);
			g_object_weak_ref (G_OBJECT (ptr), (GWeakNotify) obj_destroyed_cb, objattrs2);
		}

		CopyData cdata;
		cdata.to_mgr = mgr;
		cdata.ptr = ptr;
		g_hash_table_foreach (objattrs->values_hash, (GHFunc) foreach_copy_func, &cdata);

		objattrs = objattrs2;
	}

	/* Actually add the attribute */
	if (value) {
		AttName *attname;

		attname = g_new (AttName, 1);
		attname->mgr = mgr;
		attname->att_name = (gchar*) att_name; /* NOT duplicated */
		attname->att_name_destroy = destroy;
		if (steal_value)
			g_hash_table_insert (objattrs->values_hash, attname, (GValue*) value);
		else
			g_hash_table_insert (objattrs->values_hash, attname, gda_value_copy (value));
	}
	else {
		AttName attname;
		attname.att_name = (gchar*) att_name;
		g_hash_table_remove (objattrs->values_hash, &attname);
	}
	if (mgr->signal_func && mgr->for_objects)
		mgr->signal_func ((GObject*) ptr, att_name, value, mgr->signal_data);

	g_rec_mutex_unlock (& (mgr->mutex));
}

/**
 * gda_attributes_manager_set:
 * @mgr: a #GdaAttributesManager
 * @ptr: a pointer to the resources to which the attribute will apply
 * @att_name: an attribute's name
 * @value: (transfer none) (allow-none): a #GValue, or %NULL
 *
 * Associates an attribute named @att_name to @ptr, with the value @value. Any previous association is replaced by
 * this one, and if @value is %NULL then the association is removed.
 *
 * Note: @att_name is *not* copied, so it should be a string which exists as long as @mgr exists.
 * Libgda provides several predefined names for common attributes,
 * see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 *
 * If @att_name needs to be freed when not used anymore, then use gda_attributes_manager_set_full().
 */
void
gda_attributes_manager_set (GdaAttributesManager *mgr, gpointer ptr, const gchar *att_name, const GValue *value)
{
	manager_real_set (mgr, ptr, att_name, NULL, value, FALSE);
}

/**
 * gda_attributes_manager_set_full:
 * @mgr: a #GdaAttributesManager
 * @ptr: a pointer to the resources to which the attribute will apply
 * @att_name: an attribute's name
 * @value: (allow-none): a #GValue, or %NULL
 * @destroy: function called when @att_name has to be freed
 *
 * Does the same as gda_attributes_manager_set() except that @destroy is called when @att_name needs
 * to be freed.
 */
void
gda_attributes_manager_set_full (GdaAttributesManager *mgr, gpointer ptr,
				 const gchar *att_name, const GValue *value, GDestroyNotify destroy)
{
	manager_real_set (mgr, ptr, att_name, destroy, value, FALSE);
}


/**
 * gda_attributes_manager_get:
 * @mgr: a #GdaAttributesManager
 * @ptr: a pointer to the resources to which the attribute will apply
 * @att_name: an attribute's name
 *
 * Retrieves the value of an attribute previously set using gda_attributes_manager_set().
 *
 * Returns: (transfer none): the attribute's value, or %NULL if the attribute is not set.
 */
const GValue *
gda_attributes_manager_get (GdaAttributesManager *mgr, gpointer ptr, const gchar *att_name)
{
	ObjAttrs *objattrs;
	const GValue *cvalue = NULL;

	g_rec_mutex_lock (& (mgr->mutex));

	objattrs = g_hash_table_lookup (mgr->obj_hash, ptr);
	if (objattrs) {
		AttName attname;
		attname.att_name = (gchar*) att_name;
		cvalue = g_hash_table_lookup (objattrs->values_hash, &attname);
	}

	g_rec_mutex_unlock (& (mgr->mutex));
	return cvalue;
}

/**
 * gda_attributes_manager_copy:
 * @from_mgr: a #GdaAttributesManager
 * @from: a pointer from which attributes are copied
 * @to_mgr: a #GdaAttributesManager
 * @to: a pointer to which attributes are copied
 *
 * For each attribute set for @from (in @from_mgr), set the same attribute to @to (in @to_mgr). @from_mgr and
 * @to_mgr can be equal.
 */
void
gda_attributes_manager_copy (GdaAttributesManager *from_mgr, gpointer *from, 
			     GdaAttributesManager *to_mgr, gpointer *to)
{
	ObjAttrs *from_objattrs, *to_objattrs;

	g_rec_mutex_lock (& (from_mgr->mutex));
	g_rec_mutex_lock (& (to_mgr->mutex));

	from_objattrs = g_hash_table_lookup (from_mgr->obj_hash, from);
	if (!from_objattrs) {
		g_rec_mutex_unlock (& (to_mgr->mutex));
		g_rec_mutex_unlock (& (from_mgr->mutex));
		return;
	}

	to_objattrs = g_hash_table_lookup (to_mgr->obj_hash, to);

	if ((from_mgr == to_mgr) && !to_objattrs) {
		from_objattrs->objects = g_slist_prepend (from_objattrs->objects, to);
		g_hash_table_insert (from_mgr->obj_hash, to, from_objattrs);

		if (from_mgr->for_objects) 
			g_object_weak_ref (G_OBJECT (to), (GWeakNotify) obj_destroyed_cb, from_objattrs);

		g_rec_mutex_unlock (& (to_mgr->mutex));
		g_rec_mutex_unlock (& (from_mgr->mutex));
		return;
	}

	/* copy attributes */
	CopyData cdata;
	cdata.to_mgr = to_mgr;
	cdata.ptr = to;
	g_hash_table_foreach (from_objattrs->values_hash, (GHFunc) foreach_copy_func, &cdata);

	g_rec_mutex_unlock (& (to_mgr->mutex));
	g_rec_mutex_unlock (& (from_mgr->mutex));
}

static GdaAttributesManager*
att_mgr_copy (GdaAttributesManager* src) {
	GdaAttributesManager *cp = g_new0 (GdaAttributesManager, 1);
	g_rec_mutex_init (& (cp->mutex));
	cp->obj_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
					       (GDestroyNotify) objattrs_unref);
	cp->for_objects = src->for_objects;
	cp->signal_func = src->signal_func;
	cp->signal_data = src->signal_data;
	return cp;
}

G_DEFINE_BOXED_TYPE(GdaAttributesManager, gda_attributes_manager, att_mgr_copy, gda_attributes_manager_free)

static void
foreach_copy_func (AttName *attname, const GValue *value, CopyData *cdata)
{
	if (attname->att_name_destroy)
		manager_real_set (cdata->to_mgr, cdata->ptr, g_strdup (attname->att_name), g_free, value, FALSE);
	else
		manager_real_set (cdata->to_mgr, cdata->ptr, attname->att_name, NULL, value, FALSE);
}

/**
 * gda_attributes_manager_clear:
 * @mgr: a #GdaAttributesManager
 * @ptr: a pointer to the resources for which all the attributes will be removed
 *
 * Remove all the attributes managed by @mgr for the @ptr resource.
 */
void
gda_attributes_manager_clear (GdaAttributesManager *mgr, gpointer ptr)
{
	ObjAttrs *objattrs;

	g_rec_mutex_lock (& (mgr->mutex));

	objattrs = g_hash_table_lookup (mgr->obj_hash, ptr);
	if (objattrs) {
		objattrs->objects = g_slist_remove (objattrs->objects, ptr);
		g_hash_table_remove (mgr->obj_hash, ptr);
	}

	g_rec_mutex_unlock (& (mgr->mutex));
}

typedef struct {
	GdaAttributesManagerFunc func;
	gpointer data;
} FData;
static void foreach_foreach_func (AttName *attname, const GValue *value, FData *fdata);

/**
 * gda_attributes_manager_foreach:
 * @mgr: a #GdaAttributesManager
 * @ptr: a pointer to the resources for which all the attributes used
 * @func: (scope call): a #GdaAttributesManagerFunc function
 * @data: (closure): user data to be passed as last argument of @func each time it is called
 *
 * Calls @func for every attribute set to @ptr.
 */
void
gda_attributes_manager_foreach (GdaAttributesManager *mgr, gpointer ptr, 
				GdaAttributesManagerFunc func, gpointer data)
{
	ObjAttrs *objattrs;

	g_return_if_fail (func);
	g_return_if_fail (ptr);

	g_rec_mutex_lock (& (mgr->mutex));

	objattrs = g_hash_table_lookup (mgr->obj_hash, ptr);
	if (objattrs) {
		FData fdata;
		
		fdata.func = func;
		fdata.data = data;
		g_hash_table_foreach (objattrs->values_hash, (GHFunc) foreach_foreach_func, &fdata);
	}

	g_rec_mutex_unlock (& (mgr->mutex));
}

static void
foreach_foreach_func (AttName *attname, const GValue *value, FData *fdata)
{
	fdata->func (attname->att_name, value, fdata->data);
}
