/* gdaui-entry-import.c
 *
 * Copyright (C) 2009 - 2010 Vivien Malerba
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

#include "gdaui-entry-import.h"
#include <libgda/gda-data-handler.h>
#include <glib/gi18n-lib.h>
#include "gdaui-data-import.h"

/* 
 * Main static functions 
 */
static void gdaui_entry_import_class_init (GdauiEntryImportClass * class);
static void gdaui_entry_import_init (GdauiEntryImport * srv);
static void gdaui_entry_import_dispose (GObject   * object);
static void gdaui_entry_import_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static gboolean   can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryImportPrivate
{
	GtkWidget     *button;
	GdaDataModel  *model;
	GtkLabel      *label;
	GCallback      modify_cb;
};


GType
gdaui_entry_import_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryImportClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_import_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryImport),
			0,
			(GInstanceInitFunc) gdaui_entry_import_init
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryImport", &info, 0);
	}
	return type;
}

static void
gdaui_entry_import_class_init (GdauiEntryImportClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_import_dispose;
	object_class->finalize = gdaui_entry_import_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->can_expand = can_expand;
}

static void
gdaui_entry_import_init (GdauiEntryImport * gdaui_entry_import)
{
	gdaui_entry_import->priv = g_new0 (GdauiEntryImportPrivate, 1);
	gdaui_entry_import->priv->button = NULL;
	gdaui_entry_import->priv->model = NULL;
}

/**
 * gdaui_entry_import_new
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_import_new (GType type)
{
	GObject *obj;
	GdauiEntryImport *mgtxt;

	g_return_val_if_fail (type == GDA_TYPE_DATA_MODEL, NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_IMPORT, NULL);
	mgtxt = GDAUI_ENTRY_IMPORT (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgtxt), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_import_dispose (GObject   * object)
{
	GdauiEntryImport *gdaui_entry_import;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_IMPORT (object));

	gdaui_entry_import = GDAUI_ENTRY_IMPORT (object);
	if (gdaui_entry_import->priv) {
		if (gdaui_entry_import->priv->model) {
			g_object_unref (gdaui_entry_import->priv->model);
			gdaui_entry_import->priv->model = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_import_finalize (GObject   * object)
{
	GdauiEntryImport *gdaui_entry_import;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_IMPORT (object));

	gdaui_entry_import = GDAUI_ENTRY_IMPORT (object);
	if (gdaui_entry_import->priv) {

		g_free (gdaui_entry_import->priv);
		gdaui_entry_import->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

typedef void (*Callback2) (gpointer, gpointer);

static void
open_button_clicked_cb (GtkWidget *button, GdauiEntryImport *mgtxt)
{
	GtkWidget *dialog, *wid;
	gint res;
	dialog = gtk_dialog_new_with_buttons (_("Data set import from file"),
					      (GtkWindow*) gtk_widget_get_toplevel (button),
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_ACCEPT,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_REJECT,
					      NULL);
	wid = gdaui_data_import_new ();
#if GTK_CHECK_VERSION(2,18,0)
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), wid);
#else
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), wid);
#endif
	gtk_widget_show_all (dialog);
	
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (dialog);
	if (res == GTK_RESPONSE_ACCEPT) {
		if (mgtxt->priv->model)
			g_object_unref (mgtxt->priv->model);
		mgtxt->priv->model = gdaui_data_import_get_model (GDAUI_DATA_IMPORT (wid));
		if (mgtxt->priv->model) {
			g_object_ref (mgtxt->priv->model);

			gchar *str;
			gint n, c;
			gchar *tmp1, *tmp2;
			n = gda_data_model_get_n_rows (GDA_DATA_MODEL (mgtxt->priv->model));
			c = gda_data_model_get_n_columns (GDA_DATA_MODEL (mgtxt->priv->model));
			tmp1 = g_strdup_printf (ngettext ("%d row", "%d rows", n), n);
			tmp2 = g_strdup_printf (ngettext ("%d column", "%d columns", c), c);
			str = g_strdup_printf (_("Data set with %s and %s"), tmp1, tmp2);
			g_free (tmp1);
			g_free (tmp2);
			gtk_label_set_text (mgtxt->priv->label, str);
			g_free (str);
		}
		else
			gtk_label_set_text (mgtxt->priv->label, _("No data set"));
		

		/* send notofocations */
		if (mgtxt->priv->modify_cb)
			((Callback2)mgtxt->priv->modify_cb) (NULL, mgtxt);
	}
	gtk_widget_destroy (dialog);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryImport *mgtxt;
	GtkWidget *hbox;
	gfloat ya;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_IMPORT (mgwrap), NULL);
	mgtxt = GDAUI_ENTRY_IMPORT (mgwrap);
	g_return_val_if_fail (mgtxt->priv, NULL);

	hbox = gtk_hbox_new (FALSE, 0);

	mgtxt->priv->label = GTK_LABEL (gtk_label_new (_("No data set")));
	gtk_misc_get_alignment (GTK_MISC (mgtxt->priv->label), NULL, &ya);
	gtk_misc_set_alignment (GTK_MISC (mgtxt->priv->label), 0., ya);
	gtk_widget_show ((GtkWidget*) mgtxt->priv->label);
	gtk_box_pack_start (GTK_BOX (hbox), (GtkWidget*) mgtxt->priv->label, TRUE, TRUE, 0);

	mgtxt->priv->button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
	g_signal_connect (mgtxt->priv->button, "clicked",
			  G_CALLBACK (open_button_clicked_cb), mgtxt);
	gtk_widget_show (mgtxt->priv->button);
	gtk_box_pack_start (GTK_BOX (hbox), mgtxt->priv->button, FALSE, FALSE, 0);

	return hbox;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryImport *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_IMPORT (mgwrap));
	mgtxt = GDAUI_ENTRY_IMPORT (mgwrap);
	g_return_if_fail (mgtxt->priv);

	if (mgtxt->priv->model) {
		g_object_unref (mgtxt->priv->model);
		mgtxt->priv->model = NULL;
	}

	if (value) {
		if (! gda_value_is_null ((GValue *) value))
			mgtxt->priv->model = g_value_dup_object (value);
	}
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryImport *mgtxt;

	g_return_val_if_fail (GDAUI_IS_ENTRY_IMPORT (mgwrap), NULL);
	mgtxt = GDAUI_ENTRY_IMPORT (mgwrap);

	if (mgtxt->priv->model) {
		GValue *value;
		value = gda_value_new (GDA_TYPE_DATA_MODEL);
		g_value_set_object (value, mgtxt->priv->model);
		return value;
	}
	else
		return gda_value_new_null ();
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryImport *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_IMPORT (mgwrap));
	mgtxt = GDAUI_ENTRY_IMPORT (mgwrap);
	g_return_if_fail (mgtxt->priv);

	mgtxt->priv->modify_cb = modify_cb;
}

static gboolean
can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz)
{
	return FALSE;
}
