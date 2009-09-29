/* gdaui-basic-form.c
 *
 * Copyright (C) 2002 - 2009 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda-ui/gdaui-data-widget.h>
#include <libgda-ui/gdaui-raw-form.h>
#include <libgda-ui/gdaui-easy.h>

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

static void layout_spec_free (GdauiFormLayoutSpec *spec);

static void gdaui_basic_form_fill (GdauiBasicForm *form);
static void gdaui_basic_form_clean (GdauiBasicForm *form);

static void get_rid_of_set (GdaSet *paramlist, GdauiBasicForm *form);
static void paramlist_public_data_changed_cb (GdauiSet *paramlist, GdauiBasicForm *form);
static void paramlist_param_attr_changed_cb (GdaSet *paramlist, GdaHolder *param, 
					     const gchar *att_name, const GValue *att_value, GdauiBasicForm *form);

static void entry_contents_modified (GdauiDataEntry *entry, GdauiBasicForm *form);
static void entry_contents_activated (GdauiDataEntry *entry, GdauiBasicForm *form);
static void parameter_changed_cb (GdaHolder *param, GdauiDataEntry *entry);

static void mark_not_null_entry_labels (GdauiBasicForm *form, gboolean show_mark);
enum
{
	PARAM_CHANGED,
	ACTIVATED,
	LAST_SIGNAL
};

/* properties */
enum
{
        PROP_0,
	PROP_LAYOUT_SPEC,
	PROP_DATA_LAYOUT,
	PROP_PARAMLIST,
	PROP_HEADERS_SENSITIVE,
	PROP_SHOW_ACTIONS,
	PROP_ENTRIES_AUTO_DEFAULT
};

struct _GdauiBasicFormPriv
{
	GdaSet                 *set;
	GdauiSet               *set_info;
	gulong                 *signal_ids; /* array of signal ids */

	GSList                 *entries;/* list of GdauiDataEntry widgets */
	GSList                 *not_null_labels;/* list of GtkLabel widgets corresponding to NOT NULL entries */

	GdauiFormLayoutSpec  *layout_spec;
	GtkWidget              *entries_table;
	GtkWidget              *entries_glade;
	GSList                 *hidden_entries;
	GtkScrolledWindow      *scrolled_window;  /* Window child. */

