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
_wrap_gda_dsn_new (PyObject * dummy, PyObject * args)
{
	return PyGtk_New ((GtkObject *) gda_dsn_new ());
}

/* FIXME: Change the (GdaDsn *) for GDA_DSN().. 
 */


static PyObject *
_wrap_gda_dsn_set_name (PyObject * dummy, PyObject * args)
{
	gchar *name;
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!s:gda_dsn_set_name",
			       &GtkObject_Type, &dsn, &name))
		return NULL;

	gda_dsn_set_name ((GdaDsn *) dsn, name);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_dsn_set_provider (PyObject * dummy, PyObject * args)
{
	gchar *provider;
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!s:gda_dsn_set_provider",
			       &GtkObject_Type, &dsn, &provider))
		return NULL;

	gda_dsn_set_provider ((GdaDsn *) dsn, provider);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_dsn_set_dsn (PyObject * dummy, PyObject * args)
{
	gchar *dsn_str;
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!s:gda_dsn_set_dsn",
			       &GtkObject_Type, &dsn, &dsn_str))
		return NULL;

	gda_dsn_set_dsn ((GdaDsn *) dsn, dsn_str);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_dsn_set_description (PyObject * dummy, PyObject * args)
{
	gchar *description;
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!s:gda_dsn_set_description",
			       &GtkObject_Type, &dsn, &description))
		return NULL;

	gda_dsn_set_description ((GdaDsn *) dsn, description);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_dsn_set_username (PyObject * dummy, PyObject * args)
{
	gchar *username;
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!s:gda_dsn_set_username",
			       &GtkObject_Type, &dsn, &username))
		return NULL;

	gda_dsn_set_username ((GdaDsn *) dsn, username);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_dsn_set_config (PyObject * dummy, PyObject * args)
{
	gchar *config;
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!s:gda_dsn_set_config",
			       &GtkObject_Type, &dsn, &config))
		return NULL;

	gda_dsn_set_config ((GdaDsn *) dsn, config);
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *
_wrap_gda_dsn_set_global (PyObject * dummy, PyObject * args)
{
	gint global;
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!i:gda_dsn_set_global",
			       &GtkObject_Type, &dsn, &global))
		  return NULL;

	gda_dsn_set_global ((GdaDsn *) dsn, global);
	Py_INCREF (Py_None);
	return Py_None;
}





static PyObject *
_wrap_gda_dsn_get_is_global (PyObject * dummy, PyObject * args)
{
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!:gda_dsn_get_is_global",
			       &GtkObject_Type, &dsn))
		return NULL;

	return PyInt_FromLong (GDA_DSN_IS_GLOBAL ((GdaDsn *) gtk_get (dsn)));
}

static PyObject *
_wrap_gda_dsn_get_provider (PyObject * dummy, PyObject * args)
{
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!:gda_dsn_get_provider",
			       &GtkObject_Type, &dsn))
		return NULL;

	return PyString_FromString (GDA_DSN_PROVIDER
				    ((GdaDsn *) gtk_get (dsn)));
}

static PyObject *
_wrap_gda_dsn_get_dsn (PyObject * dummy, PyObject * args)
{
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!:gda_dsn_get_dsn",
			       &GtkObject_Type, &dsn))
		return NULL;

	return PyString_FromString (GDA_DSN_DSN ((GdaDsn *) gtk_get (dsn)));
}

static PyObject *
_wrap_gda_dsn_get_description (PyObject * dummy, PyObject * args)
{
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!:gda_dsn_get_description",
			       &GtkObject_Type, &dsn))
		return NULL;

	return PyString_FromString (GDA_DSN_DESCRIPTION
				    ((GdaDsn *) gtk_get (dsn)));
}

static PyObject *
_wrap_gda_dsn_get_username (PyObject * dummy, PyObject * args)
{
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!:gda_dsn_get_username",
			       &GtkObject_Type, &dsn))
		return NULL;

	return PyString_FromString (GDA_DSN_USERNAME
				    ((GdaDsn *) gtk_get (dsn)));
}

static PyObject *
_wrap_gda_dsn_get_config (PyObject * dummy, PyObject * args)
{
	PyObject *dsn;

	if (!PyArg_ParseTuple (args, "O!:gda_dsn_get_config",
			       &GtkObject_Type, &dsn))
		return NULL;

	return PyString_FromString (GDA_DSN_CONFIG
				    ((GdaDsn *) gtk_get (dsn)));
}



/* function, not method..
 */
static PyObject *
_wrap_gda_dsn_find_by_name (PyObject * dummy, PyObject * args)
{
	gchar *name;

	if (!PyArg_ParseTuple (args, "O!s:gda_dsn_find_by_name", &name))
		return NULL;

	return PyGtk_New ((GtkObject *) gda_dsn_find_by_name (name));
}
