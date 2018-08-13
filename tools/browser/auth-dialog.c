/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <string.h>
#include "auth-dialog.h"
#include "ui-support.h"
#include <gda-config.h>

/* 
 * Main static functions 
 */
static void auth_dialog_class_init (AuthDialogClass * class);
static void auth_dialog_init (AuthDialog *dialog);
static void auth_dialog_dispose (GObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

typedef struct {
	gchar         *cnc_string;
	TConnection   *tcnc; /* ref held */
	GError        *cnc_open_error;
	GdaDsnInfo     cncinfo;
	GtkWidget     *auth_widget;
	GString       *auth_string;
} AuthData;

static void
auth_data_free (AuthData *ad)
{
	g_free (ad->cncinfo.name);
	g_free (ad->cncinfo.description);
	g_free (ad->cncinfo.provider);
	g_free (ad->cncinfo.cnc_string);
	g_free (ad->cncinfo.auth_string);
	g_free (ad->cnc_string);
	if (ad->auth_string)
		g_string_free (ad->auth_string, TRUE);

	if (ad->cnc_open_error)
		g_error_free (ad->cnc_open_error);
	if (ad->tcnc)
		g_object_unref (ad->tcnc);
	g_free (ad);
}

struct _AuthDialogPrivate
{
	GSList    *auth_list; /* list of AuthData pointers */
	GtkWidget *spinner;
	guint      source_id; /* timer to check if connections have been opened */
};

/* module error */
GQuark auth_dialog_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("auth_dialog_error");
        return quark;
}

GType
auth_dialog_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (AuthDialogClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) auth_dialog_class_init,
			NULL,
			NULL,
			sizeof (AuthDialog),
			0,
			(GInstanceInitFunc) auth_dialog_init,
			0
		};
		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_DIALOG, "AuthDialog", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}


static void
auth_dialog_class_init (AuthDialogClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	/* virtual functions */
	object_class->dispose = auth_dialog_dispose;
}

static void
update_ad_auth (AuthData *ad)
{
	if (ad->auth_widget && ad->cncinfo.auth_string) {
		/* split array in a list of named parameters, and for each parameter value, 
		 * set the correcponding parameter in @dset */
		GdaSet *dset;
		dset = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (ad->auth_widget));
		gchar **array = NULL;
		array = g_strsplit (ad->cncinfo.auth_string, ";", 0);
		if (array) {
			gint index = 0;
			gchar *tok;
			gchar *value;
			gchar *name;
			
			for (index = 0; array[index]; index++) {
				name = strtok_r (array [index], "=", &tok);
				if (name)
					value = strtok_r (NULL, "=", &tok);
				else
					value = NULL;
				if (name && value) {
					GdaHolder *param;
					gda_rfc1738_decode (name);
					gda_rfc1738_decode (value);
					
					param = gda_set_get_holder (dset, name);
					if (param)
						g_assert (gda_holder_set_value_str (param, NULL, value, NULL));
				}
			}
			
			g_strfreev (array);
		}
		gdaui_basic_form_entry_grab_focus (GDAUI_BASIC_FORM (ad->auth_widget), NULL);
	}
}

 /*
  * Update the auth part
  */
static void
dsn_changed_cb (G_GNUC_UNUSED GdaConfig *config, GdaDsnInfo *info, AuthDialog *dialog)
{
	GSList *list;
	if (!info || !info->name) /* should not happen */
		return;
	for (list = dialog->priv->auth_list; list; list = list->next) {
		AuthData *ad = (AuthData*) list->data;
		if (! ad->cncinfo.name || strcmp (info->name, ad->cncinfo.name))
			continue;
		g_free (ad->cncinfo.auth_string);
		ad->cncinfo.auth_string = NULL;
		if (info->auth_string)
			ad->cncinfo.auth_string = g_strdup (info->auth_string);
		update_ad_auth (ad);
	}
}

