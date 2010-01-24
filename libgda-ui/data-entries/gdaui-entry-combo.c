/* gdaui-entry-combo.c
 *
 * Copyright (C) 2003 - 2009 Vivien Malerba <malerba@gnome-db.org>
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

#include <libgda/libgda.h>
#include "gdaui-entry-combo.h"
#include "gdaui-combo.h"
#include "gdaui-data-store.h"

static void gdaui_entry_combo_class_init (GdauiEntryComboClass *class);
static void gdaui_entry_combo_init (GdauiEntryCombo *wid);
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
static void            gdaui_entry_combo_data_entry_init   (GdauiDataEntryIface *iface);
static void            gdaui_entry_combo_set_value         (GdauiDataEntry *de, const GValue * value);
static GValue         *gdaui_entry_combo_get_value         (GdauiDataEntry *de);
static void            gdaui_entry_combo_set_ref_value     (GdauiDataEntry *de, const GValue * value);
static const GValue   *gdaui_entry_combo_get_ref_value     (GdauiDataEntry *de);
static void            gdaui_entry_combo_set_value_default (GdauiDataEntry *de, const GValue * value);
static void            gdaui_entry_combo_set_attributes    (GdauiDataEntry *de, guint attrs, guint mask);
static GdaValueAttribute gdaui_entry_combo_get_attributes    (GdauiDataEntry *de);
static gboolean        gdaui_entry_combo_expand_in_layout  (GdauiDataEntry *de);
static void            gdaui_entry_combo_grab_focus        (GdauiDataEntry *de);

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
	GdaSetNode *node;
	const GValue       *value;    /* we don't own the value, since it belongs to a GdaDataModel => don't free it */
	GValue             *value_orig;
	GValue             *value_default;
} ComboNode;
#define COMBO_NODE(x) ((ComboNode*)(x))

/* Private structure */
struct  _GdauiEntryComboPriv {
        GtkWidget              *combo_entry;
	GSList                 *combo_nodes; /* list of ComboNode structures */

	GdauiSet               *paramlist;
	GdauiSetSource         *source;	
	
	gboolean                data_valid;
	gboolean                null_forced;
	gboolean                default_forced;
	gboolean                null_possible;
	gboolean                default_possible;
	
	gboolean                show_actions;
	gboolean                set_default_if_invalid; /* use first entry when provided value is not found ? */
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gdaui_entry_combo_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryComboClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_combo_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryCombo),
			0,
			(GInstanceInitFunc) gdaui_entry_combo_init
		};		

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gdaui_entry_combo_data_entry_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GDAUI_TYPE_ENTRY_SHELL, "GdauiEntryCombo", &info, 0);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_ENTRY, &data_entry_info);
	}
	return type;
}

static void
gdaui_entry_combo_data_entry_init (GdauiDataEntryIface *iface)
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
        iface->expand_in_layout = gdaui_entry_combo_expand_in_layout;
	iface->grab_focus = gdaui_entry_combo_grab_focus;
}


