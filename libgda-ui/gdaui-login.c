/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#include "gdaui-decl.h"
#include <libgda/gda-config.h>
#include <libgda-ui/internal/gdaui-dsn-selector.h>
#include <libgda-ui/gdaui-provider-selector.h>
#include <glib/gi18n-lib.h>
#include <libgda-ui/gdaui-login.h>
#include <libgda-ui/internal/gdaui-provider-spec-editor.h>
#include <libgda-ui/internal/gdaui-provider-auth-editor.h>
#include "gdaui-enum-types.h"
#include <gtk/gtk.h>
#include <string.h>
#include <libgda/binreloc/gda-binreloc.h>

struct _GdauiLoginPrivate {
	GdauiLoginMode mode;

	GdaDsnInfo dsn_info;
	GtkWidget *rb_dsn;
	GtkWidget *rb_prov;

	GtkWidget *dsn_selector;
	GtkWidget *cc_button;

	GtkWidget *prov_selector;
	GtkWidget *cnc_params_editor;

	GtkWidget *auth_widget;
};

static void gdaui_login_class_init   (GdauiLoginClass *klass);
static void gdaui_login_init         (GdauiLogin *login, GdauiLoginClass *klass);
static void gdaui_login_set_property (GObject *object,
				      guint paramid,
				      const GValue *value,
				      GParamSpec *pspec);
static void gdaui_login_get_property (GObject *object,
				      guint param_id,
				      GValue *value,
				      GParamSpec *pspec);
static void gdaui_login_finalize     (GObject *object);

enum {
	PROP_0,
	PROP_DSN,
	PROP_MODE,
	PROP_VALID
};

static GObjectClass *parent_class = NULL;

/* signals */
enum
{
        CHANGED,
        LAST_SIGNAL
};

static gint gdaui_login_signals[LAST_SIGNAL] = { 0 };


static gboolean settings_are_valid (GdauiLogin *login);
static void run_cc_cb (GtkButton *button, GdauiLogin *login);
static void dsn_entry_changed_cb (GdauiDsnSelector *sel, GdauiLogin *login);
static void radio_button_use_dsn_toggled_cb (GtkToggleButton *button, GdauiLogin *login);
static void prov_entry_changed_cb (GdauiProviderSelector *sel, GdauiLogin *login);
static void cnc_params_editor_changed_cb (GdauiProviderSpecEditor *editor, GdauiLogin *login);

/*
 * GdauiLogin class implementation
 */

