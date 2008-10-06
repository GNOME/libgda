/* 
 * GDA common library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <gda-attributes-manager.h>
#include <gda-value.h>

typedef struct {
	GdaAttributesManager *mgr;
	gpointer              ptr;
	gchar                *att_name;
	GDestroyNotify        att_name_destroy;
} Key;

static guint hash_func (gconstpointer key);
static gboolean equal_func (gconstpointer key1, gconstpointer key2);
static void key_free (Key *key);

static void obj_destroyed_cb (Key *key, GObject *where_the_object_was);

struct _GdaAttributesManager {
	gboolean    for_objects; /* TRUE if key->data are GObjects */
	GHashTable *hash; /* key = a Key pointer, value = a GValue */
};

static guint
hash_func (gconstpointer key)
{
	return GPOINTER_TO_UINT (((Key*) key)->ptr) + g_str_hash (((Key*) key)->att_name);
}

static gboolean
equal_func (gconstpointer key1, gconstpointer key2)
{
	if ((((Key*) key1)->ptr == ((Key*) key2)->ptr) &&
	    !strcmp (((Key*) key1)->att_name, ((Key*) key2)->att_name))
		return TRUE;
	else
		return FALSE;
}

static void
key_free (Key *key)
{
	if (key->ptr && key->mgr->for_objects)
		g_object_weak_unref (G_OBJECT (key->ptr), (GWeakNotify) obj_destroyed_cb, key);
	if (key->att_name_destroy)
		key->att_name_destroy (key->att_name);

	g_free (key);
}

/**
 * gda_attributes_manager_new
 * @for_objects: set to TRUE if attributes will be set on objects.
 *
 * Creates a new #GdaAttributesManager, which can store (name, value) attributes for pointers or GObject objects
 * (in the latter case, the attibutes are destroyed when objects are also destroyed).
 *
 * Returns: the new #GdaAttributesManager
 */
GdaAttributesManager *
gda_attributes_manager_new (gboolean for_objects)
{
	GdaAttributesManager *mgr;

	mgr = g_new0 (GdaAttributesManager, 1);
	mgr->hash = g_hash_table_new_full (hash_func, equal_func, (GDestroyNotify) key_free, (GDestroyNotify) gda_value_free);
	mgr->for_objects = for_objects;

	return mgr;
}

/**
 * gda_attributes_manager_free
 * @mgr: a #GdaAttributesManager
 *
 * Frees all the resssources managed by @mgr
 */
void
gda_attributes_manager_free (GdaAttributesManager *mgr)
{
	g_hash_table_destroy (mgr->hash);
	g_free (mgr);
}

static void
obj_destroyed_cb (Key *key, GObject *where_the_object_was)
{
	key->ptr = NULL;
	g_hash_table_remove (key->mgr->hash, key);
}

static void
manager_real_set (GdaAttributesManager *mgr, gpointer ptr, 
		  const gchar *att_name, GDestroyNotify destroy, 
		  const GValue *value, gboolean steal_value)
{
	g_return_if_fail (att_name);
	if (mgr->for_objects) 
		g_return_if_fail (G_IS_OBJECT (ptr));

	if (value) {
		Key *key;

		key = g_new (Key, 1);
		key->mgr = mgr;
		key->ptr = ptr;
		key->att_name = att_name; /* NOT duplicated */
		key->att_name_destroy = destroy;
		if (mgr->for_objects) 
			g_object_weak_ref (G_OBJECT (key->ptr), (GWeakNotify) obj_destroyed_cb, key);
		if (steal_value)
			g_hash_table_insert (mgr->hash, key, value);
		else
			g_hash_table_insert (mgr->hash, key, gda_value_copy (value));
	}
	else {
		Key key;
		key.ptr = ptr;
		key.att_name = att_name;
		g_hash_table_remove (mgr->hash, &key);
	}
}

/**
 * gda_attributes_manager_set
 * @mgr: a #GdaAttributesManager
 * @ptr: a pointer to the ressources to which the attribute will apply
 * @att_name: an attribute's name, as a *static* string
 * @value: a #GValue, or %NULL
 *
 * Associates an attribute named @att_name to @ptr, with the value @value. Any previous association is replaced by
 * this one, and if @value is %NULL then the association is removed.
 *
 * Note: @att_name is *not* copied, so it should be a static string, or a string which exists as long as @mgr exists (or,
 * in case @ptr is an object, as long as that object exists). Libgda provides several predefined names for common attributes,
 * see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 */
void
gda_attributes_manager_set (GdaAttributesManager *mgr, gpointer ptr, const gchar *att_name, const GValue *value)
{
	manager_real_set (mgr, ptr, att_name, NULL, value, FALSE);
}

/**
 * gda_attributes_manager_set_full
 * @mgr: a #GdaAttributesManager
 * @ptr: a pointer to the ressources to which the attribute will apply
 * @att_name: an attribute's name, as a *static* string
 * @value: a #GValue, or %NULL
 * @destroy: function called when @att_name is destroyed
 *
 * Does the same as gda_attributes_manager_set() except that @destroy is called when @att_name needs
 * to be freed
 */
