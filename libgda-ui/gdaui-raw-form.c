/*
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <libgda/libgda.h>
#include <libgda/gda-data-proxy.h>
#include <glib/gi18n-lib.h>
#include "gdaui-raw-form.h"
#include "gdaui-data-selector.h"
#include "gdaui-data-proxy.h"
#include "gdaui-basic-form.h"
#include "internal/utility.h"
#include "data-entries/gdaui-entry-shell.h"
#include <libgda/gda-debug-macros.h>

static void gdaui_raw_form_class_init (GdauiRawFormClass * class);
static void gdaui_raw_form_init (GdauiRawForm *wid);
static void gdaui_raw_form_dispose (GObject *object);

static void gdaui_raw_form_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gdaui_raw_form_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);

static GError *iter_validate_set_cb (GdaDataModelIter *iter, GdauiRawForm *form);
static void iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdauiRawForm *form);
static void proxy_changed_cb (GdaDataProxy *proxy, GdauiRawForm *form);
static void proxy_reset_cb (GdaDataProxy *proxy, GdauiRawForm *form);
static void proxy_access_changed_cb (GdaDataProxy *proxy, GdauiRawForm *form);
static void proxy_row_inserted_or_removed_cb (GdaDataProxy *proxy, gint row, GdauiRawForm *form);

/* GdauiDataProxy interface */
static void            gdaui_raw_form_widget_init         (GdauiDataProxyIface *iface);
static GdaDataProxy   *gdaui_raw_form_get_proxy           (GdauiDataProxy *iface);
static void            gdaui_raw_form_set_column_editable (GdauiDataProxy *iface, gint column, gboolean editable);
static void            gdaui_raw_form_show_column_actions (GdauiDataProxy *iface, gint column, gboolean show_actions);
static gboolean        gdaui_raw_form_supports_action       (GdauiDataProxy *iface, GdauiAction action);
static void            gdaui_raw_form_perform_action        (GdauiDataProxy *iface, GdauiAction action);
static gboolean        gdaui_raw_form_widget_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode);
static GdauiDataProxyWriteMode gdaui_raw_form_widget_get_write_mode (GdauiDataProxy *iface);

/* GdauiDataSelector interface */
static void              gdaui_raw_form_selector_init (GdauiDataSelectorIface *iface);
static GdaDataModel     *gdaui_raw_form_selector_get_model (GdauiDataSelector *iface);
static void              gdaui_raw_form_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model);
static GArray           *gdaui_raw_form_selector_get_selected_rows (GdauiDataSelector *iface);
static GdaDataModelIter *gdaui_raw_form_selector_get_data_set (GdauiDataSelector *iface);
static gboolean          gdaui_raw_form_selector_select_row (GdauiDataSelector *iface, gint row);
static void              gdaui_raw_form_selector_unselect_row (GdauiDataSelector *iface, gint row);
static void              gdaui_raw_form_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible);

struct _GdauiRawFormPriv
{
	GdaDataModel               *data_model;
	GdaDataProxy               *proxy; /* proxy for @model */
	GdaDataModelIter           *iter;  /* proxy's iter */

	GdauiDataProxyWriteMode     write_mode;
};

#define PAGE_NO_DATA 0
#define PAGE_FORM    1

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* properties */
enum {
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
			(GInstanceInitFunc) gdaui_raw_form_init,
			0
		};

		static const GInterfaceInfo proxy_info = {
                        (GInterfaceInitFunc) gdaui_raw_form_widget_init,
                        NULL,
                        NULL
                };

		static const GInterfaceInfo selector_info = {
                        (GInterfaceInitFunc) gdaui_raw_form_selector_init,
                        NULL,
                        NULL
                };

		type = g_type_register_static (GDAUI_TYPE_BASIC_FORM, "GdauiRawForm", &info, 0);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_PROXY, &proxy_info);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_SELECTOR, &selector_info);
	}

	return type;
}

static void
gdaui_raw_form_widget_init (GdauiDataProxyIface *iface)
{
	iface->get_proxy = gdaui_raw_form_get_proxy;
	iface->set_column_editable = gdaui_raw_form_set_column_editable;
	iface->show_column_actions = gdaui_raw_form_show_column_actions;
	iface->supports_action = gdaui_raw_form_supports_action;
	iface->perform_action = gdaui_raw_form_perform_action;
	iface->set_write_mode = gdaui_raw_form_widget_set_write_mode;
	iface->get_write_mode = gdaui_raw_form_widget_get_write_mode;
}

