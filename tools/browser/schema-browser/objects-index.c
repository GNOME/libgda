/*
 * Copyright (C) 2009 The GNOME Foundation
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgda/gda-tree.h>
#include "objects-index.h"
#include <libgdaui/gdaui-tree-store.h>
#include "../dnd.h"
#include "../support.h"
#include "../cc-gray-bar.h"

struct _ObjectsIndexPrivate {
	BrowserConnection *bcnc;
	GtkWidget         *main;
};

static void objects_index_class_init (ObjectsIndexClass *klass);
static void objects_index_init       (ObjectsIndex *index,
				       ObjectsIndexClass *klass);
static void objects_index_dispose   (GObject *object);

static void meta_changed_cb (BrowserConnection *bcnc, GdaMetaStruct *mstruct, ObjectsIndex *index);

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint objects_index_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;


/*
 * ObjectsIndex class implementation
 */

static void
objects_index_class_init (ObjectsIndexClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	objects_index_signals [SELECTION_CHANGED] =
                g_signal_new ("selection-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (ObjectsIndexClass, selection_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING, G_TYPE_NONE,
                              1, G_TYPE_STRING);
	klass->selection_changed = NULL;

	object_class->dispose = objects_index_dispose;
}


static void
objects_index_init (ObjectsIndex *index, ObjectsIndexClass *klass)
{
	index->priv = g_new0 (ObjectsIndexPrivate, 1);
}

static void
objects_index_dispose (GObject *object)
{
	ObjectsIndex *index = (ObjectsIndex *) object;

	/* free memory */
	if (index->priv) {
		if (index->priv->bcnc) {
			g_signal_handlers_disconnect_by_func (index->priv->bcnc,
							      G_CALLBACK (meta_changed_cb), index);
			g_object_unref (index->priv->bcnc);
		}
		g_free (index->priv);
		index->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
objects_index_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (ObjectsIndexClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) objects_index_class_init,
			NULL,
			NULL,
			sizeof (ObjectsIndex),
			0,
			(GInstanceInitFunc) objects_index_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "ObjectsIndex",
					       &info, 0);
	}
	return type;
}

typedef struct {
	gchar *schema;
	GtkWidget *container;
	GtkWidget *objects;
	gint left;
	gint top;
	gint nrows;
} SchemaData;
static void add_to_schema_data (ObjectsIndex *index, SchemaData *sd, GdaMetaDbObject *dbo);
static void
update_display (ObjectsIndex *index)
{
#define NCOLS 4
	gchar *str;
	GSList *schemas = NULL; /* holds pointers to @SchemaData structures */
	SchemaData *default_sd = NULL, *sd;

	if (index->priv->main) {
		gtk_widget_destroy (index->priv->main);
		index->priv->main = NULL;
	}

	GdaMetaStruct *mstruct;
	GSList *dbo_list, *list;
	mstruct = browser_connection_get_meta_struct (index->priv->bcnc);
	if (!mstruct) {
		/* not yet ready */
		return;
	}
	dbo_list = gda_meta_struct_get_all_db_objects (mstruct);
	for (list = dbo_list; list; list = list->next) {
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (list->data);
		GSList *list;
		gboolean is_default;

		if (dbo->obj_type != GDA_META_DB_TABLE)
			continue;
		g_assert (dbo->obj_schema);

		is_default = strcmp (dbo->obj_short_name, dbo->obj_full_name) ? TRUE : FALSE;
		sd = NULL;
		for (list = schemas; list; list = list->next) {
			if (!strcmp (((SchemaData *) list->data)->schema, dbo->obj_schema)) {
				sd = (SchemaData *) list->data;
				break;
			}
		}
		if (is_default && !default_sd) {
			GtkWidget *label;

			default_sd = g_new0 (SchemaData, 1);
			default_sd->schema = NULL;
			default_sd->objects = gtk_table_new (2, NCOLS, FALSE);
			default_sd->container = default_sd->objects;
			
			/*
			str = g_strdup_printf ("<b>%s:</b>", _("Tables"));
			label = gtk_label_new ("");
			gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
			gtk_label_set_markup (GTK_LABEL (label), str);
			g_free (str);
			gtk_table_attach (GTK_TABLE (default_sd->objects), label, 0, NCOLS, 0, 1,
					  GTK_EXPAND | GTK_FILL, 0, 0, 0);
			gtk_table_set_row_spacing (GTK_TABLE (default_sd->objects), 0, 5);
			*/

			default_sd->left = 0;
			default_sd->top = 1;
			default_sd->nrows = 1;
		}
		if (!sd) {
			GtkWidget *hbox, *label, *icon;

			sd = g_new0 (SchemaData, 1);
			sd->schema = g_strdup (dbo->obj_schema);
			sd->left = 0;
			sd->top = 0;
			sd->nrows = 1;
			
			sd->objects = gtk_table_new (2, NCOLS, FALSE);
			
			str = g_strdup_printf (_("Tables in schema <b>'%s'</b>"), sd->schema);
			label = gtk_label_new ("");
			gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
			gtk_label_set_markup (GTK_LABEL (label), str);
			g_free (str);

			icon = gtk_image_new_from_pixbuf (browser_get_pixbuf_icon (BROWSER_ICON_SCHEMA));
			hbox = gtk_hbox_new (FALSE, 0);
			gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
			gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);
			gtk_widget_show_all (hbox);

			sd->container = gtk_expander_new ("");
			gtk_expander_set_label_widget (GTK_EXPANDER (sd->container), hbox);

			gtk_container_add (GTK_CONTAINER (sd->container), sd->objects);
			schemas = g_slist_append (schemas, sd);
		}

		add_to_schema_data (index, sd, dbo);
		if (is_default)
			add_to_schema_data (index, default_sd, dbo);
	}
	g_slist_free (dbo_list);

	if (default_sd)
		schemas = g_slist_prepend (schemas, default_sd);

	GtkWidget *sw, *vbox;
	sw = gtk_scrolled_window_new (NULL, NULL);
	index->priv->main = sw;
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (index), sw, TRUE, TRUE, 0);
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), vbox);
	for (list = schemas; list; list = list->next) {
		sd = (SchemaData*) list->data;

		if (sd->nrows == 1) {
			/* add some padding */
			gint i;
			for (i = sd->left; i < NCOLS; i++)
				add_to_schema_data (index, sd, NULL);
		}

		gtk_box_pack_start (GTK_BOX (vbox), sd->container, FALSE, FALSE, 5);
		g_free (sd->schema);
		g_free (sd);
	}
	gtk_widget_show_all (sw);
}

