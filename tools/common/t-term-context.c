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

#include "t-term-context.h"
#include "t-utils.h"
#include "t-app.h"
#include <glib/gi18n-lib.h>
#include <sql-parser/gda-sql-parser.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <thread-wrapper/gda-connect.h>

/*
 * Main static functions
 */
static void t_term_context_class_init (TTermContextClass *klass);
static void t_term_context_init (TTermContext *self);
static void t_term_context_dispose (GObject *object);
static void t_term_context_run (TContext *self);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _TTermContextPrivate {
	FILE *input_stream;
	GString *partial_command;
	gboolean interactive;

	GMainLoop *main_loop; /* used when t_context_run() has been called (no ref is held here, only on the stack) */
	gulong exit_sig_id;
};

GType
t_term_context_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GMutex registering;
                static const GTypeInfo info = {
                        sizeof (TTermContextClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) t_term_context_class_init,
                        NULL,
                        NULL,
                        sizeof (TTermContext),
                        0,
                        (GInstanceInitFunc) t_term_context_init,
                        0
                };


                g_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (T_TYPE_CONTEXT, "TTermContext", &info, 0);
                g_mutex_unlock (&registering);
        }
        return type;
}

static void
t_term_context_class_init (TTermContextClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        parent_class = g_type_class_peek_parent (klass);

	T_CONTEXT_CLASS (klass)->run = t_term_context_run;
        object_class->dispose = t_term_context_dispose;
}

static void
t_term_context_init (TTermContext *self)
{
        self->priv = g_new0 (TTermContextPrivate, 1);
	self->priv->interactive = FALSE;
	self->priv->exit_sig_id = 0;
}

static void
t_term_context_dispose (GObject *object)
{
	TTermContext *console = T_TERM_CONTEXT (object);

	if (console->priv) {
		if (console->priv->exit_sig_id > 0)
			gda_signal_handler_disconnect (t_app_get (), console->priv->exit_sig_id);
		t_term_context_set_input_stream (console, NULL);
		if (console->priv->main_loop)
			g_main_loop_quit (console->priv->main_loop);
		g_free (console->priv);
		console->priv = NULL;
	}

	/* parent class */
        parent_class->dispose (object);
}


/* TERM console functions */
static const char *prompt_func (void);
static void setup_sigint_handler (void);
static gboolean treat_line_func (const gchar *cmde, gboolean *out_exec_command_ok);
static char **completion_func (const char *text, const gchar *line, int start, int end, gpointer data);
static void compute_prompt (TTermContext *console, GString *string, gboolean in_command,
			    gboolean for_readline, ToolOutputFormat format);


/**
 * t_term_context_new:
 *
 * Returns: (transfer none): a new #TTermContext
 */
TTermContext *
t_term_context_new (const gchar *id)
{
	TApp *main_data;
	main_data = t_app_get ();

        TTermContext *console;
	console = g_object_new (T_TYPE_TERM_CONTEXT, "id", id, NULL);

	t_context_set_command_group (T_CONTEXT (console), main_data->term_commands);

        return console;
}

/**
 * t_term_context_set_interactive:
 * @console: a #TTermContext
 * @interactive:
 *
 *
 */
void
t_term_context_set_interactive (TTermContext *term_console, gboolean interactive)
{
	g_return_if_fail (T_IS_TERM_CONTEXT (term_console));
	term_console->priv->interactive = interactive;
}

/*
 * SIGINT handling
 */
#ifndef G_OS_WIN32
struct sigaction old_sigint_handler; 
static void sigint_handler (int sig_num);
typedef enum {
	SIGINT_HANDLER_DISABLED = 0,
	SIGINT_HANDLER_PARTIAL_COMMAND,
} SigintHandlerCode;
static SigintHandlerCode sigint_handler_status = SIGINT_HANDLER_DISABLED;

