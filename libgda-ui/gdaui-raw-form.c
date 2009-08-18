/* gdaui-raw-form.c
 *
 * Copyright (C) 2002 - 2009 Vivien Malerba  <malerba@gnome-db.org>
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

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <libgda/libgda.h>
#include <libgda/gda-data-proxy.h>
#include <glib/gi18n-lib.h>
#include "gdaui-raw-form.h"
#include "gdaui-data-widget.h"
#include "gdaui-basic-form.h"
#include "gdaui-data-widget-filter.h"
#include "internal/utility.h"

static void gdaui_raw_form_class_init (GdauiRawFormClass * class);
static void gdaui_raw_form_init (GdauiRawForm *wid);
static void gdaui_raw_form_dispose (GObject   *object);

static void gdaui_raw_form_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void gdaui_raw_form_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec);

static void gdaui_raw_form_initialize (GdauiRawForm *form, GtkWidget *layout, GHashTable *box_widgets);

static GError *iter_validate_set_cb (GdaDataModelIter *iter, GdauiRawForm *form);
static void iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdauiRawForm *form);
static void proxy_changed_cb (GdaDataProxy *proxy, GdauiRawForm *form);
static void proxy_reset_cb (GdaDataProxy *proxy, GdauiRawForm *form);
static void proxy_row_inserted_or_removed_cb (GdaDataProxy *proxy, gint row, GdauiRawForm *form);

/* GdauiDataWidget interface */
static void            gdaui_raw_form_widget_init         (GdauiDataWidgetIface *iface);
static GdaDataProxy   *gdaui_raw_form_get_proxy           (GdauiDataWidget *iface);
static void            gdaui_raw_form_col_set_show        (GdauiDataWidget *iface, gint column, gboolean shown);
static void            gdaui_raw_form_set_column_editable (GdauiDataWidget *iface, gint column, gboolean editable);
static void            gdaui_raw_form_show_column_actions (GdauiDataWidget *iface, gint column, gboolean show_actions);
static GtkActionGroup *gdaui_raw_form_get_actions_group   (GdauiDataWidget *iface);
static GdaDataModelIter *gdaui_raw_form_widget_get_data_set   (GdauiDataWidget *iface);
static GdaDataModel   *gdaui_raw_form_widget_get_gda_model (GdauiDataWidget *iface);
static void            gdaui_raw_form_widget_set_gda_model (GdauiDataWidget *iface, GdaDataModel *model);
static gboolean        gdaui_raw_form_widget_set_write_mode (GdauiDataWidget *iface, GdauiDataWidgetWriteMode mode);
static GdauiDataWidgetWriteMode gdaui_raw_form_widget_get_write_mode (GdauiDataWidget *iface);
static void gdaui_raw_form_set_data_layout (GdauiDataWidget  *iface, gpointer  data);

struct _GdauiRawFormPriv
{
	GdaDataModel               *model;
	GdaDataProxy               *proxy; /* proxy for @model */
	GdaDataModelIter           *iter;  /* proxy's iter */

	GdauiDataWidgetWriteMode  write_mode;

	GtkActionGroup             *actions_group;

	GtkWidget                  *filter;
	GtkWidget                  *filter_window;
};

#define PAGE_NO_DATA 0
#define PAGE_FORM    1

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* properties */
enum
{
        PROP_0,
	PROP_MODEL,
};

GType
gdaui_raw_form_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiRawFormClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_raw_form_class_init,
			NULL,
			NULL,
			sizeof (GdauiRawForm),
			0,
			(GInstanceInitFunc) gdaui_raw_form_init
		};		
		
		static const GInterfaceInfo work_widget_info = {
                        (GInterfaceInitFunc) gdaui_raw_form_widget_init,
                        NULL,
                        NULL
                };
		
		type = g_type_register_static (GDAUI_TYPE_BASIC_FORM, "GdauiRawForm", &info, 0);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_WIDGET, &work_widget_info);
	}

	return type;
}

static void
gdaui_raw_form_widget_init (GdauiDataWidgetIface *iface)
{
	iface->get_proxy = gdaui_raw_form_get_proxy;
	iface->col_set_show = gdaui_raw_form_col_set_show;
	iface->set_column_editable = gdaui_raw_form_set_column_editable;
	iface->show_column_actions = gdaui_raw_form_show_column_actions;
	iface->get_actions_group = gdaui_raw_form_get_actions_group;
	iface->get_data_set = gdaui_raw_form_widget_get_data_set;
	iface->get_gda_model = gdaui_raw_form_widget_get_gda_model;
	iface->set_gda_model = gdaui_raw_form_widget_set_gda_model;
	iface->set_write_mode = gdaui_raw_form_widget_set_write_mode;
	iface->get_write_mode = gdaui_raw_form_widget_get_write_mode;
	iface->set_data_layout = gdaui_raw_form_set_data_layout;
}

