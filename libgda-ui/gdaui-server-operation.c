/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include "gdaui-server-operation.h"
#include "gdaui-basic-form.h"
#include "gdaui-raw-grid.h"
#include "gdaui-raw-form.h"
#include "gdaui-data-store.h"
#include "gdaui-data-proxy.h"
#include "gdaui-data-selector.h"
#include "gdaui-data-proxy-info.h"

static void gdaui_server_operation_class_init (GdauiServerOperationClass *class);
static void gdaui_server_operation_init (GdauiServerOperation *wid);
static void gdaui_server_operation_dispose (GObject *object);

static void gdaui_server_operation_set_property (GObject *object,
						 guint param_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void gdaui_server_operation_get_property (GObject *object,
						 guint param_id,
						 GValue *value,
						 GParamSpec *pspec);

static void gdaui_server_operation_fill (GdauiServerOperation *form);

/* properties */
enum {
	PROP_0,
	PROP_SERVER_OP_OBJ,
	PROP_OPT_HEADER
};

typedef struct _WidgetData {
	struct _WidgetData    *parent;
	gchar                 *path_name; /* NULL if for SEQUENCE_ITEM */
	GSList                *children;
	GtkWidget             *widget;
} WidgetData;
#define WIDGET_DATA(x) ((WidgetData*)(x))

struct _GdauiServerOperationPriv
{
	GdaServerOperation     *op;
	GSList                 *widget_data; /* list of WidgetData structures */
#ifdef HAVE_LIBGLADE
	GladeXML               *glade;
#endif
	gboolean                opt_header;
};

WidgetData *widget_data_new (WidgetData *parent, const gchar *path_name);
void        widget_data_free (WidgetData *wd);
WidgetData *widget_data_find (GdauiServerOperation *form, const gchar *path);

WidgetData *
widget_data_new (WidgetData *parent, const gchar *path_name)
{
	WidgetData *wd;

	wd = g_new0 (WidgetData, 1);
	wd->parent = parent;
	if (path_name)
		wd->path_name = g_strdup (path_name);
	if (parent)
		parent->children = g_slist_append (parent->children, wd);
	return wd;
}

void
widget_data_free (WidgetData *wd)
{
	g_free (wd->path_name);
	g_slist_foreach (wd->children, (GFunc) widget_data_free, NULL);
	g_slist_free (wd->children);
	g_free (wd);
}

WidgetData *
widget_data_find (GdauiServerOperation *form, const gchar *path)
{
	gchar **array;
	gint i, index;
	WidgetData *wd = NULL;
	GSList *list;

	if (!path)
		return NULL;
	g_assert (*path == '/');

	array = g_strsplit (path, "/", 0);
	if (!array [1]) {
		g_strfreev (array);
		return NULL;
	}

	list = form->priv->widget_data;
	while (list && !wd) {
		if (WIDGET_DATA (list->data)->path_name &&
		    !strcmp (WIDGET_DATA (list->data)->path_name, array[1]))
			wd = WIDGET_DATA (list->data);
		list = list->next;
	}

	i = 2;
	while (array[i] && wd) {
		char *end;
		list = wd->children;

		index = strtol (array[i], &end, 10);
		if (end && *end)
			index = -1; /* could not convert array[i] to an int */

		if ((index >= 0) && wd->children && !WIDGET_DATA (wd->children->data)->path_name)
			wd = g_slist_nth_data (wd->children, index);
		else {
			wd = NULL;
			while (list && !wd) {
				if (WIDGET_DATA (list->data)->path_name &&
				    !strcmp (WIDGET_DATA (list->data)->path_name, array[i]))
					wd = WIDGET_DATA (list->data);
				list = list->next;
			}
		}
		i++;
	}

	/*g_print ("## %s (%s): %p\n", __FUNCTION__, path, wd);*/
	g_strfreev (array);
	return wd;
}

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gdaui_server_operation_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiServerOperationClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_server_operation_class_init,
			NULL,
			NULL,
			sizeof (GdauiServerOperation),
			0,
			(GInstanceInitFunc) gdaui_server_operation_init,
			0
		};

		type = g_type_register_static (GTK_TYPE_BOX, "GdauiServerOperation", &info, 0);
	}

	return type;
}

static void
gdaui_server_operation_class_init (GdauiServerOperationClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_server_operation_dispose;

	/* Properties */
        object_class->set_property = gdaui_server_operation_set_property;
        object_class->get_property = gdaui_server_operation_get_property;
	g_object_class_install_property (object_class, PROP_SERVER_OP_OBJ,
					 g_param_spec_object ("server-operation",
							      _("The specification of the operation to implement"),
							      NULL, GDA_TYPE_SERVER_OPERATION,
							      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_OPT_HEADER,
					 g_param_spec_boolean ("hide-single-header",
							       _("Request section header to be hidden if there is only one section"),
							       NULL, FALSE, G_PARAM_CONSTRUCT | G_PARAM_READABLE |
							       G_PARAM_WRITABLE));
}

static void
gdaui_server_operation_init (GdauiServerOperation * wid)
{
	wid->priv = g_new0 (GdauiServerOperationPriv, 1);
	wid->priv->op = NULL;
	wid->priv->widget_data = NULL;
#ifdef HAVE_LIBGLADE
	wid->priv->glade = NULL;
#endif
	wid->priv->opt_header = FALSE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (wid), GTK_ORIENTATION_VERTICAL);
}


