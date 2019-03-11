/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2013 Daniel Espinosa <esodan@gmail.com>
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
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include "gdaui-basic-form.h"
#include "marshallers/gdaui-marshal.h"
#include "gdaui-enums.h"
#include "internal/utility.h"
#include "gdaui-data-entry.h"
#include <libgda-ui/data-entries/gdaui-entry-combo.h>
#include <libgda-ui/gdaui-data-proxy.h>
#include <libgda-ui/gdaui-data-selector.h>
#include <libgda-ui/gdaui-raw-form.h>
#include <libgda-ui/gdaui-easy.h>
#include <libgda/gda-debug-macros.h>

static void gdaui_basic_form_class_init (GdauiBasicFormClass * class);
static void gdaui_basic_form_init (GdauiBasicForm *wid);
static void gdaui_basic_form_dispose (GObject *object);

static void gdaui_basic_form_set_property (GObject *object,
					   guint param_id,
					   const GValue *value,
					   GParamSpec *pspec);
static void gdaui_basic_form_get_property (GObject *object,
					   guint param_id,
					   GValue *value,
					   GParamSpec *pspec);
static void gdaui_basic_form_widget_grab_focus (GtkWidget *widget);

typedef struct {
	GdaHolder *holder;
	gulong     signal_id;
} SignalData;

typedef enum {
	PACKING_TABLE,
} PackingType;

typedef struct {
	GtkWidget *grid_table;
	gint top;
} PackingTable;

typedef struct {
	GdauiBasicForm *form;
	GdauiDataEntry *entry; /* ref held here */
	GtkWidget      *label; /* ref held here */
	gchar          *label_title;
	gboolean        prog_hidden; /* status as requested by the programmer */
	gboolean        hidden; /* real status of the data entry */
	gboolean        not_null; /* TRUE if @entry's contents can't be NULL */
	gboolean        forward_param_updates; /* forward them to the GdauiDataEntry widgets ? */

	gulong          entry_shown_id; /* signal ID */
	gulong          label_shown_id; /* signal ID */

	gulong          entry_contents_modified_id; /* signal ID */
	gulong          entry_contents_activated_id; /* signal ID */

	GdaHolder      *single_param;
	SignalData      single_signal;

	GdauiSetGroup  *group;
	GArray         *group_signals; /* array of SignalData */

	/* entry packing function */
	PackingType     packing_type;
	union {
		PackingTable table;
	}               pack;
	
} SingleEntry;
static void real_gdaui_basic_form_entry_set_visible (GdauiBasicForm *form,
						      SingleEntry *sentry, gboolean show);
static SingleEntry *get_single_entry_for_holder (GdauiBasicForm *form, GdaHolder *param);
static SingleEntry *get_single_entry_for_id (GdauiBasicForm *form, const gchar *id);
static void create_entry_widget (SingleEntry *sentry);
static void create_entries (GdauiBasicForm *form);
static void pack_entry_widget (SingleEntry *sentry);
static void pack_entries_in_table (GdauiBasicForm *form);
static void pack_entries_in_xml_layout (GdauiBasicForm *form, xmlNodePtr form_node);
static void unpack_entries (GdauiBasicForm *form);
static void destroy_entries (GdauiBasicForm *form);
static gchar *create_text_label_for_sentry (SingleEntry *sentry, gchar **out_title);

static void gdaui_basic_form_set_entries_auto_default (GdauiBasicForm *form, gboolean auto_default);

static void get_rid_of_set (GdaSet *paramlist, GdauiBasicForm *form);
static void paramlist_public_data_changed_cb (GdauiSet *paramlist, GdauiBasicForm *form);
static void paramlist_param_attr_changed_cb (GdaSet *paramlist, GdaHolder *param,
					     const gchar *att_name, const GValue *att_value, GdauiBasicForm *form);
static void paramlist_holder_type_set_cb (GdaSet *paramlist, GdaHolder *param,
					  GdauiBasicForm *form);

static void entry_contents_modified (GdauiDataEntry *entry, SingleEntry *sentry);
static void entry_contents_activated (GdauiDataEntry *entry, GdauiBasicForm *form);
static void sync_entry_attributes (GdaHolder *param, SingleEntry *sentry);
static void parameter_changed_cb (GdaHolder *param, SingleEntry *sentry);

static void mark_not_null_entry_labels (GdauiBasicForm *form, gboolean show_mark);
enum {
	HOLDER_CHANGED,
	ACTIVATED,
	LAYOUT_CHANGED,
	POPULATE_POPUP,
	LAST_SIGNAL
};

/* properties */
enum {
	PROP_0,
	PROP_XML_LAYOUT,
	PROP_PARAMLIST,
	PROP_ENTRIES_AUTO_DEFAULT,
	PROP_CAN_VEXPAND
};

typedef struct {
	GtkSizeGroup *size_group; /* ref held here */
	GdauiBasicFormPart part;
} SizeGroup;
static void
size_group_free (SizeGroup *sg)
{
	g_return_if_fail (sg != NULL);
	if (sg->size_group != NULL) {
		g_object_unref (sg->size_group);
		sg->size_group = NULL;
	}
}
static SizeGroup*
size_group_new (void) {
	return g_new0 (SizeGroup,1);
}

typedef struct
{
	GdaSet     *set;
	GdauiSet   *set_info;
	GSList     *s_entries;/* list of SingleEntry pointers */
	GHashTable *place_holders; /* key = place holder ID, value = a GtkWidget pointer */

	GtkWidget  *top_container;

	gboolean    entries_auto_default;

	GSList     *size_groups; /* list of SizeGroup pointers */

	GtkWidget  *mainbox;

	/* unknown value color */
	gdouble     red;
	gdouble     green;
	gdouble     blue;
	gdouble     alpha;
} GdauiBasicFormPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiBasicForm, gdaui_basic_form, GTK_TYPE_BOX)

static guint gdaui_basic_form_signals[LAST_SIGNAL] = { 0, 0 };


static void
gdaui_basic_form_class_init (GdauiBasicFormClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	GTK_WIDGET_CLASS (klass)->grab_focus = gdaui_basic_form_widget_grab_focus;

	/* signals */
	/**
	 * GdauiBasicForm::holder-changed:
	 * @form: #GdauiBasicForm
	 * @param: the #GdaHolder that changed
	 * @is_user_modif: TRUE if the modification has been initiated by a user modification
	 *
	 * Emitted when a GdaHolder changed in @form
	 */
	gdaui_basic_form_signals[HOLDER_CHANGED] =
		g_signal_new ("holder-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdauiBasicFormClass, holder_changed),
			      NULL, NULL,
			      _gdaui_marshal_VOID__OBJECT_BOOLEAN, G_TYPE_NONE, 2,
			      GDA_TYPE_HOLDER, G_TYPE_BOOLEAN);
	/**
	 * GdauiBasicForm::activated:
	 * @form: #GdauiBasicForm
	 *
	 * Emitted when the use has activated any of the #GdaDataEntry widget
	 * in @form.
	 */
	gdaui_basic_form_signals[ACTIVATED] =
		g_signal_new ("activated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdauiBasicFormClass, activated),
			      NULL, NULL,
			      _gdaui_marshal_VOID__VOID, G_TYPE_NONE, 0);

	/**
	 * GdauiBasicForm::layout-changed:
	 * @form: #GdauiBasicForm
	 *
	 * Emitted when the form's layout changes
	 */
	gdaui_basic_form_signals[LAYOUT_CHANGED] =
		g_signal_new ("layout-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdauiBasicFormClass, layout_changed),
			      NULL, NULL,
			      _gdaui_marshal_VOID__VOID, G_TYPE_NONE, 0);

	/**
	 * GdauiBasicForm::populate-popup:
	 * @form: #GdauiBasicForm
	 * @menu: a #GtkMenu to modify
	 *
	 * Connect this signal and modify the popup menu.
	 *
	 * Since: 4.2.4
	 */
	gdaui_basic_form_signals[POPULATE_POPUP] =
		g_signal_new ("populate-popup",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              0,
                              NULL, NULL,
                              _gdaui_marshal_VOID__OBJECT, G_TYPE_NONE,
			      1, GTK_TYPE_MENU);

	klass->holder_changed = NULL;
	klass->activated = NULL;
	klass->layout_changed = NULL;
	object_class->dispose = gdaui_basic_form_dispose;

	/* Properties */
        object_class->set_property = gdaui_basic_form_set_property;
        object_class->get_property = gdaui_basic_form_get_property;

	g_object_class_install_property (object_class, PROP_XML_LAYOUT,
					 g_param_spec_pointer ("xml-layout",
							       _("Pointer to an XML layout specification  (as an xmlNodePtr to a <gdaui_form> node)"), NULL,
							       G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_PARAMLIST,
					 g_param_spec_pointer ("paramlist",
							       _("List of parameters to show in the form"), NULL,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_ENTRIES_AUTO_DEFAULT,
					 g_param_spec_boolean ("entries-auto-default",
							       _("Entries Auto-default"),
							       NULL, FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_CAN_VEXPAND,
					 g_param_spec_boolean ("can-expand-v",
							       _("TRUE if expanding the form vertically makes sense"),
							       NULL, FALSE,
							       G_PARAM_READABLE));

}

static void
hidden_entry_mitem_toggled_cb (GtkCheckMenuItem *check, GdauiBasicForm *form)
{
	SingleEntry *sentry;
	sentry = g_object_get_data (G_OBJECT (check), "s");
	g_assert (sentry);
	real_gdaui_basic_form_entry_set_visible (form, sentry,
						 gtk_check_menu_item_get_active (check));
}

static void
do_popup_menu (GdauiBasicForm *form, GdkEventButton *event)
{
	GtkWidget *menu, *submenu, *mitem;
	GSList *list;
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	
	menu = gtk_menu_new ();
	mitem = gtk_menu_item_new_with_label (_("Shown data entries"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
	gtk_widget_show (mitem);
	
	submenu = gtk_menu_new ();
	gtk_widget_show (submenu);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (mitem), submenu);
	
	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry*) list->data;
		if (sentry->prog_hidden)
			continue;
		mitem = gtk_check_menu_item_new_with_label (sentry->label_title);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mitem), !sentry->hidden);
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), mitem);
		gtk_widget_show (mitem);

		g_object_set_data (G_OBJECT (mitem), "s", sentry);
		g_signal_connect (mitem, "toggled",
				  G_CALLBACK (hidden_entry_mitem_toggled_cb), form);
	}
		
	/* allow listeners to add their custom menu items */
	g_signal_emit (G_OBJECT (form), gdaui_basic_form_signals [POPULATE_POPUP], 0, GTK_MENU (menu));

	gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