static void
gdaui_raw_form_class_init (GdauiRawFormClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);
	object_class->dispose = gdaui_raw_form_dispose;

	/* Properties */
        object_class->set_property = gdaui_raw_form_set_property;
        object_class->get_property = gdaui_raw_form_get_property;
	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model", _("Data to display"), NULL, GDA_TYPE_DATA_MODEL,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void action_new_cb (GtkAction *action, GdauiRawForm *form);
static void action_delete_cb (GtkAction *action, GdauiRawForm *form);
static void action_undelete_cb (GtkAction *action, GdauiRawForm *form);
static void action_commit_cb (GtkAction *action, GdauiRawForm *form);
static void action_reset_cb (GtkAction *action, GdauiRawForm *form);
static void action_first_record_cb (GtkAction *action, GdauiRawForm *form);
static void action_prev_record_cb (GtkAction *action, GdauiRawForm *form);
static void action_next_record_cb (GtkAction *action, GdauiRawForm *form);
static void action_last_record_cb (GtkAction *action, GdauiRawForm *form);
static void action_filter_cb (GtkAction *action, GdauiRawForm *form);

static GtkActionEntry ui_actions[] = {
	{ "ActionNew", GTK_STOCK_ADD, "_New", NULL, "Create a new data entry", G_CALLBACK (action_new_cb)},
	{ "ActionDelete", GTK_STOCK_REMOVE, "_Delete", NULL, "Delete the selected entry", G_CALLBACK (action_delete_cb)},
	{ "ActionUndelete", GTK_STOCK_UNDELETE, "_Undelete", NULL, "Cancels the deletion of the current entry", 
	  G_CALLBACK (action_undelete_cb)},	
	{ "ActionCommit", GTK_STOCK_SAVE, "_Commit", NULL, "Commit the latest changes", G_CALLBACK (action_commit_cb)},
	{ "ActionReset", GTK_STOCK_REFRESH, "_Reset", NULL, "Reset the data", G_CALLBACK (action_reset_cb)},
	{ "ActionFirstRecord", GTK_STOCK_GOTO_FIRST, "_First record", NULL, "Go to first record of records", 
	  G_CALLBACK (action_first_record_cb)},
	{ "ActionLastRecord", GTK_STOCK_GOTO_LAST, "_Last record", NULL, "Go to last record of records", 
	  G_CALLBACK (action_last_record_cb)},
	{ "ActionPrevRecord", GTK_STOCK_GO_BACK, "_Previous record", NULL, "Go to previous record of records", 
	  G_CALLBACK (action_prev_record_cb)},
	{ "ActionNextRecord", GTK_STOCK_GO_FORWARD, "Ne_xt record", NULL, "Go to next record of records",
	  G_CALLBACK (action_next_record_cb)},
	{ "ActionFilter", GTK_STOCK_FIND, "Filter", NULL, "Filter records",
	  G_CALLBACK (action_filter_cb)}
};

static void
action_new_activated_cb (GtkAction *action, GdauiRawForm *wid)
{
	/* make the first entry grab the focus */
	if (wid->priv->iter && GDA_SET (wid->priv->iter)->holders)
		gdaui_basic_form_entry_grab_focus (GDAUI_BASIC_FORM (wid), 
						      (GdaHolder *)
						      GDA_SET (wid->priv->iter)->holders->data);
}

static void
form_activated_cb (GdauiRawForm *form, gpointer data)
{
	if (form->priv->write_mode == GDAUI_DATA_WIDGET_WRITE_ON_VALUE_ACTIVATED) {
		gint row;

		row = gda_data_model_iter_get_row (form->priv->iter);
		if (row >= 0) {
			/* write back the current row */
			if (gda_data_proxy_row_has_changed (form->priv->proxy, row)) {
				GError *error = NULL;
				if (!gda_data_proxy_apply_row_changes (form->priv->proxy, row, &error)) {
					gboolean discard;
					discard = _gdaui_utility_display_error_with_keep_or_discard_choice ((GdauiDataWidget *) form, 
												     error);
					if (discard)
						gda_data_proxy_cancel_row_changes (form->priv->proxy, row, -1);
					g_error_free (error);
				}
			}
		}
	}
}