/**
 * gdaui_server_operation_new
 * @op: a #GdaServerOperation structure
 *
 * Creates a new #GdauiServerOperation widget using all the parameters provided in @paramlist.
 *
 * The global layout is rendered using a table (a #GtkTable), and an entry is created for each
 * node of @paramlist.
 *
 * Returns: the new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_server_operation_new (GdaServerOperation *op)
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_SERVER_OPERATION, "server-operation", op, NULL);

	return (GtkWidget *) obj;
}

static void sequence_item_added_cb (GdaServerOperation *op, const gchar *seq_path, gint item_index, GdauiServerOperation *form);
static void sequence_item_remove_cb (GdaServerOperation *op, const gchar *seq_path, gint item_index, GdauiServerOperation *form);

static void
gdaui_server_operation_dispose (GObject *object)
{
	GdauiServerOperation *form;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_SERVER_OPERATION (object));
	form = GDAUI_SERVER_OPERATION (object);

	if (form->priv) {
		/* paramlist */
		if (form->priv->op) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (form->priv->op),
							      G_CALLBACK (sequence_item_added_cb), form);
			g_signal_handlers_disconnect_by_func (G_OBJECT (form->priv->op),
							      G_CALLBACK (sequence_item_remove_cb), form);
			g_object_unref (form->priv->op);
		}

		if (form->priv->widget_data) {
			g_slist_foreach (form->priv->widget_data, (GFunc) widget_data_free, NULL);
			g_slist_free (form->priv->widget_data);
			form->priv->widget_data = NULL;
		}

#ifdef HAVE_LIBGLADE
		if (form->priv->glade)
			g_object_unref (form->priv->glade);
#endif

		/* the private area itself */
		g_free (form->priv);
		form->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}