static gboolean
popup_menu_cb (G_GNUC_UNUSED GtkWidget *wid, GdauiBasicForm *form)
{
	do_popup_menu (form, NULL);
	return TRUE;
}

static gboolean
button_press_event_cb (G_GNUC_UNUSED GtkWidget *wid, GdkEventButton *event, GdauiBasicForm *form)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		do_popup_menu (form, event);
		return TRUE;
	}

	return FALSE;
}

static void
gdaui_basic_form_init (GdauiBasicForm *wid)
{
	GtkWidget *evbox;
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (wid);
	priv->set = NULL;
	priv->s_entries = NULL;
	priv->place_holders = NULL;
	priv->top_container = NULL;

	priv->entries_auto_default = FALSE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (wid), GTK_ORIENTATION_VERTICAL);

	evbox = gtk_event_box_new ();
	gtk_widget_show (evbox);
	gtk_box_pack_start (GTK_BOX (wid), evbox, TRUE, TRUE, 0);
	priv->mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	
	gtk_widget_show (priv->mainbox);
	gtk_container_add (GTK_CONTAINER (evbox), priv->mainbox);
	g_object_set (evbox, "visible-window", FALSE, NULL);

	g_signal_connect (evbox, "popup-menu",
			  G_CALLBACK (popup_menu_cb), wid);
	g_signal_connect (evbox, "button-press-event",
			  G_CALLBACK (button_press_event_cb), wid);

	priv->red = 0.;
	priv->green = 0.;
	priv->blue = 0.;
	priv->alpha = 0.;
}

/**
 * gdaui_basic_form_new:
 * @data_set: a #GdaSet structure
 *
 * Creates a new #GdauiBasicForm widget using all the #GdaHolder objects provided in @data_set.
 *
 * The global layout is rendered using a table (a #GtkTable), and an entry is created for each
 * node of @data_set.
 *
 * Returns: (transfer full): the new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_basic_form_new (GdaSet *data_set)
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_BASIC_FORM, "paramlist", data_set, NULL);

	return (GtkWidget *) obj;
}

static void
widget_shown_cb (GtkWidget *wid, SingleEntry *sentry)
{
	g_assert ((wid == (GtkWidget*) sentry->entry) || (wid == sentry->label));
	if (sentry->hidden)
		gtk_widget_hide (wid);
}

static void
get_rid_of_set (GdaSet *paramlist, GdauiBasicForm *form)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (paramlist != NULL);
	GSList *list;
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	g_assert (paramlist == priv->set);

	/* unref the paramlist */
	g_signal_handlers_disconnect_by_func (priv->set_info,
					      G_CALLBACK (paramlist_public_data_changed_cb), form);
	g_signal_handlers_disconnect_by_func (paramlist,
					      G_CALLBACK (paramlist_param_attr_changed_cb), form);
	g_signal_handlers_disconnect_by_func (paramlist,
					      G_CALLBACK (paramlist_holder_type_set_cb), form);

	if (priv->set) {
		g_object_unref (priv->set);
		priv->set = NULL;
	}

	if (priv->set_info) {
		g_object_unref (priv->set_info);
		priv->set_info = NULL;
	}

	/* render all the entries non sensitive */
	for (list = priv->s_entries; list; list = list->next)
		gdaui_data_entry_set_editable (GDAUI_DATA_ENTRY (((SingleEntry*)list->data)->entry), FALSE);
}

static void
paramlist_holder_type_set_cb (G_GNUC_UNUSED GdaSet *paramlist, GdaHolder *param,
			      GdauiBasicForm *form)
{
	SingleEntry *sentry;

	sentry = get_single_entry_for_holder (form, param);
	if (sentry) {
		create_entry_widget (sentry);
		pack_entry_widget (sentry);
		gdaui_basic_form_entry_set_visible (form, param, !sentry->hidden);
	}
}


static void
paramlist_public_data_changed_cb (G_GNUC_UNUSED GdauiSet *paramlist, GdauiBasicForm *form)
{
	/* here we want to re-define all the data entry widgets */
	destroy_entries (form);
	create_entries (form);
	pack_entries_in_table (form);
	g_signal_emit (G_OBJECT (form), gdaui_basic_form_signals[LAYOUT_CHANGED], 0);
}

static void
paramlist_param_attr_changed_cb (G_GNUC_UNUSED GdaSet *paramlist, GdaHolder *param,
				 const gchar *att_name, G_GNUC_UNUSED const GValue *att_value,
				 GdauiBasicForm *form)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	SingleEntry *sentry;
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	sentry = get_single_entry_for_holder (form, param);

	if (!g_strcmp0 (att_name, "to-default")) {
		GtkWidget *entry = NULL;
		if (sentry)
			entry = (GtkWidget*) sentry->entry;
		if (entry) {
			guint attrs = 0;
			guint mask = 0;
			const GValue *defv;
			gboolean toset;

			defv = gda_holder_get_default_value (param);
			attrs |= defv ? GDA_VALUE_ATTR_CAN_BE_DEFAULT : 0;
			mask |= GDA_VALUE_ATTR_CAN_BE_DEFAULT;

			toset = gda_holder_get_not_null (param);
			attrs |= toset ? 0 : GDA_VALUE_ATTR_CAN_BE_NULL;
			mask |= GDA_VALUE_ATTR_CAN_BE_NULL;

			if (gda_holder_value_is_default (param)) {
				attrs |= GDA_VALUE_ATTR_IS_DEFAULT;
				mask |= GDA_VALUE_ATTR_IS_DEFAULT;
			}
			g_signal_handlers_block_by_func (G_OBJECT (entry),
							 G_CALLBACK (entry_contents_modified), sentry);
			gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry), attrs, mask);
			g_signal_handlers_unblock_by_func (G_OBJECT (entry),
							   G_CALLBACK (entry_contents_modified), sentry);
		}
	}
	else if (!g_strcmp0 (att_name, "plugin")) {
		if (sentry) {
			/* recreate an entry widget */
			create_entry_widget (sentry);
			pack_entry_widget (sentry);
			gdaui_basic_form_entry_set_visible (form, param, !sentry->hidden);
		}
		else
			paramlist_public_data_changed_cb (priv->set_info, form);
	}
	else if (!g_strcmp0 (att_name, "name") ||
		 !g_strcmp0 (att_name, "description")) {
		if (sentry) {
			gchar *str, *title;
			GdaSetSource *ss;
			str = create_text_label_for_sentry (sentry, &title);
			gtk_label_set_text (GTK_LABEL (sentry->label), str);
			g_free (str);
			g_free (sentry->label_title);
			sentry->label_title = title;
			ss =  gda_set_group_get_source (gdaui_set_group_get_group (sentry->group));
			if (!ss) {
				g_object_get (G_OBJECT (param), "description", &title, NULL);
				if (title && *title)
					gtk_widget_set_tooltip_text (sentry->label, title);
				g_free (title);
			}
			else {
				title = g_object_get_data (G_OBJECT (gda_set_source_get_data_model (ss)),
							   "descr");
				if (title && *title)
					gtk_widget_set_tooltip_text (sentry->label, title);
			}
		}
		else
			paramlist_public_data_changed_cb (priv->set_info, form);
	}
}

static void
gdaui_basic_form_dispose (GObject *object)
{
	GdauiBasicForm *form;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_BASIC_FORM (object));
	form = GDAUI_BASIC_FORM (object);
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	/* paramlist */
	if (priv->set) {
		get_rid_of_set (priv->set, form);
		priv->set = NULL;
	}

	destroy_entries (form);

	if (priv->size_groups) {
		g_slist_free_full (priv->size_groups, (GDestroyNotify) size_group_free);
		priv->size_groups = NULL;
	}


	/* for the parent class */
	G_OBJECT_CLASS (gdaui_basic_form_parent_class)->dispose (object);
}


