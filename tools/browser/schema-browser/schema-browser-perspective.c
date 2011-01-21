/*
 * Copyright (C) 2009 - 2010 Vivien Malerba
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "schema-browser-perspective.h"
#include "favorite-selector.h"
#include "objects-index.h"
#include "../browser-window.h"
#include "table-info.h"
#include "../support.h"
#include "../browser-page.h"
#ifdef HAVE_GOOCANVAS
#include "relations-diagram.h"
#endif

/* 
 * Main static functions 
 */
static void schema_browser_perspective_class_init (SchemaBrowserPerspectiveClass *klass);
static void schema_browser_perspective_init (SchemaBrowserPerspective *stmt);
static void schema_browser_perspective_dispose (GObject *object);

/* BrowserPerspective interface */
static void                 schema_browser_perspective_perspective_init (BrowserPerspectiveIface *iface);
static GtkActionGroup      *schema_browser_perspective_get_actions_group (BrowserPerspective *perspective);
static const gchar         *schema_browser_perspective_get_actions_ui (BrowserPerspective *perspective);
static void                 schema_browser_perspective_page_tab_label_change (BrowserPerspective *perspective, BrowserPage *page);
static void                 schema_browser_perspective_get_current_customization (BrowserPerspective *perspective,
										  GtkActionGroup **out_agroup,
										  const gchar **out_ui);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _SchemaBrowserPerspectivePrivate {
	GtkWidget *notebook;
	GtkWidget *favorites;
	gboolean favorites_shown;
	BrowserWindow *bwin;
};

GType
schema_browser_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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
		
		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_VBOX, "SchemaBrowserPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_static_mutex_unlock (&registering);
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
	iface->i_get_actions_group = schema_browser_perspective_get_actions_group;
	iface->i_get_actions_ui = schema_browser_perspective_get_actions_ui;
	iface->i_page_tab_label_change = schema_browser_perspective_page_tab_label_change;
	iface->i_get_current_customization = schema_browser_perspective_get_current_customization;
}


static void
schema_browser_perspective_init (SchemaBrowserPerspective *perspective)
{
	perspective->priv = g_new0 (SchemaBrowserPerspectivePrivate, 1);
	perspective->priv->favorites_shown = TRUE;
}

static void fav_selection_changed_cb (GtkWidget *widget, gint fav_id, BrowserFavoritesType fav_type,
				      const gchar *selection, SchemaBrowserPerspective *bpers);
static void objects_index_selection_changed_cb (GtkWidget *widget, BrowserFavoritesType fav_type,
						const gchar *selection, SchemaBrowserPerspective *bpers);
static void nb_switch_page_cb (GtkNotebook *nb, GtkWidget *page, gint page_num,
			       SchemaBrowserPerspective *perspective); 
/**
 * schema_browser_perspective_new
 *
 * Creates new #BrowserPerspective widget which 
 */
BrowserPerspective *
schema_browser_perspective_new (BrowserWindow *bwin)
{
	BrowserConnection *bcnc;
	BrowserPerspective *bpers;
	SchemaBrowserPerspective *perspective;

	bpers = (BrowserPerspective*) g_object_new (TYPE_SCHEMA_BROWSER_PERSPECTIVE, NULL);
	perspective = (SchemaBrowserPerspective*) bpers;

	perspective->priv->bwin = bwin;

	/* contents */
	GtkWidget *paned, *wid, *nb;
	bcnc = browser_window_get_connection (bwin);
	paned = gtk_hpaned_new ();
	wid = favorite_selector_new (bcnc);
	g_signal_connect (wid, "selection-changed",
			  G_CALLBACK (fav_selection_changed_cb), bpers);
	gtk_paned_add1 (GTK_PANED (paned), wid);
	gtk_paned_set_position (GTK_PANED (paned), DEFAULT_FAVORITES_SIZE);
	perspective->priv->favorites = wid;

	nb = gtk_notebook_new ();
	perspective->priv->notebook = nb;
	gtk_paned_add2 (GTK_PANED (paned), nb);
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (nb), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (nb));
	g_signal_connect (G_OBJECT (nb), "switch-page",
			  G_CALLBACK (nb_switch_page_cb), perspective);

	wid = objects_index_new (bcnc);
	g_signal_connect (wid, "selection-changed",
			  G_CALLBACK (objects_index_selection_changed_cb), bpers);
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid,
				  browser_make_tab_label_with_stock (_("Index"), GTK_STOCK_ABOUT, FALSE,
								     NULL));
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (nb), wid, TRUE);
	gtk_notebook_set_group_name (GTK_NOTEBOOK (nb), "schema-browser");

	gtk_notebook_set_menu_label (GTK_NOTEBOOK (nb), wid,
				     browser_make_tab_label_with_stock (_("Index"), GTK_STOCK_ABOUT, FALSE,
									NULL));
	gtk_box_pack_start (GTK_BOX (bpers), paned, TRUE, TRUE, 0);
	gtk_widget_show_all (paned);

	if (!perspective->priv->favorites_shown)
		gtk_widget_hide (perspective->priv->favorites);

	return bpers;
}

static void
nb_switch_page_cb (GtkNotebook *nb, G_GNUC_UNUSED GtkWidget *page, gint page_num,
		   SchemaBrowserPerspective *perspective)
{
	GtkWidget *page_contents;
	GtkActionGroup *actions = NULL;
	const gchar *ui = NULL;

	page_contents = gtk_notebook_get_nth_page (nb, page_num);
	if (IS_BROWSER_PAGE (page_contents)) {
		actions = browser_page_get_actions_group (BROWSER_PAGE (page_contents));
		ui = browser_page_get_actions_ui (BROWSER_PAGE (page_contents));
	}
	browser_window_customize_perspective_ui (perspective->priv->bwin,
						 BROWSER_PERSPECTIVE (perspective), actions, 
						 ui);
	if (actions)
		g_object_unref (actions);
}

