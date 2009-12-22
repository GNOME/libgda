/* Widgets/Login
 *
 * The GdauiLogin widget displays information required to open a connection
 */

#include <libgda-ui/libgda-ui.h>

static GtkWidget *window = NULL;

static void
cb1_toggled_cb (GtkCheckButton *cb, GtkWidget *login)
{
	GdauiLoginMode mode;
	g_object_get (G_OBJECT (login), "mode", &mode, NULL);
	mode ^= GDA_UI_LOGIN_ENABLE_CONTROL_CENTRE_MODE;
	gdaui_login_set_mode (GDAUI_LOGIN (login), mode);
}

static void
cb2_toggled_cb (GtkCheckButton *cb, GtkWidget *login)
{
	GdauiLoginMode mode;
	g_object_get (G_OBJECT (login), "mode", &mode, NULL);
	mode ^= GDA_UI_LOGIN_HIDE_DSN_SELECTION_MODE;
	gdaui_login_set_mode (GDAUI_LOGIN (login), mode);
}

static void
cb3_toggled_cb (GtkCheckButton *cb, GtkWidget *login)
{
	GdauiLoginMode mode;
	g_object_get (G_OBJECT (login), "mode", &mode, NULL);
	mode ^= GDA_UI_LOGIN_HIDE_DIRECT_CONNECTION_MODE;
	gdaui_login_set_mode (GDAUI_LOGIN (login), mode);
}

static void
button_clicked_cb (GtkButton *button, GtkWidget *login)
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
login_changed_cb (GdauiLogin *login, gboolean is_valid, GtkLabel *label)
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
		GtkWidget *table, *cb, *button;
		GtkWidget *login, *frame;

		window = gtk_dialog_new_with_buttons ("GdauiLogin widget",
						      GTK_WINDOW (do_widget),
						      0,
						      GTK_STOCK_CLOSE,
						      GTK_RESPONSE_NONE,
						      NULL);
		gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
		g_signal_connect (window, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (window, "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &window);
		

		table = gtk_table_new (3, 2, FALSE);
#if GTK_CHECK_VERSION(2,18,0)
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))),
				    table, TRUE, TRUE, 0);
#else
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), table, TRUE, TRUE, 0);
#endif
		gtk_container_set_border_width (GTK_CONTAINER (table), 5);

		/* Create the login widget */
		frame = gtk_frame_new ("Login widget:");
		gtk_table_attach_defaults (GTK_TABLE (table), frame, 0, 2, 3, 4);

		login = gdaui_login_new (NULL);
		gtk_container_add (GTK_CONTAINER (frame), login);
		
		/* Create the options */
		cb = gtk_check_button_new_with_label ("Enable control center");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), TRUE);
		gtk_table_attach (GTK_TABLE (table), cb, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
		g_signal_connect (cb, "toggled",
				  G_CALLBACK (cb1_toggled_cb), login);
		
		cb = gtk_check_button_new_with_label ("Hide DSN selection");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), FALSE);
		gtk_table_attach (GTK_TABLE (table), cb, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
		g_signal_connect (cb, "toggled",
				  G_CALLBACK (cb2_toggled_cb), login);

		cb = gtk_check_button_new_with_label ("Hide direct connection");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), FALSE);
		gtk_table_attach (GTK_TABLE (table), cb, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
		g_signal_connect (cb, "toggled",
				  G_CALLBACK (cb3_toggled_cb), login);
		
		button = gtk_button_new_with_label ("Show connection's parameters");
		gtk_table_attach (GTK_TABLE (table), button, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
		g_signal_connect (button, "clicked",
				  G_CALLBACK (button_clicked_cb), login);

		GtkWidget *status;
		gboolean valid;
		status = gtk_label_new ("...");
		gtk_table_attach (GTK_TABLE (table), status, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);
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