static void
gdaui_raw_form_selector_init (GdauiDataSelectorIface *iface)
{
	iface->get_model = gdaui_raw_form_selector_get_model;
	iface->set_model = gdaui_raw_form_selector_set_model;
	iface->get_selected_rows = gdaui_raw_form_selector_get_selected_rows;
	iface->get_data_set = gdaui_raw_form_selector_get_data_set;
	iface->select_row = gdaui_raw_form_selector_select_row;
	iface->unselect_row = gdaui_raw_form_selector_unselect_row;
	iface->set_column_visible = gdaui_raw_form_selector_set_column_visible;
}

static void
basic_form_layout_changed (GdauiBasicForm *bform)
{
	GdauiRawForm *form = GDAUI_RAW_FORM (bform);
	iter_row_changed_cb (form->priv->iter, gda_data_model_iter_get_row (form->priv->iter), form);
}

static void
gdaui_raw_form_class_init (GdauiRawFormClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
	object_class->dispose = gdaui_raw_form_dispose;
	GDAUI_BASIC_FORM_CLASS (class)->layout_changed = basic_form_layout_changed;

	/* Properties */
        object_class->set_property = gdaui_raw_form_set_property;
        object_class->get_property = gdaui_raw_form_get_property;
	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model", _("Data to display"),
							      NULL, GDA_TYPE_DATA_MODEL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static gboolean
gdaui_raw_form_supports_action (GdauiDataProxy *iface, GdauiAction action)
{
	switch (action) {
	case GDAUI_ACTION_NEW_DATA:
	case GDAUI_ACTION_WRITE_MODIFIED_DATA:
	case GDAUI_ACTION_DELETE_SELECTED_DATA:
	case GDAUI_ACTION_UNDELETE_SELECTED_DATA:
	case GDAUI_ACTION_RESET_DATA:
	case GDAUI_ACTION_MOVE_FIRST_RECORD:
        case GDAUI_ACTION_MOVE_PREV_RECORD:
        case GDAUI_ACTION_MOVE_NEXT_RECORD:
        case GDAUI_ACTION_MOVE_LAST_RECORD:
		return TRUE;
	default:
		return FALSE;
	};
}

static void
form_holder_changed_cb (GdauiRawForm *form, G_GNUC_UNUSED gpointer data)
{
	if (form->priv->write_mode == GDAUI_DATA_PROXY_WRITE_ON_VALUE_CHANGE) {
		gint row;

		row = gda_data_model_iter_get_row (form->priv->iter);
		if (row >= 0) {
			/* write back the current row */
			if (gda_data_proxy_row_has_changed (form->priv->proxy, row)) {
				GError *error = NULL;
				if (!gda_data_proxy_apply_row_changes (form->priv->proxy, row, &error)) {
					_gdaui_utility_display_error ((GdauiDataProxy *) form, TRUE, error);
					if (error)
						g_error_free (error);
				}
			}
		}
	}
}

static void arrow_actions_real_do (GdauiRawForm *form, gint movement);

static void
gdaui_raw_form_perform_action (GdauiDataProxy *iface, GdauiAction action)
{
	GdauiRawForm *form = GDAUI_RAW_FORM (iface);

	switch (action) {
	case GDAUI_ACTION_NEW_DATA: {
		gint newrow;
		GError *error = NULL;
		GSList *list;

		if (form->priv->write_mode >= GDAUI_DATA_PROXY_WRITE_ON_ROW_CHANGE) {
			/* validate current row (forces calling iter_validate_set_cb()) */
			if (gda_data_model_iter_is_valid (form->priv->iter) &&
			    ! gda_set_is_valid (GDA_SET (form->priv->iter), NULL))
				return;
		}

		/* append a row in the proxy */
		g_signal_handlers_block_by_func (form, G_CALLBACK (form_holder_changed_cb), NULL);
		newrow = gda_data_model_append_row (GDA_DATA_MODEL (form->priv->proxy), &error);
		if (newrow == -1) {
			g_warning (_("Can't append row to data model: %s"),
				   error && error->message ? error->message : _("Unknown error"));
			g_error_free (error);
			g_signal_handlers_unblock_by_func (form, G_CALLBACK (form_holder_changed_cb), NULL);
			return;
		}

		if (!gda_data_model_iter_move_to_row (form->priv->iter, newrow)) {
			g_warning ("Can't set GdaDataModelIterator on new row");
			g_signal_handlers_unblock_by_func (form, G_CALLBACK (form_holder_changed_cb), NULL);
			return;
		}

		/* set parameters to their default values */
		for (list = GDA_SET (form->priv->iter)->holders; list; list = list->next) {
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
		}

		g_signal_handlers_unblock_by_func (form, G_CALLBACK (form_holder_changed_cb), NULL);
		form_holder_changed_cb (form, NULL);

		/* make the first entry grab the focus */
		if (form->priv->iter && GDA_SET (form->priv->iter)->holders)
			gdaui_basic_form_entry_grab_focus (GDAUI_BASIC_FORM (form),
							   (GdaHolder *)
							   GDA_SET (form->priv->iter)->holders->data);
		break;
	}
	case GDAUI_ACTION_WRITE_MODIFIED_DATA: {
		gint row;
		GError *error = NULL;
		gboolean allok = TRUE;
		gint mod1, mod2;

		mod1 = gda_data_proxy_get_n_modified_rows (form->priv->proxy);
		row = gda_data_model_iter_get_row (form->priv->iter);
		if (form->priv->write_mode >= GDAUI_DATA_PROXY_WRITE_ON_ROW_CHANGE) {
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
				_gdaui_utility_display_error ((GdauiDataProxy *) form, FALSE, error);
			else
				_gdaui_utility_display_error ((GdauiDataProxy *) form, TRUE, error);
			g_error_free (error);
		}

		/* get to a correct selected row */
		for (; (row >= 0) &&!gda_data_model_iter_move_to_row (form->priv->iter, row); row--);

		break;
	}
	case GDAUI_ACTION_DELETE_SELECTED_DATA: {
		gint row;
		row = gda_data_model_iter_get_row (form->priv->iter);
		g_return_if_fail (row >= 0);
		gda_data_proxy_delete (form->priv->proxy, row);
		
		if (form->priv->write_mode >= GDAUI_DATA_PROXY_WRITE_ON_ROW_CHANGE) {
			/* force the proxy to apply the current row deletion */
			gint newrow;
			
			newrow = gda_data_model_iter_get_row (form->priv->iter);
			if (row == newrow) {/* => row has been marked as delete but not yet really deleted */
				GError *error = NULL;
				if (!gda_data_proxy_apply_row_changes (form->priv->proxy, row, &error)) {
					_gdaui_utility_display_error ((GdauiDataProxy *) form, TRUE, error);
					if (error)
						g_error_free (error);
				}
			}
		}
		break;
	}

	case GDAUI_ACTION_UNDELETE_SELECTED_DATA: {
		gint row;
		
		row = gda_data_model_iter_get_row (form->priv->iter);
		g_return_if_fail (row >= 0);
		gda_data_proxy_undelete (form->priv->proxy, row);

		break;
	}
	case GDAUI_ACTION_RESET_DATA: {
		gda_data_proxy_cancel_all_changes (form->priv->proxy);
		gda_data_model_send_hint (GDA_DATA_MODEL (form->priv->proxy), GDA_DATA_MODEL_HINT_REFRESH, NULL);
		break;
	}
	case GDAUI_ACTION_MOVE_FIRST_RECORD:
		arrow_actions_real_do (form, -2);
		break;
        case GDAUI_ACTION_MOVE_PREV_RECORD:
		arrow_actions_real_do (form, -1);
		break;
        case GDAUI_ACTION_MOVE_NEXT_RECORD:
		arrow_actions_real_do (form, 1);
		break;
        case GDAUI_ACTION_MOVE_LAST_RECORD:
		arrow_actions_real_do (form, 2);
		break;
	default:
		break;
	}
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

static void
form_activated_cb (GdauiRawForm *form, G_GNUC_UNUSED gpointer data)
{
	if (form->priv->write_mode == GDAUI_DATA_PROXY_WRITE_ON_VALUE_ACTIVATED) {
		gint row;
		
		row = gda_data_model_iter_get_row (form->priv->iter);
		if (row >= 0) {
			/* write back the current row */
			if (gda_data_proxy_row_has_changed (form->priv->proxy, row)) {
				GError *error = NULL;
				if (!gda_data_proxy_apply_row_changes (form->priv->proxy, row, &error)) {
					gboolean discard;
					discard = _gdaui_utility_display_error_with_keep_or_discard_choice ((GdauiDataProxy *) form,
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
gdaui_raw_form_init (GdauiRawForm *wid)
{
	wid->priv = g_new0 (GdauiRawFormPriv, 1);

	wid->priv->data_model = NULL;
	wid->priv->proxy = NULL;
	wid->priv->iter = NULL;
	wid->priv->write_mode = GDAUI_DATA_PROXY_WRITE_ON_DEMAND;

	g_signal_connect (G_OBJECT (wid), "activated",
			  G_CALLBACK (form_activated_cb), NULL);
	g_signal_connect (G_OBJECT (wid), "holder-changed",
			  G_CALLBACK (form_holder_changed_cb), NULL);

	/* raw forms' default unknown color */
	gdaui_basic_form_set_unknown_color ((GdauiBasicForm*) wid, -1., -1., -1., -1.);
}

/**
 * gdaui_raw_form_new:
 * @model: (allow-none): a #GdaDataModel, or %NULL
 *
 * Creates a new #GdauiRawForm widget to display data in @model
 *
 * Returns: (transfer full): the new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_raw_form_new (GdaDataModel *model)
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_RAW_FORM, "model", model, NULL);

	return GTK_WIDGET (obj);
}

static void gdaui_raw_form_clean (GdauiRawForm *form);
static void
gdaui_raw_form_dispose (GObject *object)
{
	GdauiRawForm *form;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_RAW_FORM (object));
	form = GDAUI_RAW_FORM (object);

	if (form->priv) {
		gdaui_raw_form_clean (form);

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

        form = GDAUI_RAW_FORM (object);
        if (form->priv) {
                switch (param_id) {
		case PROP_MODEL: {
			GdaDataModel *model = (GdaDataModel*) g_value_get_object (value);

			if (model)
				g_return_if_fail (GDA_IS_DATA_MODEL (model));
			else
				return;

			if (form->priv->proxy) {
				/* data model has been changed */
				if (GDA_IS_DATA_PROXY (model)) {
					/* clean all */
					gdaui_raw_form_clean (form);
					g_assert (!form->priv->proxy);
				}
				else
					g_object_set (G_OBJECT (form->priv->proxy), "model", model, NULL);
			}

			if (!form->priv->proxy) {
				/* first time setting */
				if (GDA_IS_DATA_PROXY (model))
					form->priv->proxy = g_object_ref (G_OBJECT (model));
				else
					form->priv->proxy = GDA_DATA_PROXY (gda_data_proxy_new (model));
				form->priv->data_model = gda_data_proxy_get_proxied_model (form->priv->proxy);
				form->priv->iter = gda_data_model_create_iter (GDA_DATA_MODEL (form->priv->proxy));
				gda_data_model_iter_move_to_row (form->priv->iter, 0);

				g_signal_connect (form->priv->iter, "validate-set",
						  G_CALLBACK (iter_validate_set_cb), form);
				g_signal_connect (form->priv->iter, "row-changed",
						  G_CALLBACK (iter_row_changed_cb), form);

				g_signal_connect (G_OBJECT (form->priv->proxy), "row_inserted",
						  G_CALLBACK (proxy_row_inserted_or_removed_cb), form);
				g_signal_connect (G_OBJECT (form->priv->proxy), "row_removed",
						  G_CALLBACK (proxy_row_inserted_or_removed_cb), form);
				g_signal_connect (G_OBJECT (form->priv->proxy), "changed",
						  G_CALLBACK (proxy_changed_cb), form);
				g_signal_connect (G_OBJECT (form->priv->proxy), "reset",
						  G_CALLBACK (proxy_reset_cb), form);
				g_signal_connect (G_OBJECT (form->priv->proxy), "access-changed",
						  G_CALLBACK (proxy_access_changed_cb), form);

				g_object_set (object, "paramlist", form->priv->iter, NULL);

				/* we don't want chuncking */
				gda_data_proxy_set_sample_size (form->priv->proxy, 0);

				/* handle invalid iterators' GdaHolder */
				GSList *list;
				for (list = GDA_SET (form->priv->iter)->holders; list; list = list->next) {
					GtkWidget *entry;
					entry = gdaui_basic_form_get_entry_widget (GDAUI_BASIC_FORM (form),
										   (GdaHolder*) list->data);
					if (entry)
						gdaui_entry_shell_set_unknown ((GdauiEntryShell*) entry,
									       !gda_holder_is_valid ((GdaHolder*) list->data));
				}

				iter_row_changed_cb (form->priv->iter,
						     gda_data_model_iter_get_row (form->priv->iter), form);

				/* actions */
				if (gda_data_proxy_is_read_only (form->priv->proxy))
					g_object_set ((GObject*) form, "show-actions", FALSE, NULL);

				/* data display update */
				proxy_changed_cb (form->priv->proxy, form);

				gdaui_raw_form_widget_set_write_mode ((GdauiDataProxy *) form,
								      form->priv->write_mode);

				g_signal_emit_by_name (object, "proxy-changed", form->priv->proxy);
			}
			break;
		}
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
			g_value_set_object(value, form->priv->data_model);
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

	if ((form->priv->write_mode >= GDAUI_DATA_PROXY_WRITE_ON_ROW_CHANGE) &&
	    /* write back the current row */
	    gda_data_proxy_row_has_changed (form->priv->proxy, row) &&
	    !gda_data_proxy_apply_row_changes (form->priv->proxy, row, &error)) {
		if (_gdaui_utility_display_error_with_keep_or_discard_choice ((GdauiDataProxy *) form,
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
	gdaui_basic_form_set_as_reference (GDAUI_BASIC_FORM (form));

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
	g_signal_emit_by_name (G_OBJECT (form), "selection-changed");
}

static void
proxy_changed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, GdauiRawForm *form)
{
	/* TO remove ? */
	gtk_widget_set_sensitive (GTK_WIDGET (form),
				  gda_data_model_get_n_rows (GDA_DATA_MODEL (form->priv->proxy)) == 0 ? FALSE : TRUE);
}

static gboolean
model_reset_was_soft (GdauiRawForm *form, GdaDataModel *new_model)
{
	GdaDataModelIter *iter;
	gboolean retval = FALSE;

	if (!new_model)
		return FALSE;
	else if (new_model == (GdaDataModel*) form->priv->proxy)
		return TRUE;
	else if (!form->priv->iter)
		return FALSE;

	iter = gda_data_model_create_iter (new_model);
	retval = ! _gdaui_utility_iter_differ (form->priv->iter, iter);
	g_object_unref (iter);
	return retval;
}

static void
gdaui_raw_form_clean (GdauiRawForm *form)
{
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
		g_signal_handlers_disconnect_by_func (G_OBJECT (form->priv->proxy),
						      G_CALLBACK (proxy_access_changed_cb), form);
		g_object_unref (form->priv->proxy);
		form->priv->proxy = NULL;
	}
}

static void
proxy_reset_cb (GdaDataProxy *proxy, GdauiRawForm *form)
{
	gint iter_row;
	gboolean reset_soft;

	iter_row = gda_data_model_iter_get_row (form->priv->iter);
	reset_soft = model_reset_was_soft (form, gda_data_proxy_get_proxied_model (form->priv->proxy));

	if (iter_row >= 0)
		gda_data_model_iter_move_to_row (form->priv->iter, iter_row);
	else
		gda_data_model_iter_move_to_row (form->priv->iter, 0);

	form->priv->data_model = gda_data_proxy_get_proxied_model (form->priv->proxy);
	iter_row_changed_cb (form->priv->iter, gda_data_model_iter_get_row (form->priv->iter), form);

	if (! reset_soft)
		g_signal_emit_by_name (form, "proxy-changed", form->priv->proxy);
}

static void
proxy_access_changed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, GdauiRawForm *form)
{
	iter_row_changed_cb (form->priv->iter, gda_data_model_iter_get_row (form->priv->iter), form);
}

static void
proxy_row_inserted_or_removed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, gint row, GdauiRawForm *form)
{
	if (gda_data_model_get_n_rows (GDA_DATA_MODEL (form->priv->proxy)) != 0)
		if (gda_data_model_iter_get_row (form->priv->iter) == -1)
			gda_data_model_iter_move_to_row (form->priv->iter, row > 0 ? row - 1 : 0);
}


/*
 * GdauiDataProxy interface implementation
 */
static GdaDataProxy *
gdaui_raw_form_get_proxy (GdauiDataProxy *iface)
{
	GdauiRawForm *form;

	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), NULL);
	form = GDAUI_RAW_FORM (iface);
	g_return_val_if_fail (form->priv, NULL);

	return form->priv->proxy;
}

void
gdaui_raw_form_set_column_editable (GdauiDataProxy *iface, G_GNUC_UNUSED gint column, G_GNUC_UNUSED gboolean editable)
{
	GdauiRawForm *form;

	g_return_if_fail (GDAUI_IS_RAW_FORM (iface));
	form = GDAUI_RAW_FORM (iface);
	g_return_if_fail (form->priv);

	TO_IMPLEMENT;
	/* What needs to be done:
	 * - create a gdaui_basic_form_set_entry_editable() function for GdauiBasicForm, and call it
	 * - in the GdauiDataEntry, add a GDA_VALUE_ATTR_EDITABLE property which defaults to TRUE.
	 * - imtplement the gdaui_basic_form_set_entry_editable() in the same way as gdaui_basic_form_set_entries_to_default()
	 *   by setting that new property
	 * - implement the new property in GdauiEntryWrapper and GdauiEntryCombo.
	 */
}

static void
gdaui_raw_form_show_column_actions (GdauiDataProxy *iface, G_GNUC_UNUSED gint column, gboolean show_actions)
{
	GdauiRawForm *form;

	g_return_if_fail (GDAUI_IS_RAW_FORM (iface));
	form = GDAUI_RAW_FORM (iface);
	g_return_if_fail (form->priv);

	/* REM: don't take care of the @column argument */
	g_object_set ((GObject*) form, "show-actions", show_actions, NULL);
}

static gboolean
gdaui_raw_form_widget_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode)
{
	GdauiRawForm *form;

	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), FALSE);
	form = GDAUI_RAW_FORM (iface);
	g_return_val_if_fail (form->priv, FALSE);

	form->priv->write_mode = mode;
	return TRUE;
}

