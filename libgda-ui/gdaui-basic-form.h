/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_BASIC_FORM__
#define __GDAUI_BASIC_FORM__

#include <gtk/gtk.h>
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_BASIC_FORM          (gdaui_basic_form_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiBasicForm, gdaui_basic_form, GDAUI, BASIC_FORM, GtkBox)


/* struct for the object's class */
struct _GdauiBasicFormClass
{
	GtkBoxClass        parent_class;

	/* signals */
	void       (*holder_changed) (GdauiBasicForm *form, GdaHolder *holder, gboolean is_user_action);
	void       (*activated)      (GdauiBasicForm *form);
	void       (*layout_changed) (GdauiBasicForm *form);
};

/**
 * SECTION:gdaui-basic-form
 * @short_description: Form widget mapping the values contained in a #GdaSet
 * @title: GdauiBasicForm
 * @stability: Stable
 * @Image: vi-basic-form.png
 * @see_also:
 *
 * The #GdauiBasicForm widget is a form containing an entry for each #GdaHolder object
 * contained in a #GdaSet (specified when the form is created). A typical usage is when the
 * user is requested to enter a value which will be used in a statement (without any error checking for clarity):
 * <programlisting>
 * GdaStatement *stmt;
 * GdaSet *params;
 * stmt = gda_sql_parser_parse_string (parser, "SELECT * FROM customers where name LIKE ##name::string", NULL, NULL);
 * gda_statement_get_parameters (stmt, &amp;params, NULL);
 * 
 * GtkWidget *form;
 * gint result;
 * form = gdaui_basic_form_new_in_dialog (params, NULL, "Customer search", "Enter Customer search expression");
 * result = gtk_dialog_run (GTK_DIALOG (form));
 * gtk_widget_destroy (form);
 * if (result == GTK_RESPONSE_ACCEPT) {
 *    // execute statement
 *    GdaDataModel *model;
 *    model = gda_connection_statement_execute_select (cnc, stmt, params, NULL);
 *    [...]
 * }
 * g_object_unref (params);
 * g_object_unref (stmt);
 * </programlisting>
 *
 * The default layout within a #GdauiBasicForm is a vertical column: all the data entry widgets are aligned
 * in a single column. This behaviour can be changed using the gdaui_basic_form_set_layout_from_file() method or
 * setting the <link linkend="GdauiBasicForm--xml-layout">xml-layout</link> property.
 * 
 * <anchor id="GdauiBasicFormXMLLayout"/>
 * The #GdauiBasicForm class parses textual descriptions of XML layout which
 * which can be described by the following DTD. 
 *
 * <programlisting><![CDATA[
 * <!ELEMENT gdaui_layouts (gdaui_form | gdaui_grid)>
 * 
 * <!ELEMENT gdaui_form (gdaui_section | gdaui_column | gdaui_notebook)*>
 * <!ATTLIST gdaui_form
 *          name CDATA #REQUIRED
 * 	  container (columns|rows|hpaned|vpaned) #IMPLIED>
 * 
 * <!ELEMENT gdaui_section (gdaui_section | gdaui_column | gdaui_notebook)*>
 * <!ATTLIST gdaui_section
 *          title CDATA #IMPLIED >
 * 
 * <!ELEMENT gdaui_notebook (gdaui_section | gdaui_column | gdaui_notebook)*>
 * 
 * <!ELEMENT gdaui_column (gdaui_entry | gdaui_placeholder)*>
 * 
 * <!ELEMENT gdaui_entry EMPTY>
 * <!ATTLIST gdaui_entry
 *          name CDATA #REQUIRED
 * 	  editable (true|false) #IMPLIED
 * 	  label CDATA #IMPLIED
 * 	  plugin CDATA #IMPLIED>
 * 
 * <!ELEMENT gdaui_placeholder EMPTY>
 * <!ATTLIST gdaui_placeholder
 * 	  id CDATA #REQUIRED
 * 	  label CDATA #IMPLIED>
 * ]]></programlisting>
 *
 * <example>
 *  <title>A GdauiBasicForm layout example</title>
 *  <programlisting><![CDATA[
 * <?xml version="1.0" encoding="UTF-8"?>
 * <gdaui_layouts>
 *  <gdaui_form name="customers" container="hpaned">
 *    <gdaui_section title="Summary">
 *      <gdaui_column>
 * 	<gdaui_entry name="id" editable="no"/>
 * 	<gdaui_entry name="name"/>
 * 	<gdaui_entry name="comments" plugin="text"/>
 * 	<gdaui_entry name="total_orders" label="Total ordered" plugin="number:NB_DECIMALS=2;CURRENCY=€"/>
 *      </gdaui_column>
 *    </gdaui_section>
 *    <gdaui_section title="Photo">
 *      <gdaui_column>
 * 	<gdaui_entry name="photo" plugin="picture"/>
 *      </gdaui_column>
 *    </gdaui_section>
 *  </gdaui_form>
 * </gdaui_layouts>
 * ]]></programlisting>
 * </example>
 */

GtkWidget        *gdaui_basic_form_new                      (GdaSet *data_set);
GtkWidget        *gdaui_basic_form_new_in_dialog            (GdaSet *data_set, GtkWindow *parent,
							     const gchar *title, const gchar *header);
GdaSet           *gdaui_basic_form_get_data_set             (GdauiBasicForm *form);
gboolean          gdaui_basic_form_is_valid                 (GdauiBasicForm *form);
gboolean          gdaui_basic_form_has_changed              (GdauiBasicForm *form);
void              gdaui_basic_form_reset                    (GdauiBasicForm *form);
void              gdaui_basic_form_set_as_reference         (GdauiBasicForm *form);

void              gdaui_basic_form_entry_set_visible        (GdauiBasicForm *form, 
							     GdaHolder *holder, gboolean show);
void              gdaui_basic_form_entry_grab_focus         (GdauiBasicForm *form, GdaHolder *holder);
void              gdaui_basic_form_entry_set_editable       (GdauiBasicForm *form, GdaHolder *holder, 
							     gboolean editable);
void              gdaui_basic_form_set_entries_to_default   (GdauiBasicForm *form);

GtkWidget        *gdaui_basic_form_get_entry_widget         (GdauiBasicForm *form, GdaHolder *holder);
GtkWidget        *gdaui_basic_form_get_label_widget         (GdauiBasicForm *form, GdaHolder *holder);

void              gdaui_basic_form_set_layout_from_file     (GdauiBasicForm *form, const gchar *file_name,
							     const gchar *form_name);
GtkWidget        *gdaui_basic_form_get_place_holder         (GdauiBasicForm *form, const gchar *placeholder_id);
void              gdaui_basic_form_update_data_set            (GdauiBasicForm *form, GError **error);

typedef enum {
	GDAUI_BASIC_FORM_LABELS,
	GDAUI_BASIC_FORM_ENTRIES
} GdauiBasicFormPart;
void              gdaui_basic_form_add_to_size_group        (GdauiBasicForm *form, GtkSizeGroup *size_group,
							     GdauiBasicFormPart part);
void              gdaui_basic_form_remove_from_size_group   (GdauiBasicForm *form, GtkSizeGroup *size_group,
							     GdauiBasicFormPart part);

void              gdaui_basic_form_set_unknown_color        (GdauiBasicForm *form, gdouble red, gdouble green,
							     gdouble blue, gdouble alpha);

G_END_DECLS

#endif