static void
gdaui_login_class_init (GdauiLoginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = (GtkWidgetClass*) klass;

	parent_class = g_type_class_peek_parent (klass);

	gdaui_login_signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdauiLoginClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__BOOLEAN, G_TYPE_NONE,
                              1, G_TYPE_BOOLEAN);

	klass->changed = NULL;

	object_class->set_property = gdaui_login_set_property;
	object_class->get_property = gdaui_login_get_property;
	object_class->finalize = gdaui_login_finalize;

	widget_class->show_all = gtk_widget_show;
	
	/* add class properties */
	g_object_class_install_property (object_class, PROP_DSN,
					 g_param_spec_string ("dsn", NULL, NULL, NULL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_MODE,
					 g_param_spec_flags ("mode", NULL, NULL, 
							     GDAUI_TYPE_LOGIN_MODE, 0, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_VALID,
					 g_param_spec_boolean ("valid", NULL, NULL,
							       FALSE, G_PARAM_READABLE));
}

static void
config_dsn_changed_cb (G_GNUC_UNUSED GdaConfig *config, GdaDsnInfo *dsn, GdauiLogin *login)
{
	if (!login->priv->prov_selector)
		return;
	gchar *sdsn;
	sdsn = _gdaui_dsn_selector_get_dsn (GDAUI_DSN_SELECTOR (login->priv->dsn_selector));
	if (dsn && dsn->name && sdsn && !strcmp (dsn->name, sdsn)) {
		dsn_entry_changed_cb (GDAUI_DSN_SELECTOR (login->priv->dsn_selector), login);
		g_print ("Update...\n");
	}
}

static void
auth_data_changed_cb (GdauiProviderAuthEditor *auth, GdauiLogin *login)
{
	g_signal_emit (login, gdaui_login_signals [CHANGED], 0, settings_are_valid (login));
}

static void
gdaui_login_init (GdauiLogin *login, G_GNUC_UNUSED GdauiLoginClass *klass)
{
	GtkWidget *grid;
	GtkWidget *wid;
	
	/* allocate the internal structure */
	login->priv = g_new0 (GdauiLoginPrivate, 1);
	
	/* Init the properties*/
	login->priv->mode = GDA_UI_LOGIN_ENABLE_CONTROL_CENTRE_MODE;
	memset (&(login->priv->dsn_info), 0, sizeof (GdaDsnInfo));

	gtk_orientable_set_orientation (GTK_ORIENTABLE (login), GTK_ORIENTATION_VERTICAL);
	
	/* catch DSN definition changes */
	GdaConfig *conf = gda_config_get ();
	g_signal_connect (conf, "dsn-changed",
			  G_CALLBACK (config_dsn_changed_cb), login);
	g_object_unref (conf);

	/* table layout */
	grid = gtk_grid_new ();
	gtk_widget_show (grid);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
	gtk_box_pack_start (GTK_BOX (login), grid, TRUE, TRUE, 0);
	
	/* radio buttons */
	wid = gtk_radio_button_new_with_label (NULL, _("Use data source:"));
	g_signal_connect (wid, "toggled",
			  G_CALLBACK (radio_button_use_dsn_toggled_cb), login);
	gtk_grid_attach (GTK_GRID (grid), wid, 0, 0, 1, 1);
	gtk_widget_show (wid);
	login->priv->rb_dsn = wid;

	wid = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (wid),
							   _("Specify connection:"));
	gtk_grid_attach (GTK_GRID (grid), wid, 0, 1, 1, 1);
	gtk_widget_show (wid);
	login->priv->rb_prov = wid;

	/* widget to specify a DSN to use */
	login->priv->dsn_selector = _gdaui_dsn_selector_new ();
	gtk_widget_show (login->priv->dsn_selector); /* Show the DSN selector */
	gtk_grid_attach (GTK_GRID (grid), login->priv->dsn_selector, 1, 0, 1, 1);
	g_signal_connect (G_OBJECT (login->priv->dsn_selector), "changed",
			  G_CALLBACK (dsn_entry_changed_cb), login);
			  
	/* Create the DSN add button */
	login->priv->cc_button = gtk_button_new_with_label (_("Data sources..."));
	gtk_button_set_image (GTK_BUTTON (login->priv->cc_button),
			      gtk_image_new_from_icon_name ("preferences-system", GTK_ICON_SIZE_BUTTON));
	g_signal_connect (G_OBJECT (login->priv->cc_button), "clicked",
			  G_CALLBACK (run_cc_cb), login);
	gtk_widget_show (login->priv->cc_button);
	gtk_grid_attach (GTK_GRID (grid), login->priv->cc_button, 2, 0, 1, 1);
		
	/* widget to specify a direct connection */
	login->priv->prov_selector = gdaui_provider_selector_new ();
	gtk_grid_attach (GTK_GRID (grid),login->priv->prov_selector, 1, 1, 2, 1);
	gtk_widget_show (login->priv->prov_selector);
	gtk_widget_set_sensitive (login->priv->prov_selector, FALSE);
	g_signal_connect (login->priv->prov_selector, "changed",
			  G_CALLBACK (prov_entry_changed_cb), login);

	login->priv->cnc_params_editor = _gdaui_provider_spec_editor_new (NULL);
	gtk_grid_attach (GTK_GRID (grid), login->priv->cnc_params_editor, 1, 2, 2, 1);
	gtk_widget_show (login->priv->cnc_params_editor);
	gtk_widget_set_sensitive (login->priv->cnc_params_editor, FALSE);
	g_signal_connect (login->priv->cnc_params_editor, "changed",
			  G_CALLBACK (cnc_params_editor_changed_cb), login);
	  
	/* Create the authentication part */
	login->priv->auth_widget = _gdaui_provider_auth_editor_new (NULL);
	gtk_grid_attach (GTK_GRID (grid), login->priv->auth_widget, 1, 3, 2, 1);
	gtk_widget_show (login->priv->auth_widget);
	g_signal_connect (login->priv->auth_widget, "changed",
			  G_CALLBACK (auth_data_changed_cb), login);

	prov_entry_changed_cb (GDAUI_PROVIDER_SELECTOR (login->priv->prov_selector), login);
}