static void
form_param_changed_cb (GdauiRawForm *form, gpointer data)
{
	if (form->priv->write_mode == GDAUI_DATA_WIDGET_WRITE_ON_VALUE_CHANGE) {
		gint row;

		row = gda_data_model_iter_get_row (form->priv->iter);
		if (row >= 0) {
			/* write back the current row */
			if (gda_data_proxy_row_has_changed (form->priv->proxy, row)) {
				GError *error = NULL;
				if (!gda_data_proxy_apply_row_changes (form->priv->proxy, row, &error)) {
					_gdaui_utility_display_error ((GdauiDataWidget *) form, TRUE, error);
					if (error)
						g_error_free (error);
				}
			}
		}
	}
}

static void
gdaui_raw_form_init (GdauiRawForm *wid)
{
	GtkAction *action;
	wid->priv = g_new0 (GdauiRawFormPriv, 1);

	wid->priv->model = NULL;
	wid->priv->proxy = NULL;
	wid->priv->iter = NULL;
	wid->priv->write_mode = GDAUI_DATA_WIDGET_WRITE_ON_DEMAND;

	g_signal_connect (G_OBJECT (wid), "activated",
			  G_CALLBACK (form_activated_cb), NULL);
	g_signal_connect (G_OBJECT (wid), "param_changed",
			  G_CALLBACK (form_param_changed_cb), NULL);

	/* action group */
        wid->priv->actions_group = gtk_action_group_new ("Actions");
        gtk_action_group_add_actions (wid->priv->actions_group, ui_actions, G_N_ELEMENTS (ui_actions), wid);
	action = gtk_action_group_get_action (wid->priv->actions_group, "ActionNew");
	g_signal_connect (G_OBJECT (action), "activate",
			  G_CALLBACK (action_new_activated_cb), wid);

	wid->priv->filter = NULL;
	wid->priv->filter_window = NULL;
}

/**
 * gdaui_raw_form_new
 * @model: a #GdaDataModel, or %NULL
 *
 * Creates a new #GdauiRawForm widget to display data in @model
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_raw_form_new (GdaDataModel *model)
{
	GObject *obj;
	
	obj = g_object_new (GDAUI_TYPE_RAW_FORM, "model", model, NULL);

	return GTK_WIDGET (obj);	
}

static void
gdaui_raw_form_dispose (GObject *object)
{
	GdauiRawForm *form;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_RAW_FORM (object));
	form = GDAUI_RAW_FORM (object);

	if (form->priv) {
		if (form->priv->filter) 
			gtk_widget_destroy (form->priv->filter);
		if (form->priv->filter_window) 
			gtk_widget_destroy (form->priv->filter_window);

		/* proxy's iterator */
		if (form->priv->iter) {
			g_signal_handlers_disconnect_by_func (form->priv->iter,
							      G_CALLBACK (iter_row_changed_cb), form);
			g_signal_handlers_disconnect_by_func (form->priv->iter,
							      G_CALLBACK (iter_validate_set_cb), form);
			g_object_unref (form->priv->iter);
			form->priv->iter = NULL;
		}
		
		/* proxy */
		if (form->priv->proxy) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (form->priv->proxy),
							      G_CALLBACK (proxy_row_inserted_or_removed_cb), form);
			g_signal_handlers_disconnect_by_func (G_OBJECT (form->priv->proxy),
							      G_CALLBACK (proxy_changed_cb), form);
			g_signal_handlers_disconnect_by_func (G_OBJECT (form->priv->proxy),
							      G_CALLBACK (proxy_reset_cb), form);
			g_object_unref (form->priv->proxy);
			form->priv->proxy = NULL;
		}

		/* UI */
		if (form->priv->actions_group) 
			g_object_unref (G_OBJECT (form->priv->actions_group));

		/* the private area itself */
		g_free (form->priv);
		form->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