static void
close_button_clicked_cb (G_GNUC_UNUSED GtkWidget *wid, GtkWidget *page_widget)
{
	gtk_widget_destroy (page_widget);
}

static void
objects_index_selection_changed_cb (GtkWidget *widget, BrowserFavoritesType fav_type,
				    const gchar *selection, SchemaBrowserPerspective *bpers)
{
	fav_selection_changed_cb (widget, -1, fav_type, selection, bpers);
}


static void
fav_selection_changed_cb (G_GNUC_UNUSED GtkWidget *widget, gint fav_id, BrowserFavoritesType fav_type,
			  const gchar *selection, SchemaBrowserPerspective *bpers)
{
	if (fav_type == BROWSER_FAVORITES_TABLES) {
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
	else if (fav_type == BROWSER_FAVORITES_DIAGRAMS) {
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
		g_free (perspective->priv);
		perspective->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

#ifdef HAVE_GOOCANVAS
static void
action_create_diagram_cb (G_GNUC_UNUSED GtkAction *action, SchemaBrowserPerspective *bpers)
{
	schema_browser_perspective_display_diagram (bpers, -1);
}
#endif

static void
favorites_toggle_cb (GtkToggleAction *action, BrowserPerspective *bpers)
{
	SchemaBrowserPerspective *perspective;
	perspective = SCHEMA_BROWSER_PERSPECTIVE (bpers);
	perspective->priv->favorites_shown = gtk_toggle_action_get_active (action);
	if (perspective->priv->favorites_shown)
		gtk_widget_show (perspective->priv->favorites);
	else
		gtk_widget_hide (perspective->priv->favorites);
}

static const GtkToggleActionEntry ui_toggle_actions [] =
{
        { "SchemaBrowserFavoritesShow", NULL, N_("_Show favorites"), "F9", N_("Show or hide favorites"), G_CALLBACK (favorites_toggle_cb), FALSE }
};

static GtkActionEntry ui_actions[] = {
#ifdef HAVE_GOOCANVAS
        { "Schema", NULL, N_("_Schema"), NULL, N_("Schema"), NULL },
        { "NewDiagram", GTK_STOCK_ADD, N_("_New Diagram"), NULL, N_("Create a new diagram"),
          G_CALLBACK (action_create_diagram_cb)},
#endif
};

static const gchar *ui_actions_info =
        "<ui>"
        "  <menubar name='MenuBar'>"
	"    <menu name='Display' action='Display'>"
	"      <menuitem name='SchemaBrowserFavoritesShow' action='SchemaBrowserFavoritesShow'/>"
        "    </menu>"
        "    <placeholder name='MenuExtension'>"
        "      <menu name='Schema' action='Schema'>"
        "        <menuitem name='NewDiagram' action= 'NewDiagram'/>"
        "      </menu>"
        "    </placeholder>"
        "  </menubar>"
        "</ui>";

static GtkActionGroup *
schema_browser_perspective_get_actions_group (BrowserPerspective *bpers)
{
	GtkActionGroup *agroup;
	agroup = gtk_action_group_new ("SchemaBrowserActions");
	gtk_action_group_set_translation_domain (agroup, GETTEXT_PACKAGE);

	gtk_action_group_add_actions (agroup, ui_actions, G_N_ELEMENTS (ui_actions), bpers);
	gtk_action_group_add_toggle_actions (agroup, ui_toggle_actions, G_N_ELEMENTS (ui_toggle_actions),
					     bpers);
	GtkAction *action;
	action = gtk_action_group_get_action (agroup, "SchemaBrowserFavoritesShow");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      SCHEMA_BROWSER_PERSPECTIVE (bpers)->priv->favorites_shown);	

	return agroup;
}

static const gchar *
schema_browser_perspective_get_actions_ui (G_GNUC_UNUSED BrowserPerspective *bpers)
{
	return ui_actions_info;
}

static void
schema_browser_perspective_page_tab_label_change (BrowserPerspective *perspective, BrowserPage *page)
{
	SchemaBrowserPerspective *bpers;
	GtkWidget *tab_label;
	GtkWidget *close_btn;
	
	bpers = SCHEMA_BROWSER_PERSPECTIVE (perspective);
	tab_label = browser_page_get_tab_label (page, &close_btn);
	if (tab_label) {
		gtk_notebook_set_tab_label (GTK_NOTEBOOK (bpers->priv->notebook),
					    GTK_WIDGET (page), tab_label);
		g_signal_connect (close_btn, "clicked",
				  G_CALLBACK (close_button_clicked_cb), page);
		
		tab_label = browser_page_get_tab_label (page, NULL);
		gtk_notebook_set_menu_label (GTK_NOTEBOOK (bpers->priv->notebook),
					     GTK_WIDGET (page), tab_label);
	}
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
			browser_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) bpers),
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

static void
schema_browser_perspective_get_current_customization (BrowserPerspective *perspective,
						      GtkActionGroup **out_agroup,
						      const gchar **out_ui)
{
	SchemaBrowserPerspective *bpers;
	GtkWidget *page_contents;

	bpers = SCHEMA_BROWSER_PERSPECTIVE (perspective);
	page_contents = gtk_notebook_get_nth_page (GTK_NOTEBOOK (bpers->priv->notebook),
						   gtk_notebook_get_current_page (GTK_NOTEBOOK (bpers->priv->notebook)));
	if (IS_BROWSER_PAGE (page_contents)) {
		*out_agroup = browser_page_get_actions_group (BROWSER_PAGE (page_contents));
		*out_ui = browser_page_get_actions_ui (BROWSER_PAGE (page_contents));
	}
}
