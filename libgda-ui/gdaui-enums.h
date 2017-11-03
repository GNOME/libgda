/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_ENUMS__
#define __GDAUI_ENUMS__

/* enum for the different modes of action */
typedef enum {
	/* navigation modes */
	GDAUI_ACTION_NAVIGATION_ARROWS       = 1 << 0,
	GDAUI_ACTION_NAVIGATION_SCROLL       = 1 << 1,

	/* modifications */
	GDAUI_ACTION_MODIF_AUTO_COMMIT       = 1 << 2,
	GDAUI_ACTION_MODIF_COMMIT_IMMEDIATE  = 1 << 3,
	GDAUI_ACTION_ASK_CONFIRM_UPDATE      = 1 << 4,
	GDAUI_ACTION_ASK_CONFIRM_DELETE      = 1 << 5,
	GDAUI_ACTION_ASK_CONFIRM_INSERT      = 1 << 6,

	/* Error reporting */
	GDAUI_ACTION_REPORT_ERROR            = 1 << 7
} GdauiActionMode;

/* enum for the different possible actions */
typedef enum {
	/* actions in GdauiDataWidget widgets */
	GDAUI_ACTION_NEW_DATA,
	GDAUI_ACTION_WRITE_MODIFIED_DATA,
	GDAUI_ACTION_DELETE_SELECTED_DATA,
	GDAUI_ACTION_UNDELETE_SELECTED_DATA,
	GDAUI_ACTION_RESET_DATA,
	GDAUI_ACTION_MOVE_FIRST_RECORD,
	GDAUI_ACTION_MOVE_PREV_RECORD,
	GDAUI_ACTION_MOVE_NEXT_RECORD,
	GDAUI_ACTION_MOVE_LAST_RECORD,
	GDAUI_ACTION_MOVE_FIRST_CHUNK,
        GDAUI_ACTION_MOVE_PREV_CHUNK,
        GDAUI_ACTION_MOVE_NEXT_CHUNK,
        GDAUI_ACTION_MOVE_LAST_CHUNK
} GdauiAction;

/* possible predefined attribute names for gda_holder_get_attribute() or gda_column_get_attribute() */
#define GDAUI_ATTRIBUTE_PLUGIN "__gdaui_attr_plugin" /* G_TYPE_STRING expected */

/* issued from HIG 3 */
#define GDAUI_HIG_FORM_HBORDER 18
#define GDAUI_HIG_FORM_HSPACE 12

#define GDAUI_HIG_FORM_VBORDER 18
#define GDAUI_HIG_FORM_VSPACE 6
#define GDAUI_HIG_FORM_VSEP 18

#endif