static void
gdaui_login_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdauiLogin *login = (GdauiLogin *) object;

	g_return_if_fail (GDAUI_IS_LOGIN (login));

	switch (param_id) {
	case PROP_DSN: {
		const gchar *dsn;
		dsn = g_value_get_string (value);
		gdaui_login_set_dsn (login, dsn);
		break;
	}
	case PROP_MODE:
		gdaui_login_set_mode (login, g_value_get_flags (value));
		break;
	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_login_get_property (GObject *object,
			       guint param_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	GdauiLogin *login = (GdauiLogin *) object;

	g_return_if_fail (GDAUI_IS_LOGIN (login));

	switch (param_id) {
	case PROP_DSN :
		g_value_set_string (value, login->priv->dsn_info.name);
		break;
	case PROP_MODE:
		g_value_set_flags (value, login->priv->mode);
		break;
	case PROP_VALID:
		g_value_set_boolean (value, settings_are_valid (login));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
clear_dsn_info (GdauiLogin *login)
{
	g_free (login->priv->dsn_info.name);
	login->priv->dsn_info.name = NULL;

	g_free (login->priv->dsn_info.provider);
	login->priv->dsn_info.provider = NULL;

	g_free (login->priv->dsn_info.description);
	login->priv->dsn_info.description = NULL;

	g_free (login->priv->dsn_info.cnc_string);
	login->priv->dsn_info.cnc_string = NULL;

	g_free (login->priv->dsn_info.auth_string);
	login->priv->dsn_info.auth_string = NULL;
}

static void
gdaui_login_finalize (GObject *object)
{
	GdauiLogin *login = (GdauiLogin *) object;

	g_return_if_fail (GDAUI_IS_LOGIN (login));

	GdaConfig *conf = gda_config_get ();
	g_signal_handlers_disconnect_by_func (conf,
					      G_CALLBACK (config_dsn_changed_cb), login);
	g_object_unref (conf);

	/* free memory */
	clear_dsn_info (login);
	g_free (login->priv);
	login->priv = NULL;

	parent_class->finalize (object);
}

GType
gdaui_login_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiLoginClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_login_class_init,
			NULL,
			NULL,
			sizeof (GdauiLogin),
			0,
			(GInstanceInitFunc) gdaui_login_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "GdauiLogin", &info, 0);
	}
	return type;
}

static gboolean
settings_are_valid (GdauiLogin *login)
{
	/* validate cnc information */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (login->priv->rb_dsn))) {
		/* using a DSN */
		gchar *dsn;
		dsn = _gdaui_dsn_selector_get_dsn (GDAUI_DSN_SELECTOR (login->priv->dsn_selector));
		if (dsn)
			g_free (dsn);
		else
			return FALSE;
	}
	else {
		/* using a direct CNC string */
		const gchar *prov;
		prov = gdaui_provider_selector_get_provider (GDAUI_PROVIDER_SELECTOR (login->priv->prov_selector));
		if (!prov)
			return FALSE;

		if (! _gdaui_provider_spec_editor_is_valid (GDAUI_PROVIDER_SPEC_EDITOR (login->priv->cnc_params_editor)))
			return FALSE;
	}

	/* validate authentication */
	if (! _gdaui_provider_auth_editor_is_valid (GDAUI_PROVIDER_AUTH_EDITOR (login->priv->auth_widget)))
		return FALSE;

	return TRUE;
}

