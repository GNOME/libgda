/* gdaui-basic-form.c
 *
 * Copyright (C) 2002 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/binreloc/gda-binreloc.h>

#define SPACING 3
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

typedef struct {
	GdaHolder *holder;
	gulong     signal_id;
} SignalData;

typedef enum {
	PACKING_TABLE,
} PackingType;

typedef struct {
	GtkWidget *table;
	gint top;
} PackingTable;

typedef struct {
	GdauiBasicForm *form;
	GdauiDataEntry *entry; /* ref held here */
	GtkWidget      *label; /* ref held here */
	gchar          *label_title;
	gboolean        hidden;
	gboolean        not_null; /* TRUE if @entry's contents can't be NULL */
	gboolean        can_expand; /* tells if @entry can expand */
	gboolean        forward_param_updates; /* forward them to the GdauiDataEntry widgets ? */

	gulong          entry_shown_id; /* signal ID */
	gulong          label_shown_id; /* signal ID */

	gulong          entry_contents_modified_id; /* signal ID */
	gulong          entry_expand_changed_id; /* signal ID */
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

static void gdaui_basic_form_show_entry_actions (GdauiBasicForm *form, gboolean show_actions);
static void gdaui_basic_form_set_entries_auto_default (GdauiBasicForm *form, gboolean auto_default);

static void get_rid_of_set (GdaSet *paramlist, GdauiBasicForm *form);
static void paramlist_public_data_changed_cb (GdauiSet *paramlist, GdauiBasicForm *form);
static void paramlist_param_attr_changed_cb (GdaSet *paramlist, GdaHolder *param,
					     const gchar *att_name, const GValue *att_value, GdauiBasicForm *form);
static void paramlist_holder_type_set_cb (GdaSet *paramlist, GdaHolder *param,
					  GdauiBasicForm *form);

static void entry_contents_modified (GdauiDataEntry *entry, SingleEntry *sentry);
static void entry_expand_changed_cb (GdauiDataEntry *entry, SingleEntry *sentry);
static void entry_contents_activated (GdauiDataEntry *entry, GdauiBasicForm *form);
static void parameter_changed_cb (GdaHolder *param, SingleEntry *sentry);

static void mark_not_null_entry_labels (GdauiBasicForm *form, gboolean show_mark);
enum {
	HOLDER_CHANGED,
	ACTIVATED,
	LAYOUT_CHANGED,
	LAST_SIGNAL
};

/* properties */
enum {
	PROP_0,
	PROP_XML_LAYOUT,
	PROP_PARAMLIST,
	PROP_HEADERS_SENSITIVE,
	PROP_SHOW_ACTIONS,
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
	g_object_unref (sg->size_group);
	g_free (sg);
}

struct _GdauiBasicFormPriv
{
	GdaSet     *set;
	GdauiSet   *set_info;
	GSList     *s_entries;/* list of SingleEntry pointers */
	GHashTable *place_holders; /* key = place holder ID, value = a GtkWidget pointer */

	GtkWidget  *top_container;

	gboolean    headers_sensitive;
	gboolean    show_actions;
	gboolean    entries_auto_default;

	GSList     *size_groups; /* list of SizeGroup pointers */

	GtkWidget  *mainbox;
};


static guint gdaui_basic_form_signals[LAST_SIGNAL] = { 0, 0 };

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gdaui_basic_form_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiBasicFormClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_basic_form_class_init,
			NULL,
			NULL,
			sizeof (GdauiBasicForm),
			0,
			(GInstanceInitFunc) gdaui_basic_form_init,
			0
		};

		type = g_type_register_static (GTK_TYPE_VBOX, "GdauiBasicForm", &info, 0);
	}

	return type;
}