	gboolean                headers_sensitive;
	gboolean                forward_param_updates; /* forward them to the GdauiDataEntry widgets ? */
	gboolean                show_actions;
	gboolean                entries_auto_default;
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
			(GInstanceInitFunc) gdaui_basic_form_init
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
	 * GdauiBasicForm::param-changed:
	 * @form: GdauiBasicForm
	 * @param: that changed
	 * @is_user_modif: TRUE if the modification has been initiated by a user modification
	 *
	 * Emitted when a GdaHolder changes
	 */
	gdaui_basic_form_signals[PARAM_CHANGED] =
		g_signal_new ("param_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdauiBasicFormClass, param_changed),
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

	class->param_changed = NULL;
	class->activated = NULL;
	object_class->dispose = gdaui_basic_form_dispose;

	/* Properties */
        object_class->set_property = gdaui_basic_form_set_property;
        object_class->get_property = gdaui_basic_form_get_property;
	
	g_object_class_install_property (object_class, PROP_LAYOUT_SPEC,
					 g_param_spec_pointer ("layout_spec", 
							       _("Pointer to a GdauiFormLayoutSpec structure"), NULL,
							       G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_DATA_LAYOUT,
					 g_param_spec_pointer ("data_layout", 
							       _("Pointer to an XML data layout specification"), NULL,
							       G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_PARAMLIST,
					 g_param_spec_pointer ("paramlist", 
							       _("List of parameters to show in the form"), NULL,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_HEADERS_SENSITIVE,
					 g_param_spec_boolean ("headers_sensitive",
							       _("Entry headers are sensitive"), 
							       NULL, FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_SHOW_ACTIONS,
					 g_param_spec_boolean ("show_actions",
							       _("Show Entry actions"), 
							       NULL, FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_ENTRIES_AUTO_DEFAULT,
					 g_param_spec_boolean ("entries_auto_default",
							       _("Entries Auto-default"), 
							       NULL, FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gdaui_basic_form_init (GdauiBasicForm * wid)
{
	wid->priv = g_new0 (GdauiBasicFormPriv, 1);
	wid->priv->set = NULL;
	wid->priv->entries = NULL;
	wid->priv->not_null_labels = NULL;
	wid->priv->layout_spec = NULL;
	wid->priv->entries_glade = NULL;
	wid->priv->entries_table = NULL;
	wid->priv->hidden_entries = NULL;
	wid->priv->signal_ids = NULL;

	wid->priv->headers_sensitive = FALSE;
	wid->priv->show_actions = FALSE;
	wid->priv->entries_auto_default = FALSE;

	wid->priv->forward_param_updates = TRUE;
}

static void widget_shown_cb (GtkWidget *wid, GdauiBasicForm *form);

/**
 * gdaui_basic_form_new
 * @paramlist: a #GdaSet structure
 *
 * Creates a new #GdauiBasicForm widget using all the parameters provided in @paramlist.
 *
 * The global layout is rendered using a table (a #GtkTable), and an entry is created for each
 * node of @paramlist.
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_basic_form_new (GdaSet *paramlist)
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_BASIC_FORM, "paramlist", paramlist, NULL);

	return (GtkWidget *) obj;
}

/**
 * gdaui_basic_form_new_custom
 * @paramlist: a #GdaSet structure
 * @glade_file: a Glade XML file name
 * @root_element: the name of the top-most widget in @glade_file to use in the new form
 * @form_prefix: the prefix used to look for widgets to add entries in
 *
 * Creates a new #GdauiBasicForm widget using all the parameters provided in @paramlist.
 *
 * The layout is specified in the @glade_file specification, and an entry is created for each
 * node of @paramlist.
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_basic_form_new_custom (GdaSet *paramlist, const gchar *glade_file, 
				const gchar *root_element, const gchar *form_prefix)
{
	GdauiFormLayoutSpec spec;
	GObject *obj;
#ifdef HAVE_LIBGLADE
	spec.xml_object = NULL;
#endif
	spec.xml_file = (gchar *) glade_file;
	spec.root_element = (gchar *) root_element;
	spec.form_prefix = (gchar *) form_prefix;
	obj = g_object_new (GDAUI_TYPE_BASIC_FORM, "layout_spec", &spec, "paramlist", paramlist, NULL);

	return (GtkWidget *) obj;
}


static void
widget_shown_cb (GtkWidget *wid, GdauiBasicForm *form)
{
	if (g_slist_find (form->priv->hidden_entries, wid)) {
		if (form->priv->entries_table && g_slist_find (form->priv->entries, wid)) {
			gint row = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (wid), "row_no"));
			gtk_table_set_row_spacing (GTK_TABLE (form->priv->entries_table), row, 0);
		}
		
		gtk_widget_hide (wid);
	}
}

static void
get_rid_of_set (GdaSet *paramlist, GdauiBasicForm *form)
{
	GSList *list;
	gint i;

	g_assert (paramlist == form->priv->set);

	/* disconnect from parameters */
	for (list = form->priv->set->holders, i = 0;
	     list;
	     list = list->next, i++)
		g_signal_handler_disconnect (G_OBJECT (list->data), form->priv->signal_ids[i]);
	g_free (form->priv->signal_ids);
	form->priv->signal_ids = NULL;

	/* unref the paramlist */
	g_signal_handlers_disconnect_by_func (form->priv->set_info,
					      G_CALLBACK (paramlist_public_data_changed_cb), form);
	g_signal_handlers_disconnect_by_func (paramlist,
					      G_CALLBACK (paramlist_param_attr_changed_cb), form);

	g_object_unref (form->priv->set);
	form->priv->set = NULL;

	if (form->priv->set_info) {
		g_object_unref (form->priv->set_info);
		form->priv->set_info = NULL;
	}

	/* render all the entries non sensitive */
	list = form->priv->entries;
	while (list) {
		gdaui_data_entry_set_editable (GDAUI_DATA_ENTRY (list->data), FALSE);
		list = g_slist_next (list);
	}
}

static void
paramlist_public_data_changed_cb (GdauiSet *paramlist, GdauiBasicForm *form)
{
	/* here we want to re-define all the data entry widgets */
	gdaui_basic_form_clean (form);
	gdaui_basic_form_fill (form);
}

static void
paramlist_param_attr_changed_cb (GdaSet *paramlist, GdaHolder *param, 
				 const gchar *att_name, const GValue *att_value, GdauiBasicForm *form)
{
	GtkWidget *entry;

	if (!strcmp (att_name, GDA_ATTRIBUTE_IS_DEFAULT)) {
		entry = gdaui_basic_form_get_entry_widget (form, param);
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
							 G_CALLBACK (entry_contents_modified), form);
			gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry), attrs, mask);
			g_signal_handlers_unblock_by_func (G_OBJECT (entry),
							   G_CALLBACK (entry_contents_modified), form);
		}
	}
	else if (!strcmp (att_name, GDAUI_ATTRIBUTE_PLUGIN)) {
		/* TODO: be more specific and change only the cell renderer corresponding to @param */

		/* keep a list of hidden columns */
		GSList *list, *hidden_params = NULL;
		for (list = form->priv->hidden_entries; list; list = list->next) {
			GdaHolder *param = g_object_get_data (G_OBJECT (list->data), "param");
			if (param)
				hidden_params = g_slist_prepend (hidden_params, param);
			else {
				/* multiple parameters, take the 1st param */
				GdauiSetGroup *group;
				group = g_object_get_data (G_OBJECT (list->data), "__gdaui_group");
				hidden_params = g_slist_prepend (hidden_params, GDA_SET_NODE (group->group->nodes->data)->holder);
			}
		}

		/* re-create entries */
		paramlist_public_data_changed_cb (form->priv->set_info, form);

		/* hide entries which were hidden */
		for (list = hidden_params; list; list = list->next) 
			gdaui_basic_form_entry_show (form, GDA_HOLDER (list->data), FALSE);
		g_slist_free (hidden_params);
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

		gdaui_basic_form_clean (form);

		/* the private area itself */
		g_free (form->priv);
		form->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

static void
load_xml_data_layout_button (GdauiBasicForm  *form,
			     xmlNodePtr         node,
			     gpointer           data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (data && GTK_IS_TABLE(data));

	gchar *title = NULL;
	gchar *script = NULL;
	gint sequence = 0;

	xmlChar *str;
	str = xmlGetProp (node, "title");
	if (str) {
		title = g_strdup (str);
		g_print ("title: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "script");
	if (str) {
		script = g_strdup (str);
		g_print ("script: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "sequence");
	if (str) {
		sequence = atoi (str);
		g_print ("sequence: %s\n", str);
		xmlFree (str);
	}

	GtkButton *button = (GtkButton *) gtk_button_new_with_mnemonic (title);
	gtk_widget_show (GTK_WIDGET(button));

	gint n_columns, n_rows;
	g_object_get (G_OBJECT(data), "n-columns", &n_columns, NULL);
	g_object_get (G_OBJECT(data), "n-rows", &n_rows, NULL);

	gint col, row;
	col = 2 * ((sequence - 1) / n_rows);
	row = (sequence - 1) % n_rows;

	gtk_table_attach (GTK_TABLE(data), GTK_WIDGET(button),
			  col, col + 2, row, row + 1,
			  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
			  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND), 0, 0);

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "script")) {
			// load_data_layout_button_script (table, child);
		}
	}
}

static void
load_xml_data_layout_item (GdauiBasicForm  *form,
			   xmlNodePtr         node,
			   gpointer           data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (data && GTK_IS_TABLE(data));

	gchar *name = NULL;
	gint sequence = 0;
	gboolean editable = FALSE;
	gboolean sort_ascending = FALSE;

	xmlChar *str;
	str = xmlGetProp (node, "name");
	if (str) {
		name = g_strdup (str);
		g_print ("name: %s\n", str);
		xmlFree (str);
	}

	/* str = xmlGetProp (node, "relationship"); */
	/* if (str) { */
	/* 	g_print ("relationship: %s\n", str); */
	/* 	xmlFree (str); */
	/* } */

	/* str = xmlGetProp (node, "related_relationship"); */
	/* if (str) { */
	/* 	g_print ("related_relationship: %s\n", str); */
	/* 	xmlFree (str); */
	/* } */

	str = xmlGetProp (node, "sequence");
	if (str) {
		sequence = atoi (str);
		g_print ("sequence: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "editable");
	if (str) {
		editable = (*str == 't' || *str == 'T') ? TRUE : FALSE;
		g_print ("editable: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "sort_ascending");
	if (str) {
		sort_ascending = (*str == 't' || *str == 'T') ? TRUE : FALSE;
		g_print ("sort_ascending: %s\n", str);
		xmlFree (str);
	}

	/* GSList *slist = form->priv->set->holders; */
	/* while (slist != NULL) { */
	/* 	GdaHolder *holder = slist->data; */
	/* 	g_print ("SET HOLDER=%s\n", gda_holder_get_id (holder)); */
	/* 	slist = g_slist_next (slist); */
	/* } */

	gint n_columns, n_rows;
	g_object_get (G_OBJECT(data), "n-columns", &n_columns, NULL);
	g_object_get (G_OBJECT(data), "n-rows", &n_rows, NULL);

	gint col, row;
	col = 2 * ((sequence - 1) / n_rows);
	row = (sequence - 1) % n_rows;

	GdaHolder *holder = gda_set_get_holder (form->priv->set, name);
	g_return_if_fail (holder != NULL);

	/* const gchar *id = gda_holder_get_id (holder); */

	/* const gchar *text; */
	/* const GValue *value = gda_holder_get_attribute (holder, GDA_ATTRIBUTE_DESCRIPTION); */
	/* if (value != NULL && G_VALUE_HOLDS(value, G_TYPE_STRING)) */
	/* 	text = g_value_get_string (value); */
	/* else */
	/* 	text = id; */
	const gchar *text = NULL;
	if (GDAUI_IS_RAW_FORM (form)) {
		GdaDataModel *model = gdaui_data_widget_get_gda_model (GDAUI_DATA_WIDGET (form));
		if (model && GDA_IS_DATA_SELECT (model))
			text = gda_utility_data_model_find_column_description (GDA_DATA_SELECT (model), name);
	}
	if (! text)
		text = gda_holder_get_id (holder);

	GtkLabel *label = GTK_LABEL(gtk_label_new (text));
	gtk_widget_show (GTK_WIDGET(label));
	gtk_table_attach (GTK_TABLE(data), GTK_WIDGET(label),
			  col, col + 1, row, row + 1,
			  (GtkAttachOptions) GTK_FILL,
			  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND), 0, 0);
	gtk_misc_set_alignment (GTK_MISC(label), 0, /* 0 */ 0.5);

	GtkAlignment *alignment = GTK_ALIGNMENT(gtk_alignment_new (0.5, 0.5, 1, 1));
	gtk_widget_show (GTK_WIDGET(alignment));
	gtk_table_attach (GTK_TABLE(data), GTK_WIDGET(alignment),
			  col + 1, col + 2, row, row + 1,
			  (GtkAttachOptions) (GTK_EXPAND|GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND|GTK_FILL), 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT(alignment), 0, 0, 12, 0);

	GtkHBox *hbox = GTK_HBOX(gtk_hbox_new (FALSE, /* 0 */ 6));
	gtk_widget_show (GTK_WIDGET(hbox));
	gtk_container_add (GTK_CONTAINER(alignment), GTK_WIDGET(hbox));

	// name hbox (both name and id are equals)
	gtk_widget_set_name (GTK_WIDGET(hbox), name);

	g_free (name);

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		/* if (child->type == XML_ELEMENT_NODE && */
		/*     !xmlStrcmp (child->name, (const xmlChar *) "formatting")) { */
		/* } */

		/* if (child->type == XML_ELEMENT_NODE && */
		/*     !xmlStrcmp (child->name, (const xmlChar *) "title_custom")) { */
		/* } */
	}
}

static gint
count_items (xmlNodePtr  node)
{
	gint n = 0;

	g_return_val_if_fail (node->type == XML_ELEMENT_NODE &&
			      (!xmlStrcmp (node->name, (const xmlChar *) "data_layout_group") ||
			       !xmlStrcmp (node->name, (const xmlChar *) "data_layout_portal") ||
			       !xmlStrcmp (node->name, (const xmlChar *) "data_layout_notebook")), -1);

	if (node->children) {
		xmlNodePtr child;
		child = node->children;
		while (child) {
			if (child->type == XML_ELEMENT_NODE &&
			    (!xmlStrcmp (child->name, (const xmlChar *) "data_layout_group") ||
			     !xmlStrcmp (child->name, (const xmlChar *) "data_layout_item") ||
			     !xmlStrcmp (child->name, (const xmlChar *) "data_layout_portal") ||
			     !xmlStrcmp (node->name, (const xmlChar *) "data_layout_notebook") ||
			     !xmlStrcmp (child->name, (const xmlChar *) "data_layout_button"))) {
				n++;
			}
			child = child->next;
		}
	}

	return n;
}

static void
load_xml_data_layout_portal (GdauiBasicForm  *form,
			     xmlNodePtr         node,
			     gpointer           data)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	gchar *name = NULL;
	gchar *relationship = NULL;
	gint sequence = 0;
	gboolean hidden = FALSE;
	gint columns_count = 1;

	xmlChar *str;
	str = xmlGetProp (node, "name");
	if (str) {
		name = g_strdup (str);
		g_print ("name: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "relationship");
	if (str) {
		relationship = g_strdup (str);
		g_print ("relationship: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "sequence");
	if (str) {
		sequence = atoi (str);
		g_print ("sequence: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "hidden");
	if (str) {
		hidden = *str == 't' || *str == 'T' ? TRUE : FALSE;
		g_print ("hidden: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "columns_count");
	if (str) {
		columns_count = atoi (str);
		g_print ("columns_count: %s\n", str);
		xmlFree (str);
	}

	GtkWidget *vbox;
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);

	gint n_columns, n_rows;
	g_object_get (G_OBJECT(data), "n-columns", &n_columns, NULL);
	g_object_get (G_OBJECT(data), "n-rows", &n_rows, NULL);

	gint col, row;
	col = 2 * ((sequence - 1) / n_rows);
	row = (sequence - 1) % n_rows;

	gtk_table_attach (GTK_TABLE (data), vbox,
			  col, col + 2, row, row + 1,
			  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
			  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND), 0, 0);

	gtk_widget_set_name (vbox, name);

	if (!xmlStrcmp (node->parent->name, (const xmlChar *) "data_layout_group")) {


		gint n_columns, n_rows;
		g_object_get (G_OBJECT(data), "n-columns", &n_columns, NULL);
		g_object_get (G_OBJECT(data), "n-rows", &n_rows, NULL);

		gint col, row;
		col = 2 * ((sequence - 1) / n_rows);
		row = (sequence - 1) % n_rows;

		gtk_table_attach (GTK_TABLE(data), GTK_WIDGET(vbox),
				  col, col + 2, row, row + 1,
				  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
				  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND), 0, 0);
	} else
	if (!xmlStrcmp (node->parent->name, (const xmlChar *) "data_layout_notebook")) {

		GtkLabel *label;
		gchar *markup = g_strdup_printf ("<b>%s</b>", (name != NULL) ? name : "");
		label = GTK_LABEL(gtk_label_new (markup));
		g_free (markup);
		gtk_widget_show (GTK_WIDGET(label));
		gtk_label_set_use_markup (label, TRUE);

		gtk_container_add (GTK_CONTAINER(data), GTK_WIDGET(vbox));

		gtk_notebook_set_tab_label (GTK_NOTEBOOK(data),
					    gtk_notebook_get_nth_page
					    (GTK_NOTEBOOK(data), sequence - 1),
					    GTK_WIDGET(label));
	}

	g_free (name);
	g_free (relationship);
}

static void
load_xml_data_layout_group (GdauiBasicForm  *form,
			    xmlNodePtr         node,
			    gpointer           data);

static void
load_xml_data_layout_notebook (GdauiBasicForm  *form,
			       xmlNodePtr         node,
			       gpointer           data)
{
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	gchar *name = NULL;
	gint sequence = 0;
	gchar *title = NULL;

	xmlChar *str;
	str = xmlGetProp (node, "name");
	if (str) {
		g_print ("name: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "title");
	if (str) {
		g_print ("title: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "sequence");
	if (str) {
		sequence = atoi (str);
		g_print ("sequence: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "columns_count");
	if (str) {
		g_print ("columns_count: %s\n", str);
		xmlFree (str);
	}

	/* GtkLabel *label; */
	/* gchar *markup = g_strdup_printf ("<b>%s</b>", (title != NULL) ? title : ""); */
	/* label = GTK_LABEL(gtk_label_new (markup)); */
	/* g_free (markup); */
	/* gtk_widget_show (GTK_WIDGET(label)); */
	/* gtk_label_set_use_markup (label, TRUE); */

	GtkWidget *notebook;
	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);

	gint n_columns, n_rows;
	g_object_get (G_OBJECT(data), "n-columns", &n_columns, NULL);
	g_object_get (G_OBJECT(data), "n-rows", &n_rows, NULL);

	gint col, row;
	col = 2 * ((sequence - 1) / n_rows);
	row = (sequence - 1) % n_rows;

	gtk_table_attach (GTK_TABLE(data), notebook,
			  col, col + 2, row, row + 1,
			  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
			  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND), 0, 0);

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_group")) {
			load_xml_data_layout_group (form, child, /* data */ notebook);
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_portal")) {
			load_xml_data_layout_portal (form, child, /* data */ notebook);
		}
	}

	g_free (name);
	g_free (title);
}

static void
load_xml_data_layout_group (GdauiBasicForm  *form,
			    xmlNodePtr         node,
			    gpointer           data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (form->priv->scrolled_window != NULL);

	gchar *name = NULL;
	gint sequence = 0;
	gint columns_count = 1;
	gchar *title = NULL;

	xmlChar *str;
	str = xmlGetProp (node, "name");
	if (str) {
		name = g_strdup (str);
		g_print ("name: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "sequence");
	if (str) {
		sequence = atoi (str);
		g_print ("sequence: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "columns_count");
	if (str) {
		columns_count = atoi (str);
		g_print ("columns_count: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "title");
	if (str) {
		title = g_strdup (str);
		g_print ("title: %s\n", str);
		xmlFree (str);
	}

	GtkLabel *label;
	label = GTK_LABEL(gtk_label_new (NULL));
	gtk_widget_show (GTK_WIDGET(label));

	gint n = count_items (node);

	gint cols, rows;
	cols = 2 * columns_count;
	rows = n / columns_count + n % columns_count;

	GtkTable *table;
	table = GTK_TABLE(gtk_table_new (rows, cols, FALSE));
	gtk_widget_show (GTK_WIDGET(table));

	gtk_container_set_border_width (GTK_CONTAINER(table), 6);
	gtk_table_set_row_spacings (table, 3);
	gtk_table_set_col_spacings (table, 3);

	if (!xmlStrcmp (node->parent->name, (const xmlChar *) "data_layout_groups")) {

		GtkFrame *frame = GTK_FRAME(gtk_frame_new (NULL));
		gtk_widget_show (GTK_WIDGET(frame));
		gtk_frame_set_shadow_type (frame, GTK_SHADOW_NONE);

		GtkAlignment *alignment = GTK_ALIGNMENT(gtk_alignment_new (0.5, 0.5, 1, 1));
		gtk_widget_show (GTK_WIDGET(alignment));
		gtk_container_add (GTK_CONTAINER(frame), GTK_WIDGET(alignment));
		gtk_alignment_set_padding (alignment, 0, 0, 12, 0);

		gtk_container_add (GTK_CONTAINER(alignment), GTK_WIDGET(table));

		gchar *markup = g_strdup_printf ("<b>%s</b>", (title != NULL) ? title : "");
		gtk_label_set_text (label, markup);
		g_free (markup);
		gtk_label_set_use_markup (label, TRUE);

		gtk_frame_set_label_widget (frame, GTK_WIDGET(label));


		gtk_box_pack_start (GTK_BOX(data), GTK_WIDGET(frame), FALSE, TRUE, 0);
	} else
	if (!xmlStrcmp (node->parent->name, (const xmlChar *) "data_layout_group")) {

		GtkFrame *frame = GTK_FRAME(gtk_frame_new (NULL));
		gtk_widget_show (GTK_WIDGET(frame));
		gtk_frame_set_shadow_type (frame, GTK_SHADOW_NONE);

		GtkAlignment *alignment = GTK_ALIGNMENT(gtk_alignment_new (0.5, 0.5, 1, 1));
		gtk_widget_show (GTK_WIDGET(alignment));
		gtk_container_add (GTK_CONTAINER(frame), GTK_WIDGET(alignment));
		gtk_alignment_set_padding (alignment, 0, 0, 12, 0);

		gtk_container_add (GTK_CONTAINER(alignment), GTK_WIDGET(table));

		gchar *markup = g_strdup_printf ("<b>%s</b>", (title != NULL) ? title : "");
		gtk_label_set_text (label, markup);
		g_free (markup);
		gtk_label_set_use_markup (label, TRUE);

		gtk_frame_set_label_widget (frame, GTK_WIDGET(label));


		gint n_columns, n_rows;
		g_object_get (G_OBJECT(data), "n-columns", &n_columns, NULL);
		g_object_get (G_OBJECT(data), "n-rows", &n_rows, NULL);

		gint col, row;
		col = 2 * ((sequence - 1) / n_rows);
		row = (sequence - 1) % n_rows;

		gtk_table_attach (GTK_TABLE(data), GTK_WIDGET(frame),
				  col, col + 2, row, row + 1,
				  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
				  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND), 0, 0);
	} else
	if (!xmlStrcmp (node->parent->name, (const xmlChar *) "data_layout_notebook")) {

		gchar *text = g_strdup ((title != NULL) ? title : "");
		gtk_label_set_text (label, text);
		g_free (text);

		gtk_container_add (GTK_CONTAINER(data), GTK_WIDGET(table));

		gtk_notebook_set_tab_label (GTK_NOTEBOOK(data),
					    gtk_notebook_get_nth_page
					    (GTK_NOTEBOOK(data), sequence - 1),
					    GTK_WIDGET(label));
	}

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_group")) {
			load_xml_data_layout_group (form, child, /* data */ table);
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_item")) {
			load_xml_data_layout_item (form, child, /* data */ table);
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_portal")) {
			load_xml_data_layout_portal (form, child, /* data */ table);
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_notebook")) {
			load_xml_data_layout_notebook (form, child, /* data */ table);
		}
		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "trans_set")) {
			/* load_data_layout_group_trans_set (form, child, data); */
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_button")) {
			load_xml_data_layout_button (form, child, /* data */ table);
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_item_groupby")) {
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_item_header")) {
		}
	}

	g_free (name);
	g_free (title);
}

static void
load_xml_data_layout_groups (GdauiBasicForm  *form,
			     xmlNodePtr         node,
			     gpointer           data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	g_return_if_fail (form->priv->scrolled_window == NULL);

	form->priv->scrolled_window = GTK_SCROLLED_WINDOW
		(gtk_scrolled_window_new (NULL, NULL));
	gtk_scrolled_window_set_policy (form->priv->scrolled_window,
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_border_width (GTK_CONTAINER(form->priv->scrolled_window), 6);
	gtk_widget_show (GTK_WIDGET(form->priv->scrolled_window));

	GtkVBox *vbox = GTK_VBOX(gtk_vbox_new (FALSE, 0));
	gtk_widget_show (GTK_WIDGET(vbox));

	gtk_scrolled_window_add_with_viewport (form->priv->scrolled_window,
					       (GtkWidget *) vbox);
	/* gtk_box_pack_start (GTK_BOX(form), (GtkWidget *) form->priv->scrolled_window, */
	/* 		    TRUE, TRUE, 0); */

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_group")) {
			load_xml_data_layout_group (form, child, /* data */ vbox);
		}
	}

}

static void
load_xml_data_layout (GdauiBasicForm  *form,
		      xmlNodePtr         node,
		      gpointer           data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	gchar *parent_table = NULL;
	gchar *name = NULL;

	xmlChar *str;
	str = xmlGetProp (node, "parent_table");
	if (str) {
		parent_table = g_strdup (str);
		g_print ("parent_table: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "name");
	if (str) {
		name = g_strdup (str);
		g_print ("name: %s\n", str);
		xmlFree (str);
	}

	// Don't recurse unnecessarily
	gboolean retval = FALSE;
	if (strcmp ((const gchar *) data, parent_table) != 0 ||
	    strcmp ("details", name) != 0) {
		retval = TRUE;
	}
	g_free (parent_table);
	g_free (name);
	if (retval)
		return;

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_groups")) {
			load_xml_data_layout_groups (form, child, data);
		}
	}

}

static void
load_xml_data_layouts (GdauiBasicForm  *form,
		       xmlNodePtr         node,
		       gpointer           data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout")) {
			load_xml_data_layout (form, child, data);
		}
	}
}

/* static void */
/* load_xml_table (GdauiBasicForm  *form, */
/* 		xmlNodePtr         node, */
/* 		gpointer           data) */
/* { */
/* 	g_print ("%s:\n", __func__); */

/* 	xmlChar *str; */
/* 	str = xmlGetProp (node, "name"); */
/* 	if (str) { */
/* 		g_print ("name: %s\n", str); */
/* 		xmlFree (str); */
/* 	} */

/* 	/\* str = xmlGetProp (node, "title"); *\/ */
/* 	/\* if (str) { *\/ */
/* 	/\* 	g_print ("title: %s\n", str); *\/ */
/* 	/\* 	xmlFree (str); *\/ */
/* 	/\* } *\/ */

/* 	/\* str = xmlGetProp (node, "hidden"); *\/ */
/* 	/\* if (str) { *\/ */
/* 	/\* 	g_print ("hidden: %s\n", str); *\/ */
/* 	/\* 	xmlFree (str); *\/ */
/* 	/\* } *\/ */

/* 	/\* str = xmlGetProp (node, "default"); *\/ */
/* 	/\* if (str) { *\/ */
/* 	/\* 	g_print ("default: %s\n", str); *\/ */
/* 	/\* 	xmlFree (str); *\/ */
/* 	/\* } *\/ */

/* 	xmlNodePtr child; */
/* 	for (child = node->children; child != NULL; child = child->next) { */

/* 		/\* if (child->type == XML_ELEMENT_NODE && *\/ */
/* 		/\*     !xmlStrcmp (child->name, (const xmlChar *) "fields")) { *\/ */

/* 		/\* 	load_xml_fields (table, child, data); *\/ */
/* 		/\* } *\/ */

/* 		/\* if (child->type == XML_ELEMENT_NODE && *\/ */
/* 		/\*     !xmlStrcmp (child->name, (const xmlChar *) "relationships")) { *\/ */

/* 		/\* 	load_xml_relationships (table, child, data); *\/ */
/* 		/\* } *\/ */

/* 		if (child->type == XML_ELEMENT_NODE && */
/* 		    !xmlStrcmp (child->name, (const xmlChar *) "data_layouts")) { */

/* 			load_xml_data_layouts (form, child, data); */
/* 		} */

/* 		/\* if (child->type == XML_ELEMENT_NODE && *\/ */
/* 		/\*     !xmlStrcmp (child->name, (const xmlChar *) "reports")) { *\/ */

/* 		/\* 	load_xml_reports (table, child, data); *\/ */
/* 		/\* } *\/ */

/* 		/\* if (child->type == XML_ELEMENT_NODE && *\/ */
/* 		/\*     !xmlStrcmp (child->name, (const xmlChar *) "trans_set")) { *\/ */

/* 		/\* 	load_xml_table_trans_set (table, child, data); *\/ */
/* 		/\* } *\/ */
/* 	} */

/* } */

static void
gdaui_basic_form_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	GdauiBasicForm *form;
#ifdef HAVE_LIBGLADE
	GdauiFormLayoutSpec *lspec, *new_spec = NULL;
#endif

        form = GDAUI_BASIC_FORM (object);
        if (form->priv) {
                switch (param_id) {
		case PROP_LAYOUT_SPEC:
#ifdef HAVE_LIBGLADE
			lspec = g_value_get_pointer (value);
			if (lspec) {
				g_return_if_fail (lspec->xml_file || lspec->xml_object);
				g_return_if_fail (lspec->root_element);
				
				/* spec copy */
				new_spec = g_new0 (GdauiFormLayoutSpec, 1);
				if (lspec->xml_file)
					new_spec->xml_file = g_strdup (lspec->xml_file);
				if (lspec->xml_object) {
					new_spec->xml_object = lspec->xml_object;
					g_object_ref (new_spec->xml_object);
				}
				if (lspec->root_element)
					new_spec->root_element = g_strdup (lspec->root_element);
				if (lspec->form_prefix)
					new_spec->form_prefix = g_strdup (lspec->form_prefix);
				
				/* spec verify */
				if (!new_spec->xml_object) {
					new_spec->xml_object = glade_xml_new (new_spec->xml_file, new_spec->root_element, NULL);
					if (! new_spec->xml_object) {
						layout_spec_free (new_spec);
						g_warning (_("Could not load file '%s'"), new_spec->xml_file);
						return;
					}
				}
			}

			gdaui_basic_form_clean (form);
			if (new_spec) {
				form->priv->layout_spec = new_spec;
				g_print ("Loaded Glade file, reinit interface\n");
			}
			gdaui_basic_form_fill (form);
#else
			g_warning (_("Libglade support not built."));
#endif
			break;
		case PROP_DATA_LAYOUT:
			{
				xmlNodePtr node = g_value_get_pointer (value);

				gdaui_basic_form_clean (form);

				xmlNodePtr child;
				for (child = node->children; child != NULL; child = child->next) {

					if (child->type == XML_ELEMENT_NODE &&
					    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_groups")) {
						load_xml_data_layout_groups (form, child, NULL);
					}
				}

				if (form->priv->scrolled_window != NULL) {
					g_print ("Loaded XML file, reinit interface\n");
				}
				gdaui_basic_form_fill (form);
	
			}
			break;
		case PROP_PARAMLIST:
			if (form->priv->set) {
#ifdef HAVE_LIBGLADE
			new_spec = NULL;
			if (form->priv->layout_spec) {
				/* old spec */
				lspec = form->priv->layout_spec;
				/* spec copy */
				new_spec = g_new0 (GdauiFormLayoutSpec, 1);
				if (lspec->xml_file)
					new_spec->xml_file = g_strdup (lspec->xml_file);
				if (lspec->xml_object) {
					new_spec->xml_object = lspec->xml_object;
					g_object_ref (new_spec->xml_object);
				}
				if (lspec->root_element)
					new_spec->root_element = g_strdup (lspec->root_element);
				if (lspec->form_prefix)
					new_spec->form_prefix = g_strdup (lspec->form_prefix);
			}
#endif
#ifdef HAVE_LIBGLADE
			new_spec = NULL;
			if (form->priv->layout_spec) {
				/* old spec */
				lspec = form->priv->layout_spec;
				/* spec copy */
				new_spec = g_new0 (GdauiFormLayoutSpec, 1);
				if (lspec->xml_file)
					new_spec->xml_file = g_strdup (lspec->xml_file);
				if (lspec->xml_object) {
					new_spec->xml_object = lspec->xml_object;
					g_object_ref (new_spec->xml_object);
				}
				if (lspec->root_element)
					new_spec->root_element = g_strdup (lspec->root_element);
				if (lspec->form_prefix)
					new_spec->form_prefix = g_strdup (lspec->form_prefix);
			}
#endif
				get_rid_of_set (form->priv->set, form);
				gdaui_basic_form_clean (form);
			}

			form->priv->set = g_value_get_pointer (value);
			if (form->priv->set) {
				g_return_if_fail (GDA_IS_SET (form->priv->set));
								
				g_object_ref (form->priv->set);
				form->priv->set_info = gdaui_set_new (GDA_SET (form->priv->set));

				g_signal_connect (form->priv->set_info, "public_data_changed",
						  G_CALLBACK (paramlist_public_data_changed_cb), form);
				g_signal_connect (form->priv->set, "holder-attr-changed",
						  G_CALLBACK (paramlist_param_attr_changed_cb), form);

#ifdef HAVE_LIBGLADE
				if (new_spec)
					form->priv->layout_spec = new_spec;
				new_spec = NULL;
#endif
				gdaui_basic_form_fill (form);
			}
			break;
		case PROP_HEADERS_SENSITIVE:
			form->priv->headers_sensitive = g_value_get_boolean (value);
			break;
		case PROP_SHOW_ACTIONS:
			gdaui_basic_form_show_entry_actions(form, g_value_get_boolean(value));
			break;
		case PROP_ENTRIES_AUTO_DEFAULT:
			gdaui_basic_form_set_entries_auto_default(form, g_value_get_boolean(value));
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
			g_value_set_boolean(value, form->priv->show_actions);
			break;
		case PROP_ENTRIES_AUTO_DEFAULT:
			g_value_set_boolean(value, form->priv->entries_auto_default);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }	
}

static void
gdaui_basic_form_clean (GdauiBasicForm *form)
{
	GSList *list;
	gint i = 0;

	if (form->priv->set) {
		g_assert (form->priv->signal_ids);
		for (i = 0, list = form->priv->set->holders; 
		     list; 
		     i++, list = list->next) {
			if (form->priv->signal_ids[i] > 0)
				g_signal_handler_disconnect (G_OBJECT (list->data), form->priv->signal_ids[i]);
		}
		g_free (form->priv->signal_ids);
		form->priv->signal_ids = NULL;
	}

	/* destroy all the widgets */
	while (form->priv->entries)
		/* destroy any remaining widget */
		gtk_widget_destroy (GTK_WIDGET (form->priv->entries->data));

	if (form->priv->entries_table) {
		gtk_widget_destroy (form->priv->entries_table);
		form->priv->entries_table = NULL;
	}
	if (form->priv->entries_glade) {
		gtk_widget_destroy (form->priv->entries_glade);
		form->priv->entries_glade = NULL;
	}
	if (form->priv->layout_spec) {
		layout_spec_free (form->priv->layout_spec);
		form->priv->layout_spec = NULL;
	}
	
	g_slist_free (form->priv->not_null_labels);
	form->priv->not_null_labels = NULL;

	g_slist_free (form->priv->hidden_entries);
	form->priv->hidden_entries = NULL;

	if (form->priv->scrolled_window) {
		gtk_widget_destroy (GTK_WIDGET(form->priv->scrolled_window));
		form->priv->scrolled_window = NULL;
	}
}

static void
layout_spec_free (GdauiFormLayoutSpec *spec)
{
#ifdef HAVE_LIBGLADE
	if (spec->xml_object)
		g_object_unref (spec->xml_object);
#endif
	g_free (spec->xml_file);
	g_free (spec->root_element);
	g_free (spec->form_prefix);
	g_free (spec);
}

static void entry_destroyed_cb (GtkWidget *entry, GdauiBasicForm *form);
static void label_destroyed_cb (GtkWidget *label, GdauiBasicForm *form);

static void
find_hbox (GtkWidget  *widget,
	   gpointer    data)
{
	gpointer *d = data;

	if (GTK_IS_CONTAINER(widget)) {
		const gchar *name = gtk_widget_get_name (widget);
		if (GTK_IS_HBOX(widget) &&
		    strcmp (name, (gchar *) *d) == 0)
			*(++d) = widget;
		else
			gtk_container_foreach (GTK_CONTAINER(widget),
					       (GtkCallback) find_hbox, data);
	}
}

/*
 * create the entries in the widget
 */
static void 
gdaui_basic_form_fill (GdauiBasicForm *form)
{
	GSList *list;
	gint i;
	gboolean form_expand = FALSE;
	
	/* parameters list management */
	if (!form->priv->set || !form->priv->set_info->groups_list)
		/* nothing to do */
		return;

	/* allocating space for the signal ids and connect to the parameter's changes */
	form->priv->signal_ids = g_new0 (gulong, g_slist_length (form->priv->set->holders));
	i = 0;

	/* creating all the data entries, and putting them into the form->priv->entries list */
	for (list = form->priv->set_info->groups_list; list; list = list->next) {
		GdauiSetGroup *group;
		GtkWidget *entry = NULL;

		group = GDAUI_SET_GROUP (list->data);
		if (! group->group->nodes_source) { 
			/* there is only one non-constrained parameter */
			GdaHolder *param;
			GType type;
			const GValue *val, *default_val, *value;
			gboolean nnul;
			const gchar *plugin = NULL;
			const GValue *plugin_val;

			g_assert (g_slist_length (group->group->nodes) == 1);

			param = GDA_HOLDER (GDA_SET_NODE (group->group->nodes->data)->holder);

			val = gda_holder_get_value (param);
			default_val = gda_holder_get_default_value (param);
			nnul = gda_holder_get_not_null (param);

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
			gdaui_data_entry_set_value (GDAUI_DATA_ENTRY (entry), val);

			if (!nnul ||
			    (nnul && value && 
			     (G_VALUE_TYPE ((GValue *) value) != GDA_TYPE_NULL)))
				gdaui_data_entry_set_original_value (GDAUI_DATA_ENTRY (entry), value);
			
			if (default_val) {
				gdaui_data_entry_set_value_default (GDAUI_DATA_ENTRY (entry), default_val);
				gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry),
								    GDA_VALUE_ATTR_CAN_BE_DEFAULT,
								    GDA_VALUE_ATTR_CAN_BE_DEFAULT);
			}

			gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry),
							    nnul ? 0 : GDA_VALUE_ATTR_CAN_BE_NULL,
							    GDA_VALUE_ATTR_CAN_BE_NULL);
			    
			g_object_set_data (G_OBJECT (entry), "param", param);
			g_object_set_data (G_OBJECT (entry), "form", form);
			form->priv->entries = g_slist_append (form->priv->entries, entry);
			g_signal_connect (entry, "destroy", G_CALLBACK (entry_destroyed_cb), form);

			/* connect to the parameter's changes */
			i = g_slist_index (form->priv->set->holders, param);
			g_assert (i >= 0);
                        form->priv->signal_ids[i] = g_signal_connect (G_OBJECT (param), "changed",
                                                                      G_CALLBACK (parameter_changed_cb), 
								      entry);
		}
		else { 
			/* several parameters depending on the values of a GdaDataModel object */
			GSList *plist;
			gboolean nnul = TRUE;

			entry = gdaui_entry_combo_new (form->priv->set_info, group->source);
			g_object_set_data (G_OBJECT (entry), "__gdaui_group", group);
			g_object_set_data (G_OBJECT (entry), "form", form);
			form->priv->entries = g_slist_append (form->priv->entries, entry);
			g_signal_connect (entry, "destroy", G_CALLBACK (entry_destroyed_cb), form);

			/* connect to the parameter's changes */
			for (plist = group->group->nodes; plist; plist = plist->next) {
				GdaHolder *param;

				param = GDA_SET_NODE (plist->data)->holder;
				if (!gda_holder_get_not_null (param))
					nnul = FALSE;
				i = g_slist_index (form->priv->set->holders, param);
				g_assert (i >= 0);
				form->priv->signal_ids[i] = g_signal_connect (param, "changed",
                                                                              G_CALLBACK (parameter_changed_cb), 
									      entry);
			}
			gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry),
							    nnul ? 0 : GDA_VALUE_ATTR_CAN_BE_NULL,
							    GDA_VALUE_ATTR_CAN_BE_NULL);
		}

		/* connect the entry's changes */
		g_signal_connect (G_OBJECT (entry), "contents_modified",
				  G_CALLBACK (entry_contents_modified), form);
		g_signal_connect (G_OBJECT (entry), "contents_activated",
				  G_CALLBACK (entry_contents_activated), form);
	}


	/*
	 * If there is a layout spec, then try to use it
	 */
#ifdef HAVE_LIBGLADE
	if (form->priv->layout_spec) {
		GtkWidget *layout = NULL;
		
		layout = glade_xml_get_widget (form->priv->layout_spec->xml_object, form->priv->layout_spec->root_element);
		if (!layout) {
			g_warning (_("Can't find widget named '%s', returning to basic layout"), 
				   form->priv->layout_spec->root_element);
			layout_spec_free (form->priv->layout_spec);
			form->priv->layout_spec = NULL;
		}
		else {
			/* really use the provided layout */
			GtkWidget *box;
			GSList *groups;
			
			gtk_box_pack_start (GTK_BOX (form), layout,  TRUE, TRUE, 0);
			list = form->priv->entries;
			groups = form->priv->set->groups_list;
			while (groups && list) {
				gint param_no;
				gchar *box_name;

				param_no = g_slist_index (form->priv->set->holders,
							  ((GdaSetNode *)(((GdaSetGroup *)groups->data)->nodes->data))->holder);
				box_name = g_strdup_printf ("%s_%d", form->priv->layout_spec->form_prefix, param_no);
				box = glade_xml_get_widget (form->priv->layout_spec->xml_object, box_name);
				g_print ("Box named %s => %p\n", box_name, box);
				g_free (box_name);
				if (box) {
					gboolean expand;
					expand = gdaui_data_entry_expand_in_layout (GDAUI_DATA_ENTRY (list->data));
					form_expand = form_expand || expand;

					gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (list->data), expand, TRUE, 0);
					gtk_widget_show (GTK_WIDGET (list->data));
					if (! g_object_get_data (G_OBJECT (box), "show_actions")) 
						gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (list->data),
										    0, GDA_VALUE_ATTR_ACTIONS_SHOWN);
				}
				list = g_slist_next (list);
				groups = g_slist_next (groups);
			}
			g_assert (!groups && !list);
			gtk_widget_show (layout);
		}
	}
#endif

	if (form->priv->scrolled_window != NULL) {

		gtk_box_pack_start (GTK_BOX(form),
				    (GtkWidget *) form->priv->scrolled_window,
				    TRUE, TRUE, 0);

		GSList *holders = form->priv->set->holders;
		GSList *entries = form->priv->entries;
		while (holders && entries) {
			GdaHolder *holder = holders->data;
			GdauiDataEntry *entry = entries->data;

			const gchar *id = gda_holder_get_id (holder);

			gpointer d[2];
			d[0] = (gchar *) id;
			d[1] = NULL;

			gtk_container_foreach (GTK_CONTAINER(form->priv->scrolled_window),
					       (GtkCallback) find_hbox, d);

			GtkHBox *hbox = d[1];
			g_print ("Hbox for: %s -- %p\n", id, hbox);

			if (hbox != NULL) {
				gboolean expand = gdaui_data_entry_expand_in_layout
					(GDAUI_DATA_ENTRY (entry));
				form_expand = form_expand || expand;

				gtk_box_pack_start (GTK_BOX(hbox), GTK_WIDGET(entry),
						    expand, TRUE, 0);
				gtk_widget_show (GTK_WIDGET(entry));

				if (!g_object_get_data (G_OBJECT(hbox), "show_actions"))
					gdaui_data_entry_set_attributes
						(GDAUI_DATA_ENTRY(entry),
						 0, GDA_VALUE_ATTR_ACTIONS_SHOWN);
			}

			holders = g_slist_next (holders);
			entries = g_slist_next (entries);
		}

		g_assert (holders == NULL && entries == NULL);
		gtk_widget_show (GTK_WIDGET(form->priv->scrolled_window));
	}

	/* 
	 * There is no layout spec (or the provided one could not be used),
	 * so use the default tables arrangment
	 */
	if (!form->priv->layout_spec && form->priv->scrolled_window == NULL) {
		GtkWidget *table, *label;

		/* creating a table for all the entries */
		table = gtk_table_new (g_slist_length (form->priv->entries), 2, FALSE);
		gtk_table_set_row_spacings (GTK_TABLE (table), 5);
		gtk_table_set_col_spacings (GTK_TABLE (table), 5);
		form->priv->entries_table = table;
		gtk_box_pack_start (GTK_BOX (form), table, TRUE, TRUE, 0);
		list = form->priv->entries;
		i = 0;
		while (list) {
			gboolean expand;
			GtkWidget *entry_label;
			GdaHolder *param;
			
			/* label for the entry */
			param = g_object_get_data (G_OBJECT (list->data), "param");
			if (param) {
				gchar *title;
				gchar *str;

				g_object_get (G_OBJECT (param), "name", &title, NULL);
				if (!title)
					title = g_strdup (_("Value"));
				str = g_strdup_printf ("%s:", title);
				label = gtk_label_new (str);
				g_free (str);
				g_object_set_data_full (G_OBJECT (label), "_gda_title", title, g_free);
				if (gda_holder_get_not_null (param)) 
					form->priv->not_null_labels = g_slist_prepend (form->priv->not_null_labels,
										       label);

#ifdef HAVE_LIBGLADE_FIXME
				if (new_spec)
					form->priv->layout_spec = new_spec;
				new_spec = NULL;
#endif
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
				
				gtk_table_attach (GTK_TABLE (table), label, 0, 1, i, i+1,
						  GTK_FILL | GTK_SHRINK, GTK_SHRINK, 0, 0);
				gtk_widget_show (label);
				entry_label = label;
				
				g_object_get (G_OBJECT (param), "description", &title, NULL);
				if (title && *title)
					gtk_widget_set_tooltip_text (label, title);
				g_free (title);
			}
			else {
				/* FIXME: find a better label and tooltip and improve data entry attributes */
				gchar *title = NULL;
				gchar *str;
				gboolean nullok = TRUE;
				GSList *params;
				GdauiSetGroup *group;

				group = g_object_get_data (G_OBJECT (list->data), "__gdaui_group");
				for (params = group->group->nodes; params; params = params->next) {
					if (nullok && gda_holder_get_not_null (GDA_SET_NODE (params->data)->holder))
						nullok = FALSE;
					if (!title)
						g_object_get (G_OBJECT (GDA_SET_NODE (params->data)->holder), 
							      "name", &title, NULL);
				}
				
				if (!title) {
					str = g_object_get_data (G_OBJECT (group->group->nodes_source->data_model), 
								 "name");
					if (str)
						title = g_strdup (str);
				}
				if (!title)
					title = g_strdup (_("Value"));
				str = g_strdup_printf ("%s:", title);
				label = gtk_label_new (str);
				g_free (str);
				g_object_set_data_full (G_OBJECT (label), "_gda_title", title, g_free);
				if (!nullok) 
					form->priv->not_null_labels = g_slist_prepend (form->priv->not_null_labels, label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
				
				gtk_table_attach (GTK_TABLE (table), label, 0, 1, i, i+1,
						  GTK_FILL | GTK_SHRINK, GTK_SHRINK, 0, 0);
				gtk_widget_show (label);
				entry_label = label;
				
				title = g_object_get_data (G_OBJECT (group->group->nodes_source->data_model),
							   "descr");
				if (title && *title)
					gtk_widget_set_tooltip_text (label, title);
			}

			/* add the entry itself to the table */
			expand = gdaui_data_entry_expand_in_layout (GDAUI_DATA_ENTRY (list->data));
			form_expand = form_expand || expand;
			gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (list->data), 1, 2, i, i+1,
					  GTK_FILL | GTK_EXPAND, 
					  form_expand ? (GTK_FILL | GTK_EXPAND) : GTK_SHRINK, 0, 0);
			gtk_widget_show (GTK_WIDGET (list->data));
			g_object_set_data (G_OBJECT (list->data), "entry_label", entry_label);
			if (entry_label) {
				g_signal_connect (entry_label, "destroy",
						  G_CALLBACK (label_destroyed_cb), form);
				g_object_set (G_OBJECT (entry_label), "can-focus", FALSE, NULL);
			}
			g_object_set_data (G_OBJECT (list->data), "row_no", GINT_TO_POINTER (i));
			
			list = g_slist_next (list);
			i++;
		}
		mark_not_null_entry_labels (form, TRUE);
		gtk_widget_show (table);
	}

	g_object_set_data (G_OBJECT (form), "expand", GINT_TO_POINTER (form_expand));
	
	/* Set the Show actions in the entries */
	gdaui_basic_form_show_entry_actions (form, form->priv->show_actions);
	/* Set the Auto entries default in the entries */
	gdaui_basic_form_set_entries_auto_default (form, form->priv->entries_auto_default);
}

static void
entry_destroyed_cb (GtkWidget *entry, GdauiBasicForm *form)
{
	GtkWidget *label_entry;

	form->priv->entries = g_slist_remove (form->priv->entries, entry);
	label_entry = g_object_get_data (G_OBJECT (entry), "entry_label");
	if (label_entry) {
		/* don't take care of label_entry anymore */
		g_signal_handlers_disconnect_by_func (G_OBJECT (label_entry),
						      G_CALLBACK (label_destroyed_cb), form);
		g_object_set_data (G_OBJECT (entry), "entry_label", NULL);
	}
}

static void
label_destroyed_cb (GtkWidget *label, GdauiBasicForm *form)
{
	GSList *list = form->priv->entries;

	while (list) {
		if (g_object_get_data (G_OBJECT (list->data), "entry_label") == label) {
			g_object_set_data (G_OBJECT (list->data), "entry_label", NULL);
			list = NULL;
		}
		else
			list = list->next;
	}
}

/*
 * if @show_mark is TRUE, display the label as bold 
 */
static void
mark_not_null_entry_labels (GdauiBasicForm *form, gboolean show_mark)
{
	GSList *list;

	for (list = form->priv->not_null_labels; list; list = list->next) {
		const gchar *title;
		gchar *str;

		title = g_object_get_data (G_OBJECT (list->data), "_gda_title");
		str = _gdaui_utility_markup_title (title, !show_mark);
		if (show_mark) 
			gtk_label_set_markup (GTK_LABEL (list->data), str);
		else 
			gtk_label_set_text (GTK_LABEL (list->data), str);
		g_free (str);
	}
}

static void
entry_contents_activated (GdauiDataEntry *entry, GdauiBasicForm *form)
{
#ifdef debug_signal
	g_print (">> 'ACTIVATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (form), gdaui_basic_form_signals[ACTIVATED], 0);
#ifdef debug_signal
	g_print ("<< 'ACTIVATED' from %s\n", __FUNCTION__);
#endif
}

static void
entry_contents_modified (GdauiDataEntry *entry, GdauiBasicForm *form)
{
	GdaHolder *param;
	GdaValueAttribute attr;

	attr = gdaui_data_entry_get_attributes (entry);
	param = g_object_get_data (G_OBJECT (entry), "param");
	if (param) { /* single parameter */
		GValue *value;
		
		form->priv->forward_param_updates = FALSE;

		/* parameter's value */
		value = gdaui_data_entry_get_value (entry);
		if ((!value || gda_value_is_null (value)) &&
		    (attr & GDA_VALUE_ATTR_IS_DEFAULT))
			gda_holder_set_value_to_default (param);
		else if (gda_holder_set_value (param, value, NULL)) {
#ifdef debug_signal
			g_print (">> 'PARAM_CHANGED' from %s\n", __FUNCTION__);
#endif
			g_signal_emit (G_OBJECT (form), gdaui_basic_form_signals[PARAM_CHANGED], 0, param, TRUE);
#ifdef debug_signal
			g_print ("<< 'PARAM_CHANGED' from %s\n", __FUNCTION__);
#endif
		}
		else
			TO_IMPLEMENT;
		gda_value_free (value);
		form->priv->forward_param_updates = TRUE;
	}
	else { /* multiple parameters */
		GSList *params;
		GSList *values, *list;
		GdauiSetGroup *group;

		group = g_object_get_data (G_OBJECT (entry), "__gdaui_group");
		params = group->group->nodes;
		values = gdaui_entry_combo_get_values (GDAUI_ENTRY_COMBO (entry));
		g_assert (g_slist_length (params) == g_slist_length (values));

		for (list = values; list; list = list->next, params = params->next) {
			/* REM: if there is more than one value in 'params', then a 
			 * signal is emitted for each param that is changed, 
			 * and there is no way for the listener of that signal to know if it
			 * the end of the "param_changed" sequence. What could be done is:
			 * - adding another boolean to tell if that signal is the 
			 *   last one in the "param_changed" sequence, or
			 * - modify the signal to add a list of parameters which are changed 
			 *   and emit only one signal.
			 */
			GdaHolder *param;
			form->priv->forward_param_updates = FALSE;

			/* parameter's value */
			param = GDA_SET_NODE (params->data)->holder;
			if (gda_holder_set_value (param, (GValue *)(list->data), NULL)) {
#ifdef debug_signal
				g_print (">> 'PARAM_CHANGED' from %s\n", __FUNCTION__);
#endif
				g_signal_emit (G_OBJECT (form), gdaui_basic_form_signals[PARAM_CHANGED], 
					       0, param, TRUE);
#ifdef debug_signal
				g_print ("<< 'PARAM_CHANGED' from %s\n", __FUNCTION__);
#endif
			}
			else
				TO_IMPLEMENT;
			form->priv->forward_param_updates = TRUE;
		}
		g_slist_free (values);

#ifdef PROXY_STORE_EXTRA_VALUES
		/* updating the GdaDataProxy if there is one */
		if (GDA_IS_DATA_MODEL_ITER (form->priv->set)) {
			GdaDataProxy *proxy;
			gint proxy_row;
			
			proxy_row = gda_data_model_iter_get_row ((GdaDataModelIter *) form->priv->set);

			g_object_get (G_OBJECT (form->priv->set), "data_model", &proxy, NULL);
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


/*
 * Called when a parameter changes
 * We emit a "param_changed" signal only if the 'form->priv->forward_param_updates' is TRUE, which means
 * the param change does not come from a GdauiDataEntry change.
 */ 
static void
parameter_changed_cb (GdaHolder *param, GdauiDataEntry *entry)
{
	GdauiBasicForm *form = g_object_get_data (G_OBJECT (entry), "form");
	GdauiSetGroup *group = g_object_get_data (G_OBJECT (entry), "__gdaui_group");
	const GValue *value = gda_holder_get_value (param);

	if (form->priv->forward_param_updates) {
		gboolean param_valid;
		gboolean default_if_invalid = FALSE;

		/* There can be a feedback from the entry if the param is invalid and "set_default_if_invalid"
		   exists and is TRUE */
		param_valid = gda_holder_is_valid (param);
		if (!param_valid) 
			if (g_object_class_find_property (G_OBJECT_GET_CLASS (entry), "set_default_if_invalid"))
				g_object_get (G_OBJECT (entry), "set_default_if_invalid", &default_if_invalid, NULL);

		/* updating the corresponding entry */
		if (! default_if_invalid) {
			g_signal_handlers_block_by_func (G_OBJECT (entry),
							 G_CALLBACK (entry_contents_modified), form);
			g_signal_handlers_block_by_func (G_OBJECT (entry),
							 G_CALLBACK (entry_contents_activated), form);
		}
		if (group) {
			GSList *values = NULL;
			GSList *list = group->group->nodes;
			gboolean allnull = TRUE;

			while (list) {
				const GValue *pvalue;
				pvalue = gda_holder_get_value (GDA_SET_NODE (list->data)->holder);
				values = g_slist_append (values, (GValue *) pvalue);
				if (allnull && pvalue && 
				    (G_VALUE_TYPE ((GValue *) pvalue) != GDA_TYPE_NULL))
					allnull = FALSE;

				list = g_slist_next (list);
			}
			
			if (!allnull) 
				gdaui_entry_combo_set_values (GDAUI_ENTRY_COMBO (entry), values);
			else 
				gdaui_entry_combo_set_values (GDAUI_ENTRY_COMBO (entry), NULL);

			g_slist_free (values);
		}
		else
			gdaui_data_entry_set_value (entry, value);

		if (! default_if_invalid) {
			g_signal_handlers_unblock_by_func (G_OBJECT (entry),
							   G_CALLBACK (entry_contents_modified), form);
			g_signal_handlers_unblock_by_func (G_OBJECT (entry),
							   G_CALLBACK (entry_contents_activated), form);
		}

#ifdef debug_signal
		g_print (">> 'PARAM_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (form), gdaui_basic_form_signals[PARAM_CHANGED], 0, param, FALSE);
#ifdef debug_signal
		g_print ("<< 'PARAM_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
}

/**
 * gdaui_basic_form_get_paramlist
 * @form: a #GdauiBasicForm widget
 *
 * Get a pointer to the #GdaSet used internally by @form to store
 * values
 *
 * Returns:
 */
GdaSet *
gdaui_basic_form_get_paramlist (GdauiBasicForm *form)
{
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);

	return form->priv->set;
}

/**
 * gdaui_basic_form_set_current_as_orig
 * @form: a #GdauiBasicForm widget
 *
 * Tells @form that the current values in the different entries are
 * to be considered as the original values for all the entries; the immediate
 * consequence is that any sub-sequent call to gdaui_basic_form_has_been_changed()
 * will return FALSE (of course until any entry is changed).
 */
void
gdaui_basic_form_set_current_as_orig (GdauiBasicForm *form)
{
	GSList *list;
	GdaHolder *param;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	for (list = form->priv->entries; list; list = list->next) {
		GdauiSetGroup *group;

		group = g_object_get_data (G_OBJECT (list->data), "__gdaui_group");

		if (group) {
			/* Combo entry */
			GSList *values = NULL;
			GSList *params = group->group->nodes;
			gboolean allnull = TRUE;
			
			while (params) {
				const GValue *pvalue;
				pvalue = gda_holder_get_value (GDA_SET_NODE (params->data)->holder);
				values = g_slist_append (values, (GValue *) pvalue);
				if (allnull && pvalue && 
				    (G_VALUE_TYPE ((GValue *) pvalue) != GDA_TYPE_NULL))
					allnull = FALSE;
				
				params = g_slist_next (params);
			}
			
			if (!allnull) 
				gdaui_entry_combo_set_values_orig (GDAUI_ENTRY_COMBO (list->data), values);
			else 
				gdaui_entry_combo_set_values_orig (GDAUI_ENTRY_COMBO (list->data), NULL);
			
			g_slist_free (values);
		}
		else {
			/* non combo entry */
			param = g_object_get_data (G_OBJECT (list->data), "param");
			g_signal_handlers_block_by_func (G_OBJECT (list->data),
							 G_CALLBACK (entry_contents_modified), form);
			gdaui_data_entry_set_original_value (GDAUI_DATA_ENTRY (list->data), gda_holder_get_value (param));
			g_signal_handlers_unblock_by_func (G_OBJECT (list->data),
							   G_CALLBACK (entry_contents_modified), form);
		}
	}
}

/**
 * gdaui_basic_form_is_valid 
 * @form: a #GdauiBasicForm widget
 *
 * Tells if the form can be used as-is (if all the parameters do have some valid values)
 *
 * Returns: TRUE if the form is valid
 */
gboolean
gdaui_basic_form_is_valid (GdauiBasicForm *form)
{
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), FALSE);

	return gda_set_is_valid (form->priv->set, NULL);
}

/**
 * gdaui_basic_form_get_data_set
 * @form: a #GdauiBasicForm widget
 *
 * Get a pointer to the #GdaSet object which
 * is modified by @form
 *
 * Returns:
 */
GdaSet *
gdaui_basic_form_get_data_set (GdauiBasicForm *form)
{
	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);

	return form->priv->set;
}

/**
 * gdaui_basic_form_has_been_changed
 * @form: a #GdauiBasicForm widget
 *
 * Tells if the form has had at least on entry changed, or not
 *
 * Returns:
 */
gboolean
gdaui_basic_form_has_been_changed (GdauiBasicForm *form)
{
	gboolean changed = FALSE;
	GSList *list;

	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), FALSE);
	
	list = form->priv->entries;
	while (list && !changed) {
		if (! (gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (list->data)) & GDA_VALUE_ATTR_IS_UNCHANGED))
			changed = TRUE;
		list = g_slist_next (list);
	}

	return changed;
}

/**
 * gdaui_basic_form_show_entry_actions
 * @form: a #GdauiBasicForm widget
 * @show_actions: a boolean
 *
 * Show or hide the actions button available at the end of each data entry
 * in the form
 */
void
gdaui_basic_form_show_entry_actions (GdauiBasicForm *form, gboolean show_actions)
{
	GSList *entries;
	guint show;
	
	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	show = show_actions ? GDA_VALUE_ATTR_ACTIONS_SHOWN : 0;
	form->priv->show_actions = show_actions;

	entries = form->priv->entries;
	while (entries) {
		gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entries->data), show, 
						    GDA_VALUE_ATTR_ACTIONS_SHOWN);
		entries = g_slist_next (entries);
	}

	/* mark_not_null_entry_labels (form, show_actions); */
}

/**
 * gdaui_basic_form_reset
 * @form: a #GdauiBasicForm widget
 *
 * Resets all the entries in the form to their
 * original values
 */
void
gdaui_basic_form_reset (GdauiBasicForm *form)
{
	GSList *list;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	
	list = form->priv->entries;
	while (list) {
		GdaSetNode *node = g_object_get_data (G_OBJECT (list->data), "node");

		if (node) {
			/* Combo entry */
			GSList *values = NULL;

			values = gdaui_entry_combo_get_values_orig (GDAUI_ENTRY_COMBO (list->data));
			gdaui_entry_combo_set_values (GDAUI_ENTRY_COMBO (list->data), values);
			g_slist_free (values);
		}
		else {
			/* non combo entry */
			const GValue *value;

			value = gdaui_data_entry_get_original_value (GDAUI_DATA_ENTRY (list->data));
			gdaui_data_entry_set_value (GDAUI_DATA_ENTRY (list->data), value);
		}
		list = g_slist_next (list);
	}
}


/**
 * gdaui_basic_form_entry_show
 * @form: a #GdauiBasicForm widget
 * @param: a #GdaHolder object
 * @show:
 *
 * Shows or hides the #GdauiDataEntry in @form which corresponds to the
 * @param parameter
 */
void
gdaui_basic_form_entry_show (GdauiBasicForm *form, GdaHolder *param, gboolean show)
{
	GtkWidget *entry;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	entry = gdaui_basic_form_get_entry_widget (form, param);

	if (entry) {
		gint row = -1;
		GtkWidget *entry_label = g_object_get_data (G_OBJECT (entry), "entry_label");
		
		if (form->priv->entries_table)
			row = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (entry), "row_no"));
		
		if (show) {
			if (g_slist_find (form->priv->hidden_entries, entry)) {
				form->priv->hidden_entries = g_slist_remove (form->priv->hidden_entries, entry);
				g_signal_handlers_disconnect_by_func (G_OBJECT (entry), 
								      G_CALLBACK (widget_shown_cb), form);
			}
			gtk_widget_show (entry);
			
			if (entry_label) {
				if (g_slist_find (form->priv->hidden_entries, entry_label)) {
					form->priv->hidden_entries = g_slist_remove (form->priv->hidden_entries, 
										     entry_label);
					g_signal_handlers_disconnect_by_func (G_OBJECT (entry_label), 
									      G_CALLBACK (widget_shown_cb), form);
				}
				gtk_widget_show (entry_label);
			}
			if (row > -1) 
				gtk_table_set_row_spacing (GTK_TABLE (form->priv->entries_table), row, 5);
		}
		else {
			if (!g_slist_find (form->priv->hidden_entries, entry)) {
				form->priv->hidden_entries = g_slist_append (form->priv->hidden_entries, entry);
				g_signal_connect_after (G_OBJECT (entry), "show", 
							G_CALLBACK (widget_shown_cb), form);
			}
			gtk_widget_hide (entry);
			
			if (entry_label) {
				if (!g_slist_find (form->priv->hidden_entries, entry_label)) {
					form->priv->hidden_entries = g_slist_append (form->priv->hidden_entries, 
										     entry_label);
					g_signal_connect_after (G_OBJECT (entry_label), "show", 
								G_CALLBACK (widget_shown_cb), form);
				}
				gtk_widget_hide (entry_label);
			}
			if (row > -1)
				gtk_table_set_row_spacing (GTK_TABLE (form->priv->entries_table), row, 0);
		}
	}
}

