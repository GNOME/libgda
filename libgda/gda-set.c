/*
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2013 Daniel Espinosa <esodan@gmail.com>
 * Copyright (C) 2015 Corentin Noël <corentin@elementary.io>
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
#define G_LOG_DOMAIN "GDA-set"

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
#include <libgda/binreloc/gda-binreloc.h>

/**
 * GdaSetGroup:
 * @nodes: (element-type Gda.SetNode): list of GdaSetNode, at least one entry
 * @nodes_source: (nullable):  if NULL, then @nodes contains exactly one entry
 *
 * Since 5.2, you must consider this struct as opaque. Any access to its internal must use public API.
 * Don't try to use #gda_set_group_free on a struct that was created manually.
 */
struct _GdaSetGroup {
	GSList       *nodes;       /* list of GdaSetNode, at least one entry */
	GdaSetSource *nodes_source; /* if NULL, then @nodes contains exactly one entry */

	/*< private >*/
	/* Padding for future expansion */
	gpointer      _gda_reserved1;
	gpointer      _gda_reserved2;
};
/*
   Register GdaSetGroup type
*/
GType
gda_set_group_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
        if (type == 0)
			type = g_boxed_type_register_static ("GdaSetGroup",
							     (GBoxedCopyFunc) gda_set_group_copy,
							     (GBoxedFreeFunc) gda_set_group_free);
	}

	return type;
}

/**
 * gda_set_group_new:
 * @node: a #GdaSetNode struct
 * 
 * Creates a new #GdaSetGroup struct. If @source is %NULL then new group contains 
 * just one #GdaSetNode.
 *
 * Return: (transfer full): a new #GdaSetGroup struct.
 *
 * Since: 5.2
 */
GdaSetGroup*
gda_set_group_new (GdaSetNode *node)
{
	GdaSetGroup *sg;
	
	g_return_val_if_fail (node, NULL);

	sg = g_new0 (GdaSetGroup, 1);
	sg->nodes_source = NULL;
	sg->nodes = NULL;
	sg->nodes = g_slist_append (sg->nodes, node);
	return sg;
}

/**
 * gda_set_group_copy:
 * @sg: a #GdaSetGroup
 *
 * Copy constructor.
 *
 * Returns: a new #GdaSetGroup
 *
 * Since: 5.2
 */
GdaSetGroup *
gda_set_group_copy (GdaSetGroup *sg)
{
	g_return_val_if_fail (sg, NULL);

	GdaSetGroup *n;
	n = g_new0 (GdaSetGroup, 1);
	n->nodes_source = sg->nodes_source;
	n->nodes = g_slist_copy (sg->nodes);
	return n;
}

/**
 * gda_set_group_free:
 * @sg: (nullable): a #GdaSetGroup struct to free
 * 
 * Frees any resources taken by @sg struct. If @sg is %NULL, then nothing happens.
 *
 * Since: 5.2
 */
void
gda_set_group_free (GdaSetGroup *sg)
{
	g_return_if_fail(sg);
	g_slist_free (sg->nodes);
	g_free (sg);
}

/**
 * gda_set_group_set_source:
 * @sg: a #GdaSetGroup
 * @source: a #GdaSetSource to set
 * 
 * Since: 5.2
 */
void 
gda_set_group_set_source (GdaSetGroup *sg, GdaSetSource *source)
{
	g_return_if_fail (sg);
	sg->nodes_source = source;
}

/**
 * gda_set_group_get_source:
 * @sg: a #GdaSetGroup
 * 
 * Returns: a #GdaSetSource. If %NULL then @sg contains just one element.
 * 
 * Since: 5.2
 */
GdaSetSource*
gda_set_group_get_source      (GdaSetGroup *sg)
{
	g_return_val_if_fail (sg, NULL);
	return sg->nodes_source;
}

/**
 * gda_set_group_add_node:
 * @sg: a #GdaSetGroup
 * @node: a #GdaSetNode to set
 * 
 * Since: 5.2
 */
void 
gda_set_group_add_node (GdaSetGroup *sg, GdaSetNode *node)
{
	g_return_if_fail (sg);
	g_return_if_fail (node);
	sg->nodes = g_slist_append (sg->nodes, node);
}

/**
 * gda_set_group_get_node:
 * @sg: a #GdaSetGroup
 * 
 * This method always return first #GdaSetNode in @sg.
 * 
 * Returns: first #GdaSetNode in @sg.
 * 
 * Since: 5.2
 */
GdaSetNode* 
gda_set_group_get_node (GdaSetGroup *sg)
{
	g_return_val_if_fail (sg, NULL);
	g_return_val_if_fail (sg->nodes, NULL);
	return GDA_SET_NODE (sg->nodes->data);
}

/**
 * gda_set_group_get_nodes:
 * @sg: a #GdaSetGroup
 * 
 * Returns a #GSList with the #GdaSetNode grouped by @sg. You must use
 * #g_slist_free on returned list.
 * 
 * Returns: (transfer none) (element-type Gda.SetNode): a #GSList with all nodes in @sg. 
 * 
 * Since: 5.2
 */
GSList* 
gda_set_group_get_nodes (GdaSetGroup *sg)
{
	g_return_val_if_fail (sg, NULL);
	g_return_val_if_fail (sg->nodes, NULL);
	return sg->nodes;
}

/**
 * gda_set_group_get_n_nodes:
 * @sg: a #GdaSetGroup
 * 
 * Returns: number of nodes in @sg. 
 * 
 * Since: 5.2
 */
gint
gda_set_group_get_n_nodes (GdaSetGroup *sg)
{
	g_return_val_if_fail (sg, -1);
	return g_slist_length (sg->nodes);
}

/**
 * GdaSetSource:
 * @data_model: Can't be NULL
 * @nodes: (element-type Gda.SetNode): list of #GdaSetNode for which source_model == @data_model
 * 
 * Since 5.2, you must consider this struct as opaque. Any access to its internal must use public API.
 * Don't try to use #gda_set_source_free on a struct that was created manually.
 **/
struct _GdaSetSource {
	GdaDataModel   *data_model;   /* Can't be NULL */
	GSList         *nodes;        /* list of #GdaSetNode for which source_model == @data_model */

	/*< private >*/
	/* Padding for future expansion */
	gpointer        _gda_reserved1;
	gpointer        _gda_reserved2;
	gpointer        _gda_reserved3;
	gpointer        _gda_reserved4;
};

/*
   Register GdaSetSource type
*/
GType
gda_set_source_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
        if (type == 0)
			type = g_boxed_type_register_static ("GdaSetSource",
							     (GBoxedCopyFunc) gda_set_source_copy,
							     (GBoxedFreeFunc) gda_set_source_free);
	}

	return type;
}

/**
 * gda_set_source_new:
 * @model: a #GdaDataModel
 * 
 * Creates a new #GdaSetSource struct.
 *
 * Return: (transfer full): a new #GdaSetSource struct.
 *
 * Since: 5.2
 */
GdaSetSource*
gda_set_source_new (GdaDataModel *model)
{
	g_return_val_if_fail (model != NULL && GDA_IS_DATA_MODEL (model), NULL);
	GdaSetSource *s = g_new0 (GdaSetSource, 1);
	s->nodes = NULL;
	s->data_model = g_object_ref (model);

	return s;
}