static void
radio_button_use_dsn_toggled_cb (GtkToggleButton *button, GdauiLogin *login)
{
	gboolean use_dsn = gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive (login->priv->dsn_selector, use_dsn);
	gtk_widget_set_sensitive (login->priv->prov_selector, !use_dsn);
	gtk_widget_set_sensitive (login->priv->cnc_params_editor, !use_dsn);
	if (use_dsn)
		dsn_entry_changed_cb (GDAUI_DSN_SELECTOR (login->priv->dsn_selector), login);
	else
		prov_entry_changed_cb (GDAUI_PROVIDER_SELECTOR (login->priv->prov_selector), login);
}

static void
run_cc_cb (G_GNUC_UNUSED GtkButton *button, GdauiLogin *login)
{
	gboolean sresult = FALSE;
	GError *lerror = NULL;
	gchar *cmd;

	/* run gda-control-center tool */
#ifdef G_OS_WIN32
#define EXENAME "gda-control-center-" GDA_ABI_VERSION ".exe"
	gchar *argv[] = {EXENAME, NULL};
	cmd = gda_gbr_get_file_path (GDA_BIN_DIR, NULL);
	sresult = g_spawn_async (cmd, argv, NULL, 0, NULL, NULL, NULL, &lerror);
	g_free (cmd);
#else
#define EXENAME "gda-control-center-" GDA_ABI_VERSION
	GAppInfo *appinfo;
	GdkAppLaunchContext *context;
	GdkScreen *screen;
	cmd = gda_gbr_get_file_path (GDA_BIN_DIR, (char *) EXENAME, NULL);
	appinfo = g_app_info_create_from_commandline (cmd,
						      NULL,
						      G_APP_INFO_CREATE_NONE,
						      &lerror);
	g_free (cmd);
	if (!appinfo)
		goto checkerror;

	screen = gtk_widget_get_screen (GTK_WIDGET (login));
	context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
	gdk_app_launch_context_set_screen (context, screen);
	sresult = g_app_info_launch (appinfo, NULL, G_APP_LAUNCH_CONTEXT (context), NULL);
	if (! sresult) {
		g_object_unref (appinfo);
		appinfo = g_app_info_create_from_commandline (EXENAME,
							      NULL,
							      G_APP_INFO_CREATE_NONE,
							      NULL);
		if (!appinfo) {
			g_object_unref (context);
			goto checkerror;
		}
		sresult = g_app_info_launch (appinfo, NULL, G_APP_LAUNCH_CONTEXT (context), &lerror);
	}
	g_object_unref (context);
	g_object_unref (appinfo);
#endif /* G_OS_WIN32 */

 checkerror:
	if (!sresult) {
		GtkWidget *msgdialog;
		GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (login));
		if (!gtk_widget_is_toplevel (toplevel))
			toplevel = NULL;
		msgdialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (toplevel), GTK_DIALOG_MODAL,
								GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
								"<b>%s:</b>\n%s",
								_("Could not execute the Database access control center"),
								lerror && lerror->message ? lerror->message : _("No detail"));

		if (lerror)
			g_error_free (lerror);
		gtk_dialog_run (GTK_DIALOG (msgdialog));
		gtk_widget_destroy (msgdialog);
	}
}