/**
 * gdaui_basic_form_entry_grab_focus
 * @form: a #GdauiBasicForm widget
 * @param: a #GdaHolder object
 * 
 * Makes the data entry corresponding to @param grab the focus for the window it's in
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
 * gdaui_basic_form_entry_set_editable
 * @form: a #GdauiBasicForm widget
 * @param: a #GdaHolder object; or %NULL
 * @editable: %TRUE if corresponding data entry must be editable
 *
 * Sets the #GdauiDataEntry in @form which corresponds to the
 * @param parameter editable or not. If @param is %NULL, then all the parameters
 * are concerned.
 */
void
gdaui_basic_form_entry_set_editable (GdauiBasicForm *form, GdaHolder *param, gboolean editable)
{
	GtkWidget *entry;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	if (param) {
		entry = gdaui_basic_form_get_entry_widget (form, param);
		if (entry) {
			/* GtkWidget *entry_label = g_object_get_data (G_OBJECT (entry), "entry_label"); */
			
			gdaui_data_entry_set_editable (GDAUI_DATA_ENTRY (entry), editable);
			/*if (entry_label)
			  gtk_widget_set_sensitive (entry_label, editable || !form->priv->headers_sensitive);*/
		}
	}
	else {
		GSList *list;
		for (list = form->priv->entries; list; list = list->next)
			gdaui_data_entry_set_editable (GDAUI_DATA_ENTRY (list->data), editable);
	}
}