static void
gdaui_entry_combo_class_init (GdauiEntryComboClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_combo_dispose;

	/* Properties */
        object_class->set_property = gdaui_entry_combo_set_property;
        object_class->get_property = gdaui_entry_combo_get_property;
        g_object_class_install_property (object_class, PROP_SET_DEFAULT_IF_INVALID,
					 g_param_spec_boolean ("set-default-if-invalid", NULL, NULL, FALSE,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	/* RC setting */
#define RC_STRING						\
	"style \"gnomedb\" { GdauiCombo::appears-as-list = 1 }" \
		"class \"GtkComboBox\" style \"gnomedb\""
	/*gtk_rc_parse_string (RC_STRING);*/
}

static void
real_combo_block_signals (GdauiEntryCombo *wid)
{
	g_signal_handlers_block_by_func (G_OBJECT (wid->priv->combo_entry),
					 G_CALLBACK (combo_contents_changed_cb), wid);
}

static void
real_combo_unblock_signals (GdauiEntryCombo *wid)
{
	g_signal_handlers_unblock_by_func (G_OBJECT (wid->priv->combo_entry),
					   G_CALLBACK (combo_contents_changed_cb), wid);
}


static void
gdaui_entry_combo_emit_signal (GdauiEntryCombo *combo)
{
#ifdef debug_signal
	g_print (">> 'CONTENTS_MODIFIED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (combo), "contents-modified");
#ifdef debug_signal
	g_print ("<< 'CONTENTS_MODIFIED' from %s\n", __FUNCTION__);
#endif
}

static void
gdaui_entry_combo_init (GdauiEntryCombo *combo)
{
	/* Private structure */
	combo->priv = g_new0 (GdauiEntryComboPriv, 1);
	combo->priv->combo_nodes = NULL;
	combo->priv->set_default_if_invalid = FALSE;
	combo->priv->combo_entry = NULL;
	combo->priv->data_valid = FALSE;
	combo->priv->null_forced = FALSE;
	combo->priv->default_forced = FALSE;
	combo->priv->null_possible = TRUE;
	combo->priv->default_possible = FALSE;
	combo->priv->show_actions = TRUE;

	combo->priv->paramlist = NULL;
	combo->priv->source = NULL;
}

/**
 * gdaui_entry_combo_new
 * @paramlist: a #GdauiSet object
 * @source: a #GdauiSetSource structure, part of @paramlist
 *
 * Creates a new #GdauiEntryCombo widget. The widget is a combo box which displays a
 * selectable list of items (the items come from the 'source->data_model' data model)
 *
 * The widget allows the value setting of one or more #GdaHolder objects
 * (one for each 'source->nodes') while proposing potentially "more readable" choices.
 * 
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_combo_new (GdauiSet *paramlist, GdauiSetSource *source)
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_ENTRY_COMBO, NULL);
  
	_gdaui_entry_combo_construct (GDAUI_ENTRY_COMBO (obj), paramlist, source);

	return GTK_WIDGET (obj);
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
	g_return_if_fail (g_slist_find (paramlist->sources_list, source));
  
	combo->priv->paramlist = paramlist;
	combo->priv->source = source;
	g_object_ref (G_OBJECT (paramlist));

	/* create the ComboNode structures, 
	 * and use the values provided by the parameters to display the correct row */
	null_possible = TRUE;
	values = NULL;
	for (list = source->source->nodes; list; list = list->next) {
		ComboNode *cnode = g_new0 (ComboNode, 1);
		
		cnode->node = GDA_SET_NODE (list->data);
		cnode->value = gda_holder_get_value (cnode->node->holder);
		combo->priv->combo_nodes = g_slist_append (combo->priv->combo_nodes, cnode);

		values = g_slist_append (values, (GValue *) cnode->value);
		if (gda_holder_get_not_null (cnode->node->holder))
			null_possible = FALSE;
	}
	combo->priv->null_possible = null_possible;

	/* create the combo box itself */
	entry = gdaui_combo_new_with_model (GDA_DATA_MODEL (source->source->data_model), 
					    combo->priv->source->shown_n_cols, 
					    combo->priv->source->shown_cols_index);
	g_object_set (G_OBJECT (entry), "as-list", TRUE, NULL);
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (combo_contents_changed_cb), combo);

	gdaui_entry_shell_pack_entry (GDAUI_ENTRY_SHELL (combo), entry);
	gtk_widget_show (entry);
	combo->priv->combo_entry = entry;

	_gdaui_combo_set_selected_ext (GDAUI_COMBO (entry), values, NULL);
	g_slist_free (values);
	gdaui_combo_add_null (GDAUI_COMBO (entry), combo->priv->null_possible);
}

static void
choose_auto_default_value (GdauiEntryCombo *combo)
{
	gint pos = 0;
	if (combo->priv->null_possible)
		pos ++;
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo->priv->combo_entry), pos);
}

