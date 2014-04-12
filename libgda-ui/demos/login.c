/* Widgets/Login
 *
 * The GdauiLogin widget displays information required to open a connection
 */

#include <libgda-ui/libgda-ui.h>

static GtkWidget *window = NULL;

static void
cb1_toggled_cb (G_GNUC_UNUSED GtkCheckButton *cb, GtkWidget *login)
{
	GdauiLoginMode mode;
	g_object_get (G_OBJECT (login), "mode", &mode, NULL);
	mode ^= GDA_UI_LOGIN_ENABLE_CONTROL_CENTRE_MODE;
	gdaui_login_set_mode (GDAUI_LOGIN (login), mode);
}

static void
cb2_toggled_cb (G_GNUC_UNUSED GtkCheckButton *cb, GtkWidget *login)
{
	GdauiLoginMode mode;
	g_object_get (G_OBJECT (login), "mode", &mode, NULL);
	mode ^= GDA_UI_LOGIN_HIDE_DSN_SELECTION_MODE;
	gdaui_login_set_mode (GDAUI_LOGIN (login), mode);
}

static void
cb3_toggled_cb (G_GNUC_UNUSED GtkCheckButton *cb, GtkWidget *login)
{
	GdauiLoginMode mode;
	g_object_get (G_OBJECT (login), "mode", &mode, NULL);
	mode ^= GDA_UI_LOGIN_HIDE_DIRECT_CONNECTION_MODE;
	gdaui_login_set_mode (GDAUI_LOGIN (login), mode);
}

static void
button_clicked_cb (G_GNUC_UNUSED GtkButton *button, GtkWidget *login)
{
	const GdaDsnInfo *info;
	info = gdaui_login_get_connection_information (GDAUI_LOGIN (login));

	g_print ("\nCurrent connection's parameters:\n");
	g_print ("DSN name:    %s\n", info->name);
	g_print ("provider:    %s\n", info->provider);
	g_print ("description: %s\n", info->description);
	g_print ("cnc_string:  %s\n", info->cnc_string);
	g_print ("auth_string: %s\n", info->auth_string);
}

static void
login_changed_cb (G_GNUC_UNUSED GdauiLogin *login, gboolean is_valid, GtkLabel *label)
{
	if (is_valid)
		gtk_label_set_markup (label, "<span foreground='#00AA00'>Valid information</span>");
	else
		gtk_label_set_markup (label, "<span foreground='red'>Invalid information</span>");
}

GtkWidget *
do_login (GtkWidget *do_widget)
{  
	if (!window) {
		GtkWidget *grid, *cb, *button;
		GtkWidget *login, *frame;

		window = gtk_dialog_new_with_buttons ("GdauiLogin widget",
						      GTK_WINDOW (do_widget),
						      0,
						      "Close", GTK_RESPONSE_NONE,
						      NULL);
		gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
		g_signal_connect (window, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (window, "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &window);
		

		grid = gtk_grid_new ();
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))),
				    grid, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (grid), 5);

		/* Create the login widget */
		frame = gtk_frame_new ("Login widget:");
		gtk_grid_attach (GTK_GRID (grid), frame, 0, 3, 2, 1);

		login = gdaui_login_new (NULL);
		gtk_container_add (GTK_CONTAINER (frame), login);
		
		/* Create the options */
		cb = gtk_check_button_new_with_label ("Enable control center");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), TRUE);
		gtk_grid_attach (GTK_GRID (grid), cb, 0, 0, 1, 1);
		g_signal_connect (cb, "toggled",
				  G_CALLBACK (cb1_toggled_cb), login);
		
		cb = gtk_check_button_new_with_label ("Hide DSN selection");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), FALSE);
		gtk_grid_attach (GTK_GRID (grid), cb, 0, 1, 1, 1);
		g_signal_connect (cb, "toggled",
				  G_CALLBACK (cb2_toggled_cb), login);

		cb = gtk_check_button_new_with_label ("Hide direct connection");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), FALSE);
		gtk_grid_attach (GTK_GRID (grid), cb, 0, 2, 1, 1);
		g_signal_connect (cb, "toggled",
				  G_CALLBACK (cb3_toggled_cb), login);
		
		button = gtk_button_new_with_label ("Show connection's parameters");
		gtk_grid_attach (GTK_GRID (grid), button, 1, 0, 1, 1);
		g_signal_connect (button, "clicked",
				  G_CALLBACK (button_clicked_cb), login);

		GtkWidget *status;
		gboolean valid;
		status = gtk_label_new ("...");
		gtk_grid_attach (GTK_GRID (grid), status, 1, 2, 1, 1);
		g_object_get (G_OBJECT (login), "valid", &valid, NULL);
		login_changed_cb (GDAUI_LOGIN (login), valid, GTK_LABEL (status));
		g_signal_connect (login, "changed",
				  G_CALLBACK (login_changed_cb), status);
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


