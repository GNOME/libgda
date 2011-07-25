/*
 * Copyright (C) 2009 - 2010 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-data-proxy.h"
#include "gdaui-data-selector.h"
#include "gdaui-raw-grid.h"
#include "gdaui-data-proxy-info.h"
#include "gdaui-enum-types.h"

static void gdaui_data_proxy_info_class_init (GdauiDataProxyInfoClass * class);
static void gdaui_data_proxy_info_init (GdauiDataProxyInfo *wid);
static void gdaui_data_proxy_info_dispose (GObject *object);

static void gdaui_data_proxy_info_set_property (GObject *object,
						guint param_id,
						const GValue *value,
						GParamSpec *pspec);
static void gdaui_data_proxy_info_get_property (GObject *object,
						guint param_id,
						GValue *value,
						GParamSpec *pspec);

static void modif_buttons_make (GdauiDataProxyInfo *info);
static void modif_buttons_update (GdauiDataProxyInfo *info);

static void data_proxy_proxy_changed_cb (GdauiDataProxy *data_proxy, GdaDataProxy *proxy, GdauiDataProxyInfo *info);
static void proxy_changed_cb (GdaDataProxy *proxy, GdauiDataProxyInfo *info);
static void proxy_sample_changed_cb (GdaDataProxy *proxy, gint sample_start, gint sample_end, GdauiDataProxyInfo *info);
static void proxy_row_changed_cb (GdaDataProxy *proxy, gint row, GdauiDataProxyInfo *info);
static void raw_grid_selection_changed_cb (GdauiRawGrid *grid, GdauiDataProxyInfo *info);


struct _GdauiDataProxyInfoPriv
{
	GdauiDataProxy *data_proxy;
	GdaDataProxy      *proxy;
	GdaDataModelIter  *iter;
	GdauiDataProxyInfoFlag flags; /* ORed values. */

	GtkUIManager      *uimanager;
	GtkActionGroup    *agroup; /* no ref held! */
	guint              merge_id_row_modif;
	guint              merge_id_row_move;
	guint              merge_id_chunck_change;

	GtkWidget         *buttons_bar;
	GtkWidget         *tool_item;
	GtkWidget         *current_sample;
	GtkWidget         *row_spin;

	guint              idle_id;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* properties */
enum {
	PROP_0,
	PROP_DATA_PROXY,
	PROP_FLAGS,
	PROP_UI_MANAGER
};

GType
gdaui_data_proxy_info_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiDataProxyInfoClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_data_proxy_info_class_init,
			NULL,
			NULL,
			sizeof (GdauiDataProxyInfo),
			0,
			(GInstanceInitFunc) gdaui_data_proxy_info_init,
			0
		};

		type = g_type_register_static (GTK_TYPE_HBOX, "GdauiDataProxyInfo", &info, 0);
	}

	return type;
}