static void
gdaui_server_operation_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	GdauiServerOperation *form;

        form = GDAUI_SERVER_OPERATION (object);
        if (form->priv) {
                switch (param_id) {
		case PROP_SERVER_OP_OBJ:
			if (form->priv->op) {
				TO_IMPLEMENT;
				g_assert_not_reached ();
			}

			form->priv->op = GDA_SERVER_OPERATION(g_value_get_object (value));
			if (form->priv->op) {
				g_return_if_fail (GDA_IS_SERVER_OPERATION (form->priv->op));

				g_object_ref (form->priv->op);

				gdaui_server_operation_fill (form);
				g_signal_connect (G_OBJECT (form->priv->op), "sequence-item-added",
						  G_CALLBACK (sequence_item_added_cb), form);
				g_signal_connect (G_OBJECT (form->priv->op), "sequence-item-remove",
						  G_CALLBACK (sequence_item_remove_cb), form);
			}
			break;
		case PROP_OPT_HEADER:
			form->priv->opt_header = g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gdaui_server_operation_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	GdauiServerOperation *form;

        form = GDAUI_SERVER_OPERATION (object);
        if (form->priv) {
                switch (param_id) {
		case PROP_SERVER_OP_OBJ:
			g_value_set_object (value, form->priv->op);
			break;
		case PROP_OPT_HEADER:
			g_value_set_boolean (value, form->priv->opt_header);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

/*
 * create the entries in the widget
 */

static GtkWidget *fill_create_widget (GdauiServerOperation *form, const gchar *path,
				      gchar **section_str, GSList **label_widgets);
static void seq_add_item (GtkButton *button, GdauiServerOperation *form);
static void seq_del_item (GtkButton *button, GdauiServerOperation *form);


/*
 * @path is like "/SEQ", DOES NOT contain the index of the item to add, which is also in @index
 */
static void
sequence_grid_attach_widget (GdauiServerOperation *form, GtkWidget *grid, GtkWidget *wid,
			     const gchar *path, gint index)
{
	GtkWidget *image;
	guint min, size;

	min = gda_server_operation_get_sequence_min_size (form->priv->op, path);
	size = gda_server_operation_get_sequence_size (form->priv->op, path);

	/* new widget */
	gtk_grid_attach (GTK_GRID (grid), wid, 0, index, 1, 1);
	gtk_widget_show (wid);

	/* "-" button */
	image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
	wid = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (wid), image);
	gtk_grid_attach (GTK_GRID (grid), wid, 1, index, 1, 1);
	gtk_widget_show (wid);
	g_object_set_data_full (G_OBJECT (wid), "_seq_path", g_strdup (path), g_free);
	g_object_set_data (G_OBJECT (wid), "_index", GINT_TO_POINTER (index+1));
	g_signal_connect (G_OBJECT (wid), "clicked",
			  G_CALLBACK (seq_del_item), form);
	if (size <= min)
		gtk_widget_set_sensitive (wid, FALSE);
}

static GtkWidget *create_table_fields_array_create_widget (GdauiServerOperation *form, const gchar *path,
							   gchar **section_str, GSList **label_widgets);
static GtkWidget *
fill_create_widget (GdauiServerOperation *form, const gchar *path, gchar **section_str, GSList **label_widgets)
{
	GdaServerOperationNode *info_node;
	GtkWidget *plwid = NULL;

	info_node = gda_server_operation_get_node_info (form->priv->op, path);
	g_assert (info_node);

	if (label_widgets)
		*label_widgets = NULL;
	if (section_str)
		*section_str = NULL;

	/* very custom widget rendering goes here */
	if ((gda_server_operation_get_op_type (form->priv->op) == GDA_SERVER_OPERATION_CREATE_TABLE) &&
	    !strcmp (path, "/FIELDS_A"))
		return create_table_fields_array_create_widget (form, path, section_str, label_widgets);

	/* generic widget rendering */
	switch (info_node->type) {
	case GDA_SERVER_OPERATION_NODE_PARAMLIST: {
		GdaSet *plist;

		plist = info_node->plist;
		plwid = gdaui_basic_form_new (plist);
		g_object_set ((GObject*) plwid, "show-actions", FALSE, NULL);

		if (section_str) {
			const gchar *name;
			name = g_object_get_data (G_OBJECT (plist), "name");
			if (name && *name)
				*section_str = g_strdup_printf ("<b>%s:</b>", name);
			else
				*section_str = NULL;
		}
		if (label_widgets) {
			GSList *params;

			params = plist->holders;
			while (params) {
				GtkWidget *label_entry;

				label_entry = gdaui_basic_form_get_label_widget (GDAUI_BASIC_FORM (plwid),
										 GDA_HOLDER (params->data));
				if (label_entry && !g_slist_find (*label_widgets, label_entry))
					*label_widgets = g_slist_prepend (*label_widgets, label_entry);
				params = params->next;
			}
			*label_widgets = g_slist_reverse (*label_widgets);
		}
		break;
	}
	case GDA_SERVER_OPERATION_NODE_DATA_MODEL: {
		GdaDataModel *model;
		GtkWidget *winfo;
		GtkWidget *box, *grid;

		plwid = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (plwid),
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (plwid),
						     GTK_SHADOW_NONE);

		model = info_node->model;
		grid = gdaui_raw_grid_new (model);
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (plwid), grid);
		gtk_viewport_set_shadow_type (GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (plwid))),
					      GTK_SHADOW_NONE);
		gdaui_data_proxy_set_write_mode (GDAUI_DATA_PROXY (grid),
						 GDAUI_DATA_PROXY_WRITE_ON_ROW_CHANGE);
		gtk_widget_show (grid);

		g_object_set (G_OBJECT (grid), "info-cell-visible", FALSE, NULL);

		winfo = gdaui_data_proxy_info_new (GDAUI_DATA_PROXY (grid),
						   GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS);

		box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_box_pack_start (GTK_BOX (box), plwid, TRUE, TRUE, 0);
		gtk_widget_show (plwid);

		gtk_box_pack_start (GTK_BOX (box), winfo, FALSE, TRUE, 0);
		gtk_widget_show (winfo);

		plwid = box;

		if (section_str)
			*section_str = g_strdup_printf ("<b>%s:</b>",
							(gchar*) g_object_get_data (G_OBJECT (model), "name"));

		if (label_widgets) {
			GtkWidget *label_entry;
			gchar *str;

			if (info_node->status == GDA_SERVER_OPERATION_STATUS_REQUIRED) {
				str = g_strdup_printf ("<b>%s:</b>",
						       (gchar*) g_object_get_data (G_OBJECT (model), "name"));
				label_entry = gtk_label_new (str);
				gtk_label_set_use_markup (GTK_LABEL (label_entry), TRUE);
			}
			else {
				str = g_strdup_printf ("%s:", (gchar*) g_object_get_data (G_OBJECT (model), "name"));
				label_entry = gtk_label_new (str);
			}
			g_free (str);
			gtk_misc_set_alignment (GTK_MISC (label_entry), 0., 0.);

			gtk_widget_show (label_entry);
			str = (gchar *) g_object_get_data (G_OBJECT (model), "descr");
			if (str && *str)
				gtk_widget_set_tooltip_text (label_entry, str);

			*label_widgets = g_slist_prepend (*label_widgets, label_entry);
		}

		gtk_widget_set_vexpand (plwid, TRUE);
		break;
	}
	case GDA_SERVER_OPERATION_NODE_PARAM: {
		GdaSet *plist;
		GdaHolder *param;
		GSList *list;

		param = info_node->param;
		list = g_slist_append (NULL, param);
		plist = gda_set_new (list);
		g_slist_free (list);
		plwid = gdaui_basic_form_new (plist);
		g_object_set ((GObject*) plwid, "show-actions", FALSE, NULL);
		/* we don't need plist anymore */
		g_object_unref (plist);

		if (section_str)
			*section_str = g_strdup_printf ("<b>%s:</b>",
							(gchar*) g_object_get_data (G_OBJECT (param), "name"));
		if (label_widgets) {
			GtkWidget *label_entry;

			label_entry = gdaui_basic_form_get_label_widget (GDAUI_BASIC_FORM (plwid), param);
			*label_widgets = g_slist_prepend (*label_widgets, label_entry);
		}
		break;
	}
	case GDA_SERVER_OPERATION_NODE_SEQUENCE: {
		guint n, size;
		GtkWidget *grid, *wid, *image;
		WidgetData *wdp, *wd;
		gchar *parent_path = NULL, *path_name = NULL;
		guint max;

		max = gda_server_operation_get_sequence_max_size (form->priv->op, path);
		if (section_str) {
			const gchar *seq_name;
			seq_name = gda_server_operation_get_sequence_name (form->priv->op, path);
			*section_str = g_strdup_printf ("<b>%s:</b>", seq_name);
		}

		plwid = gtk_scrolled_window_new (NULL, NULL);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (plwid),
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (plwid),
						     GTK_SHADOW_NONE);

		size = gda_server_operation_get_sequence_size (form->priv->op, path);
		grid = gtk_grid_new ();
		gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (plwid), grid);
		gtk_viewport_set_shadow_type (GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (plwid))),
					      GTK_SHADOW_NONE);
		gtk_widget_show (grid);

		parent_path = gda_server_operation_get_node_parent (form->priv->op, path);
		path_name = gda_server_operation_get_node_path_portion (form->priv->op, path);
		wdp = widget_data_find (form, parent_path);
		wd = widget_data_new (wdp, path_name);
		wd->widget = grid;
		if (! wdp)
			form->priv->widget_data = g_slist_append (form->priv->widget_data, wd);
		g_free (parent_path);
		g_free (path_name);

		/* existing entries */
		for (n = 0; n < size; n++) {
			GtkWidget *wid;
			gchar *str;

			str = g_strdup_printf ("%s/%d", path, n);
			wid = fill_create_widget (form, str, NULL, NULL);
			sequence_grid_attach_widget (form, grid, wid, path, n);
			g_free (str);
		}

		if (size < max) {
			/* last row is for new entries */
			wid = gtk_label_new (_("Add"));
			gtk_misc_set_alignment (GTK_MISC (wid), .0, -1);
			gtk_grid_attach (GTK_GRID (grid), wid, 0, size, 1, 1);
			gtk_widget_show (wid);

			image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
			wid = gtk_button_new ();
			gtk_button_set_image (GTK_BUTTON (wid), image);
			gtk_grid_attach (GTK_GRID (grid), wid, 1, size, 1, 1);
			gtk_widget_show (wid);

			g_signal_connect (G_OBJECT (wid), "clicked",
					  G_CALLBACK (seq_add_item), form);
			g_object_set_data_full (G_OBJECT (wid), "_seq_path", g_strdup (path), g_free);
		}

		gtk_widget_set_vexpand (plwid, TRUE);
		break;
	}
	case GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM: {
		gchar **node_names;
		gint size;
		gchar *parent_path;
		WidgetData *wdp, *wdi;

		node_names = gda_server_operation_get_sequence_item_names (form->priv->op, path);
		size = g_strv_length (node_names);
		if (size > 1) {
			GtkWidget *grid;
			gint i, tab_index;

			grid = gtk_grid_new ();
			for (i = 0, tab_index = 0; i < size; i++) {
				GtkWidget *wid;
				GSList *lab_list, *list;
				gint nb_labels = 0;

				wid = fill_create_widget (form, node_names[i], NULL, &lab_list);
				for (list = lab_list; list; list = list->next) {
					GtkWidget *label_entry = (GtkWidget *) list->data;
					GtkWidget *parent;

					if (label_entry) {
						parent = gtk_widget_get_parent (label_entry);
						if (parent) {
							g_object_ref (label_entry);
							gtk_container_remove (GTK_CONTAINER (parent), label_entry);
						}
						gtk_grid_attach (GTK_GRID (grid), label_entry,
								 0, tab_index, 1, 1);
						if (parent)
							g_object_unref (label_entry);
					}
					nb_labels++;
					tab_index++;
				}
				g_slist_free (lab_list);

				if (nb_labels > 0)
					gtk_grid_attach (GTK_GRID (grid), wid, 1,
							 tab_index - nb_labels, 1, nb_labels);
				else {
					gtk_grid_attach (GTK_GRID (grid), wid, 1, tab_index, 1, 1);
					tab_index += 1;
				}
				gtk_widget_show (wid);
			}
			plwid = grid;
		}
		else
			plwid = fill_create_widget (form, node_names[0], NULL, NULL);

		parent_path = gda_server_operation_get_node_parent (form->priv->op, path);
		wdp = widget_data_find (form, parent_path);
		g_assert (wdp);
		wdi = widget_data_new (wdp, NULL);
		wdi->widget = plwid;

		g_free (parent_path);
		g_strfreev (node_names);
		break;
	}
	default:
		g_assert_not_reached ();
		break;
	}

	return plwid;
}

