/* gdaui-data-widget.c
 *
 * Copyright (C) 2004 - 2009 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/libgda.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "gdaui-data-widget.h"
#include  <glib/gi18n-lib.h>
#include <libgda-ui/gdaui-raw-form.h>
#include <libgda-ui/gdaui-raw-grid.h>

/* signals */
enum
{
        PROXY_CHANGED,
        ITER_CHANGED,
        LAST_SIGNAL
};

static gint gdaui_data_widget_signals[LAST_SIGNAL] = { 0, 0 };

static void gdaui_data_widget_iface_init (gpointer g_class);

GType
gdaui_data_widget_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiDataWidgetIface),
			(GBaseInitFunc) gdaui_data_widget_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, "GdauiDataWidget", &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	return type;
}


static void
gdaui_data_widget_iface_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		gdaui_data_widget_signals[PROXY_CHANGED] = 
			g_signal_new ("proxy_changed",
                                      GDAUI_TYPE_DATA_WIDGET,
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (GdauiDataWidgetIface, proxy_changed),
                                      NULL, NULL,
                                      g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE,
                                      1, GDA_TYPE_DATA_PROXY);
		gdaui_data_widget_signals[ITER_CHANGED] = 
			g_signal_new ("iter_changed",
                                      GDAUI_TYPE_DATA_WIDGET,
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (GdauiDataWidgetIface, iter_changed),
                                      NULL, NULL,
                                      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE,
                                      1, G_TYPE_POINTER);

		initialized = TRUE;
	}
}

/**
 * gdaui_data_widget_get_proxy
 * @iface: an object which implements the #GdauiDataWidget interface
 *
 * Get a pointer to the #GdaDataProxy being used by @iface
 *
 * Returns: a #GdaDataProxy pointer
 */
GdaDataProxy *
gdaui_data_widget_get_proxy (GdauiDataWidget *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_WIDGET (iface), NULL);
	
	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->get_proxy)
		return (GDAUI_DATA_WIDGET_GET_IFACE (iface)->get_proxy) (iface);
	else
		return NULL;
}

/**
 * gdaui_data_widget_column_show
 * @iface: an object which implements the #GdauiDataWidget interface
 * @column: column number to show
 *
 * Shows the data at @column in the data model @iface operates on
 */
void
gdaui_data_widget_column_show (GdauiDataWidget *iface, gint column)
{
	g_return_if_fail (GDAUI_IS_DATA_WIDGET (iface));

	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->col_set_show)
		(GDAUI_DATA_WIDGET_GET_IFACE (iface)->col_set_show) (iface, column, TRUE);
}


/**
 * gdaui_data_widget_column_hide
 * @iface: an object which implements the #GdauiDataWidget interface
 * @column: column number to hide
 *
 * Hides the data at @column in the data model @iface operates on
 */
void
gdaui_data_widget_column_hide (GdauiDataWidget *iface, gint column)
{
	g_return_if_fail (GDAUI_IS_DATA_WIDGET (iface));

	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->col_set_show)
		(GDAUI_DATA_WIDGET_GET_IFACE (iface)->col_set_show) (iface, column, FALSE);
}


/**
 * gdaui_data_widget_column_set_editable
 * @iface: an object which implements the #GdauiDataWidget interface
 * @column: column number of the data
 * @editable:
 *
 * Sets if the data entry in the @iface widget at @column (in the data model @iface operates on) can be edited or not.
 */
void 
gdaui_data_widget_column_set_editable (GdauiDataWidget *iface, gint column, gboolean editable)
{
	g_return_if_fail (GDAUI_IS_DATA_WIDGET (iface));

	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->set_column_editable)
		(GDAUI_DATA_WIDGET_GET_IFACE (iface)->set_column_editable) (iface, column, editable);	
}

/**
 * gdaui_data_widget_column_show_actions
 * @iface: an object which implements the #GdauiDataWidget interface
 * @column: column number of the data, or -1 to apply the setting to all the columns
 * @show_actions:
 * 
 * Sets if the data entry in the @iface widget at @column (in the data model @iface operates on) must show its
 * actions menu or not.
 */
void
gdaui_data_widget_column_show_actions (GdauiDataWidget *iface, gint column, gboolean show_actions)
{
	g_return_if_fail (GDAUI_IS_DATA_WIDGET (iface));

	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->show_column_actions)
		(GDAUI_DATA_WIDGET_GET_IFACE (iface)->show_column_actions) (iface, column, show_actions);
}