static void
dsn_entry_changed_cb (GdauiDsnSelector *sel, GdauiLogin *login)
{
	gchar *dsn;
        GdaDsnInfo *info = NULL;
        dsn = _gdaui_dsn_selector_get_dsn (sel);

	if (dsn) {
		info = gda_config_get_dsn_info (dsn);
		g_free (dsn);
	}

	/* update prov_selector */
	g_signal_handlers_block_by_func (login->priv->prov_selector, G_CALLBACK (prov_entry_changed_cb), login);
	gdaui_provider_selector_set_provider (GDAUI_PROVIDER_SELECTOR (login->priv->prov_selector),
					      info ? info->provider : NULL);
	g_signal_handlers_unblock_by_func (login->priv->prov_selector, G_CALLBACK (prov_entry_changed_cb), login);

	/* update auth editor */
	_gdaui_provider_auth_editor_set_provider (GDAUI_PROVIDER_AUTH_EDITOR (login->priv->auth_widget),
						 info ? info->provider : NULL);
	_gdaui_provider_auth_editor_set_auth (GDAUI_PROVIDER_AUTH_EDITOR (login->priv->auth_widget),
					     info && info->auth_string ? info->auth_string : NULL);

	/* update spec editor */
	_gdaui_provider_spec_editor_set_provider (GDAUI_PROVIDER_SPEC_EDITOR (login->priv->cnc_params_editor),
						  info ? info->provider : NULL);
	_gdaui_provider_spec_editor_set_specs (GDAUI_PROVIDER_SPEC_EDITOR (login->priv->cnc_params_editor),
					       info ? info->cnc_string : NULL);

	if (login->priv->auth_widget)
		gtk_widget_grab_focus (login->priv->auth_widget);

	g_signal_emit (login, gdaui_login_signals [CHANGED], 0, settings_are_valid (login));
}

static void
prov_entry_changed_cb (GdauiProviderSelector *sel, GdauiLogin *login)
{

	const gchar *prov;
        
	g_signal_handlers_block_by_func (login->priv->dsn_selector, G_CALLBACK (dsn_entry_changed_cb), login);
	_gdaui_dsn_selector_set_dsn (GDAUI_DSN_SELECTOR (login->priv->dsn_selector), NULL);
	g_signal_handlers_unblock_by_func (login->priv->dsn_selector, G_CALLBACK (dsn_entry_changed_cb), login);

        prov = gdaui_provider_selector_get_provider (sel);
	_gdaui_provider_spec_editor_set_provider (GDAUI_PROVIDER_SPEC_EDITOR (login->priv->cnc_params_editor),
						 prov);
	_gdaui_provider_auth_editor_set_provider (GDAUI_PROVIDER_AUTH_EDITOR (login->priv->auth_widget),
						 prov);

	if (login->priv->auth_widget)
		gtk_widget_grab_focus (login->priv->auth_widget);

	g_signal_emit (login, gdaui_login_signals [CHANGED], 0, settings_are_valid (login));
}

static void
cnc_params_editor_changed_cb (G_GNUC_UNUSED GdauiProviderSpecEditor *editor, GdauiLogin *login)
{
	g_signal_emit (login, gdaui_login_signals [CHANGED], 0, settings_are_valid (login));
}

/**
 * gdaui_login_new:
 * @dsn: (allow-none): a data source name, or %NULL
 *
 * Creates a new login widget which enables the user to specify connection parameters.
 *
 * Returns: (transfer full): a new widget
  *
 * Since: 4.2
*/
GtkWidget *
gdaui_login_new (G_GNUC_UNUSED const gchar *dsn)
{
	GtkWidget *login;

	login = GTK_WIDGET(g_object_new (GDAUI_TYPE_LOGIN, NULL));
	return login;
}


/**
 * gdaui_login_set_mode:
 * @login: a #GdauiLogin object
 * @mode: a flag
 *
 * Set how @login operates
 *
 * Since: 4.2
 */
