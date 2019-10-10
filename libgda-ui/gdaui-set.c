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

/**
 * GdauiSetGroup:
 * @group: 
 * @source: 
 *
 * The <structname>GdauiSetGroup</structname>.
 *
 * To create a new #GdauiSetGroup use #gdaiu_set_group_new. 
 * 
 * To free a #GdauiSetGroup, created by #gdaui_set_group_new, use #gda_set_group_free.
 *
 * Since 5.2, you must consider this struct as opaque. Any access to its internal must use public API.
 * Don't try to use #gdaui_set_group_free on a struct that was created manually.
 */
struct _GdauiSetGroup {
        GdaSetGroup      *group;
        GdauiSetSource   *source; /* if NULL, then @group->nodes contains exactly one entry */

	/*< private >*/
        /* Padding for future expansion */
        gpointer      _gda_reserved1;
        gpointer      _gda_reserved2;
};

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
 * @group: a #GdaSetGroup
 * 
 * Creates a new #GdauiSetGroup struct.
 *
 * Return: (transfer full): a new #GdauiSetGroup struct.
 *
 * Since: 5.2
 */
GdauiSetGroup*
gdaui_set_group_new (GdaSetGroup *group)
{
	g_return_val_if_fail (group, NULL);
	GdauiSetGroup *sg = g_new0 (GdauiSetGroup, 1);
	sg->source = NULL;
	sg->group = group;
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
	n = gdaui_set_group_new (gdaui_set_group_get_group (sg));
	gdaui_set_group_set_source (n, gdaui_set_group_get_source (sg));
	return n;
}

/**
 * gdaui_set_group_free:
 * @sg: (nullable): a #GdauiSetGroup struct to free
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
 * @source (nullable): a #GdauiSetSource struct or NULL
 * 
 * Set source to @source. if @source is #NULL, then @group nodes contains exactly one entry.
 *
 * Since: 5.2
 */
void
gdaui_set_group_set_source (GdauiSetGroup *sg, GdauiSetSource *source)
{
	g_return_if_fail (sg);
	sg->source = source;
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
	g_warn_if_fail (group);
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

/**
 * GdauiSetSource:
 * @source: a #GdaSetSource
 * @shown_cols_index: (array length=shown_n_cols): Array with the column number to be shown from #GdaSetSource
 * @shown_n_cols: number of elements of @shown_cols_index
 * @ref_cols_index: (array length=ref_n_cols): Array with the number of columns as PRIMARY KEY in #GdaSetSource
 * @ref_n_cols: number of elements of @ref_cols_index
 *
 * The <structname>GdauiSetSource</structname> is a ...
 *
 * To create a new #GdauiSetSource use #gdaui_set_source_new.
 * 
 * To free a #GdauiSetSource, created by #gdaui_set_source_new, use #gdaui_set_source_free.
 *
 * Since 5.2, you must consider this struct as opaque. Any access to its internal must use public API.
 * Don't try to use #gdaui_set_source_free on a struct that was created manually.
 */
struct _GdauiSetSource {
        GdaSetSource   *source;

	/* displayed columns in 'source->data_model' */
 	gint shown_n_cols;
 	gint *shown_cols_index;

 	/* columns used as a reference (corresponding to PK values) in 'source->data_model' */
 	gint ref_n_cols;
 	gint *ref_cols_index; 

	/*< private >*/
        /* Padding for future expansion */
        gpointer        _gda_reserved1;
        gpointer        _gda_reserved2;
        gpointer        _gda_reserved3;
        gpointer        _gda_reserved4;
};

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
gdaui_set_source_new (GdaSetSource *source)
{
	g_return_val_if_fail (source, NULL);
	GdauiSetSource *s = g_new0 (GdauiSetSource, 1);
	s->source = source;
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
	g_return_val_if_fail (s, NULL);	
	n = gdaui_set_source_new (gdaui_set_source_get_source (s));
	gdaui_set_source_set_ref_columns (n, 
	                                  gdaui_set_source_get_ref_columns (s),
	                                  gdaui_set_source_get_ref_n_cols (s));
	gdaui_set_source_set_shown_columns (n,
	                                    gdaui_set_source_get_shown_columns (s),
	                                    gdaui_set_source_get_shown_n_cols (s));
	return n;
}

/**
 * gdaui_set_source_free:
 * @s: (nullable): a #GdauiSetSource struct to free
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
	g_return_if_fail (source);
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
 * gdaui_set_source_get_shown_n_cols:
 * @s: a #GdauiSetSource
 * 
 * Returns: number of columns to be shown.
 *
 * Since: 5.2
 */
gint
gdaui_set_source_get_shown_n_cols   (GdauiSetSource *s)
{
	g_return_val_if_fail (s, -1);
	return s->shown_n_cols;
}

/**
 * gdaui_set_source_get_shown_columns:
 * @s: a #GdauiSetSource
 * 
 * Returns: (array zero-terminated=1) (transfer none): array of of columns to be shown.
 *
 * Since: 5.2
 */
gint*
gdaui_set_source_get_shown_columns  (GdauiSetSource *s)
{
	g_return_val_if_fail (s, NULL);
	return s->shown_cols_index;
}

/**
 * gdaui_set_source_set_shown_columns:
 * @s: a #GdauiSetSource
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
	s->shown_cols_index = g_new0 (gint, s->shown_n_cols+1); /* This adds a null terminated array */
	for (i = 0; i < n_columns; i++) {
		s->shown_cols_index[i] = columns[i];
	}
}