static void
gdaui_basic_form_class_init (GdauiBasicFormClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	/* signals */
	/**
	 * GdauiBasicForm::holder-changed:
	 * @form: GdauiBasicForm
	 * @param: that changed
	 * @is_user_modif: TRUE if the modification has been initiated by a user modification
	 *
	 * Emitted when a GdaHolder changes
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
	 * @form: GdauiBasicForm
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
	 * @form: GdauiBasicForm
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

	class->holder_changed = NULL;
	class->activated = NULL;
	class->layout_changed = NULL;
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
	g_object_class_install_property (object_class, PROP_HEADERS_SENSITIVE,
					 g_param_spec_boolean ("headers-sensitive",
							       _("Entry headers are sensitive"),
							       NULL, FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_SHOW_ACTIONS,
					 g_param_spec_boolean ("show-actions",
							       _("Show Entry actions"),
							       NULL, FALSE,
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
	int button, event_time;
	GtkWidget *menu, *submenu, *mitem;
	GSList *list;
	
	menu = gtk_menu_new ();
	mitem = gtk_menu_item_new_with_label (_("Shown data entries"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
	gtk_widget_show (mitem);
	
	submenu = gtk_menu_new ();
	gtk_widget_show (submenu);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (mitem), submenu);
	
	for (list = form->priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry*) list->data;
		mitem = gtk_check_menu_item_new_with_label (sentry->label_title);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mitem), !sentry->hidden);
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), mitem);
		gtk_widget_show (mitem);

		g_object_set_data (G_OBJECT (mitem), "s", sentry);
		g_signal_connect (mitem, "toggled",
				  G_CALLBACK (hidden_entry_mitem_toggled_cb), form);
	}
		
	if (event) {
		button = event->button;
		event_time = event->time;
	}
	else {
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 
			button, event_time);
}

static gboolean
popup_menu_cb (G_GNUC_UNUSED GtkWidget *wid, GdauiBasicForm *form)
{
	do_popup_menu (form, NULL);
	return TRUE;
}

static gboolean
button_press_event_cb (G_GNUC_UNUSED GdauiBasicForm *wid, GdkEventButton *event, GdauiBasicForm *form)
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
	wid->priv = g_new0 (GdauiBasicFormPriv, 1);
	wid->priv->set = NULL;
	wid->priv->s_entries = NULL;
	wid->priv->place_holders = NULL;
	wid->priv->top_container = NULL;

	wid->priv->headers_sensitive = FALSE;
	wid->priv->show_actions = FALSE;
	wid->priv->entries_auto_default = FALSE;

	evbox = gtk_event_box_new ();
	gtk_widget_show (evbox);
	gtk_box_pack_start (GTK_BOX (wid), evbox, TRUE, TRUE, 0);
	wid->priv->mainbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (wid->priv->mainbox);
	gtk_container_add (GTK_CONTAINER (evbox), wid->priv->mainbox);

	g_signal_connect (evbox, "popup-menu",
			  G_CALLBACK (popup_menu_cb), wid);
	g_signal_connect (evbox, "button-press-event",
			  G_CALLBACK (button_press_event_cb), wid);
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
	if (sentry->hidden) {
		GtkWidget *parent;
		parent = gtk_widget_get_parent (wid);
		if (parent && GTK_IS_TABLE (parent)) {
			gint row;
			gtk_container_child_get (GTK_CONTAINER (parent), wid, "top-attach", &row, NULL);
			gtk_table_set_row_spacing (GTK_TABLE (parent), row, 0);
		}

		gtk_widget_hide (wid);
	}
}

static void
get_rid_of_set (GdaSet *paramlist, GdauiBasicForm *form)
{
	GSList *list;

	g_assert (paramlist == form->priv->set);

	/* unref the paramlist */
	g_signal_handlers_disconnect_by_func (form->priv->set_info,
					      G_CALLBACK (paramlist_public_data_changed_cb), form);

	g_signal_handlers_disconnect_by_func (paramlist,
					      G_CALLBACK (paramlist_param_attr_changed_cb), form);
	g_signal_handlers_disconnect_by_func (paramlist,
					      G_CALLBACK (paramlist_holder_type_set_cb), form);

	g_object_unref (form->priv->set);
	form->priv->set = NULL;

	if (form->priv->set_info) {
		g_object_unref (form->priv->set_info);
		form->priv->set_info = NULL;
	}

	/* render all the entries non sensitive */
	for (list = form->priv->s_entries; list; list = list->next)
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
		gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (sentry->entry),
						 form->priv->show_actions ? GDA_VALUE_ATTR_ACTIONS_SHOWN : 0,
						 GDA_VALUE_ATTR_ACTIONS_SHOWN);
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
	SingleEntry *sentry;

	sentry = get_single_entry_for_holder (form, param);

	if (!strcmp (att_name, GDA_ATTRIBUTE_IS_DEFAULT)) {
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

			defv = gda_holder_get_attribute (param, GDA_ATTRIBUTE_IS_DEFAULT);
			if (defv && (G_VALUE_TYPE (defv) == G_TYPE_BOOLEAN) && g_value_get_boolean (defv)) {
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
	else if (!strcmp (att_name, GDAUI_ATTRIBUTE_PLUGIN)) {
		if (sentry) {
			/* recreate an entry widget */
			create_entry_widget (sentry);
			gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (sentry->entry),
							 form->priv->show_actions ? GDA_VALUE_ATTR_ACTIONS_SHOWN : 0,
							 GDA_VALUE_ATTR_ACTIONS_SHOWN);
			pack_entry_widget (sentry);
			gdaui_basic_form_entry_set_visible (form, param, !sentry->hidden);
		}
		else
			paramlist_public_data_changed_cb (form->priv->set_info, form);
	}
	else if (!strcmp (att_name, GDA_ATTRIBUTE_NAME) ||
		 !strcmp (att_name, GDA_ATTRIBUTE_DESCRIPTION)) {
		if (sentry) {
			gchar *str, *title;
			str = create_text_label_for_sentry (sentry, &title);
			gtk_label_set_text (GTK_LABEL (sentry->label), str);
			g_free (str);
			g_free (sentry->label_title);
			sentry->label_title = title;
			
			if (! sentry->group->group->nodes_source) {
				g_object_get (G_OBJECT (param), "description", &title, NULL);
				if (title && *title)
					gtk_widget_set_tooltip_text (sentry->label, title);
				g_free (title);
			}
			else {
				title = g_object_get_data (G_OBJECT (sentry->group->group->nodes_source->data_model),
							   "descr");
				if (title && *title)
					gtk_widget_set_tooltip_text (sentry->label, title);
			}
		}
		else
			paramlist_public_data_changed_cb (form->priv->set_info, form);
	}
}

