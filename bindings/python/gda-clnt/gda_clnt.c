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

/* GnomeDB
 */
#include <gda-client.h>

/* GnomeDB Python binding stuf
 */
#include <gtk_python.h>
#include "gda_clnt_batch.c"
#include "gda_clnt_command.c"
#include "gda_clnt_connection.c"


static PyMethodDef _gda_clnt_methods[] = {

    /* Gda-Batch
     */
    {"gda_batch_new",                  _wrap_gda_batch_new,                  1},
    {"gda_batch_get_type",             _wrap_gda_batch_get_type,             1},
  
    {"gda_batch_load_file",            _wrap_gda_batch_load_file,            1},
    {"gda_batch_add_command",          _wrap_gda_batch_add_command,          1},
    {"gda_batch_clear",                _wrap_gda_batch_clear,                1},

    {"gda_batch_start",                _wrap_gda_batch_start,                1},
    {"gda_batch_stop",                 _wrap_gda_batch_stop,                 1}, 
    {"gda_batch_is_running",           _wrap_gda_batch_is_running,           1},

    {"gda_batch_get_connection",       _wrap_gda_batch_get_connection,       1},
    {"gda_batch_set_connection",       _wrap_gda_batch_set_connection,       1},
    {"gda_batch_get_transaction_mode", _wrap_gda_batch_get_transaction_mode, 1},
    {"gda_batch_set_transaction_mode", _wrap_gda_batch_set_transaction_mode, 1},


    /* Gda-Command
     */
    {"gda_command_new",              _wrap_gda_command_new,              1},
    {"gda_command_get_type",         _wrap_gda_command_get_type,         1},

    {"gda_command_get_connection",   _wrap_gda_command_get_connection,   1},
    {"gda_command_set_connection",   _wrap_gda_command_set_connection,   1},

    {"gda_command_get_text",         _wrap_gda_command_get_text,         1},
    {"gda_command_set_text",         _wrap_gda_command_set_text,         1},

    {"gda_command_get_cmd_type",     _wrap_gda_command_get_cmd_type,     1},
    {"gda_command_set_cmd_type",     _wrap_gda_command_set_cmd_type,     1},

    {"gda_command_execute",          _wrap_gda_command_execute,          1},
    {"gda_command_create_parameter", _wrap_gda_command_create_parameter, 1},

    {"gda_command_get_timeout",      _wrap_gda_command_get_timeout,      1},
    {"gda_command_set_timeout",      _wrap_gda_command_set_timeout,      1},


    /* Gda-connection
     */
    {"gda_connection_new", _wrap_gda_connection_new, 1},


    {NULL, NULL, 0}
};


void
init_gda_clnt ()
{
    PyObject *dict, *module;
 
    module = Py_InitModule ("_gda_clnt", _gda_clnt_methods); 
    dict = PyModule_GetDict (module);


    if (PyErr_Occurred())
      Py_FatalError ("Can't inicitialice module");
}










