static void
gdaui_basic_form_set_property (GObject *object,
			       guint param_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	GdauiBasicForm *form;

	form = GDAUI_BASIC_FORM (object);
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	switch (param_id) {
	case PROP_XML_LAYOUT: {
		/* node should be a "gdaui_form" node */
		xmlNodePtr node = g_value_get_pointer (value);
		if (node) {
			g_return_if_fail (node);
			g_return_if_fail (!strcmp ((gchar*) node->name, "gdaui_form"));

			pack_entries_in_xml_layout (form, node);
		}
		else
			pack_entries_in_table (form);

		g_signal_emit (G_OBJECT (form), gdaui_basic_form_signals[LAYOUT_CHANGED], 0);
		break;
	}
	case PROP_PARAMLIST:
		if (priv->set) {
			get_rid_of_set (priv->set, form);
			destroy_entries (form);
		}

		priv->set = g_value_get_pointer (value);
		if (priv->set) {
			g_return_if_fail (GDA_IS_SET (priv->set));

			g_object_ref (priv->set);
			priv->set_info = gdaui_set_new (GDA_SET (priv->set));

			g_signal_connect (priv->set_info, "public-data-changed",
					  G_CALLBACK (paramlist_public_data_changed_cb), form);
			g_signal_connect (priv->set, "holder-attr-changed",
					  G_CALLBACK (paramlist_param_attr_changed_cb), form);
			g_signal_connect (priv->set, "holder-type-set",
					  G_CALLBACK (paramlist_holder_type_set_cb), form);

			create_entries (form);
			pack_entries_in_table (form);
			g_signal_emit (G_OBJECT (form), gdaui_basic_form_signals[LAYOUT_CHANGED], 0);
		}
		break;
	case PROP_ENTRIES_AUTO_DEFAULT:
		gdaui_basic_form_set_entries_auto_default (form, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_basic_form_get_property (GObject *object,
			       guint param_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	GdauiBasicForm *form;

	form = GDAUI_BASIC_FORM (object);
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	switch (param_id) {
		case PROP_PARAMLIST:
			g_value_set_pointer (value, priv->set);
			break;
		case PROP_ENTRIES_AUTO_DEFAULT:
			g_value_set_boolean (value, priv->entries_auto_default);
			break;
		case PROP_CAN_VEXPAND: {
			gboolean can_expand = FALSE;
			can_expand = gtk_widget_compute_expand (GTK_WIDGET (form),
								GTK_ORIENTATION_VERTICAL);
			g_value_set_boolean (value, can_expand);
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
disconnect_single_entry_signals (SingleEntry *sentry)
{
	if (sentry->entry)
		g_signal_handler_disconnect (sentry->entry, sentry->entry_contents_modified_id);

	sentry->entry_contents_modified_id = 0;
	if (sentry->entry)
		g_signal_handler_disconnect (sentry->entry, sentry->entry_contents_activated_id);
	sentry->entry_contents_activated_id = 0;

	if (sentry->single_signal.signal_id > 0) {
		SignalData *sd = &(sentry->single_signal);
		g_signal_handler_disconnect (sd->holder, sd->signal_id);
		g_object_unref ((GObject*) sd->holder);
		sd->signal_id = 0;
		sd->holder = NULL;
	}
	if (sentry->group_signals) {
		gsize i;
		for (i = 0; i < sentry->group_signals->len; i++) {
			SignalData sd = g_array_index (sentry->group_signals, SignalData,
						       i);
			g_signal_handler_disconnect (sd.holder, sd.signal_id);
			g_object_unref ((GObject*) sd.holder);
		}
		g_array_free (sentry->group_signals, TRUE);
		sentry->group_signals = NULL;
	}
}

/*
 * unpack_entries
 *
 * Remove entries from the GTK container in which they are
 */
static void
unpack_entries (GdauiBasicForm *form)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GSList *list;
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry *) list->data;
		if (sentry->entry) {
			GtkWidget *parent;
			parent = gtk_widget_get_parent ((GtkWidget*) sentry->entry);
			if (parent)
				gtk_container_remove (GTK_CONTAINER (parent), (GtkWidget*) sentry->entry);
		}
		if (sentry->label) {
			GtkWidget *parent;
			parent = gtk_widget_get_parent (sentry->label);
			if (parent)
				gtk_container_remove (GTK_CONTAINER (parent), sentry->label);
		}
	}
}

static void
destroy_entries (GdauiBasicForm *form)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	/* destroy all the widgets */
	if (priv->s_entries) {
		GSList *list;
		for (list = priv->s_entries; list; list = list->next) {
			if (list->data == NULL) continue;
			SingleEntry *sentry = (SingleEntry *) list->data;

			disconnect_single_entry_signals (sentry);
			if (sentry->entry != NULL) {
				g_object_unref ((GObject *) sentry->entry);
				sentry->entry = NULL;
			}
			if (sentry->label != NULL) {
				g_object_unref ((GObject *) sentry->label);
				sentry->label = NULL;
			}
			if (sentry->label_title != NULL) {
				g_free (sentry->label_title);
				sentry->label_title = NULL;
			}
			g_free (sentry);
		}
		g_slist_free (priv->s_entries);
		priv->s_entries = NULL;
	}

	if (priv->top_container) {
		gtk_widget_destroy (priv->top_container);
		priv->top_container = NULL;
		if (priv->place_holders) {
			g_hash_table_destroy (priv->place_holders);
			priv->place_holders = NULL;
		}
	}
}

static gchar *
create_text_label_for_sentry (SingleEntry *sentry, gchar **out_title)
{
	gchar *label = NULL;
	GdaSetSource *ss;
	g_assert (out_title);
	ss = gda_set_group_get_source (gdaui_set_group_get_group (sentry->group));
	if (!ss) {
		g_object_get (G_OBJECT (sentry->single_param), "name", out_title, NULL);
		if (!*out_title)
			*out_title = g_strdup (_("Value"));
		label = g_strdup_printf ("%s:", *out_title);
		/*
		g_object_get (G_OBJECT (param), "description", &title, NULL);
		if (title && *title)
			gtk_widget_set_tooltip_text (sentry->label, title);
		g_free (title);
		*/

	}
	else {
		GSList *params;
		gchar *title = NULL;

		label = g_object_get_data (G_OBJECT (gda_set_source_get_data_model (ss)), "name");
		if (label)
			title = g_strdup (label);
		else {
			GString *tstring = NULL;
			for (params = gda_set_group_get_nodes (gdaui_set_group_get_group (sentry->group));
			                 params; params = params->next) {
				g_object_get (gda_set_node_get_holder (GDA_SET_NODE (params->data)),
					      "name", &title, NULL);
				if (title) {
					if (tstring) 
						g_string_append (tstring, ",\n");
					else
						tstring = g_string_new ("");
					g_string_append (tstring, title);
				}
			}
			if (tstring)
				title = g_string_free (tstring, FALSE);
		}

		if (!title)
			title = g_strdup (_("Value"));

		label = g_strdup_printf ("%s:", title);
		*out_title = title;
	}

	return label;
}

static void
create_entry_widget (SingleEntry *sentry)
{
	GdauiSetGroup *group;
	GdaSetGroup *sg;
	GtkWidget *entry;
	gboolean editable = TRUE;
	GValue *ref_value = NULL;
  gint items_count;

	disconnect_single_entry_signals (sentry);
	if (sentry->entry) {
		GtkWidget *parent;
		parent = gtk_widget_get_parent ((GtkWidget*) sentry->entry);
		if (parent)
			gtk_container_remove (GTK_CONTAINER (parent), (GtkWidget*) sentry->entry);

		if (sentry->entry_shown_id) {
			g_signal_handler_disconnect (sentry->entry, sentry->entry_shown_id);
			sentry->entry_shown_id = 0;
		}

		const GValue *cvalue;
		cvalue = gdaui_data_entry_get_reference_value (sentry->entry);
		if (cvalue)
			ref_value = gda_value_copy (cvalue);
		editable = gdaui_data_entry_get_editable (sentry->entry);
		g_object_unref ((GObject *) sentry->entry);
		sentry->entry = NULL;
	}

	if (sentry->label) {
		GtkWidget *parent;
		parent = gtk_widget_get_parent (sentry->label);
		if (parent)
			gtk_container_remove (GTK_CONTAINER (parent), sentry->label);

		if (sentry->label_shown_id) {
			g_signal_handler_disconnect (sentry->label, sentry->label_shown_id);
			sentry->label_shown_id = 0;
		}

		g_object_unref ((GObject *) sentry->label);
		sentry->label = NULL;
	}

	group = sentry->group;
	sg = gdaui_set_group_get_group (group);

	g_return_if_fail (GDAUI_IS_BASIC_FORM (sentry->form));
	GdauiBasicFormPrivate *form_priv = gdaui_basic_form_get_instance_private (sentry->form);

	if (! gda_set_group_get_source (sg)) {
		/* there is only one non-constrained parameter */
		GdaHolder *param;
		GType type;
		const GValue *val, *default_val, *value;
		gboolean nnul;
		const gchar *plugin = NULL;

		items_count = gda_set_group_get_n_nodes (sg);
		if ( items_count != 1) { /* only 1 item in the list */
			g_warning (ngettext("There are more entries than expected in group. Found %d",
			                    "There are more entries than expected in group. Found %d",
			                    items_count),
			           items_count);
			return;
		}

		param = GDA_HOLDER (gda_set_node_get_holder (gda_set_group_get_node (sg)));
		sentry->single_param = param;
		
		val = gda_holder_get_value (param);
		default_val = gda_holder_get_default_value (param);
		nnul = gda_holder_get_not_null (param);
		sentry->not_null = nnul;

		/* determine initial value */
		type = gda_holder_get_g_type (param);
		value = val;
		if (!value && default_val &&
		    g_type_is_a (G_VALUE_TYPE ((GValue *) default_val), type))
			value = default_val;

		/* create entry */
		g_object_get (param, "plugin", &plugin, NULL);
		entry = GTK_WIDGET (gdaui_new_data_entry (type, plugin));
		sentry->entry = GDAUI_DATA_ENTRY (entry);

		/* set current value */
		if (gda_holder_is_valid (param))
			gdaui_data_entry_set_value (GDAUI_DATA_ENTRY (entry), val);
		else
			gdaui_data_entry_set_value (GDAUI_DATA_ENTRY (entry), NULL);

		if (ref_value) {
			gdaui_data_entry_set_reference_value (GDAUI_DATA_ENTRY (entry), ref_value);
			gda_value_free (ref_value);
		}
		else if (!nnul ||
			 (nnul && value &&
			  !g_type_is_a (G_VALUE_TYPE ((GValue *) value), GDA_TYPE_NULL)))
			gdaui_data_entry_set_reference_value (GDAUI_DATA_ENTRY (entry), value);

		if (default_val)
			gdaui_data_entry_set_default_value (GDAUI_DATA_ENTRY (entry), default_val);

		/* connect to the parameter's changes */
		sentry->single_signal.holder = g_object_ref (param);
		sentry->single_signal.signal_id = g_signal_connect (G_OBJECT (param), "changed",
								    G_CALLBACK (parameter_changed_cb),
								    sentry);

		/* label */
		gchar *title;
		gchar *str;
		str = create_text_label_for_sentry (sentry, &title);
		sentry->label = gtk_label_new (str);
		g_free (str);
		g_object_ref_sink (sentry->label);
		sentry->label_title = title;
		gtk_widget_show (sentry->label);
			
		g_object_get (G_OBJECT (param), "description", &title, NULL);
		if (title && *title)
			gtk_widget_set_tooltip_text (sentry->label, title);
		g_free (title);

		/* set up the data entry's attributes */
		sync_entry_attributes (param, sentry);
	}
	else {
		/* several parameters depending on the values of a GdaDataModel object */
		GSList *plist;
		gboolean nullok = TRUE;
		entry = gdaui_entry_combo_new (form_priv->set_info, gdaui_set_group_get_source (group));
		sentry->entry = GDAUI_DATA_ENTRY (entry);

		/* connect to the parameter's changes */
		sentry->group_signals = g_array_new (FALSE, FALSE, sizeof (SignalData));
		for (plist = gda_set_group_get_nodes (gdaui_set_group_get_group (group)); plist; plist = plist->next) {
			GdaHolder *param;

			param = gda_set_node_get_holder (GDA_SET_NODE (plist->data));
			if (nullok && gda_holder_get_not_null (param))
				nullok = FALSE;

			SignalData sd;
			sd.holder = g_object_ref (param);
			sd.signal_id = g_signal_connect (G_OBJECT (param), "changed",
							 G_CALLBACK (parameter_changed_cb),
							 sentry);
			g_array_append_val (sentry->group_signals, sd);
		}
		sentry->not_null = !nullok;

		/* label */
		gchar *title = NULL;
		gchar *str;
		GdaSetSource *ss;

		str = create_text_label_for_sentry (sentry, &title);
		sentry->label = gtk_label_new (str);
		g_free (str);
		g_object_ref_sink (sentry->label);
		sentry->label_title = title;
		gtk_widget_show (sentry->label);
		ss = gda_set_group_get_source (gdaui_set_group_get_group (group));
		title = g_object_get_data (G_OBJECT (gda_set_source_get_data_model (ss)), "descr");
		if (title && *title)
			gtk_widget_set_tooltip_text (sentry->label, title);

		/* set up the data entry's attributes using the 1st parameter */
		plist = gda_set_group_get_nodes (gdaui_set_group_get_group (group));
		sync_entry_attributes (gda_set_node_get_holder (GDA_SET_NODE (plist->data)), sentry);
	}
	g_object_ref_sink (sentry->entry);

	gtk_widget_set_halign (sentry->label, GTK_ALIGN_END);
	gtk_widget_set_hexpand (sentry->label, FALSE);
	g_object_set (G_OBJECT (sentry->label), "xalign", 1., NULL);
	gdaui_data_entry_set_editable (sentry->entry, editable);
	if (form_priv->alpha > 0.)
		gdaui_data_entry_set_unknown_color (sentry->entry, form_priv->red,
						    form_priv->green, form_priv->blue,
						    form_priv->alpha);

	GSList *list;
	for (list = form_priv->size_groups; list; list = list->next) {
		SizeGroup *sg = (SizeGroup*) list->data;
		switch (sg->part) {
		case GDAUI_BASIC_FORM_LABELS:
			if (sentry->label)
				gtk_size_group_add_widget (sg->size_group, sentry->label);
			break;
		case GDAUI_BASIC_FORM_ENTRIES:
			if (sentry->entry)
				gtk_size_group_add_widget (sg->size_group, GTK_WIDGET (sentry->entry));
			break;
		default:
			g_assert_not_reached ();
		}
	}

	/* connect the entry's changes */
	sentry->entry_contents_modified_id = g_signal_connect (G_OBJECT (entry), "contents-modified",
							       G_CALLBACK (entry_contents_modified),
							       sentry);

	sentry->entry_contents_activated_id = g_signal_connect (G_OBJECT (entry), "contents-activated",
								G_CALLBACK (entry_contents_activated),
								sentry->form);
}

/*
 * create the entries in the widget
 */
static void
create_entries (GdauiBasicForm *form)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	GSList *list;

	/* parameters list management */
	if (!priv->set || !gdaui_set_get_groups (priv->set_info))
		/* nothing to do */
		return;

	/* creating all the data entries, and putting them into the priv->entries list */
	for (list = gdaui_set_get_groups (priv->set_info); list; list = list->next) {
		SingleEntry *sentry;

		sentry = g_new0 (SingleEntry, 1);
		sentry->form = form;
		sentry->forward_param_updates = TRUE;
		priv->s_entries = g_slist_append (priv->s_entries, sentry);

		sentry->group = GDAUI_SET_GROUP (list->data);
		create_entry_widget (sentry);
	}
 
	/* Set the Auto entries default in the entries */
	gdaui_basic_form_set_entries_auto_default (form, priv->entries_auto_default);

	gdaui_basic_form_reset (form);
}

static void
pack_entry_widget (SingleEntry *sentry)
{
	switch (sentry->packing_type) {
	case PACKING_TABLE: {
		/* label for the entry */
		gint i = sentry->pack.table.top;
		GtkGrid *grid = GTK_GRID (sentry->pack.table.grid_table);
		GtkWidget *parent;

		if (sentry->label) {
			parent = gtk_widget_get_parent (sentry->label);
			if (parent)
				gtk_container_remove (GTK_CONTAINER (parent), sentry->label);
			gtk_grid_attach (grid, sentry->label, 0, i, 1, 1);
		}
		
		/* add the entry itself to the table */
		parent = gtk_widget_get_parent ((GtkWidget*) sentry->entry);
		if (parent)
			gtk_container_remove (GTK_CONTAINER (parent), (GtkWidget*) sentry->entry);
		gtk_grid_attach (grid, (GtkWidget*) sentry->entry, 1, i, 1, 1);

		gtk_widget_show ((GtkWidget*) sentry->entry);
		if (sentry->label)
			g_object_set (G_OBJECT (sentry->label), "can-focus", FALSE, NULL);
		break;
	}
	default:
		TO_IMPLEMENT;
	}
}

/*
 * create the entries in the widget
 */
static void
pack_entries_in_table (GdauiBasicForm *form)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	GtkWidget *grid;
	gint i;
	GSList *list;

	unpack_entries (form);
	if (priv->top_container) {
		gtk_widget_destroy (priv->top_container);
		priv->top_container = NULL;
		if (priv->place_holders) {
			g_hash_table_destroy (priv->place_holders);
			priv->place_holders = NULL;
		}
	}

	/* creating a table for all the entries */
	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), GDAUI_HIG_FORM_VSPACE);
	gtk_grid_set_column_spacing (GTK_GRID (grid), GDAUI_HIG_FORM_HSPACE);
	priv->top_container = grid;
	gtk_box_pack_start (GTK_BOX (priv->mainbox), grid, TRUE, TRUE, 0);
	g_object_set (G_OBJECT (grid),
		      "margin-top", GDAUI_HIG_FORM_VBORDER,
		      "margin-bottom", GDAUI_HIG_FORM_VBORDER,
		      "margin-start", GDAUI_HIG_FORM_HBORDER,
		      "margin-end", GDAUI_HIG_FORM_HBORDER, NULL);
		      
	for (list = priv->s_entries, i = 0;
	     list;
	     list = list->next, i++) {
		SingleEntry *sentry = (SingleEntry *) list->data;

		sentry->packing_type = PACKING_TABLE;
		sentry->pack.table.grid_table = grid;
		sentry->pack.table.top = i;

		pack_entry_widget (sentry);
	}
	mark_not_null_entry_labels (form, TRUE);
	gtk_widget_show (grid);
}

