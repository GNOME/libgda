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
_wrap_gda_batch_new (PyObject *dummy, PyObject *args) {
    return PyGtk_New ((GtkObject *)gda_batch_new());
}

static PyObject *
_wrap_gda_batch_get_type (PyObject *dummy, PyObject *args) {
    return PyInt_FromLong (gda_batch_get_type());
}

static PyObject *
_wrap_gda_batch_load_file (PyObject *dummy, PyObject *args) {
    gchar *filename;
    gboolean clean;
    PyObject *job;

    if (!PyArg_ParseTuple (args, "O!si:gda_batch_load_file", 
			   &GtkObject_Type, &job, &filename, &clean))
	return NULL;

    return PyInt_FromLong (gda_batch_load_file (GDA_BATCH(gtk_get (job)), filename, clean));
}

static PyObject *
_wrap_gda_batch_add_command (PyObject *dymmy, PyObject *args) {
    gchar *cmd;
    PyObject *job;

    if (!PyArg_ParseTuple (args, "O!s:gda_batch_add_command",
			   &GtkObject_Type, &job, &cmd))
	return NULL;

    gda_batch_add_command (GDA_BATCH(gtk_get(job)), cmd);
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject *
_wrap_gda_batch_clear (PyObject *dymmy, PyObject *args) {
    PyObject *job;

    if (!PyArg_ParseTuple (args, "O!:gda_batch_clear",
			   &GtkObject_Type, &job)) 
	return NULL;

    gda_batch_clear (GDA_BATCH(gtk_get(job)));
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject * 
_wrap_gda_batch_start (PyObject *dymmy, PyObject *args) {
    PyObject *job;

    if (!PyArg_ParseTuple (args, "O!:gda_batch_start",
			   &GtkObject_Type, &job)) 
	return NULL;

    return PyInt_FromLong (gda_batch_start (GDA_BATCH(gtk_get(job))));
}

static PyObject *
_wrap_gda_batch_stop (PyObject *dymmy, PyObject *args) {
    PyObject *job;

    if (!PyArg_ParseTuple (args, "O!:gda_batch_stop",
			   &GtkObject_Type, &job)) 
	return NULL;

    gda_batch_stop (GDA_BATCH(gtk_get(job)));
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject *
_wrap_gda_batch_is_running (PyObject *dymmy, PyObject *args) {
    PyObject *job;

    if (!PyArg_ParseTuple (args, "O!:gda_batch_is_running",
			   &GtkObject_Type, &job)) 
	return NULL;

    return PyInt_FromLong (gda_batch_is_running (GDA_BATCH(gtk_get(job))));
}

static PyObject * 
_wrap_gda_batch_get_connection (PyObject *dymmy, PyObject *args) {
    PyObject *job;

    if (!PyArg_ParseTuple (args, "O!:gda_batch_get_connection",
			   &GtkObject_Type, &job))
	return NULL;
    
    return PyGtk_New ((GtkObject *)(gda_batch_get_connection (GDA_BATCH(gtk_get(job)))));
    
}

static PyObject *
_wrap_gda_batch_set_connection (PyObject *dymmy, PyObject *args) {
    PyObject *job;
    PyObject *cnc;

    if (!PyArg_ParseTuple (args, "O!O!:gda_batch_set_connection",
			   &GtkObject_Type, &job,
			   &GtkObject_Type, &cnc)) 
	return NULL;
    
    gda_batch_set_connection (GDA_BATCH(gtk_get(job)), GDA_CONNECTION(gtk_get(cnc)));
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject * 
_wrap_gda_batch_get_transaction_mode (PyObject *dymmy, PyObject *args) {
    PyObject *job;

    if (!PyArg_ParseTuple (args, "O!:gda_batch_get_transaction_mode",
			   &GtkObject_Type, &job))
	return NULL;

    return PyInt_FromLong (gda_batch_get_transaction_mode (GDA_BATCH(gtk_get(job))));
}

static PyObject *
_wrap_gda_batch_set_transaction_mode (PyObject *dymmy, PyObject *args) {
    PyObject *job;
    gint     mode;

    if (!PyArg_ParseTuple (args, "O!i:gda_batch_set_transaction_mode",
			   &GtkObject_Type, &job, &mode))
	return NULL;

    gda_batch_set_transaction_mode (GDA_BATCH(gtk_get(job)), mode);
    Py_INCREF (Py_None);
    return Py_None;
}
