/**
 * gdaui_basic_form_set_entries_auto_default
 * @form: a #GdauiBasicForm widget
 * @auto_default:
 *
 * Sets weather all the #GdauiDataEntry entries in the form must default
 * to a default value if they are assigned a non valid value.
 * Depending on the real type of entry, it will provide a default value
 * which the user does not need to modify if it is OK.
 *
 * For example a date entry can by default display the current date.
 */
void
gdaui_basic_form_set_entries_auto_default (GdauiBasicForm *form, gboolean auto_default)
{
	GSList *entries;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));
	
	form->priv->entries_auto_default = auto_default;
	entries = form->priv->entries;
	while (entries) {
		if (g_object_class_find_property (G_OBJECT_GET_CLASS (entries->data), "set_default_if_invalid"))
			g_object_set (G_OBJECT (entries->data), "set_default_if_invalid", auto_default, NULL);
		entries = g_slist_next (entries);
	}	
}

/**
 * gdaui_basic_form_set_entries_default
 * @form: a #GdauiBasicForm widget
 *
 * For each entry in the form, sets it to a default value if it is possible to do so.
 */
void
gdaui_basic_form_set_entries_default (GdauiBasicForm *form)
{
	GSList *entries;
	guint attrs;

	g_return_if_fail (GDAUI_IS_BASIC_FORM (form));

	entries = form->priv->entries;
	while (entries) {
		attrs = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (entries->data));
		if (attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT)
			gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entries->data), 
							    GDA_VALUE_ATTR_IS_DEFAULT, GDA_VALUE_ATTR_IS_DEFAULT);
		entries = g_slist_next (entries);
	}
}