static GtkWidget *load_xml_layout_children (GdauiBasicForm *form, xmlNodePtr parent_node);
static GtkWidget *load_xml_layout_column (GdauiBasicForm *form, xmlNodePtr box_node);
static GtkWidget *load_xml_layout_section (GdauiBasicForm *form, xmlNodePtr section_node);

static void
pack_entries_in_xml_layout (GdauiBasicForm *form, xmlNodePtr form_node)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	GtkWidget *top;

	unpack_entries (form);
	if (priv->top_container) {
		gtk_widget_destroy (priv->top_container);
		priv->top_container = NULL;
		if (priv->place_holders) {
			g_hash_table_destroy (priv->place_holders);
			priv->place_holders = NULL;
		}
	}

	top = load_xml_layout_children (form, form_node);
	gtk_box_pack_start (GTK_BOX (priv->mainbox), top, TRUE, TRUE, 0);
	gtk_widget_show_all (top);
	mark_not_null_entry_labels (form, TRUE);
}

/*
 * Loads all @parent_node's children and returns the corresponding widget
 * @parent_node can be a "gdaui_form", "gdaui_section",
 * it _CANNOT_ be a "gdaui_entry"
 */ 
static GtkWidget *
load_xml_layout_children (GdauiBasicForm *form, xmlNodePtr parent_node)
{
	GtkWidget *top = NULL;
	xmlChar *prop;
	typedef enum {
		TOP_BOX,
		TOP_PANED
	} TopContainerType;
	TopContainerType ctype = TOP_BOX;
	gint pos = 0;

	prop = xmlGetProp (parent_node, BAD_CAST "container");
	if (prop) {
		if (xmlStrEqual (prop, BAD_CAST "columns")) {
			top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
			ctype = TOP_BOX;
		}
		if (xmlStrEqual (prop, BAD_CAST "rows")) {
			top = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
			ctype = TOP_BOX;
		}
		else if (xmlStrEqual (prop, BAD_CAST "hpaned")) {
			top = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
			ctype = TOP_PANED;
		}
		else if (xmlStrEqual (prop, BAD_CAST "vpaned")) {
			top = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
			ctype = TOP_PANED;
		}
		else 
			g_warning ("Unknown container type '%s', ignoring", (gchar*) prop);
		xmlFree (prop);
	}
	if (!top)
		top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	xmlNodePtr child;
	for (child = parent_node->children; child; child = child->next) {
		if (child->type != XML_ELEMENT_NODE)
			continue;

		GtkWidget *wid = NULL;

		if (xmlStrEqual (child->name, BAD_CAST "gdaui_column"))
			wid = load_xml_layout_column (form, child);
		else if (xmlStrEqual (child->name, BAD_CAST "gdaui_section"))
			wid = load_xml_layout_section (form, child);
		else
			g_warning ("Unknown node type '%s', ignoring", (gchar*) child->name);

		if (wid) {
			switch (ctype) {
			case TOP_BOX:
				gtk_box_pack_start (GTK_BOX (top), wid, TRUE, TRUE, GDAUI_HIG_FORM_VSPACE);
				break;
			case TOP_PANED:
				if (pos == 0)
					gtk_paned_add1 (GTK_PANED (top), wid);
				else if (pos == 1)
					gtk_paned_add2 (GTK_PANED (top), wid);
				else
					g_warning ("Paned container can't have more than two children");
				break;
			default:
				g_assert_not_reached ();
			}
			pos++;
		}
	}

	return top;
}

