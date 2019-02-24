/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2013, 2018 Daniel Espinosa <esodan@gmail.com>
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

#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include "gdaui-entry-combo.h"
#include "gdaui-combo.h"
#include "gdaui-data-store.h"
#include <libgda/gda-debug-macros.h>

static void gdaui_entry_combo_dispose (GObject *object);

static void gdaui_entry_combo_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void gdaui_entry_combo_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec);

static void          choose_auto_default_value (GdauiEntryCombo *combo);
static void          combo_contents_changed_cb (GdauiCombo *entry, GdauiEntryCombo *combo);

static void          gdaui_entry_combo_emit_signal (GdauiEntryCombo *combo);
static void          real_combo_block_signals (GdauiEntryCombo *wid);
static void          real_combo_unblock_signals (GdauiEntryCombo *wid);

/* GdauiDataEntry interface (value must be a GDA_TYPE_LIST) */
static void            gdaui_entry_combo_data_entry_init   (GdauiDataEntryInterface *iface);
static void            gdaui_entry_combo_set_value         (GdauiDataEntry *de, const GValue * value);
static GValue         *gdaui_entry_combo_get_value         (GdauiDataEntry *de);
static void            gdaui_entry_combo_set_ref_value     (GdauiDataEntry *de, const GValue * value);
static const GValue   *gdaui_entry_combo_get_ref_value     (GdauiDataEntry *de);
static void            gdaui_entry_combo_set_value_default (GdauiDataEntry *de, const GValue * value);
static void            gdaui_entry_combo_set_attributes    (GdauiDataEntry *de, guint attrs, guint mask);
static GdaValueAttribute gdaui_entry_combo_get_attributes    (GdauiDataEntry *de);
static void            gdaui_entry_combo_grab_focus        (GdauiDataEntry *de);
static void            gdaui_entry_combo_set_unknown_color (GdauiDataEntry *de, gdouble red, gdouble green,
							    gdouble blue, gdouble alpha);

static void           _gdaui_entry_combo_construct(GdauiEntryCombo* combo, 
						   GdauiSet *paramlist, GdauiSetSource *source);

/* properties */
enum {
	PROP_0,
	PROP_SET_DEFAULT_IF_INVALID
};

/* ComboNode structures: there is one such structure for each GdaHolder in the GdaSetNode being
 * used.
 */
typedef struct {
	GdaSetNode   *node;
	GValue       *value;
	GValue       *value_orig;
	GValue       *value_default;
} ComboNode;
#define COMBO_NODE(x) ((ComboNode*)(x))

/* Private structure */
typedef struct {
        GtkWidget              *combo_entry;
	GSList                 *combo_nodes; /* list of ComboNode structures */

	GdauiSet               *paramlist;
	GdauiSetSource         *source;	
	
	gboolean                data_valid;
	gboolean                null_forced;
	gboolean                default_forced;
	gboolean                invalid;
	gboolean                null_possible;
	gboolean                default_possible;

	gboolean                set_default_if_invalid; /* use first entry when provided value is not found ? */
} GdauiEntryComboPrivate;

G_DEFINE_TYPE_WITH_CODE (GdauiEntryCombo, gdaui_entry_combo, GDAUI_TYPE_ENTRY_SHELL,
                         G_ADD_PRIVATE (GdauiEntryCombo)
                         G_IMPLEMENT_INTERFACE (GDAUI_TYPE_DATA_ENTRY, gdaui_entry_combo_data_entry_init))

static void
gdaui_entry_combo_data_entry_init (GdauiDataEntryInterface *iface)
{
        iface->set_value_type = NULL;
        iface->get_value_type = NULL;
        iface->set_value = gdaui_entry_combo_set_value;
        iface->get_value = gdaui_entry_combo_get_value;
        iface->set_ref_value = gdaui_entry_combo_set_ref_value;
        iface->get_ref_value = gdaui_entry_combo_get_ref_value;
        iface->set_value_default = gdaui_entry_combo_set_value_default;
        iface->set_attributes = gdaui_entry_combo_set_attributes;
        iface->get_attributes = gdaui_entry_combo_get_attributes;
        iface->get_handler = NULL;
        iface->grab_focus = gdaui_entry_combo_grab_focus;
        iface->set_unknown_color = gdaui_entry_combo_set_unknown_color;
}