static void form_param_changed (GdauiBasicForm *form, GdaHolder *param, gboolean is_user_modif, GtkDialog *dlg);

/**
 * gdaui_basic_form_get_entry_widget
 * @form: a #GdauiBasicForm widget
 * @param: a #GdaHolder object
 *
 * Get the #GdauiDataEntry in @form which corresponds to the param parameter.
 *
 * Returns: the requested widget, or %NULL if not found
 */
GtkWidget *
gdaui_basic_form_get_entry_widget (GdauiBasicForm *form, GdaHolder *param)
{
	GSList *entries;
	GtkWidget *entry = NULL;

	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);

	for (entries = form->priv->entries; entries && !entry; entries = entries->next) {
		GdaHolder *thisparam = g_object_get_data (G_OBJECT (entries->data), "param");

		if (thisparam) {
			if (thisparam == param)
				entry = GTK_WIDGET (entries->data);
		}
		else {
			/* multiple parameters */
			GSList *params;
			GdauiSetGroup *group;

			group = g_object_get_data (G_OBJECT (entries->data), "__gdaui_group");
			for (params = group->group->nodes; params; params = params->next) {
				if (GDA_SET_NODE (params->data)->holder == (gpointer) param) {
					entry = GTK_WIDGET (entries->data);
					break;
				}
			}
		}
	}

	return entry;
}

