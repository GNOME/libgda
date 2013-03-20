/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2013 Daniel Espinosa <esodan@gmail.com>
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
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-set.h"
#include "marshallers/gdaui-marshal.h"

/*
   Register GdauiSetGroup type
*/
GType
gdaui_set_group_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
        if (type == 0)
			type = g_boxed_type_register_static ("GdauiSetGroup",
							     (GBoxedCopyFunc) gdaui_set_group_copy,
							     (GBoxedFreeFunc) gdaui_set_group_free);
	}

	return type;
}

/**
 * gdaui_set_group_new:
 * 
 * Creates a new #GdauiSetGroup struct.
 *
 * Return: (transfer full): a new #GdauiSetGroup struct.
 *
 * Since: 5.2
 */
GdauiSetGroup*
gdaui_set_group_new (void)
{
	GdauiSetGroup *sg = g_new0 (GdauiSetGroup, 1);
	sg->source = NULL;
	sg->group = NULL;
	return sg;
}

/**
 * gdaui_set_group_copy:
 * @sg: a #GdauiSetGroup
 *
 * Copy constructor.
 *
 * Returns: a new #GdauiSetGroup
 *
 * Since: 5.2
 */
GdauiSetGroup *
gdaui_set_group_copy (GdauiSetGroup *sg)
{
	g_return_val_if_fail (sg, NULL);

	GdauiSetGroup *n;
	n = gdaui_set_group_new ();
	n->source = sg->source;
	n->group = sg->group;
	return n;
}

/**
 * gdaui_set_group_free:
 * @sg: (allow-none): a #GdauiSetGroup struct to free
 * 
 * Frees any resources taken by @sg struct. If @sg is %NULL, then nothing happens.
 *
 * Since: 5.2
 */
void
gdaui_set_group_free (GdauiSetGroup *sg)
{
	g_return_if_fail(sg);
	g_free (sg);
}

/**
 * gdaui_set_group_set_source:
 * @sg: a #GdauiSetGroup struct to free
 * @source: a #GdauiSetSource struct
 * 
 * Set source to @source.
 *
 * Since: 5.2
 */
void
gdaui_set_group_set_source (GdauiSetGroup *sg, GdauiSetSource *source)
{
	g_return_if_fail (sg);
	sg->source = sg->source;
}

/**
 * gdaui_set_group_get_source:
 * @sg: a #GdauiSetGroup struct
 * 
 * Get source used by @sg.
 * 
 * Returns: used #GdaSetGroup
 *
 * Since: 5.2
 */
GdauiSetSource*
gdaui_set_group_get_source (GdauiSetGroup *sg)
{
	g_return_val_if_fail (sg, NULL);
	return sg->source;
}

/**
 * gdaui_set_group_set_group:
 * @sg: a #GdauiSetGroup struct to free
 * @group: a #GdaSetGroup struct
 * 
 * Set source to @source.
 *
 * Since: 5.2
 */
void
gdaui_set_group_set_group (GdauiSetGroup *sg, GdaSetGroup *group)
{
	g_return_if_fail (sg);
	g_return_if_fail (group);
	sg->group = sg->group;
}

/**
 * gdaui_set_group_get_group:
 * @sg: a #GdauiSetGroup struct to free
 * 
 * Get group used by @sg.
 * 
 * Returns: used #GdaSetGroup
 *
 * Since: 5.2
 */
GdaSetGroup*
gdaui_set_group_get_group (GdauiSetGroup *sg)
{
	g_return_val_if_fail (sg, NULL);
	return sg->group;
}

/*
   Register GdauiSetSource type
*/
GType
gdaui_set_source_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
        if (type == 0)
			type = g_boxed_type_register_static ("GdauiSetSource",
							     (GBoxedCopyFunc) gdaui_set_source_copy,
							     (GBoxedFreeFunc) gdaui_set_source_free);
	}

	return type;
}

/**
 * gdaui_set_source_new:
 * 
 * Creates a new #GdauiSetSource struct.
 *
 * Return: (transfer full): a new #GdauiSetSource struct.
 *
 * Since: 5.2
 */