static void
gdaui_server_operation_fill (GdauiServerOperation *form)
{
	gint i;
	gchar **topnodes;
#ifdef HAVE_LIBGLADE
	gchar *glade_file;
#endif

	/* parameters list management */
	if (!form->priv->op)
		/* nothing to do */
		return;

	/* load Glade file for specific GUI if it exists */
#ifdef HAVE_LIBGLADE
	glade_file = gdaui_gbr_get_data_dir_path ("server_operation.glade");
	form->priv->glade = glade_xml_new (glade_file,
					   gda_server_operation_op_type_to_string (gda_server_operation_get_op_type (form->priv->op)),
					   NULL);
	g_free (glade_file);
	if (form->priv->glade) {
		GtkWidget *mainw;
		mainw = glade_xml_get_widget (form->priv->glade,
					      gda_server_operation_op_type_to_string (gda_server_operation_get_op_type (form->priv->op)));
		if (mainw) {
			gtk_box_pack_start (GTK_BOX (form), mainw, TRUE, TRUE, 0);
			gtk_widget_show (mainw);
		}
		else {
			g_object_unref (form->priv->glade);
			form->priv->glade = NULL;
		}
	}
#endif

	/* user visible widgets */
	topnodes = gda_server_operation_get_root_nodes (form->priv->op);
	i = 0;
	while (topnodes[i]) {
		GtkWidget *plwid;
		gchar *section_str;
		GtkWidget *container = NULL;

#ifdef HAVE_LIBGLADE
		if (form->priv->glade) {
			container = glade_xml_get_widget (form->priv->glade, topnodes[i]);
			if (!container) {
				i++;
				continue;
			}
		}
#endif
		if (!container)
			container = (GtkWidget *) form;

		plwid = fill_create_widget (form, topnodes[i], &section_str, NULL);
		if (plwid) {
			GdaServerOperationNodeStatus status;
			GtkWidget *label = NULL, *hbox = NULL;

			if (! (form->priv->opt_header && (g_strv_length (topnodes) == 1)) && section_str) {
				GtkWidget *lab;
				label = gtk_label_new ("");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
				gtk_label_set_markup (GTK_LABEL (label), section_str);
				g_free (section_str);

				hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
				gtk_widget_show (hbox);
				lab = gtk_label_new ("    ");
				gtk_box_pack_start (GTK_BOX (hbox), lab, FALSE, FALSE, 0);
				gtk_widget_show (lab);

				gtk_box_pack_start (GTK_BOX (hbox), plwid, TRUE, TRUE, 0);
				gtk_widget_show (plwid);
			}
			else
				gtk_widget_show (plwid);


			gda_server_operation_get_node_type (form->priv->op, topnodes[i], &status);
			switch (status) {
			case GDA_SERVER_OPERATION_STATUS_OPTIONAL: {
				GtkWidget *exp;
				exp = gtk_expander_new ("");
				if (!label) {
					gchar *str;
					label = gtk_label_new ("");
					gtk_widget_show (label);
					gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
					str = g_strdup_printf ("<b>%s:</b>", _("Options"));
					gtk_label_set_markup (GTK_LABEL (label), str);
					g_free (str);
				}

				gtk_expander_set_label_widget (GTK_EXPANDER (exp), label);
				gtk_box_pack_start (GTK_BOX (container), exp, TRUE, TRUE, 5);
				if (hbox)
					gtk_container_add (GTK_CONTAINER (exp), hbox);
				else
					gtk_container_add (GTK_CONTAINER (exp), plwid);
				gtk_widget_show (exp);
				break;
			}
			case GDA_SERVER_OPERATION_STATUS_REQUIRED: {
				gboolean expand;
				expand = gtk_widget_get_vexpand (plwid);

				if (label)
					gtk_box_pack_start (GTK_BOX (container), label, FALSE, TRUE, 5);
				if (hbox)
					gtk_box_pack_start (GTK_BOX (container), hbox, expand, TRUE, 0);
				else
					gtk_box_pack_start (GTK_BOX (container), plwid, expand, TRUE, 0);
				break;
			}
			default:
				break;
			}
		}

		i++;
	}

	/* destroying unused widgets in the Glade description */
#ifdef HAVE_LIBGLADE
	if (form->priv->glade) {
		GList *widgets, *list;

		widgets = glade_xml_get_widget_prefix (form->priv->glade, "/");
		for (list = widgets; list; list = list->next) {
			const gchar *name;

			name = glade_get_widget_name ((GtkWidget *) (list->data));
			if (!gda_server_operation_get_node_info (form->priv->op, name)) {
				GtkWidget *parent;

				/* dirty hack to remove a notebook page */
				parent = gtk_widget_get_parent ((GtkWidget *) (list->data));
				if (GTK_IS_VIEWPORT (parent))
					parent = gtk_widget_get_parent (parent);
				if (GTK_IS_SCROLLED_WINDOW (parent))
					parent = gtk_widget_get_parent (parent);
				if (GTK_IS_NOTEBOOK (parent)) {
					gint pageno;

					pageno = gtk_notebook_page_num (GTK_NOTEBOOK (parent),
									(GtkWidget *) (list->data));
					gtk_notebook_remove_page (GTK_NOTEBOOK (parent), pageno);
				}
				else
					gtk_widget_destroy ((GtkWidget *) (list->data));
			}
		}
		g_list_free (widgets);
	}
#endif

	g_strfreev (topnodes);

}