static GtkWidget *
load_xml_layout_column (GdauiBasicForm *form, xmlNodePtr box_node)
{
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	GtkWidget *grid;
	grid = gtk_grid_new ();
	
	xmlNodePtr child;
	gint i;
	for (i = 0, child = box_node->children; child; i++, child = child->next) {
		if (child->type != XML_ELEMENT_NODE)
			continue;

		if (xmlStrEqual (child->name, BAD_CAST "gdaui_entry")) {
			xmlChar *name;
			name = xmlGetProp (child, BAD_CAST "name");
			if (name) {
				SingleEntry *sentry;
				sentry = get_single_entry_for_id (form, (gchar*) name);
				if (sentry) {
					sentry->packing_type = PACKING_TABLE;
					sentry->pack.table.grid_table = grid;
					sentry->pack.table.top = i;

					xmlChar *plugin;
					plugin = xmlGetProp (child, BAD_CAST "plugin");
					if (plugin && sentry->single_param) {
						GValue *value;
						value = gda_value_new_from_string ((gchar*) plugin, G_TYPE_STRING);
						g_object_set (sentry->single_param, "plugin", value, NULL);
						gda_value_free (value);
					}
					if (plugin)
						xmlFree (plugin);

					xmlChar *label;
					label = xmlGetProp (child, BAD_CAST "label");
					if (label) {
						g_free (sentry->label_title);
						sentry->label_title = g_strdup ((gchar*) label);
						if (sentry->label) {
							gtk_widget_destroy (sentry->label);
							g_object_unref (sentry->label);
						}
						sentry->label = gtk_label_new ((gchar*) label);
						g_object_ref_sink (sentry->label);
						gtk_widget_set_halign (sentry->label, GTK_ALIGN_END);
						gtk_widget_set_hexpand (sentry->label, FALSE);
						g_object_set (G_OBJECT (sentry->label), "xalign", 1., NULL);

						xmlFree (label);
					}

					pack_entry_widget (sentry);
				}
				else
					g_warning ("Could not find entry named '%s', ignoring",
						   (gchar*) name);
				xmlFree (name);
			}
		}
		else if (xmlStrEqual (child->name, BAD_CAST "gdaui_placeholder")) {
			xmlChar *id;
			id = xmlGetProp (child, BAD_CAST "id");
			if (id) {
				GtkWidget *ph;
				xmlChar *label;
				label = xmlGetProp (child, BAD_CAST "label");

				if (label) {
					GtkWidget *wid;
					wid = gtk_label_new ((gchar*) label);
					gtk_grid_attach (GTK_GRID (grid), wid, 0, i, 1, 1);
					xmlFree (label);
				}

				if (! priv->place_holders)
					priv->place_holders = g_hash_table_new_full (g_str_hash, g_str_equal,
											   g_free, g_object_unref);
				ph = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
				g_hash_table_insert (priv->place_holders, g_strdup ((gchar*) id),
						     g_object_ref_sink ((GObject*)ph));
				gtk_grid_attach (GTK_GRID (grid), ph, 1, i, 1, 1);
				xmlFree (id);
			}
		}
		else
			g_warning ("Unknown node type '%s', ignoring", (gchar*) child->name);	
	}

	gtk_grid_set_row_spacing (GTK_GRID (grid), GDAUI_HIG_FORM_VSPACE);
	gtk_grid_set_column_spacing (GTK_GRID (grid), GDAUI_HIG_FORM_HSPACE);
	return grid;
}

static GtkWidget *
load_xml_layout_section (GdauiBasicForm *form, xmlNodePtr section_node)
{
	xmlChar *title;
	GtkWidget *hbox, *label, *sub, *retval;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
	title = xmlGetProp (section_node, BAD_CAST "title");
	if (title) {
		gchar *str;
		str = g_markup_printf_escaped ("<b>%s</b>", (gchar*) title);
		xmlFree (title);

		label = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_widget_set_halign (label, GTK_ALIGN_END);
		gtk_widget_set_hexpand (label, FALSE);
		g_object_set (G_OBJECT (label), "xalign", 1., NULL);

		GtkWidget *vbox;
		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
		retval = vbox;
	}
	else
		retval = hbox;

	label = gtk_label_new ("  ");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	sub = load_xml_layout_children (form, section_node);
	gtk_box_pack_start (GTK_BOX (hbox), sub, TRUE, TRUE, 0);
	
	return retval;
}

/*
 * if @show_mark is TRUE, display the label as bold
 */
static void
mark_not_null_entry_labels (GdauiBasicForm *form, gboolean show_mark)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	GSList *list;

	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry*) list->data;
		if (show_mark && sentry->not_null) {
			gchar *str;
			str = _gdaui_utility_markup_title (sentry->label_title, !show_mark || !sentry->not_null);
			gtk_label_set_markup (GTK_LABEL (sentry->label), str);
			g_free (str);
		}
		else
			gtk_label_set_text (GTK_LABEL (sentry->label), sentry->label_title);
	}
}

static void
entry_contents_activated (G_GNUC_UNUSED GdauiDataEntry *entry, GdauiBasicForm *form)
{
	g_signal_emit (G_OBJECT (form), gdaui_basic_form_signals[ACTIVATED], 0);
}

static void
entry_contents_modified (GdauiDataEntry *entry, SingleEntry *sentry)
{
	GdaHolder *param;
	GdaValueAttribute attr;

	attr = gdaui_data_entry_get_attributes (entry);
	param = sentry->single_param;
	if (param) { /* single parameter */
		GValue *value;

		sentry->forward_param_updates = FALSE;

		/* parameter's value */
		value = gdaui_data_entry_get_value (entry);
		if (attr & GDA_VALUE_ATTR_IS_DEFAULT)
			gda_holder_set_value_to_default (param);
		else if (gda_holder_set_value (param, value, NULL))
			g_signal_emit (G_OBJECT (sentry->form), gdaui_basic_form_signals[HOLDER_CHANGED],
				       0, param, TRUE);
		else {
			/* GdaHolder refused value => reset GdaDataEntry */
			g_signal_handler_block (G_OBJECT (entry),
						sentry->entry_contents_modified_id);
			gdaui_data_entry_set_value (entry,
						    gda_holder_is_valid (param) ?
						    gda_holder_get_value (param) : NULL);
			g_signal_handler_unblock (G_OBJECT (entry),
						  sentry->entry_contents_modified_id);
		}
		gda_value_free (value);
		sentry->forward_param_updates = TRUE;
	}
	else { /* multiple parameters */
		GSList *params;
		GSList *values, *list;
		GdauiSetGroup *group;

		group = sentry->group;
		params = gda_set_group_get_nodes (gdaui_set_group_get_group (group));
		values = gdaui_entry_combo_get_values (GDAUI_ENTRY_COMBO (entry));
		g_assert (g_slist_length (params) == g_slist_length (values));

		for (list = values; list; list = list->next, params = params->next) {
			/* REM: if there is more than one value in 'params', then a
			 * signal is emitted for each param that is changed,
			 * and there is no way for the listener of that signal to know if it
			 * the end of the "holder-changed" sequence. What could be done is:
			 * - adding another boolean to tell if that signal is the
			 *   last one in the "holder-changed" sequence, or
			 * - modify the signal to add a list of parameters which are changed
			 *   and emit only one signal.
			 */
			GdaHolder *param;
			sentry->forward_param_updates = FALSE;

			/* parameter's value */
			param = gda_set_node_get_holder (GDA_SET_NODE (params->data));
			gda_holder_set_value (param, (GValue *)(list->data), NULL);
			g_signal_emit (G_OBJECT (sentry->form), gdaui_basic_form_signals[HOLDER_CHANGED],
				       0, param, TRUE);
			sentry->forward_param_updates = TRUE;
		}
		g_slist_free (values);

#ifdef PROXY_STORE_EXTRA_VALUES
		/* updating the GdaDataProxy if there is one */
		g_return_if_fail (GDAUI_IS_BASIC_FORM (sentry->form));
		GdauiBasicFormPrivate *form_priv = gdaui_basic_form_get_instance_private (sentry->form);
		if (GDA_IS_DATA_MODEL_ITER (form_priv->set)) {
			GdaDataProxy *proxy;
			gint proxy_row;

			proxy_row = gda_data_model_iter_get_row ((GdaDataModelIter *) form_priv->set);

			g_object_get (G_OBJECT (form_priv->set), "data-model", &proxy, NULL);
			if (GDA_IS_DATA_PROXY (proxy)) {
				GSList *all_new_values;
				gint i, col;

				all_new_values = gdaui_entry_combo_get_all_values (GDAUI_ENTRY_COMBO (entry));
				for (i = 0; i < group->nodes_source->shown_n_cols; i++) {
					GValue *value;

					col = group->nodes_source->shown_cols_index[i];
					value = (GValue *) g_slist_nth_data (all_new_values, col);
					gda_data_proxy_set_model_row_value (proxy,
									    group->nodes_source->data_model,
									    proxy_row, col, value);
				}
				g_slist_free (all_new_values);
			}
			g_object_unref (proxy);
		}
#endif
	}
}