static void
gdaui_entry_combo_dispose (GObject *object)
{
	GdauiEntryCombo *combo;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_COMBO (object));

	combo = GDAUI_ENTRY_COMBO (object);

	if (combo->priv) {
		if (combo->priv->paramlist) 
			g_object_unref (combo->priv->paramlist);

		if (combo->priv->combo_nodes) {
			GSList *list;
			for (list = combo->priv->combo_nodes; list; list = list->next) {
				ComboNode *node = COMBO_NODE (list->data);

				if (node->value)
					node->value = NULL; /* don't free that value since we have not copied it */
				if (node->value_orig)
					gda_value_free (node->value_orig);
				if (node->value_default)
					gda_value_free (node->value_default);
				g_free (node);
			}
			g_slist_free (combo->priv->combo_nodes);
			combo->priv->combo_nodes = NULL;
		}

		g_free (combo->priv);
		combo->priv = NULL;
	}
	
	/* for the parent class */
	parent_class->dispose (object);
}

static void 
gdaui_entry_combo_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
	GdauiEntryCombo *combo = GDAUI_ENTRY_COMBO (object);
	if (combo->priv) {
		switch (param_id) {
		case PROP_SET_DEFAULT_IF_INVALID:
			if (combo->priv->set_default_if_invalid != g_value_get_boolean (value)) {
				guint attrs;

				combo->priv->set_default_if_invalid = g_value_get_boolean (value);
				attrs = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (combo));

				if (combo->priv->set_default_if_invalid && (attrs & GDA_VALUE_ATTR_DATA_NON_VALID)) 
					choose_auto_default_value (combo);
			}
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
	if (combo->priv) {
		switch (param_id) {
		case PROP_SET_DEFAULT_IF_INVALID:
			g_value_set_boolean (value, combo->priv->set_default_if_invalid);
			break;
		}
	}
}

static void
combo_contents_changed_cb (GdauiCombo *entry, GdauiEntryCombo *combo)
{
	if (gdaui_combo_is_null_selected (GDAUI_COMBO (combo->priv->combo_entry))) /* Set to NULL? */ {
		gdaui_entry_combo_set_values (combo, NULL);
		gdaui_entry_combo_emit_signal (combo);
	}
	else {
		GtkTreeIter iter;
		GSList *list;
		GtkTreeModel *model;

		combo->priv->null_forced = FALSE;
		combo->priv->default_forced = FALSE;
		combo->priv->data_valid = TRUE;
			
		/* updating the node's values */
		g_assert (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo->priv->combo_entry), &iter));
		
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo->priv->combo_entry));
		for (list = combo->priv->combo_nodes; list; list = list->next) {
			ComboNode *node = COMBO_NODE (list->data);

			gtk_tree_model_get (model, &iter, node->node->source_column, &(node->value), -1);
			/*g_print ("%s(): Set Combo Node value to %s\n", __FUNCTION__,
			  node->value ? gda_value_stringify (node->value) : "Null");*/
		}
		
		g_signal_emit_by_name (G_OBJECT (combo), "status-changed");
		gdaui_entry_combo_emit_signal (combo);
	}
}

/**
 * gdaui_entry_combo_set_values
 * @combo: a #GdauiEntryCombo widet
 * @values: a list of #GValue values, or %NULL
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
 * Returns: TRUE if no error occurred.
 */