static void
sigint_handler (int sig_num)
{
	TTermContext *term_console;
	term_console = T_TERM_CONTEXT (t_app_get_term_console ());
	if (sigint_handler_status == SIGINT_HANDLER_PARTIAL_COMMAND) {
		if (term_console->priv->partial_command) {
			g_string_free (term_console->priv->partial_command, TRUE);
			term_console->priv->partial_command = NULL;
			/* disable SIGINT handling */
			sigint_handler_status = SIGINT_HANDLER_DISABLED;
		}
		/* show a new prompt */
		gchar *tmp;
		tmp = t_utils_compute_prompt (T_CONTEXT (term_console), FALSE, FALSE,
					      BASE_TOOL_OUTPUT_FORMAT_DEFAULT |
					      (t_context_get_output_format (T_CONTEXT (term_console)) &
					       BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM));
		g_print ("\n%s", tmp);
		g_free (tmp);
		tmp = t_utils_compute_prompt (T_CONTEXT (term_console), FALSE, TRUE,
					      BASE_TOOL_OUTPUT_FORMAT_DEFAULT |
					      (t_context_get_output_format (T_CONTEXT (term_console)) &
					       BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM));
		g_free (tmp);
		fflush (NULL);
	}
	else {
		g_print ("\n");
		exit (EXIT_SUCCESS);
	}

	if (old_sigint_handler.sa_handler)
		old_sigint_handler.sa_handler (sig_num);
}
#endif /* G_OS_WIN32 */

static void 
setup_sigint_handler (void)
{
#ifndef G_OS_WIN32
	struct sigaction sac;
	memset (&sac, 0, sizeof (sac));
	sigemptyset (&sac.sa_mask);
	sac.sa_handler = sigint_handler;
	sac.sa_flags = SA_RESTART;
	sigaction (SIGINT, &sac, &old_sigint_handler);
#endif
}


static void
display_result (TApp *main_data, ToolCommandResult *res)
{
	switch (res->type) {
	case BASE_TOOL_COMMAND_RESULT_TXT_STDOUT: 
		g_print ("%s", res->u.txt->str);
		if (res->u.txt->str [strlen (res->u.txt->str) - 1] != '\n')
			g_print ("\n");
		fflush (NULL);
		break;
	case BASE_TOOL_COMMAND_RESULT_EMPTY:
		break;
	case BASE_TOOL_COMMAND_RESULT_MULTIPLE: {
		GSList *list;
		for (list = res->u.multiple_results; list; list = list->next)
			display_result (main_data, (ToolCommandResult *) list->data);
		break;
	}
	case BASE_TOOL_COMMAND_RESULT_EXIT:
		break;
	default: {
		if (res->type == BASE_TOOL_COMMAND_RESULT_DATA_MODEL) {
			t_app_store_data_model (res->u.model, T_LAST_DATA_MODEL_NAME);
		}
		gchar *str;
		FILE *ostream;
		ToolOutputFormat oformat;
		ostream = t_context_get_output_stream (t_app_get_term_console (), NULL);
		oformat = t_context_get_output_format (t_app_get_term_console ());
		str = base_tool_output_result_to_string (res, oformat, ostream, t_app_get_options ());
		base_tool_output_output_string (ostream, str);
		g_free (str);
	}
	}
}

typedef struct {
	gboolean single_line; /* set to TRUE to force considering the line as complete */
	gboolean exec_command_ok;
} TermLineData;

/*
 * Returns: %TRUE if exit has been requested
 */
