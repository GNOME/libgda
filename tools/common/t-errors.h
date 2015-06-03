/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __T_ERRORS__
#define __T_ERRORS__

#include <glib.h>

/*
 * error reporting
 */
extern GQuark t_error_quark (void);
#define T_ERROR t_error_quark ()

typedef enum {
	T_NO_CONNECTION_ERROR,
	T_CONNECTION_CLOSED_ERROR,
	T_INTERNAL_COMMAND_ERROR,
	T_COMMAND_ARGUMENTS_ERROR,
	T_OBJECT_NOT_FOUND_ERROR,
	T_PROVIDER_NOT_FOUND_ERROR,
	T_DSN_NOT_FOUND_ERROR,
	T_STORED_DATA_ERROR,
	T_PURGE_ERROR
} TError;

#endif