gboolean
gdaui_entry_combo_set_values (GdauiEntryCombo *combo, GSList *values)
{
	gboolean err = FALSE;
	gboolean allnull = TRUE;
	GSList *list;

	g_return_val_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo), FALSE);
	g_return_val_if_fail (combo->priv, FALSE);

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

		g_return_val_if_fail (g_slist_length (values) == g_slist_length (combo->priv->combo_nodes), FALSE);
		
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo->priv->combo_entry));
		if (gdaui_data_store_get_iter_from_values (GDAUI_DATA_STORE (model), &iter, 
							   values, combo->priv->source->ref_cols_index)) {
			ComboNode *node;

			real_combo_block_signals (combo);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo->priv->combo_entry), &iter);
			real_combo_unblock_signals (combo);

			/* adjusting the values */
			for (list = combo->priv->combo_nodes; list; list = list->next) {
				node = COMBO_NODE (list->data);
				gtk_tree_model_get (model, &iter, node->node->source_column,
						    &(node->value), -1);
			}
			
			combo->priv->null_forced = FALSE;
			combo->priv->default_forced = FALSE;
		}
		else
			/* values not found */
			err = TRUE;
	}
	else  { /* set to NULL */
		GSList *list;
		for (list = combo->priv->combo_nodes; list; list = list->next)
			COMBO_NODE (list->data)->value = NULL;

		if (combo->priv->null_possible) {
			real_combo_block_signals (combo);
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo->priv->combo_entry), -1);
			real_combo_unblock_signals (combo);

			combo->priv->null_forced = TRUE;
			combo->priv->default_forced = FALSE;
		}
		else 
			err = TRUE;
	}

	combo->priv->data_valid = !err;
	g_signal_emit_by_name (G_OBJECT (combo), "status-changed");

	if (!err) 
		/* notify the status and contents changed */
		gdaui_entry_combo_emit_signal (combo);

	return !err;
}

/**
 * gdaui_entry_combo_get_values
 * @combo: a #GdauiEntryCombo widet
 *
 * Get the values stored within @combo. The returned values are the ones
 * within @combo, so they must not be freed afterwards, however the returned
 * list has to be freed afterwards.
 *
 * Returns: a new list of values
 */
GSList *
gdaui_entry_combo_get_values (GdauiEntryCombo *combo)
{
	GSList *list;
	GSList *retval = NULL;
	g_return_val_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo), NULL);
	g_return_val_if_fail (combo->priv, NULL);

	for (list = combo->priv->combo_nodes; list; list = list->next) {
		ComboNode *node = COMBO_NODE (list->data);		
		retval = g_slist_append (retval, (GValue *) node->value);
	}

	return retval;
}

/**
 * gdaui_entry_combo_get_all_values
 * @combo: a #GdauiEntryCombo widet
 *
 * Get a list of all the values in @combo's data model's selected row. The list
 * must be freed by the caller.
 *
 * Returns: a new list of values
 */
GSList *
gdaui_entry_combo_get_all_values (GdauiEntryCombo *combo)
{
	g_return_val_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo), NULL);
	g_return_val_if_fail (combo->priv, NULL);

	return _gdaui_combo_get_selected_ext (GDAUI_COMBO (combo->priv->combo_entry), 0, NULL);
}

/**
 * gdaui_entry_combo_set_values_orig
 * @combo: a #GdauiEntryCombo widet
 * @values: a list of #GValue values
 *
 * Sets the original values of @combo to the specified ones. None of the
 * values provided in the list is modified.
 */
void
gdaui_entry_combo_set_values_orig (GdauiEntryCombo *combo, GSList *values)
{
	GSList *list;

	g_return_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo));
	g_return_if_fail (combo->priv);

	/* rem: we don't care if the values can be set or not, we just try */
	gdaui_entry_combo_set_values (combo, values);

	/* clean all the orig values */
	for (list = combo->priv->combo_nodes; list; list = list->next) {
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
		
		g_return_if_fail (g_slist_length (values) == g_slist_length (combo->priv->combo_nodes));
		
		/* 
		 * first make sure the value types are the same as for the data model 
		 */
		for (nodes = combo->priv->combo_nodes, argptr = values;
		     argptr && nodes && equal;
		     nodes = nodes->next, argptr = argptr->next) {
			GdaColumn *attrs;
			GType type = GDA_TYPE_NULL;
			
			attrs = gda_data_model_describe_column (combo->priv->source->source->data_model, 
								COMBO_NODE (nodes->data)->node->source_column);
			arg_value = (GValue*) (argptr->data);
			
			if (arg_value)
				type = G_VALUE_TYPE ((GValue*) arg_value);
			equal = (type == gda_column_get_g_type (attrs));			
		}
		
		/* 
		 * then, actual copy of the values
		 */
		if (equal) {
			for (nodes = combo->priv->combo_nodes, argptr = values;
			     argptr && nodes;
			     nodes = nodes->next, argptr = argptr->next)
				if (argptr->data)
					COMBO_NODE (nodes->data)->value_orig =
						gda_value_copy ((GValue*) (argptr->data));
		} 
	}
}