static void
gdaui_raw_form_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
	GdauiRawForm *form;
	gpointer ptr;

        form = GDAUI_RAW_FORM (object);
        if (form->priv) {
                switch (param_id) {
		case PROP_MODEL:
			ptr = GDA_DATA_MODEL(g_value_get_object (value));
			if (ptr)
				g_return_if_fail (GDA_IS_DATA_MODEL (ptr));
			if (form->priv->proxy) {
				/* remove old data model settings */
				g_signal_handlers_disconnect_by_func (form->priv->iter,
								      G_CALLBACK (iter_row_changed_cb), form);
				g_signal_handlers_disconnect_by_func (form->priv->iter,
								      G_CALLBACK (iter_validate_set_cb), form);

				g_object_unref (G_OBJECT (form->priv->iter));
				form->priv->iter = NULL;
				
				g_signal_handlers_disconnect_by_func (G_OBJECT (form->priv->proxy),
								      G_CALLBACK (proxy_row_inserted_or_removed_cb), 
								      form);
				g_signal_handlers_disconnect_by_func (G_OBJECT (form->priv->proxy),
								      G_CALLBACK (proxy_changed_cb), form);
				g_signal_handlers_disconnect_by_func (G_OBJECT (form->priv->proxy),
								      G_CALLBACK (proxy_reset_cb), form);
				g_object_unref (G_OBJECT (form->priv->proxy));
				form->priv->proxy = NULL;
				form->priv->model = NULL;
			}
			if (ptr) {
				/* handle the data model */
				if (GDA_IS_DATA_PROXY (ptr)) {
					form->priv->proxy = (GdaDataProxy *) ptr;
					g_object_ref (ptr);
				}
				else
					form->priv->proxy = (GdaDataProxy *) gda_data_proxy_new ((GdaDataModel*) ptr);

				form->priv->model = gda_data_proxy_get_proxied_model (form->priv->proxy);

				form->priv->iter = gda_data_model_create_iter ((GdaDataModel *) form->priv->proxy);
				gda_data_model_iter_move_to_row (form->priv->iter, 0);

				g_signal_connect (form->priv->iter, "validate-set",
						  G_CALLBACK (iter_validate_set_cb), form);
				g_signal_connect (form->priv->iter, "row_changed",
						  G_CALLBACK (iter_row_changed_cb), form);

				g_signal_connect (G_OBJECT (form->priv->proxy), "row_inserted",
						  G_CALLBACK (proxy_row_inserted_or_removed_cb), form);
				g_signal_connect (G_OBJECT (form->priv->proxy), "row_removed",
						  G_CALLBACK (proxy_row_inserted_or_removed_cb), form);
				g_signal_connect (G_OBJECT (form->priv->proxy), "changed",
						  G_CALLBACK (proxy_changed_cb), form);
				g_signal_connect (G_OBJECT (form->priv->proxy), "reset",
						  G_CALLBACK (proxy_reset_cb), form);

				/* we don't want chuncking */
				g_object_set (object, "paramlist", form->priv->iter, NULL);
				gda_data_proxy_set_sample_size (form->priv->proxy, 0);
				gdaui_raw_form_initialize (form, NULL, NULL);
			}

			gdaui_raw_form_widget_set_write_mode ((GdauiDataWidget *) form, form->priv->write_mode);
			g_signal_emit_by_name (object, "proxy_changed", form->priv->proxy);
			g_signal_emit_by_name (object, "iter_changed", form->priv->iter);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

static void
gdaui_raw_form_get_property (GObject *object,
			   guint param_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	GdauiRawForm *form;

        form = GDAUI_RAW_FORM (object);
        if (form->priv) {
                switch (param_id) {
		case PROP_MODEL:
			g_value_set_object(value, form->priv->model);					
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
        }	
}

static GError *
iter_validate_set_cb (GdaDataModelIter *iter, GdauiRawForm *form)
{
	GError *error = NULL;
	gint row = gda_data_model_iter_get_row (iter);
	
	if (row < 0)
		return NULL;

	if ((form->priv->write_mode >= GDAUI_DATA_WIDGET_WRITE_ON_ROW_CHANGE) &&
		/* write back the current row */
	    gda_data_proxy_row_has_changed (form->priv->proxy, row) &&
	    !gda_data_proxy_apply_row_changes (form->priv->proxy, row, &error)) {
		if (_gdaui_utility_display_error_with_keep_or_discard_choice ((GdauiDataWidget *) form, 
									      error)) {
			gda_data_proxy_cancel_row_changes (form->priv->proxy, row, -1);
			if (error) {
				g_error_free (error);
				error = NULL;
			}
		}
	}
	
	return error;
}

static void
iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdauiRawForm *form)
{
	gdaui_basic_form_set_current_as_orig (GDAUI_BASIC_FORM (form));

	gtk_widget_set_sensitive (GTK_WIDGET (form), (row == -1) ? FALSE : TRUE);
	if (row >= 0) {
		GSList *params;
		guint attributes;
		GdaHolder *param;
		gint i;

		for (i = 0, params = ((GdaSet *) iter)->holders; params; i++, params = params->next) {
			param = (GdaHolder *) params->data;
			attributes = gda_data_proxy_get_value_attributes (form->priv->proxy, row, i);
			gdaui_basic_form_entry_set_editable ((GdauiBasicForm *) form, 
								param, !(attributes & GDA_VALUE_ATTR_NO_MODIF));
		}
	}
}

static void
proxy_changed_cb (GdaDataProxy *proxy, GdauiRawForm *form)
{
	/* TO remove ? */
	gtk_widget_set_sensitive (GTK_WIDGET (form), 
				  gda_data_model_get_n_rows (GDA_DATA_MODEL (form->priv->proxy)) == 0 ? FALSE : TRUE);
}

static void
proxy_reset_cb (GdaDataProxy *proxy, GdauiRawForm *form)
{
	g_object_ref (G_OBJECT (proxy));
	g_object_set (G_OBJECT (form), "model", proxy, NULL);
	g_object_unref (G_OBJECT (proxy));
}

static void
proxy_row_inserted_or_removed_cb (GdaDataProxy *proxy, gint row, GdauiRawForm *form)
{
	if (gda_data_model_get_n_rows (GDA_DATA_MODEL (form->priv->proxy)) != 0)
		if (gda_data_model_iter_get_row (form->priv->iter) == -1)
			gda_data_model_iter_move_to_row (form->priv->iter, row > 0 ? row - 1 : 0);
}

/*
 * Real initialization
 */
static void 
gdaui_raw_form_initialize (GdauiRawForm *form, GtkWidget *layout, GHashTable *box_widgets)
{
	/*
	 * handling of the layout
	 */
	if (layout && box_widgets) {
		/* REM: the @box_widgets has keys for query fields, and we need to provide
		   gdaui_basic_form_new_in_layout() with a hash table which has keys for GdaSetNode;
		   this is why we need to use another hash table: 'fbw' */

		TO_IMPLEMENT;
		/* GSList *list; */
/* 		GdaHolder *param; */
/* 		GdaSetNode *node; */
/* 		gpointer widget; */

/* 		fbw = g_hash_table_new (NULL, NULL); */
/* 		g_object_get (G_OBJECT (orig_query), "really_all_fields", &list, NULL); */
/* 		while (list) { */
/* 			widget = g_hash_table_lookup (box_widgets, list->data); */
/* 			if (widget) { */
/* 				node = gdaui_work_core_find_context_node (form->priv->core, GDA_QUERY_FIELD (list->data)); */
/* 				if (node) */
/* 					g_hash_table_insert (fbw, node, widget); */
/* 			} */
/* 			list = g_slist_next (list); */
/* 		} */
	}

	/* 
	 * the form itself 
	 */
	if (!layout && gda_data_proxy_is_read_only (form->priv->proxy))
		gdaui_basic_form_show_entry_actions (GDAUI_BASIC_FORM (form), FALSE);

	/* data display update */
	proxy_changed_cb (form->priv->proxy, form);
}

/*
 *
 * Modification buttons (Commit changes, Reset form, New entry, Delete)
 *
 */
static void
action_new_cb (GtkAction *action, GdauiRawForm *form)
{
	gint newrow;
	GError *error = NULL;
	GSList *list;

	if (form->priv->write_mode >= GDAUI_DATA_WIDGET_WRITE_ON_ROW_CHANGE) 
		if (! gda_set_is_valid (GDA_SET (form->priv->iter), NULL))
			return;

	/* append a row in the proxy */
	newrow = gda_data_model_append_row (GDA_DATA_MODEL (form->priv->proxy), &error);
	if (newrow == -1) {
		g_warning (_("Can't append row to data model: %s"), 
			   error && error->message ? error->message : _("Unknown error"));
		g_error_free (error);
		return;
	}

	g_assert (gda_data_model_iter_move_to_row (form->priv->iter, newrow));

	/* set parameters to their default values */
	list = GDA_SET (form->priv->iter)->holders;
	while (list) {
		GdaHolder *param;
		const GValue *value;
		
		g_object_get (G_OBJECT (list->data), "full_bind", &param, NULL);
		if (! param) {
			value = gda_holder_get_default_value (GDA_HOLDER (list->data));
			if (value)
				gda_holder_set_value_to_default (GDA_HOLDER (list->data));
		}
		else
			g_object_unref (param);
		
		list = g_slist_next (list);
	}
}

static void
action_delete_cb (GtkAction *action, GdauiRawForm *form)
{
	gint row;

	row = gda_data_model_iter_get_row (form->priv->iter);
	g_return_if_fail (row >= 0);
	gda_data_proxy_delete (form->priv->proxy, row);

	if (form->priv->write_mode >= GDAUI_DATA_WIDGET_WRITE_ON_ROW_CHANGE) {
		/* force the proxy to apply the current row deletion */
		gint newrow;

		newrow = gda_data_model_iter_get_row (form->priv->iter);
		if (row == newrow) {/* => row has been marked as delete but nit yet really deleted */
			GError *error = NULL;
			if (!gda_data_proxy_apply_row_changes (form->priv->proxy, row, &error)) {
				_gdaui_utility_display_error ((GdauiDataWidget *) form, TRUE, error);
				if (error)
					g_error_free (error);
			}
		}
	}
}

static void
action_undelete_cb (GtkAction *action, GdauiRawForm *form)
{
	gint row;

	row = gda_data_model_iter_get_row (form->priv->iter);
	g_return_if_fail (row >= 0);
	gda_data_proxy_undelete (form->priv->proxy, row);
}

static void
action_commit_cb (GtkAction *action, GdauiRawForm *form)
{
	gint row;
	GError *error = NULL;
	gboolean allok = TRUE;
	gint mod1, mod2;

	mod1 = gda_data_proxy_get_n_modified_rows (form->priv->proxy);
	row = gda_data_model_iter_get_row (form->priv->iter);
	if (form->priv->write_mode >= GDAUI_DATA_WIDGET_WRITE_ON_ROW_CHANGE) {
		gint newrow;

		allok = gda_data_proxy_apply_row_changes (form->priv->proxy, row, &error);
		if (allok) {
			newrow = gda_data_model_iter_get_row (form->priv->iter);
			if (row != newrow) /* => current row has changed because the proxy had to emit a "row_removed" when
					      actually succeeded the commit 
					      => we need to come back to that row
					   */
				gda_data_model_iter_move_to_row (form->priv->iter, row);
		}
	}
	else
		allok = gda_data_proxy_apply_all_changes (form->priv->proxy, &error);

	mod2 = gda_data_proxy_get_n_modified_rows (form->priv->proxy);
	if (!allok) {
		if (mod1 != mod2)
			/* the data model has changed while doing the writing */
			_gdaui_utility_display_error ((GdauiDataWidget *) form, FALSE, error);
		else
			_gdaui_utility_display_error ((GdauiDataWidget *) form, TRUE, error);
		g_error_free (error);
	}

	iter_row_changed_cb (form->priv->iter, gda_data_model_iter_get_row (form->priv->iter), form);
}

static void
action_reset_cb (GtkAction *action, GdauiRawForm *form)
{
	gda_data_proxy_cancel_all_changes (form->priv->proxy);
	gda_data_model_send_hint (GDA_DATA_MODEL (form->priv->proxy), GDA_DATA_MODEL_HINT_REFRESH, NULL);
}

static void arrow_actions_real_do (GdauiRawForm *form, gint movement);
static void
action_first_record_cb (GtkAction *action, GdauiRawForm *form)
{
	arrow_actions_real_do (form, -2);
}

static void
action_prev_record_cb (GtkAction *action, GdauiRawForm *form)
{
	arrow_actions_real_do (form, -1);
}

static void
action_next_record_cb (GtkAction *action, GdauiRawForm *form)
{
	arrow_actions_real_do (form, 1);
}

static void
action_last_record_cb (GtkAction *action, GdauiRawForm *form)
{
	arrow_actions_real_do (form, 2);
}

static void
hide_filter_window (GdauiRawForm *form)
{
	gtk_widget_hide (form->priv->filter_window);
	gtk_grab_remove (form->priv->filter_window);
}

static gboolean
filter_event (GtkWidget *widget, GdkEventAny *event, GdauiRawForm *form)
{
	hide_filter_window (form);
	return TRUE;
}

static gboolean
key_press_filter_event (GtkWidget *widget, GdkEventKey *event, GdauiRawForm *form)
{
	if (event->keyval == GDK_Escape ||
	    event->keyval == GDK_Tab ||
            event->keyval == GDK_KP_Tab ||
            event->keyval == GDK_ISO_Left_Tab) {
		hide_filter_window (form);
		return TRUE;
	}
	return FALSE;
}

static void
filter_position_func (GtkWidget *widget,
		      GtkWidget *search_dialog,
		      gpointer   user_data)
{
	gint x, y;
	gint tree_x, tree_y;
	gint tree_width, tree_height;
	GdkWindow *window = GTK_WIDGET (widget)->window;
	GdkScreen *screen = gdk_drawable_get_screen (window);
	GtkRequisition requisition;
	gint monitor_num;
	GdkRectangle monitor;

	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);
  
	gtk_widget_realize (search_dialog);

	gdk_window_get_origin (window, &tree_x, &tree_y);
	gdk_drawable_get_size (window,
			       &tree_width,
			       &tree_height);
	gtk_widget_size_request (search_dialog, &requisition);

	if (tree_x + tree_width > gdk_screen_get_width (screen))
		x = gdk_screen_get_width (screen) - requisition.width;
	else if (tree_x + tree_width - requisition.width < 0)
		x = 0;
	else
		x = tree_x + tree_width - requisition.width;

	if (tree_y + tree_height + requisition.height > gdk_screen_get_height (screen))
		y = gdk_screen_get_height (screen) - requisition.height;
	else if (tree_y + tree_height < 0) /* isn't really possible ... */
		y = 0;
	else
		y = tree_y + tree_height;

	gtk_window_move (GTK_WINDOW (search_dialog), x, y);
}

static gboolean
popup_grab_on_window (GdkWindow  *window,
                      guint32     activate_time) 
{
        if (gdk_pointer_grab (window, TRUE,
                              GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
                              NULL, NULL,
                              activate_time) == 0) {

                if (gdk_keyboard_grab (window, TRUE, activate_time) == 0) 
                        return TRUE;
		else {
                        gdk_pointer_ungrab (activate_time);
                        return FALSE;
                }
        }

        return FALSE;
}

static void
action_filter_cb (GtkAction *action, GdauiRawForm *form)
{
	GtkWidget *toplevel;
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (form));
	
	if (!form->priv->filter_window) {
		/* create filter window */
		GtkWidget *frame, *vbox;
	       
		form->priv->filter_window = gtk_window_new (GTK_WINDOW_POPUP);

		gtk_widget_set_events (form->priv->filter_window,
				       gtk_widget_get_events (form->priv->filter_window) | GDK_KEY_PRESS_MASK);

		if (GTK_WIDGET_TOPLEVEL (toplevel) && GTK_WINDOW (toplevel)->group)
			gtk_window_group_add_window (GTK_WINDOW (toplevel)->group,
						     GTK_WINDOW (form->priv->filter_window));
		
		g_signal_connect (form->priv->filter_window, "delete_event",
				  G_CALLBACK (filter_event), form);
		g_signal_connect (form->priv->filter_window, "button_press_event",
				  G_CALLBACK (filter_event), form);
		g_signal_connect (form->priv->filter_window, "key_press_event",
				  G_CALLBACK (key_press_filter_event), form);
		frame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
		gtk_widget_show (frame);
		gtk_container_add (GTK_CONTAINER (form->priv->filter_window), frame);
		
		vbox = gtk_vbox_new (FALSE, 0);
		gtk_widget_show (vbox);
		gtk_container_add (GTK_CONTAINER (frame), vbox);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);
		
		/* add real filter widget */
		if (! form->priv->filter) {
			form->priv->filter = gdaui_data_widget_filter_new (GDAUI_DATA_WIDGET (form));
			gtk_widget_show (form->priv->filter);
		}
		gtk_container_add (GTK_CONTAINER (vbox), form->priv->filter);
	}
	else if (GTK_WIDGET_TOPLEVEL (toplevel)) {
		if (GTK_WINDOW (toplevel)->group)
			gtk_window_group_add_window (GTK_WINDOW (toplevel)->group,
						     GTK_WINDOW (form->priv->filter_window));
		else if (GTK_WINDOW (form->priv->filter_window)->group)
			gtk_window_group_remove_window (GTK_WINDOW (form->priv->filter_window)->group,
							GTK_WINDOW (form->priv->filter_window));
	}

	/* move the filter window to a correct location */
	/* FIXME: let the user specify the position function like GtkTreeView -> search_position_func() */
	gtk_grab_add (form->priv->filter_window);
	filter_position_func (GTK_WIDGET (form), form->priv->filter_window, NULL);
	gtk_widget_show (form->priv->filter_window);
	popup_grab_on_window (form->priv->filter_window->window,
			      gtk_get_current_event_time ());
}