static void
gdaui_basic_form_dispose (GObject *object)
{
	GdauiBasicForm *form;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_BASIC_FORM (object));
	form = GDAUI_BASIC_FORM (object);

	if (form->priv) {
		/* paramlist */
		if (form->priv->set)
			get_rid_of_set (form->priv->set, form);

		destroy_entries (form);

		if (form->priv->size_groups) {
			g_slist_foreach (form->priv->size_groups, (GFunc) size_group_free, NULL);
			g_slist_free (form->priv->size_groups);
		}

		/* the private area itself */
		g_free (form->priv);
		form->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}


static void
gdaui_basic_form_set_property (GObject *object,
			       guint param_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	GdauiBasicForm *form;

        form = GDAUI_BASIC_FORM (object);
        if (form->priv) {
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
			if (form->priv->set) {
				get_rid_of_set (form->priv->set, form);
				destroy_entries (form);
			}

			form->priv->set = g_value_get_pointer (value);
			if (form->priv->set) {
				g_return_if_fail (GDA_IS_SET (form->priv->set));

				g_object_ref (form->priv->set);
				form->priv->set_info = _gdaui_set_new (GDA_SET (form->priv->set));

				g_signal_connect (form->priv->set_info, "public-data-changed",
						  G_CALLBACK (paramlist_public_data_changed_cb), form);
				g_signal_connect (form->priv->set, "holder-attr-changed",
						  G_CALLBACK (paramlist_param_attr_changed_cb), form);
				g_signal_connect (form->priv->set, "holder-type-set",
						  G_CALLBACK (paramlist_holder_type_set_cb), form);

				create_entries (form);
				pack_entries_in_table (form);
				g_signal_emit (G_OBJECT (form), gdaui_basic_form_signals[LAYOUT_CHANGED], 0);
			}
			break;
		case PROP_HEADERS_SENSITIVE:
			form->priv->headers_sensitive = g_value_get_boolean (value);
			break;
		case PROP_SHOW_ACTIONS:
			gdaui_basic_form_show_entry_actions (form, g_value_get_boolean (value));
			break;
		case PROP_ENTRIES_AUTO_DEFAULT:
			gdaui_basic_form_set_entries_auto_default (form, g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
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
        if (form->priv) {
                switch (param_id) {
		case PROP_PARAMLIST:
			g_value_set_pointer (value, form->priv->set);
			break;
		case PROP_HEADERS_SENSITIVE:
			g_value_set_boolean (value, form->priv->headers_sensitive);
			break;
		case PROP_SHOW_ACTIONS:
			g_value_set_boolean (value, form->priv->show_actions);
			break;
		case PROP_ENTRIES_AUTO_DEFAULT:
			g_value_set_boolean (value, form->priv->entries_auto_default);
			break;
		case PROP_CAN_VEXPAND: {
			gboolean can_expand = FALSE;
			GSList *list;
			for (list = form->priv->s_entries; list; list = list->next) {
				if (((SingleEntry*) list->data)->can_expand) {
					can_expand = TRUE;
					break;
				}
			}
			g_value_set_boolean (value, can_expand);
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

static void
disconnect_single_entry_signals (SingleEntry *sentry)
{
	if (sentry->entry) {
		g_signal_handler_disconnect (sentry->entry, sentry->entry_contents_modified_id);
		g_signal_handler_disconnect (sentry->entry, sentry->entry_expand_changed_id);
	}
	sentry->entry_contents_modified_id = 0;
	sentry->entry_expand_changed_id = 0;
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
	GSList *list;
	for (list = form->priv->s_entries; list; list = list->next) {
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
	/* destroy all the widgets */
	if (form->priv->s_entries) {
		GSList *list;
		for (list = form->priv->s_entries; list; list = list->next) {
			SingleEntry *sentry = (SingleEntry *) list->data;

			disconnect_single_entry_signals (sentry);

			g_object_unref ((GObject *) sentry->entry);
			g_object_unref ((GObject *) sentry->label);
			g_free (sentry->label_title);
			g_free (sentry);
		}
		g_slist_free (form->priv->s_entries);
		form->priv->s_entries = NULL;
	}

	if (form->priv->top_container) {
		gtk_widget_destroy (form->priv->top_container);
		form->priv->top_container = NULL;
		if (form->priv->place_holders) {
			g_hash_table_destroy (form->priv->place_holders);
			form->priv->place_holders = NULL;
		}
	}
}

static gchar *
create_text_label_for_sentry (SingleEntry *sentry, gchar **out_title)
{
	gchar *label = NULL;
	g_assert (out_title);
	if (! sentry->group->group->nodes_source) {
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

		label = g_object_get_data (G_OBJECT (sentry->group->group->nodes_source->data_model), "name");
		if (label)
			title = g_strdup (label);
		else {
			GString *tstring = NULL;
			for (params = sentry->group->group->nodes; params; params = params->next) {
				g_object_get (G_OBJECT (GDA_SET_NODE (params->data)->holder),
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
	GtkWidget *entry;
	gboolean editable = TRUE;
	GValue *ref_value = NULL;

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
	if (! group->group->nodes_source) {
		/* there is only one non-constrained parameter */
		GdaHolder *param;
		GType type;
		const GValue *val, *default_val, *value;
		gboolean nnul;
		const gchar *plugin = NULL;
		const GValue *plugin_val;

		g_assert (! group->group->nodes->next); /* only 1 item in the list */

		param = GDA_HOLDER (GDA_SET_NODE (group->group->nodes->data)->holder);
		sentry->single_param = param;
		
		val = gda_holder_get_value (param);
		default_val = gda_holder_get_default_value (param);
		nnul = gda_holder_get_not_null (param);
		sentry->not_null = nnul;

		/* determine initial value */
		type = gda_holder_get_g_type (param);
		value = val;
		if (!value && default_val &&
		    (G_VALUE_TYPE ((GValue *) default_val) == type))
			value = default_val;

		/* create entry */
		plugin_val = gda_holder_get_attribute (param, GDAUI_ATTRIBUTE_PLUGIN);
		if (plugin_val) {
			if (G_VALUE_TYPE (plugin_val) == G_TYPE_STRING)
				plugin = g_value_get_string (plugin_val);
			else
				g_warning (_("The '%s' attribute should be a G_TYPE_STRING value"),
					   GDAUI_ATTRIBUTE_PLUGIN);
		}
		entry = GTK_WIDGET (gdaui_new_data_entry (type, plugin));

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
			  (G_VALUE_TYPE ((GValue *) value) != GDA_TYPE_NULL)))
			gdaui_data_entry_set_reference_value (GDAUI_DATA_ENTRY (entry), value);

		if (default_val) {
			gdaui_data_entry_set_default_value (GDAUI_DATA_ENTRY (entry), default_val);
			gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry),
							 GDA_VALUE_ATTR_CAN_BE_DEFAULT,
							 GDA_VALUE_ATTR_CAN_BE_DEFAULT);
			if (gda_holder_value_is_default (param))
				gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry),
							 GDA_VALUE_ATTR_IS_DEFAULT,
							 GDA_VALUE_ATTR_IS_DEFAULT);
		}

		gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry),
						 nnul ? 0 : GDA_VALUE_ATTR_CAN_BE_NULL,
						 GDA_VALUE_ATTR_CAN_BE_NULL);

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
		gtk_misc_set_alignment (GTK_MISC (sentry->label), 0., 0.);
		gtk_widget_show (sentry->label);
			
		g_object_get (G_OBJECT (param), "description", &title, NULL);
		if (title && *title)
			gtk_widget_set_tooltip_text (sentry->label, title);
		g_free (title);
	}
	else {
		/* several parameters depending on the values of a GdaDataModel object */
		GSList *plist;
		gboolean nullok = TRUE;
			
		entry = gdaui_entry_combo_new (sentry->form->priv->set_info, group->source);

		/* connect to the parameter's changes */
		sentry->group_signals = g_array_new (FALSE, FALSE, sizeof (SignalData));
		for (plist = group->group->nodes; plist; plist = plist->next) {
			GdaHolder *param;

			param = GDA_SET_NODE (plist->data)->holder;
			if (gda_holder_get_not_null (param))
				nullok = FALSE;

			SignalData sd;
			sd.holder = g_object_ref (param);
			sd.signal_id = g_signal_connect (G_OBJECT (param), "changed",
							 G_CALLBACK (parameter_changed_cb),
							 sentry);
			g_array_append_val (sentry->group_signals, sd);
		}
		gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry),
						 nullok ? GDA_VALUE_ATTR_CAN_BE_NULL : 0,
						 GDA_VALUE_ATTR_CAN_BE_NULL);
		sentry->not_null = !nullok;

		/* label */
		gchar *title = NULL;
		gchar *str;

		str = create_text_label_for_sentry (sentry, &title);
		sentry->label = gtk_label_new (str);
		g_free (str);
		g_object_ref_sink (sentry->label);
		sentry->label_title = title;
		gtk_misc_set_alignment (GTK_MISC (sentry->label), 0., 0.);
		gtk_widget_show (sentry->label);
			
		title = g_object_get_data (G_OBJECT (group->group->nodes_source->data_model),
					   "descr");
		if (title && *title)
			gtk_widget_set_tooltip_text (sentry->label, title);

	}
	sentry->entry = GDAUI_DATA_ENTRY (entry);
	g_object_ref_sink (sentry->entry);
	gdaui_data_entry_set_editable (sentry->entry, editable);

	GSList *list;
	for (list = sentry->form->priv->size_groups; list; list = list->next) {
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
	sentry->entry_expand_changed_id = g_signal_connect (sentry->entry, "expand-changed",
							    G_CALLBACK (entry_expand_changed_cb), sentry);

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
	GSList *list;

	/* parameters list management */
	if (!form->priv->set || !form->priv->set_info->groups_list)
		/* nothing to do */
		return;

	/* creating all the data entries, and putting them into the form->priv->entries list */
	for (list = form->priv->set_info->groups_list; list; list = list->next) {
		SingleEntry *sentry;

		sentry = g_new0 (SingleEntry, 1);
		sentry->form = form;
		sentry->forward_param_updates = TRUE;
		form->priv->s_entries = g_slist_append (form->priv->s_entries, sentry);

		sentry->group = GDAUI_SET_GROUP (list->data);
		create_entry_widget (sentry);
	}
 
	/* Set the Show actions in the entries */
	gdaui_basic_form_show_entry_actions (form, form->priv->show_actions);
	/* Set the Auto entries default in the entries */
	gdaui_basic_form_set_entries_auto_default (form, form->priv->entries_auto_default);
}

static void
pack_entry_widget (SingleEntry *sentry)
{
	gboolean expand;
	expand = gdaui_data_entry_can_expand (GDAUI_DATA_ENTRY (sentry->entry), FALSE);
	sentry->can_expand = expand;

	switch (sentry->packing_type) {
	case PACKING_TABLE: {
		/* label for the entry */
		gint i = sentry->pack.table.top;
		GtkTable *table = GTK_TABLE (sentry->pack.table.table);
		GtkWidget *parent;

		if (sentry->label) {
			parent = gtk_widget_get_parent (sentry->label);
			if (parent)
				gtk_container_remove (GTK_CONTAINER (parent), sentry->label);
			gtk_table_attach (table, sentry->label, 0, 1, i, i+1,
					  GTK_FILL | GTK_SHRINK, GTK_SHRINK, 0, 0);
		}
		
		/* add the entry itself to the table */
		parent = gtk_widget_get_parent ((GtkWidget*) sentry->entry);
		if (parent)
			gtk_container_remove (GTK_CONTAINER (parent), (GtkWidget*) sentry->entry);
		gtk_table_attach (table, (GtkWidget*) sentry->entry, 1, 2, i, i+1,
				  GTK_FILL | GTK_EXPAND,
				  expand ? (GTK_FILL | GTK_EXPAND) : GTK_SHRINK, 0, 0);

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
	GtkWidget *table;
	gint i;
	GSList *list;

	unpack_entries (form);
	if (form->priv->top_container) {
		gtk_widget_destroy (form->priv->top_container);
		form->priv->top_container = NULL;
		if (form->priv->place_holders) {
			g_hash_table_destroy (form->priv->place_holders);
			form->priv->place_holders = NULL;
		}
	}

	/* creating a table for all the entries */
	table = gtk_table_new (g_slist_length (form->priv->s_entries), 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), SPACING);
	gtk_table_set_col_spacings (GTK_TABLE (table), SPACING);
	form->priv->top_container = table;
	gtk_box_pack_start (GTK_BOX (form->priv->mainbox), table, TRUE, TRUE, 0);
	for (list = form->priv->s_entries, i = 0;
	     list;
	     list = list->next, i++) {
		SingleEntry *sentry = (SingleEntry *) list->data;

		sentry->packing_type = PACKING_TABLE;
		sentry->pack.table.table = table;
		sentry->pack.table.top = i;

		pack_entry_widget (sentry);
	}
	mark_not_null_entry_labels (form, TRUE);
	gtk_widget_show (table);
}

static GtkWidget *load_xml_layout_children (GdauiBasicForm *form, xmlNodePtr parent_node);
static GtkWidget *load_xml_layout_column (GdauiBasicForm *form, xmlNodePtr box_node);
static GtkWidget *load_xml_layout_section (GdauiBasicForm *form, xmlNodePtr section_node);

static void
pack_entries_in_xml_layout (GdauiBasicForm *form, xmlNodePtr form_node)
{
	GtkWidget *top;

	unpack_entries (form);
	if (form->priv->top_container) {
		gtk_widget_destroy (form->priv->top_container);
		form->priv->top_container = NULL;
		if (form->priv->place_holders) {
			g_hash_table_destroy (form->priv->place_holders);
			form->priv->place_holders = NULL;
		}
	}

	top = load_xml_layout_children (form, form_node);
	gtk_box_pack_start (GTK_BOX (form->priv->mainbox), top, TRUE, TRUE, 0);
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
			top = gtk_hbox_new (FALSE, 0);
			ctype = TOP_BOX;
		}
		if (xmlStrEqual (prop, BAD_CAST "rows")) {
			top = gtk_vbox_new (FALSE, 0);
			ctype = TOP_BOX;
		}
		else if (xmlStrEqual (prop, BAD_CAST "hpaned")) {
			top = gtk_hpaned_new ();
			ctype = TOP_PANED;
		}
		else if (xmlStrEqual (prop, BAD_CAST "vpaned")) {
			top = gtk_vpaned_new ();
			ctype = TOP_PANED;
		}
		else 
			g_warning ("Unknown container type '%s', ignoring", (gchar*) prop);
		xmlFree (prop);
	}
	if (!top)
		top = gtk_hbox_new (FALSE, 0);

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
				gtk_box_pack_start (GTK_BOX (top), wid, TRUE, TRUE, SPACING);
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
	GtkWidget *table;
	table = gtk_table_new (1, 2, FALSE);
	
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
					sentry->pack.table.table = table;
					sentry->pack.table.top = i;

					xmlChar *plugin;
					plugin = xmlGetProp (child, BAD_CAST "plugin");
					if (plugin && sentry->single_param) {
						GValue *value;
						value = gda_value_new_from_string ((gchar*) plugin, G_TYPE_STRING);
						gda_holder_set_attribute_static (sentry->single_param,
										 GDAUI_ATTRIBUTE_PLUGIN, value);
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
						gtk_misc_set_alignment (GTK_MISC (sentry->label), 0., 0.);

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
					gtk_table_attach (GTK_TABLE (table), wid, 0, 1, i, i+1, GTK_SHRINK, 0, 0, 0);
					xmlFree (label);
				}

				if (! form->priv->place_holders)
					form->priv->place_holders = g_hash_table_new_full (g_str_hash, g_str_equal,
											   g_free, g_object_unref);
				ph = gtk_vbox_new (FALSE, 0);
				g_hash_table_insert (form->priv->place_holders, g_strdup ((gchar*) id),
						     g_object_ref_sink ((GObject*)ph));
				gtk_table_attach_defaults (GTK_TABLE (table), ph, 1, 2, i, i+1);
				xmlFree (id);
			}
		}
		else
			g_warning ("Unknown node type '%s', ignoring", (gchar*) child->name);	
	}

	gtk_table_set_row_spacings (GTK_TABLE (table), SPACING);
	gtk_table_set_col_spacings (GTK_TABLE (table), SPACING);
	return table;
}