/*
 * For sequences: adding an item by clicking on the "+" button
 */
static void
seq_add_item (GtkButton *button, GdauiServerOperation *form)
{
	gchar *path;

	path = g_object_get_data (G_OBJECT (button), "_seq_path");
	gda_server_operation_add_item_to_sequence (form->priv->op, path);
}

/*
 * For sequences: removing an item by clicking on the "-" button
 */
static void
seq_del_item (GtkButton *button, GdauiServerOperation *form)
{
	gchar *seq_path, *item_path;
	gint index;

	seq_path = g_object_get_data (G_OBJECT (button), "_seq_path");
	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "_index")) - 1;
	g_assert (index >= 0);
	item_path = g_strdup_printf ("%s/%d", seq_path, index);
	gda_server_operation_del_item_from_sequence (form->priv->op, item_path);
	g_free (item_path);
}

/*
 * For sequences: treating the "sequence-item-added" signal
 */
struct MoveChild {
	GtkWidget *widget;
	guint16    top_attach;
};

static void
sequence_item_added_cb (GdaServerOperation *op, const gchar *seq_path, gint item_index, GdauiServerOperation *form)
{
	GtkWidget *grid;
	GList *children, *list, *to_move = NULL;
	GtkWidget *wid;
	gchar *str;
	WidgetData *wd;
	guint max, min, size;

	max = gda_server_operation_get_sequence_max_size (op, seq_path);
	min = gda_server_operation_get_sequence_min_size (op, seq_path);
	size = gda_server_operation_get_sequence_size (op, seq_path);

	wd = widget_data_find (form, seq_path);
	g_assert (wd);
	grid = wd->widget;
	g_assert (grid);

	/* move children DOWN if necessary */
	children = gtk_container_get_children (GTK_CONTAINER (grid));
	for (list = children; list; list = list->next) {
		GtkWidget *child = GTK_WIDGET (list->data);

		if (child) {
			guint top_attach, left_attach;
			gtk_container_child_get (GTK_CONTAINER (grid), child,
						 "top-attach", &top_attach,
						 "left-attach", &left_attach, NULL);
			/* ADD/REMOVE button sensitivity */
			if (left_attach == 1) {
				if (top_attach == size-1)
					gtk_widget_set_sensitive (child, (size < max) ? TRUE : FALSE);
				else
					gtk_widget_set_sensitive (child, (size > min) ? TRUE : FALSE);
			}

			/* move children DOWN if necessary and change the "_index" property */
			if (top_attach >= (guint)item_index) {
				struct MoveChild *mc;
				gint index;

				mc = g_new (struct MoveChild, 1);
				mc->widget = child;
				mc->top_attach = top_attach + 1;
				to_move = g_list_append (to_move, mc);

				index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "_index"));
				if (index > 0)
					g_object_set_data (G_OBJECT (child), "_index",
							   GINT_TO_POINTER (index + 1));
			}
		}
	}
	g_list_free (children);


	for (list = to_move; list; list = list->next) {
		struct MoveChild *mc;

		mc = (struct MoveChild *) (list->data);
		gtk_container_child_set (GTK_CONTAINER (grid), mc->widget,
					 "top-attach", mc->top_attach,
					 "height", 1, NULL);
		g_free (list->data);
	}
	g_list_free (to_move);

	/* add widget corresponding to the new sequence item */
	str = g_strdup_printf ("%s/%d", seq_path, item_index);
	wid = fill_create_widget (form, str, NULL, NULL);
	sequence_grid_attach_widget (form, grid, wid, seq_path, item_index);
	g_free (str);
}