static void
arrow_actions_real_do (GdauiRawForm *form, gint movement)
{
	gint row, oldrow;

	row = gda_data_model_iter_get_row (form->priv->iter);
	g_return_if_fail (row >= 0);
	oldrow = row;
	
	/* see if some data have been modified and need to be written to the DBMS */
	/* if ((form->priv->mode & GDAUI_ACTION_MODIF_AUTO_COMMIT) && */
/* 	    gda_data_proxy_has_changed (form->priv->proxy)) */
/* 		action_commit_cb (NULL, form); */
	
	/* movement */
	switch (movement) {
	case -2:
		row = 0;
		break;
	case -1:
		if (row > 0)
			row--;
		break;
	case 1:
		if (row < gda_data_model_get_n_rows (GDA_DATA_MODEL (form->priv->proxy)) - 1 )
			row++;
		break;
	case 2:
		row = gda_data_model_get_n_rows (GDA_DATA_MODEL (form->priv->proxy)) - 1;
		break;
	default:
		g_assert_not_reached ();
	}

	if (oldrow != row)
		gda_data_model_iter_move_to_row (form->priv->iter, row);
}

/*
 * GdauiDataWidget interface implementation
 */
static GdaDataProxy *
gdaui_raw_form_get_proxy (GdauiDataWidget *iface)
{
	GdauiRawForm *form;

	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), NULL);
	form = GDAUI_RAW_FORM (iface);
	g_return_val_if_fail (form->priv, NULL);

	return form->priv->proxy;
}

