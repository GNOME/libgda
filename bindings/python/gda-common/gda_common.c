/* GNOME DB python binding
 * Copyright (C) 2000 Alvaro Lopez Ortega
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Gnome DB
 */
#include <gda-common.h>

/* Gnome DB Python binding
 */
#include <gtk_python.h>
#include "gda_common_log.c"
#include "gda_common_dsn.c"
#include "gda_common_server.c"


static PyMethodDef _gda_common_methods[] = {

	/* Gda-Log
	 */
	{"gda_log_enable", _wrap_gda_log_enable, 1},
	{"gda_log_disable", _wrap_gda_log_disable, 1},
	{"gda_log_is_enabled", _wrap_gda_log_is_enabled, 1},

	{"gda_log_message", _wrap_gda_log_message, 1},
	{"gda_log_error", _wrap_gda_log_error, 1},

	/* Gda-Dsn
	 */
	{"gda_dsn_new", _wrap_gda_dsn_new, 1},
	{"gda_dsn_find_by_name", _wrap_gda_dsn_find_by_name, 1},

	{"gda_dsn_set_name", _wrap_gda_dsn_set_name, 1},
	{"gda_dsn_set_provider", _wrap_gda_dsn_set_provider, 1},
	{"gda_dsn_set_dsn", _wrap_gda_dsn_set_dsn, 1},
	{"gda_dsn_set_description", _wrap_gda_dsn_set_description, 1},
	{"gda_dsn_set_username", _wrap_gda_dsn_set_username, 1},
	{"gda_dsn_set_config", _wrap_gda_dsn_set_config, 1},
	{"gda_dsn_set_global", _wrap_gda_dsn_set_global, 1},

	{"gda_dsn_get_is_global", _wrap_gda_dsn_get_is_global, 1},
	{"gda_dsn_get_provider", _wrap_gda_dsn_get_provider, 1},
	{"gda_dsn_get_description", _wrap_gda_dsn_get_description, 1},
	{"gda_dsn_get_username", _wrap_gda_dsn_get_username, 1},
	{"gda_dsn_get_config", _wrap_gda_dsn_get_config, 1},
	{"gda_dsn_get_dsn", _wrap_gda_dsn_get_dsn, 1},

	/* Gda-Server
	 */
	{"gda_server_new", _wrap_gda_server_new, 1},
	{"gda_server_list", _wrap_gda_server_list, 1},
	{"gda_server_find_by_name", _wrap_gda_server_find_by_name, 1},

	{NULL, NULL, 0}
};


void
init_gda_common ()
{
	PyObject *dict, *module;

	module = Py_InitModule ("_gda_common", _gda_common_methods);
	dict = PyModule_GetDict (module);


	if (PyErr_Occurred ())
		Py_FatalError ("Can't inicitialice module");
}