static void
gdaui_entry_combo_class_init (GdauiEntryComboClass *class)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (class);
        object_class->dispose = gdaui_entry_combo_dispose;

        /* Properties */
        object_class->set_property = gdaui_entry_combo_set_property;
        object_class->get_property = gdaui_entry_combo_get_property;
        g_object_class_install_property (object_class, PROP_SET_DEFAULT_IF_INVALID,
                                         g_param_spec_boolean ("set-default-if-invalid", NULL, NULL, FALSE,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
real_combo_block_signals (GdauiEntryCombo *combo)
{
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
	g_signal_handlers_block_by_func (G_OBJECT (priv->combo_entry),
					 G_CALLBACK (combo_contents_changed_cb), combo);
}

static void
real_combo_unblock_signals (GdauiEntryCombo *combo)
{
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
	g_signal_handlers_unblock_by_func (G_OBJECT (priv->combo_entry),
					   G_CALLBACK (combo_contents_changed_cb), combo);
}


static void
gdaui_entry_combo_emit_signal (GdauiEntryCombo *combo)
{
	g_signal_emit_by_name (G_OBJECT (combo), "contents-modified");
}

static void
gdaui_entry_combo_init (GdauiEntryCombo *combo)
{
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
	priv->combo_nodes = NULL;
	priv->set_default_if_invalid = FALSE;
	priv->combo_entry = NULL;
	priv->data_valid = FALSE;
	priv->invalid = FALSE;
	priv->null_forced = FALSE;
	priv->default_forced = FALSE;
	priv->null_possible = TRUE;
	priv->default_possible = FALSE;

	priv->paramlist = NULL;
	priv->source = NULL;
}

/**
 * gdaui_entry_combo_new:
 * @paramlist: a #GdauiSet object
 * @source: a #GdauiSetSource structure, part of @paramlist
 *
 * Creates a new #GdauiEntryCombo widget. The widget is a combo box which displays a
 * selectable list of items (the items come from the 'source->data_model' data model)
 *
 * The widget allows the value setting of one or more #GdaHolder objects
 * (one for each 'source->nodes') while proposing potentially "more readable" choices.
 * 
 * Returns: (transfer full): the new widget
 */
GtkWidget *
gdaui_entry_combo_new (GdauiSet *paramlist, GdauiSetSource *source)
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_ENTRY_COMBO, NULL);
  
	_gdaui_entry_combo_construct (GDAUI_ENTRY_COMBO (obj), paramlist, source);

	return GTK_WIDGET (obj);
}

static void
uiset_source_model_changed_cb (G_GNUC_UNUSED GdauiSet *paramlist, GdauiSetSource *source, GdauiEntryCombo* combo)
{
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
	if (source == priv->source) {
		GSList *list, *values = NULL;
		GdaSetSource *s;
		s = gdaui_set_source_get_source (source);
		for (list = gda_set_source_get_nodes (s); list; list = list->next) {
			const GValue *cvalue;
			cvalue = gda_holder_get_value (gda_set_node_get_holder (GDA_SET_NODE (list->data)));
			values = g_slist_append (values, (GValue *) cvalue);
		}
		gdaui_combo_set_data (GDAUI_COMBO (priv->combo_entry),
				       gda_set_source_get_data_model (s),
				       gdaui_set_source_get_shown_n_cols (priv->source),
				       gdaui_set_source_get_shown_columns (priv->source));
		_gdaui_combo_set_selected_ext (GDAUI_COMBO (priv->combo_entry), values, NULL);
		g_slist_free (values);
		gdaui_combo_add_null (GDAUI_COMBO (priv->combo_entry), priv->null_possible);
	}
}

/*
 * _gdaui_entry_combo_construct
 * @combo: a #GdauiEntryCombo object to be construced
 * @paramlist: a #GdaSet object
 * @source: a #GdaSetSource structure, part of @paramlist
 *
 * 
 * TODO: This is just a work-around for language bindings. Ideally we would use construction
 * properties instead.
 */
