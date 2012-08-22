/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include "relations-diagram.h"
#include "../support.h"
#include "../gdaui-bar.h"
#include "../canvas/browser-canvas-db-relations.h"
#include <gdk/gdkkeysyms.h>
#include <libgda-ui/internal/popup-container.h>
#include "../browser-page.h"
#include "../browser-perspective.h"
#include "../browser-window.h"
#include "../data-manager/data-manager-perspective.h"
#include "../../tools-utils.h"

struct _RelationsDiagramPrivate {
	BrowserConnection *bcnc;
	gint fav_id; /* diagram's ID as a favorite, -1=>not a favorite */

	GdauiBar *header;
	GtkWidget *canvas;
	GtkWidget *save_button;

	GtkWidget *popup_container; /* to enter canvas's name */
	GtkWidget *name_entry;
	GtkWidget *real_save_button;
};

static void relations_diagram_class_init (RelationsDiagramClass *klass);
static void relations_diagram_init       (RelationsDiagram *diagram, RelationsDiagramClass *klass);
static void relations_diagram_dispose   (GObject *object);
static void relations_diagram_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void relations_diagram_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec);

/* BrowserPage interface */
static void                 relations_diagram_page_init (BrowserPageIface *iface);
static GtkActionGroup      *relations_diagram_page_get_actions_group (BrowserPage *page);
static const gchar         *relations_diagram_page_get_actions_ui (BrowserPage *page);
static GtkWidget           *relations_diagram_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button);

static void meta_changed_cb (BrowserConnection *bcnc, GdaMetaStruct *mstruct, RelationsDiagram *diagram);
static void favorites_changed_cb (BrowserConnection *bcnc, RelationsDiagram *diagram);
static void relations_diagram_set_fav_id (RelationsDiagram *diagram, gint fav_id, GError **error);

/* properties */
enum {
        PROP_0,
};

static GObjectClass *parent_class = NULL;


/*
 * RelationsDiagram class implementation
 */

static void
relations_diagram_class_init (RelationsDiagramClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* Properties */
        object_class->set_property = relations_diagram_set_property;
        object_class->get_property = relations_diagram_get_property;

	object_class->dispose = relations_diagram_dispose;
}

static void
relations_diagram_page_init (BrowserPageIface *iface)
{
	iface->i_get_actions_group = relations_diagram_page_get_actions_group;
	iface->i_get_actions_ui = relations_diagram_page_get_actions_ui;
	iface->i_get_tab_label = relations_diagram_page_get_tab_label;
}

static void
relations_diagram_init (RelationsDiagram *diagram, G_GNUC_UNUSED RelationsDiagramClass *klass)
{
	diagram->priv = g_new0 (RelationsDiagramPrivate, 1);
	diagram->priv->fav_id = -1;
	diagram->priv->popup_container = NULL;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (diagram), GTK_ORIENTATION_VERTICAL);
}

static void
relations_diagram_dispose (GObject *object)
{
	RelationsDiagram *diagram = (RelationsDiagram *) object;

	/* free memory */
	if (diagram->priv) {
		if (diagram->priv->bcnc) {
			g_signal_handlers_disconnect_by_func (diagram->priv->bcnc,
							      G_CALLBACK (meta_changed_cb), diagram);
			g_signal_handlers_disconnect_by_func (diagram->priv->bcnc,
							      G_CALLBACK (favorites_changed_cb), diagram);
			g_object_unref (diagram->priv->bcnc);
		}

		if (diagram->priv->popup_container)
			gtk_widget_destroy (diagram->priv->popup_container);

		g_free (diagram->priv);
		diagram->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
relations_diagram_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (RelationsDiagramClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) relations_diagram_class_init,
			NULL,
			NULL,
			sizeof (RelationsDiagram),
			0,
			(GInstanceInitFunc) relations_diagram_init,
			0
		};
		static GInterfaceInfo page_info = {
                        (GInterfaceInitFunc) relations_diagram_page_init,
			NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_BOX, "RelationsDiagram", &info, 0);
		g_type_add_interface_static (type, BROWSER_PAGE_TYPE, &page_info);
	}
	return type;
}

