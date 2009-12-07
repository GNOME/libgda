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
#include "table-info.h"
#include "../dnd.h"
#include "../support.h"
#include "../cc-gray-bar.h"
#include "table-columns.h"
#ifdef HAVE_GOOCANVAS
#include "table-relations.h"
#endif
#include "schema-browser-perspective.h"
#include "../browser-page.h"
#include "../browser-stock-icons.h"

struct _TableInfoPrivate {
	BrowserConnection *bcnc;

	gchar *schema;
	gchar *table_name;
	gchar *table_short_name;

	CcGrayBar *header;
	GtkWidget *contents; /* notebook with pageO <=> @unknown_table_notice, page1 <=> @pages */
	GtkWidget *unknown_table_notice;
	GtkWidget *pages; /* notebook to store individual pages */
};

static void table_info_class_init (TableInfoClass *klass);
static void table_info_init       (TableInfo *tinfo, TableInfoClass *klass);
static void table_info_dispose   (GObject *object);
static void table_info_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void table_info_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);
/* BrowserPage interface */
static void                 table_info_page_init (BrowserPageIface *iface);
static GtkActionGroup      *table_info_page_get_actions_group (BrowserPage *page);
static const gchar         *table_info_page_get_actions_ui (BrowserPage *page);
static GtkWidget           *table_info_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button);

static void meta_changed_cb (BrowserConnection *bcnc, GdaMetaStruct *mstruct, TableInfo *tinfo);

/* properties */
enum {
        PROP_0,
};

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint table_info_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;


/*
 * TableInfo class implementation
 */

static void
table_info_class_init (TableInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* Properties */
        object_class->set_property = table_info_set_property;
        object_class->get_property = table_info_get_property;

	object_class->dispose = table_info_dispose;
}

static void
table_info_page_init (BrowserPageIface *iface)
{
	iface->i_get_actions_group = table_info_page_get_actions_group;
	iface->i_get_actions_ui = table_info_page_get_actions_ui;
	iface->i_get_tab_label = table_info_page_get_tab_label;
}

static void
table_info_init (TableInfo *tinfo, TableInfoClass *klass)
{
	tinfo->priv = g_new0 (TableInfoPrivate, 1);
}

static void
table_info_dispose (GObject *object)
{
	TableInfo *tinfo = (TableInfo *) object;

	/* free memory */
	if (tinfo->priv) {
		g_free (tinfo->priv->schema);
		g_free (tinfo->priv->table_name);
		g_free (tinfo->priv->table_short_name);
		if (tinfo->priv->bcnc) {
			g_signal_handlers_disconnect_by_func (tinfo->priv->bcnc,
							      G_CALLBACK (meta_changed_cb), tinfo);
			g_object_unref (tinfo->priv->bcnc);
		}

		g_free (tinfo->priv);
		tinfo->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
table_info_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (TableInfoClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) table_info_class_init,
			NULL,
			NULL,
			sizeof (TableInfo),
			0,
			(GInstanceInitFunc) table_info_init
		};

		static GInterfaceInfo page_info = {
                        (GInterfaceInitFunc) table_info_page_init,
			NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_VBOX, "TableInfo", &info, 0);
		g_type_add_interface_static (type, BROWSER_PAGE_TYPE, &page_info);
	}
	return type;
}

static void
table_info_set_property (GObject *object,
			 guint param_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	TableInfo *tinfo;
	tinfo = TABLE_INFO (object);
	switch (param_id) {
	}
}

static void
table_info_get_property (GObject *object,
			 guint param_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	TableInfo *tinfo;
	tinfo = TABLE_INFO (object);
	switch (param_id) {
	}
}

static void
display_table_not_found_error (TableInfo *tinfo, gboolean is_error)
{
	if (is_error)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (tinfo->priv->contents), 0);
	else
		gtk_notebook_set_current_page (GTK_NOTEBOOK (tinfo->priv->contents), 1);
}

static gchar *
table_info_to_selection (TableInfo *tinfo)
{
	GString *string;
	gchar *tmp;
	string = g_string_new ("OBJ_TYPE=table");
	tmp = gda_rfc1738_encode (tinfo->priv->schema);
	g_string_append_printf (string, ";OBJ_SCHEMA=%s", tmp);
	g_free (tmp);
	tmp = gda_rfc1738_encode (tinfo->priv->table_name);
	g_string_append_printf (string, ";OBJ_NAME=%s", tmp);
	g_free (tmp);
	tmp = gda_rfc1738_encode (tinfo->priv->table_short_name);
	g_string_append_printf (string, ";OBJ_SHORT_NAME=%s", tmp);
	g_free (tmp);
	return g_string_free (string, FALSE);
}

