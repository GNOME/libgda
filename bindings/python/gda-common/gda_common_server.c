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
_wrap_gda_server_new (PyObject * dummy, PyObject * args)
{
	return PyGtk_New ((GtkObject *) gda_server_new ());
}

static PyObject *
_wrap_gda_server_list (PyObject * dummy, PyObject * args)
{
	/* FIXME: TODO.. :)
	 */
	return NULL;
}



/* function, not method..
 */
static PyObject *
_wrap_gda_server_find_by_name (PyObject * dummy, PyObject * args)
{
	gchar *name;

	if (!PyArg_ParseTuple (args, "s:gda_server_find_by_name", &name))
		return NULL;

	return PyGtk_New ((GtkObject *) gda_server_find_by_name (name));
}