void
gdaui_login_set_mode (GdauiLogin *login, GdauiLoginMode mode)
{
	g_return_if_fail (GDAUI_IS_LOGIN (login));
	login->priv->mode = mode;

	if (mode & (GDA_UI_LOGIN_HIDE_DSN_SELECTION_MODE | GDA_UI_LOGIN_HIDE_DIRECT_CONNECTION_MODE)) {
		gtk_widget_hide (login->priv->rb_dsn);
		gtk_widget_hide (login->priv->rb_prov);
	}
	else {
		gtk_widget_show (login->priv->rb_dsn);
		gtk_widget_show (login->priv->rb_prov);
	}

	if (mode & GDA_UI_LOGIN_HIDE_DSN_SELECTION_MODE) {
		gtk_widget_hide (login->priv->cc_button);
		gtk_widget_hide (login->priv->dsn_selector);
	}
	else {
		if (mode & GDA_UI_LOGIN_ENABLE_CONTROL_CENTRE_MODE)
			gtk_widget_show (login->priv->cc_button);
		else
			gtk_widget_hide (login->priv->cc_button);
		gtk_widget_show (login->priv->dsn_selector);
	}

	if (mode & GDA_UI_LOGIN_HIDE_DIRECT_CONNECTION_MODE) {
		gtk_widget_hide (login->priv->prov_selector);
		gtk_widget_hide (login->priv->cnc_params_editor);
	}
	else {
		gtk_widget_show (login->priv->prov_selector);
		gtk_widget_show (login->priv->cnc_params_editor);
	}

	if ((mode & GDA_UI_LOGIN_HIDE_DSN_SELECTION_MODE) &&
	    !(mode & GDA_UI_LOGIN_HIDE_DIRECT_CONNECTION_MODE))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (login->priv->rb_prov), TRUE);
	else if ((mode & GDA_UI_LOGIN_HIDE_DIRECT_CONNECTION_MODE) &&
		 !(mode & GDA_UI_LOGIN_HIDE_DSN_SELECTION_MODE))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (login->priv->rb_dsn), TRUE);
}

/**
 * gdaui_login_get_connection_information:
 * @login: a #GdauiLogin object
 *
 * Get the information specified in @login as a pointer to a (read-only) #GdaDsnInfo.
 * If the connection is not specified by a DSN, then the 'name' attribute of the returned
 * #GdaDsnInfo will be %NULL, and otherwise it will contain the name of the selected DSN.
 *
 * Returns: (transfer none): a pointer to a (read-only) #GdaDsnInfo.
 *
 * Since: 4.2
 */
const GdaDsnInfo *
gdaui_login_get_connection_information (GdauiLogin *login)
{
	g_return_val_if_fail (GDAUI_IS_LOGIN (login), NULL);

	clear_dsn_info (login);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (login->priv->rb_dsn))) {
		GdaDsnInfo *info = NULL;
		gchar *dsn;
		dsn = _gdaui_dsn_selector_get_dsn (GDAUI_DSN_SELECTOR (login->priv->dsn_selector));
		if (dsn && *dsn)
			info = gda_config_get_dsn_info (dsn);
		g_free (dsn);
		if (info) {
			login->priv->dsn_info.name = g_strdup (info->name);
			if (info->provider)
				login->priv->dsn_info.provider = g_strdup (info->provider);
			if (info->description)
				login->priv->dsn_info.description = g_strdup (info->description);
			if (info->cnc_string)
				login->priv->dsn_info.cnc_string = g_strdup (info->cnc_string);
			login->priv->dsn_info.is_system = info->is_system;
		}
		login->priv->dsn_info.auth_string = _gdaui_provider_auth_editor_get_auth (GDAUI_PROVIDER_AUTH_EDITOR (login->priv->auth_widget));
	}
	else {
		const gchar *str;
		str = gdaui_provider_selector_get_provider (GDAUI_PROVIDER_SELECTOR (login->priv->prov_selector));
		if (str)
			login->priv->dsn_info.provider = g_strdup (str);
		login->priv->dsn_info.cnc_string = _gdaui_provider_spec_editor_get_specs (GDAUI_PROVIDER_SPEC_EDITOR (login->priv->cnc_params_editor));
		login->priv->dsn_info.auth_string = _gdaui_provider_auth_editor_get_auth (GDAUI_PROVIDER_AUTH_EDITOR (login->priv->auth_widget));
	}

	return &(login->priv->dsn_info);
}