/**
 * gdaui_data_widget_get_actions_group
 * @iface: an object which implements the #GdauiDataWidget interface
 *
 * Each widget imlplementing the #GdauiDataWidget interface provides actions. Actions can be triggered
 * using the gdaui_data_widget_perform_action() method, but using this method allows for the creation of
 * toolbars, menus, etc calling these actions.
 *
 * The actions are among: 
 * <itemizedlist><listitem><para>Data edition actions: "ActionNew", "ActionCommit", 
 *    "ActionDelete, "ActionUndelete, "ActionReset", </para></listitem>
 * <listitem><para>Record by record moving: "ActionFirstRecord", "ActionPrevRecord", 
 *    "ActionNextRecord", "ActionLastRecord",</para></listitem>
 * <listitem><para>Chuncks of records moving: "ActionFirstChunck", "ActionPrevChunck", 
 *     "ActionNextChunck", "ActionLastChunck".</para></listitem>
 * <listitem><para>Filtering: "ActionFilter"</para></listitem></itemizedlist>
 * 
 * Returns: the #GtkActionGroup with all the possible actions on the widget.
 */
GtkActionGroup *
gdaui_data_widget_get_actions_group (GdauiDataWidget *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_WIDGET (iface), NULL);

	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->get_actions_group)
		return (GDAUI_DATA_WIDGET_GET_IFACE (iface)->get_actions_group) (iface);
	return NULL;
}

/**
 * gdaui_data_widget_perform_action
 * @iface: an object which implements the #GdauiDataWidget interface
 * @action: a #GdauiAction action
 *
 * Forces the widget to perform the selected @action, as if the user
 * had pressed on the corresponding action button in the @iface widget,
 * if the corresponding action is possible and if the @iface widget
 * supports the action.
 */
void
gdaui_data_widget_perform_action (GdauiDataWidget *iface, GdauiAction action)
{
	gchar *action_name = NULL;
	GtkActionGroup *group;
	GtkAction *gtkaction;

	g_return_if_fail (GDAUI_IS_DATA_WIDGET (iface));
	group = gdaui_data_widget_get_actions_group (iface);
	if (!group) {
		g_warning ("Object class %s does not support the gdaui_data_widget_get_actions_group() method",
			   G_OBJECT_TYPE_NAME (iface));
		return;
	}
	
	switch (action) {
	case GDAUI_ACTION_NEW_DATA:
		action_name = "ActionNew";
		break;
	case GDAUI_ACTION_WRITE_MODIFIED_DATA:
		action_name = "ActionCommit";
		break;
        case GDAUI_ACTION_DELETE_SELECTED_DATA:
		action_name = "ActionDelete";
		break;
        case GDAUI_ACTION_UNDELETE_SELECTED_DATA:
		action_name = "ActionUndelete";
		break;
        case GDAUI_ACTION_RESET_DATA:
		action_name = "ActionReset";
		break;
        case GDAUI_ACTION_MOVE_FIRST_RECORD:
		action_name = "ActionFirstRecord";
		break;
        case GDAUI_ACTION_MOVE_PREV_RECORD:
		action_name = "ActionPrevRecord";
		break;
        case GDAUI_ACTION_MOVE_NEXT_RECORD:
		action_name = "ActionNextRecord";
		break;
        case GDAUI_ACTION_MOVE_LAST_RECORD:
		action_name = "ActionLastRecord";
		break;
        case GDAUI_ACTION_MOVE_FIRST_CHUNCK:
		action_name = "ActionFirstChunck";
		break;
        case GDAUI_ACTION_MOVE_PREV_CHUNCK:
		action_name = "ActionPrevChunck";
		break;
        case GDAUI_ACTION_MOVE_NEXT_CHUNCK:
		action_name = "ActionNextChunck";
		break;
        case GDAUI_ACTION_MOVE_LAST_CHUNCK:
		action_name = "ActionLastChunck";
		break;
	default:
		g_assert_not_reached ();
	}

	gtkaction = gtk_action_group_get_action (group, action_name);
	if (gtkaction)
		gtk_action_activate (gtkaction);
}

/**
 * gdaui_data_widget_get_current_data
 * @iface: an object which implements the #GdauiDataWidget interface
 *
 * Get the #GdaDataModelIter object which contains all the parameters which in turn contain the actual
 * data stored in @iface. When the user changes what's displayed or what's selected (depending on the
 * actual widget) in @iface, then the parameter's values change as well.
 *
 * Returns: the #GdaSet object for data (not a new object)
 */
GdaDataModelIter *
gdaui_data_widget_get_current_data (GdauiDataWidget *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_WIDGET (iface), NULL);

	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->get_data_set)
		return (GDAUI_DATA_WIDGET_GET_IFACE (iface)->get_data_set) (iface);
	return NULL;
}

/**
 * gdaui_data_widget_get_gda_model
 * @iface: an object which implements the #GdauiDataWidget interface
 *
 * Get the current #GdaDataModel used by @iface
 *
 * Returns: the #GdaDataModel, or %NULL if there is none
 */
GdaDataModel *
gdaui_data_widget_get_gda_model (GdauiDataWidget *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_WIDGET (iface), NULL);

	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->get_gda_model)
		return (GDAUI_DATA_WIDGET_GET_IFACE (iface)->get_gda_model) (iface);
	return NULL;
}

/**
 * gdaui_data_widget_set_gda_model
 * @iface: an object which implements the #GdauiDataWidget interface
 * @model: a valid #GdaDataModel
 *
 * Sets the data model which is used by @iface.
 */