GdauiSetSource*
gdaui_set_source_new (void)
{
	GdauiSetSource *s = g_new0 (GdauiSetSource, 1);
	s->source = NULL;
	s->shown_n_cols = 0;
	s->shown_cols_index = NULL;
	s->ref_n_cols = 0;
	s->ref_cols_index = NULL;
	
	return s;
}

/**
 * gdaui_set_source_copy:
 * @s: a #GdauiSetGroup
 *
 * Copy constructor.
 *
 * Returns: a new #GdauiSetSource
 *
 * Since: 5.2
 */
GdauiSetSource *
gdaui_set_source_copy (GdauiSetSource *s)
{
	GdauiSetSource *n;
	gint i,j;
	g_return_val_if_fail (s, NULL);	
	n = gdaui_set_source_new ();
	n->source = s->source;
	n->ref_n_cols = s->ref_n_cols;
	n->ref_cols_index = g_new0 (gint,n->ref_n_cols);
	for (i = 0; i < n->ref_n_cols; i++) {
		n->ref_cols_index[i] = s->ref_cols_index[i];
	}
	n->shown_n_cols = s->shown_n_cols;
	n->shown_cols_index = g_new0 (gint, n->shown_n_cols);
	for (j = 0; j < n->shown_n_cols; j++) {
		n->shown_cols_index[j] = s->shown_cols_index[j];
	}
	return n;
}

/**
 * gdaui_set_source_free:
 * @s: (allow-none): a #GdauiSetSource struct to free
 * 
 * Frees any resources taken by @s struct. If @s is %NULL, then nothing happens.
 *
 * Since: 5.2
 */
void
gdaui_set_source_free (GdauiSetSource *s)
{
	g_return_if_fail(s);
	if(s->shown_cols_index)
		g_free(s->shown_cols_index);
	if (s->ref_cols_index)
		g_free(s->ref_cols_index);
	g_free (s);
}

/**
 * gdaui_set_source_set_source:
 * @s: a #GdauiSetSource struct to free
 * @source: a #GdaSetSource struct
 * 
 * Set source to @source.
 *
 * Since: 5.2
 */
void
gdaui_set_source_set_source (GdauiSetSource *s, GdaSetSource *source)
{
	g_return_if_fail (s);
	s->source = s->source;
}

/**
 * gdaui_set_source_get_source:
 * @s: a #GdauiSetGroup struct
 * 
 * Get source used by @sg.
 * 
 * Returns: used #GdaSetSource
 *
 * Since: 5.2
 */
GdaSetSource*
gdaui_set_source_get_source (GdauiSetSource *s)
{
	g_return_val_if_fail (s, NULL);
	return s->source;
}

/**
 * gdaui_set_source_set_shown_columns:
 * @s: a #GdauiSetSource struct to free
 * @columns: (array length=n_columns): an array of with columns numbers to be shown from a #GdaSetSource
 * @n_columns: number of columns of the array
 * 
 * Set the columns to be shown.
 *
 * Since: 5.2
 */
void
gdaui_set_source_set_shown_columns (GdauiSetSource *s, gint *columns, gint n_columns)
{
	gint i;
	g_return_if_fail (s);
	g_return_if_fail (columns);
	if (s->shown_cols_index)
		g_free (s->shown_cols_index);
	s->shown_n_cols = n_columns;
	for (i = 0; i < n_columns; i++) {
		s->shown_cols_index[i] = columns[i];
	}
}

/**
 * gdaui_set_source_set_ref_columns:
 * @s: a #GdauiSetSource struct to free
 * @columns: (array length=n_columns): an array of with columns numbers of referen (Primary Key) at #GdaSetSource
 * @n_columns: number of columns of the array
 * 
 * Set the columns to be shown.
 *
 * Since: 5.2
 */
void
gdaui_set_source_set_ref_columns (GdauiSetSource *s, gint *columns, gint n_columns)
{
	gint i;
	g_return_if_fail (s);
	g_return_if_fail (columns);
	if (s->ref_cols_index)
		g_free (s->ref_cols_index);
	s->ref_n_cols = n_columns;
	for (i = 0; i < n_columns; i++) {
		s->ref_cols_index[i] = columns[i];
	}
}