/**
 * gdaui_set_source_get_ref_n_cols:
 * @s: a #GdauiSetSource
 * 
 * Returns: number of columns to referenced.
 *
 * Since: 5.2
 */
gint
gdaui_set_source_get_ref_n_cols   (GdauiSetSource *s)
{
	g_return_val_if_fail (s, -1);
	return s->ref_n_cols;
}

/**
 * gdaui_set_source_get_ref_columns:
 * @s: a #GdauiSetSource
 * 
 * Returns: (array zero-terminated=1) (transfer none): array of of columns to be shown.
 *
 * Since: 5.2
 */
gint*
gdaui_set_source_get_ref_columns  (GdauiSetSource *s)
{
	g_return_val_if_fail (s, NULL);
	return s->ref_cols_index;
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
	s->ref_cols_index = g_new0 (gint, s->ref_n_cols+1); /* This creates a null terminated array */
	for (i = 0; i < n_columns; i++) {
		s->ref_cols_index[i] = columns[i];
	}
}

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

typedef struct
{
	GdaSet *set;
	GSList *sources_list; /* list of GdauiSetSource */
	GSList *groups_list;  /* list of GdauiSetGroup */
} GdauiSetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiSet, gdaui_set, G_TYPE_OBJECT);


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

static void
gdaui_set_class_init (GdauiSetClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	gdaui_set_parent_class = g_type_class_peek_parent (class);
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
         * @source: (type GdauiSetSource): the #GdauiSetSource
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
	GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);
	priv->set = NULL;
  priv->sources_list = NULL;
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
        GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);

        if (priv->set) {
                g_signal_handlers_disconnect_by_func (G_OBJECT (priv->set),
                                                      G_CALLBACK (wrapped_set_public_data_changed_cb), set);
                g_signal_handlers_disconnect_by_func (G_OBJECT (priv->set),
                                                      G_CALLBACK (wrapped_set_source_model_changed_cb), set);

                g_object_unref (priv->set);
                priv->set = NULL;
        }

        clean_public_data (set);

        /* for the parent class */
        G_OBJECT_CLASS (gdaui_set_parent_class)->dispose (object);
}