void _gdaui_entry_combo_construct (GdauiEntryCombo* combo, GdauiSet *paramlist, GdauiSetSource *source)
{
	GSList *list;
	GSList *values;
	GtkWidget *entry;
	gboolean null_possible;

	g_return_if_fail (GDAUI_IS_SET (paramlist));
	g_return_if_fail (source);
	g_return_if_fail (g_slist_find (gdaui_set_get_sources (paramlist), source));
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
  
	priv->paramlist = paramlist;
	priv->source = source;
	g_object_ref (G_OBJECT (paramlist));
	g_signal_connect (paramlist, "source-model-changed",
			  G_CALLBACK (uiset_source_model_changed_cb), combo);

	/* create the ComboNode structures, 
	 * and use the values provided by the parameters to display the correct row */
	null_possible = TRUE;
	values = NULL;
	for (list = gda_set_source_get_nodes (gdaui_set_source_get_source (source)); list; list = list->next) {
		ComboNode *cnode = g_new0 (ComboNode, 1);
		
		cnode->node = GDA_SET_NODE (list->data);
		cnode->value = NULL;
		priv->combo_nodes = g_slist_append (priv->combo_nodes, cnode);

		values = g_slist_append (values, (GValue *) gda_holder_get_value (gda_set_node_get_holder (cnode->node)));
		if (gda_holder_get_not_null (gda_set_node_get_holder (cnode->node)))
			null_possible = FALSE;
	}
	priv->null_possible = null_possible;

	/* create the combo box itself */
	entry = gdaui_combo_new_with_model (gda_set_source_get_data_model (gdaui_set_source_get_source (source)), 
					    gdaui_set_source_get_shown_n_cols (priv->source),
					    gdaui_set_source_get_shown_columns (priv->source));
	g_object_set (G_OBJECT (entry), "as-list", TRUE, NULL);

	gdaui_entry_shell_pack_entry (GDAUI_ENTRY_SHELL (combo), entry);
	gtk_widget_show (entry);
	priv->combo_entry = entry;

	if (values) {
		_gdaui_combo_set_selected_ext (GDAUI_COMBO (entry), values, NULL);
		gdaui_entry_combo_set_reference_values (combo, values);
		g_slist_free (values);
	}

	gdaui_combo_add_null (GDAUI_COMBO (entry), priv->null_possible);

	priv->data_valid = priv->null_possible ? TRUE : FALSE;

	combo_contents_changed_cb ((GdauiCombo*) entry, combo);
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (combo_contents_changed_cb), combo);
}

static void
choose_auto_default_value (GdauiEntryCombo *combo)
{
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
	gint pos = 0;
	if (priv->null_possible)
		pos ++;
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_entry), pos);
}

static void
gdaui_entry_combo_dispose (GObject *object)
{
	GdauiEntryCombo *combo;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_COMBO (object));

	combo = GDAUI_ENTRY_COMBO (object);
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

	if (priv->paramlist) {
		g_signal_handlers_disconnect_by_func (priv->paramlist,
						      G_CALLBACK (uiset_source_model_changed_cb),
						      combo);
		g_clear_object (&priv->paramlist);
	}

	if (priv->combo_nodes) {
		GSList *list;
		for (list = priv->combo_nodes; list; list = list->next) {
			ComboNode *node = COMBO_NODE (list->data);

			gda_value_free (node->value);
			gda_value_free (node->value_orig);
			gda_value_free (node->value_default);
			g_free (node);
		}
		g_slist_free (priv->combo_nodes);
		priv->combo_nodes = NULL;
	}

	/* for the parent class */
	G_OBJECT_CLASS (gdaui_entry_combo_parent_class)->dispose (object);
}

