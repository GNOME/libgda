/*
 * Copyright (C) 2009 - 2011 Vivien Malerba
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


#ifndef __GDAUI_DATA_ENTRY_H_
#define __GDAUI_DATA_ENTRY_H_

#include <glib-object.h>
#include <libgda/libgda.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_ENTRY          (gdaui_data_entry_get_type())
#define GDAUI_DATA_ENTRY(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_DATA_ENTRY, GdauiDataEntry)
#define GDAUI_IS_DATA_ENTRY(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_DATA_ENTRY)
#define GDAUI_DATA_ENTRY_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDAUI_TYPE_DATA_ENTRY, GdauiDataEntryIface))

typedef struct _GdauiDataEntry        GdauiDataEntry;
typedef struct _GdauiDataEntryIface   GdauiDataEntryIface;

/* struct for the interface */
struct _GdauiDataEntryIface
{
	GTypeInterface           g_iface;

	/* signals */
	void            (* contents_modified)    (GdauiDataEntry *de);
	void            (* contents_activated)   (GdauiDataEntry *de);
	void            (* status_changed)       (GdauiDataEntry *de);
	gboolean        (* contents_valid)       (GdauiDataEntry *de, GError **error);

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
	gboolean        (*can_expand)            (GdauiDataEntry *de, gboolean horiz);
	void            (*set_editable)          (GdauiDataEntry *de, gboolean editable);
	gboolean        (*get_editable)          (GdauiDataEntry *de);
	void            (*grab_focus)            (GdauiDataEntry *de);

	/* another signal */
	void            (* expand_changed)       (GdauiDataEntry *de);

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



GType           gdaui_data_entry_get_type               (void) G_GNUC_CONST;

void            gdaui_data_entry_set_value_type         (GdauiDataEntry *de, GType type);
GType           gdaui_data_entry_get_value_type         (GdauiDataEntry *de);

void            gdaui_data_entry_set_value              (GdauiDataEntry *de, const GValue *value);
GValue         *gdaui_data_entry_get_value              (GdauiDataEntry *de);
gboolean        gdaui_data_entry_content_is_valid       (GdauiDataEntry *de, GError **error);
void            gdaui_data_entry_set_reference_value    (GdauiDataEntry *de, const GValue *value);
const GValue   *gdaui_data_entry_get_reference_value    (GdauiDataEntry *de);
void            gdaui_data_entry_set_reference_current  (GdauiDataEntry *de);
void            gdaui_data_entry_set_default_value      (GdauiDataEntry *de, const GValue *value);

void            gdaui_data_entry_set_attributes         (GdauiDataEntry *de, GdaValueAttribute attrs,
							 GdaValueAttribute mask);
GdaValueAttribute gdaui_data_entry_get_attributes       (GdauiDataEntry *de);

GdaDataHandler *gdaui_data_entry_get_handler            (GdauiDataEntry *de);
gboolean        gdaui_data_entry_can_expand             (GdauiDataEntry *de, gboolean horiz);
void            gdaui_data_entry_set_editable           (GdauiDataEntry *de, gboolean editable);
gboolean        gdaui_data_entry_get_editable           (GdauiDataEntry *de);
void            gdaui_data_entry_grab_focus             (GdauiDataEntry *de);

G_END_DECLS

#endif