static void
auth_dialog_init (AuthDialog *dialog)
{
	GtkWidget *label, *hbox, *wid;
	char *markup;
	GtkWidget *dcontents;

	dialog->priv = g_new0 (AuthDialogPrivate, 1);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("C_onnect"), GTK_RESPONSE_ACCEPT,
				_("_Cancel"), GTK_RESPONSE_REJECT, NULL);

	dcontents = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_set_spacing (GTK_BOX (dcontents), 5);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, TRUE);

	GdkPixbuf *pix;
	pix = gdk_pixbuf_new_from_resource ("/images/gda-browser-auth.png", NULL);
	gtk_window_set_icon (GTK_WINDOW (dialog), pix);
	g_object_unref (pix);

	/* label and spinner */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); 
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
	gtk_box_pack_start (GTK_BOX (dcontents), hbox, FALSE, FALSE, 0);
	
	wid = gtk_image_new_from_resource ("/images/gda-browser-auth-big.png");
	gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);

	label = gtk_label_new ("");
	markup = g_markup_printf_escaped ("<big><b>%s\n</b></big>\n",
					  _("Connection opening"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 12);
	
	dialog->priv->spinner = gtk_spinner_new ();
	gtk_box_pack_start (GTK_BOX (hbox), dialog->priv->spinner, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);
	gtk_widget_hide (dialog->priv->spinner);

	GdaConfig *conf = gda_config_get ();
	g_signal_connect (conf, "dsn-changed",
			  G_CALLBACK (dsn_changed_cb), dialog);
	g_object_unref (conf);
}

static void
auth_dialog_dispose (GObject *object)
{
	AuthDialog *dialog;
	dialog = AUTH_DIALOG (object);
	if (dialog->priv) {
		GdaConfig *conf = gda_config_get ();
		g_signal_handlers_disconnect_by_func (conf,
						      G_CALLBACK (dsn_changed_cb), dialog);
		g_object_unref (conf);
		if (dialog->priv->auth_list) {
			g_slist_foreach (dialog->priv->auth_list, (GFunc) auth_data_free, NULL);
			g_slist_free (dialog->priv->auth_list);
		}
		if (dialog->priv->source_id)
			g_source_remove (dialog->priv->source_id);
		g_free (dialog->priv);
		dialog->priv = NULL;
	}

	/* parent class */
        parent_class->dispose (object);
}

/**
 * auth_dialog_new
 *
 * Creates a new dialog dialog
 *
 * Returns: a new #AuthDialog object
 */
AuthDialog *
auth_dialog_new (GtkWindow *parent)
{
	return (AuthDialog*) g_object_new (AUTH_TYPE_DIALOG, "title", _("Authentication"),
					   "transient-for", parent,
					   "resizable", FALSE,
					   "border-width", 10, NULL);
}

static void
real_connection_open (AuthDialog *dialog, AuthData *ad)
{
#ifdef DUMMY
	sleep (5);
	g_set_error (&(ad->cnc_open_error), T_ERROR, TOOLS_INTERNAL_COMMAND_ERROR, "%s", "Dummy error!");
	return;
#endif

	GdaDsnInfo *cncinfo = &(ad->cncinfo);
	if (cncinfo->name)
		ad->tcnc = t_connection_open (NULL, cncinfo->name, ad->auth_string ? ad->auth_string->str : NULL, FALSE, &(ad->cnc_open_error));
	else {
		gchar *tmp;
		tmp = g_strdup_printf ("%s://%s", cncinfo->provider, cncinfo->cnc_string);
		ad->tcnc = t_connection_open (NULL, tmp, ad->auth_string ? ad->auth_string->str : NULL, FALSE, &(ad->cnc_open_error));
		g_free (tmp);
	}
	if (ad->tcnc)
		ad->tcnc = g_object_ref (ad->tcnc);
}

