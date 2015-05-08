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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "schema-browser-perspective.h"
#include "favorite-selector.h"
#include "objects-index.h"
#include "../browser-window.h"
#include "table-info.h"
#include "../ui-support.h"
#include "../browser-page.h"
#ifdef HAVE_GOOCANVAS
#include "relations-diagram.h"
#endif
#include <libgda/gda-debug-macros.h>

/* 
 * Main static functions 
 */
static void schema_browser_perspective_class_init (SchemaBrowserPerspectiveClass *klass);
static void schema_browser_perspective_init (SchemaBrowserPerspective *stmt);
static void schema_browser_perspective_dispose (GObject *object);

/* BrowserPerspective interface */
static void                 schema_browser_perspective_perspective_init (BrowserPerspectiveIface *iface);
static void                 schema_browser_perspective_customize (BrowserPerspective *perspective,
								  GtkToolbar *toolbar, GtkHeaderBar *header, GMenu *menu);
static void                 schema_browser_perspective_uncustomize (BrowserPerspective *perspective,
								    GtkToolbar *toolbar, GtkHeaderBar *header, GMenu *menu);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _SchemaBrowserPerspectivePrivate {
	GtkWidget *notebook;
	GtkWidget *favorites;
	gboolean favorites_shown;
	BrowserWindow *bwin;

	GArray *custom_parts;
};

GType
schema_browser_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (SchemaBrowserPerspectiveClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) schema_browser_perspective_class_init,
			NULL,
			NULL,
			sizeof (SchemaBrowserPerspective),
			0,
			(GInstanceInitFunc) schema_browser_perspective_init,
			0
		};

		static GInterfaceInfo perspective_info = {
                        (GInterfaceInitFunc) schema_browser_perspective_perspective_init,
			NULL,
                        NULL
                };
		
		g_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_BOX, "SchemaBrowserPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
schema_browser_perspective_class_init (SchemaBrowserPerspectiveClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = schema_browser_perspective_dispose;
}

static void
schema_browser_perspective_perspective_init (BrowserPerspectiveIface *iface)
{
	iface->i_customize = schema_browser_perspective_customize;
        iface->i_uncustomize = schema_browser_perspective_uncustomize;
}


static void
schema_browser_perspective_init (SchemaBrowserPerspective *perspective)
{
	perspective->priv = g_new0 (SchemaBrowserPerspectivePrivate, 1);
	perspective->priv->favorites_shown = TRUE;
	perspective->priv->custom_parts = NULL;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (perspective), GTK_ORIENTATION_VERTICAL);
}

static void fav_selection_changed_cb (GtkWidget *widget, gint fav_id, TFavoritesType fav_type,
				      const gchar *selection, SchemaBrowserPerspective *bpers);
static void objects_index_selection_changed_cb (GtkWidget *widget, TFavoritesType fav_type,
						const gchar *selection, SchemaBrowserPerspective *bpers);
/**
 * schema_browser_perspective_new
 *
 * Creates new #BrowserPerspective widget which 
 */