/**
 * gdaui_entry_combo_get_values_orig
 * @combo: a #GdauiEntryCombo widet
 *
 * Get the original values stored within @combo. The returned values are the ones
 * within @combo, so they must not be freed afterwards; the list has to be freed afterwards.
 *
 * Returns: a new list of values
 */
GSList *
gdaui_entry_combo_get_values_orig (GdauiEntryCombo *combo)
{
	GSList *list;
	GSList *retval = NULL;
	gboolean allnull = TRUE;

	g_return_val_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo), NULL);
	g_return_val_if_fail (combo->priv, NULL);

	for (list = combo->priv->combo_nodes; list; list = list->next) {
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
 * gdaui_entry_combo_set_values_default
 * @combo: a #GdauiEntryCombo widet
 * @values: a list of #GValue values
 *
 * Sets the default values of @combo to the specified ones. None of the
 * values provided in the list is modified.
 */
void
gdaui_entry_combo_set_values_default (GdauiEntryCombo *combo, GSList *values)
{
	g_return_if_fail (combo && GDAUI_IS_ENTRY_COMBO (combo));
	g_return_if_fail (combo->priv);
	TO_IMPLEMENT;
}


/* 
 * GdauiDataEntry Interface implementation 
 */

static void
gdaui_entry_combo_set_value (GdauiDataEntry *iface, const GValue *value)
{
	GdauiEntryCombo *combo;

        g_return_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface));
        combo = GDAUI_ENTRY_COMBO (iface);
        g_return_if_fail (combo->priv);
        g_return_if_fail (!value ||
                          (value && (gda_value_isa ((GValue*) value, GDA_TYPE_LIST) ||
                                     gda_value_isa ((GValue*) value, GDA_TYPE_LIST))));

	TO_IMPLEMENT;
}

static GValue *
gdaui_entry_combo_get_value (GdauiDataEntry *iface)
{
        GdauiEntryCombo *combo;
	
        g_return_val_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface), NULL);
        combo = GDAUI_ENTRY_COMBO (iface);
        g_return_val_if_fail (combo->priv, NULL);
	
	TO_IMPLEMENT;
	
	return NULL;
}

static void
gdaui_entry_combo_set_ref_value (GdauiDataEntry *iface, const GValue * value)
{
        GdauiEntryCombo *combo;
	
        g_return_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface));
        combo = GDAUI_ENTRY_COMBO (iface);
        g_return_if_fail (combo->priv);

	TO_IMPLEMENT;
}

static const GValue *
gdaui_entry_combo_get_ref_value (GdauiDataEntry *iface)
{
        GdauiEntryCombo *combo;
	
        g_return_val_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface), NULL);
        combo = GDAUI_ENTRY_COMBO (iface);
        g_return_val_if_fail (combo->priv, NULL);
                     
	TO_IMPLEMENT;

	return NULL;
}

static void
gdaui_entry_combo_set_value_default (GdauiDataEntry *iface, const GValue * value)
{
        GdauiEntryCombo *combo;
	
        g_return_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface));
        combo = GDAUI_ENTRY_COMBO (iface);
        g_return_if_fail (combo->priv);
	
	TO_IMPLEMENT;
}