static void
update_dialog_focus (AuthDialog *dialog)
{
	GSList *list;
	gboolean allvalid = TRUE;
	for (list = dialog->priv->auth_list; list; list = list->next) {
		AuthData *ad;
		ad = (AuthData*) list->data;
		if (ad->auth_widget && !ad->tcnc &&
		    ! gdaui_basic_form_is_valid (GDAUI_BASIC_FORM (ad->auth_widget))) {
			allvalid = FALSE;
			gtk_widget_grab_focus (ad->auth_widget);
			break;
		}
	}

	if (allvalid) {
		GtkWidget *wid;
		wid = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
		gtk_widget_grab_focus (wid);
	}
}

static void
auth_form_activated_cb (G_GNUC_UNUSED GdauiBasicForm *form, AuthDialog *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
}

static void
auth_contents_changed_cb (GdauiBasicForm *form, GdaHolder *h, gboolean is_user_modif, AuthDialog *dialog)
{
	GSList *list;
	for (list = dialog->priv->auth_list; list; list = list->next) {
		AuthData *ad = (AuthData*) list->data;
		if (! gdaui_basic_form_is_valid (GDAUI_BASIC_FORM (ad->auth_widget)))
			break;
	}

	gboolean is_valid;
	is_valid = list ? FALSE : TRUE;
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, is_valid);
        if (is_valid)
                gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
}

/**
 * auth_dialog_add_cnc_string
 */