static void
gdaui_raw_form_col_set_show (GdauiDataWidget *iface, gint column, gboolean shown)
{
	GdauiRawForm *form;
	GdaHolder *param;

	g_return_if_fail (GDAUI_IS_RAW_FORM (iface));
	form = GDAUI_RAW_FORM (iface);
	g_return_if_fail (form->priv);

	param = gda_data_model_iter_get_holder_for_field (form->priv->iter, column);
	g_return_if_fail (param);
	gdaui_basic_form_entry_show (GDAUI_BASIC_FORM (form), param, shown);
}

void
gdaui_raw_form_set_column_editable (GdauiDataWidget *iface, gint column, gboolean editable)
{
	GdauiRawForm *form;

	g_return_if_fail (GDAUI_IS_RAW_FORM (iface));
	form = GDAUI_RAW_FORM (iface);
	g_return_if_fail (form->priv);

	TO_IMPLEMENT;
	/* What needs to be done:
	 * - create a gdaui_basic_form_set_entry_editable() function for GdauiBasicForm, and call it
	 * - in the GdauiDataEntry, add a GDA_VALUE_ATTR_EDITABLE property which defaults to TRUE.
	 * - imtplement the gdaui_basic_form_set_entry_editable() in the same way as gdaui_basic_form_set_entries_default()
	 *   by setting that new property
	 * - implement the new property in GdauiEntryWrapper and GdauiEntryCombo.
	 */
}

