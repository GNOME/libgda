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

static PyObject *
_wrap_gda_connection_new (PyObject *dummy, PyObject *args) {
    PyObject *orb;

    if (!PyArg_ParseTuple (args, "O!:gda_connection_new",
			   &GtkObject_Type, &orb))
        return NULL;

    return PyGtk_New ((GtkObject *)gda_connection_new ((gkt_get(orb))));
}

//static PyObject *
//_wrap_gda_connection_list_providers (PyObject *dummy, PyObject *args) {
  
//}