static void
relations_diagram_set_property (GObject *object,
				guint param_id,
				G_GNUC_UNUSED const GValue *value,
				GParamSpec *pspec)
{
	/*RelationsDiagram *diagram;
	  diagram = RELATIONS_DIAGRAM (object);*/
	switch (param_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
relations_diagram_get_property (GObject *object,
				guint param_id,
				G_GNUC_UNUSED GValue *value,
				GParamSpec *pspec)
{
	/*RelationsDiagram *diagram;
	  diagram = RELATIONS_DIAGRAM (object);*/
	switch (param_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
meta_changed_cb (G_GNUC_UNUSED BrowserConnection *bcnc, GdaMetaStruct *mstruct, RelationsDiagram *diagram)
{
	g_object_set (G_OBJECT (diagram->priv->canvas), "meta-struct", mstruct, NULL);
}

static void
favorites_changed_cb (G_GNUC_UNUSED BrowserConnection *bcnc, RelationsDiagram *diagram)
{
	if (diagram->priv->fav_id >= 0)
		relations_diagram_set_fav_id (diagram, diagram->priv->fav_id, NULL);
}

/*
 * POPUP
 */
static void
real_save_clicked_cb (GtkWidget *button, RelationsDiagram *diagram)
{
	gchar *str;

	str = browser_canvas_serialize_items (BROWSER_CANVAS (diagram->priv->canvas));

	GError *lerror = NULL;
	ToolsFavorites *bfav;
	ToolsFavoritesAttributes fav;

	memset (&fav, 0, sizeof (ToolsFavoritesAttributes));
	fav.id = diagram->priv->fav_id;
	fav.type = TOOLS_FAVORITES_DIAGRAMS;
	fav.name = gtk_editable_get_chars (GTK_EDITABLE (diagram->priv->name_entry), 0, -1);
	if (!*fav.name) {
		g_free (fav.name);
		fav.name = g_strdup (_("Diagram"));
	}
	fav.contents = str;
	
	gtk_widget_hide (diagram->priv->popup_container);
	
	bfav = browser_connection_get_favorites (diagram->priv->bcnc);
	if (! tools_favorites_add (bfav, 0, &fav, ORDER_KEY_SCHEMA, G_MAXINT, &lerror)) {
		browser_show_error ((GtkWindow*) gtk_widget_get_toplevel (button),
				    "<b>%s:</b>\n%s",
				    _("Could not save diagram"),
				    lerror && lerror->message ? lerror->message : _("No detail"));
		if (lerror)
			g_error_free (lerror);
	}

	relations_diagram_set_fav_id (diagram, fav.id, NULL);

	g_free (fav.name);
	g_free (str);
}

static void
save_clicked_cb (GtkWidget *button, RelationsDiagram *diagram)
{
	gchar *str;

	if (!diagram->priv->popup_container) {
		GtkWidget *window, *wid, *hbox;

		window = popup_container_new (button);
		diagram->priv->popup_container = window;

		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_container_add (GTK_CONTAINER (window), hbox);
		wid = gtk_label_new ("");
		str = g_strdup_printf ("%s:", _("Canvas's name"));
		gtk_label_set_markup (GTK_LABEL (wid), str);
		g_free (str);
		gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);

		wid = gtk_entry_new ();
		gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 5);
		diagram->priv->name_entry = wid;
		if (diagram->priv->fav_id > 0) {
			ToolsFavoritesAttributes fav;
			if (tools_favorites_get (browser_connection_get_favorites (diagram->priv->bcnc),
						   diagram->priv->fav_id, &fav, NULL)) {
				gtk_entry_set_text (GTK_ENTRY (wid), fav.name);
				tools_favorites_reset_attributes (&fav);
			}
		}

		g_signal_connect (wid, "activate",
				  G_CALLBACK (real_save_clicked_cb), diagram);

		wid = gtk_button_new_with_label (_("Save"));
		gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
		g_signal_connect (wid, "clicked",
				  G_CALLBACK (real_save_clicked_cb), diagram);
		diagram->priv->real_save_button = wid;

		gtk_widget_show_all (hbox);
	}

        gtk_widget_show (diagram->priv->popup_container);
}


/**
 * relations_diagram_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
relations_diagram_new (BrowserConnection *bcnc)
{
	RelationsDiagram *diagram;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);

	diagram = RELATIONS_DIAGRAM (g_object_new (RELATIONS_DIAGRAM_TYPE, NULL));

	diagram->priv->bcnc = g_object_ref (bcnc);
	g_signal_connect (diagram->priv->bcnc, "meta-changed",
			  G_CALLBACK (meta_changed_cb), diagram);
	g_signal_connect (bcnc, "favorites-changed",
			  G_CALLBACK (favorites_changed_cb), diagram);


	/* header */
	GtkWidget *hbox, *wid;
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (diagram), hbox, FALSE, FALSE, 0);

        GtkWidget *label;
	gchar *str;
	str = g_strdup_printf ("<b>%s</b>\n%s", _("Relations diagram"), _("Unsaved"));
	label = gdaui_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	diagram->priv->header = GDAUI_BAR (label);

	wid = gdaui_bar_add_button_from_stock (GDAUI_BAR (label), GTK_STOCK_SAVE);
	diagram->priv->save_button = wid;

	g_signal_connect (wid, "clicked",
			  G_CALLBACK (save_clicked_cb), diagram);

        gtk_widget_show_all (hbox);

	/* main contents */
	wid = browser_canvas_db_relations_new (NULL);
	diagram->priv->canvas = wid;
	gtk_box_pack_start (GTK_BOX (diagram), wid, TRUE, TRUE, 0);	
        gtk_widget_show_all (wid);
	
	GdaMetaStruct *mstruct;
	mstruct = browser_connection_get_meta_struct (diagram->priv->bcnc);
	if (mstruct)
		meta_changed_cb (diagram->priv->bcnc, mstruct, diagram);
	
	return (GtkWidget*) diagram;
}