static GdauiDataProxyWriteMode
gdaui_raw_form_widget_get_write_mode (GdauiDataProxy *iface)
{
	GdauiRawForm *form;

	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), GDAUI_DATA_PROXY_WRITE_ON_DEMAND);
	form = GDAUI_RAW_FORM (iface);
	g_return_val_if_fail (form->priv, GDAUI_DATA_PROXY_WRITE_ON_DEMAND);

	return form->priv->write_mode;
}

/* GdauiDataSelector interface */
static GdaDataModel *
gdaui_raw_form_selector_get_model (GdauiDataSelector *iface)
{
	GdauiRawForm *form;

	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), NULL);
	form = GDAUI_RAW_FORM (iface);

	return GDA_DATA_MODEL (form->priv->proxy);
}

static void
gdaui_raw_form_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model)
{
	GdauiRawForm *form;

	g_return_if_fail (GDAUI_IS_RAW_FORM (iface));
	form = GDAUI_RAW_FORM (iface);

	g_object_set (form, "model", model, NULL);
}

static GArray *
gdaui_raw_form_selector_get_selected_rows (GdauiDataSelector *iface)
{
	GdauiRawForm *form;

	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), NULL);
	form = GDAUI_RAW_FORM (iface);

	if (gda_data_model_iter_is_valid (form->priv->iter)) {
		GArray *array;
		gint row;
		array = g_array_new (FALSE, FALSE, sizeof (gint));
		row = gda_data_model_iter_get_row (form->priv->iter);
		g_array_append_val (array, row);
		return array;
	}
	else
		return NULL;
}

