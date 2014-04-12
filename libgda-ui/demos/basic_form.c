/* Forms/Basic form
 *
 * The GdauiBasicForm widget allows the user to modify values
 * stored in individual GdaHolder objects grouped in a GdaSet
 */

#include <libgda-ui/libgda-ui.h>
#include "demo-common.h"

static GtkWidget *window = NULL;

GtkWidget *
do_basic_form (GtkWidget *do_widget)
{  
	if (!window) {
		GtkWidget *vbox;
		GtkWidget *label;
		GtkWidget *form;
		
		window = gtk_dialog_new_with_buttons ("GdauiBasicForm",
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
		
		label = gtk_label_new ("This example shows 2 GdauiBasicForm widgets operating on the\n"
				       "same GdaSet. When a value is modified in one form, then it is\n"
				       "automatically updated in the other form.\n\n"
				       "Also the top form uses the default layout, while the bottom one\n"
				       "uses a custom (2 columns) layout.\n"
				       "The 'an int' entry is hidden in the top form.");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		GdaSet *set;
		gchar *filename;
		set = gda_set_new_inline (3,
					  "a string", G_TYPE_STRING, "A string Value",
					  "an int", G_TYPE_INT, 12,
					  "a picture", GDA_TYPE_BINARY, NULL);
		form = gdaui_basic_form_new (set);
		gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);
		gdaui_basic_form_entry_set_visible (GDAUI_BASIC_FORM (form), gda_set_get_holder (set, "an int"), FALSE);

		label = gtk_label_new ("2nd form is below:");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		form = gdaui_basic_form_new (set);
		gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);

                filename = demo_find_file ("custom_layout.xml", NULL);
		gdaui_basic_form_set_layout_from_file (GDAUI_BASIC_FORM (form), filename, "simple");
		g_free (filename);

		g_object_unref (set);
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