void
gda_attributes_manager_set_full (GdaAttributesManager *mgr, gpointer ptr,
				 const gchar *att_name, const GValue *value, GDestroyNotify destroy)
{
	manager_real_set (mgr, ptr, att_name, destroy, value, FALSE);
}


/**
 * gda_attributes_manager_get
 * @mgr: a #GdaAttributesManager
 * @ptr: a pointer to the ressources to which the attribute will apply
 * @att_name: an attribute's name, as a *static* string
 *
 * Retreives the value of an attribute previously set using gda_attributes_manager_set().
 *
 * Returns: the attribute's value, or %NULL if the attribute is not set.
 */
const GValue *
gda_attributes_manager_get (GdaAttributesManager *mgr, gpointer ptr, const gchar *att_name)
{
	Key key;
	key.ptr = ptr;
	key.att_name = (gchar*) att_name;
	return g_hash_table_lookup (mgr->hash, &key);
}

typedef struct {
	gpointer   *from;
	gpointer   *to;
	GSList     *keys;
	GSList     *values;
} CopyData;
static void foreach_copy_func (Key *key, const GValue *value, CopyData *cdata);

/**
 * gda_attributes_manager_copy
 * @from_mgr: a #GdaAttributesManager
 * @from:
 * @to_mgr: a #GdaAttributesManager
 * @to:
 *
 * For each attribute set for @from (in @from_mgr), set the same attribute to @to (in @to_mgr). @from_mgr and
 * @to_mgr can be equal.
 */
void
gda_attributes_manager_copy (GdaAttributesManager *from_mgr, gpointer *from, 
			     GdaAttributesManager *to_mgr, gpointer *to)
{
	CopyData cdata;
	GSList *nlist, *vlist;
	cdata.from = from;
	cdata.to = to;
	cdata.keys = NULL;
	cdata.values = NULL;
	g_hash_table_foreach (from_mgr->hash, (GHFunc) foreach_copy_func, &cdata);
	for (nlist = cdata.keys, vlist = cdata.values;
	     nlist && vlist;
	     nlist = nlist->next, vlist = vlist->next)
		gda_attributes_manager_set_full (to_mgr, to, ((Key*) nlist->data)->att_name, (GValue*) vlist->data,
						 ((Key*) nlist->data)->att_name_destroy);
	g_slist_free (cdata.keys);
	g_slist_free (cdata.values);
}

static void
foreach_copy_func (Key *key, const GValue *value, CopyData *cdata)
{
	if (key->ptr == cdata->from) {
		cdata->keys = g_slist_prepend (cdata->keys, key);
		cdata->values = g_slist_prepend (cdata->values, (gpointer) value);
	}
}

static void foreach_clear_func (Key *key, const GValue *value, CopyData *cdata);

/**
 * gda_attributes_manager_clear
 * @mgr: a #GdaAttributesManager
 * @ptr: a pointer to the ressources for which all the attributes will be removed
 *
 * Remove all the attributes managed by @mgr for the @ptr ressource.
 */
void
gda_attributes_manager_clear (GdaAttributesManager *mgr, gpointer ptr)
{
	CopyData cdata;
	GSList *nlist;
	cdata.from = ptr;
	cdata.to = NULL;
	cdata.keys = NULL;
	cdata.values = NULL;
	g_hash_table_foreach (mgr->hash, (GHFunc) foreach_clear_func, &cdata);
	for (nlist = cdata.keys;  nlist; nlist = nlist->next)
		gda_attributes_manager_set (mgr, ptr, (gchar*) nlist->data, NULL);
	g_slist_free (cdata.keys);
}

static void
foreach_clear_func (Key *key, const GValue *value, CopyData *cdata)
{
	if (key->ptr == cdata->from) 
		cdata->keys = g_slist_prepend (cdata->keys, (gpointer) key->att_name);
}


typedef struct {
	gpointer ptr;
	GdaAttributesManagerFunc func;
	gpointer data;
} FData;
static void foreach_foreach_func (Key *key, const GValue *value, FData *fdata);

/**
 * gda_attributes_manager_foreach
 * @mgr: a #GdaAttributesManager
 * @ptr: a pointer to the ressources for which all the attributes used
 * @func: a #GdaAttributesManagerFunc function
 * @data: user data to be passed as last argument of @func each time it is called
 *
 * Calls @func for every attribute set to @ptr.
 */
void
gda_attributes_manager_foreach (GdaAttributesManager *mgr, gpointer ptr, 
				GdaAttributesManagerFunc func, gpointer data)
{
	FData fdata;
	g_return_if_fail (func);
	g_return_if_fail (ptr);

	fdata.ptr = ptr;
	fdata.func = func;
	fdata.data = data;
	g_hash_table_foreach (mgr->hash, (GHFunc) foreach_foreach_func, &fdata);
}

static void
foreach_foreach_func (Key *key, const GValue *value, FData *fdata)
{
	if (key->ptr == fdata->ptr)
		fdata->func (key->att_name, value, fdata->data);
}