void
gdaui_data_widget_set_gda_model (GdauiDataWidget *iface, GdaDataModel *model)
{
	g_return_if_fail (GDAUI_IS_DATA_WIDGET (iface));
	g_return_if_fail (model && GDA_IS_DATA_MODEL (model));
	
	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->set_gda_model)
		(GDAUI_DATA_WIDGET_GET_IFACE (iface)->set_gda_model) (iface, model);
}

/**
 * gdaui_data_widget_set_write_mode
 * @iface: an object which implements the #GdauiDataWidget interface
 * @mode:
 *
 * Specifies the way the modifications stored in the #GdaDataProxy used internally by @iface are written back to
 * the #GdaDataModel which holds the data displayed in @iface.
 *
 * Returns: TRUE if the proposed mode has been taken into account
 */
gboolean
gdaui_data_widget_set_write_mode (GdauiDataWidget *iface, GdauiDataWidgetWriteMode mode)
{
	g_return_val_if_fail (GDAUI_IS_DATA_WIDGET (iface), FALSE);
	
	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->set_write_mode)
		return (GDAUI_DATA_WIDGET_GET_IFACE (iface)->set_write_mode) (iface, mode);
	else 
		return FALSE;
}

/**
 * gdaui_data_widget_get_write_mode
 * @iface: an object which implements the #GdauiDataWidget interface
 *
 * Get the way the modifications stored in the #GdaDataProxy used internally by @iface are written back to
 * the #GdaDataModel which holds the data displayed in @iface.
 *
 * Returns: the write mode used by @iface
 */
GdauiDataWidgetWriteMode
gdaui_data_widget_get_write_mode (GdauiDataWidget *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_WIDGET (iface), GDAUI_DATA_WIDGET_WRITE_ON_DEMAND);
	
	if (GDAUI_DATA_WIDGET_GET_IFACE (iface)->get_write_mode)
		return (GDAUI_DATA_WIDGET_GET_IFACE (iface)->get_write_mode) (iface);
	else 
		return GDAUI_DATA_WIDGET_WRITE_ON_DEMAND;
}

/**
 * gdaui_data_widget_set_data_layout_from_file
 * @iface: an object which implements the #GdauiDataWidget interface
 * @file_name:
 * @parent_table:
 *
 * Sets a data layout according an XML string in the @iface widget.
 */
void
gdaui_data_widget_set_data_layout_from_file (GdauiDataWidget *iface, const gchar *file_name,
					     const gchar  *parent_table)
{
	g_return_if_fail (GDAUI_IS_DATA_WIDGET (iface));
	g_return_if_fail (file_name);
        g_return_if_fail (parent_table);

	g_return_if_fail (GDAUI_DATA_WIDGET_GET_IFACE (iface)->set_data_layout);

	xmlDocPtr doc;
        doc = xmlParseFile (file_name);
        if (doc == NULL) {
                g_warning (_("'%s' Document not parsed successfully\n"), file_name);
                return;
        }

        xmlDtdPtr dtd = NULL;

	gchar *file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "data-layout.dtd", NULL);
        if (g_file_test (file, G_FILE_TEST_EXISTS))
                dtd = xmlParseDTD (NULL, file);
        if (dtd == NULL) {
                g_warning (_("'%s' DTD not parsed successfully. "
                             "XML data layout validation will not be "
                             "performed (some errors may occur)"), file);
        }
        g_free (file);

        /* Get the root element node */
        xmlNodePtr root_node = NULL;
        root_node = xmlDocGetRootElement (doc);

        /* Must have root element, a name and the name must be "data_layouts" */
        if (!root_node ||
            !root_node->name ||
            xmlStrcmp (root_node->name, "data_layouts")) {
                xmlFreeDoc (doc);
                return;
        }

        xmlNodePtr node;
        for (node = root_node->children; node != NULL; node = node->next) {

                if (node->type == XML_ELEMENT_NODE &&
                    !xmlStrcmp (node->name, (const xmlChar *) "data_layout")) {
                        gboolean retval = FALSE;
                        xmlChar *str;

                        str = xmlGetProp (node, "parent_table");
                        if (str) {
                                if (strcmp (str, parent_table) == 0)
                                        retval = TRUE;
                                //g_print ("parent_table: %s\n", str);
                                xmlFree (str);
                        }

                        str = xmlGetProp (node, "name");
                        if (str) {
                                if (retval == TRUE &&
                                    ((GDAUI_IS_RAW_GRID (iface) && strcmp (str, "list") == 0) ||
				     (GDAUI_IS_RAW_FORM (iface) &&strcmp (str, "details") == 0)))  // Now proceed
                                        (GDAUI_DATA_WIDGET_GET_IFACE (iface)->set_data_layout) (iface, node);
                                //g_print ("name: %s\n", str);
                                xmlFree (str);
                        }
                }
        }
	
        /* Free the document */
        xmlFreeDoc (doc);

        /* Free the global variables that may
         * have been allocated by the parser */
        xmlCleanupParser ();
}