/*
 * For sequences: treating the "sequence-item-remove" signal
 */
static void
sequence_item_remove_cb (GdaServerOperation *op, const gchar *seq_path, gint item_index, GdauiServerOperation *form)
{
	GtkWidget *grid;
	GList *children, *list, *to_move = NULL;
	gchar *str;
	WidgetData *wds, *wdi;
	guint min, size;

	min = gda_server_operation_get_sequence_min_size (op, seq_path);
	size = gda_server_operation_get_sequence_size (op, seq_path);
	/* note: size is the size of the sequence _before_ the actual removal of the sequence item */

	wds = widget_data_find (form, seq_path);
	g_assert (wds);
	grid = wds->widget;
	g_assert (grid);

	/* remove widget */
	str = g_strdup_printf ("%s/%d", seq_path, item_index);
	wdi = widget_data_find (form, str);
	g_free (str);
	g_assert (wdi);
	gtk_widget_destroy (wdi->widget);
	g_assert (wdi->parent == wds);
	wds->children = g_slist_remove (wds->children, wdi);
	widget_data_free (wdi);

	/* remove the widget associated to the sequence item */
	children = gtk_container_get_children (GTK_CONTAINER (grid));
	for (list = children; list; ) {
		GtkWidget *child = GTK_WIDGET (list->data);
		if (child) {
			guint top_attach;
			gtk_container_child_get (GTK_CONTAINER (grid), child,
						 "top-attach", &top_attach, NULL);
			if (top_attach == (guint)item_index) {
				gtk_widget_destroy (child);
				g_list_free (children);
				children = gtk_container_get_children (GTK_CONTAINER (grid));
				list = children;
				continue;
			}
		}
		list = list->next;
	}
	g_list_free (children);

	/* move children UP if necessary */
	children = gtk_container_get_children (GTK_CONTAINER (grid));
	for (list = children; list; list = list->next) {
		GtkWidget *child = GTK_WIDGET (list->data);
		if (child) {
			guint top_attach, left_attach;
			gtk_container_child_get (GTK_CONTAINER (grid), child,
						 "top-attach", &top_attach,
						 "left-attach", &left_attach, NULL);
			/* ADD/REMOVE button sensitivity */
			if (left_attach == 1) {
				if (top_attach == size)
					gtk_widget_set_sensitive (child, TRUE);
				else
					gtk_widget_set_sensitive (child, (size-1 > min) ? TRUE : FALSE);
			}

			/* move widgets UP if necessary and change the "_index" property */
			if (top_attach > (guint)item_index) {
				struct MoveChild *mc;
				gint index;

				mc = g_new (struct MoveChild, 1);
				mc->widget = child;
				mc->top_attach = top_attach - 1;
				to_move = g_list_append (to_move, mc);

				index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "_index"));
				if (index > 0)
					g_object_set_data (G_OBJECT (child), "_index",
							   GINT_TO_POINTER (index - 1));
			}
		}
	}
	g_list_free (children);

	for (list = to_move; list; list = list->next) {
		struct MoveChild *mc;

		mc = (struct MoveChild *) (list->data);
		gtk_container_child_set (GTK_CONTAINER (grid), mc->widget,
					 "top-attach", mc->top_attach,
					 "height", 1, NULL);
		g_free (list->data);
	}
	g_list_free (to_move);
}