static void
gdaui_entry_combo_set_attributes (GdauiDataEntry *iface, guint attrs, guint mask)
{
	GdauiEntryCombo *combo;

	g_return_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface));
	combo = GDAUI_ENTRY_COMBO (iface);
	g_return_if_fail (combo->priv);

	/* Setting to NULL */
	if (mask & GDA_VALUE_ATTR_IS_NULL) {
		if ((mask & GDA_VALUE_ATTR_CAN_BE_NULL) &&
		    !(attrs & GDA_VALUE_ATTR_CAN_BE_NULL))
			g_return_if_reached ();
		if (attrs & GDA_VALUE_ATTR_IS_NULL) {
			gdaui_entry_combo_set_values (combo, NULL);
			
			/* if default is set, see if we can keep it that way */
			if (combo->priv->default_forced) {
				GSList *list;
				gboolean allnull = TRUE;
				for (list = combo->priv->combo_nodes; list; list = list->next) {
					if (COMBO_NODE (list->data)->value_default && 
					    (G_VALUE_TYPE (COMBO_NODE (list->data)->value_default) != 
					     GDA_TYPE_NULL)) {
						allnull = FALSE;
						break;
					}
				}

				if (!allnull)
					combo->priv->default_forced = FALSE;
			}

			gdaui_entry_combo_emit_signal (combo);
			return;
		}
		else {
			combo->priv->null_forced = FALSE;
			gdaui_entry_combo_emit_signal (combo);
		}
	}

	/* Can be NULL ? */
	if (mask & GDA_VALUE_ATTR_CAN_BE_NULL)
		if (combo->priv->null_possible != (attrs & GDA_VALUE_ATTR_CAN_BE_NULL) ? TRUE : FALSE) {
			combo->priv->null_possible = (attrs & GDA_VALUE_ATTR_CAN_BE_NULL) ? TRUE : FALSE;
			gdaui_combo_add_null (GDAUI_COMBO (combo->priv->combo_entry),
						      combo->priv->null_possible);		 
		}


	/* Setting to DEFAULT */
	if (mask & GDA_VALUE_ATTR_IS_DEFAULT) {
		if ((mask & GDA_VALUE_ATTR_CAN_BE_DEFAULT) &&
		    !(attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT))
			g_return_if_reached ();
		if (attrs & GDA_VALUE_ATTR_IS_DEFAULT) {
			GSList *tmplist = NULL;
			GSList *list;
			
			for (list = combo->priv->combo_nodes; list; list = list->next)
				tmplist = g_slist_append (tmplist, COMBO_NODE (list->data)->value_default);

			gdaui_entry_combo_set_values (combo, tmplist);
			g_slist_free (tmplist);

			/* if NULL is set, see if we can keep it that way */
			if (combo->priv->null_forced) {
				GSList *list;
				gboolean allnull = TRUE;
				for (list = combo->priv->combo_nodes; list; list = list->next) {
					if (COMBO_NODE (list->data)->value_default && 
					    (G_VALUE_TYPE (COMBO_NODE (list->data)->value_default) != 
					     GDA_TYPE_NULL)) {
						allnull = FALSE;
						break;
					}
				}
				
				if (!allnull)
					combo->priv->null_forced = FALSE;
			}

			combo->priv->default_forced = TRUE;
			gdaui_entry_combo_emit_signal (combo);
			return;
		}
		else {
			combo->priv->default_forced = FALSE;
			gdaui_entry_combo_emit_signal (combo);
		}
	}

	/* Can be DEFAULT ? */
	if (mask & GDA_VALUE_ATTR_CAN_BE_DEFAULT)
		combo->priv->default_possible = (attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT) ? TRUE : FALSE;
	
	/* Modified ? */
	if (mask & GDA_VALUE_ATTR_IS_UNCHANGED) {
		if (attrs & GDA_VALUE_ATTR_IS_UNCHANGED) {
			GSList *tmplist = NULL;
			GSList *list;
			
			for (list = combo->priv->combo_nodes; list; list = list->next)
				tmplist = g_slist_append (tmplist, COMBO_NODE (list->data)->value_orig);
				
			gdaui_entry_combo_set_values (combo, tmplist);
			g_slist_free (tmplist);
			combo->priv->default_forced = FALSE;
			gdaui_entry_combo_emit_signal (combo);
		}
	}

	/* Actions buttons ? */
	if (mask & GDA_VALUE_ATTR_ACTIONS_SHOWN) {
		GValue *gval;
		combo->priv->show_actions = (attrs & GDA_VALUE_ATTR_ACTIONS_SHOWN) ? TRUE : FALSE;
		
		gval = g_new0 (GValue, 1);
		g_value_init (gval, G_TYPE_BOOLEAN);
		g_value_set_boolean (gval, combo->priv->show_actions);
		g_object_set_property (G_OBJECT (combo), "actions", gval);
		g_free (gval);
	}

	/* NON WRITABLE attributes */
	if (mask & GDA_VALUE_ATTR_DATA_NON_VALID) 
		g_warning ("Can't force a GdauiDataEntry to be invalid!");

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
	g_return_val_if_fail (combo->priv, 0);

	for (list = combo->priv->combo_nodes; list; list = list->next) {
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
		retval = retval | GDA_VALUE_ATTR_IS_UNCHANGED;

	if (isnull || combo->priv->null_forced)
		retval = retval | GDA_VALUE_ATTR_IS_NULL;

	/* can be NULL? */
	if (combo->priv->null_possible) 
		retval = retval | GDA_VALUE_ATTR_CAN_BE_NULL;
	
	/* is default */
	if (combo->priv->default_forced)
		retval = retval | GDA_VALUE_ATTR_IS_DEFAULT;
	
	/* can be default? */
	if (combo->priv->default_possible)
		retval = retval | GDA_VALUE_ATTR_CAN_BE_DEFAULT;
	

	/* actions shown */
	if (combo->priv->show_actions)
		retval = retval | GDA_VALUE_ATTR_ACTIONS_SHOWN;

	/* data valid? */
	if (! combo->priv->data_valid)
		retval = retval | GDA_VALUE_ATTR_DATA_NON_VALID;
	else {
		GSList *nodes;
		gboolean allnull = TRUE;
		
		for (nodes = combo->priv->combo_nodes; nodes; nodes = nodes->next) {
			ComboNode *node = COMBO_NODE (nodes->data);

			/* all the nodes are NULL ? */
			if (node->value && (G_VALUE_TYPE ((GValue *) node->value) != GDA_TYPE_NULL)) {
				allnull = FALSE;
				break;
			}
		}

		if ((allnull && !combo->priv->null_possible) ||
		    (combo->priv->null_forced && !combo->priv->null_possible))
			retval = retval | GDA_VALUE_ATTR_DATA_NON_VALID;
	}

	/* has original value? */
	list2 = gdaui_entry_combo_get_values_orig (combo);
	if (list2) {
		retval = retval | GDA_VALUE_ATTR_HAS_VALUE_ORIG;
		g_slist_free (list2);
	}

	return retval;
}


static gboolean
gdaui_entry_combo_expand_in_layout (GdauiDataEntry *iface)
{
	GdauiEntryCombo *combo;

	g_return_val_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface), FALSE);
	combo = GDAUI_ENTRY_COMBO (iface);
	g_return_val_if_fail (combo->priv, FALSE);

	return FALSE;
}

static void
gdaui_entry_combo_grab_focus (GdauiDataEntry *iface)
{
	GdauiEntryCombo *combo;

	g_return_if_fail (iface && GDAUI_IS_ENTRY_COMBO (iface));
	combo = GDAUI_ENTRY_COMBO (iface);
	g_return_if_fail (combo->priv);

	if (combo->priv->combo_entry)
		gtk_widget_grab_focus (combo->priv->combo_entry);
}