static void 
gdaui_entry_combo_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
	GdauiEntryCombo *combo = GDAUI_ENTRY_COMBO (object);
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
	if (priv) {
		switch (param_id) {
		case PROP_SET_DEFAULT_IF_INVALID:
			if (priv->set_default_if_invalid != g_value_get_boolean (value)) {
				guint attrs;

				priv->set_default_if_invalid = g_value_get_boolean (value);
				attrs = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (combo));

				if (priv->set_default_if_invalid && (attrs & GDA_VALUE_ATTR_DATA_NON_VALID))
					choose_auto_default_value (combo);
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gdaui_entry_combo_get_property (GObject *object,
				guint param_id,
				GValue *value,
				GParamSpec *pspec)
{
	GdauiEntryCombo *combo = GDAUI_ENTRY_COMBO (object);
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
	if (priv) {
		switch (param_id) {
		case PROP_SET_DEFAULT_IF_INVALID:
			g_value_set_boolean (value, priv->set_default_if_invalid);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
combo_contents_changed_cb (G_GNUC_UNUSED GdauiCombo *entry, GdauiEntryCombo *combo)
{
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
	if (gdaui_combo_is_null_selected (GDAUI_COMBO (priv->combo_entry))) /* Set to NULL? */ {
		gdaui_entry_combo_set_values (combo, NULL);
		gdaui_entry_combo_emit_signal (combo);
	}
	else {
		GtkTreeIter iter;
		GSList *list;
		GtkTreeModel *model;

		priv->null_forced = FALSE;
		priv->default_forced = FALSE;
		priv->data_valid = TRUE;
			
		/* updating the node's values */
		g_assert (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->combo_entry), &iter));
		
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo_entry));
		for (list = priv->combo_nodes; list; list = list->next) {
			ComboNode *node = COMBO_NODE (list->data);

			gda_value_free (node->value);
			gtk_tree_model_get (model, &iter, gda_set_node_get_source_column (node->node), &(node->value), -1);
			if (node->value)
				node->value = gda_value_copy (node->value);
			/*g_print ("%s (%p): Set Combo Node value to %s (%p)\n", __FUNCTION__, combo, 
			  node->value ? gda_value_stringify (node->value) : "Null", node->value);*/
		}
		
		g_signal_emit_by_name (G_OBJECT (combo), "status-changed");
		gdaui_entry_combo_emit_signal (combo);
	}
}

/**
 * gdaui_entry_combo_set_values:
 * @combo: a #GdauiEntryCombo widet
 * @values: (element-type GValue) (nullable): a list of #GValue values, or %NULL
 *
 * Sets the values of @combo to the specified ones. None of the
 * values provided in the list is modified.
 *
 * @values holds a list of #GValue values, one for each parameter that is present in the @node argument
 * of the gdaui_entry_combo_new() function which created @combo.
 *
 * An error can occur when there is no corresponding value(s) to be displayed
 * for the provided values.
 *
 * If @values is %NULL, then the entry itself is set to NULL;
 *
 * Returns: %TRUE if no error occurred.
 */
gboolean
gdaui_entry_combo_set_values (GdauiEntryCombo *combo, GSList *values)
{
	gboolean err = FALSE;
	gboolean allnull = TRUE;
	GSList *list;

	g_return_val_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo), FALSE);
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

	/* try to determine if all the values are NULL or of type GDA_TYPE_NULL */
	for (list = values; list; list = list->next) {
		if (list->data && (G_VALUE_TYPE ((GValue *)list->data) != GDA_TYPE_NULL)) {
			allnull = FALSE;
			break;
		}
	}

	/* actual values settings */
	if (!allnull) {
		GtkTreeIter iter;
		GtkTreeModel *model;
		GSList *list;

		g_return_val_if_fail (g_slist_length (values) == g_slist_length (priv->combo_nodes), FALSE);
		
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo_entry));
		if (gdaui_data_store_get_iter_from_values (GDAUI_DATA_STORE (model), &iter, 
							   values, gdaui_set_source_get_ref_columns (priv->source))) {
			ComboNode *node;

			real_combo_block_signals (combo);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->combo_entry), &iter);
			real_combo_unblock_signals (combo);

			/* adjusting the values */
			for (list = priv->combo_nodes; list; list = list->next) {
				node = COMBO_NODE (list->data);
				gda_value_free (node->value);
				gtk_tree_model_get (model, &iter, gda_set_node_get_source_column (node->node),
						    &(node->value), -1);
				if (node->value)
					node->value = gda_value_copy (node->value);
			}
			
			priv->null_forced = FALSE;
			priv->default_forced = FALSE;
		}
		else
			/* values not found */
			err = TRUE;
	}
	else  { /* set to NULL */
		GSList *list;
		for (list = priv->combo_nodes; list; list = list->next) {
			gda_value_free (COMBO_NODE (list->data)->value);
			COMBO_NODE (list->data)->value = NULL;
		}

		if (priv->null_possible) {
			real_combo_block_signals (combo);
			gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_entry), -1);
			real_combo_unblock_signals (combo);

			priv->null_forced = TRUE;
			priv->default_forced = FALSE;
		}
		else 
			err = TRUE;
	}

	priv->data_valid = !err;
	g_signal_emit_by_name (G_OBJECT (combo), "status-changed");

	/* notify the status and contents changed */
	gdaui_entry_combo_emit_signal (combo);

	return !err;
}