static gboolean
term_treat_line (TTermContext *term_console, const gchar *cmde, TermLineData *ldata)
{
	TApp *main_data = t_app_get ();
	g_assert (main_data);

	ldata->exec_command_ok = TRUE;

	FILE *ostream;
	ostream = t_context_get_output_stream (T_CONTEXT (term_console), NULL);

	if (!cmde) {
		base_tool_input_save_history (NULL, NULL);
		if (!ostream)
			g_print ("\n");
		if (term_console->priv->main_loop)
			g_main_loop_quit (term_console->priv->main_loop);
		return TRUE;
	}

	gchar *loc_cmde = NULL;
	loc_cmde = g_strdup (cmde);
	g_strchug (loc_cmde);
	if (*loc_cmde) {
		if (*loc_cmde == '#') {
			g_free (loc_cmde);
			return FALSE;
		}

		base_tool_input_add_to_history (loc_cmde);
		if (!term_console->priv->partial_command) {
#ifndef G_OS_WIN32
			/* enable SIGINT handling */
			sigint_handler_status = SIGINT_HANDLER_PARTIAL_COMMAND;
#endif
			term_console->priv->partial_command = g_string_new (loc_cmde);
		}
		else {
			g_string_append_c (term_console->priv->partial_command, '\n');
			g_string_append (term_console->priv->partial_command, loc_cmde);
		}
		if (ldata->single_line ||
		    t_utils_command_is_complete (T_CONTEXT (term_console), term_console->priv->partial_command->str)) {
			/* execute command */
			ToolCommandResult *res;
			FILE *to_stream;
			GError *error = NULL;
			
			if ((*term_console->priv->partial_command->str != '\\') &&
			    (*term_console->priv->partial_command->str != '.')) {
				TConnection *tcnc;
				tcnc = t_context_get_connection (T_CONTEXT (term_console));
				if (tcnc)
					t_connection_set_query_buffer (tcnc,
								       term_console->priv->partial_command->str);
			}
			
			if (ostream)
				to_stream = ostream;
			else
				to_stream = stdout;
			res = t_context_command_execute (T_CONTEXT (term_console),
							 term_console->priv->partial_command->str,
							 GDA_STATEMENT_MODEL_RANDOM_ACCESS, &error);
			
			if (!res) {
				if (!error ||
				    (error->domain != GDA_SQL_PARSER_ERROR) ||
				    (error->code != GDA_SQL_PARSER_EMPTY_SQL_ERROR)) {
					ToolOutputFormat oformat;
					oformat = t_context_get_output_format (T_CONTEXT (term_console));
					g_fprintf (to_stream, "%sERROR:%s ",
						   base_tool_output_color_s (BASE_TOOL_COLOR_RED, oformat),
						   base_tool_output_color_s (BASE_TOOL_COLOR_RESET, oformat));
					g_fprintf (to_stream,
						   "%s\n", 
						   error && error->message ? error->message : _("No detail"));
					ldata->exec_command_ok = FALSE;
				}
				if (error) {
					g_error_free (error);
					error = NULL;
				}
			}
			else {
				display_result (main_data, res);
				if (res->type == BASE_TOOL_COMMAND_RESULT_EXIT) {
					base_tool_command_result_free (res);
					t_app_request_quit ();
					if (term_console->priv->main_loop)
						g_main_loop_quit (term_console->priv->main_loop);
					g_free (loc_cmde);
					return TRUE;
				}
				base_tool_command_result_free (res);
			}
			g_string_free (term_console->priv->partial_command, TRUE);
			term_console->priv->partial_command = NULL;

#ifndef G_OS_WIN32
			/* disable SIGINT handling */
			sigint_handler_status = SIGINT_HANDLER_DISABLED;
#endif
		}
	}

	g_free (loc_cmde);
	return FALSE;
}

static const char *
prompt_func (void)
{
	/* compute a new prompt */
	ToolOutputFormat oformat;
	oformat = t_context_get_output_format (t_app_get_term_console ());

	static GString *prompt = NULL;
	if (!prompt)
		prompt = g_string_new ("");

	TTermContext *term_console;
	term_console = T_TERM_CONTEXT (t_app_get_term_console ());

	compute_prompt (NULL, prompt, term_console->priv->partial_command == NULL ? FALSE : TRUE, TRUE,
			BASE_TOOL_OUTPUT_FORMAT_DEFAULT |
			(oformat & BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM));
	return (char*) prompt->str;
}