/**
 * gda_set_source_copy:
 * @s: a #GdaSetGroup
 *
 * Copy constructor.
 *
 * Returns: a new #GdaSetSource
 *
 * Since: 5.2
 */
GdaSetSource *
gda_set_source_copy (GdaSetSource *s)
{
	GdaSetSource *n;
	g_return_val_if_fail (s, NULL);	
	n = gda_set_source_new (gda_set_source_get_data_model (s));
	n->nodes = g_slist_copy (s->nodes);
	return n;
}
	
/**
 * gda_set_source_free:
 * @s: (nullable): a #GdaSetSource struct to free
 * 
 * Frees any resources taken by @s struct. If @s is %NULL, then nothing happens.
 *
 * Since: 5.2
 */
void
gda_set_source_free (GdaSetSource *s)
{
	g_return_if_fail(s);
	g_object_unref (s->data_model);
	g_slist_free (s->nodes); /* FIXME: A source must own its nodes, then they must be freed here
										this leaves to others responsability free nodes BEFORE
										to free this source */
	g_free (s);
}

/**
 * gda_set_source_get_data_model:
 * @s: a #GdaSetSource
 * 
 * Returns: (transfer none): a #GdaDataModel used by @s
 * 
 * Since: 5.2
 */
GdaDataModel*
gda_set_source_get_data_model (GdaSetSource *s)
{
	g_return_val_if_fail (s, NULL);
	return s->data_model;
}

/**
 * gda_set_source_set_data_model:
 * @s: a #GdaSetSource struct to free
 * @model: a #GdaDataModel
 * 
 * Set a #GdaDataModel
 *
 * Since: 5.2
 */
void
gda_set_source_set_data_model (GdaSetSource *s, GdaDataModel *model)
{
	g_return_if_fail (s);
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	s->data_model = g_object_ref (model);
}

/**
 * gda_set_source_add_node:
 * @s: a #GdaSetSource
 * @node: a #GdaSetNode to add
 * 
 * Set a #GdaDataModel
 *
 * Since: 5.2
 */
void
gda_set_source_add_node (GdaSetSource *s, GdaSetNode *node)
{
	g_return_if_fail (s);
	g_return_if_fail (node);
	s->nodes = g_slist_append (s->nodes, node);
}

/**
 * gda_set_source_get_nodes:
 * @s: a #GdaSetSource
 * 
 * Returns: (transfer none) (element-type Gda.SetNode): a list of #GdaSetNode structs
 *
 * Since: 5.2
 */
GSList*
gda_set_source_get_nodes (GdaSetSource *s)
{
	g_return_val_if_fail (s, NULL);
	g_return_val_if_fail (s->nodes, NULL);
	return s->nodes;
}

/**
 * gda_set_source_get_n_nodes:
 * @s: a #GdaSetSource
 * 
 * Returns: number of nodes in @sg. 
 * 
 * Since: 5.2
 */
gint
gda_set_source_get_n_nodes (GdaSetSource *s)
{
	g_return_val_if_fail (s, -1);
	return g_slist_length (s->nodes);
}
/**
 * GdaSetNode:
 * @holder: a #GdaHolder. It can't be NULL
 * @source_model: a #GdaDataModel. It could be NULL
 * @source_column: a #gint with the number of column in @source_model
 * 
 * Since 5.2, you must consider this struct as opaque. Any access to its internal must use public API.
 * Don't try to use #gda_set_node_free on a struct that was created manually.
 */
struct _GdaSetNode {
	GdaHolder    *holder;        /* Can't be NULL */
	GdaDataModel *source_model;  /* may be NULL */
	gint          source_column; /* unused if @source_model is NULL */

	/*< private >*/
	/* Padding for future expansion */
	gpointer      _gda_reserved1;
	gpointer      _gda_reserved2;
};
/*
   Register GdaSetNode type
*/
GType
gda_set_node_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
        if (type == 0)
			type = g_boxed_type_register_static ("GdaSetNode",
							     (GBoxedCopyFunc) gda_set_node_copy,
							     (GBoxedFreeFunc) gda_set_node_free);
	}

	return type;
}

/**
 * gda_set_node_new:
 * @holder: a #GdaHolder to used by new #GdaSetNode
 * 
 * Creates a new #GdaSetNode struct.
 *
 * Return: (transfer full): a new #GdaSetNode struct.
 *
 * Since: 5.2
 */
GdaSetNode*
gda_set_node_new (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	GdaSetNode *n = g_new0 (GdaSetNode, 1);
	n->holder = holder;
	n->source_model = NULL;
	return n;
}

/**
 * gda_set_node_copy:
 * @node: a #GdaSetNode to copy from
 *
 * Copy constructor.
 *
 * Returns: a new #GdaSetNode
 *
 * Since: 5.2
 */
GdaSetNode *
gda_set_node_copy (GdaSetNode *node)
{
	g_return_val_if_fail (node, NULL);

	GdaSetNode *n;
	n = gda_set_node_new (gda_set_node_get_holder (node));
	gda_set_node_set_source_column (n, gda_set_node_get_source_column (node));
	gda_set_node_set_holder (n, gda_set_node_get_holder (node));
	gda_set_node_set_data_model (n, gda_set_node_get_data_model (node));
	return n;
}

/**
 * gda_set_node_free:
 * @node: (nullable): a #GdaSetNode struct to free
 * 
 * Frees any resources taken by @node struct. If @node is %NULL, then nothing happens.
 *
 * Since: 5.2
 */
void
gda_set_node_free (GdaSetNode *node)
{
	if (node == NULL)
		return;
	g_free (node);
}

/**
 * gda_set_node_get_holder:
 * @node: a #GdaSetNode struct to get holder from
 * 
 * Returns: (transfer none): the #GdaHolder used by @node
 *
 * Since: 5.2
 */
GdaHolder*
gda_set_node_get_holder (GdaSetNode *node)
{
	g_return_val_if_fail (node, NULL);
	return node->holder;
}

/**
 * gda_set_node_set_holder:
 * @node: a #GdaSetNode struct to set holder to
 * 
 * Set a #GdaHolder to @node.
 *
 * Since: 5.2
 */
void
gda_set_node_set_holder (GdaSetNode *node, GdaHolder *holder)
{
	g_return_if_fail (node);
	g_return_if_fail (GDA_IS_HOLDER (holder));
	node->holder = holder;
}

/**
 * gda_set_node_get_data_model:
 * @node: a #GdaSetNode struct to get holder from
 * 
 * Returns: (transfer none): the #GdaDataModel used by @node
 *
 * Since: 5.2
 */
GdaDataModel*
gda_set_node_get_data_model (GdaSetNode *node)
{
	g_return_val_if_fail (node, NULL);
	return node->source_model;
}

/**
 * gda_set_node_set_data_model:
 * @node: a #GdaSetNode struct to set data model to
 * @model: (nullable): a #GdaDataModel to be used by @node
 * 
 * Set a #GdaDataModel to be used by @node. @model increment its reference
 * counting when set. Internally referenced column number is set to first column
 * in @model.
 *
 * Since: 5.2
 */
void
gda_set_node_set_data_model (GdaSetNode *node, GdaDataModel *model)
{
	g_return_if_fail (node);
	if (GDA_IS_DATA_MODEL (model)) {
		node->source_model = model;
		node->source_column = 0;
	}
	else {
		node->source_model = NULL;
		node->source_column = -1;
	}
}