static void gdaui_set_class_init (GdauiSetClass * class);
static void gdaui_set_init (GdauiSet *wid);
static void gdaui_set_dispose (GObject *object);

static void gdaui_set_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gdaui_set_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);

static void wrapped_set_public_data_changed_cb (GdaSet *wset, GdauiSet *set);
static void wrapped_set_source_model_changed_cb (GdaSet *wset, GdaSetSource *source, GdauiSet *set);
static void clean_public_data (GdauiSet *set);
static void compute_public_data (GdauiSet *set);
static void update_public_data (GdauiSet *set);
static void compute_shown_columns_index (GdauiSetSource *dsource);
static void compute_ref_columns_index (GdauiSetSource *dsource);

struct _GdauiSetPriv
{
	GdaSet *set;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* properties */
enum {
	PROP_0,
	PROP_SET
};

/* signals */
enum {
	PUBLIC_DATA_CHANGED,
	SOURCE_MODEL_CHANGED,
	LAST_SIGNAL
};

static gint gdaui_set_signals[LAST_SIGNAL] = { 0, 0 };

GType
_gdaui_set_get_type (void) {
	return gdaui_set_get_type ();
}

GType
gdaui_set_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiSetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_set_class_init,
			NULL,
			NULL,
			sizeof (GdauiSet),
			0,
			(GInstanceInitFunc) gdaui_set_init,
			0
		};		

		type = g_type_register_static (G_TYPE_OBJECT, "GdauiSet", &info, 0);
	}

	return type;
}