static void
source_drag_data_get_cb (GtkWidget *widget, GdkDragContext *context,
			 GtkSelectionData *selection_data,
			 guint info, guint time, TableInfo *tinfo)
{
	switch (info) {
	case TARGET_KEY_VALUE: {
		gchar *str;
		str = table_info_to_selection (tinfo);
#if GTK_CHECK_VERSION(2,18,0)
		gtk_selection_data_set (selection_data,
					gtk_selection_data_get_target (selection_data), 8, (guchar*) str,
					strlen (str));
#else
		gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar*) str,
					strlen (str));
#endif
		g_free (str);
		break;
	}
	default:
	case TARGET_PLAIN: {
		gchar *str;
		str = g_strdup_printf ("%s.%s", tinfo->priv->schema, tinfo->priv->table_name);
		gtk_selection_data_set_text (selection_data, str, -1);
		g_free (str);
		break;
	}
	case TARGET_ROOTWIN:
		TO_IMPLEMENT; /* dropping on the Root Window => create a file */
		break;
	}
}

static void
meta_changed_cb (BrowserConnection *bcnc, GdaMetaStruct *mstruct, TableInfo *tinfo)
{
	GdaMetaDbObject *dbo;
	GValue *schema_v = NULL, *name_v;

	g_value_set_string ((schema_v = gda_value_new (G_TYPE_STRING)), tinfo->priv->schema);
	g_value_set_string ((name_v = gda_value_new (G_TYPE_STRING)), tinfo->priv->table_name);
	dbo = gda_meta_struct_get_db_object (mstruct, NULL, schema_v, name_v);
	if (schema_v)
		gda_value_free (schema_v);
	gda_value_free (name_v);

	if (tinfo->priv->table_short_name) {
		g_free (tinfo->priv->table_short_name);
		tinfo->priv->table_short_name = NULL;

		gtk_drag_source_unset ((GtkWidget *) tinfo->priv->header);
		g_signal_handlers_disconnect_by_func (tinfo->priv->header,
						      G_CALLBACK (source_drag_data_get_cb), tinfo);
	}
	if (dbo) {
		tinfo->priv->table_short_name = g_strdup (dbo->obj_short_name);
		gtk_drag_source_set ((GtkWidget *) tinfo->priv->header, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
				     dbo_table, G_N_ELEMENTS (dbo_table), GDK_ACTION_COPY);
		gtk_drag_source_set_icon_pixbuf ((GtkWidget *) tinfo->priv->header,
						 browser_get_pixbuf_icon (BROWSER_ICON_TABLE));
		g_signal_connect (tinfo->priv->header, "drag-data-get",
				  G_CALLBACK (source_drag_data_get_cb), tinfo);
	}

	display_table_not_found_error (tinfo, dbo ? FALSE : TRUE);
}

/**
 * table_info_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
table_info_new (BrowserConnection *bcnc,
		const gchar *schema, const gchar *table)
{
	TableInfo *tinfo;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	g_return_val_if_fail (schema, NULL);
	g_return_val_if_fail (table, NULL);

	tinfo = TABLE_INFO (g_object_new (TABLE_INFO_TYPE, NULL));

	tinfo->priv->bcnc = g_object_ref (bcnc);
	g_signal_connect (tinfo->priv->bcnc, "meta-changed",
			  G_CALLBACK (meta_changed_cb), tinfo);
	tinfo->priv->schema = g_strdup (schema);
	tinfo->priv->table_name = g_strdup (table);
	
	/* header */
        GtkWidget *label;
	gchar *str, *tmp;

	tmp = g_strdup_printf ("In schema '%s'", schema);
	str = g_strdup_printf ("<b>%s</b>\n%s", table, tmp);
	g_free (tmp);
	label = cc_gray_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (tinfo), label, FALSE, FALSE, 0);
        gtk_widget_show (label);
	tinfo->priv->header = CC_GRAY_BAR (label);

	/* main contents */
	GtkWidget *top_nb;
	top_nb = gtk_notebook_new ();
	tinfo->priv->contents = top_nb;
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (top_nb), GTK_POS_BOTTOM);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (top_nb), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (top_nb), FALSE);
	gtk_box_pack_start (GTK_BOX (tinfo), top_nb, TRUE, TRUE, 0);	

	/* "table not found" error page */
	GtkWidget *image, *hbox;

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (""), TRUE, TRUE, 0);
	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 10);
	label = gtk_label_new (_("Table not found. If you think this is an error,\n"
				 "please refresh the meta data from the database\n"
				 "(menu Connection/Fetch meta data)."));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (""), TRUE, TRUE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (top_nb), hbox, NULL);
	tinfo->priv->unknown_table_notice = label;

	/* notebook for the pages */
	GtkWidget *sub_nb;
	sub_nb = gtk_notebook_new ();
	tinfo->priv->pages = sub_nb;
	gtk_notebook_append_page (GTK_NOTEBOOK (top_nb), sub_nb, NULL);
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (sub_nb), GTK_POS_BOTTOM);

	/* append pages */
	GtkWidget *page;
	page = table_columns_new (tinfo);
	if (page) {
		label = gtk_label_new ("");
		str = g_strdup_printf ("<small>%s</small>", _("Columns"));
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_widget_show (page);
		gtk_notebook_append_page (GTK_NOTEBOOK (sub_nb), page, label);
	}