static gchar *
double_underscores (const gchar *str)
{
	gchar *out, *outptr;
	gint len;
	if (!str)
		return NULL;

	len = strlen (str);
	out = g_malloc (sizeof (gchar) * (len * 2 +1));
	for (outptr = out; *str; str++, outptr++) {
		if (*str == '_') {
			*outptr = '_';
			outptr++;
		}
		*outptr = *str;
	}
	*outptr = 0;

	return out;
}

static void objects_button_clicked_cb (GtkWidget *button, ObjectsIndex *index);
static void source_drag_data_get_cb (GtkWidget *widget, GdkDragContext *context,
				     GtkSelectionData *selection_data,
				     guint info, guint time, ObjectsIndex *index);

static void
add_to_schema_data (ObjectsIndex *index, SchemaData *sd, GdaMetaDbObject *dbo)
{
	GtkWidget *wid, *icon;
	
	wid = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (wid), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (wid), FALSE);
	if (dbo) {
		icon = gtk_image_new_from_pixbuf (browser_get_pixbuf_icon (BROWSER_ICON_TABLE));
		gtk_button_set_alignment (GTK_BUTTON (wid), 0., -1);
		gtk_button_set_image (GTK_BUTTON (wid), icon);

		gchar *str;
		str = double_underscores (dbo->obj_name);
		gtk_button_set_label (GTK_BUTTON (wid), str);
		g_free (str);

		g_object_set_data_full (G_OBJECT (wid), "dbo_obj_schema", g_strdup (dbo->obj_schema), g_free);
		g_object_set_data_full (G_OBJECT (wid), "dbo_obj_name", g_strdup (dbo->obj_name), g_free);
		g_object_set_data_full (G_OBJECT (wid), "dbo_obj_short_name", g_strdup (dbo->obj_short_name), g_free);
		g_signal_connect (wid, "clicked",
				  G_CALLBACK (objects_button_clicked_cb), index);

		/* setup DnD */
		gtk_drag_source_set (wid, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
				     dbo_table, G_N_ELEMENTS (dbo_table), GDK_ACTION_COPY);
		gtk_drag_source_set_icon_pixbuf (wid, browser_get_pixbuf_icon (BROWSER_ICON_TABLE));
		g_signal_connect (wid, "drag-data-get",
				  G_CALLBACK (source_drag_data_get_cb), index);
	}
	else {
		gtk_button_set_label (GTK_BUTTON (wid), "             ");
		gtk_widget_set_sensitive (wid, FALSE);
	}

	gtk_button_set_use_underline (GTK_BUTTON (wid), FALSE);
	if (! sd->schema && dbo) {
		gchar *str;
		str = g_strdup_printf (_("Objects in the '%s' schema"), dbo->obj_schema);
		gtk_widget_set_tooltip_text (wid, str);
		g_free (str);
	}
	gtk_table_attach (GTK_TABLE (sd->objects), wid, sd->left, sd->left + 1, sd->top, sd->top +1,
			  GTK_EXPAND | GTK_FILL, 0, 5, 0);
	sd->left ++;
	if (sd->left >= NCOLS) {
		sd->left = 0;
		sd->top ++;
		sd->nrows ++;
	}
}