static GtkWidget *
load_xml_layout_section (GdauiBasicForm *form, xmlNodePtr section_node)
{
	xmlChar *title;
	GtkWidget *hbox, *label, *sub, *retval;

	hbox = gtk_hbox_new (FALSE, 0); /* HIG */
	title = xmlGetProp (section_node, BAD_CAST "title");
	if (title) {
		gchar *str;
		str = g_markup_printf_escaped ("<b>%s</b>", (gchar*) title);
		xmlFree (title);

		label = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_misc_set_alignment (GTK_MISC (label), 0., -1);

		GtkWidget *vbox;
		vbox = gtk_vbox_new (FALSE, 0);
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
	GSList *list;

	for (list = form->priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry*) list->data;
		gchar *str;

		str = _gdaui_utility_markup_title (sentry->label_title, !show_mark || !sentry->not_null);
		if (show_mark && sentry->not_null)
			gtk_label_set_markup (GTK_LABEL (sentry->label), str);
		else
			gtk_label_set_text (GTK_LABEL (sentry->label), str);
		g_free (str);
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
		else if (attr & GDA_VALUE_ATTR_DATA_NON_VALID) {
			gda_holder_force_invalid (param);
			g_signal_emit (G_OBJECT (sentry->form), gdaui_basic_form_signals[HOLDER_CHANGED],
				       0, param, TRUE);
		}
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
		params = group->group->nodes;
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
			param = GDA_SET_NODE (params->data)->holder;
			gda_holder_set_value (param, (GValue *)(list->data), NULL);
			g_signal_emit (G_OBJECT (sentry->form), gdaui_basic_form_signals[HOLDER_CHANGED],
				       0, param, TRUE);
			sentry->forward_param_updates = TRUE;
		}
		g_slist_free (values);

#ifdef PROXY_STORE_EXTRA_VALUES
		/* updating the GdaDataProxy if there is one */
		if (GDA_IS_DATA_MODEL_ITER (sentry->form->priv->set)) {
			GdaDataProxy *proxy;
			gint proxy_row;

			proxy_row = gda_data_model_iter_get_row ((GdaDataModelIter *) sentry->form->priv->set);

			g_object_get (G_OBJECT (sentry->form->priv->set), "data-model", &proxy, NULL);
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
entry_expand_changed_cb (G_GNUC_UNUSED GdauiDataEntry *entry, SingleEntry *sentry)
{
	pack_entry_widget (sentry);
}


/*
 * Called when a parameter changes
 * We emit a "holder-changed" signal only if the 'sentry->forward_param_updates' is TRUE, which means
 * the param change does not come from a GdauiDataEntry change.
 */
static void
parameter_changed_cb (GdaHolder *param, SingleEntry *sentry)
{
	const GValue *value = gda_holder_get_value (param);
	GdauiDataEntry *entry;

	entry = sentry->entry;
	if (sentry->forward_param_updates) {
		gboolean param_valid;
		gboolean default_if_invalid = FALSE;

		/* There can be a feedback from the entry if the param is invalid and "set-default-if-invalid"
		   exists and is TRUE */
		param_valid = gda_holder_is_valid (param);
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

		if (sentry->single_param)
			gdaui_data_entry_set_value (entry, param_valid ? value : NULL);
		else {
			GSList *values = NULL;
			GSList *list;
			gboolean allnull = TRUE;

			for (list = sentry->group->group->nodes; list; list = list->next) {
				const GValue *pvalue;
				pvalue = gda_holder_get_value (GDA_SET_NODE (list->data)->holder);
				values = g_slist_append (values, (GValue *) pvalue);
				if (allnull && pvalue &&
				    (G_VALUE_TYPE ((GValue *) pvalue) != GDA_TYPE_NULL))
					allnull = FALSE;
			}

			if (!allnull)
				gdaui_entry_combo_set_values (GDAUI_ENTRY_COMBO (entry), values);
			else
				gdaui_entry_combo_set_values (GDAUI_ENTRY_COMBO (entry), NULL);

			g_slist_free (values);
		}

		if (! default_if_invalid) {
			g_signal_handler_unblock (G_OBJECT (entry),
						  sentry->entry_contents_modified_id);
			g_signal_handler_unblock (G_OBJECT (entry),
						  sentry->entry_contents_activated_id);
		}		

		gdaui_entry_shell_set_unknown (GDAUI_ENTRY_SHELL (entry),
					       !gda_holder_is_valid (param));

		g_signal_emit (G_OBJECT (sentry->form), gdaui_basic_form_signals[HOLDER_CHANGED], 0,
			       param, FALSE);
	}
	else
		gdaui_entry_shell_set_unknown (GDAUI_ENTRY_SHELL (entry),
					       !gda_holder_is_valid (param));
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

	for (list = form->priv->s_entries; list; list = list->next) {
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

			for (params = sentry->group->group->nodes; params; params = params->next) {
				const GValue *pvalue;
				pvalue = gda_holder_get_value (GDA_SET_NODE (params->data)->holder);
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

	return gda_set_is_valid (form->priv->set, NULL);
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

	return form->priv->set;
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

	for (list = form->priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry*) list->data;
		if (! (gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (sentry->entry)) &
		       GDA_VALUE_ATTR_IS_UNCHANGED))
			return TRUE;
	}
	return FALSE;
}

/*
 * gdaui_basic_form_show_entry_actions
 * @form: a #GdauiBasicForm widget
 * @show_actions: a boolean
 *
 * Show or hide the actions button available at the end of each data entry
 * in the form
 */
static void
gdaui_basic_form_show_entry_actions (GdauiBasicForm *form, gboolean show_actions)
{
	GSList *list;
	guint show;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	show = show_actions ? GDA_VALUE_ATTR_ACTIONS_SHOWN : 0;
	form->priv->show_actions = show_actions;

	for (list = form->priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry*) list->data;
		gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (sentry->entry), show,
						 GDA_VALUE_ATTR_ACTIONS_SHOWN);
		/* mark_not_null_entry_labels (form, show_actions); */
	}
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

	for (list = form->priv->s_entries; list; list = list->next) {
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

		GtkWidget *parent;
		parent = gtk_widget_get_parent ((GtkWidget *) sentry->entry);
		if (parent && GTK_IS_TABLE (parent)) {
			gint row;
			gtk_container_child_get (GTK_CONTAINER (parent), (GtkWidget *) sentry->entry, "top-attach",
						 &row, NULL);
			gtk_table_set_row_spacing (GTK_TABLE (parent), row, show ? SPACING : 0);
		}
	}
}