/**
 * gda_set_node_get_source_column:
 * @node: a #GdaSetNode struct to get column source from 
 * 
 * Returns: the number of column referenced in a given #GdaDataModel. If negative
 * no column is referenced or no #GdaDataModel is used by @node.
 *
 * Since: 5.2
 */
gint
gda_set_node_get_source_column (GdaSetNode *node)
{
	g_return_val_if_fail (node, -1);
	return node->source_column;
}

/**
 * gda_set_node_set_source_column:
 * @node: a #GdaSetNode struct to set column source to, from an used data model 
 * 
 * Set column number in the #GdaDataModel used @node. If no #GdaDataModel is set
 * then column is set to invalid (-1);
 *
 * Since: 5.2
 */
void
gda_set_node_set_source_column (GdaSetNode *node, gint column)
{
	g_return_if_fail (column >= 0);
	if (GDA_IS_DATA_MODEL (node->source_model)) {
		if (column < gda_data_model_get_n_columns (node->source_model))
			node->source_column = column;
	}
}

/* 
 * Main static functions 
 */

/* private structure */
typedef struct
{
	gchar           *id;
	gchar           *name;
	gchar           *descr;
	GHashTable      *holders_hash; /* key = GdaHoler ID, value = GdaHolder */
	GPtrArray       *holders_array;
	gboolean         read_only;
	gboolean         validate_changes;

	GSList         *holders;   /* list of GdaHolder objects */
	GSList         *nodes_list;   /* list of GdaSetNode */
	GSList         *sources_list; /* list of GdaSetSource */
	GSList         *groups_list;  /* list of GdaSetGroup */
} GdaSetPrivate;


G_DEFINE_TYPE_WITH_PRIVATE(GdaSet, gda_set, G_TYPE_OBJECT)

static void gda_set_dispose (GObject *object);
static void gda_set_finalize (GObject *object);

static void set_remove_node (GdaSet *set, GdaSetNode *node);
static void set_remove_source (GdaSet *set, GdaSetSource *source);

static void holder_to_default_cb (GdaHolder *holder, GdaSet *dataset);
static void changed_holder_cb (GdaHolder *holder, GdaSet *dataset);
static GError *validate_change_holder_cb (GdaHolder *holder, const GValue *value, GdaSet *dataset);
static void source_changed_holder_cb (GdaHolder *holder, GdaSet *dataset);
static void holder_notify_cb (GdaHolder *holder, GParamSpec *pspec, GdaSet *dataset);


static void compute_public_data (GdaSet *set);
static gboolean gda_set_real_add_holder (GdaSet *set, GdaHolder *holder);

/* properties */
enum
{
	PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_DESCR,
	PROP_HOLDERS,
	PROP_VALIDATE_CHANGES
};

/* signals */
enum
{
	HOLDER_CHANGED,
	PUBLIC_DATA_CHANGED,
	HOLDER_ATTR_CHANGED,
	VALIDATE_HOLDER_CHANGE,
	VALIDATE_SET,
	HOLDER_TYPE_SET,
	SOURCE_MODEL_CHANGED,
	LAST_SIGNAL
};

static gint gda_set_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0, 0 };