static void
gdaui_data_proxy_info_class_init (GdauiDataProxyInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);


	object_class->dispose = gdaui_data_proxy_info_dispose;

	/* Properties */
        object_class->set_property = gdaui_data_proxy_info_set_property;
        object_class->get_property = gdaui_data_proxy_info_get_property;
	g_object_class_install_property (object_class, PROP_DATA_PROXY,
                                         g_param_spec_object ("data-proxy", NULL, NULL, GDAUI_TYPE_DATA_PROXY,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_FLAGS,
                                         g_param_spec_flags ("flags", NULL, NULL, GDAUI_TYPE_DATA_PROXY_INFO_FLAG,
							     GDAUI_DATA_PROXY_INFO_CURRENT_ROW,
							     G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * GdauiDataProxyInfo:ui-manager:
	 *
	 * Use this property to obtain the #GtkUIManager object internally used (to add new actions
	 * for example).
	 *
	 * Since: 4.2.9
	 */
	g_object_class_install_property (object_class, PROP_UI_MANAGER,
                                         g_param_spec_object ("ui-manager", NULL, NULL, GTK_TYPE_UI_MANAGER,
							      G_PARAM_READABLE));
}

static void
gdaui_data_proxy_info_init (GdauiDataProxyInfo *wid)
{
	wid->priv = g_new0 (GdauiDataProxyInfoPriv, 1);
	wid->priv->data_proxy = NULL;
	wid->priv->proxy = NULL;
	wid->priv->row_spin = NULL;
}

/**
 * gdaui_data_proxy_info_new:
 * @data_proxy: a widget implementing the #GdauiDataProxy interface
 * @flags: OR'ed values, specifying what to display in the new widget
 *
 * Creates a new #GdauiDataProxyInfo widget suitable to display information about @data_proxy
 *
 * Returns: (transfer full): the new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_data_proxy_info_new (GdauiDataProxy *data_proxy, GdauiDataProxyInfoFlag flags)
{
	GtkWidget *info;

	g_return_val_if_fail (!data_proxy || GDAUI_IS_DATA_PROXY (data_proxy), NULL);

	info = (GtkWidget *) g_object_new (GDAUI_TYPE_DATA_PROXY_INFO,
					   "data-proxy", data_proxy,
					   "flags", flags, NULL);

	return info;
}

static void
data_proxy_destroyed_cb (GdauiDataProxy *wid, GdauiDataProxyInfo *info)
{
	g_assert (wid == info->priv->data_proxy);
	g_signal_handlers_disconnect_by_func (G_OBJECT (wid),
					      G_CALLBACK (data_proxy_destroyed_cb), info);
	g_signal_handlers_disconnect_by_func (G_OBJECT (wid),
					      G_CALLBACK (data_proxy_proxy_changed_cb), info);
	if (GDAUI_IS_RAW_GRID (info->priv->data_proxy))
		g_signal_handlers_disconnect_by_func (info->priv->data_proxy,
						      G_CALLBACK (raw_grid_selection_changed_cb), info);

	info->priv->data_proxy = NULL;
}


static void
release_proxy (GdauiDataProxyInfo *info)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (info->priv->proxy),
					      G_CALLBACK (proxy_changed_cb), info);
	g_signal_handlers_disconnect_by_func (G_OBJECT (info->priv->proxy),
					      G_CALLBACK (proxy_sample_changed_cb), info);
	g_signal_handlers_disconnect_by_func (G_OBJECT (info->priv->proxy),
					      G_CALLBACK (proxy_row_changed_cb), info);
	g_object_unref (info->priv->proxy);
	info->priv->proxy = NULL;
}

static void iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdauiDataProxyInfo *info);
static void
release_iter (GdauiDataProxyInfo *info)
{
	g_signal_handlers_disconnect_by_func (info->priv->iter,
					      G_CALLBACK (iter_row_changed_cb), info);
	g_object_unref (info->priv->iter);
	info->priv->iter = NULL;
}

static void
data_proxy_proxy_changed_cb (GdauiDataProxy *data_proxy, G_GNUC_UNUSED GdaDataProxy *proxy, GdauiDataProxyInfo *info)
{
	g_object_set (G_OBJECT (info), "data-proxy", data_proxy, NULL);
}

static void
gdaui_data_proxy_info_dispose (GObject *object)
{
	GdauiDataProxyInfo *info;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_DATA_PROXY_INFO (object));
	info = GDAUI_DATA_PROXY_INFO (object);

	if (info->priv) {
		if (info->priv->proxy)
			release_proxy (info);
		if (info->priv->iter)
			release_iter (info);
		if (info->priv->data_proxy)
			data_proxy_destroyed_cb (info->priv->data_proxy, info);
		if (info->priv->idle_id)
			g_source_remove (info->priv->idle_id);

		if (info->priv->uimanager) {
			if (info->priv->merge_id_row_modif)
				gtk_ui_manager_remove_ui (info->priv->uimanager,
							  info->priv->merge_id_row_modif);
			if (info->priv->merge_id_row_move)
				gtk_ui_manager_remove_ui (info->priv->uimanager,
							  info->priv->merge_id_row_move);
			if (info->priv->merge_id_chunck_change)
				gtk_ui_manager_remove_ui (info->priv->uimanager,
							  info->priv->merge_id_chunck_change);
			g_object_unref (info->priv->uimanager);
		}

		/* the private area itself */
		g_free (info->priv);
		info->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

static void
gdaui_data_proxy_info_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdauiDataProxyInfo *info;

        info = GDAUI_DATA_PROXY_INFO (object);
        if (info->priv) {
                switch (param_id) {
                case PROP_DATA_PROXY:
			if (info->priv->data_proxy)
				data_proxy_destroyed_cb (info->priv->data_proxy, info);
			if (info->priv->iter)
				release_iter (info);
			if (info->priv->proxy)
				release_proxy (info);

			info->priv->data_proxy = GDAUI_DATA_PROXY (g_value_get_object (value));
			if (info->priv->data_proxy) {
				GdaDataProxy *proxy;
				GdaDataModelIter *iter;

				/* data widget */
				g_signal_connect (info->priv->data_proxy, "destroy",
						  G_CALLBACK (data_proxy_destroyed_cb), info);
				g_signal_connect (info->priv->data_proxy, "proxy-changed",
						  G_CALLBACK (data_proxy_proxy_changed_cb), info);
				if (GDAUI_IS_RAW_GRID (info->priv->data_proxy))
					g_signal_connect (info->priv->data_proxy, "selection-changed",
							  G_CALLBACK (raw_grid_selection_changed_cb), info);

				/* proxy */
				proxy = gdaui_data_proxy_get_proxy (info->priv->data_proxy);
				if (proxy) {
					info->priv->proxy = proxy;
					g_object_ref (info->priv->proxy);
					g_signal_connect (G_OBJECT (proxy), "changed",
							  G_CALLBACK (proxy_changed_cb), info);
					g_signal_connect (G_OBJECT (proxy), "sample-changed",
							  G_CALLBACK (proxy_sample_changed_cb), info);
					g_signal_connect (G_OBJECT (proxy), "row-inserted",
							  G_CALLBACK (proxy_row_changed_cb), info);
					g_signal_connect (G_OBJECT (proxy), "row-removed",
							  G_CALLBACK (proxy_row_changed_cb), info);

					/* iter */
					iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR
											  (info->priv->data_proxy));
					info->priv->iter = iter;
					if (iter) {
						g_object_ref (G_OBJECT (iter));
						g_signal_connect (iter, "row-changed",
								  G_CALLBACK (iter_row_changed_cb), info);
					}
				}
				modif_buttons_update (info);
			}
                        break;
                case PROP_FLAGS:
			info->priv->flags = g_value_get_flags (value);
			modif_buttons_make (info);
			modif_buttons_update (info);
                        break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

static void
gdaui_data_proxy_info_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	GdauiDataProxyInfo *info;

        info = GDAUI_DATA_PROXY_INFO (object);
        if (info->priv) {
                switch (param_id) {
		case PROP_DATA_PROXY:
			g_value_set_object (value, info->priv->data_proxy);
			break;
		case PROP_FLAGS:
			g_value_set_flags (value, info->priv->flags);
			break;
		case PROP_UI_MANAGER:
			g_value_set_object (value, info->priv->uimanager);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}


static void
proxy_changed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, G_GNUC_UNUSED GdauiDataProxyInfo *info)
{
	modif_buttons_update (info);
}

static void
proxy_sample_changed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, G_GNUC_UNUSED gint sample_start,
			 G_GNUC_UNUSED gint sample_end, G_GNUC_UNUSED GdauiDataProxyInfo *info)
{
	modif_buttons_update (info);
}

static void
proxy_row_changed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, G_GNUC_UNUSED gint row, GdauiDataProxyInfo *info)
{
	modif_buttons_update (info);
}