GtkWidget *
relations_diagram_new_with_fav_id (BrowserConnection *bcnc, gint fav_id, GError **error)
{
	RelationsDiagram *diagram = NULL;
	ToolsFavoritesAttributes fav;
	xmlDocPtr doc = NULL;

	if (! tools_favorites_get (browser_connection_get_favorites (bcnc),
				     fav_id, &fav, error))
		return FALSE;


	doc = xmlParseDoc (BAD_CAST fav.contents);
	if (!doc) {
		g_set_error (error, TOOLS_ERROR, TOOLS_INTERNAL_COMMAND_ERROR,
			     "%s", _("Error parsing favorite's contents"));
		goto out;
	}

	/* create diagram */
	diagram = RELATIONS_DIAGRAM (relations_diagram_new (bcnc));
	if (!diagram)
		goto out;
	gchar *str, *tmp;
	tmp = g_markup_printf_escaped (_("'%s' diagram"), fav.name);
	str = g_strdup_printf ("<b>%s</b>\n%s", _("Relations diagram"), tmp);
	g_free (tmp);
	gdaui_bar_set_text (diagram->priv->header, str);
	g_free (str);
	diagram->priv->fav_id = fav_id;
	relations_diagram_set_fav_id (diagram, fav_id, NULL);

	/* fill the diagram */
	xmlNodePtr root, node;
	root = xmlDocGetRootElement (doc);
	if (!root)
		goto out;
	for (node = root->children; node; node = node->next) {
		if (!strcmp ((gchar*) node->name, "table")) {
			xmlChar *schema;
			xmlChar *name;
			schema = xmlGetProp (node, BAD_CAST "schema");
			name = xmlGetProp (node, BAD_CAST "name");
			if (schema && name) {
				BrowserCanvasTable *table;
				GValue *v1, *v2;
				g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), (gchar*) schema);
				g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), (gchar*) name);
				xmlFree (schema);
				xmlFree (name);
				table = browser_canvas_db_relations_add_table (BROWSER_CANVAS_DB_RELATIONS (diagram->priv->canvas),
									       NULL, v1, v2);
				gda_value_free (v1);
				gda_value_free (v2);
				if (table) {
					xmlChar *x, *y;
					x = xmlGetProp (node, BAD_CAST "x");
					y = xmlGetProp (node, BAD_CAST "y");
					browser_canvas_translate_item (BROWSER_CANVAS (diagram->priv->canvas),
								       (BrowserCanvasItem*) table,
								       x ? atof ((gchar*) x) : 0.,
								       y ? atof ((gchar*) y) : 0.);
					if (x)
						xmlFree (x);
					if (y)
						xmlFree (y);
				}
			}
			else {
				if (schema)
					xmlFree (schema);
				if (name)
					xmlFree (name);
				g_set_error (error, TOOLS_ERROR, TOOLS_STORED_DATA_ERROR,
					     "%s", _("Missing table attribute in favorite's contents"));
				gtk_widget_destroy ((GtkWidget*) diagram);
				diagram = NULL;
				goto out;
			}
		}
	}

 out:
	tools_favorites_reset_attributes (&fav);
	if (doc)
		xmlFreeDoc (doc);
	return (GtkWidget*) diagram;
}

/*
 * relations_diagram_set_fav_id
 *
 * Sets the favorite ID of @diagram: ensure every displayed information is up to date
 */