gboolean
auth_dialog_add_cnc_string (AuthDialog *dialog, const gchar *cnc_string, GError **error)
{
	g_return_val_if_fail (AUTH_IS_DIALOG (dialog), FALSE);
	g_return_val_if_fail (cnc_string, FALSE);

	gchar *real_cnc_string;
	GdaDsnInfo *info;
        gchar *user, *pass, *real_cnc, *real_provider, *real_auth_string = NULL;

	/* if cnc string is a regular file, then use it with SQLite or MSAccess */
        if (g_file_test (cnc_string, G_FILE_TEST_IS_REGULAR)) {
                gchar *path, *file, *e1, *e2;
                const gchar *pname = "SQLite";

                path = g_path_get_dirname (cnc_string);
                file = g_path_get_basename (cnc_string);
                if (g_str_has_suffix (file, ".mdb")) {
                        pname = "MSAccess";
                        file [strlen (file) - 4] = 0;
                }
                else if (g_str_has_suffix (file, ".db"))
                        file [strlen (file) - 3] = 0;
                e1 = gda_rfc1738_encode (path);
                e2 = gda_rfc1738_encode (file);
                g_free (path);
                g_free (file);
                real_cnc_string = g_strdup_printf ("%s://DB_DIR=%s;EXTRA_FUNCTIONS=TRUE;DB_NAME=%s", pname, e1, e2);
                g_free (e1);
                g_free (e2);
                gda_connection_string_split (real_cnc_string, &real_cnc, &real_provider, &user, &pass);
        }
        else {
                gda_connection_string_split (cnc_string, &real_cnc, &real_provider, &user, &pass);
                real_cnc_string = g_strdup (cnc_string);
        }
        if (!real_cnc) {
                g_free (user);
                g_free (pass);
                g_free (real_provider);
                g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR,
                             _("Malformed connection string '%s'"), cnc_string);
                g_free (real_cnc_string);
                return FALSE;
        }

	AuthData *ad;
	ad = g_new0 (AuthData, 1);
	ad->cnc_string = g_strdup (cnc_string);
	ad->auth_string = NULL;
	info = gda_config_get_dsn_info (real_cnc);
        if (info && !real_provider) {
		ad->cncinfo.name = g_strdup (info->name);
		ad->cncinfo.provider = g_strdup (info->provider);
		if (info->description)
			ad->cncinfo.description = g_strdup (info->description);
		if (info->cnc_string)
			ad->cncinfo.cnc_string = g_strdup (info->cnc_string);
		if (info->auth_string)
			ad->cncinfo.auth_string = g_strdup (info->auth_string);
	}
	else {
		ad->cncinfo.name = NULL;
		ad->cncinfo.provider = real_provider;
		real_provider = NULL;
		ad->cncinfo.cnc_string = real_cnc;
		real_cnc = NULL;
		ad->cncinfo.auth_string = real_auth_string;
		real_auth_string = NULL;
	}

	if (! ad->cncinfo.provider) {
		g_free (user);
                g_free (pass);
                g_free (real_provider);
                g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR,
                             _("Malformed connection string '%s'"), cnc_string);
                g_free (real_cnc_string);
		auth_data_free (ad);
		return FALSE;
	}

	if (user || pass) {
		gchar *s1;
		s1 = gda_rfc1738_encode (user);
		if (pass) {
			gchar *s2;
			s2 = gda_rfc1738_encode (pass);
			real_auth_string = g_strdup_printf ("USERNAME=%s;PASSWORD=%s", s1, s2);
			g_free (s2);
		}
		else
			real_auth_string = g_strdup_printf ("USERNAME=%s", s1);
		g_free (s1);
	}
	if (real_auth_string) {
		if (ad->cncinfo.auth_string)
			g_free (ad->cncinfo.auth_string);
		ad->cncinfo.auth_string = real_auth_string;
		real_auth_string = NULL;
	}

	dialog->priv->auth_list = g_slist_append (dialog->priv->auth_list, ad);

	/* build widget */
	gboolean auth_needed = FALSE;
	GdaProviderInfo *pinfo;
	pinfo = gda_config_get_provider_info (ad->cncinfo.provider);
	if (pinfo  && pinfo->auth_params && gda_set_get_holders (pinfo->auth_params))
		auth_needed = TRUE;
	if (auth_needed) {
		GdaSet *set;

                set = gda_set_copy (pinfo->auth_params);
                ad->auth_widget = gdaui_basic_form_new (set);
                g_signal_connect (G_OBJECT (ad->auth_widget), "activated",
				  G_CALLBACK (auth_form_activated_cb), dialog);
                g_signal_connect (G_OBJECT (ad->auth_widget), "holder-changed",
				  G_CALLBACK (auth_contents_changed_cb), dialog);
                g_object_unref (set);

		/* add widget */
		GtkWidget *hbox, *label;
		gchar *str, *tmp, *ptr;
		GtkWidget *dcontents;

		dcontents = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
		label = gtk_label_new ("");
		tmp = g_strdup (ad->cnc_string);
		for (ptr = tmp; *ptr; ptr++) {
			if (*ptr == ':') {
				/* remove everything up to the '@' */
				gchar *ptr2;
				for (ptr2 = ptr+1; *ptr2; ptr2++) {
					if (*ptr2 == '@') {
						memmove (ptr, ptr2, strlen (ptr2) + 1);
						break;
					}
				}
				break;
			}
		}
		str = g_strdup_printf ("<b>%s: %s</b>\n%s", _("For connection"), tmp,
				       _("enter authentication information"));
		g_free (tmp);
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (dcontents), label, FALSE, FALSE, 0);
		gtk_widget_show (label);

		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
		gtk_box_pack_start (GTK_BOX (dcontents), hbox, TRUE, TRUE, 0);
		label = gtk_label_new ("      ");
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), ad->auth_widget, TRUE, TRUE, 0);
		gtk_widget_show_all (hbox);

		/* set values */
		if (ad->cncinfo.auth_string)
			update_ad_auth (ad);
	}
	else {
		/* open connection right away */
		real_connection_open (dialog, ad);
	}
	
	g_free (real_cnc_string);
        g_free (real_cnc);
        g_free (user);
        g_free (pass);
        g_free (real_provider);
        g_free (real_auth_string);

	update_dialog_focus (dialog);

	return TRUE;
}