static void
iter_row_changed_cb (G_GNUC_UNUSED GdaDataModelIter *iter, G_GNUC_UNUSED gint row, GdauiDataProxyInfo *info)
{
	modif_buttons_update (info);
}

static void
raw_grid_selection_changed_cb (G_GNUC_UNUSED GdauiRawGrid *grid, GdauiDataProxyInfo *info)
{
	modif_buttons_update (info);
}

/*
 *
 * Modification buttons (Commit changes, Reset info, New entry, Delete)
 *
 */
static const gchar *ui_row_modif =
	"<ui>"
	"  <toolbar name='ToolBar'>"
	"    <toolitem action='ActionNew'/>"
	"    <toolitem action='ActionDelete'/>"
	"    <toolitem action='ActionUndelete'/>"
	"    <toolitem action='ActionCommit'/>"
	"    <toolitem action='ActionReset'/>"
	"  </toolbar>"
	"</ui>";
static const gchar *ui_row_move =
	"<ui>"
	"  <toolbar name='ToolBar'>"
	"    <toolitem action='ActionFirstRecord'/>"
	"    <toolitem action='ActionPrevRecord'/>"
	"    <toolitem action='ActionNextRecord'/>"
	"    <toolitem action='ActionLastRecord'/>"
	"    <toolitem action='ActionFilter'/>"
	"  </toolbar>"
	"</ui>";