/**
 * gdaui_entry_combo_get_values:
 * @combo: a #GdauiEntryCombo widet
 *
 * Get the values stored within @combo. The returned values are the ones
 * within @combo, so they must not be freed afterwards, however the returned
 * list has to be freed afterwards.
 *
 * Returns: (transfer container) (element-type GValue): a new list of values
 */
GSList *
gdaui_entry_combo_get_values (GdauiEntryCombo *combo)
{
	GSList *list;
	GSList *retval = NULL;
	g_return_val_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo), NULL);
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

	for (list = priv->combo_nodes; list; list = list->next) {
		ComboNode *node = COMBO_NODE (list->data);		
		retval = g_slist_append (retval, (GValue *) node->value);
	}

	return retval;
}

/**
 * gdaui_entry_combo_get_all_values:
 * @combo: a #GdauiEntryCombo widet
 *
 * Get a list of all the values in @combo's data model's selected row. The list
 * must be freed by the caller.
 *
 * Returns: (transfer container) (element-type GValue): a new list of values
 */
GSList *
gdaui_entry_combo_get_all_values (GdauiEntryCombo *combo)
{
	g_return_val_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo), NULL);
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

	return _gdaui_combo_get_selected_ext (GDAUI_COMBO (priv->combo_entry), 0, NULL);
}

/**
 * gdaui_entry_combo_set_reference_values:
 * @combo: a #GdauiEntryCombo widet
 * @values: (element-type GValue): a list of #GValue values
 *
 * Sets the original values of @combo to the specified ones. None of the
 * values provided in the list is modified.
 */
void
gdaui_entry_combo_set_reference_values (GdauiEntryCombo *combo, GSList *values)
{
	GSList *list;

	g_return_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo));
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

	/* rem: we don't care if the values can be set or not, we just try */
	gdaui_entry_combo_set_values (combo, values);

	/* clean all the orig values */
	for (list = priv->combo_nodes; list; list = list->next) {
		ComboNode *node = COMBO_NODE (list->data);
		if (node->value_orig) {
			gda_value_free (node->value_orig);
			node->value_orig = NULL;
		}
	}

	if (values) {
		GSList *nodes;
		GSList *argptr;
		const GValue *arg_value;
		gboolean equal = TRUE;
		
		g_return_if_fail (g_slist_length (values) == g_slist_length (priv->combo_nodes));
		
		/* 
		 * first make sure the value types are the same as for the data model 
		 */
		for (nodes = priv->combo_nodes, argptr = values;
		     argptr && nodes && equal;
		     nodes = nodes->next, argptr = argptr->next) {
			GdaColumn *attrs;
			GType type = GDA_TYPE_NULL;
			
			attrs = gda_data_model_describe_column (
			                    gda_set_source_get_data_model (gdaui_set_source_get_source (priv->source)),
								gda_set_node_get_source_column (COMBO_NODE (nodes->data)->node));
			arg_value = (GValue*) (argptr->data);
			
			if (arg_value)
				type = G_VALUE_TYPE ((GValue*) arg_value);
			equal = (type == gda_column_get_g_type (attrs));			
		}
		
		/* 
		 * then, actual copy of the values
		 */
		if (equal) {
			for (nodes = priv->combo_nodes, argptr = values;
			     argptr && nodes;
			     nodes = nodes->next, argptr = argptr->next)
				if (argptr->data)
					COMBO_NODE (nodes->data)->value_orig =
						gda_value_copy ((GValue*) (argptr->data));
		} 
	}
}

