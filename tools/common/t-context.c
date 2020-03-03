/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include "t-context.h"
#include "t-utils.h"
#include "t-app.h"
#include <glib/gi18n-lib.h>
#include <sql-parser/gda-sql-parser.h>
#include <glib/gstdio.h>
#include <errno.h>

/*
 * Main static functions
 */
static void t_context_class_init (TContextClass *klass);
static void t_context_init (TContext *self);
static void t_context_dispose (GObject *object);
static void t_context_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void t_context_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
        PROP_0,
        PROP_ID
};

struct _TContextPrivate {
	gchar *id;
	TConnection *current; /* ref held */

	ToolOutputFormat output_format;
	FILE *output_stream;
	gboolean output_is_pipe;

	GDateTime *last_time_used;
	ToolCommandGroup *command_group;

	gulong sigid;
};

GType
t_context_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GMutex registering;
                static const GTypeInfo info = {
                        sizeof (TContextClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) t_context_class_init,
                        NULL,
                        NULL,
                        sizeof (TContext),
                        0,
                        (GInstanceInitFunc) t_context_init,
                        0
                };


                g_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (G_TYPE_OBJECT, "TContext", &info, 0);
                g_mutex_unlock (&registering);
        }
        return type;
}

static void
t_context_class_init (TContextClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        parent_class = g_type_class_peek_parent (klass);

	klass->run = NULL;

	/* Properties */
        object_class->set_property = t_context_set_property;
        object_class->get_property = t_context_get_property;

        g_object_class_install_property (object_class, PROP_ID,
                                         g_param_spec_string ("id", NULL, NULL, NULL,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));

        object_class->dispose = t_context_dispose;
}

static void
t_context_init (TContext *self)
{
        self->priv = g_new0 (TContextPrivate, 1);
        self->priv->output_format = BASE_TOOL_OUTPUT_FORMAT_DEFAULT;
	self->priv->sigid = 0;

	TContext *tcon;
	tcon = t_app_get_term_console ();
        if (tcon)
                t_context_set_connection (self, tcon->priv->current);

        self->priv->output_stream = NULL;
	_t_app_add_context (self);
}

static void
t_context_dispose (GObject *object)
{
	TContext *console = T_CONTEXT (object);

	if (console->priv) {
		_t_app_remove_context (console);
		t_context_set_connection (console, NULL);

		g_free (console->priv->id);

		if (console->priv->output_stream) {
			if (console->priv->output_is_pipe) {
				pclose (console->priv->output_stream);
#ifndef G_OS_WIN32
				signal (SIGPIPE, SIG_DFL);
#endif
			}
			else
				fclose (console->priv->output_stream);
			console->priv->output_stream = NULL;
			console->priv->output_is_pipe = FALSE;
		}
		g_free (console->priv);
		console->priv = NULL;
	}

    g_date_time_unref (console->priv->last_time_used);

	/* parent class */
        parent_class->dispose (object);
}

