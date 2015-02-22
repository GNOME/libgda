/* Data widgets linking/Data model with parameters
 *
 * Display a data model which requires a parameter along with a
 * form to display the required parameter.
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;

static void
salesrep_changed_cb (GdaHolder *holder, G_GNUC_UNUSED gpointer data)
{
	gchar *str;
	str = gda_value_stringify (gda_holder_get_value (holder));
	g_print ("SalesRep changed to: %s\n", str);
	g_free (str);
}

GtkWidget *
do_linked_model_param (GtkWidget *do_widget)
{  
	if (!window) {
                GdaStatement *stmt;
		GtkWidget *vbox;
		GtkWidget *label;
		GdaDataModel *cust_model, *sr_model;
		GtkWidget *form, *grid;
		GdaSet *params;
		GdaHolder *param;

		window = gtk_dialog_new_with_buttons ("GdaDataModel depending on a parameter",
						      GTK_WINDOW (do_widget),
						      0,
						      "Close", GTK_RESPONSE_NONE,
						      NULL);
		
		g_signal_connect (window, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (window, "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &window);
		
		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))),
				    vbox, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
		
		label = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (label),
				      "The bottom grid show a list of customers which either\n"
				       "don't have a salesrep or have a specified salesrep: the salesrep\n"
				       "is a parameter which is selected in the top GdauiBasicForm.\n\n"
				       "<u>Note:</u> the grid is updated anytime a salesrep is selected and is\n"
				       "empty as long as no salesrep is selected.");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		/* create a data model for the salesrep */
		stmt = gda_sql_parser_parse_string (demo_parser, "SELECT id, name FROM salesrep", NULL, NULL);
		sr_model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt);
		
		/* create a data model for the customers, and get a list of parameters */
		stmt = gda_sql_parser_parse_string (demo_parser, 
						"SELECT c.id, c.name, s.name AS \"SalesRep\""
						"FROM customers c "
						"LEFT JOIN salesrep s ON (s.id=c.default_served_by) "
						"WHERE s.id = ##SalesRep::gint::null", NULL, NULL);
		gda_statement_get_parameters (stmt, &params, NULL);
		cust_model = gda_connection_statement_execute_select_full (demo_cnc, stmt, params,
									   GDA_STATEMENT_MODEL_ALLOW_NOPARAM,
									   NULL, NULL);
		g_object_set (cust_model, "auto-reset", TRUE, NULL);
		g_object_unref (stmt);

		/* restrict the c.default_served_by field in the grid to be within the sr_model */
		param = gda_set_get_holder (params, "SalesRep");
		g_assert (gda_holder_set_source_model (param, sr_model, 0, NULL));
		g_signal_connect (param, "changed",
				  G_CALLBACK (salesrep_changed_cb), NULL);


		/* create a basic form to set the values in params */
		label = gtk_label_new ("<b>GdauiBasicForm to choose a sales person:</b>");
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
		gtk_widget_show (label);

		form = gdaui_basic_form_new (params);
		gtk_box_pack_start (GTK_BOX (vbox), form, FALSE, TRUE, 0);
		gtk_widget_show (form);

		/* create grid widget */
		label = gtk_label_new ("<b>GdauiGrid for the customers:</b>");
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
		gtk_widget_show (label);

		grid = gdaui_grid_new (cust_model);
		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);
		gtk_widget_show (grid);
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
