/* Widgets/Provider selector
 *
 * The GdauiProviderSelector widget allows the user to select a database pvodier
 * among the ones detected when Libgda has been initialized
 */

#include <libgda-ui/libgda-ui.h>

static GtkWidget *window = NULL;

static void
selection_changed_cb (GdauiProviderSelector *psel, G_GNUC_UNUSED gpointer data)
{
	g_print ("Provider selected: %s\n", gdaui_provider_selector_get_provider (psel));
}

GtkWidget *
do_provider_sel (GtkWidget *do_widget)
{  
	if (!window) {
		GtkWidget *vbox, *psel, *label;

		window = gtk_dialog_new_with_buttons ("GdauiProviderSelector",
						      GTK_WINDOW (do_widget),
						      0,
						      "Close", GTK_RESPONSE_NONE,
						      NULL);
		gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
		g_signal_connect (window, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (window, "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &window);
		
		vbox = gtk_dialog_get_content_area (GTK_DIALOG (window));

		/* Create the widget */
		label = gtk_label_new ("Provider selector:");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		psel = gdaui_provider_selector_new ();
		gtk_box_pack_start (GTK_BOX (vbox), psel, FALSE, FALSE, 0);
		g_signal_connect (psel, "selection-changed",
				  G_CALLBACK (selection_changed_cb), NULL);
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