#ifdef HAVE_GOOCANVAS
	page = table_relations_new (tinfo);
	if (page) {
		label = gtk_label_new ("");
		str = g_strdup_printf ("<small>%s</small>", _("Relations"));
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_widget_show (page);
		gtk_notebook_append_page (GTK_NOTEBOOK (sub_nb), page, label);
	}
#endif
	gtk_notebook_set_current_page (GTK_NOTEBOOK (sub_nb), 0);

	/* show everything */
        gtk_widget_show_all (top_nb);
	display_table_not_found_error (tinfo, TRUE);
	
	GdaMetaStruct *mstruct;
	mstruct = browser_connection_get_meta_struct (tinfo->priv->bcnc);
	if (mstruct)
		meta_changed_cb (tinfo->priv->bcnc, mstruct, tinfo);

	return (GtkWidget*) tinfo;
}

/**
 * table_info_get_table_schema
 */
const gchar *
table_info_get_table_schema (TableInfo *tinfo)
{
	g_return_val_if_fail (IS_TABLE_INFO (tinfo), NULL);
	return tinfo->priv->schema;
}

/**
 * table_info_get_table_name
 */
const gchar *
table_info_get_table_name (TableInfo *tinfo)
{
	g_return_val_if_fail (IS_TABLE_INFO (tinfo), NULL);
	return tinfo->priv->table_name;
}

/**
 * table_info_get_connection
 */
BrowserConnection *
table_info_get_connection (TableInfo *tinfo)
{
	g_return_val_if_fail (IS_TABLE_INFO (tinfo), NULL);
	return tinfo->priv->bcnc;
}

/*
 * UI actions
 */
static void
action_add_to_fav_cb (GtkAction *action, TableInfo *tinfo)
{
	BrowserFavorites *bfav;
        BrowserFavoritesAttributes fav;
        GError *error = NULL;

        memset (&fav, 0, sizeof (BrowserFavoritesAttributes));
        fav.id = -1;
        fav.type = BROWSER_FAVORITES_TABLES;
        fav.name = NULL;
        fav.descr = NULL;
        fav.contents = table_info_to_selection (tinfo);

        bfav = browser_connection_get_favorites (tinfo->priv->bcnc);
        if (! browser_favorites_add (bfav, 0, &fav, ORDER_KEY_SCHEMA, G_MAXINT, &error)) {
                browser_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) tinfo),
                                    _("Could not add favorite: %s"),
                                    error && error->message ? error->message : _("No detail"));
                if (error)
                        g_error_free (error);
        }
	g_free (fav.contents);
}

static GtkActionEntry ui_actions[] = {
	{ "AddToFav", STOCK_ADD_BOOKMARK, N_("_Favorite"), NULL, N_("Add table to favorites"),
	  G_CALLBACK (action_add_to_fav_cb)},
};
static const gchar *ui_actions_info =
	"<ui>"
	"  <menubar name='MenuBar'>"
	"  </menubar>"
	"  <toolbar name='ToolBar'>"
	"    <separator/>"
	"    <toolitem action='AddToFav'/>"
	"  </toolbar>"
	"</ui>";

static GtkActionGroup *
table_info_page_get_actions_group (BrowserPage *page)
{
	GtkActionGroup *agroup;
	agroup = gtk_action_group_new ("SchemaBrowserRelationsDiagramActions");
	gtk_action_group_add_actions (agroup, ui_actions, G_N_ELEMENTS (ui_actions), page);
	
	return agroup;
}

static const gchar *
table_info_page_get_actions_ui (BrowserPage *page)
{
	return ui_actions_info;
}

static GtkWidget *
table_info_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button)
{
	TableInfo *tinfo;
	const gchar *tab_name;
	GdkPixbuf *table_pixbuf;

	tinfo = TABLE_INFO (page);
	table_pixbuf = browser_get_pixbuf_icon (BROWSER_ICON_TABLE);
	tab_name = tinfo->priv->table_short_name ? tinfo->priv->table_short_name : tinfo->priv->table_name;
	return browser_make_tab_label_with_pixbuf (tab_name,
						   table_pixbuf,
						   out_close_button ? TRUE : FALSE, out_close_button);
}