BrowserPerspective *
schema_browser_perspective_new (BrowserWindow *bwin)
{
	TConnection *tcnc;
	BrowserPerspective *bpers;
	SchemaBrowserPerspective *perspective;
	gboolean fav_supported;

	bpers = (BrowserPerspective*) g_object_new (TYPE_SCHEMA_BROWSER_PERSPECTIVE, NULL);
	perspective = (SchemaBrowserPerspective*) bpers;
	tcnc = browser_window_get_connection (bwin);
	fav_supported = t_connection_get_favorites (tcnc) ? TRUE : FALSE;
	perspective->priv->bwin = bwin;

	/* contents */
	GtkWidget *paned, *wid, *nb;
	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	if (fav_supported) {
		wid = favorite_selector_new (tcnc);
		g_signal_connect (wid, "selection-changed",
				  G_CALLBACK (fav_selection_changed_cb), bpers);
		gtk_paned_add1 (GTK_PANED (paned), wid);
		gtk_paned_set_position (GTK_PANED (paned), DEFAULT_FAVORITES_SIZE);
		perspective->priv->favorites = wid;
	}

	nb = gtk_notebook_new ();
	perspective->priv->notebook = nb;
	gtk_paned_add2 (GTK_PANED (paned), nb);
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (nb), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (nb));

	wid = objects_index_new (tcnc);
	g_signal_connect (wid, "selection-changed",
			  G_CALLBACK (objects_index_selection_changed_cb), bpers);
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid,
				  ui_make_tab_label_with_icon (_("Index"), "help-about", FALSE, NULL));
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (nb), wid, TRUE);
	gtk_notebook_set_group_name (GTK_NOTEBOOK (nb), "schema-browser");

	gtk_notebook_set_menu_label (GTK_NOTEBOOK (nb), wid,
				     ui_make_tab_label_with_icon (_("Index"), "help-about", FALSE, NULL));
	gtk_box_pack_start (GTK_BOX (bpers), paned, TRUE, TRUE, 0);
	gtk_widget_show_all (paned);

	if (perspective->priv->favorites && !perspective->priv->favorites_shown)
		gtk_widget_hide (perspective->priv->favorites);

	browser_perspective_declare_notebook (bpers, GTK_NOTEBOOK (perspective->priv->notebook));

	return bpers;
}

static void
close_button_clicked_cb (G_GNUC_UNUSED GtkWidget *wid, GtkWidget *page_widget)
{
	gtk_widget_destroy (page_widget);
}

static void
objects_index_selection_changed_cb (GtkWidget *widget, TFavoritesType fav_type,
				    const gchar *selection, SchemaBrowserPerspective *bpers)
{
	fav_selection_changed_cb (widget, -1, fav_type, selection, bpers);
}


static void
fav_selection_changed_cb (G_GNUC_UNUSED GtkWidget *widget, gint fav_id, TFavoritesType fav_type,
			  const gchar *selection, SchemaBrowserPerspective *bpers)
{
	if (fav_type == T_FAVORITES_TABLES) {
		GdaQuarkList *ql;
		const gchar *type;
		const gchar *schema = NULL, *table = NULL, *short_name = NULL;

		ql = gda_quark_list_new_from_string (selection);
		if (ql) {
			type = gda_quark_list_find (ql, "OBJ_TYPE");
			schema = gda_quark_list_find (ql, "OBJ_SCHEMA");
			table = gda_quark_list_find (ql, "OBJ_NAME");
			short_name = gda_quark_list_find (ql, "OBJ_SHORT_NAME");
		}
		
		if (!type || !schema || !table) {
			if (ql)
				gda_quark_list_free (ql);
			return;
		}

		if (!strcmp (type, "table")) {
			schema_browser_perspective_display_table_info (bpers, schema, table, short_name);
		}
		else {
			gint ntabs, i;
			ntabs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (bpers->priv->notebook));
			for (i = 0; i < ntabs; i++) {
				GtkWidget *child;
				child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
				if (IS_TABLE_INFO (child)) {
					if (!strcmp (schema, table_info_get_table_schema (TABLE_INFO (child))) &&
					    !strcmp (table, table_info_get_table_name (TABLE_INFO (child)))) {
						gtk_notebook_set_current_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
						return;
					}
				}
			}

			g_warning ("Non handled favorite type: %s", type);
			TO_IMPLEMENT;
		}
	
		if (ql)
			gda_quark_list_free (ql);
	}
	else if (fav_type == T_FAVORITES_DIAGRAMS) {
#ifdef HAVE_GOOCANVAS
		schema_browser_perspective_display_diagram (bpers, fav_id);
#else
		g_warning ("Can't display diagram because canvas not compiled.");
#endif
	}
#ifdef GDA_DEBUG_NO
	g_print ("Reacted to selection fav_id=>%d type=>%u, contents=>%s\n", fav_id, fav_type, selection);	
#endif
}