static void
sync_entry_attributes (GdaHolder *param, SingleEntry *sentry)
{
	const GValue *value = gda_holder_get_value (param);
	GdauiDataEntry *entry;
	gboolean param_valid;
	param_valid = gda_holder_is_valid (param);

	entry = sentry->entry;
	if (sentry->single_param) {
		gdaui_data_entry_set_value (entry, param_valid ? value : NULL);
		sentry->not_null = gda_holder_get_not_null (param);
	}
	else {
		GSList *values = NULL;
		GSList *list;
		gboolean allnull = TRUE;
		gboolean nullok = TRUE;

		for (list = gda_set_group_get_nodes (gdaui_set_group_get_group (sentry->group));
		     list; list = list->next) {
			const GValue *pvalue;
			pvalue = gda_holder_get_value (gda_set_node_get_holder (GDA_SET_NODE (list->data)));
			param_valid = param_valid && gda_holder_is_valid (param);
			values = g_slist_append (values, (GValue *) pvalue);
			if (allnull && pvalue &&
			    (G_VALUE_TYPE ((GValue *) pvalue) != GDA_TYPE_NULL))
				allnull = FALSE;
			if (nullok && gda_holder_get_not_null (param))
				nullok = FALSE;
		}

		if (!allnull)
			gdaui_entry_combo_set_values (GDAUI_ENTRY_COMBO (entry), values);
		else
			gdaui_entry_combo_set_values (GDAUI_ENTRY_COMBO (entry), NULL);

		g_slist_free (values);
		sentry->not_null = !nullok;
	}

	g_signal_emit (G_OBJECT (sentry->form), gdaui_basic_form_signals[HOLDER_CHANGED], 0,
		       param, FALSE);

	/* correctly update the NULLOK status */
	GdaValueAttribute attr;
	gboolean nullok;
	attr = gdaui_data_entry_get_attributes (entry);
	nullok = !sentry->not_null;
	if (((attr & GDA_VALUE_ATTR_CAN_BE_NULL) && !nullok) ||
	    (! (attr & GDA_VALUE_ATTR_CAN_BE_NULL) && nullok))
		gdaui_data_entry_set_attributes (entry, nullok ? GDA_VALUE_ATTR_CAN_BE_NULL : 0,
						 GDA_VALUE_ATTR_CAN_BE_NULL);

	/* update the VALID atttibute */
	gdaui_data_entry_set_attributes (entry, (!param_valid) ? GDA_VALUE_ATTR_DATA_NON_VALID : 0,
					 GDA_VALUE_ATTR_DATA_NON_VALID);
}

/*
 * Called when a parameter changes
 * We emit a "holder-changed" signal only if the 'sentry->forward_param_updates' is TRUE, which means
 * the param change does not come from a GdauiDataEntry change.
 */
static void
parameter_changed_cb (GdaHolder *param, SingleEntry *sentry)
{
	GdauiDataEntry *entry;
	entry = sentry->entry;
	if (sentry->forward_param_updates) {
		gboolean default_if_invalid = FALSE;
		gboolean param_valid;
		param_valid = gda_holder_is_valid (param);

		/* There can be a feedback from the entry if the param is invalid and "set-default-if-invalid"
		   exists and is TRUE */
		if (!param_valid)
			if (g_object_class_find_property (G_OBJECT_GET_CLASS (entry),
							  "set-default-if-invalid"))
				g_object_get (G_OBJECT (entry),
				"set-default-if-invalid", &default_if_invalid, NULL);

		/* updating the corresponding entry */
		if (! default_if_invalid) {
			g_signal_handler_block (G_OBJECT (entry),
						sentry->entry_contents_modified_id);
			g_signal_handler_block (G_OBJECT (entry),
						sentry->entry_contents_activated_id);
		}
		sync_entry_attributes (param, sentry);
		if (! default_if_invalid) {
			g_signal_handler_unblock (G_OBJECT (entry),
						  sentry->entry_contents_modified_id);
			g_signal_handler_unblock (G_OBJECT (entry),
						  sentry->entry_contents_activated_id);
		}
	}
}

/**
 * gdaui_basic_form_set_as_reference:
 * @form: a #GdauiBasicForm widget
 *
 * Tells @form that the current values in the different entries are
 * to be considered as the original values for all the entries; the immediate
 * consequence is that any sub-sequent call to gdaui_basic_form_has_changed()
 * will return %FALSE (of course until any entry is changed).
 *
 * Since: 4.2
 */
void
gdaui_basic_form_set_as_reference (GdauiBasicForm *form)
{
	GSList *list;
	GdaHolder *param;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry*) list->data;

		if (sentry->single_param) {
			/* non combo entry */
			param = sentry->single_param;
			g_signal_handler_block (G_OBJECT (sentry->entry),
						sentry->entry_contents_modified_id);
			gdaui_data_entry_set_reference_value (GDAUI_DATA_ENTRY (sentry->entry),
							     gda_holder_get_value (param));
			g_signal_handler_unblock (G_OBJECT (sentry->entry),
						  sentry->entry_contents_modified_id);
		}
		else {
			/* Combo entry */
			GSList *values = NULL;
			GSList *params;
			gboolean allnull = TRUE;

			for (params = gda_set_group_get_nodes (gdaui_set_group_get_group (sentry->group));
			     params; params = params->next) {
				const GValue *pvalue;
				pvalue = gda_holder_get_value (gda_set_node_get_holder (GDA_SET_NODE (params->data)));
				values = g_slist_append (values, (GValue *) pvalue);
				if (allnull && pvalue &&
				    (G_VALUE_TYPE ((GValue *) pvalue) != GDA_TYPE_NULL))
					allnull = FALSE;
			}

			if (!allnull)
				gdaui_entry_combo_set_reference_values (GDAUI_ENTRY_COMBO (sentry->entry), values);
			else
				gdaui_entry_combo_set_reference_values (GDAUI_ENTRY_COMBO (sentry->entry), NULL);

			g_slist_free (values);
		}
	}
}

/**
 * gdaui_basic_form_is_valid:
 * @form: a #GdauiBasicForm widget
 *
 * Tells if the form can be used as-is (if all the parameters do have some valid values)
 *
 * Returns: %TRUE if the form is valid
 *
 * Since: 4.2
 */
gboolean
gdaui_basic_form_is_valid (GdauiBasicForm *form)
{
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), FALSE);
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	return gda_set_is_valid (priv->set, NULL);
}

/**
 * gdaui_basic_form_get_data_set:
 * @form: a #GdauiBasicForm widget
 *
 * Get a pointer to the #GdaSet object which
 * is modified by @form
 *
 * Returns: (transfer none): a pointer to the #GdaSet
 *
 * Since: 4.2
 */
GdaSet *
gdaui_basic_form_get_data_set (GdauiBasicForm *form)
{
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	return priv->set;
}

/**
 * gdaui_basic_form_has_changed:
 * @form: a #GdauiBasicForm widget
 *
 * Tells if the form has had at least on entry changed since @form was created or
 * gdaui_basic_form_set_as_reference() has been called.
 *
 * Returns: %TRUE if one entry has changed at least
 *
 * Since: 4.2
 */
gboolean
gdaui_basic_form_has_changed (GdauiBasicForm *form)
{
	GSList *list;

	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), FALSE);
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry*) list->data;
		if (! (gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (sentry->entry)) &
		       GDA_VALUE_ATTR_IS_UNCHANGED))
			return TRUE;
	}
	return FALSE;
}

/**
 * gdaui_basic_form_reset:
 * @form: a #GdauiBasicForm widget
 *
 * Resets all the entries in the form to their
 * original values
 *
 * Since: 4.2
 */
void
gdaui_basic_form_reset (GdauiBasicForm *form)
{
	GSList *list;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry*) list->data;

		if (!sentry->single_param) {
			/* Combo entry */
			GSList *values = NULL;

			values = gdaui_entry_combo_get_reference_values (GDAUI_ENTRY_COMBO (sentry->entry));
			gdaui_entry_combo_set_values (GDAUI_ENTRY_COMBO (sentry->entry), values);
			g_slist_free (values);
		}
		else {
			/* non combo entry */
			const GValue *value;

			value = gdaui_data_entry_get_reference_value (GDAUI_DATA_ENTRY (sentry->entry));
			gdaui_data_entry_set_value (GDAUI_DATA_ENTRY (sentry->entry), value);
		}
	}
}

