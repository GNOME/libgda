/* gdaui-data-entry.c
 *
 * Copyright (C) 2003 - 2010 Vivien Malerba
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

#include "gdaui-data-entry.h"
#include "marshallers/gdaui-marshal.h"

/* signals */
enum {
	CONTENTS_MODIFIED,
	CONTENTS_ACTIVATED,
	STATUS_CHANGED,
	CONTENTS_VALID,
	EXPAND_CHANGED,
	LAST_SIGNAL
};

static gint gdaui_data_entry_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0 };
static void gdaui_data_entry_iface_init (gpointer g_class);

GType
gdaui_data_entry_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiDataEntryIface),
			(GBaseInitFunc) gdaui_data_entry_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};

		type = g_type_register_static (G_TYPE_INTERFACE, "GdauiDataEntry", &info, 0);
	}
	return type;
}

static gboolean
contents_valid_accumulator (GSignalInvocationHint *ihint,
			    GValue *return_accu,
			    const GValue *handler_return,
			    gpointer data)
{
        gboolean thisvalue;

        thisvalue = g_value_get_boolean (handler_return);
        g_value_set_boolean (return_accu, thisvalue);

        return thisvalue; /* stop signal if 'thisvalue' is FALSE */
}

static gboolean
m_class_contents_valid (GdauiDataEntry *de, GError **error)
{
	return TRUE;
}

static void
gdaui_data_entry_iface_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		gdaui_data_entry_signals[CONTENTS_MODIFIED] =
			g_signal_new ("contents-modified",
				      GDAUI_TYPE_DATA_ENTRY,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET (GdauiDataEntryIface, contents_modified),
				      NULL, NULL,
				      _gdaui_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		gdaui_data_entry_signals[CONTENTS_ACTIVATED] =
			g_signal_new ("contents-activated",
				      GDAUI_TYPE_DATA_ENTRY,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET (GdauiDataEntryIface, contents_activated),
				      NULL, NULL,
				      _gdaui_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		gdaui_data_entry_signals[STATUS_CHANGED] =
			g_signal_new ("status-changed",
				      GDAUI_TYPE_DATA_ENTRY,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET (GdauiDataEntryIface, status_changed),
				      NULL, NULL,
				      _gdaui_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		gdaui_data_entry_signals[CONTENTS_VALID] =
			g_signal_new ("contents-valid",
				      GDAUI_TYPE_DATA_ENTRY,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdauiDataEntryIface, contents_valid),
				      contents_valid_accumulator, NULL,
				      _gdaui_marshal_BOOLEAN__POINTER,
				      G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
		gdaui_data_entry_signals[EXPAND_CHANGED] =
			g_signal_new ("expand-changed",
				      GDAUI_TYPE_DATA_ENTRY,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET (GdauiDataEntryIface, expand_changed),
				      NULL, NULL,
				      _gdaui_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);

		((GdauiDataEntryIface*) g_class)->contents_valid = m_class_contents_valid;
		initialized = TRUE;
	}
}

/**
 * gdaui_data_entry_set_value_type
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 * @type:
 *
 * Sets the type of value the GdauiDataEntry will handle. The type must be compatible with what
 * the widget can handle.
 *
 * Since: 4.2
 */
void
gdaui_data_entry_set_value_type (GdauiDataEntry *de, GType type)
{
	g_return_if_fail (GDAUI_IS_DATA_ENTRY (de));

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->set_value_type)
		(GDAUI_DATA_ENTRY_GET_IFACE (de)->set_value_type) (de, type);
}


/**
 * gdaui_data_entry_get_value_type
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 *
 * Fetch the type of data the GdauiDataEntry handles
 *
 * Returns: the GType type
 *
 * Since: 4.2
 */
GType
gdaui_data_entry_get_value_type (GdauiDataEntry *de)
{
	g_return_val_if_fail (GDAUI_IS_DATA_ENTRY (de), G_TYPE_INVALID);

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_value_type)
		return (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_value_type) (de);
	return G_TYPE_INVALID;
}

/**
 * gdaui_data_entry_set_value
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 * @value: a #GValue, or %NULL
 *
 * Push a value into the #GdauiDataEntry. The value parameter must either be:
 * <itemizedlist>
 *   <listitem><para>of type GDA_TYPE_NULL (may be created using gda_value_new_null()) to 
 *      represent a NULL value (SQL NULL), or</para></listitem>
 *   <listitem><para>of type specified using gdaui_data_entry_set_value_type(), or</para></listitem>
 *   <listitem><para>NULL to represent an undetermined value (usually an error)</para></listitem>
 * </itemizedlist>
 *
 * Since: 4.2
 */