/**
 * gdaui_entry_combo_get_reference_values:
 * @combo: a #GdauiEntryCombo widet
 *
 * Get the original values stored within @combo. The returned values are the ones
 * within @combo, so they must not be freed afterwards; the list has to be freed afterwards.
 *
 * Returns: (transfer container) (element-type GValue): a new list of values
 */
GSList *
gdaui_entry_combo_get_reference_values (GdauiEntryCombo *combo)
{
	GSList *list;
	GSList *retval = NULL;
	gboolean allnull = TRUE;

	g_return_val_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo), NULL);
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

	for (list = priv->combo_nodes; list; list = list->next) {
		ComboNode *node = COMBO_NODE (list->data);

		if (node->value_orig && 
		    (G_VALUE_TYPE (node->value_orig) != GDA_TYPE_NULL))
			allnull = FALSE;
		
		retval = g_slist_append (retval, node->value_orig);
	}

	if (allnull) {
		g_slist_free (retval);
		retval = NULL;
	}

	return retval;
}

/**
 * gdaui_entry_combo_set_default_values:
 * @combo: a #GdauiEntryCombo widet
 * @values: (element-type GValue): a list of #GValue values
 *
 * Sets the default values of @combo to the specified ones. None of the
 * values provided in the list is modified.
 */
void
gdaui_entry_combo_set_default_values (GdauiEntryCombo *combo, G_GNUC_UNUSED GSList *values)
{
	g_return_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo));
	//GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
	TO_IMPLEMENT;
}


/* 
 * GdauiDataEntry Interface implementation 
 */

static void
gdaui_entry_combo_set_value (GdauiDataEntry *iface, G_GNUC_UNUSED const GValue *value)
{
        g_return_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface));
        //combo = GDAUI_ENTRY_COMBO (iface);
        //GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

        TO_IMPLEMENT;
}

static GValue *
gdaui_entry_combo_get_value (GdauiDataEntry *iface)
{
        g_return_val_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface), NULL);
        //combo = GDAUI_ENTRY_COMBO (iface);
        //GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

        TO_IMPLEMENT;

        return NULL;
}

static void
gdaui_entry_combo_set_ref_value (GdauiDataEntry *iface, G_GNUC_UNUSED const GValue * value)
{
        g_return_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface));
        //combo = GDAUI_ENTRY_COMBO (iface);
        //GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

        TO_IMPLEMENT;
}

static const GValue *
gdaui_entry_combo_get_ref_value (GdauiDataEntry *iface)
{
        g_return_val_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface), NULL);
        //combo = GDAUI_ENTRY_COMBO (iface);
        //GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);
        TO_IMPLEMENT;

        return NULL;
}

static void
gdaui_entry_combo_set_value_default (GdauiDataEntry *iface, G_GNUC_UNUSED const GValue * value)
{
        g_return_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface));
        //combo = GDAUI_ENTRY_COMBO (iface);
        //GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

        TO_IMPLEMENT;
}