static void
schema_browser_perspective_dispose (GObject *object)
{
	SchemaBrowserPerspective *perspective;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_SCHEMA_BROWSER_PERSPECTIVE (object));

	perspective = SCHEMA_BROWSER_PERSPECTIVE (object);
	if (perspective->priv) {
		browser_perspective_declare_notebook ((BrowserPerspective*) perspective, NULL);
		g_free (perspective->priv);
		perspective->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

#ifdef HAVE_GOOCANVAS
static void
action_diagram_new_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	/* @data is a SchemaBrowserPerspective */
	SchemaBrowserPerspective *perspective;
	perspective = SCHEMA_BROWSER_PERSPECTIVE (data);
	schema_browser_perspective_display_diagram (perspective, -1);
}
#endif

static void
favorites_toggle_cb (GSimpleAction *action, GVariant *state, gpointer data)
{
	/* @data is a SchemaBrowserPerspective */
	SchemaBrowserPerspective *perspective;
	perspective = SCHEMA_BROWSER_PERSPECTIVE (data);
	if (! perspective->priv->favorites)
		return;

	perspective->priv->favorites_shown = g_variant_get_boolean (state);
	if (perspective->priv->favorites_shown)
		gtk_widget_show (perspective->priv->favorites);
	else
		gtk_widget_hide (perspective->priv->favorites);
	g_simple_action_set_state (action, state);
}

static GActionEntry win_entries[] = {
        { "show-favorites", NULL, NULL, "true", favorites_toggle_cb },
#ifdef HAVE_GOOCANVAS
        { "action-diagram-new", action_diagram_new_cb, NULL, NULL, NULL },
#endif
};

static void
schema_browser_perspective_customize (BrowserPerspective *perspective,
				    GtkToolbar *toolbar, GtkHeaderBar *header, GMenu *menu)
{
	g_print ("%s ()\n", __FUNCTION__);
	SchemaBrowserPerspective *persp;
	persp = SCHEMA_BROWSER_PERSPECTIVE (perspective);

	BrowserWindow *bwin;
	bwin = browser_perspective_get_window (perspective);

	/* add perspective's actions */
	g_action_map_add_action_entries (G_ACTION_MAP (bwin),
					 win_entries, G_N_ELEMENTS (win_entries),
					 perspective);

	g_assert (! persp->priv->custom_parts);
	persp->priv->custom_parts = g_array_new (FALSE, FALSE, sizeof (gpointer));

	/* add to toolbar */
	GtkToolItem *titem;
	titem = gtk_toggle_tool_button_new ();
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "user-bookmarks-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Show favorites"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.show-favorites");
	gtk_widget_show (GTK_WIDGET (titem));
	g_array_append_val (persp->priv->custom_parts, titem);

#ifdef HAVE_GOOCANVAS
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "tab-new-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Create a new diagram"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.action-diagram-new");
	gtk_widget_show (GTK_WIDGET (titem));
	g_array_append_val (persp->priv->custom_parts, titem);
#endif
}

static void
schema_browser_perspective_uncustomize (BrowserPerspective *perspective,
				      GtkToolbar *toolbar, GtkHeaderBar *header, GMenu *menu)
{
	g_print ("%s ()\n", __FUNCTION__);
	SchemaBrowserPerspective *persp;
	persp = SCHEMA_BROWSER_PERSPECTIVE (perspective);

	BrowserWindow *bwin;
	bwin = browser_perspective_get_window (perspective);

	/* remove perspective's actions */
	guint i;
	for (i = 0; i < G_N_ELEMENTS (win_entries); i++) {
		GActionEntry *entry;
		entry = &win_entries [i];
		g_action_map_remove_action (G_ACTION_MAP (bwin), entry->name);
	}

	/* cleanups, headerbar and toolbar */
	g_assert (persp->priv->custom_parts);
	for (i = 0; i < persp->priv->custom_parts->len; i++) {
		GObject *obj;
		obj = g_array_index (persp->priv->custom_parts, GObject*, i);
		if (GTK_IS_WIDGET (obj))
			gtk_widget_destroy (GTK_WIDGET (obj));
		else
			g_warning ("Unknown type to uncustomize: %s\n", G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (obj)));
	}
	g_array_free (persp->priv->custom_parts, TRUE);
	persp->priv->custom_parts = NULL;
}