static void
gdaui_set_set_property (GObject *object,
			guint param_id,
			const GValue *value,
			GParamSpec *pspec)
{
	GdauiSet *set;
	
	set = GDAUI_SET (object);
	
        GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);

	switch (param_id) {
	case PROP_SET:
		priv->set = g_value_get_object (value);
		if (priv->set) {
			g_object_ref (priv->set);
			compute_public_data (set);
			g_signal_connect (priv->set, "public-data-changed",
					  G_CALLBACK (wrapped_set_public_data_changed_cb), set);
			g_signal_connect (priv->set, "source-model-changed",
					  G_CALLBACK (wrapped_set_source_model_changed_cb), set);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gdaui_set_get_sources:
 * @set: a #GdauiSet object
 *
 * Returns: (transfer none)(element-type Gdaui.SetSource): list of #GdauiSetSource
 */
GSList*
gdaui_set_get_sources (GdauiSet *set) {
	GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);
	return priv->sources_list;
}


/**
 * gdaui_set_get_groups:
 * @set: a #GdauiSet object
 *
 * Returns: (transfer none)(element-type Gdaui.SetGroup): list of #GdauiSetGroup
 */
GSList*
gdaui_set_get_groups (GdauiSet *set) {
	GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);
	return priv->groups_list;
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
  GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);
	GSList *list;
	for (list = priv->sources_list; list; list = list->next) {
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
  GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);
	
	for (list = priv->sources_list; list; list = list->next) {
		GdauiSetSource *dsource = (GdauiSetSource*) list->data;
		gdaui_set_source_free (dsource);
	}
	g_slist_free (priv->sources_list);
	priv->sources_list = NULL;

	for (list = priv->groups_list; list; list = list->next) {
		GdauiSetGroup *dgroup = (GdauiSetGroup*) list->data;
		gdaui_set_group_free (dgroup);
	}
	g_slist_free (priv->groups_list);
	priv->groups_list = NULL;
}

static void
compute_public_data (GdauiSet *set)
{
	GSList *list;
        GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);
	GdaSet *aset = GDA_SET (priv->set);
	GHashTable *hash;
	
	/* scan GdaSetSource list */
	hash = g_hash_table_new (NULL, NULL);
	for (list = gda_set_get_sources (aset); list; list = list->next) {
		GdauiSetSource *dsource;
		dsource = gdaui_set_source_new (GDA_SET_SOURCE (list->data));
		priv->sources_list = g_slist_prepend (priv->sources_list, dsource);
		g_hash_table_insert (hash, list->data, dsource);

		compute_shown_columns_index (dsource);
		compute_ref_columns_index (dsource);
	}
	priv->sources_list = g_slist_reverse (priv->sources_list);

	/* scan GdaSetGroup list */
	for (list = gda_set_get_groups (aset); list; list = list->next) {
		GdauiSetGroup *dgroup;
		if (list->data == NULL) {
      g_warning (_("No group exists for Set"));
      continue;
    }
		dgroup = gdaui_set_group_new (GDA_SET_GROUP (list->data));
		gdaui_set_group_set_source (dgroup, 
		                            g_hash_table_lookup (hash, 
		                                                 gda_set_group_get_source (GDA_SET_GROUP (list->data))));
		priv->groups_list = g_slist_prepend (priv->groups_list, dgroup);
	}
	priv->groups_list = g_slist_reverse (priv->groups_list);

	g_hash_table_destroy (hash);
}