static void
objects_button_clicked_cb (GtkWidget *button, ObjectsIndex *index)
{
	const gchar *schema;
	const gchar *objects_name, *short_name;
	gchar *str, *tmp1, *tmp2, *tmp3;

	schema = g_object_get_data (G_OBJECT (button), "dbo_obj_schema");
	objects_name = g_object_get_data (G_OBJECT (button), "dbo_obj_name");
	short_name = g_object_get_data (G_OBJECT (button), "dbo_obj_short_name");
	
	tmp1 = gda_rfc1738_encode (schema);
	tmp2 = gda_rfc1738_encode (objects_name);
	tmp3 = gda_rfc1738_encode (short_name);
	str = g_strdup_printf ("OBJ_TYPE=table;OBJ_SCHEMA=%s;OBJ_NAME=%s;OBJ_SHORT_NAME=%s", tmp1, tmp2, tmp3);
	g_free (tmp1);
	g_free (tmp2);
	g_free (tmp3);
	g_signal_emit (index, objects_index_signals [SELECTION_CHANGED], 0, str);
	g_free (str);
}

static void
source_drag_data_get_cb (GtkWidget *widget, GdkDragContext *context,
			 GtkSelectionData *selection_data,
			 guint info, guint time, ObjectsIndex *index)
{
	const gchar *schema;
	const gchar *objects_name;
	const gchar *objects_short_name;

	schema = g_object_get_data (G_OBJECT (widget), "dbo_obj_schema");
	objects_name = g_object_get_data (G_OBJECT (widget), "dbo_obj_name");
	objects_short_name = g_object_get_data (G_OBJECT (widget), "dbo_obj_short_name");

	switch (info) {
	case TARGET_KEY_VALUE: {
		GString *string;
		gchar *tmp;
		string = g_string_new ("OBJ_TYPE=table");
		tmp = gda_rfc1738_encode (schema);
		g_string_append_printf (string, ";OBJ_SCHEMA=%s", tmp);
		g_free (tmp);
		tmp = gda_rfc1738_encode (objects_name);
		g_string_append_printf (string, ";OBJ_NAME=%s", tmp);
		g_free (tmp);
		tmp = gda_rfc1738_encode (objects_short_name);
		g_string_append_printf (string, ";OBJ_SHORT_NAME=%s", tmp);
		g_free (tmp);
		gtk_selection_data_set (selection_data, selection_data->target, 8, string->str, strlen (string->str));
		g_string_free (string, TRUE);
		break;
	}
	default:
	case TARGET_PLAIN: {
		gchar *str;
		str = g_strdup_printf ("%s.%s", schema, objects_name);
		gtk_selection_data_set_text (selection_data, str, -1);
		g_free (str);
		break;
	}
	case TARGET_ROOTWIN:
		TO_IMPLEMENT; /* dropping on the Root Window => create a file */
		break;
	}
}

/**
 * objects_index_new
 *
 *
 *
 * Returns:
 */
GtkWidget *
objects_index_new (BrowserConnection *bcnc)
{
	ObjectsIndex *index;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	index = OBJECTS_INDEX (g_object_new (OBJECTS_INDEX_TYPE, NULL));

	index->priv->bcnc = g_object_ref (bcnc);
	g_signal_connect (index->priv->bcnc, "meta-changed",
			  G_CALLBACK (meta_changed_cb), index);

	/* header */
	GtkWidget *label;
	gchar *str;

	str = g_strdup_printf ("<b>%s</b>", _("Tables' index"));
	label = cc_gray_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (index), label, FALSE, FALSE, 0);
        gtk_widget_show (label);

	update_display (index);

	return (GtkWidget*) index;
}

static void
meta_changed_cb (BrowserConnection *bcnc, GdaMetaStruct *mstruct, ObjectsIndex *index)
{
	update_display (index);
}