static void
gda_set_set_property (GObject *object,
		      guint param_id,
		      const GValue *value,
		      GParamSpec *pspec)
{
	GdaSet* set;
	set = GDA_SET (object);
  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	switch (param_id) {
	case PROP_ID:
		if (priv->id != NULL)  {
			g_free (priv->id);
		}
		priv->id = g_value_dup_string (value);
		break;
	case PROP_NAME:
		if (priv->name != NULL) {
			g_free (priv->name);
		}
		priv->name = g_value_dup_string (value);
		break;
	case PROP_DESCR:
		if (priv->descr) {
			g_free (priv->descr);
		}
		priv->descr = g_value_dup_string (value);
		break;
	case PROP_HOLDERS: {
		/* add the holders */
		GSList* holders;
		for (holders = (GSList*) g_value_get_pointer (value); holders; holders = holders->next) 
			gda_set_real_add_holder (set, GDA_HOLDER (holders->data));
		compute_public_data (set);
		break;
	}
	case PROP_VALIDATE_CHANGES:
		if (priv->validate_changes != g_value_get_boolean (value)) {
			GSList *list;
			priv->validate_changes = g_value_get_boolean (value);
			for (list = priv->holders; list; list = list->next) {
				GdaHolder *holder = (GdaHolder*) list->data;
				g_object_set ((GObject*) holder, "validate-changes",
					      priv->validate_changes, NULL);
				if (priv->validate_changes)
					g_signal_connect ((GObject*) holder, "validate-change",
							  G_CALLBACK (validate_change_holder_cb), set);
				else
					g_signal_handlers_disconnect_by_func ((GObject*) holder,
									      G_CALLBACK (validate_change_holder_cb),
									      set);
			}
		}
		break;
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
  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	switch (param_id) {
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_NAME:
		if (priv->name)
			g_value_set_string (value, priv->name);
		else
			g_value_set_string (value, priv->id);
		break;
	case PROP_DESCR:
		g_value_set_string (value, priv->descr);
		break;
	case PROP_VALIDATE_CHANGES:
		g_value_set_boolean (value, priv->validate_changes);
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

static gboolean
validate_accumulator (G_GNUC_UNUSED GSignalInvocationHint *ihint,
                     GValue *return_accu,
                     const GValue *handler_return,
                     G_GNUC_UNUSED gpointer data)
{
	GError *error;

	error = g_value_get_boxed (handler_return);
	g_value_set_boxed (return_accu, error);

	return error ? FALSE : TRUE; /* stop signal if an error has been set */
}

static GError *
m_validate_holder_change (G_GNUC_UNUSED GdaSet *set, G_GNUC_UNUSED GdaHolder *holder,
			  G_GNUC_UNUSED const GValue *new_value)
{
	return NULL;
}

static GError*
m_validate_set (G_GNUC_UNUSED GdaSet *set)
{
	return NULL;
}

static void
gda_set_class_init (GdaSetClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	gda_set_parent_class = g_type_class_peek_parent (class);

	gda_set_signals[HOLDER_CHANGED] =
		g_signal_new ("holder-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaSetClass, holder_changed),
			      NULL, NULL,
			      _gda_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
			      GDA_TYPE_HOLDER);

	/**
	 * GdaSet::validate-holder-change:
	 * @set: the #GdaSet
	 * @holder: the #GdaHolder which is going to change
	 * @new_value: the proposed new value for @holder
	 * 
	 * Gets emitted when a #GdaHolder's in @set is going to change its value. One can connect to
	 * this signal to control which values @holder can have (for example to implement some business rules)
	 *
	 * Returns: NULL if @holder is allowed to change its value to @new_value, or a #GError
	 * otherwise.
	 */
	gda_set_signals[VALIDATE_HOLDER_CHANGE] =
		g_signal_new ("validate-holder-change",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaSetClass, validate_holder_change),
			      validate_accumulator, NULL,
			     _gda_marshal_ERROR__OBJECT_VALUE, G_TYPE_ERROR, 2,
			      GDA_TYPE_HOLDER, G_TYPE_VALUE);
	/**
	 * GdaSet::validate-set:
	 * @set: the #GdaSet
	 * 
	 * Gets emitted when gda_set_is_valid() is called, use
	 * this signal to control which combination of values @set's holder can have (for example to implement some business rules)
	 *
	 * Returns: NULL if @set's contents has been validated, or a #GError
	 * otherwise.
	 */
	gda_set_signals[VALIDATE_SET] =
		g_signal_new ("validate-set",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaSetClass, validate_set),
			      validate_accumulator, NULL,
			      _gda_marshal_ERROR__VOID, G_TYPE_ERROR, 0);
	/**
	 * GdaSet::holder-attr-changed:
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
	 * GdaSet::public-data-changed:
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

	/**
	 * GdaSet::holder-type-set:
	 * @set: the #GdaSet
	 * @holder: the #GdaHolder for which the #GType has been set
	 *
	 * Gets emitted when @holder in @set has its type finally set, in case
	 * it was #GDA_TYPE_NULL
	 *
	 * Since: 4.2
	 */
	gda_set_signals[HOLDER_TYPE_SET] =
		g_signal_new ("holder-type-set",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaSetClass, holder_type_set),
			      NULL, NULL,
			      _gda_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
			      GDA_TYPE_HOLDER);

	/**
	 * GdaSet::source-model-changed:
	 * @set: the #GdaSet
	 * @source: the #GdaSetSource for which the @data_model attribute has changed
	 *
	 * Gets emitted when the data model in @source has changed
	 *
	 * Since: 4.2
	 */
	gda_set_signals[SOURCE_MODEL_CHANGED] =
		g_signal_new ("source-model-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaSetClass, source_model_changed),
			      NULL, NULL,
			      _gda_marshal_VOID__POINTER, G_TYPE_NONE, 1,
			      G_TYPE_POINTER);


	class->holder_changed = NULL;
	class->validate_holder_change = m_validate_holder_change;
	class->validate_set = m_validate_set;
	class->holder_attr_changed = NULL;
	class->public_data_changed = NULL;
	class->holder_type_set = NULL;
	class->source_model_changed = NULL;

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
	/**
	 * GdaSet:validate-changes:
	 *
	 * Defines if the "validate-set" signal gets emitted when
	 * any holder in the data set changes. This property also affects the
	 * GdaHolder:validate-changes property.
	 *
	 * Since: 5.2.0
	 */
	g_object_class_install_property (object_class, PROP_VALIDATE_CHANGES,
					 g_param_spec_boolean ("validate-changes", NULL, "Defines if the validate-set signal is emitted", TRUE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	object_class->dispose = gda_set_dispose;
	object_class->finalize = gda_set_finalize;
}

static void
gda_set_init (GdaSet *set)
{
  GdaSetPrivate *priv = gda_set_get_instance_private (set);
	priv->holders = NULL;
	priv->nodes_list = NULL;
	priv->sources_list = NULL;
	priv->groups_list = NULL;
	priv->holders_hash = g_hash_table_new (g_str_hash, g_str_equal);
	priv->holders_array = NULL;
	priv->read_only = FALSE;
	priv->validate_changes = TRUE;
}


/**
 * gda_set_new:
 * @holders: (element-type Gda.Holder) (transfer none): a list of #GdaHolder objects
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

/**
 * gda_set_new_read_only:
 * @holders: (element-type Gda.Holder) (transfer none): a list of #GdaHolder objects
 *
 * Creates a new #GdaSet like gda_set_new(), but does not allow modifications to any of the #GdaHolder
 * object in @holders. This function is used for Libgda's database providers' implementation.
 *
 * Returns: a new #GdaSet object
 *
 * Since: 4.2
 */
GdaSet *
gda_set_new_read_only (GSList *holders)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_SET, NULL);

  GdaSetPrivate *priv = gda_set_get_instance_private (GDA_SET(obj));

	priv->read_only = TRUE;
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
 * Returns: (transfer full): a new #GdaSet object
 */
GdaSet *
gda_set_copy (GdaSet *set)
{
	GdaSet *copy;
	GSList *list, *holders = NULL;
	g_return_val_if_fail (GDA_IS_SET (set), NULL);
  GdaSetPrivate *priv = gda_set_get_instance_private (set);
	
	for (list = priv->holders; list; list = list->next)
		holders = g_slist_prepend (holders, gda_holder_copy (GDA_HOLDER (list->data)));
	holders = g_slist_reverse (holders);

	copy = g_object_new (GDA_TYPE_SET, "holders", holders, NULL);
	g_slist_free_full (holders, (GDestroyNotify) g_object_unref);

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
 * Returns: (transfer full): a new #GdaSet object
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
		holder = gda_holder_new (type, id);

		value = gda_value_new (type);
		if (g_type_is_a (type, G_TYPE_BOOLEAN))
			g_value_set_boolean (value, va_arg (ap, int));
                else if (g_type_is_a (type, G_TYPE_STRING))
			g_value_set_string (value, va_arg (ap, gchar *));
                else if (g_type_is_a (type, G_TYPE_OBJECT))
			g_value_set_object (value, va_arg (ap, gpointer));
		else if (g_type_is_a (type, G_TYPE_INT))
			g_value_set_int (value, va_arg (ap, gint));
		else if (g_type_is_a (type, G_TYPE_UINT))
			g_value_set_uint (value, va_arg (ap, guint));
		else if (g_type_is_a (type, GDA_TYPE_BINARY))
			gda_value_set_binary (value, va_arg (ap, GdaBinary *));
		else if (g_type_is_a (type, G_TYPE_INT64))
			g_value_set_int64 (value, va_arg (ap, gint64));
		else if (g_type_is_a (type, G_TYPE_UINT64))
			g_value_set_uint64 (value, va_arg (ap, guint64));
		else if (g_type_is_a (type, GDA_TYPE_SHORT))
			gda_value_set_short (value, va_arg (ap, int));
		else if (g_type_is_a (type, GDA_TYPE_USHORT))
			gda_value_set_ushort (value, va_arg (ap, guint));
		else if (g_type_is_a (type, G_TYPE_CHAR))
			g_value_set_schar (value, va_arg (ap, gint));
		else if (g_type_is_a (type, G_TYPE_UCHAR))
			g_value_set_uchar (value, va_arg (ap, guint));
		else if (g_type_is_a (type, G_TYPE_FLOAT))
			g_value_set_float (value, va_arg (ap, double));
		else if (g_type_is_a (type, G_TYPE_DOUBLE))
			g_value_set_double (value, va_arg (ap, gdouble));
		else if (g_type_is_a (type, GDA_TYPE_NUMERIC))
			gda_value_set_numeric (value, va_arg (ap, GdaNumeric *));
		else if (g_type_is_a (type, G_TYPE_DATE))
			g_value_set_boxed (value, va_arg (ap, GDate *));
		else if (g_type_is_a (type, G_TYPE_LONG))
			g_value_set_long (value, va_arg (ap, glong));
		else if (g_type_is_a (type, G_TYPE_ULONG))
			g_value_set_ulong (value, va_arg (ap, gulong));
		else if (g_type_is_a (type, G_TYPE_GTYPE))
			g_value_set_gtype (value, va_arg(ap, GType));
		else if (g_type_is_a (type, G_TYPE_DATE_TIME))
			g_value_set_boxed (value, va_arg(ap, GDateTime *));
		else if (g_type_is_a (type, GDA_TYPE_TIME))
			gda_value_set_time (value, va_arg(ap, GdaTime *));
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
		g_slist_free_full (holders, (GDestroyNotify) g_object_unref);
	}
	return set;
}