/**
 * gdaui_basic_form_get_label_widget
 * @form: a #GdauiBasicForm widget
 * @param: a #GdaHolder object
 *
 * Get the label in @form which corresponds to the param parameter.
 *
 * Returns: the requested widget, or %NULL if not found
 */
GtkWidget *
gdaui_basic_form_get_label_widget (GdauiBasicForm *form, GdaHolder *param)
{
	GtkWidget *entry;

	g_return_val_if_fail (GDAUI_IS_BASIC_FORM (form), NULL);

	entry = gdaui_basic_form_get_entry_widget (form, param);
	if (entry) 
		return g_object_get_data (G_OBJECT (entry), "entry_label");
	else
		return NULL;
}


/**
 * gdaui_basic_form_new_in_dialog
 * @paramlist: a #GdaSet structure
 * @parent: the parent window for the new dialog, or %NULL
 * @title: the title of the dialog window, or %NULL
 * @header: a helper text displayed at the top of the dialog, or %NULL
 *
 * Creates a new #GdauiBasicForm widget in the same way as gdaui_basic_form_new()
 * and puts it into a #GtkDialog widget. The returned dialog has the "Ok" and "Cancel" buttons
 * which respectively return GTK_RESPONSE_ACCEPT and GTK_RESPONSE_REJECT.
 *
 * The #GdauiBasicForm widget is attached to the dialog using the user property
 * "form".
 *
 * Returns: the new #GtkDialog widget
 */