static void
relations_diagram_set_fav_id (RelationsDiagram *diagram, gint fav_id, GError **error)
{
	g_return_if_fail (IS_RELATIONS_DIAGRAM (diagram));
	ToolsFavoritesAttributes fav;

	if ((fav_id >=0) &&
	    tools_favorites_get (browser_connection_get_favorites (diagram->priv->bcnc),
				   fav_id, &fav, error)) {
		gchar *str, *tmp;
		tmp = g_markup_printf_escaped (_("'%s' diagram"), fav.name);
		str = g_strdup_printf ("<b>%s</b>\n%s", _("Relations diagram"), tmp);
		g_free (tmp);
		gdaui_bar_set_text (diagram->priv->header, str);
		g_free (str);
		
		diagram->priv->fav_id = fav.id;
		
		tools_favorites_reset_attributes (&fav);
	}
	else {
		gchar *str;
		str = g_strdup_printf ("<b>%s</b>\n%s", _("Relations diagram"), _("Unsaved"));
		gdaui_bar_set_text (diagram->priv->header, str);
		g_free (str);
		diagram->priv->fav_id = -1;
	}

	/* update notebook's tab label */
	BrowserPerspective *pers;
	pers = browser_page_get_perspective (BROWSER_PAGE (diagram));
	if (pers)
		browser_perspective_page_tab_label_change (pers, BROWSER_PAGE (diagram));
}

/**
 * relations_diagram_get_fav_id
 *
 */
gint
relations_diagram_get_fav_id (RelationsDiagram *diagram)
{
	g_return_val_if_fail (IS_RELATIONS_DIAGRAM (diagram), -1);
	return diagram->priv->fav_id;
}

static void
action_view_contents_cb  (G_GNUC_UNUSED GtkAction *action, RelationsDiagram *diagram)
{
	gchar *str;
	str = browser_canvas_db_relations_items_to_data_manager (BROWSER_CANVAS_DB_RELATIONS (diagram->priv->canvas));
	g_print ("%s\n", str);

	if (str) {
		BrowserWindow *bwin;
		BrowserPerspective *pers;
		bwin = (BrowserWindow*) gtk_widget_get_toplevel ((GtkWidget*) diagram);
		pers = browser_window_change_perspective (bwin, _("Data manager"));

		data_manager_perspective_new_tab (DATA_MANAGER_PERSPECTIVE (pers), str);
		g_free (str);
	}
}


static GtkActionEntry ui_actions[] = {
	{ "ViewContents", GTK_STOCK_EDIT, N_("_Contents"), NULL, N_("View contents"),
	  G_CALLBACK (action_view_contents_cb)},
};
static const gchar *ui_actions_info =
	"<ui>"
	"  <menubar name='MenuBar'>"
	"  </menubar>"
	"  <toolbar name='ToolBar'>"
	"    <separator/>"
	"    <toolitem action='ViewContents'/>"
	"  </toolbar>"
	"</ui>";

static GtkActionGroup *
relations_diagram_page_get_actions_group (BrowserPage *page)
{
	GtkActionGroup *agroup;
	agroup = gtk_action_group_new ("SchemaBrowserRelationsDiagramActions");
	gtk_action_group_set_translation_domain (agroup, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (agroup, ui_actions, G_N_ELEMENTS (ui_actions), page);
	
	return agroup;
}

static const gchar *
relations_diagram_page_get_actions_ui (G_GNUC_UNUSED BrowserPage *page)
{
	return ui_actions_info;
}


static GtkWidget *
relations_diagram_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button)
{
	GtkWidget *wid;
	RelationsDiagram *diagram;
	gchar *tab_name = NULL;
	GdkPixbuf *table_pixbuf;

	diagram = RELATIONS_DIAGRAM (page);
	if (diagram->priv->fav_id > 0) {
		ToolsFavoritesAttributes fav;
		if (tools_favorites_get (browser_connection_get_favorites (diagram->priv->bcnc),
					   diagram->priv->fav_id, &fav, NULL)) {
			tab_name = g_strdup (fav.name);
			tools_favorites_reset_attributes (&fav);
		}
	}
	if (!tab_name)
		tab_name = g_strdup (_("Diagram"));

	table_pixbuf = browser_get_pixbuf_icon (BROWSER_ICON_DIAGRAM);
	wid = browser_make_tab_label_with_pixbuf (tab_name,
						  table_pixbuf,
						  out_close_button ? TRUE : FALSE, out_close_button);
	g_free (tab_name);
	return wid;
}
