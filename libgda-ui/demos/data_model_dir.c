/* Data models/Directory data model
 *
 * A GdaDataModelDir data model listing all the files in a directory displayed in
 * a GdauiForm widget where the 'picture' plugin is used to display the contents of each file
 * 
 */

#include <libgda/binreloc/gda-binreloc.h>
#include <libgda-ui/libgda-ui.h>

static GtkWidget *window = NULL;

/* hack to find the directory where Libgdaui's data pictures are stored */
static gchar *
get_data_path ()
{
	gchar *path;

	if (g_file_test ("demos.h", G_FILE_TEST_EXISTS))
		path = g_strdup ("../data");
	else {
		gchar *tmp;
		tmp = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "gdaui-generic.png", NULL);
		path = g_path_get_dirname (tmp);
		g_free (tmp);
		if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
			g_free (path);
			path = NULL;
		}
	}

	if (!path)
		path = g_strdup (".");

	return path;
}

GtkWidget *
do_data_model_dir (GtkWidget *do_widget)
{  
	if (!window) {
		GtkWidget *vbox;
		GtkWidget *label;
		GdaDataModel *model;
		GtkWidget *form, *grid, *nb;
		GdaSet *data_set;
		GdaHolder *param;
		gchar *path;

		window = gtk_dialog_new_with_buttons ("GdaDataModelDir data model",
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
		
		label = gtk_label_new ("The following GdauiForm widget displays data from a GdaDataModelDir "
				       "data model which lists the files contained in the selected directory.\n\n"
				       "Each file contents is then displayed using the 'picture' plugin \n"
				       "(right click to open a menu, or double click to load an image).");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* GdaDataModelDir object */
		path = get_data_path ();
		model = gda_data_model_dir_new (path);
		g_free (path);

		/* Create the demo widget */
		nb = gtk_notebook_new ();
		gtk_box_pack_start (GTK_BOX (vbox), nb, TRUE, TRUE, 0);

		form = gdaui_form_new (model);
		gtk_notebook_append_page (GTK_NOTEBOOK (nb), form, gtk_label_new ("Form"));

		grid = gdaui_grid_new (model);
		gtk_notebook_append_page (GTK_NOTEBOOK (nb), grid, gtk_label_new ("Grid"));
		g_object_unref (model);

		/* specify that we want to use the 'picture' plugin */
		data_set = GDA_SET (gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (grid)));
		param = gda_set_get_holder (data_set, "data");

		g_object_set (param, "plugin", "picture", NULL);
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