static const gchar *ui_chunck_change =
	"<ui>"
	"  <toolbar name='ToolBar'>"
	"    <toolitem action='ActionFirstChunck'/>"
	"    <toolitem action='ActionPrevChunck'/>"
	"    <toolitem action='ActionNextChunck'/>"
	"    <toolitem action='ActionLastChunck'/>"
	"    <toolitem action='ActionFilter'/>"
	"  </toolbar>"
	"</ui>";

static void row_spin_changed_cb (GtkSpinButton *spin, GdauiDataProxyInfo *info);
static void
modif_buttons_make (GdauiDataProxyInfo *info)
{
	GtkWidget *wid;
	GdauiDataProxyInfoFlag flags = info->priv->flags;
	static gboolean rc_done = FALSE;

	if (!rc_done) {
                rc_done = TRUE;
                gtk_rc_parse_string ("style \"gdaui-data-proxy-info-style\"\n"
                                     "{\n"
                                     "GtkToolbar::shadow-type = GTK_SHADOW_NONE\n"
				     "GtkSpinButton::shadow-type = GTK_SHADOW_NONE\n"
                                     "xthickness = 0\n"
                                     "ythickness = 0\n"
                                     "}\n"
                                     "widget \"*.gdaui-data-proxy-info\" style \"gdaui-data-proxy-info-style\"");
        }

	if (! info->priv->data_proxy)
		return;

	if (info->priv->uimanager) {
		if (info->priv->merge_id_row_modif) {
			gtk_ui_manager_remove_ui (info->priv->uimanager, info->priv->merge_id_row_modif);
			info->priv->merge_id_row_modif = 0;
		}
		if (info->priv->merge_id_row_move) {
			gtk_ui_manager_remove_ui (info->priv->uimanager, info->priv->merge_id_row_move);
			info->priv->merge_id_row_move = 0;
		}
		if (info->priv->merge_id_chunck_change) {
			gtk_ui_manager_remove_ui (info->priv->uimanager, info->priv->merge_id_chunck_change);
			info->priv->merge_id_chunck_change = 0;
		}
		gtk_ui_manager_remove_action_group (info->priv->uimanager, info->priv->agroup);
		info->priv->agroup = NULL;
		gtk_ui_manager_ensure_update (info->priv->uimanager);
	}
	else
		info->priv->uimanager = gtk_ui_manager_new ();	

	info->priv->agroup = gdaui_data_proxy_get_actions_group (info->priv->data_proxy);
	gtk_ui_manager_insert_action_group (info->priv->uimanager, info->priv->agroup, 0);

	if (flags & (GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS |
		     GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS |
		     GDAUI_DATA_PROXY_INFO_CHUNCK_CHANGE_BUTTONS)) {
		GtkUIManager *ui;
		ui = info->priv->uimanager;
		if (flags & GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS)
			info->priv->merge_id_row_modif = gtk_ui_manager_add_ui_from_string (ui, ui_row_modif,
											    -1, NULL);
		if (flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS)
			info->priv->merge_id_row_move = gtk_ui_manager_add_ui_from_string (ui, ui_row_move,
											   -1, NULL);
		if (flags & GDAUI_DATA_PROXY_INFO_CHUNCK_CHANGE_BUTTONS)
			info->priv->merge_id_chunck_change = gtk_ui_manager_add_ui_from_string (ui, ui_chunck_change,
												-1, NULL);
	}

	if (! info->priv->buttons_bar) {
		GtkUIManager *ui;
		ui = info->priv->uimanager;
		info->priv->buttons_bar = gtk_ui_manager_get_widget (ui, "/ToolBar");
		gtk_toolbar_set_icon_size (GTK_TOOLBAR (info->priv->buttons_bar), GTK_ICON_SIZE_MENU);
		g_object_set (G_OBJECT (info->priv->buttons_bar), "toolbar-style", GTK_TOOLBAR_ICONS, NULL);
		gtk_widget_set_name (info->priv->buttons_bar, "gdaui-data-proxy-info");
		gtk_box_pack_start (GTK_BOX (info), info->priv->buttons_bar, TRUE, TRUE, 0);
		gtk_widget_show (info->priv->buttons_bar);
	}

	if (flags & GDAUI_DATA_PROXY_INFO_CURRENT_ROW) {
		if (info->priv->tool_item) {
			/* remove the current contents */
			gtk_widget_destroy (gtk_bin_get_child (GTK_BIN (info->priv->tool_item)));
			info->priv->row_spin = NULL;
			info->priv->current_sample = NULL;
		}
		else {
			GtkToolItem *ti;
			ti = gtk_tool_item_new  ();
			gtk_toolbar_insert (GTK_TOOLBAR (info->priv->buttons_bar), ti, -1);
			info->priv->tool_item = GTK_WIDGET (ti);	
		}

		GtkWidget *toolwid;
		PangoContext *pc;
		PangoFontDescription *fd, *fdc;
		pc = gtk_widget_get_pango_context (GTK_WIDGET (info));
		fd = pango_context_get_font_description (pc);
		fdc = pango_font_description_copy (fd);
		pango_font_description_set_size (fdc,
						 pango_font_description_get_size (fd) * .8);

		if (flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS) {
			toolwid = gtk_hbox_new (FALSE, 0);

			/* read-write spin counter (mainly for forms) */
			wid = gtk_spin_button_new_with_range (0, 1, 1);
			gtk_widget_modify_font (wid, fdc);
			gtk_widget_set_name (wid, "gdaui-data-proxy-info");
			gtk_spin_button_set_digits (GTK_SPIN_BUTTON (wid), 0);
			gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (wid), TRUE);
			gtk_box_pack_start (GTK_BOX (toolwid), wid, FALSE, TRUE, 2);
			gtk_widget_set_sensitive (wid, FALSE);
			info->priv->row_spin = wid;
			g_signal_connect (G_OBJECT (wid), "value-changed",
					  G_CALLBACK (row_spin_changed_cb), info);

			/* rows counter */
			wid = gtk_label_new (" /?");
			gtk_widget_modify_font (wid, fdc);
			info->priv->current_sample = wid;
			gtk_box_pack_start (GTK_BOX (toolwid), wid, FALSE, FALSE, 2);
		}
		else {
			/* read-only counter (mainly for grids) */
			wid = gtk_label_new ("? - ? /?");
			gtk_widget_modify_font (wid, fdc);
			info->priv->current_sample = wid;
			toolwid = wid;
		}

		pango_font_description_free (fdc);

		gtk_container_add (GTK_CONTAINER (info->priv->tool_item), toolwid);
		gtk_widget_show_all (info->priv->tool_item);
	}
	else if (info->priv->tool_item)
		gtk_widget_hide (info->priv->tool_item);
}