/**
 * gda_set_set_holder_value:
 * @set: a #GdaSet object
 * @error: (nullable): a place to store errors, or %NULL
 * @holder_id: the ID of the holder to set the value
 * @...: value, of the correct type, depending on the requested holder's type (not NULL)
 *
 * Set the value of the #GdaHolder which ID is @holder_id to a specified value
 *
 * Returns: %TRUE if no error occurred and the value was set correctly
 */
gboolean
gda_set_set_holder_value (GdaSet *set, GError **error, const gchar *holder_id, ...)
{
	GdaHolder *holder;
	va_list ap;
	GValue *value;
	GType type;
  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	g_return_val_if_fail (GDA_IS_SET (set), FALSE);
	g_return_val_if_fail (priv, FALSE);

	holder = gda_set_get_holder (set, holder_id);
	if (!holder) {
		g_set_error (error, GDA_SET_ERROR, GDA_SET_HOLDER_NOT_FOUND_ERROR,
			     _("GdaHolder with ID '%s' not found in set"), holder_id);
		return FALSE;
	}
	type = gda_holder_get_g_type (holder);
	va_start (ap, holder_id);
	value = gda_value_new (type);
	if (g_type_is_a (type, G_TYPE_BOOLEAN))
		g_value_set_boolean (value, va_arg (ap, int));
	else if (g_type_is_a (type, G_TYPE_STRING))
		g_value_set_string (value, va_arg (ap, gchar *));
	else if (g_type_is_a (type, G_TYPE_OBJECT))
		g_value_set_object (value, va_arg (ap, gpointer));
	else if (g_type_is_a (type, G_TYPE_INT))
		g_value_set_int (value, va_arg (ap, gint));
	else if (g_type_is_a (type, G_TYPE_UINT))
		g_value_set_uint (value, va_arg (ap, guint));
	else if (g_type_is_a (type, GDA_TYPE_BINARY))
		gda_value_set_binary (value, va_arg (ap, GdaBinary *));
	else if (g_type_is_a (type, G_TYPE_INT64))
		g_value_set_int64 (value, va_arg (ap, gint64));
	else if (g_type_is_a (type, G_TYPE_UINT64))
		g_value_set_uint64 (value, va_arg (ap, guint64));
	else if (g_type_is_a (type, GDA_TYPE_SHORT))
		gda_value_set_short (value, va_arg (ap, int));
	else if (g_type_is_a (type, GDA_TYPE_USHORT))
		gda_value_set_ushort (value, va_arg (ap, guint));
	else if (g_type_is_a (type, G_TYPE_CHAR))
		g_value_set_schar (value, va_arg (ap, gint));
	else if (g_type_is_a (type, G_TYPE_UCHAR))
		g_value_set_uchar (value, va_arg (ap, guint));
	else if (g_type_is_a (type, G_TYPE_FLOAT))
		g_value_set_float (value, va_arg (ap, double));
	else if (g_type_is_a (type, G_TYPE_DOUBLE))
		g_value_set_double (value, va_arg (ap, gdouble));
	else if (g_type_is_a (type, GDA_TYPE_NUMERIC))
		gda_value_set_numeric (value, va_arg (ap, GdaNumeric *));
	else if (g_type_is_a (type, G_TYPE_DATE))
		g_value_set_boxed (value, va_arg (ap, GDate *));
	else if (g_type_is_a (type, G_TYPE_DATE_TIME))
		g_value_set_boxed (value, va_arg (ap, GDateTime*));
	else if (g_type_is_a (type, GDA_TYPE_TIME))
		gda_value_set_time (value, va_arg (ap, GdaTime*));
	else if (g_type_is_a (type, G_TYPE_LONG))
		g_value_set_long (value, va_arg (ap, glong));
	else if (g_type_is_a (type, G_TYPE_ULONG))
		g_value_set_ulong (value, va_arg (ap, gulong));
	else if (g_type_is_a (type, G_TYPE_GTYPE))
		g_value_set_gtype (value, va_arg (ap, GType));	
	else {
		g_set_error (error, GDA_SET_ERROR, GDA_SET_IMPLEMENTATION_ERROR,
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
 * Returns: (nullable) (transfer none): the requested GValue, or %NULL (see gda_holder_get_value())
 */
const GValue *
gda_set_get_holder_value (GdaSet *set, const gchar *holder_id)
{
	GdaHolder *holder;

	g_return_val_if_fail (GDA_IS_SET (set), NULL);

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

  g_return_val_if_fail (priv, NULL);

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
 * @error: (nullable): a place to store the error, or %NULL
 *
 * Creates a new #GdaSet object from the @xml_spec
 * specifications
 *
 * Returns: (transfer full): a new object, or %NULL if an error occurred
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
	  xmlDtdPtr gda_paramlist_dtd = NULL;
    GFile *file;
	  gchar *path;

    validc = g_new0 (xmlValidCtxt, 1);
    validc->userData = &err_str;
    validc->error = xml_validity_error_func;
    validc->warning = NULL;

    xmlcheck = xmlDoValidityCheckingDefaultValue;
    xmlDoValidityCheckingDefaultValue = 1;

    /* replace the DTD with ours */
		/* paramlist DTD */
    path = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "libgda-paramlist.dtd", NULL);
    file = g_file_new_for_path (path);
	  if (g_file_query_exists (file, NULL)) {
      gda_paramlist_dtd = xmlParseDTD (NULL, (xmlChar*) path);
    }

	  if (!gda_paramlist_dtd) {
		  if (g_getenv ("GDA_TOP_SRC_DIR")) {
        gchar *ipath;

			  ipath = g_build_filename (g_getenv ("GDA_TOP_SRC_DIR"), "libgda", "libgda-paramlist.dtd", NULL);
			  gda_paramlist_dtd = xmlParseDTD (NULL, (xmlChar*) ipath);
        g_free (ipath);
		  }
		  if (!gda_paramlist_dtd)
			  g_message (_("Could not parse '%s': XML data import validation will not be performed (some weird errors may occur)"),
				     path);
	  }
    g_free (path);
    g_object_unref (file);
	  if (gda_paramlist_dtd != NULL) {
		  gda_paramlist_dtd->name = xmlStrdup((xmlChar*) "data-set-spec");
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
		if (gda_paramlist_dtd != NULL) {
			doc->intSubset = old_dtd;
      xmlFreeDtd (gda_paramlist_dtd);
      gda_paramlist_dtd = NULL;
    }
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
 * @xml_spec: a #xmlNodePtr for a &lt;parameters&gt; tag
 * @error: (nullable): a place to store the error, or %NULL
 *
 * Creates a new #GdaSet object from the @xml_spec
 * specifications
 *
 * Returns: (transfer full): a new object, or %NULL if an error occurred
 */
GdaSet *
gda_set_new_from_spec_node (xmlNodePtr xml_spec, GError **error)
{
	GdaSet *set = NULL;
	GSList *holders = NULL, *sources = NULL;
	GSList *list;
	const gchar *lang = setlocale(LC_ALL, NULL);

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
					g_set_error (error, GDA_SET_ERROR, GDA_SET_XML_SPEC_ERROR,
						     "%s", err->message);
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
				GType gt;
				gt = gdatype ? gda_g_type_from_string ((gchar *) gdatype) : G_TYPE_STRING;
				if (gt == G_TYPE_INVALID)
					gt = GDA_TYPE_NULL;
				holder = (GdaHolder*) (g_object_new (GDA_TYPE_HOLDER,
								     "g-type", gt,
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

    GdaSetPrivate *priv = gda_set_get_instance_private (set);

		prop = xmlGetProp(xml_spec, (xmlChar*)"id");
		if (prop) {
			priv->id = g_strdup ((gchar*)prop);
			xmlFree (prop);
		}
		prop = xmlGetProp(xml_spec, (xmlChar*)"name");
		if (prop) {
			priv->name = g_strdup ((gchar*)prop);
			xmlFree (prop);
		}
		prop = xmlGetProp(xml_spec, (xmlChar*)"descr");
		if (prop) {
			priv->descr = g_strdup ((gchar*)prop);
			xmlFree (prop);
		}
	}

	g_slist_free_full (holders, (GDestroyNotify) g_object_unref);
	g_slist_free_full (sources, (GDestroyNotify) g_object_unref);

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
	GdaDataModel *model;

	g_return_if_fail (GDA_IS_SET (set));

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	g_return_if_fail (priv);
	g_return_if_fail (g_slist_find (priv->holders, holder));

	if (priv->validate_changes)
		g_signal_handlers_disconnect_by_func (G_OBJECT (holder),
						      G_CALLBACK (validate_change_holder_cb), set);
	if (! priv->read_only) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (holder),
						      G_CALLBACK (changed_holder_cb), set);
		g_signal_handlers_disconnect_by_func (G_OBJECT (holder),
						      G_CALLBACK (source_changed_holder_cb), set);
	}
	g_signal_handlers_disconnect_by_func (holder,
					      G_CALLBACK (holder_notify_cb), set);
	g_signal_handlers_disconnect_by_func (holder,
					      G_CALLBACK (holder_to_default_cb), set);

	/* now destroy the GdaSetNode and the GdaSetSource if necessary */
	node = gda_set_get_node (set, holder);
	g_assert (node);
	model = gda_set_node_get_data_model (node);
	if (GDA_IS_DATA_MODEL (model)) {
		GdaSetSource *source;
		GSList *nodes;

		source = gda_set_get_source_for_model (set, model);
		g_assert (source);
		nodes = gda_set_source_get_nodes (source);
		g_assert (nodes);
		if (! nodes->next)
			set_remove_source (set, source);
	}
	set_remove_node (set, node);

	priv->holders = g_slist_remove (priv->holders, holder);
	g_hash_table_remove (priv->holders_hash, gda_holder_get_id (holder));
	if (priv->holders_array) {
		g_ptr_array_free (priv->holders_array, TRUE);
		priv->holders_array = NULL;
	}
	g_object_unref (G_OBJECT (holder));
}

static void
source_changed_holder_cb (G_GNUC_UNUSED GdaHolder *holder, GdaSet *set)
{
	compute_public_data (set);
}


static GError *
validate_change_holder_cb (GdaHolder *holder, const GValue *value, GdaSet *set)
{
  g_return_val_if_fail (GDA_IS_SET (set), NULL);
  GdaSetPrivate *priv = gda_set_get_instance_private (set);
	/* signal the holder validate-change */
	GError *error = NULL;
	if (priv->read_only)
		g_set_error (&error, GDA_SET_ERROR, GDA_SET_READ_ONLY_ERROR, "%s", _("Data set does not allow modifications"));
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
group_free (GdaSetGroup *group, G_GNUC_UNUSED gpointer data)
{
	gda_set_group_free (group);
}

static void
gda_set_dispose (GObject *object)
{
	GdaSet *set;
	GSList *list;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_SET (object));

	set = GDA_SET (object);

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	/* free the holders list */
	if (priv->holders) {
		for (list = priv->holders; list; list = list->next) {
			if (priv->validate_changes)
				g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
								      G_CALLBACK (validate_change_holder_cb), set);
			if (! priv->read_only) {
				g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
								      G_CALLBACK (changed_holder_cb), set);
				g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
								      G_CALLBACK (source_changed_holder_cb), set);
			}
			g_object_unref (list->data);
		}
		g_slist_free (priv->holders);
	}
	if (priv->holders_hash) {
		g_hash_table_destroy (priv->holders_hash);
		priv->holders_hash = NULL;
	}
	if (priv->holders_array) {
		g_ptr_array_free (priv->holders_array, TRUE);
		priv->holders_array = NULL;
	}

	/* free the nodes if there are some */
	while (priv->nodes_list)
		set_remove_node (set, GDA_SET_NODE (priv->nodes_list->data));
	while (priv->sources_list)
		set_remove_source (set, GDA_SET_SOURCE (priv->sources_list->data));

	g_slist_foreach (priv->groups_list, (GFunc) group_free, NULL);
	g_slist_free (priv->groups_list);
	priv->groups_list = NULL;

	/* parent class */
	G_OBJECT_CLASS(gda_set_parent_class)->dispose (object);
}