/**
 * gdaui_basic_form_entry_set_visible:
 * @form: a #GdauiBasicForm widget
 * @param: a #GdaHolder object
 * @show: set to %TRUE to show the data entry, and to %FALSE to hide it
 *
 * Shows or hides the #GdauiDataEntry in @form which corresponds to the
 * @param parameter
 *
 * Since: 4.2
 */
void
gdaui_basic_form_entry_set_visible (GdauiBasicForm *form, GdaHolder *param, gboolean show)
{
	SingleEntry *sentry;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (GDA_IS_HOLDER (param));

	sentry = get_single_entry_for_holder (form, param);
	if (!sentry) {
		g_warning (_("Can't find data entry for GdaHolder"));
		return;
	}

	real_gdaui_basic_form_entry_set_visible (form, sentry, show);
}

/**
 * gdaui_basic_form_entry_grab_focus:
 * @form: a #GdauiBasicForm widget
 * @param: a #GdaHolder object
 *
 * Makes the data entry corresponding to @param grab the focus for the window it's in
 *
 * Since: 4.2
 */
void
gdaui_basic_form_entry_grab_focus (GdauiBasicForm *form, GdaHolder *param)
{
	GtkWidget *entry;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	entry = gdaui_basic_form_get_entry_widget (form, param);

	if (entry)
		gdaui_data_entry_grab_focus (GDAUI_DATA_ENTRY (entry));
}

