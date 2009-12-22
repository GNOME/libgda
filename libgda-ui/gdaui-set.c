/* gdaui-set.c
 *
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-set.h"
#include "marshallers/gdaui-marshal.h"

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
static void clean_public_data (GdauiSet *set);
static void compute_public_data (GdauiSet *set);
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
	LAST_SIGNAL
};

static gint gdaui_set_signals[LAST_SIGNAL] = { 0 };

GType
_gdaui_set_get_type (void)
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
			(GInstanceInitFunc) gdaui_set_init
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
         * GdauiSet::public-data-changed
         * @set: the #GdauiSet
         * 
         * Gets emitted when @set's public data (#GdauiSetGroup or #GdauiSetSource values) have changed
         */
        gdaui_set_signals[PUBLIC_DATA_CHANGED] =
                g_signal_new ("public-data-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdauiSetClass, public_data_changed),
                              NULL, NULL,
                              _gdaui_marshal_VOID__VOID, G_TYPE_NONE, 0);

        class->public_data_changed = NULL;

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
 *  Returns: the new widget
 */
GdauiSet *
_gdaui_set_new (GdaSet *set)
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
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
wrapped_set_public_data_changed_cb (GdaSet *wset, GdauiSet *set)
{
	clean_public_data (set);
	compute_public_data (set);
	g_signal_emit (set, gdaui_set_signals[PUBLIC_DATA_CHANGED], 0);
}

static void
clean_public_data (GdauiSet *set)
{
	GSList *list;
	
	for (list = set->sources_list; list; list = list->next) {
		GdauiSetSource *dsource = (GdauiSetSource*) list->data;
		g_free (dsource->shown_cols_index);
		g_free (dsource->ref_cols_index);
		g_free (dsource);
	}
	g_slist_free (set->sources_list);
	set->sources_list = NULL;

	for (list = set->groups_list; list; list = list->next) {
		GdauiSetGroup *dgroup = (GdauiSetGroup*) list->data;
		g_free (dgroup);
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
		dsource = g_new0 (GdauiSetSource, 1);
		set->sources_list = g_slist_prepend (set->sources_list, dsource);
		g_hash_table_insert (hash, list->data, dsource);

		dsource->source = GDA_SET_SOURCE (list->data);
		compute_shown_columns_index (dsource);
		compute_ref_columns_index (dsource);
	}
	set->sources_list = g_slist_reverse (set->sources_list);

	/* scan GdaSetGroup list */
	for (list = aset->groups_list; list; list = list->next) {
		GdauiSetGroup *dgroup;
		dgroup = g_new0 (GdauiSetGroup, 1);
		set->groups_list = g_slist_prepend (set->groups_list, dgroup);
		dgroup->group = GDA_SET_GROUP (list->data);
		dgroup->source = g_hash_table_lookup (hash, GDA_SET_GROUP (list->data)->nodes_source);
	}
	set->groups_list = g_slist_reverse (set->groups_list);

	g_hash_table_destroy (hash);
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

	dsource->shown_n_cols = masksize;
	dsource->shown_cols_index = mask;
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

	dsource->ref_n_cols = masksize;
	dsource->ref_cols_index = mask;
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

/*
 * _gdaui_set_get_group
 */
GdauiSetGroup  *
_gdaui_set_get_group (GdauiSet *dbset, GdaHolder *holder)
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