/**
 * gdaui_server_operation_new_in_dialog
 * @op: a #GdaServerOperation object
 * @parent: (allow-none): the parent window for the new dialog, or %NULL
 * @title: (allow-none): the title of the dialog window, or %NULL
 * @header: (allow-none): a helper text displayed at the top of the dialog, or %NULL
 *
 * Creates a new #GdauiServerOperation widget in the same way as gdaui_server_operation_new()
 * and puts it into a #GtkDialog widget. The returned dialog has the "Ok" and "Cancel" buttons
 * which respectively return GTK_RESPONSE_ACCEPT and GTK_RESPONSE_REJECT.
 *
 * The #GdauiServerOperation widget is attached to the dialog using the user property
 * "form".
 *
 * Returns: the new #GtkDialog widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_server_operation_new_in_dialog (GdaServerOperation *op, GtkWindow *parent,
				      const gchar *title, const gchar *header)
{
	GtkWidget *form;
	GtkWidget *dlg;
	GtkWidget *dcontents;
	const gchar *rtitle;

	form = gdaui_server_operation_new (op);

	rtitle = title;
	if (!rtitle)
		rtitle = _("Server operation specification");

	dlg = gtk_dialog_new_with_buttons (rtitle, parent,
					   GTK_DIALOG_MODAL,
					   GTK_STOCK_OK,
					   GTK_RESPONSE_ACCEPT,
					   GTK_STOCK_CANCEL,
					   GTK_RESPONSE_REJECT,
					   NULL);
	dcontents = gtk_dialog_get_content_area (GTK_DIALOG (dlg));

	if (header && *header) {
		GtkWidget *label;

		label = gtk_label_new (NULL);
		gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
		gtk_label_set_markup (GTK_LABEL (label), header);
		gtk_box_pack_start (GTK_BOX (dcontents), label, FALSE, FALSE, 5);

		gtk_widget_show (label);
	}
	gtk_container_set_border_width (GTK_CONTAINER (dcontents), 4);
	gtk_box_pack_start (GTK_BOX (dcontents), form, TRUE, TRUE, 10);

	gtk_widget_show_all (form);

	return dlg;
}


/*
 * CREATE_TABLE "/FIELDS_A" Custom widgets rendering
 */
static void create_table_grid_fields_iter_row_changed_cb (GdaDataModelIter *grid_iter, gint row,
							  GdaDataModelIter *form_iter);