static void
t_context_set_property (GObject *object,
			guint param_id,
			const GValue *value,
			GParamSpec *pspec)
{
	TContext *console;
	console = T_CONTEXT (object);
	if (console->priv) {
                switch (param_id) {
                case PROP_ID:
			g_free (console->priv->id);
			console->priv->id = NULL;
			if (g_value_get_string (value))
				console->priv->id = g_value_dup_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
t_context_get_property (GObject *object,
			guint param_id,
			GValue *value,
			GParamSpec *pspec)
{
	TContext *console;
	console = T_CONTEXT (object);
	if (console->priv) {
                switch (param_id) {
                case PROP_ID:
			g_value_set_string (value, console->priv->id);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/*
 * This function is the one in which @console is run, and it blocks in this function until it finishes
 */
static gpointer
t_context_run_thread (TContext *console)
{
	TContextClass *klass;
	klass = T_CONTEXT_CLASS (G_OBJECT_GET_CLASS (console));
	g_assert (klass->run);
	klass->run (console);

	return NULL;
}

/**
 * t_context_run:
 * @console: a #TContext
 *
 * NB: the execution is done in a sub thread, and the caller thread has to call g_thread_join()
 * on the returned thread.
 *
 * Returns: the #GThread in which the execution takes place
 */
GThread *
t_context_run (TContext *console)
{
	g_return_val_if_fail (console, NULL);

	TContextClass *klass;
	klass = T_CONTEXT_CLASS (G_OBJECT_GET_CLASS (console));
	if (!klass->run) {
		g_warning ("TContext does not implement the run() virtual method!");
		return NULL;
	}

	GThread *th;
	gchar *name;
	name = g_strdup_printf ("Console%p", console);
	th = g_thread_new (name, (GThreadFunc) t_context_run_thread, console);
	g_free (name);
	return th;
}

static ToolCommandResult *
t_context_execute_sql_command (TContext *console, const gchar *command,
			       GdaStatementModelUsage usage,
			       GError **error)
{
	ToolCommandResult *res = NULL;
	GdaBatch *batch;
	const GSList *stmt_list;
	GdaStatement *stmt;
	GdaSet *params;
	GObject *obj;
	const gchar *remain = NULL;
	TConnection *tcnc;

	g_assert (T_IS_CONTEXT (console));

	tcnc = console->priv->current;
	if (!tcnc) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	batch = gda_sql_parser_parse_string_as_batch (t_connection_get_parser (tcnc), command, &remain, error);
	if (!batch)
		return NULL;
	if (remain) {
		g_object_unref (batch);
		return NULL;
	}
	
	stmt_list = gda_batch_get_statements (batch);
	if (!stmt_list) {
		g_object_unref (batch);
		return NULL;
	}

	if (stmt_list->next) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("More than one SQL statement"));
		g_object_unref (batch);
		return NULL;
	}
		
	stmt = GDA_STATEMENT (stmt_list->data);
	g_object_ref (stmt);
	g_object_unref (batch);

	if (!gda_statement_get_parameters (stmt, &params, error)) {
		g_object_unref (stmt);
		return NULL;
	}

	/* fill parameters with some defined parameters */
	if (params && gda_set_get_holders (params)) {
		GSList *list;
		for (list = gda_set_get_holders (params); list; list = list->next) {
			GdaHolder *h = GDA_HOLDER (list->data);
			GValue *value;
			value = t_app_get_parameter_value (gda_holder_get_id (h));
			if (value) {
				if ((G_VALUE_TYPE (value) == gda_holder_get_g_type (h)) ||
				    (G_VALUE_TYPE (value) == GDA_TYPE_NULL)) {
					if (!gda_holder_take_value (h, value, error)) {
						g_free (res);
						res = NULL;
						goto cleanup;
					}
				}
				else {
					gchar *str;
					str = gda_value_stringify (value);
					gda_value_free (value);
					value = gda_value_new_from_string (str, gda_holder_get_g_type (h));
					g_free (str);
					if (! value) {
						g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
							     _("Could not interpret the '%s' parameter's value"), 
							     gda_holder_get_id (h));
						g_free (res);
						res = NULL;
						goto cleanup;
					}
					else if (! gda_holder_take_value (h, value, error)) {
						g_free (res);
						res = NULL;
						goto cleanup;
					}
				}
			}
			else {
				if (! gda_holder_is_valid (h)) {
					g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
						     _("No internal parameter named '%s' required by query"), 
						     gda_holder_get_id (h));
					g_free (res);
					res = NULL;
					goto cleanup;
				}
			}
		}
	}

	res = g_new0 (ToolCommandResult, 1);
	res->was_in_transaction_before_exec =
		gda_connection_get_transaction_status (t_connection_get_cnc (tcnc)) ? TRUE : FALSE;
	res->cnc = g_object_ref (t_connection_get_cnc (tcnc));
	obj = gda_connection_statement_execute (t_connection_get_cnc (tcnc), stmt, params, usage, NULL, error);
	if (!obj) {
		g_object_unref (res->cnc);
		g_free (res);
		res = NULL;
	}
	else {
		if (GDA_IS_DATA_MODEL (obj)) {
			res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = GDA_DATA_MODEL (obj);
		}
		else if (GDA_IS_SET (obj)) {
			res->type = BASE_TOOL_COMMAND_RESULT_SET;
			res->u.set = GDA_SET (obj);
		}
		else
			g_assert_not_reached ();
	}

 cleanup:
	g_object_unref (stmt);
	if (params)
		g_object_unref (params);

	return res;
}

/**
 * t_context_command_execute:
 * @console: a #TContext
 * @command: a string containing one or more internal or SQL commands
 * @usage: a #GdaStatementModelUsage for the returned #GdaDataModel objects, if any
 * @error: a place to store errors, or %NULL
 *
 * Executes @command
 *
 * Returns: (transfer full): a new #ToolCommandResult
 */
ToolCommandResult *
t_context_command_execute (TContext *console, const gchar *command,
			   GdaStatementModelUsage usage, GError **error)
{
	TConnection *tcnc;

	g_return_val_if_fail (T_IS_CONTEXT (console), NULL);
	tcnc = console->priv->current;

	if (!command || !(*command))
                return NULL;

	if (base_tool_command_is_internal (command))
		return base_tool_command_group_execute (console->priv->command_group, command + 1,
							console, error);

	else if (*command == '#') {
		/* nothing to do */
		ToolCommandResult *res;
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		return res;
	}
        else {
                if (!tcnc) {
                        g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
				     "%s", _("No connection specified"));
                        return NULL;
                }
                if (!gda_connection_is_opened (t_connection_get_cnc (tcnc))) {
                        g_set_error (error, T_ERROR, T_CONNECTION_CLOSED_ERROR,
				     "%s", _("Connection closed"));
                        return NULL;
                }

                return t_context_execute_sql_command (console, command, usage, error);
        }
}

/**
 * t_context_get_id:
 * @console: a #TContext object
 *
 * Returns: (transfer none): the console's ID
 */
const gchar *
t_context_get_id (TContext *console)
{
	g_return_val_if_fail (T_IS_CONTEXT (console), NULL);
	return console->priv->id;
}

/**
 * t_context_set_command_group:
 * @console: a #TContext object
 * @group: (nullable) (transfer none): a #ToolCommandGroup
 *
 * Defines @console's commands group
 */
void
t_context_set_command_group (TContext *console, ToolCommandGroup *group)
{
	g_return_if_fail (T_IS_CONTEXT (console));
	console->priv->command_group = group;
}

/**
 * t_context_get_command_group:
 * @console: a #TContext object
 *
 * Returns: (transfer none): the console's group of available commands
 */
ToolCommandGroup *
t_context_get_command_group (TContext *console)
{
	g_return_val_if_fail (T_IS_CONTEXT (console), NULL);
	return console->priv->command_group;
}

/**
 * t_context_get_output_format:
 * @console: a #TContext
 *
 * Returns: the output format for the context
 */
ToolOutputFormat
t_context_get_output_format (TContext *console)
{
	g_return_val_if_fail (T_IS_CONTEXT (console), BASE_TOOL_OUTPUT_FORMAT_DEFAULT);
	return console->priv->output_format;
}

/**
 * t_context_get_output_format:
 * @console: a #TContext
 * @format: the format
 *
 * Defines the context's output format
 */
void
t_context_set_output_format (TContext *console, ToolOutputFormat format)
{
	g_return_if_fail (T_IS_CONTEXT (console));

	console->priv->output_format = format;
}

/**
 * t_context_get_output_stream:
 * @console: a #TContext
 * @out_is_pipe: (nullable): a place to store information about the stream being a pipe or not, or %NULL
 *
 * Returns: the current output stream, or %NULL if none defined (i.e. stdout)
 */
FILE *
t_context_get_output_stream (TContext *console, gboolean *out_is_pipe)
{
	g_return_val_if_fail (T_IS_CONTEXT (console), NULL);
	if (out_is_pipe)
		*out_is_pipe = console->priv->output_is_pipe;
	return console->priv->output_stream;
}

/**
 * t_context_set_output_file:
 * @console: a #TContext
 * @file: (nullable): a string representing the output string, or %NULL (i.e. stdout)
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Note: the "| cmd" is an accepted notation to pipe the outpit stream to another program
 *
 * Sets @console's output stream
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
t_context_set_output_file (TContext *console, const gchar *file, GError **error)
{
	g_return_val_if_fail (T_IS_CONTEXT (console), FALSE);

	if (console->priv->output_stream) {
		if (console->priv->output_is_pipe) {
			pclose (console->priv->output_stream);
#ifndef G_OS_WIN32
			signal (SIGPIPE, SIG_DFL);
#endif
		}
		else
			fclose (console->priv->output_stream);
		console->priv->output_stream = NULL;
		console->priv->output_is_pipe = FALSE;
	}

	if (file) {
		gchar *copy = g_strdup (file);
		g_strchug (copy);

		if (*copy != '|') {
			/* output to a file */
			console->priv->output_stream = g_fopen (copy, "w");
			if (!console->priv->output_stream) {
				g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
					     _("Can't open file '%s' for writing: %s\n"), 
					     copy,
					     strerror (errno));
				g_free (copy);
				return FALSE;
			}
			console->priv->output_is_pipe = FALSE;
		}
		else {
			/* output to a pipe */
			if (t_utils_check_shell_argument (copy+1)) {
				console->priv->output_stream = popen (copy+1, "w");
				if (!console->priv->output_stream) {
					g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
						     _("Can't open pipe '%s': %s"), 
						     copy,
						     strerror (errno));
					g_free (copy);
					return FALSE;
				}
			}
			else {
				g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
					     _("Can't open pipe '%s': %s"),
					     copy + 1,
					     "program name must only contain alphanumeric characters");
				g_free (copy);
				return FALSE;
			}