GtkWidget *
gdaui_basic_form_new_in_dialog (GdaSet *paramlist, GtkWindow *parent,
				   const gchar *title, const gchar *header)
{
	GtkWidget *form;
	GtkWidget *dlg;
	const gchar *rtitle;

	form = gdaui_basic_form_new (paramlist);
 
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
	if (header && *header) {
		GtkWidget *label;

		label = gtk_label_new (NULL);
		gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
		gtk_label_set_markup (GTK_LABEL (label), header);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), label, FALSE, FALSE, 5);
		gtk_widget_show (label);
	}

	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), 4);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), form, TRUE, TRUE, 10);

	g_signal_connect (G_OBJECT (form), "param_changed",
			  G_CALLBACK (form_param_changed), dlg);
	g_object_set_data (G_OBJECT (dlg), "form", form);

	gtk_widget_show_all (form);
	form_param_changed (GDAUI_BASIC_FORM (form), NULL, FALSE, GTK_DIALOG (dlg));

	return dlg;
}

static void
form_param_changed (GdauiBasicForm *form, GdaHolder *param, gboolean is_user_modif, GtkDialog *dlg)
{
	gboolean valid;

	valid = gdaui_basic_form_is_valid (form);

	gtk_dialog_set_response_sensitive (dlg, GTK_RESPONSE_ACCEPT, valid);
}