void
gdaui_data_entry_set_value (GdauiDataEntry *de, const GValue *value)
{
	g_return_if_fail (GDAUI_IS_DATA_ENTRY (de));

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->set_value)
		(GDAUI_DATA_ENTRY_GET_IFACE (de)->set_value) (de, value);
}

/**
 * gdaui_data_entry_get_value
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 *
 * Fetch the value held in the GdauiDataEntry widget. If the value is set to NULL,
 * the returned value is of type GDA_TYPE_NULL. If the value is set to default,
 * then the returned value is of type GDA_TYPE_NULL or is the default value if it
 * has been provided to the widget (and is of the same type as the one provided by @de).
 *
 * Returns: a new #GValue
 *
 * Since: 4.2
 */
GValue *
gdaui_data_entry_get_value (GdauiDataEntry *de)
{
	g_return_val_if_fail (GDAUI_IS_DATA_ENTRY (de), NULL);

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_value)
		return (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_value) (de);

	return NULL;
}

/**
 * gdaui_data_entry_content_is_valid
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 * @error: a place to store an error, or %NULL
 *
 * Tests the validity of @de's contents. The validity is a determined from:
 * <itemizedlist>
 *   <listitem><para>the @de widget itself if it is capable of doing it (depending on the implementation)</para></listitem>
 *   <listitem><para>the results of the "contents-valid" signal which can be connected from </para></listitem>
 * </itemizedlist>
 *
 * Returns: TRUE if @de's contents is valid
 *
 * Since: 4.2
 */
gboolean
gdaui_data_entry_content_is_valid (GdauiDataEntry *de, GError **error)
{
	gboolean is_valid;
	g_return_val_if_fail (GDAUI_IS_DATA_ENTRY (de), FALSE);

	g_signal_emit (de, gdaui_data_entry_signals [CONTENTS_VALID], 0, error, &is_valid);
	return is_valid;
}


/**
 * gdaui_data_entry_set_reference_value
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 * @value: a #GValue, or %NULL
 *
 * Push a value into the GdauiDataEntry in the same way as gdaui_data_entry_set_value() but
 * also sets this value as the reference value.
 *
 * Since: 4.2
 */
void
gdaui_data_entry_set_reference_value (GdauiDataEntry *de, const GValue *value)
{
	g_return_if_fail (GDAUI_IS_DATA_ENTRY (de));

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->set_ref_value)
		(GDAUI_DATA_ENTRY_GET_IFACE (de)->set_ref_value) (de, value);
}

/**
 * gdaui_data_entry_reset
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 *
 * Tells that the current value in @de is to be considered as the reference value
 *
 * Since: 4.2
 */
void
gdaui_data_entry_reset (GdauiDataEntry *de)
{
	GValue *value;
	g_return_if_fail (GDAUI_IS_DATA_ENTRY (de));

	value = gdaui_data_entry_get_value (de);
	gdaui_data_entry_set_reference_value (de, value);
	if (value)
		gda_value_free (value);
}


/**
 * gdaui_data_entry_get_reference_value
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 *
 * Fetch the reference value held in the #GdauiDataEntry widget
 *
 * Returns: the #GValue (not modifiable)
 *
 * Since: 4.2
 */
const GValue *
gdaui_data_entry_get_reference_value (GdauiDataEntry *de)
{
	g_return_val_if_fail (GDAUI_IS_DATA_ENTRY (de), NULL);

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_ref_value)
		return (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_ref_value) (de);

	return NULL;
}


/**
 * gdaui_data_entry_set_default_value
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 * @value: a #GValue, or %NULL
 *
 * Sets the default value for the GdauiDataEntry which gets displayed when the
 * user forces the default value. If it is not set then it is set to type GDA_TYPE_NULL.
 * The value parameter must either be:
 * <itemizedlist>
 *   <listitem><para>NULL or of type GDA_TYPE_NULL, or</para></listitem>
 *   <listitem><para>of type specified using gdaui_data_entry_set_value_type().</para></listitem>
 * </itemizedlist>
 *
 * Since: 4.2
 */