static void
real_gdaui_basic_form_entry_set_visible (GdauiBasicForm *form, SingleEntry *sentry, gboolean show)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (sentry);

	if (sentry->entry) {
		if (show) {
			if (sentry->entry_shown_id) {
				g_signal_handler_disconnect (sentry->entry, sentry->entry_shown_id);
				sentry->entry_shown_id = 0;
			}
			if (sentry->label_shown_id) {
				g_signal_handler_disconnect (sentry->label, sentry->label_shown_id);
				sentry->label_shown_id = 0;
			}
			gtk_widget_show ((GtkWidget*) sentry->entry);
			if (sentry->label)
				gtk_widget_show (sentry->label);
		}
		else {
			if (! sentry->entry_shown_id)
				sentry->entry_shown_id = g_signal_connect_after (sentry->entry, "show",
										 G_CALLBACK (widget_shown_cb), sentry);
			if (sentry->label && !sentry->label_shown_id)
				sentry->label_shown_id = g_signal_connect_after (sentry->label, "show",
										 G_CALLBACK (widget_shown_cb), sentry);
			gtk_widget_hide ((GtkWidget*) sentry->entry);
			if (sentry->label)
				gtk_widget_hide (sentry->label);
		}
		sentry->hidden = !show;
	}
}

/**
 * gdaui_basic_form_entry_set_visible:
 * @form: a #GdauiBasicForm widget
 * @holder: a #GdaHolder object
 * @show: set to %TRUE to show the data entry, and to %FALSE to hide it
 *
 * Shows or hides the #GdauiDataEntry in @form which corresponds to the
 * @holder data holder
 *
 * Since: 4.2
 */
void
gdaui_basic_form_entry_set_visible (GdauiBasicForm *form, GdaHolder *holder, gboolean show)
{
	SingleEntry *sentry;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (GDA_IS_HOLDER (holder));

	sentry = get_single_entry_for_holder (form, holder);
	if (!sentry) {
		g_warning (_("Can't find data entry for GdaHolder"));
		return;
	}

	real_gdaui_basic_form_entry_set_visible (form, sentry, show);
	sentry->prog_hidden = !show;
}

static void
gdaui_basic_form_widget_grab_focus (GtkWidget *widget)
{
	gdaui_basic_form_entry_grab_focus (GDAUI_BASIC_FORM (widget), NULL);
}

/**
 * gdaui_basic_form_entry_grab_focus:
 * @form: a #GdauiBasicForm widget
 * @holder: (nullable): a #GdaHolder object, or %NULL
 *
 * Makes the data entry corresponding to @holder grab the focus for the window it's in. If @holder is %NULL,
 * then the focus is on the first entry which needs attention.
 *
 * Since: 4.2
 */
void
gdaui_basic_form_entry_grab_focus (GdauiBasicForm *form, GdaHolder *holder)
{
	GtkWidget *entry = NULL;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	if (holder) {
		g_return_if_fail (GDA_IS_HOLDER (holder));
		entry = gdaui_basic_form_get_entry_widget (form, holder);
	}

	if (!entry && priv->set) {
		GSList *list;
		for (list = gda_set_get_holders (priv->set); list; list = list->next) {
			GdaHolder *holder;
			holder = GDA_HOLDER (list->data);
			if (!gda_holder_is_valid (holder)) {
				entry = gdaui_basic_form_get_entry_widget (form, holder);
				if (entry)
					break;
			}
		}
	}
	if (entry)
		gdaui_data_entry_grab_focus (GDAUI_DATA_ENTRY (entry));
}

/**
 * gdaui_basic_form_entry_set_editable:
 * @form: a #GdauiBasicForm widget
 * @holder: (nullable): a #GdaHolder object; or %NULL
 * @editable: %TRUE if corresponding data entry must be editable
 *
 * Sets the #GdauiDataEntry in @form which corresponds to the
 * @holder parameter editable or not. If @holder is %NULL, then all the parameters
 * are concerned.
 *
 * Since: 4.2
 */
void
gdaui_basic_form_entry_set_editable (GdauiBasicForm *form, GdaHolder *holder, gboolean editable)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	if (holder) {
		SingleEntry *sentry;
		sentry = get_single_entry_for_holder (form, holder);
		if (sentry)
			gdaui_data_entry_set_editable (sentry->entry, editable);
	}
	else {
		GSList *list;
		for (list = priv->s_entries; list; list = list->next)
			gdaui_data_entry_set_editable (GDAUI_DATA_ENTRY (((SingleEntry*) list->data)->entry),
						       editable);
	}
}


/*
 * gdaui_basic_form_set_entries_auto_default
 * @form: a #GdauiBasicForm widget
 * @auto_default:
 *
 * Sets wether all the #GdauiDataEntry entries in the form must default
 * to a default value if they are assigned a non valid value.
 * Depending on the real type of entry, it will provide a default value
 * which the user does not need to modify if it is OK.
 *
 * For example a date entry can by default display the current date.
 */
static void
gdaui_basic_form_set_entries_auto_default (GdauiBasicForm *form, gboolean auto_default)
{
	GSList *list;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	priv->entries_auto_default = auto_default;
	for (list = priv->s_entries; list; list = list->next) {
		if (g_object_class_find_property (G_OBJECT_GET_CLASS (((SingleEntry*) list->data)->entry),
						  "set-default-if-invalid"))
			g_object_set (G_OBJECT (((SingleEntry*) list->data)->entry),
				      "set-default-if-invalid", auto_default, NULL);
	}
}

/**
 * gdaui_basic_form_set_entries_to_default:
 * @form: a #GdauiBasicForm widget
 *
 * For each entry in the form, sets it to a default value if it is possible to do so.
 *
 * Since: 4.2
 */
void
gdaui_basic_form_set_entries_to_default (GdauiBasicForm *form)
{
	GSList *list;
	guint attrs;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry*) list->data;
		attrs = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (sentry->entry));
		if (attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT)
			gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (sentry->entry),
							 GDA_VALUE_ATTR_IS_DEFAULT, GDA_VALUE_ATTR_IS_DEFAULT);
	}
}

static void form_holder_changed (GdauiBasicForm *form, GdaHolder *param, gboolean is_user_modif, GtkDialog *dlg);

static SingleEntry *
get_single_entry_for_holder (GdauiBasicForm *form, GdaHolder *param)
{
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	GSList *list;
	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry *) list->data;
		if (sentry->single_param && (sentry->single_param == param))
			return sentry;
		else if (! sentry->single_param) {
			/* multiple parameters */
			GSList *params;

			for (params = gda_set_group_get_nodes (gdaui_set_group_get_group (sentry->group)); params;
			     params = params->next) {
				if (gda_set_node_get_holder (GDA_SET_NODE (params->data)) == (gpointer) param)
					return sentry;
			}
		}
	}
	return NULL;
}

static SingleEntry *
get_single_entry_for_id (GdauiBasicForm *form, const gchar *id)
{
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	GSList *list;
	g_return_val_if_fail (id, NULL);

	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry *) list->data;
		if (sentry->label_title && !strcmp (sentry->label_title, id))
			return sentry;
		if (sentry->single_param) {
			const gchar *hid;
			hid = gda_holder_get_id (sentry->single_param);
			if (hid && !strcmp (hid, id))
				return sentry;
		}
	}
	return NULL;	
}

/**
 * gdaui_basic_form_get_entry_widget:
 * @form: a #GdauiBasicForm widget
 * @holder: a #GdaHolder object
 *
 * Get the #GdauiDataEntry in @form which corresponds to the @holder place.
 *
 * Returns: (transfer none): the requested widget, or %NULL if not found
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_basic_form_get_entry_widget (GdauiBasicForm *form, GdaHolder *holder)
{
	SingleEntry *sentry;
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);

	sentry = get_single_entry_for_holder (form, holder);
	if (sentry)
		return GTK_WIDGET (sentry->entry);
	else
		return NULL;
}

/**
 * gdaui_basic_form_get_label_widget:
 * @form: a #GdauiBasicForm widget
 * @holder: a #GdaHolder object
 *
 * Get the label in @form which corresponds to the @holder holder.
 *
 * Returns: (transfer none): the requested widget, or %NULL if not found
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_basic_form_get_label_widget (GdauiBasicForm *form, GdaHolder *holder)
{
	SingleEntry *sentry;
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);

	sentry = get_single_entry_for_holder (form, holder);
	if (sentry)
		return sentry->label;
	else
		return NULL;
}

/**
 * gdaui_basic_form_new_in_dialog:
 * @data_set: a #GdaSet object
 * @parent: (nullable): the parent window for the new dialog, or %NULL
 * @title: (nullable): the title of the dialog window, or %NULL
 * @header: (nullable): a helper text displayed at the top of the dialog, or %NULL
 *
 * Creates a new #GdauiBasicForm widget in the same way as gdaui_basic_form_new()
 * and puts it into a #GtkDialog widget. The returned dialog has the "Ok" and "Cancel" buttons
 * which respectively return GTK_RESPONSE_ACCEPT and GTK_RESPONSE_REJECT.
 *
 * The #GdauiBasicForm widget is attached to the dialog using the user property
 * "form".
 *
 * Returns: (transfer full): the new #GtkDialog widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_basic_form_new_in_dialog (GdaSet *data_set, GtkWindow *parent,
				const gchar *title, const gchar *header)
{
	GtkWidget *form;
	GtkWidget *dlg;
	const gchar *rtitle;

	form = gdaui_basic_form_new (data_set);

	rtitle = title;
	if (!rtitle)
		rtitle = _("Values to be defined");

	dlg = gtk_dialog_new_with_buttons (rtitle, parent,
					   GTK_DIALOG_MODAL,
					   _("_Cancel"), GTK_RESPONSE_REJECT,
					   _("_Ok"), GTK_RESPONSE_ACCEPT,
					   NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_ACCEPT);
	if (header && *header) {
		GtkWidget *label;
		gchar *str;

		label = gtk_label_new (NULL);
		gtk_widget_set_halign (label, GTK_ALIGN_END);
		gtk_widget_set_hexpand (label, FALSE);
		g_object_set (G_OBJECT (label), "xalign", 1., NULL);
		str = g_markup_printf_escaped ("<b>%s:</b>", header);
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
				    label, FALSE, FALSE, GDAUI_HIG_FORM_VSEP);
		gtk_widget_show (label);
	}

	gboolean can_expand;
	can_expand = gtk_widget_compute_expand (GTK_WIDGET (form), GTK_ORIENTATION_VERTICAL);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg)))), 4);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), form,
			    can_expand, can_expand, 0);

	g_signal_connect (G_OBJECT (form), "holder-changed",
			  G_CALLBACK (form_holder_changed), dlg);
	g_object_set_data (G_OBJECT (dlg), "form", form);

	gtk_widget_show_all (form);
	form_holder_changed (GDAUI_BASIC_FORM (form), NULL, FALSE, GTK_DIALOG (dlg));

	return dlg;
}

static void
form_holder_changed (GdauiBasicForm *form, G_GNUC_UNUSED GdaHolder *param,
		     G_GNUC_UNUSED gboolean is_user_modif, GtkDialog *dlg)
{
	gboolean valid;

	valid = gdaui_basic_form_is_valid (form);

	gtk_dialog_set_response_sensitive (dlg, GTK_RESPONSE_ACCEPT, valid);
}

/**
 * gdaui_basic_form_set_layout_from_file:
 * @form: a #GdauiBasicForm
 * @file_name: XML file name to use
 * @form_name: the name of the form to use, in @file_name
 *
 * Sets a form layout according an XML description contained in @file_name, for the form identified
 * by the @form_name name (as an XML layout file can contain the descriptions of several forms and grids).
 *
 * Since: 4.2
 */