#ifndef G_OS_WIN32
			signal (SIGPIPE, SIG_IGN);
#endif
			console->priv->output_is_pipe = TRUE;
		}
		g_free (copy);
	}

	t_utils_term_compute_color_attribute ();
	return TRUE;
}

/**
 * t_context_get_connection:
 * @console: a #TContext
 *
 * Returns: (transfer none): the connection currently used by @console
 */
TConnection *
t_context_get_connection (TContext *console)
{
	g_return_val_if_fail (T_IS_CONTEXT (console), NULL);
	return console->priv->current;
}

static void
t_connection_status_changed_cb (TConnection *tcnc, GdaConnectionStatus status, TContext *console)
{
	if (status == GDA_CONNECTION_STATUS_CLOSED) {
		gint index;
		TConnection *ntcnc = NULL;
		const GSList *tcnc_list;
		tcnc_list = t_app_get_all_connections ();
		index = g_slist_index ((GSList*) tcnc_list, tcnc);
		if (index == 0)
			ntcnc = g_slist_nth_data ((GSList*) tcnc_list, index + 1);
		else
			ntcnc = g_slist_nth_data ((GSList*) tcnc_list, index - 1);
		t_context_set_connection (console, ntcnc);
	}
}

/**
 * t_context_set_connection:
 * @console: a #TContext
 * @tcnc: (nullable): the #TConnection to use, or %NULL
 *
 * Defines the connection used by @context
 */
void
t_context_set_connection (TContext *console, TConnection *tcnc)
{
	g_return_if_fail (T_IS_CONTEXT (console));
	g_return_if_fail (!tcnc || T_IS_CONNECTION (tcnc));

	if (console->priv->current == tcnc)
		return;

	if (console->priv->current) {
		if (console->priv->sigid) {
			g_signal_handler_disconnect (console->priv->current, console->priv->sigid);
			console->priv->sigid = 0;
		}

		g_object_unref (console->priv->current);
		console->priv->current = NULL;
	}
	if (tcnc) {
		console->priv->current = g_object_ref (tcnc);
		console->priv->sigid = g_signal_connect (tcnc, "status-changed",
							 G_CALLBACK (t_connection_status_changed_cb), console);
	}
}

/**
 * t_context_get_last_time_used:
 * @console: a #TContext
 *
 * Get a pointer to the #GTimeVal representing the last time @console was used
 *
 * Returns: (transfer none): a #GTimeVal pointer
 */
GDateTime*
t_context_get_last_time_used (TContext *console)
{
	g_return_val_if_fail (T_IS_CONTEXT (console), NULL);
	return console->priv->last_time_used;
}