/**
 * gdaui_basic_form_entry_set_editable:
 * @form: a #GdauiBasicForm widget
 * @holder: (allow-none): a #GdaHolder object; or %NULL
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

	if (holder) {
		SingleEntry *sentry;
		sentry = get_single_entry_for_holder (form, holder);
		if (sentry)
			gdaui_data_entry_set_editable (sentry->entry, editable);
	}
	else {
		GSList *list;
		for (list = form->priv->s_entries; list; list = list->next)
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

	form->priv->entries_auto_default = auto_default;
	for (list = form->priv->s_entries; list; list = list->next) {
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

	for (list = form->priv->s_entries; list; list = list->next) {
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
	GSList *list;
	for (list = form->priv->s_entries; list; list = list->next) {
		SingleEntry *sentry = (SingleEntry *) list->data;
		if (sentry->single_param && (sentry->single_param == param))
			return sentry;
		else if (! sentry->single_param) {
			/* multiple parameters */
			GSList *params;

			for (params = sentry->group->group->nodes; params; params = params->next) {
				if (GDA_SET_NODE (params->data)->holder == (gpointer) param)
					return sentry;
			}
		}
	}
	return NULL;
}

static SingleEntry *
get_single_entry_for_id (GdauiBasicForm *form, const gchar *id)
{
	GSList *list;
	g_return_val_if_fail (id, NULL);

	for (list = form->priv->s_entries; list; list = list->next) {
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
 * @param: a #GdaHolder object
 *
 * Get the #GdauiDataEntry in @form which corresponds to the param parameter.
 *
 * Returns: (transfer none): the requested widget, or %NULL if not found
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_basic_form_get_entry_widget (GdauiBasicForm *form, GdaHolder *param)
{
	SingleEntry *sentry;
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (param), NULL);

	sentry = get_single_entry_for_holder (form, param);
	if (sentry)
		return GTK_WIDGET (sentry->entry);
	else
		return NULL;
}

/**
 * gdaui_basic_form_get_label_widget:
 * @form: a #GdauiBasicForm widget
 * @param: a #GdaHolder object
 *
 * Get the label in @form which corresponds to the param parameter.
 *
 * Returns: (transfer none): the requested widget, or %NULL if not found
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_basic_form_get_label_widget (GdauiBasicForm *form, GdaHolder *param)
{
	SingleEntry *sentry;
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (param), NULL);

	sentry = get_single_entry_for_holder (form, param);
	if (sentry)
		return sentry->label;
	else
		return NULL;
}

/**
 * gdaui_basic_form_new_in_dialog:
 * @data_set: a #GdaSet object
 * @parent: (allow-none): the parent window for the new dialog, or %NULL
 * @title: (allow-none): the title of the dialog window, or %NULL
 * @header: (allow-none): a helper text displayed at the top of the dialog, or %NULL
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
		rtitle = _("Values to be filled");

	dlg = gtk_dialog_new_with_buttons (rtitle, parent,
					   GTK_DIALOG_MODAL,
					   GTK_STOCK_CANCEL,
					   GTK_RESPONSE_REJECT,
					   GTK_STOCK_OK,
					   GTK_RESPONSE_ACCEPT,
					   NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_ACCEPT);
	if (header && *header) {
		GtkWidget *label;
		gchar *str;

		label = gtk_label_new (NULL);
		gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
		str = g_markup_printf_escaped ("<b>%s:</b>", header);
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
#if GTK_CHECK_VERSION(2,18,0)
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
				    label, FALSE, FALSE, SPACING);
#else
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), label, FALSE, FALSE, SPACING);
#endif
		gtk_widget_show (label);
	}


	gboolean can_expand;
	g_object_get ((GObject*) form, "can-expand-v", &can_expand, NULL);
#if GTK_CHECK_VERSION(2,18,0)
	gtk_container_set_border_width (GTK_CONTAINER (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg)))), 4);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), form,
			    can_expand, can_expand, 10);
#else
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), 4);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), form,
			    can_expand, can_expand, 10);
#endif

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

	gchar *file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd", "gdaui-layout.dtd", NULL);
        if (g_file_test (file, G_FILE_TEST_EXISTS))
                dtd = xmlParseDTD (NULL, BAD_CAST file);
        if (dtd == NULL) {
                g_warning (_("'%s' DTD not parsed successfully. "
                             "XML data layout validation will not be "
                             "performed (some errors may occur)"), file);
        }
        g_free (file);

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

	if (! form->priv->place_holders)
		return NULL;
	return g_hash_table_lookup (form->priv->place_holders, placeholder_id);
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

	SizeGroup *sg;
	sg = g_new (SizeGroup, 1);
	sg->size_group = g_object_ref (size_group);
	sg->part = part;
	form->priv->size_groups = g_slist_append (form->priv->size_groups, sg);

	for (list = form->priv->s_entries; list; list = list->next) {
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
	
	SizeGroup *sg;
	for (list = form->priv->size_groups; list; list = list->next) {
		sg = (SizeGroup*) list->data;
		if (sg->size_group == size_group) {
			form->priv->size_groups = g_slist_remove (form->priv->size_groups, sg);
			size_group_free (sg);
			break;
		}
		sg = NULL;
	}

	if (!sg) {
		g_warning (_("size group was not taken into account using gdaui_basic_form_add_to_size_group()"));
		return;
	}

	for (list = form->priv->s_entries; list; list = list->next) {
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