#ifdef HAVE_GOOCANVAS
/**
 * schema_browser_perspective_display_diagram
 *
 */
void
schema_browser_perspective_display_diagram (SchemaBrowserPerspective *bpers, gint fav_id)
{
	GtkWidget *diagram = NULL;

	if (fav_id >= 0) {
		gint ntabs, i;
		
		ntabs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (bpers->priv->notebook));
		for (i = 0; i < ntabs; i++) {
			GtkWidget *child;
			child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
			if (IS_RELATIONS_DIAGRAM (child)) {
				if (relations_diagram_get_fav_id (RELATIONS_DIAGRAM (child)) == fav_id) {
					gtk_notebook_set_current_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
					return;
				}
			}
		}

		GError *error = NULL;
		diagram = relations_diagram_new_with_fav_id (browser_window_get_connection (bpers->priv->bwin),
							     fav_id, &error);
		if (! diagram) {
			ui_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) bpers),
					    error && error->message ? error->message :
					    _("Could not load diagram"));
			g_clear_error (&error);
		}
	}
	else
		diagram = relations_diagram_new (browser_window_get_connection (bpers->priv->bwin));

	if (diagram) {
		GtkWidget *close_btn;
		GtkWidget *tab_label;
		gint i;
		
		tab_label = browser_page_get_tab_label (BROWSER_PAGE (diagram), &close_btn);
		i = gtk_notebook_append_page (GTK_NOTEBOOK (bpers->priv->notebook), diagram, tab_label);
		g_signal_connect (close_btn, "clicked",
				  G_CALLBACK (close_button_clicked_cb), diagram);
		
		gtk_widget_show (diagram);

		tab_label = browser_page_get_tab_label (BROWSER_PAGE (diagram), NULL);
		gtk_notebook_set_menu_label (GTK_NOTEBOOK (bpers->priv->notebook), diagram, tab_label);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
		gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (bpers->priv->notebook), diagram,
						  TRUE);
		gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (bpers->priv->notebook), diagram,
						 TRUE);
	}
}
#endif


/**
 * schema_browser_perspective_display_table_info
 *
 * Display (and create if necessary) a new page for the table's properties
 */
void
schema_browser_perspective_display_table_info (SchemaBrowserPerspective *bpers,
					       const gchar *table_schema,
					       const gchar *table_name,
					       G_GNUC_UNUSED const gchar *table_short_name)
{
	g_return_if_fail (IS_SCHEMA_BROWSER_PERSPECTIVE (bpers));

	gint ntabs, i;
	ntabs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (bpers->priv->notebook));
	for (i = 0; i < ntabs; i++) {
		GtkWidget *child;
		child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
		if (IS_TABLE_INFO (child)) {
			if (!strcmp (table_schema, table_info_get_table_schema (TABLE_INFO (child))) &&
			    !strcmp (table_name, table_info_get_table_name (TABLE_INFO (child)))) {
				gtk_notebook_set_current_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
				return;
			}
		}
	}
	
	GtkWidget *ti;
	ti = table_info_new (browser_window_get_connection (bpers->priv->bwin), table_schema, table_name);
	if (ti) {
		GtkWidget *close_btn;
		GtkWidget *tab_label;
		gint i;
		
		tab_label = browser_page_get_tab_label (BROWSER_PAGE (ti), &close_btn);
		i = gtk_notebook_append_page (GTK_NOTEBOOK (bpers->priv->notebook), ti, tab_label);
		g_signal_connect (close_btn, "clicked",
				  G_CALLBACK (close_button_clicked_cb), ti);
		
		gtk_widget_show (ti);

		tab_label = browser_page_get_tab_label (BROWSER_PAGE (ti), NULL);
		gtk_notebook_set_menu_label (GTK_NOTEBOOK (bpers->priv->notebook), ti, tab_label);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
		gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (bpers->priv->notebook), ti,
						  TRUE);
		gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (bpers->priv->notebook), ti,
						 TRUE);
	}
}