void
gdaui_data_entry_set_default_value (GdauiDataEntry *de, const GValue *value)
{
	g_return_if_fail (GDAUI_IS_DATA_ENTRY (de));
	g_return_if_fail (value);

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->set_value_default)
		(GDAUI_DATA_ENTRY_GET_IFACE (de)->set_value_default) (de, value);
}

/**
 * gdaui_data_entry_set_attributes
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 * @attrs: the attributes to set (OR'ed between them)
 * @mask: the mask corresponding to the considered attributes
 *
 * Sets the parameters of the GdauiDataEntry. Only the attributes corresponding to the
 * mask are set, the other ones are ignored.
 *
 * Since: 4.2
 */
void
gdaui_data_entry_set_attributes (GdauiDataEntry *de, GdaValueAttribute attrs, GdaValueAttribute mask)
{
	g_return_if_fail (GDAUI_IS_DATA_ENTRY (de));

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->set_attributes)
		(GDAUI_DATA_ENTRY_GET_IFACE (de)->set_attributes) (de, attrs, mask);
}

/**
 * gdaui_data_entry_get_attributes
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 *
 * Retrieves the parameters of the GdauiDataEntry widget.
 *
 * Returns: the OR'ed bits corresponding to the attributes.
 *
 * Since: 4.2
 */
GdaValueAttribute
gdaui_data_entry_get_attributes (GdauiDataEntry *de)
{
	g_return_val_if_fail (GDAUI_IS_DATA_ENTRY (de), 0);

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_attributes)
		return (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_attributes) (de);

	return 0;
}


/**
 * gdaui_data_entry_get_handler
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 *
 * Fetch the GdaDataHandler the GdauiDataEntry is using
 *
 * Returns: the GdaDataHandler object
 *
 * Since: 4.2
 */
GdaDataHandler  *
gdaui_data_entry_get_handler (GdauiDataEntry *de)
{
	g_return_val_if_fail (GDAUI_IS_DATA_ENTRY (de), NULL);

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_handler)
		return (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_handler) (de);

	return NULL;
}

/**
 * gdaui_data_entry_can_expand
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 * @horiz: %TRUE to query horizontal expansion requirements, or %FALSE for vertical
 *
 * Used for the layout of #GdaDataEntry widgets in containers: queries if @de requires
 * horizontal or vertical expansion, depending on @horiz
 *
 * Returns: TRUE if the widget requires expansion
 *
 * Since: 4.2
 */
gboolean
gdaui_data_entry_can_expand (GdauiDataEntry *de, gboolean horiz)
{
	g_return_val_if_fail (GDAUI_IS_DATA_ENTRY (de), FALSE);

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->can_expand)
		return (GDAUI_DATA_ENTRY_GET_IFACE (de)->can_expand) (de, horiz);
	else
		return FALSE;
}

/**
 * gdaui_data_entry_set_editable
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 * @editable: set to %TRUE to have an editable data entry
 *
 * Set if @de can be modified or not by the user
 *
 * Since: 4.2
 */
void
gdaui_data_entry_set_editable (GdauiDataEntry *de, gboolean editable)
{
	g_return_if_fail (GDAUI_IS_DATA_ENTRY (de));

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->set_editable)
		(GDAUI_DATA_ENTRY_GET_IFACE (de)->set_editable) (de, editable);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (de), editable);
}

/**
 * gdaui_data_entry_get_editable
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 *
 * Tells if @de can be edited by the user
 *
 * Returns: %TRUE if @de is editable
 *
 * Since: 4.2
 */
gboolean
gdaui_data_entry_get_editable (GdauiDataEntry *de)
{
	g_return_val_if_fail (GDAUI_IS_DATA_ENTRY (de), FALSE);

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_editable)
		return (GDAUI_DATA_ENTRY_GET_IFACE (de)->get_editable) (de);
	else {
		gboolean sens;
		g_object_get ((GObject*) de, "sensitive", &sens, NULL);
		return sens;
	}
}

/**
 * gdaui_data_entry_grab_focus
 * @de: a #GtkWidget object which implements the #GdauiDataEntry interface
 *
 * Makes @de grab the focus for the window it's in
 *
 * Since: 4.2
 */
void
gdaui_data_entry_grab_focus (GdauiDataEntry *de)
{
	g_return_if_fail (GDAUI_IS_DATA_ENTRY (de));

	if (GDAUI_DATA_ENTRY_GET_IFACE (de)->grab_focus)
		(GDAUI_DATA_ENTRY_GET_IFACE (de)->grab_focus) (de);
}