static void
gdaui_entry_combo_set_attributes (GdauiDataEntry *iface, guint attrs, guint mask)
{
	GdauiEntryCombo *combo;

	g_return_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface));
	combo = GDAUI_ENTRY_COMBO (iface);
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

	/* Setting to NULL */
	if (mask & GDA_VALUE_ATTR_IS_NULL) {
		if ((mask & GDA_VALUE_ATTR_CAN_BE_NULL) &&
		    !(attrs & GDA_VALUE_ATTR_CAN_BE_NULL))
			g_return_if_reached ();
		if (attrs & GDA_VALUE_ATTR_IS_NULL) {
			gdaui_entry_combo_set_values (combo, NULL);
			
			/* if default is set, see if we can keep it that way */
			if (priv->default_forced) {
				GSList *list;
				gboolean allnull = TRUE;
				for (list = priv->combo_nodes; list; list = list->next) {
					if (COMBO_NODE (list->data)->value_default && 
					    (G_VALUE_TYPE (COMBO_NODE (list->data)->value_default) != 
					     GDA_TYPE_NULL)) {
						allnull = FALSE;
						break;
					}
				}

				if (!allnull)
					priv->default_forced = FALSE;
			}

			gdaui_entry_combo_emit_signal (combo);
			return;
		}
		else {
			priv->null_forced = FALSE;
			gdaui_entry_combo_emit_signal (combo);
		}
	}

	/* Can be NULL ? */
	if (mask & GDA_VALUE_ATTR_CAN_BE_NULL)
		if (priv->null_possible != (gboolean)(attrs & GDA_VALUE_ATTR_CAN_BE_NULL) ? TRUE : FALSE) {
			priv->null_possible = (attrs & GDA_VALUE_ATTR_CAN_BE_NULL) ? TRUE : FALSE;
			gdaui_combo_add_null (GDAUI_COMBO (priv->combo_entry),
						      priv->null_possible);
		}


	/* Setting to DEFAULT */
	if (mask & GDA_VALUE_ATTR_IS_DEFAULT) {
		if ((mask & GDA_VALUE_ATTR_CAN_BE_DEFAULT) &&
		    !(attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT))
			g_return_if_reached ();
		if (attrs & GDA_VALUE_ATTR_IS_DEFAULT) {
			GSList *tmplist = NULL;
			GSList *list;
			
			for (list = priv->combo_nodes; list; list = list->next)
				tmplist = g_slist_append (tmplist, COMBO_NODE (list->data)->value_default);

			gdaui_entry_combo_set_values (combo, tmplist);
			g_slist_free (tmplist);

			/* if NULL is set, see if we can keep it that way */
			if (priv->null_forced) {
				GSList *list;
				gboolean allnull = TRUE;
				for (list = priv->combo_nodes; list; list = list->next) {
					if (COMBO_NODE (list->data)->value_default && 
					    (G_VALUE_TYPE (COMBO_NODE (list->data)->value_default) != 
					     GDA_TYPE_NULL)) {
						allnull = FALSE;
						break;
					}
				}
				
				if (!allnull)
					priv->null_forced = FALSE;
			}

			priv->default_forced = TRUE;
			gdaui_entry_combo_emit_signal (combo);
			return;
		}
		else {
			priv->default_forced = FALSE;
			gdaui_entry_combo_emit_signal (combo);
		}
	}

	/* Can be DEFAULT ? */
	if (mask & GDA_VALUE_ATTR_CAN_BE_DEFAULT)
		priv->default_possible = (attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT) ? TRUE : FALSE;
	
	/* Modified ? */
	if (mask & GDA_VALUE_ATTR_IS_UNCHANGED) {
		if (attrs & GDA_VALUE_ATTR_IS_UNCHANGED) {
			GSList *tmplist = NULL;
			GSList *list;
			
			for (list = priv->combo_nodes; list; list = list->next)
				tmplist = g_slist_append (tmplist, COMBO_NODE (list->data)->value_orig);
				
			gdaui_entry_combo_set_values (combo, tmplist);
			g_slist_free (tmplist);
			priv->default_forced = FALSE;
			gdaui_entry_combo_emit_signal (combo);
		}
	}

	/* invalid data */
	if (mask & GDA_VALUE_ATTR_DATA_NON_VALID)
		priv->invalid = attrs & GDA_VALUE_ATTR_DATA_NON_VALID;

	/* NON WRITABLE attributes */
	if (mask & GDA_VALUE_ATTR_HAS_VALUE_ORIG)
		g_warning ("Having an original value is not a write attribute on GdauiDataEntry!");

	g_signal_emit_by_name (G_OBJECT (combo), "status-changed");
}