static void
update_public_data (GdauiSet *set)
{
	GSList *list, *elist = NULL;
        GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);
	GdaSet *aset = GDA_SET (priv->set);
	GHashTable *shash; /* key = GdaSetSource, value = GdauiSetSource */
	GHashTable *ghash; /* key = GdaSetGroup, value = GdauiSetGroup */

	/* build hash with existing sources in GdauiSet */
	shash = g_hash_table_new (NULL, NULL);
	for (list = priv->sources_list; list; list = list->next) {
		GdauiSetSource *dsource = (GdauiSetSource*) list->data;
		g_hash_table_insert (shash, gdaui_set_source_get_source (dsource), dsource);
	}

	/* scan GdaSetSource list */
	elist = priv->sources_list;
	priv->sources_list = NULL;
	for (list = gda_set_get_sources (aset); list; list = list->next) {
		GdauiSetSource *dsource;
		dsource = g_hash_table_lookup (shash, list->data);
		if (dsource) {
			priv->sources_list = g_slist_prepend (priv->sources_list, dsource);
			continue;
		}
		dsource = gdaui_set_source_new (GDA_SET_SOURCE (list->data));
		priv->sources_list = g_slist_prepend (priv->sources_list, dsource);
		g_hash_table_insert (shash, list->data, dsource);

		compute_shown_columns_index (dsource);
		compute_ref_columns_index (dsource);
	}
	priv->sources_list = g_slist_reverse (priv->sources_list);

	if (elist) {
		for (list = elist; list; list = list->next) {
			if (!g_slist_find (priv->sources_list, list->data)) {
				GdauiSetSource *dsource = (GdauiSetSource*) list->data;
				gdaui_set_source_free (dsource);
			}
		}
		g_slist_free (elist);
	}

	/* build hash with existing groups */
	ghash = g_hash_table_new (NULL, NULL);
	for (list = priv->groups_list; list; list = list->next) {
		GdauiSetGroup *dgroup = (GdauiSetGroup*) list->data;
		g_hash_table_insert (ghash, gdaui_set_group_get_group (dgroup), dgroup);
	}

	/* scan GdaSetGroup list */
	elist = priv->groups_list;
	priv->groups_list = NULL;
	for (list = gda_set_get_groups (aset); list; list = list->next) {
		GdauiSetGroup *dgroup;
		dgroup = g_hash_table_lookup (ghash, list->data);
		if (dgroup) {
			priv->groups_list = g_slist_prepend (priv->groups_list, dgroup);
			continue;
		}
		dgroup = gdaui_set_group_new (GDA_SET_GROUP (list->data));
		gdaui_set_group_set_source (dgroup, g_hash_table_lookup (shash, 
		                                           gda_set_group_get_source (gdaui_set_group_get_group (dgroup))));
		priv->groups_list = g_slist_prepend (priv->groups_list, dgroup);
	}
	priv->groups_list = g_slist_reverse (priv->groups_list);

	if (elist) {
		for (list = elist; list; list = list->next) {
			if (!g_slist_find (priv->groups_list, list->data)) {
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

	nholders = gda_set_source_get_n_nodes (gdaui_set_source_get_source (dsource));
	g_return_if_fail (nholders > 0);
	ncols = gda_data_model_get_n_columns (gda_set_source_get_data_model (gdaui_set_source_get_source (dsource)));
	g_return_if_fail (ncols > 0);

	if (ncols > nholders) {
		/* we only want columns which are not holders */
		gint i, current = 0;

		masksize = ncols - nholders;
		mask = g_new0 (gint, masksize);
		for (i = 0; i < ncols ; i++) {
			GSList *list = gda_set_source_get_nodes (gdaui_set_source_get_source (dsource));
			gboolean found = FALSE;
			while (list && !found) {
				if (gda_set_node_get_source_column (GDA_SET_NODE (list->data)) == i)
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

	nholders = gda_set_source_get_n_nodes (gdaui_set_source_get_source (dsource));
	g_return_if_fail (nholders > 0);
	ncols = gda_data_model_get_n_columns (gda_set_source_get_data_model (gdaui_set_source_get_source (dsource)));
	g_return_if_fail (ncols > 0);

	if (ncols > nholders) {
		/* we only want columns which are holders */
		gint i, current = 0;

		masksize = ncols - nholders;
		mask = g_new0 (gint, masksize);
		for (i=0; i<ncols ; i++) {
			GSList *list = gda_set_source_get_nodes (gdaui_set_source_get_source (dsource));
			gboolean found = FALSE;
			while (list && !found) {
				if (gda_set_node_get_source_column (GDA_SET_NODE (list->data)) == i)
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
	
  GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);

	switch (param_id) {
	case PROP_SET:
		g_value_set_object (value, priv->set);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}	
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
gdaui_set_get_group (GdauiSet *set, GdaHolder *holder)
{
	GdaSetGroup *agroup;
	GSList *list;
	g_return_val_if_fail (GDAUI_IS_SET (set), NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);


	GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);

	agroup = gda_set_get_group (priv->set, holder);
	if (!agroup)
		return NULL;
	
	for (list = priv->groups_list; list; list = list->next) {
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
  GdauiSetPrivate *priv = gdaui_set_get_instance_private (set);
	g_print ("=== GdauiSet %p ===\n", set);
	gda_set_dump (priv->set);
	g_slist_foreach (priv->sources_list, (GFunc) set_source_dump, NULL);
	g_slist_foreach (priv->groups_list, (GFunc) set_group_dump, NULL);
	g_print ("=== GdauiSet %p END ===\n", set);
}
#endif
