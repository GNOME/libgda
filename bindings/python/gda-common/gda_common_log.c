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
_wrap_gda_log_enable (PyObject * dummy, PyObject * args)
{
	gda_log_enable ();
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_log_disable (PyObject * dummy, PyObject * args)
{
	gda_log_disable ();
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_log_is_enabled (PyObject * dummy, PyObject * args)
{
	return PyInt_FromLong (gda_log_is_enabled ());
}

static PyObject *
_wrap_gda_log_message (PyObject * dummy, PyObject * args)
{
	gchar *msg;

	if (!PyArg_ParseTuple (args, "s!:gda_log_is_enable", &msg))
		return NULL;

	gda_log_message ("%s", msg);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_log_error (PyObject * dummy, PyObject * args)
{
	gchar *msg;

	if (!PyArg_ParseTuple (args, "s!:gda_log_is_enable", &msg))
		return NULL;

	gda_log_error ("%s", msg);
	Py_INCREF (Py_None);
	return Py_None;
}
