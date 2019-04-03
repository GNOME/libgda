/* Widgets/GdaTree display
 *
 * This demonstration program creates a GdaTree object,
 * feeds it to a GdauiTreeStore (which implements the GtkTreeModel
 * interface, and creates a GtkTreeView to display the contents
 * of the GdaTree
 *
 * The GdaTreeManager objects used here are:
 *
 * manager1: one node on which a timer modifies some attributes
 *
 * manager2: one node for each table in the @demo_cnc connection
 */

/*
 * The GdaTreeManager structure is as follows:
 * .
 * |-- manager1
 * |   `-- manager2
 * `-- manager2
 *
 * Resulting in the following tree's contents:
 * .
 * |-- Scaling...
 * |   |-- categories
 * |   |-- customers
 * |   |-- locations
 * |   |-- order_contents
 * |   |-- orders
 * |   |-- pictures
 * |   |-- products
 * |   |-- roles
 * |   |-- sales_orga
 * |   |-- salesrep
 * |   `-- warehouses
 * |-- categories
 * |-- customers
 * |-- locations
 * |-- order_contents
 * |-- orders
 * |-- pictures
 * |-- products
 * |-- roles
 * |-- sales_orga
 * |-- salesrep
 * `-- warehouses
 */

#include <libgda-ui/libgda-ui.h>

static GtkWidget *window = NULL;
extern GdaConnection *demo_cnc;

static gboolean
timout_cb (GdaTreeNode *node)
{
	const GValue *cvalue;
	GValue *scale;
	gdouble sc;

	cvalue = gda_tree_node_get_node_attribute (node, "scale");
	g_assert (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_DOUBLE));
	sc = g_value_get_double (cvalue);
	sc += 0.005;
	if (sc > 1.2)
		sc = .8;
	g_value_set_double ((scale = gda_value_new (G_TYPE_DOUBLE)), sc);
	gda_tree_node_set_node_attribute (node, "scale", scale, NULL);
	gda_value_free (scale);

	return TRUE;
}

void ref_objects (GObject *object, G_GNUC_UNUSED gpointer user_data)
{
  g_object_ref (object);
}

static GSList *
node_func (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
	   G_GNUC_UNUSED gboolean *out_error, G_GNUC_UNUSED GError **error)
{
	if (children_nodes) {
		/* we don't create or modify already created GdaTreeNode object => simply ref them */
		g_slist_foreach ((GSList*) children_nodes, (GFunc) ref_objects, NULL);
		return g_slist_copy ((GSList*) children_nodes);
	}
	else {
		GSList *list;
		GdaTreeNode *snode;
		GValue *scale;

		snode = gda_tree_manager_create_node (manager, node, "Scaling...");
		g_value_set_double ((scale = gda_value_new (G_TYPE_DOUBLE)), 1.);
		gda_tree_node_set_node_attribute (snode, "scale", scale, NULL);
		gda_value_free (scale);
		g_timeout_add (50, (GSourceFunc) timout_cb, g_object_ref (snode));

		g_value_set_boolean ((scale = gda_value_new (G_TYPE_BOOLEAN)), TRUE);
		gda_tree_node_set_node_attribute (snode, "scale-set", scale, NULL);
		gda_value_free (scale);

		list = g_slist_append (NULL, snode);
		return list;
	}
}

GtkWidget *
do_tree (GtkWidget *do_widget)
{  
	if (!window) {
		GtkWidget *label, *treeview;
		GdaTree *tree;
		GdaTreeManager *manager1, *manager2;
		GtkTreeModel *model;
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;

		window = gtk_dialog_new_with_buttons ("GdaTree display",
						      GTK_WINDOW (do_widget),
						      0,
						      "Close", GTK_RESPONSE_NONE,
						      NULL);
		g_signal_connect (window, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (window, "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &window);
		
		label = gtk_label_new ("This demonstration program creates a GdaTree object,\n"
				       "feeds it to a GdauiTreeStore (which implements the GtkTreeModel\n"
				       "interface, and creates a GtkTreeView to display the contents\n"
				       "of the GdaTree");
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))),
				    label, TRUE, TRUE, 0);

		/* create GdaTree */
		tree = gda_tree_new ();
		manager1 = gda_tree_manager_new_with_func (node_func);
		manager2 = gda_tree_mgr_tables_new (demo_cnc, NULL);
		gda_tree_manager_add_manager (manager1, manager2);
		gda_tree_add_manager (tree, manager1);
		gda_tree_add_manager (tree, manager2);
		g_object_unref (manager1);
		g_object_unref (manager2);

		gda_tree_update_all (tree, NULL);

		/* create GtkTreeModel and GtkTreeView */
		model = gdaui_tree_store_new (tree, 3,
					      G_TYPE_STRING, GDA_ATTRIBUTE_NAME,
					      G_TYPE_DOUBLE, "scale",
					      G_TYPE_BOOLEAN, "scale-set");
		treeview = gtk_tree_view_new_with_model (model);
		g_object_unref (model);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))),
				    treeview, TRUE, TRUE, 0);

		/* create GtkTreeView's column */
		enum {
			COLUMN_NAME,
			COLUMN_SCALE,
			COLUMN_SCALE_SET
		};
		renderer = gtk_cell_renderer_text_new ();
		column = gtk_tree_view_column_new_with_attributes ("Name", renderer,
								   "text", COLUMN_NAME,
								   "scale-set", COLUMN_SCALE_SET,
								   "scale", COLUMN_SCALE,
								   NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
	}

	gboolean visible;
	g_object_get (G_OBJECT (window), "visible", &visible, NULL);
	if (!visible)
		gtk_widget_show_all (window);
	else {
		gtk_widget_destroy (window);
		window = NULL;
	}

	return window;
}