static void
row_spin_changed_cb (GtkSpinButton *spin, GdauiDataProxyInfo *info)
{
	gint row;
	gint value = gtk_spin_button_get_value (spin);

	if ((value >= 1) &&
	    (value <= gda_data_model_get_n_rows (GDA_DATA_MODEL (info->priv->proxy))))
		row = value - 1;

	gda_data_model_iter_move_to_row (gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (info->priv->data_proxy)),
					 row);
}

static gboolean idle_modif_buttons_update (GdauiDataProxyInfo *info);
static void
modif_buttons_update (GdauiDataProxyInfo *info)
{
	if (info->priv->idle_id == 0)
		info->priv->idle_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
						       (GSourceFunc) idle_modif_buttons_update,
						       g_object_ref (info), g_object_unref);
}


#define BLOCK_SPIN (g_signal_handlers_block_by_func (G_OBJECT (info->priv->row_spin), \
						     G_CALLBACK (row_spin_changed_cb), info))
#define UNBLOCK_SPIN (g_signal_handlers_unblock_by_func (G_OBJECT (info->priv->row_spin), \
							 G_CALLBACK (row_spin_changed_cb), info))
static gboolean
idle_modif_buttons_update (GdauiDataProxyInfo *info)
{
	GdaDataModelIter *model_iter;
	gboolean wrows, filtered_proxy = FALSE;
	gint has_selection;
	gint row;

	gint proxy_rows = 0;
	gint proxied_rows = 0;
	gint all_rows = 0;

	GtkAction *action;
	gint sample_first_row = 0, sample_last_row = 0, sample_size = 0;
	GdauiDataProxyInfoFlag flags = 0;

	model_iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (info->priv->data_proxy));
	if (info->priv->proxy) {
		filtered_proxy = gda_data_proxy_get_filter_expr (info->priv->proxy) ? TRUE : FALSE;
		proxy_rows = gda_data_model_get_n_rows (GDA_DATA_MODEL (info->priv->proxy));
		if (filtered_proxy) {
			proxied_rows = gda_data_proxy_get_filtered_n_rows (info->priv->proxy);
			all_rows = proxied_rows;
		}
		else {
			proxied_rows = gda_data_proxy_get_proxied_model_n_rows (info->priv->proxy);
			all_rows = proxied_rows + gda_data_proxy_get_n_new_rows (info->priv->proxy);
		}

		/* samples don't take into account the proxy's inserted rows */
		sample_first_row = gda_data_proxy_get_sample_start (info->priv->proxy);
		sample_last_row = gda_data_proxy_get_sample_end (info->priv->proxy);
		sample_size = gda_data_proxy_get_sample_size (info->priv->proxy);

		flags = gda_data_model_get_access_flags (GDA_DATA_MODEL (info->priv->proxy));
	}

	/* sensitiveness of the text indications and of the spin button */
	wrows = (proxy_rows <= 0) ? FALSE : TRUE;
	row = model_iter ? gda_data_model_iter_get_row (model_iter) : 0;
	if (info->priv->flags & GDAUI_DATA_PROXY_INFO_CURRENT_ROW) {
		if (proxy_rows < 0) {
			if (info->priv->flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS) {
				BLOCK_SPIN;
				gtk_spin_button_set_range (GTK_SPIN_BUTTON (info->priv->row_spin), 0, 1);
				gtk_spin_button_set_value (GTK_SPIN_BUTTON (info->priv->row_spin), 0);
				UNBLOCK_SPIN;
				gtk_label_set_text (GTK_LABEL (info->priv->current_sample), " /?");
			}
			else
				gtk_label_set_text (GTK_LABEL (info->priv->current_sample), "? - ? /?");
		}
		else {
			gchar *str;
			gint total;

			total = sample_first_row + proxy_rows;
			if (info->priv->flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS) {
				if (total <= 0)
					str = g_strdup (" / 0");
				else {
					if (filtered_proxy)
						str = g_strdup_printf (" / (%d)", proxy_rows);
					else
						str = g_strdup_printf (" / %d", proxy_rows);
				}
				BLOCK_SPIN;
				gtk_spin_button_set_range (GTK_SPIN_BUTTON (info->priv->row_spin), 1, proxy_rows);
				if (row >= 0)
					if (gtk_spin_button_get_value (GTK_SPIN_BUTTON (info->priv->row_spin)) != row+1)
						gtk_spin_button_set_value (GTK_SPIN_BUTTON (info->priv->row_spin), row+1);
				UNBLOCK_SPIN;
			}
			else {
				if (total <= 0)
					str = g_strdup_printf ("0 - 0 / 0");
				else {
					if (all_rows < 0)
						str = g_strdup_printf ("%d - %d /?", sample_first_row + 1, total);
					else {
						if (filtered_proxy)
							str = g_strdup_printf ("%d - %d / (%d)",
									       sample_first_row + 1, total, all_rows);
						else
							str = g_strdup_printf ("%d - %d / %d",
									       sample_first_row + 1, total, all_rows);
					}
				}
			}

			gtk_label_set_text (GTK_LABEL (info->priv->current_sample), str);
			g_free (str);
		}

		gtk_widget_set_sensitive (info->priv->current_sample, wrows);
		if (info->priv->row_spin)
			gtk_widget_set_sensitive (info->priv->row_spin, wrows && (row >= 0));
	}

	/* current row modifications */
	if (info->priv->buttons_bar) {
		gboolean changed = FALSE;
		gboolean to_be_deleted = FALSE;
		gboolean is_inserted = FALSE;
		gboolean force_del_btn = FALSE;
		gboolean force_undel_btn = FALSE;

		if (info->priv->proxy) {
			changed = gda_data_proxy_has_changed (info->priv->proxy);

			has_selection = (row >= 0) ? TRUE : FALSE;
			if (has_selection) {
				to_be_deleted = gda_data_proxy_row_is_deleted (info->priv->proxy, row);
				is_inserted = gda_data_proxy_row_is_inserted (info->priv->proxy, row);
			}
			else
				if (GDAUI_IS_RAW_GRID (info->priv->data_proxy)) {
					/* bad for encapsulation, but very useful... */
					GList *sel, *list;

					sel = _gdaui_raw_grid_get_selection ((GdauiRawGrid*) info->priv->data_proxy);
					if (sel) {
						list = sel;
						while (list && (!force_del_btn || !force_undel_btn)) {
							if ((GPOINTER_TO_INT (list->data) != -1) &&
							    gda_data_proxy_row_is_deleted (info->priv->proxy,
											   GPOINTER_TO_INT (list->data)))
								force_undel_btn = TRUE;
							else
								force_del_btn = TRUE;
							list = g_list_next (list);
						}
						list = sel;

						is_inserted = TRUE;
						while (list && is_inserted) {
							if (GPOINTER_TO_INT (list->data) != -1)
								is_inserted = FALSE;
							list = g_list_next (list);
						}
						g_list_free (sel);
					}
				}
		}

		if (info->priv->flags & GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS) {
			GdauiDataProxyWriteMode mode;
			mode = gdaui_data_proxy_get_write_mode (info->priv->data_proxy);

			action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionCommit");
			g_object_set (G_OBJECT (action), "sensitive", changed ? TRUE : FALSE, NULL);
			if (mode == GDAUI_DATA_PROXY_WRITE_ON_VALUE_CHANGE)
				gtk_action_set_visible (action, FALSE);
			else
				gtk_action_set_visible (action, TRUE);

			action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionReset");
			g_object_set (G_OBJECT (action), "sensitive", changed ? TRUE : FALSE, NULL);
			if (mode == GDAUI_DATA_PROXY_WRITE_ON_VALUE_CHANGE)
				gtk_action_set_visible (action, FALSE);
			else
				gtk_action_set_visible (action, TRUE);

			action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionNew");
			g_object_set (G_OBJECT (action), "sensitive",
				      flags & GDA_DATA_MODEL_ACCESS_INSERT ? TRUE : FALSE, NULL);

			action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionDelete");
			wrows = is_inserted ||
				((flags & GDA_DATA_MODEL_ACCESS_DELETE) &&
				 (force_del_btn || (! to_be_deleted && has_selection)));
			g_object_set (G_OBJECT (action), "sensitive", wrows, NULL);

			action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionUndelete");
			wrows = (flags & GDA_DATA_MODEL_ACCESS_DELETE) &&
				(force_undel_btn || (to_be_deleted && has_selection));
			g_object_set (G_OBJECT (action), "sensitive", wrows, NULL);

			if ((mode == GDAUI_DATA_PROXY_WRITE_ON_ROW_CHANGE) ||
			    (mode == GDAUI_DATA_PROXY_WRITE_ON_VALUE_CHANGE) ||
			    (mode == GDAUI_DATA_PROXY_WRITE_ON_VALUE_ACTIVATED))
				gtk_action_set_visible (action, FALSE);
			else
				gtk_action_set_visible (action, TRUE);
		}
	}

	/* current row moving */
	if (info->priv->flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS) {
		action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionFirstRecord");
		g_object_set (G_OBJECT (action), "sensitive", (row <= 0) ? FALSE : TRUE, NULL);
		action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionPrevRecord");
		g_object_set (G_OBJECT (action), "sensitive", (row <= 0) ? FALSE : TRUE, NULL);
		action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionNextRecord");
		g_object_set (G_OBJECT (action), "sensitive", (row == proxy_rows -1) || (row < 0) ? FALSE : TRUE,
			      NULL);
		action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionLastRecord");
		g_object_set (G_OBJECT (action), "sensitive", (row == proxy_rows -1) || (row < 0) ? FALSE : TRUE,
			      NULL);
	}

	/* chunck indications */
	if (info->priv->flags & GDAUI_DATA_PROXY_INFO_CHUNCK_CHANGE_BUTTONS) {
		gboolean abool;
		wrows = (sample_size > 0) ? TRUE : FALSE;
		action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionFirstChunck");
		g_object_set (G_OBJECT (action), "sensitive", wrows && sample_first_row > 0 ? TRUE : FALSE, NULL);
		action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionPrevChunck");
		g_object_set (G_OBJECT (action), "sensitive", wrows && sample_first_row > 0 ? TRUE : FALSE, NULL);
		action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionNextChunck");
		abool = (proxied_rows != -1) ? wrows && (sample_last_row < proxied_rows - 1) : TRUE;
		g_object_set (G_OBJECT (action), "sensitive", abool, NULL);
		action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionLastChunck");
		g_object_set (G_OBJECT (action), "sensitive", wrows && (sample_last_row < proxied_rows - 1), NULL);
	}

	/* filter */
	if (info->priv->flags & GDAUI_DATA_PROXY_INFO_NO_FILTER) {
		action = gtk_ui_manager_get_action (info->priv->uimanager, "/ToolBar/ActionFilter");
		g_object_set (G_OBJECT (action), "visible", FALSE, NULL);
	}

	if (info->priv->uimanager)
		gtk_ui_manager_ensure_update (info->priv->uimanager);

	/* remove IDLE */
	info->priv->idle_id = 0;
	return FALSE;
}