static GdaDataModelIter *
gdaui_raw_form_selector_get_data_set (GdauiDataSelector *iface)
{
	GdauiRawForm *form;

	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), NULL);
	form = GDAUI_RAW_FORM (iface);

	return form->priv->iter;
}

static gboolean
gdaui_raw_form_selector_select_row (GdauiDataSelector *iface, gint row)
{
	GdauiRawForm *form;

	g_return_val_if_fail (GDAUI_IS_RAW_FORM (iface), FALSE);
	form = (GdauiRawForm*) iface;

	return gda_data_model_iter_move_to_row (form->priv->iter, row);
}

static void
gdaui_raw_form_selector_unselect_row (G_GNUC_UNUSED GdauiDataSelector *iface, G_GNUC_UNUSED gint row)
{
	g_warning ("%s() method not supported\n", __FUNCTION__);
}

static void
gdaui_raw_form_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible)
{
	GdauiRawForm *form;
	GdaHolder *param;

	g_return_if_fail (GDAUI_IS_RAW_FORM (iface));
	form = GDAUI_RAW_FORM (iface);

	param = gda_data_model_iter_get_holder_for_field (form->priv->iter, column);
	g_return_if_fail (param);
	gdaui_basic_form_entry_set_visible (GDAUI_BASIC_FORM (form), param, visible);
}