static void
gda_set_finalize (GObject *object)
{
	GdaSet *set;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_SET (object));

	set = GDA_SET (object);

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	if (priv) {
		g_free (priv->id);
		g_free (priv->name);
		g_free (priv->descr);
	}

	/* parent class */
	G_OBJECT_CLASS(gda_set_parent_class)->finalize (object);
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

  g_return_if_fail (GDA_IS_SET (set));
  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	/*
	 * Get rid of all the previous structures
	 */
	while (priv->nodes_list)
		set_remove_node (set, GDA_SET_NODE (priv->nodes_list->data));
	while (priv->sources_list)
		set_remove_source (set, GDA_SET_SOURCE (priv->sources_list->data));

	g_slist_foreach (priv->groups_list, (GFunc) group_free, NULL);
	g_slist_free (priv->groups_list);
	priv->groups_list = NULL;

	/*
	 * Creation of the GdaSetNode structures
	 */
	for (list = priv->holders; list; list = list->next) {
		GdaHolder *holder = GDA_HOLDER (list->data);
		gint col;
		node = gda_set_node_new (holder);
		gda_set_node_set_data_model (node, gda_holder_get_source_model (holder, &col));
		gda_set_node_set_source_column (node, col);
		priv->nodes_list = g_slist_prepend (priv->nodes_list, node);
	}
	priv->nodes_list = g_slist_reverse (priv->nodes_list);

	/*
	 * Creation of the GdaSetSource and GdaSetGroup structures 
	 */
	for (list = priv->nodes_list; list;list = list->next) {
		node = GDA_SET_NODE (list->data);
		
		/* source */
		source = NULL;
		if (gda_set_node_get_data_model (node)) {
			source = gda_set_get_source_for_model (set, gda_set_node_get_data_model (node));
			if (source)
				gda_set_source_add_node (source, node);
			else {
				source = gda_set_source_new (gda_set_node_get_data_model (node));
				gda_set_source_add_node (source, node);
				priv->sources_list = g_slist_prepend (priv->sources_list, source);
			}
		}

		/* group */
		group = NULL;
		if (gda_set_node_get_data_model (node) && groups)
			group = g_hash_table_lookup (groups, gda_set_node_get_data_model (node));
		if (group) 
			gda_set_group_add_node (group, node);
		else {
			group = gda_set_group_new (node);
			gda_set_group_set_source (group, source);
			priv->groups_list = g_slist_prepend (priv->groups_list, group);
			if (gda_set_node_get_data_model (node)) {
				if (!groups)
					groups = g_hash_table_new (NULL, NULL); /* key = source model, 
                                                               value = GdaSetGroup */
				g_hash_table_insert (groups, gda_set_node_get_data_model (node), group);
			}
		}		
	}
	priv->groups_list = g_slist_reverse (priv->groups_list);
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
 * @holder: (transfer none): a #GdaHolder object
 *
 * Adds @holder to the list of holders managed within @set.
 *
 * NOTE: if @set already has a #GdaHolder with the same ID as @holder, then @holder
 * will not be added to the set (even if @holder's type or value is not the same as the
 * one already in @set).
 *
 * Returns: %TRUE if @holder has been added to @set (and FALSE if it has not been added because there is another #GdaHolder
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

static void
holder_to_default_cb (GdaHolder *holder, GdaSet *dataset) {
	g_signal_emit (G_OBJECT (dataset), gda_set_signals[HOLDER_ATTR_CHANGED], 0, holder,
	              "to-default", gda_holder_get_value (holder));
}

static void
holder_notify_cb (GdaHolder *holder, GParamSpec *pspec, GdaSet *dataset)
{
	g_return_if_fail (GDA_IS_SET (dataset));
	GType gtype;
	gtype = gda_holder_get_g_type (holder);
	if (!strcmp (pspec->name, "g-type")) {
		if (gtype == GDA_TYPE_NULL) {
			return;
		}
		g_signal_emit (dataset, gda_set_signals[HOLDER_TYPE_SET], 0, holder);
	}
	else if (!strcmp (pspec->name, "name")) {
		GValue *name = gda_value_new (G_TYPE_STRING);
		g_object_get_property (G_OBJECT (dataset), "name", name);
		g_signal_emit (G_OBJECT (dataset), gda_set_signals[HOLDER_ATTR_CHANGED], 0, holder,
				     "name", name);
		gda_value_free (name);
	}
	else if (!strcmp (pspec->name, "description")) {
		GValue *desc = gda_value_new (G_TYPE_STRING);
		g_object_get_property (G_OBJECT (dataset), "description", desc);
		g_signal_emit (G_OBJECT (dataset), gda_set_signals[HOLDER_ATTR_CHANGED], 0, holder,
				     "description", desc);
		gda_value_free (desc);
	}
	else if (!strcmp (pspec->name, "plugin")) {
		GValue *plugin = gda_value_new (G_TYPE_STRING);
		g_object_get_property (G_OBJECT (holder), "plugin", plugin);
		g_signal_emit (G_OBJECT (dataset), gda_set_signals[HOLDER_ATTR_CHANGED], 0, holder,
				     "plugin", plugin);
		gda_value_free (plugin);
	}
}

static gboolean
gda_set_real_add_holder (GdaSet *set, GdaHolder *holder)
{
	GdaHolder *similar;
	const gchar *hid;

  g_return_val_if_fail (GDA_IS_SET (set), FALSE);
  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	/*
	 * try to find a similar holder in the priv->holders:
	 * a holder B is similar to a holder A if it has the same ID
	 */
	hid = gda_holder_get_id (holder);
	if (hid == NULL) {
		g_warning (_("GdaHolder needs to have an ID"));
		return FALSE;
	}

	similar = (GdaHolder*) g_hash_table_lookup (priv->holders_hash, hid);
	if (!similar) {
		/* really add @holder to the set */
		priv->holders = g_slist_append (priv->holders, holder);
		g_hash_table_insert (priv->holders_hash, (gchar*) hid, holder);
		if (priv->holders_array) {
			g_ptr_array_free (priv->holders_array, TRUE);
			priv->holders_array = NULL;
		}
		g_object_ref (holder);
		if (priv->validate_changes)
			g_signal_connect (G_OBJECT (holder), "validate-change",
					  G_CALLBACK (validate_change_holder_cb), set);
		if (! priv->read_only) {
			g_signal_connect (G_OBJECT (holder), "changed",
					  G_CALLBACK (changed_holder_cb), set);
			g_signal_connect (G_OBJECT (holder), "source-changed",
					  G_CALLBACK (source_changed_holder_cb), set);
		}
		g_signal_connect (G_OBJECT (holder), "notify",
				  G_CALLBACK (holder_notify_cb), set);
		g_signal_connect (G_OBJECT (holder), "to-default",
				  G_CALLBACK (holder_to_default_cb), set);
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
  GdaSetPrivate *priv = gda_set_get_instance_private (set_to_merge);

	for (holders = priv->holders; holders; holders = holders->next)
		gda_set_real_add_holder (set, GDA_HOLDER (holders->data));
	compute_public_data (set);
}

static void
set_remove_node (GdaSet *set, GdaSetNode *node)
{
  g_return_if_fail (GDA_IS_SET (set));
  GdaSetPrivate *priv = gda_set_get_instance_private (set);
	gda_set_node_free (node);
	priv->nodes_list = g_slist_remove (priv->nodes_list, node);
}

static void
set_remove_source (GdaSet *set, GdaSetSource *source)
{
  g_return_if_fail (GDA_IS_SET (set));
  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	g_return_if_fail (g_slist_find (priv->sources_list, source));
	gda_set_source_free (source);
	priv->sources_list = g_slist_remove (priv->sources_list, source);
}

/**
 * gda_set_is_valid:
 * @set: a #GdaSet object
 * @error: (nullable): a place to store validation errors, or %NULL
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

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	g_return_val_if_fail (priv, FALSE);

	for (holders = priv->holders; holders; holders = holders->next) {
		if (!gda_holder_is_valid (GDA_HOLDER (holders->data))) {
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

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	return (GdaHolder *) g_hash_table_lookup (priv->holders_hash, holder_id);
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
	g_return_val_if_fail (pos >= 0, NULL);
  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	if (priv->holders_array == NULL) {
		GSList *list;
		priv->holders_array = g_ptr_array_new_full (g_slist_length (priv->holders),
		                                           (GDestroyNotify) g_object_unref);
		for (list = priv->holders; list; list = list->next) {
			g_ptr_array_insert (priv->holders_array, -1,
			                    g_object_ref ((GObject*) list->data));
		}
	}
	if ((guint)pos >= priv->holders_array->len)
		return NULL;
	else
		return g_ptr_array_index (priv->holders_array, pos);
}

/**
 * gda_set_get_holders:
 *
 * Returns: (transfer none) (element-type Gda.Holder): a list of #GdaHolder objects in the set
 */
GSList*
gda_set_get_holders (GdaSet *set) {
  g_return_val_if_fail (set != NULL, NULL);
	g_return_val_if_fail (GDA_IS_SET(set), NULL);

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	return priv->holders;
}
/**
 * gda_set_get_nodes:
 *
 * Returns: (transfer none) (element-type Gda.SetNode): a list of #GdaSetNode objects in the set
 */
GSList*
gda_set_get_nodes (GdaSet *set) {
	g_return_val_if_fail (GDA_IS_SET(set), NULL);

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	return priv->nodes_list;
}

/**
 * gda_set_get_sources:
 *
 * Returns: (transfer none) (element-type Gda.SetSource): a list of #GdaSetSource objects in the set
 */
GSList*
gda_set_get_sources (GdaSet *set) {
	g_return_val_if_fail (GDA_IS_SET(set), NULL);

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	return priv->sources_list;
}

/**
 * gda_set_get_groups:
 *
 * Returns: (transfer none) (element-type Gda.SetGroup): a list of #GdaSetGroup objects in the set
 */
GSList*
gda_set_get_groups (GdaSet *set) {
	g_return_val_if_fail (GDA_IS_SET(set), NULL);

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	return priv->groups_list;
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

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	g_return_val_if_fail (priv, NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	/* FIXME: May is better to use holder's hash for better performance */
	g_return_val_if_fail (g_slist_find (priv->holders, holder), NULL);

	for (list = priv->nodes_list; list && !retval; list = list->next) {
		GdaHolder *node_holder;
		retval = GDA_SET_NODE (list->data);
		node_holder = gda_set_node_get_holder (retval);
		if (node_holder == holder) /* FIXME: May is better to compare holders ID */
			break;
		else
			retval = NULL;
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
	GdaDataModel *node_model;

	node = gda_set_get_node (set, holder);
	node_model = gda_set_node_get_data_model (node);
	if (node && GDA_IS_DATA_MODEL (node_model))
		return gda_set_get_source_for_model (set, node_model);
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
	GdaSetNode *node;
	GdaSetGroup *retval = NULL;
	GSList *list;

	g_return_val_if_fail (GDA_IS_SET (set), NULL);

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	g_return_val_if_fail (priv, NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	g_return_val_if_fail (g_slist_find (priv->holders, holder), NULL);

	for (list = priv->groups_list; list; list = list->next) {
		retval = GDA_SET_GROUP (list->data);
		GSList *sublist;
		for (sublist = gda_set_group_get_nodes (retval); sublist; sublist = sublist->next) {
			node = GDA_SET_NODE (sublist->data);
			if (node) {
				GdaHolder *node_holder;
				node_holder = gda_set_node_get_holder (node);
				if (node_holder == holder) /* FIXME: May is better to compare holders ID */
					break;
			}
		}
		if (sublist)
			break;
	}
	return list ? retval : NULL;
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
	GdaDataModel *source_model;
	GSList *list;

	g_return_val_if_fail (GDA_IS_SET (set), NULL);

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	g_return_val_if_fail (priv, NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	list = priv->sources_list;
	while (list && !retval) {
		retval = GDA_SET_SOURCE (list->data);
		source_model = gda_set_source_get_data_model (retval);
		if (GDA_IS_DATA_MODEL (source_model)) {
			if (source_model == model)
				break;
			else 
				retval = NULL;
		}
		list = g_slist_next (list);
	}

	return retval;
}

/**
 * gda_set_replace_source_model:
 * @set: a #GdaSet object
 * @source: a pointer to a #GdaSetSource in @set
 * @model: a #GdaDataModel
 *
 * Replaces @source->data_model with @model, which must have the same
 * characteristics as @source->data_model (same column types)
 *
 * Also for each #GdaHolder for which @source->data_model is a source model,
 * this method calls gda_holder_set_source_model() with @model to replace
 * the source by the new model
 *
 * Since: 4.2
 */
void
gda_set_replace_source_model (GdaSet *set, GdaSetSource *source, GdaDataModel *model)
{
	GdaDataModel *source_model;
	
	g_return_if_fail (GDA_IS_SET (set));
	g_return_if_fail (source);

  GdaSetPrivate *priv = gda_set_get_instance_private (set);

	g_return_if_fail (g_slist_find (priv->sources_list, source));
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	
	/* compare models */
	gint ncols, i;

	source_model = gda_set_source_get_data_model (source);
	if (GDA_IS_DATA_MODEL (source_model)) {
		ncols = gda_data_model_get_n_columns (source_model);
		/* FIXME: This way to compare two Data Models could be useful as a function
		 * gda_data_model_compare (GdaDataModel)
		 **/
		if (ncols != gda_data_model_get_n_columns (model)) {
			g_warning (_("Replacing data model must have the same characteristics as the "
					 "data model it replaces"));
			return;
		}
		for (i = 0; i < ncols; i++) {
			GdaColumn *c1, *c2;
			GType t1, t2;
			c1 = gda_data_model_describe_column (source->data_model, i);
			c2 = gda_data_model_describe_column (model, i);
			t1 = gda_column_get_g_type (c1);
			t2 = gda_column_get_g_type (c2);

			if ((t1 != GDA_TYPE_NULL) && (t2 != GDA_TYPE_NULL) && (t1 != t2)) {
				g_warning (_("Replacing data model must have the same characteristics as the "
						 "data model it replaces"));
				return;
			}
		}
	}

	/* actually swap the models */
	GSList *list;
	gda_set_source_set_data_model (source, model);
	for (list = gda_set_source_get_nodes (source); list; list = list->next) {
		GdaSetNode *node = GDA_SET_NODE (list->data);
		gda_set_node_set_data_model (node, model);
		g_signal_handlers_block_by_func (G_OBJECT (node->holder),
						 G_CALLBACK (source_changed_holder_cb), set);
		gda_holder_set_source_model (GDA_HOLDER (node->holder), model, node->source_column,
					     NULL);
		g_signal_handlers_unblock_by_func (G_OBJECT (node->holder),
						   G_CALLBACK (source_changed_holder_cb), set);

	}
#ifdef GDA_DEBUG_signal
	g_print (">> 'SOURCE_MODEL_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (set), gda_set_signals[SOURCE_MODEL_CHANGED], 0, source);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'SOURCE_MODEL_CHANGED' from %s\n", __FUNCTION__);
#endif
}

#ifdef GDA_DEBUG_NO
static void holder_dump (GdaHolder *holder);
static void set_node_dump (GdaSetNode *node);
static void set_source_dump (GdaSetSource *source);
static void set_group_dump (GdaSetGroup *group);

static void
holder_dump (GdaHolder *holder)
{
	g_print ("  GdaHolder %p (%s)\n", holder, holder ? gda_holder_get_id (holder) : "---");
}

static void
set_source_dump (GdaSetSource *source)
{
	g_print ("  GdaSetSource %p\n", source);
	if (source) {
		g_print ("    - data_model: %p\n", source->data_model);
		GSList *list;
		for (list = source->nodes; list; list = list->next)
			g_print ("    - node: %p\n", list->data);
	}
}

static void
set_group_dump (GdaSetGroup *group)
{
	g_print ("  GdaSetGroup %p\n", group);
	if (group) {
		GSList *list;
		for (list = gda_set_group_get_nodes (group); list; list = list->next)
			g_print ("    - node: %p\n", list->data);
		g_print ("    - GdaSetSource: %p\n", group->nodes_source);
	}
}

static void
set_node_dump (GdaSetNode *node)
{
	g_print ("  GdaSetNode: %p\n", node);
	g_print ("   - holder: %p (%s)\n", node->holder, node->holder ? gda_holder_get_id (node->holder) : "ERROR : no GdaHolder!");
	g_print ("   - source_model: %p\n", node->source_model);
	g_print ("   - source_column: %d\n", node->source_column);
}

void
gda_set_dump (GdaSet *set)
{
	g_print ("=== GdaSet %p ===\n", set);
	g_slist_foreach (priv->holders, (GFunc) holder_dump, NULL);
	g_slist_foreach (priv->nodes_list, (GFunc) set_node_dump, NULL);
	g_slist_foreach (priv->sources_list, (GFunc) set_source_dump, NULL);
	g_slist_foreach (priv->groups_list, (GFunc) set_group_dump, NULL);
	g_print ("=== GdaSet %p END ===\n", set);
}
#endif