/* @cmde is stolen here */
static gboolean
treat_line_func (const gchar *cmde, gboolean *out_exec_command_ok)
{
	gboolean res;
	TermLineData ldata;
	ldata.single_line = FALSE;
	res = term_treat_line (T_TERM_CONTEXT (t_app_get_term_console ()), cmde, &ldata);
	if (out_exec_command_ok)
		*out_exec_command_ok = ldata.exec_command_ok;
	return res;
}

/**
 * clears and modifies @string to hold a prompt.
 */
static void
compute_prompt (TTermContext *console, GString *string, gboolean in_command, gboolean for_readline, ToolOutputFormat format)
{
	g_string_set_size (string, 0);
	if (!console)
		console = T_TERM_CONTEXT (t_app_get_term_console ());

	gchar *tmp;
	tmp = t_utils_compute_prompt (T_CONTEXT (console), in_command, for_readline, format);
	g_string_prepend (string, tmp);
	g_free (tmp);
}

/*
 * This function tries to provide good completion options.
 */
static char **
completion_func (G_GNUC_UNUSED const char *text, const gchar *line, int start, int end, G_GNUC_UNUSED gpointer data)
{
	TConnection *tcnc;
	char **array;
	gchar **gda_compl;
	gint i, nb_compl;
	gboolean internal_cmd = FALSE;
	if ((*line == '.') || (*line == '\\'))
		internal_cmd = TRUE;

	tcnc = t_context_get_connection (t_app_get_term_console ());
	if (!tcnc)
		return NULL;
	gda_compl = gda_completion_list_get (t_connection_get_cnc (tcnc), line, start, end);
	if (!gda_compl) {
		if (line[start] == '\"')
			return NULL;
		else {
			/* no completion function and it appears we don't have double quotes to enclose our
			 * completion query, so we try to add those double quotes and see if wa have a better
			 * chance at completion options */
			gchar *tmp1, *tmp2;
			tmp1 = g_strdup (line);
			tmp1[start] = 0;
			tmp2 = g_strdup_printf ("%s\\\"%s", tmp1, line+start);
			g_free (tmp1);
			array = completion_func (NULL, tmp2, start+1, end+2, NULL);
			if (array) {
				g_free (tmp2);
				for (i= 0; ;i++) {
					gchar *tmp;
					tmp = array[i];
					if (!tmp || !*tmp)
						break;
					if (internal_cmd) {
						array[i] = g_strdup_printf ("\\%s", tmp);
						g_free (tmp);
					}
				}
			}
			return array;
		}
	}

	for (nb_compl = 0; gda_compl[nb_compl]; nb_compl++);

	array = malloc (sizeof (char*) * (nb_compl + 1));
	for (i = 0; i < nb_compl; i++) {
		char *str;
		gchar *gstr;
		gint l;

		gstr = gda_compl[i];
		l = strlen (gstr) + 1;
		if (gstr[l-2] == '"')
			str = malloc (sizeof (char) * (l+1));
		else
			str = malloc (sizeof (char) * l);
		memcpy (str, gstr, l);
		if (internal_cmd && (str[l-2] == '"')) {
			str[l-2] = '\\';
			str[l-1] = '"';
			str[l] = 0;
		}
		array[i] = str;
	}
	array[i] = NULL;
	g_strfreev (gda_compl);
	return array;
}

/**
 * t_term_context_set_input_file:
 *
 * Change the input file, set to %NULL to be back on stdin
 */