static void
gdaui_raw_form_show_column_actions (GdauiDataWidget *iface, gint column, gboolean show_actions)
{
	GdauiRawForm *form;
	
	g_return_if_fail (GDAUI_IS_RAW_FORM (iface));
	form = GDAUI_RAW_FORM (iface);
	g_return_if_fail (form->priv);
	
	/* REM: don't take care of the @column argument */
	gdaui_basic_form_show_entry_actions (GDAUI_BASIC_FORM (form), show_actions);
}

static GtkActionGroup *
gdaui_raw_form_get_actions_group (GdauiDataWidget *iface)
{
	GdauiRawForm *form;
	
	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), NULL);
	form = GDAUI_RAW_FORM (iface);
	g_return_val_if_fail (form->priv, NULL);

	return form->priv->actions_group;
}

static GdaDataModelIter *
gdaui_raw_form_widget_get_data_set (GdauiDataWidget *iface)
{
	GdauiRawForm *form;
	
	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), NULL);
	form = GDAUI_RAW_FORM (iface);
	g_return_val_if_fail (form->priv, NULL);

	return form->priv->iter;
}

static GdaDataModel *
gdaui_raw_form_widget_get_gda_model (GdauiDataWidget *iface)
{
	GdauiRawForm *form;
	
	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), NULL);
	form = GDAUI_RAW_FORM (iface);
	g_return_val_if_fail (form->priv, NULL);

	return form->priv->model;	
}

