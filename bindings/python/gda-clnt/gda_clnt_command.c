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
_wrap_gda_command_new (PyObject * dummy, PyObject * args)
{
	return PyGtk_New ((GtkObject *) gda_command_new ());
}

static PyObject *
_wrap_gda_command_get_type (PyObject * dummy, PyObject * args)
{
	return PyInt_FromLong (gda_command_get_type ());
}

static PyObject *
_wrap_gda_command_get_connection (PyObject * dymmy, PyObject * args)
{
	PyObject *cmd;

	if (!PyArg_ParseTuple (args, "O!:gda_command_get_connection",
			       &GtkObject_Type, &cmd))
		return NULL;

	return PyGtk_New ((GtkObject
			   *) (gda_command_get_connection (GDA_COMMAND
							   (gtk_get (cmd)))));

}

static PyObject *
_wrap_gda_command_set_connection (PyObject * dymmy, PyObject * args)
{
	PyObject *cmd;
	PyObject *cnc;

	if (!PyArg_ParseTuple (args, "O!O!:gda_command_set_connection",
			       &GtkObject_Type, &cmd, &GtkObject_Type, &cnc))
		return NULL;

	return PyInt_FromLong (gda_command_set_connection
			       (GDA_COMMAND (gtk_get (cmd)),
				GDA_CONNECTION (gtk_get (cnc))));
}

static PyObject *
_wrap_gda_command_get_text (PyObject * dymmy, PyObject * args)
{
	PyObject *cmd;

	if (!PyArg_ParseTuple (args, "O!:gda_command_get_text",
			       &GtkObject_Type, &cmd))
		return NULL;

	return PyString_FromString (gda_command_get_text
				    (GDA_COMMAND (gtk_get (cmd))));
}

static PyObject *
_wrap_gda_command_set_text (PyObject * dymmy, PyObject * args)
{
	PyObject *cmd;
	gchar *text;

	if (!PyArg_ParseTuple (args, "O!s:gda_command_set_text",
			       &GtkObject_Type, &cmd, &text))
		return NULL;

	gda_command_set_text (GDA_COMMAND (gtk_get (cmd)), text);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_command_get_cmd_type (PyObject * dymmy, PyObject * args)
{
	PyObject *cmd;

	if (!PyArg_ParseTuple (args, "O!:gda_command_get_cmd_type",
			       &GtkObject_Type, &cmd))
		return NULL;

	return PyInt_FromLong (gda_command_get_cmd_type
			       (GDA_COMMAND (gtk_get (cmd))));
}

static PyObject *
_wrap_gda_command_set_cmd_type (PyObject * dymmy, PyObject * args)
{
	PyObject *cmd;
	gulong flags;

	if (!PyArg_ParseTuple (args, "O!i:gda_command_set_cmd_type",
			       &GtkObject_Type, &cmd, &flags))
		return NULL;

	gda_command_set_cmd_type (GDA_COMMAND (gtk_get (cmd)), flags);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_command_execute (PyObject * dymmy, PyObject * args)
{
	PyObject *cmd;
	gulong reccount;
	gulong flags;

	if (!PyArg_ParseTuple (args, "O!i:gda_command_execute",
			       &GtkObject_Type, &cmd, &reccount, &flags))
		return NULL;

	return PyGtk_New ((GtkObject
			   *) (gda_command_execute (GDA_COMMAND
						    (gtk_get (cmd)),
						    &reccount, flags)));
}


static PyObject *
_wrap_gda_command_create_parameter (PyObject * dymmy, PyObject * args)
{

	/* FIXME: todo..
	 */

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_command_get_timeout (PyObject * dymmy, PyObject * args)
{
	PyObject *cmd;

	if (!PyArg_ParseTuple (args, "O!:gda_command_get_timeout",
			       &GtkObject_Type, &cmd))
		return NULL;

	return PyLong_FromLong (gda_command_get_timeout
				(GDA_COMMAND (gtk_get (cmd))));
}

static PyObject *
_wrap_gda_command_set_timeout (PyObject * dymmy, PyObject * args)
{
	PyObject *cmd;
	gulong timeout;

	if (!PyArg_ParseTuple (args, "O!i:gda_command_set_timeout",
			       &GtkObject_Type, &cmd, &timeout))
		return NULL;

	gda_command_set_timeout (GDA_COMMAND (gtk_get (cmd)), timeout);
	Py_INCREF (Py_None);
	return Py_None;
}