static GdaValueAttribute
gdaui_entry_combo_get_attributes (GdauiDataEntry *iface)
{
	GdaValueAttribute retval = 0;
	GdauiEntryCombo *combo;
	GSList *list;
	GSList *list2;
	gboolean isnull = TRUE;
	gboolean isunchanged = TRUE;
	gboolean orig_value_exists = FALSE;

	g_return_val_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface), 0);
	combo = GDAUI_ENTRY_COMBO (iface);
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

	for (list = priv->combo_nodes; list; list = list->next) {
		gboolean changed = FALSE;

		/* NULL? */
		if (COMBO_NODE (list->data)->value &&
		    (G_VALUE_TYPE ((GValue *) COMBO_NODE (list->data)->value) != GDA_TYPE_NULL))
			isnull = FALSE;
		
		/* is unchanged */
		if (COMBO_NODE (list->data)->value_orig) {
			orig_value_exists = TRUE;
			
			if (COMBO_NODE (list->data)->value && 
			    (G_VALUE_TYPE ((GValue *) COMBO_NODE (list->data)->value) == 
			     G_VALUE_TYPE (COMBO_NODE (list->data)->value_orig))) {
				if (G_VALUE_TYPE ((GValue *) COMBO_NODE (list->data)->value) == 
				    GDA_TYPE_NULL) 
					changed = FALSE;
				else {
					if (gda_value_compare ((GValue *) COMBO_NODE (list->data)->value, 
							       COMBO_NODE (list->data)->value_orig))
						changed = TRUE;
				}
			}
			else
				changed = TRUE;
		}
		
		if (changed || 
		    (!orig_value_exists && !isnull))
			isunchanged = FALSE;
	}

	if (isunchanged)
		retval |= GDA_VALUE_ATTR_IS_UNCHANGED;

	if (isnull || priv->null_forced)
		retval |= GDA_VALUE_ATTR_IS_NULL;

	/* can be NULL? */
	if (priv->null_possible)
		retval |= GDA_VALUE_ATTR_CAN_BE_NULL;
	
	/* is default */
	if (priv->default_forced)
		retval |= GDA_VALUE_ATTR_IS_DEFAULT;
	
	/* can be default? */
	if (priv->default_possible)
		retval |= GDA_VALUE_ATTR_CAN_BE_DEFAULT;
	
	/* data valid? */
	if ((! priv->data_valid) || priv->invalid)
		retval |= GDA_VALUE_ATTR_DATA_NON_VALID;
	else {
		GSList *nodes;
		gboolean allnull = TRUE;
		
		for (nodes = priv->combo_nodes; nodes; nodes = nodes->next) {
			ComboNode *node = COMBO_NODE (nodes->data);

			/* all the nodes are NULL ? */
			if (node->value && (G_VALUE_TYPE ((GValue *) node->value) != GDA_TYPE_NULL)) {
				allnull = FALSE;
				break;
			}
		}

		if ((allnull && !priv->null_possible) ||
		    (priv->null_forced && !priv->null_possible))
			retval |= GDA_VALUE_ATTR_DATA_NON_VALID;
	}

	/* has original value? */
	list2 = gdaui_entry_combo_get_reference_values (combo);
	if (list2) {
		retval |= GDA_VALUE_ATTR_HAS_VALUE_ORIG;
		g_slist_free (list2);
	}

	return retval;
}

static void
gdaui_entry_combo_grab_focus (GdauiDataEntry *iface)
{
	GdauiEntryCombo *combo;

	g_return_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface));
	combo = GDAUI_ENTRY_COMBO (iface);
	GdauiEntryComboPrivate *priv = gdaui_entry_combo_get_instance_private (combo);

	if (priv->combo_entry)
		gtk_widget_grab_focus (priv->combo_entry);
}

static void
gdaui_entry_combo_set_unknown_color (GdauiDataEntry *de, gdouble red, gdouble green,
				     gdouble blue, gdouble alpha)
{
	gdaui_entry_shell_set_invalid_color (GDAUI_ENTRY_SHELL (de), red, green, blue, alpha);
}