/**
 * gdaui_login_set_dsn:
 * @login: a #GdauiLogin object
 * @dsn: (allow-none): a data source name, or %NULL
 *
 * Changes the information displayed in @login, to represent @dsn.
 * If @login's mode has %GDA_UI_LOGIN_HIDE_DSN_SELECTION_MODE, then
 * the DSN information is extracted and displayed in the direct login area.
 *
 * If @dsn is not a declared data source name, then a warning is shown and the result
 * is the same as having passed %NULL for the @dsn argument.
 *
 * In any case @login's mode (set by gdaui_login_set_mode()) is not changed.
 */
void
gdaui_login_set_dsn (GdauiLogin *login, const gchar *dsn)
{
	GdaDsnInfo *cinfo;
	g_return_if_fail (GDAUI_IS_LOGIN (login));
	
	cinfo = gda_config_get_dsn_info (dsn);
	if (!cinfo)
		g_warning (_("Unknown DSN '%s'"), dsn);
	gdaui_login_set_connection_information (login, cinfo);
}

/**
 * gdaui_login_set_connection_information:
 * @login: a #GdauiLogin object
 * @cinfo: a pointer to a structure representing the information to display.
 *
 * Changes the information displayed in @login, to represent @cinfo.
 * If @login's mode has %GDA_UI_LOGIN_HIDE_DIRECT_CONNECTION_MODE, then
 * if @cinfo->name is not %NULL it is displayed in the DSN selector, otherwise
 * a warning is shown and the result
 * is the same as having passed %NULL for the @cinfo argument.
 *
 * In any case @login's mode (set by gdaui_login_set_mode()) is not changed.
 */
void
gdaui_login_set_connection_information (GdauiLogin *login, const GdaDsnInfo *cinfo)
{
	g_return_if_fail (GDAUI_IS_LOGIN (login));
	if (!cinfo) {
		_gdaui_dsn_selector_set_dsn (GDAUI_DSN_SELECTOR (login->priv->dsn_selector), NULL);
	}
	else {
		GdaDsnInfo *info = NULL;
		if (cinfo->name)
			info = gda_config_get_dsn_info (cinfo->name);
		if (info)
			_gdaui_dsn_selector_set_dsn (GDAUI_DSN_SELECTOR (login->priv->dsn_selector), cinfo->name);
		else {
			/* force switch to */
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (login->priv->rb_dsn), FALSE);
		}
		g_signal_handlers_block_by_func (login->priv->prov_selector,
						 G_CALLBACK (prov_entry_changed_cb), login);
		gdaui_provider_selector_set_provider (GDAUI_PROVIDER_SELECTOR (login->priv->prov_selector),
		                                      cinfo->provider);
		g_signal_handlers_unblock_by_func (login->priv->prov_selector,
						   G_CALLBACK (prov_entry_changed_cb), login);

		_gdaui_provider_spec_editor_set_provider (GDAUI_PROVIDER_SPEC_EDITOR (login->priv->cnc_params_editor),
							  cinfo->provider);
		_gdaui_provider_spec_editor_set_specs (GDAUI_PROVIDER_SPEC_EDITOR (login->priv->cnc_params_editor),
						       cinfo->cnc_string);
		_gdaui_provider_auth_editor_set_provider (GDAUI_PROVIDER_AUTH_EDITOR (login->priv->auth_widget),
							  cinfo->provider);
		_gdaui_provider_auth_editor_set_auth (GDAUI_PROVIDER_AUTH_EDITOR (login->priv->auth_widget),
						      cinfo->auth_string);
	}
}
