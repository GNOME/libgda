/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2012 Daniel Espinosa <esodan@gmail.com>
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


#ifndef __GDAUI_DATA_ENTRY_H_
#define __GDAUI_DATA_ENTRY_H_

#include <glib-object.h>
#include <libgda/libgda.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* error reporting */
extern GQuark gdaui_data_entry_error_quark (void);
#define GDAUI_DATA_ENTRY_ERROR gdaui_data_entry_error_quark ()

typedef enum
{
	GDAUI_DATA_ENTRY_FILE_NOT_FOUND_ERROR,
	GDAUI_DATA_ENTRY_INVALID_DATA_ERROR
} GdauiDataEntryError;

#define GDAUI_TYPE_DATA_ENTRY          (gdaui_data_entry_get_type())
G_DECLARE_INTERFACE (GdauiDataEntry, gdaui_data_entry, GDAUI, DATA_ENTRY, GtkWidget)
/* struct for the interface */
struct _GdauiDataEntryInterface
{
	GTypeInterface           g_iface;

	/* signals */
	void            (* contents_modified)    (GdauiDataEntry *de);
	void            (* contents_activated)   (GdauiDataEntry *de);
	void            (* status_changed)       (GdauiDataEntry *de);

	/* virtual table */
	void            (*set_value_type)        (GdauiDataEntry *de, GType type);
	GType           (*get_value_type)        (GdauiDataEntry *de);
	void            (*set_value)             (GdauiDataEntry *de, const GValue * value);
	GValue         *(*get_value)             (GdauiDataEntry *de);
	void            (*set_ref_value)         (GdauiDataEntry *de, const GValue * value);
	const GValue   *(*get_ref_value)         (GdauiDataEntry *de);
	void            (*set_value_default)     (GdauiDataEntry *de, const GValue * value);
	void            (*set_attributes)        (GdauiDataEntry *de, GdaValueAttribute attrs, GdaValueAttribute mask);
	GdaValueAttribute (*get_attributes)      (GdauiDataEntry *de);
	GdaDataHandler *(*get_handler)           (GdauiDataEntry *de);
	gboolean        (*can_expand)            (GdauiDataEntry *de, gboolean horiz); /* FIXME: not used anymore */
	void            (*set_editable)          (GdauiDataEntry *de, gboolean editable);
	gboolean        (*get_editable)          (GdauiDataEntry *de);
	void            (*grab_focus)            (GdauiDataEntry *de);

	/* another signal */
	void            (*expand_changed)        (GdauiDataEntry *de);

	void            (*set_unknown_color)     (GdauiDataEntry *de, gdouble red, gdouble green,
						  gdouble blue, gdouble alpha);
	/* New Validating mecanism */
	gboolean	(*validate)		 (GdauiDataEntry* de, GError **error);

	/*< private >*/
	/* Padding for future expansion */
        void (*_gdaui_reserved1) (void);
        void (*_gdaui_reserved2) (void);
        void (*_gdaui_reserved3) (void);
        void (*_gdaui_reserved4) (void);
};

/**
 * SECTION:gdaui-data-entry
 * @short_description: Data entry widget
 * @title: GdauiDataEntry
 * @stability: Stable
 * @Image: vi-data-entry.png
 * @see_also:
 *
 * The #GdaUiDataEntry is an interface for widgets (simple or complex)
 * which lets the user view and/or modify a #GValue.
 *
 * This interface is implemented by widgets which feature data editing (usually composed of an editing
 * area and a button to have some more control on the value being edited).
 * The interface allows to control how the widget works and to query the value and the attributes
 * of the data held by the widget.
 *
 * The widget can store the original value (to be able to tell if the value has been changed
 * by the user) and a default value (which will be returned if the user explicitly forces the widget
 * to be set to the default value).
 * Control methods allow to set the type of value to be edited (the requested type must be
 * compatible with what the widget can handle), set the value (which replaces the currently edited
 * value), set the value and the original value (the value passed as argument is set and is also
 * considered to be the original value).
 *
 * #GdaUiDataEntry widgets are normally created using the gdaui_new_data_entry() function.
 */



void            gdaui_data_entry_set_value_type         (GdauiDataEntry *de, GType type);
GType           gdaui_data_entry_get_value_type         (GdauiDataEntry *de);

void            gdaui_data_entry_set_value              (GdauiDataEntry *de, const GValue *value);
GValue         *gdaui_data_entry_get_value              (GdauiDataEntry *de);
gboolean        gdaui_data_entry_validate		(GdauiDataEntry *de, GError **error);
void            gdaui_data_entry_set_reference_value    (GdauiDataEntry *de, const GValue *value);
const GValue   *gdaui_data_entry_get_reference_value    (GdauiDataEntry *de);
void            gdaui_data_entry_set_reference_current  (GdauiDataEntry *de);
void            gdaui_data_entry_set_default_value      (GdauiDataEntry *de, const GValue *value);

void            gdaui_data_entry_set_attributes         (GdauiDataEntry *de, GdaValueAttribute attrs,
							 GdaValueAttribute mask);
GdaValueAttribute gdaui_data_entry_get_attributes       (GdauiDataEntry *de);

GdaDataHandler *gdaui_data_entry_get_handler            (GdauiDataEntry *de);
void            gdaui_data_entry_set_editable           (GdauiDataEntry *de, gboolean editable);
gboolean        gdaui_data_entry_get_editable           (GdauiDataEntry *de);
void            gdaui_data_entry_grab_focus             (GdauiDataEntry *de);

void            gdaui_data_entry_set_unknown_color      (GdauiDataEntry *de, gdouble red, gdouble green,
							 gdouble blue, gdouble alpha);

G_END_DECLS

#endif