gboolean
t_term_context_set_input_file (TTermContext *term_console, const gchar *file, GError **error)
{
	g_return_val_if_fail (T_IS_TERM_CONTEXT (term_console), FALSE);
	if (term_console->priv->input_stream) {
		fclose (term_console->priv->input_stream);
		term_console->priv->input_stream = NULL;
	}

	if (file) {
		if (*file == '~') {
			gchar *tmp;
			tmp = g_strdup_printf ("%s%s", g_get_home_dir (), file+1);
			term_console->priv->input_stream = g_fopen (tmp, "r");
			g_free (tmp);
		}
		else
			term_console->priv->input_stream = g_fopen (file, "r");
		if (!term_console->priv->input_stream) {
			g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
				     _("Can't open file '%s' for reading: %s\n"),
				     file,
				     strerror (errno));
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * t_term_context_set_input_stream:
 *
 * Defines the input stream
 */
void
t_term_context_set_input_stream (TTermContext *term_console, FILE *stream)
{
	g_return_if_fail (T_IS_TERM_CONTEXT (term_console));
	if (term_console->priv->input_stream) {
		if (term_console->priv->input_stream == stream)
			return;
		fclose (term_console->priv->input_stream);
		term_console->priv->input_stream = NULL;
	}
	if (stream)
		term_console->priv->input_stream = stream;
}

/**
 * t_term_context_get_input_stream:
 *
 * Returns: the input stream, or %NULL
 */
FILE *
t_term_context_get_input_stream (TTermContext *term_console)
{
	g_return_val_if_fail (T_IS_TERM_CONTEXT (term_console), NULL);
	return term_console->priv->input_stream;
}


/**
 * t_term_context_treat_single_line:
 */
void
t_term_context_treat_single_line (TTermContext *term_console, const gchar *cmde)
{
	g_return_if_fail (T_IS_TERM_CONTEXT (term_console));
	g_return_if_fail (cmde && *cmde);

	TermLineData ld;
	ld.single_line = TRUE;
	ld.exec_command_ok = FALSE;

	term_treat_line (term_console, cmde, &ld);
}

static void
quit_requested_cb (TApp *app, TContext *self)
{
	g_main_loop_quit (T_TERM_CONTEXT (self)->priv->main_loop);
}

static void
t_term_context_run (TContext *self)
{
	FILE *istream;
	TTermContext *term_console = T_TERM_CONTEXT (self);
	gboolean interactive;
	interactive = term_console->priv->interactive;
	istream = term_console->priv->input_stream;
	if (istream) {
		gchar *cmde;
		for (cmde = base_tool_input_from_stream (istream);
		     cmde;
		     cmde = base_tool_input_from_stream (istream)) {
			TermLineData ld;
			ld.single_line = FALSE;
			ld.exec_command_ok = FALSE;

			gboolean exit_req = FALSE;
			if (term_treat_line (term_console, cmde, &ld)) {
				exit_req = TRUE;
				interactive = FALSE;
			}
			g_free (cmde);
			if (! ld.exec_command_ok)
				exit_req = TRUE;
			if (exit_req)
				break;
		}
		t_term_context_set_input_stream (term_console, NULL);
		interactive = FALSE; /* can't have this if the input stream has been closed! */
	}
	else
		interactive = TRUE;

	if (interactive) {
		setup_sigint_handler ();

		GMainContext *context;
		context = g_main_context_new ();
		g_main_context_acquire (context);

		base_tool_input_init (context, (ToolInputTreatLineFunc) treat_line_func, prompt_func, NULL);
		base_tool_input_set_completion_func (t_context_get_command_group (T_CONTEXT (self)),
						     completion_func, t_app_get (), ".\\");

		term_console->priv->exit_sig_id = gda_signal_connect (t_app_get (), "quit-requested",
								      G_CALLBACK (quit_requested_cb), self,
								      NULL, 0, context);

		GMainLoop *main_loop;
		main_loop = g_main_loop_new (context, TRUE);
		term_console->priv->main_loop = main_loop;
		g_main_loop_run (main_loop);
		term_console->priv->main_loop = NULL;
		g_main_loop_unref (main_loop);
		g_main_context_unref (context);

		base_tool_input_end ();
	}

	fflush (NULL);

	if (term_console->priv->exit_sig_id > 0) {
		gda_signal_handler_disconnect (t_app_get (), term_console->priv->exit_sig_id);
		term_console->priv->exit_sig_id = 0;
	}

	t_app_remove_feature (T_APP_TERM_CONSOLE);
}