/**
 * auth_dialog_run
 * @dialog: a #GdaAuth object
 * @retry: if set to %TRUE, then this method returns only when either a connection has been opened or the
 *         user gave up
 * @error: a place to store errors, or %NULL
 *
 * Displays the dialog and let the user open some connections. this function returns either only when
 * all the connections have been opened, or when the user cancelled.
 *
 * Return: %TRUE if all the connections have been opened, and %FALSE if the user cancelled.
 */
gboolean
auth_dialog_run (AuthDialog *dialog)
{
	gboolean allopened = FALSE;

	g_return_val_if_fail (AUTH_IS_DIALOG (dialog), FALSE);
	gtk_widget_show (GTK_WIDGET (dialog));
	
	while (1) {
		gint result;
		GSList *list;
		gboolean needs_running = FALSE;

		/* determine if we need to run the dialog */
		for (list = dialog->priv->auth_list; list; list = list->next) {
			AuthData *ad;
			ad = (AuthData *) list->data;
			if (ad->auth_widget) {
				needs_running = TRUE;
				break;
			}
		}
		
		if (needs_running)
			result = gtk_dialog_run (GTK_DIALOG (dialog));
		else
			result = GTK_RESPONSE_ACCEPT;

		gtk_widget_show (dialog->priv->spinner);
		gtk_spinner_start (GTK_SPINNER (dialog->priv->spinner));

		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, FALSE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT, FALSE);
		
		if (result == GTK_RESPONSE_ACCEPT) {
			for (list = dialog->priv->auth_list; list; list = list->next) {
				AuthData *ad;
				ad = (AuthData *) list->data;
				if (ad->auth_widget) {
					GSList *plist;
					GdaSet *set;
					set = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (ad->auth_widget));
					if (ad->auth_string) {
						g_string_free (ad->auth_string, TRUE);
						ad->auth_string = NULL;
					}
					for (plist = set ? gda_set_get_holders (set) : NULL;
					     plist; plist = plist->next) {
						GdaHolder *holder = GDA_HOLDER (plist->data);
						const GValue *cvalue = NULL;
						if (gda_holder_is_valid (holder))
							cvalue = gda_holder_get_value (holder);
						if (cvalue && (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL)) {
							gchar *r1, *r2;
							r1 = gda_value_stringify (cvalue);
							r2 = gda_rfc1738_encode (r1);
							g_free (r1);
							if (r2) {
								r1 = gda_rfc1738_encode (gda_holder_get_id (holder));
								if (ad->auth_string)
									g_string_append_c (ad->auth_string, ';');
								else
									ad->auth_string = g_string_new ("");
								g_string_append (ad->auth_string, r1);
								g_string_append_c (ad->auth_string, '=');
								g_string_append (ad->auth_string, r2);
							
								g_free (r1);
								g_free (r2);
							}
						}
					}
					gtk_widget_set_sensitive (ad->auth_widget, FALSE);
					real_connection_open (dialog, ad);
				}
			}

			allopened = TRUE;
			for (list = dialog->priv->auth_list; list; list = list->next) {
				AuthData *ad;
				
				ad = (AuthData *) list->data;
				if (ad->auth_widget && !ad->tcnc) {
					g_print ("ERROR: %s\n", ad->cnc_open_error && ad->cnc_open_error->message ?
						 ad->cnc_open_error->message : _("No detail"));
					ui_show_error (GTK_WINDOW (dialog), _("Could not open connection:\n%s"),
						       ad->cnc_open_error && ad->cnc_open_error->message ?
						       ad->cnc_open_error->message : _("No detail"));
					allopened = FALSE;
					gtk_widget_set_sensitive (ad->auth_widget, TRUE);
				}
			}
			if (allopened)
				goto out;
		}
		else {
			/* cancelled connection opening */
			goto out;
		}
		
		gtk_spinner_stop (GTK_SPINNER (dialog->priv->spinner));
		gtk_widget_hide (dialog->priv->spinner);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, TRUE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT, TRUE);
	}

 out:
	return allopened;
}