static void
gdaui_set_class_init (GdauiSetClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);
	object_class->dispose = gdaui_set_dispose;

	/**
         * GdauiSet::public-data-changed:
         * @set: the #GdauiSet
         * 
         * Gets emitted when @set's public data (#GdauiSetGroup or #GdauiSetSource values) have changed
	 *
	 * Since: 4.2
         */
        gdaui_set_signals[PUBLIC_DATA_CHANGED] =
                g_signal_new ("public-data-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdauiSetClass, public_data_changed),
                              NULL, NULL,
                              _gdaui_marshal_VOID__VOID, G_TYPE_NONE, 0);
	/**
         * GdauiSet::source-model-changed:
         * @set: the #GdauiSet
         * 
         * Gets emitted when the data model used in @set's #GdauiSetSource has changed
	 *
	 * Since: 4.2
         */
        gdaui_set_signals[SOURCE_MODEL_CHANGED] =
                g_signal_new ("source-model-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdauiSetClass, public_data_changed),
                              NULL, NULL,
                              _gdaui_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

        class->public_data_changed = NULL;
        class->source_model_changed = NULL;

	/* Properties */
        object_class->set_property = gdaui_set_set_property;
        object_class->get_property = gdaui_set_get_property;
	g_object_class_install_property (object_class, PROP_SET,
                                         g_param_spec_object ("set", NULL, NULL, 
							      GDA_TYPE_SET,
							      G_PARAM_READABLE | G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
}

static void
gdaui_set_init (GdauiSet *set)
{
	set->priv = g_new0 (GdauiSetPriv, 1);
	set->priv->set = NULL;
}

/*
 * _gdaui_set_new
 * @set: a #GdaSet
 *
 * Creates a new #GdauiSet which wraps @set's properties
 *
 * Returns: the new widget
 * 
 * Deprecated: Since 5.2
 */
GdauiSet *
_gdaui_set_new (GdaSet *set)
{
	return gdaui_set_new (set);
}

/**
 * gdaui_set_new:
 * @set: a #GdaSet
 *
 * Creates a new #GdauiSet which wraps @set's properties
 *
 * Returns: the new widget
 * Since: 5.2
 **/
GdauiSet *
gdaui_set_new (GdaSet *set)
{
	g_return_val_if_fail (GDA_IS_SET (set), NULL);

	return (GdauiSet *) g_object_new (GDAUI_TYPE_SET, "set", set, NULL);
}

static void
gdaui_set_dispose (GObject *object)
{
        GdauiSet *set;

        g_return_if_fail (GDAUI_IS_SET (object));

        set = GDAUI_SET (object);

        if (set->priv) {
                if (set->priv->set) {
                        g_signal_handlers_disconnect_by_func (G_OBJECT (set->priv->set),
                                                              G_CALLBACK (wrapped_set_public_data_changed_cb), set);
                        g_signal_handlers_disconnect_by_func (G_OBJECT (set->priv->set),
                                                              G_CALLBACK (wrapped_set_source_model_changed_cb), set);

                        g_object_unref (set->priv->set);
                        set->priv->set = NULL;
                }

		clean_public_data (set);

                g_free (set->priv);
                set->priv = NULL;
        }

        /* for the parent class */
        parent_class->dispose (object);
}

static void
gdaui_set_set_property (GObject *object,
			guint param_id,
			const GValue *value,
			GParamSpec *pspec)
{
	GdauiSet *set;
	
	set = GDAUI_SET (object);
	
	switch (param_id) {
	case PROP_SET:
		set->priv->set = g_value_get_object (value);
		if (set->priv->set) {
			g_object_ref (set->priv->set);
			compute_public_data (set);
			g_signal_connect (set->priv->set, "public-data-changed",
					  G_CALLBACK (wrapped_set_public_data_changed_cb), set);
			g_signal_connect (set->priv->set, "source-model-changed",
					  G_CALLBACK (wrapped_set_source_model_changed_cb), set);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
wrapped_set_public_data_changed_cb (G_GNUC_UNUSED GdaSet *wset, GdauiSet *set)
{
	update_public_data (set);
	g_signal_emit (set, gdaui_set_signals[PUBLIC_DATA_CHANGED], 0);
}

static void
wrapped_set_source_model_changed_cb (G_GNUC_UNUSED GdaSet *wset, GdaSetSource *source, GdauiSet *set)
{
	GdauiSetSource *uisource = NULL;
	GSList *list;
	for (list = set->sources_list; list; list = list->next) {
		if (((GdauiSetSource*) list->data)->source == source) {
			uisource = (GdauiSetSource*) list->data;
			break;
		}
	}
	if (uisource)
		g_signal_emit (set, gdaui_set_signals[SOURCE_MODEL_CHANGED], 0, uisource);
}

static void
clean_public_data (GdauiSet *set)
{
	GSList *list;
	
	for (list = set->sources_list; list; list = list->next) {
		GdauiSetSource *dsource = (GdauiSetSource*) list->data;
		gdaui_set_source_free (dsource);
	}
	g_slist_free (set->sources_list);
	set->sources_list = NULL;

	for (list = set->groups_list; list; list = list->next) {
		GdauiSetGroup *dgroup = (GdauiSetGroup*) list->data;
		gdaui_set_group_free (dgroup);
	}
	g_slist_free (set->groups_list);
	set->groups_list = NULL;
}

static void
compute_public_data (GdauiSet *set)
{
	GSList *list;
	GdaSet *aset = GDA_SET (set->priv->set);
	GHashTable *hash;
	
	/* scan GdaSetSource list */
	hash = g_hash_table_new (NULL, NULL);
	for (list = aset->sources_list; list; list = list->next) {
		GdauiSetSource *dsource;
		dsource = gdaui_set_source_new ();
		set->sources_list = g_slist_prepend (set->sources_list, dsource);
		g_hash_table_insert (hash, list->data, dsource);

		gdaui_set_source_set_source (dsource, GDA_SET_SOURCE (list->data));
		compute_shown_columns_index (dsource);
		compute_ref_columns_index (dsource);
	}
	set->sources_list = g_slist_reverse (set->sources_list);

	/* scan GdaSetGroup list */
	for (list = aset->groups_list; list; list = list->next) {
		GdauiSetGroup *dgroup;
		dgroup = gdaui_set_group_new ();
		set->groups_list = g_slist_prepend (set->groups_list, dgroup);
		gdaui_set_group_set_group (dgroup, GDA_SET_GROUP (list->data));
		gdaui_set_group_set_source (dgroup, 
		                            g_hash_table_lookup (hash, 
		                                                 GDA_SET_GROUP (list->data)->nodes_source));
	}
	set->groups_list = g_slist_reverse (set->groups_list);

	g_hash_table_destroy (hash);
}

static void
update_public_data (GdauiSet *set)
{
	GSList *list, *elist = NULL;
	GdaSet *aset = GDA_SET (set->priv->set);
	GHashTable *shash; /* key = GdaSetSource, value = GdauiSetSource */
	GHashTable *ghash; /* key = GdaSetGroup, value = GdauiSetGroup */

	/* build hash with existing sources in GdauiSet */
	shash = g_hash_table_new (NULL, NULL);
	for (list = set->sources_list; list; list = list->next) {
		GdauiSetSource *dsource = (GdauiSetSource*) list->data;
		g_hash_table_insert (shash, dsource->source, dsource);
	}

	/* scan GdaSetSource list */
	elist = set->sources_list;
	set->sources_list = NULL;
	for (list = aset->sources_list; list; list = list->next) {
		GdauiSetSource *dsource;
		dsource = g_hash_table_lookup (shash, list->data);
		if (dsource) {
			set->sources_list = g_slist_prepend (set->sources_list, dsource);
			continue;
		}
		dsource = gdaui_set_source_new ();
		set->sources_list = g_slist_prepend (set->sources_list, dsource);
		g_hash_table_insert (shash, list->data, dsource);

		gdaui_set_source_set_source (dsource, GDA_SET_SOURCE (list->data));
		compute_shown_columns_index (dsource);
		compute_ref_columns_index (dsource);
	}
	set->sources_list = g_slist_reverse (set->sources_list);

	if (elist) {
		for (list = elist; list; list = list->next) {
			if (!g_slist_find (set->sources_list, list->data)) {
				GdauiSetSource *dsource = (GdauiSetSource*) list->data;
				gdaui_set_source_free (dsource);
			}
		}
		g_slist_free (elist);
	}

	/* build hash with existing groups */
	ghash = g_hash_table_new (NULL, NULL);
	for (list = set->groups_list; list; list = list->next) {
		GdauiSetGroup *dgroup = (GdauiSetGroup*) list->data;
		g_hash_table_insert (ghash, dgroup->group, dgroup);
	}

	/* scan GdaSetGroup list */
	elist = set->groups_list;
	set->groups_list = NULL;
	for (list = aset->groups_list; list; list = list->next) {
		GdauiSetGroup *dgroup;
		dgroup = g_hash_table_lookup (ghash, list->data);
		if (dgroup) {
			set->groups_list = g_slist_prepend (set->groups_list, dgroup);
			continue;
		}
		dgroup = gdaui_set_group_new ();
		set->groups_list = g_slist_prepend (set->groups_list, dgroup);
		gdaui_set_group_set_group (dgroup, GDA_SET_GROUP (list->data));
		gdaui_set_group_set_source (dgroup, g_hash_table_lookup (shash, dgroup->group->nodes_source));
	}
	set->groups_list = g_slist_reverse (set->groups_list);

	if (elist) {
		for (list = elist; list; list = list->next) {
			if (!g_slist_find (set->groups_list, list->data)) {
				GdauiSetGroup *dgroup = (GdauiSetGroup*) list->data;
				gdaui_set_group_free (dgroup);
			}
		}
		g_slist_free (elist);
	}

	g_hash_table_destroy (shash);
	g_hash_table_destroy (ghash);
}

static void
compute_shown_columns_index (GdauiSetSource *dsource)
{
	gint ncols, nholders;
	gint *mask = NULL, masksize = 0;

	nholders = g_slist_length (dsource->source->nodes);
	g_return_if_fail (nholders > 0);
	ncols = gda_data_model_get_n_columns (GDA_DATA_MODEL (dsource->source->data_model));
	g_return_if_fail (ncols > 0);

	if (ncols > nholders) {
		/* we only want columns which are not holders */
		gint i, current = 0;

		masksize = ncols - nholders;
		mask = g_new0 (gint, masksize);
		for (i = 0; i < ncols ; i++) {
			GSList *list = dsource->source->nodes;
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
	gdaui_set_source_set_shown_columns (dsource, mask, masksize);
}

void
compute_ref_columns_index (GdauiSetSource *dsource)
{
	gint ncols, nholders;
	gint *mask = NULL, masksize = 0;

	nholders = g_slist_length (dsource->source->nodes);
	g_return_if_fail (nholders > 0);
	ncols = gda_data_model_get_n_columns (GDA_DATA_MODEL (dsource->source->data_model));
	g_return_if_fail (ncols > 0);

	if (ncols > nholders) {
		/* we only want columns which are holders */
		gint i, current = 0;

		masksize = ncols - nholders;
		mask = g_new0 (gint, masksize);
		for (i=0; i<ncols ; i++) {
			GSList *list = dsource->source->nodes;
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
	gdaui_set_source_set_ref_columns (dsource, mask, masksize);
}


static void
gdaui_set_get_property (GObject *object,
			guint param_id,
			GValue *value,
			GParamSpec *pspec)
{
	GdauiSet *set;

	set = GDAUI_SET (object);
	
	switch (param_id) {
	case PROP_SET:
		g_value_set_object (value, set->priv->set);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}	
}

/**
 * _gdaui_set_get_group:
 * @dbset:
 * @holder:
 * 
 * Returns: A new #GdauiSetGroup struct
 * Deprecated: Since 5.2
 **/
GdauiSetGroup  *
_gdaui_set_get_group (GdauiSet *dbset, GdaHolder *holder) 
{
	return gdaui_set_get_group (dbset, holder);
}

/**
 * gdaui_set_get_group:
 * @dbset:
 * @holder:
 * 
 * Returns: A new #GdauiSetGroup struct
 * Since: 5.2
 **/
GdauiSetGroup  *
gdaui_set_get_group (GdauiSet *dbset, GdaHolder *holder)
{
	GdaSetGroup *agroup;
	GSList *list;
	g_return_val_if_fail (GDAUI_IS_SET (dbset), NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);

	agroup = gda_set_get_group (dbset->priv->set, holder);
	if (!agroup)
		return NULL;
	
	for (list = dbset->groups_list; list; list = list->next) {
		if (GDAUI_SET_GROUP (list->data)->group == agroup)
			return GDAUI_SET_GROUP (list->data);
	}
	return NULL;
}

#ifdef GDA_DEBUG_NO
static void _gda_set_node_dump (GdaSetNode *node);
static void set_source_dump (GdauiSetSource *source);
static void set_group_dump (GdauiSetGroup *group);

static void
set_source_dump (GdauiSetSource *source)
{
	g_print ("  GdauiSetSource %p\n", source);
	if (source) {
		gint i;
		g_print ("    - GdaSetSource: %p\n", source->source);
		for (i = 0; i < source->shown_n_cols; i++)
			g_print ("    - shown_cols_index [%d]: %d\n", i, source->shown_cols_index[i]);
		for (i = 0; i < source->ref_n_cols; i++)
			g_print ("    - ref_cols_index [%d]: %d\n", i, source->ref_cols_index[i]);
	}
}

static void
set_group_dump (GdauiSetGroup *group)
{
	g_print ("  GdauiSetGroup %p\n", group);
	if (group) {
		g_print ("    - GdaSetGroup: %p\n", group->group);
		if (group->group->nodes)
			g_slist_foreach (group->group->nodes, (GFunc) _gda_set_node_dump, NULL);
		else
			g_print ("                 ERROR: group has no node!\n");
		g_print ("    - GdauiSetSource: %p\n", group->source);
	}
}

static void
_gda_set_node_dump (GdaSetNode *node)
{
	g_print ("      - GdaSetNode: %p\n", node);
	g_print ("        - holder: %p (%s)\n", node->holder, node->holder ? gda_holder_get_id (node->holder) : "ERROR : no GdaHolder!");
	g_print ("        - source_model: %p\n", node->source_model);
	g_print ("        - source_column: %d\n", node->source_column);
}

static void
_gdaui_set_dump (GdauiSet *set)
{
	g_print ("=== GdauiSet %p ===\n", set);
	gda_set_dump (set->priv->set);
	g_slist_foreach (set->sources_list, (GFunc) set_source_dump, NULL);
	g_slist_foreach (set->groups_list, (GFunc) set_group_dump, NULL);
	g_print ("=== GdauiSet %p END ===\n", set);
}
#endif