void
gdaui_basic_form_set_layout_from_file (GdauiBasicForm *form, const gchar *file_name, const gchar *form_name)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (file_name);
        g_return_if_fail (form_name);

	xmlDocPtr doc;
        doc = xmlParseFile (file_name);
        if (doc == NULL) {
                g_warning (_("'%s' document not parsed successfully"), file_name);
                return;
        }

        xmlDtdPtr dtd = NULL;

	GBytes *bytes;
	bytes = g_resources_lookup_data ("/gdaui/gdaui-layout.dtd", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	if (bytes) {
		xmlParserInputBufferPtr	ibuffer;
		gsize size;
		const char *data;
		data = (const char *) g_bytes_get_data (bytes, &size);
		ibuffer = xmlParserInputBufferCreateMem (data, size, XML_CHAR_ENCODING_NONE);
		dtd = xmlIOParseDTD (NULL, ibuffer, XML_CHAR_ENCODING_NONE);
		/* No need to call xmlFreeParserInputBuffer (ibuffer), because xmlIOParseDTD() does it */
		g_bytes_unref (bytes);
	}

        if (! dtd)
                g_warning ("DTD not parsed successfully, please report error to "
                           "http://gitlab.gnome.org/GNOME/libgda/issues");

        /* Get the root element node */
        xmlNodePtr root_node = NULL;
        root_node = xmlDocGetRootElement (doc);

        /* Must have root element, a name and the name must be "gdaui_layouts" */
        if (!root_node || !root_node->name ||
            ! xmlStrEqual (root_node->name, BAD_CAST "gdaui_layouts")) {
                xmlFreeDoc (doc);
                return;
        }

        xmlNodePtr node;
        for (node = root_node->children; node; node = node->next) {
                if ((node->type == XML_ELEMENT_NODE) &&
                    xmlStrEqual (node->name, BAD_CAST "gdaui_form")) {
                        xmlChar *str;
                        str = xmlGetProp (node, BAD_CAST "name");
			if (str) {
				if (!strcmp ((gchar*) str, form_name)) {
					g_object_set (G_OBJECT (form), "xml-layout", node, NULL);
					xmlFree (str);
					break;
				}
                                xmlFree (str);
                        }
                }
        }

        /* Free the document */
        xmlFreeDoc (doc);
}

/**
 * gdaui_basic_form_get_place_holder:
 * @form: a #GdauiBasicForm
 * @placeholder_id: the name of the requested place holder
 *
 * Retreives a pointer to a place holder widget. This feature is only available if a specific
 * layout has been defined for @form using gdaui_basic_form_set_layout_from_file().
 *
 * Returns: (transfer none): a pointer to the requested place holder, or %NULL if not found
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_basic_form_get_place_holder (GdauiBasicForm *form, const gchar *placeholder_id)
{
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);
	g_return_val_if_fail (placeholder_id, NULL);
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	if (! priv->place_holders)
		return NULL;
	return g_hash_table_lookup (priv->place_holders, placeholder_id);
}

/**
 * gdaui_basic_form_add_to_size_group:
 * @form: a #GdauiBasicForm
 * @size_group: a #GtkSizeGroup object
 * @part: specifies which widgets in @form are concerned
 *
 * Add @form's widgets specified by @part to @size_group
 * (the widgets can then be removed using gdaui_basic_form_remove_from_size_group()).
 *
 * Since: 4.2
 */
void
gdaui_basic_form_add_to_size_group (GdauiBasicForm *form, GtkSizeGroup *size_group, GdauiBasicFormPart part)
{
	GSList *list;
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	SizeGroup *sg;
	sg = size_group_new ();
	sg->size_group = g_object_ref (size_group);
	sg->part = part;
	priv->size_groups = g_slist_append (priv->size_groups, sg);

	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *se = (SingleEntry*) list->data;
		switch (part) {
		case GDAUI_BASIC_FORM_LABELS:
			if (se->label)
				gtk_size_group_add_widget (size_group, se->label);
			break;
		case GDAUI_BASIC_FORM_ENTRIES:
			if (se->entry)
				gtk_size_group_add_widget (size_group, GTK_WIDGET (se->entry));
			break;
		default:
			g_assert_not_reached ();
		}
	}
}

/**
 * gdaui_basic_form_remove_from_size_group:
 * @form: a #GdauiBasicForm
 * @size_group: a #GtkSizeGroup object
 * @part: specifies which widgets in @form are concerned
 *
 * Removes @form's widgets specified by @part from @size_group
 * (the widgets must have been added using gdaui_basic_form_add_to_size_group()).
 *
 * Since: 4.2
 */
void
gdaui_basic_form_remove_from_size_group (GdauiBasicForm *form, GtkSizeGroup *size_group, GdauiBasicFormPart part)
{
	GSList *list;
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);
	
	SizeGroup *sg;
	for (list = priv->size_groups; list; list = list->next) {
		if (list->data == NULL) continue;
		sg = (SizeGroup*) list->data;
		if (sg->size_group == size_group) {
			priv->size_groups = g_slist_remove (priv->size_groups, sg);
			size_group_free (sg);
			break;
		}
		sg = NULL;
	}

	if (!sg) {
		g_warning (_("size group was not taken into account using gdaui_basic_form_add_to_size_group()"));
		return;
	}

	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *se = (SingleEntry*) list->data;
		switch (part) {
		case GDAUI_BASIC_FORM_LABELS:
			if (se->label)
				gtk_size_group_remove_widget (size_group, se->label);
			break;
		case GDAUI_BASIC_FORM_ENTRIES:
			if (se->entry)
				gtk_size_group_remove_widget (size_group, GTK_WIDGET (se->entry));
			break;
		default:
			g_assert_not_reached ();
		}
	}
}

/**
 * gdaui_basic_form_set_unknown_color:
 * @form: a #GdauiBasicForm widget
 * @red: the red component of a color
 * @green: the green component of a color
 * @blue: the blue component of a color
 * @alpha: the alpha component of a color
 *
 * Defines the color to be used when @form displays an invalid value. Any value not
 * between 0. and 1. will result in the default hard coded values to be used (grayish).
 *
 * Since: 5.0.3
 */
void
gdaui_basic_form_set_unknown_color (GdauiBasicForm *form, gdouble red, gdouble green,
				    gdouble blue, gdouble alpha)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail ((red >= 0.) && (red <= 1.));
	g_return_if_fail ((green >= 0.) && (green <= 1.));
	g_return_if_fail ((blue >= 0.) && (blue <= 1.));
	g_return_if_fail ((alpha >= 0.) && (alpha <= 1.));
	GdauiBasicFormPrivate *priv = gdaui_basic_form_get_instance_private (form);

	priv->red = red;
	priv->green = green;
	priv->blue = blue;
	priv->alpha = alpha;

	GSList *list;
	for (list = priv->s_entries; list; list = list->next) {
		SingleEntry *se;
		se = (SingleEntry *) list->data;
		gdaui_data_entry_set_unknown_color (se->entry, priv->red,
						    priv->green, priv->blue,
						    priv->alpha);
	}
}

/**
 * gdaui_basic_form_update_data_set:
 * @form: a #GdauiBasicForm
 *
 * Updates values in all #GdaHolder in current #GdaSet
 */
void
gdaui_basic_form_update_data_set (GdauiBasicForm *form, GError **error) {
  g_return_if_fail (form != NULL);
  GSList *list;
  GdaSet *set;
  set = gdaui_basic_form_get_data_set (form);
  g_return_if_fail (set != NULL);
  list = gda_set_get_holders (set);
  for (; list != NULL; list = list->next) {
    GtkWidget *widget;
    GdaHolder *holder;
    GValue *value;
    if (list->data == NULL)
      continue;
    holder = GDA_HOLDER (list->data);
    widget = gdaui_basic_form_get_entry_widget (form, holder);
    if (!GDAUI_IS_DATA_ENTRY (widget)) {
      g_warning ("No data entry for holder %s", gda_holder_get_id (holder));
      continue;
    }
    value = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY (widget));
    gda_holder_set_value (holder, value, error);
  }
}