static void
gdaui_raw_form_widget_set_gda_model (GdauiDataWidget *iface, GdaDataModel *model)
{
	GdauiRawForm *form;
	
	g_return_if_fail (GDAUI_IS_RAW_FORM (iface));
	form = GDAUI_RAW_FORM (iface);
	g_return_if_fail (form->priv);

	g_object_set (form, "model", model, NULL);
}

static gboolean
gdaui_raw_form_widget_set_write_mode (GdauiDataWidget *iface, GdauiDataWidgetWriteMode mode)
{
	GdauiRawForm *form;
	
	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), FALSE);
	form = GDAUI_RAW_FORM (iface);
	g_return_val_if_fail (form->priv, FALSE);

	form->priv->write_mode = mode;
	return TRUE;
}

static GdauiDataWidgetWriteMode
gdaui_raw_form_widget_get_write_mode (GdauiDataWidget *iface)
{
	GdauiRawForm *form;
	
	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), GDAUI_DATA_WIDGET_WRITE_ON_DEMAND);
	form = GDAUI_RAW_FORM (iface);
	g_return_val_if_fail (form->priv, GDAUI_DATA_WIDGET_WRITE_ON_DEMAND);

	return form->priv->write_mode;
}

static void
gdaui_raw_form_set_data_layout (GdauiDataWidget  *iface, gpointer  data)
{
	GdauiRawForm *raw_form;
	
	g_return_if_fail (GDAUI_IS_RAW_FORM (iface));
	raw_form = GDAUI_RAW_FORM (iface);
	g_return_if_fail (raw_form->priv);

	g_object_set (G_OBJECT (raw_form), "data_layout", data, NULL);
}