static void create_table_proxy_row_inserted_cb (GdaDataProxy *proxy, gint row, GdauiServerOperation *form);
static GtkWidget *
create_table_fields_array_create_widget (GdauiServerOperation *form, const gchar *path,
					 G_GNUC_UNUSED gchar **section_str,
					 G_GNUC_UNUSED GSList **label_widgets)
{
	GdaServerOperationNode *info_node;
	GtkWidget *hlayout, *sw, *box, *label;
	GtkWidget *grid_fields, *form_props, *winfo;
	GdaDataProxy *proxy;
	gint name_col, col, nbcols;
	GdaDataModelIter *grid_iter, *form_iter;

	info_node = gda_server_operation_get_node_info (form->priv->op, path);
	g_assert (info_node->type == GDA_SERVER_OPERATION_NODE_DATA_MODEL);

	hlayout = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);

	/* form for field properties */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack2 (GTK_PANED (hlayout), box, TRUE, TRUE);

	label = gtk_label_new (_("<b>Field properties:</b>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	form_props = gdaui_raw_form_new (GDA_DATA_MODEL (info_node->model));
	proxy = gdaui_data_proxy_get_proxy (GDAUI_DATA_PROXY (form_props));
	gdaui_data_proxy_set_write_mode (GDAUI_DATA_PROXY (form_props),
					 GDAUI_DATA_PROXY_WRITE_ON_VALUE_CHANGE);
	gtk_box_pack_start (GTK_BOX (box), form_props, TRUE, TRUE, 0);
	g_signal_connect (proxy, "row-inserted",
			  G_CALLBACK (create_table_proxy_row_inserted_cb), form);

	gtk_widget_show_all (box);

	/* grid for field names */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack1 (GTK_PANED (hlayout), box, TRUE, TRUE);

	label = gtk_label_new (_("<b>Fields:</b>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);

	grid_fields = gdaui_raw_grid_new (GDA_DATA_MODEL (proxy));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (grid_fields), FALSE);
	g_object_set (G_OBJECT (grid_fields), "info-cell-visible", FALSE, NULL);

	name_col = 0;
	nbcols = gda_data_proxy_get_proxied_model_n_cols (proxy);
	g_assert (name_col < nbcols);
	for (col = 0; col < name_col; col++)
		gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (grid_fields), col, FALSE);
	for (col = name_col + 1; col < nbcols; col++)
		gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (grid_fields), col, FALSE);

	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), grid_fields);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (sw))),
				      GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (box), sw, TRUE, TRUE, 0);

	/* buttons to add/remove fields */
	winfo = gdaui_data_proxy_info_new (GDAUI_DATA_PROXY (form_props),
					   GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS);
	gtk_box_pack_start (GTK_BOX (box), winfo, FALSE, FALSE, 0);

	gtk_widget_show_all (box);

	/* keep the selections in sync */
	grid_iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (grid_fields));
	form_iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (form_props));
	g_signal_connect (grid_iter, "row-changed",
			  G_CALLBACK (create_table_grid_fields_iter_row_changed_cb), form_iter);
	g_signal_connect (form_iter, "row-changed",
			  G_CALLBACK (create_table_grid_fields_iter_row_changed_cb), grid_iter);

	gtk_widget_set_vexpand (hlayout, TRUE);

	{
		GtkActionGroup *group;
		GtkAction *action;
		group = gdaui_data_proxy_get_actions_group (GDAUI_DATA_PROXY (form_props));
		action = gtk_action_group_get_action (group, "ActionNew");
		g_object_set (G_OBJECT (action), "tooltip", _("Add a new field"), NULL);
		action = gtk_action_group_get_action (group, "ActionDelete");
		g_object_set (G_OBJECT (action), "tooltip", _("Remove selected field"), NULL);
		action = gtk_action_group_get_action (group, "ActionCommit");
		gtk_action_set_visible (action, FALSE);
		action = gtk_action_group_get_action (group, "ActionReset");
		gtk_action_set_visible (action, FALSE);
	}

	return hlayout;
}

static void
create_table_grid_fields_iter_row_changed_cb (GdaDataModelIter *iter1, gint row, GdaDataModelIter *iter2)
{
	g_signal_handlers_block_by_func (G_OBJECT (iter2),
					 G_CALLBACK (create_table_grid_fields_iter_row_changed_cb), iter1);
	gda_data_model_iter_move_to_row (iter2, row);
	g_signal_handlers_unblock_by_func (G_OBJECT (iter2),
					   G_CALLBACK (create_table_grid_fields_iter_row_changed_cb), iter1);
}

static void
create_table_proxy_row_inserted_cb (GdaDataProxy *proxy, gint row, GdauiServerOperation *form)
{
	GdaDataModelIter *iter;
	GdaHolder *holder;
	GdaServerProvider *prov;
	GdaConnection *cnc;
	const gchar *type = NULL;

	iter = gda_data_model_create_iter (GDA_DATA_MODEL (proxy));
	gda_data_model_iter_move_to_row (iter, row);
	holder = gda_set_get_nth_holder (GDA_SET (iter), 0);
	gda_holder_set_value_str (holder, NULL, "fieldname", NULL);

	g_object_get (form->priv->op, "connection", &cnc, "provider", &prov, NULL);
	if (prov)
		type = gda_server_provider_get_default_dbms_type (prov, cnc, G_TYPE_STRING);
	holder = gda_set_get_nth_holder (GDA_SET (iter), 1);
	gda_holder_set_value_str (holder, NULL, type ? type : "varchar", NULL);
	if (cnc)
		g_object_unref (cnc);
	if (prov)
		g_object_unref (prov);
}
